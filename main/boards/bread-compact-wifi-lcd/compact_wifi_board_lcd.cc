#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"

#include <esp_log.h>
#include <esp_rom_sys.h>
#include <driver/i2c_master.h>

#define TAG "CompactWifiBoardLCD"

// LCD I2C PCF8574
#define LCD_I2C_ADDRESS  0x27
#define LCD_I2C_SDA      GPIO_NUM_21
#define LCD_I2C_SCL      GPIO_NUM_47

// PCF8574 bit mapping umum:
// P0=RS, P1=RW, P2=EN, P3=Backlight
// P4-P7 = D4-D7
#define LCD_RS  0x01
#define LCD_EN  0x04
#define LCD_BL  0x08

class CompactWifiBoardLCD : public WifiBoard {
private:
    Button boot_button_;
    Display* display_ = nullptr;

    i2c_master_bus_handle_t lcd_i2c_bus_ = nullptr;
    i2c_master_dev_handle_t lcd_i2c_device_ = nullptr;

    uint8_t lcd_backlight_ = LCD_BL;

    esp_err_t LCD_WriteExpander(uint8_t data) {
        return i2c_master_transmit(
            lcd_i2c_device_,
            &data,
            1,
            100
        );
    }

    void LCD_PulseEnable(uint8_t data) {
        LCD_WriteExpander(data | LCD_EN);
        esp_rom_delay_us(1);

        LCD_WriteExpander(data & ~LCD_EN);
        esp_rom_delay_us(50);
    }

    void LCD_Write4Bits(uint8_t value) {
        uint8_t data = (value & 0xF0) | lcd_backlight_;

        LCD_WriteExpander(data);
        LCD_PulseEnable(data);
    }

    void LCD_Send(uint8_t value, bool data_mode) {
        uint8_t mode = data_mode ? LCD_RS : 0;

        uint8_t high = (value & 0xF0) | mode | lcd_backlight_;
        uint8_t low = ((value << 4) & 0xF0) | mode | lcd_backlight_;

        LCD_WriteExpander(high);
        LCD_PulseEnable(high);

        LCD_WriteExpander(low);
        LCD_PulseEnable(low);
    }

    void LCD_Command(uint8_t command) {
        LCD_Send(command, false);

        if (command == 0x01 || command == 0x02) {
            esp_rom_delay_us(2000);
        }
    }

    void LCD_Print(const char* text) {
        while (*text) {
            LCD_Send(static_cast<uint8_t>(*text), true);
            text++;
        }
    }

    void LCD_SetCursor(uint8_t column, uint8_t row) {
        static const uint8_t row_offsets[] = {
            0x00,
            0x40
        };

        if (row > 1) {
            row = 1;
        }

        LCD_Command(0x80 | (column + row_offsets[row]));
    }

    void LCD_Clear() {
        LCD_Command(0x01);
    }

    void InitializeLcdI2c() {
        i2c_master_bus_config_t bus_config = {};

        bus_config.i2c_port = I2C_NUM_0;
        bus_config.sda_io_num = LCD_I2C_SDA;
        bus_config.scl_io_num = LCD_I2C_SCL;
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = true;

        ESP_ERROR_CHECK(
            i2c_new_master_bus(&bus_config, &lcd_i2c_bus_)
        );

        i2c_device_config_t device_config = {};

        device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device_config.device_address = LCD_I2C_ADDRESS;
        device_config.scl_speed_hz = 100000;

        ESP_ERROR_CHECK(
            i2c_master_bus_add_device(
                lcd_i2c_bus_,
                &device_config,
                &lcd_i2c_device_
            )
        );

        ESP_LOGI(TAG, "LCD I2C device connected");
    }

    void InitializeCharacterLcd() {
        esp_rom_delay_us(50000);

        LCD_WriteExpander(lcd_backlight_);

        // Inisialisasi HD44780 mode 4-bit
        LCD_Write4Bits(0x30);
        esp_rom_delay_us(4500);

        LCD_Write4Bits(0x30);
        esp_rom_delay_us(4500);

        LCD_Write4Bits(0x30);
        esp_rom_delay_us(150);

        LCD_Write4Bits(0x20);

        // 4-bit, 2 baris, font 5x8
        LCD_Command(0x28);

        // Display ON, cursor OFF
        LCD_Command(0x0C);

        // Entry mode
        LCD_Command(0x06);

        LCD_Clear();

        // Tampilan awal Athena
        LCD_SetCursor(0, 0);
        LCD_Print("ATHENA AI");

        LCD_SetCursor(0, 1);
        LCD_Print("LCD READY");

        ESP_LOGI(TAG, "Character LCD initialized");
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();

            if (app.GetDeviceState() == kDeviceStateStarting) {
                EnterWifiConfigMode();
                return;
            }

            app.ToggleChatState();
        });
    }

    // Inisialisasi perangkat IoT
    void InitializeTools() {
        static LampController lamp(LAMP_GPIO);
    }

public:
    CompactWifiBoardLCD()
        : boot_button_(BOOT_BUTTON_GPIO) {

        InitializeLcdI2c();
        InitializeCharacterLcd();

        // Tidak menggunakan layar grafis SPI.
        // LCD karakter bekerja sebagai display tambahan.
        display_ = new NoDisplay();

        InitializeButtons();
        InitializeTools();
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual AudioCodec* GetAudioCodec() override {
#ifdef AUDIO_I2S_METHOD_SIMPLEX
        static NoAudioCodecSimplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK,
            AUDIO_I2S_SPK_GPIO_LRCK,
            AUDIO_I2S_SPK_GPIO_DOUT,
            AUDIO_I2S_MIC_GPIO_SCK,
            AUDIO_I2S_MIC_GPIO_WS,
            AUDIO_I2S_MIC_GPIO_DIN
        );
#else
        static NoAudioCodecDuplex audio_codec(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN
        );
#endif
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        return nullptr;
    }
};

DECLARE_BOARD(CompactWifiBoardLCD);
