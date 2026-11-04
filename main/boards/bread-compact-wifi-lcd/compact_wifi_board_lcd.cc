
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

#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>

#define TAG "CompactWifiBoardLCD"

#define LCD_RS 0x01
#define LCD_EN 0x04
#define LCD_BL 0x08

class CompactWifiBoardLCD : public WifiBoard {
private:
    Button boot_button_;
    Display* display_ = nullptr;

    i2c_master_bus_handle_t lcd_i2c_bus_ = nullptr;
    i2c_master_dev_handle_t lcd_i2c_device_ = nullptr;

    uint8_t lcd_backlight_ = LCD_BL;
    bool lcd_ready_ = false;

    std::mutex lcd_mutex_;

    std::string current_status_ = "STANDBY";
    std::string current_role_ = "ATHENA AI";
    std::string current_message_;

    esp_err_t LCD_WriteExpander(uint8_t data) {
        if (lcd_i2c_device_ == nullptr) {
            return ESP_ERR_INVALID_STATE;
        }

        return i2c_master_transmit(
            lcd_i2c_device_, &data, 1, 100
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

        uint8_t high =
            (value & 0xF0) | mode | lcd_backlight_;

        uint8_t low =
            ((value << 4) & 0xF0) | mode | lcd_backlight_;

        LCD_PulseEnable(high);
        LCD_PulseEnable(low);
    }

    void LCD_Command(uint8_t command) {
        LCD_Send(command, false);

        if (command == 0x01 || command == 0x02) {
            esp_rom_delay_us(2000);
        }
    }

    void LCD_Print(const char* text) {
        if (text == nullptr) return;

        while (*text) {
            LCD_Send(static_cast<uint8_t>(*text), true);
            text++;
        }
    }

    void LCD_SetCursor(uint8_t column, uint8_t row) {
        static const uint8_t row_offsets[] = {0x00, 0x40};

        if (row > 1) row = 1;
        if (column > 11) column = 11;

        LCD_Command(0x80 | (column + row_offsets[row]));
    }

    void LCD_Clear() {
        LCD_Command(0x01);
    }

    void LCD_PrintLine(uint8_t row, const std::string& text) {
        LCD_SetCursor(0, row);

        for (int i = 0; i < 12; i++) {
            char c = ' ';

            if (i < static_cast<int>(text.size())) {
                unsigned char ch =
                    static_cast<unsigned char>(text[i]);

                // LCD karakter standar hanya menerima ASCII.
                c = (ch >= 32 && ch <= 126)
                    ? static_cast<char>(ch)
                    : '?';
            }

            LCD_Send(static_cast<uint8_t>(c), true);
        }
    }

    void LCD_Render(const std::string& first,
                    const std::string& second) {
        if (!lcd_ready_) return;

        LCD_PrintLine(0, first);
        LCD_PrintLine(1, second);
    }

    std::string ShortText(const std::string& text) {
        if (text.size() <= 12) return text;
        return text.substr(0, 12);
    }

    void RenderCurrentMessage() {
        LCD_Render(
            ShortText(current_role_),
            ShortText(current_message_)
        );
    }

    bool InitializeLcdI2c() {
        i2c_master_bus_config_t bus_config = {};

        bus_config.i2c_port = I2C_NUM_0;
        bus_config.sda_io_num = LCD_I2C_SDA_GPIO;
        bus_config.scl_io_num = LCD_I2C_SCL_GPIO;
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = true;

        esp_err_t err = i2c_new_master_bus(
            &bus_config, &lcd_i2c_bus_
        );

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2C bus gagal: %s",
                     esp_err_to_name(err));
            return false;
        }

        i2c_device_config_t device_config = {};

        device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
        device_config.device_address = LCD_I2C_ADDRESS;
        device_config.scl_speed_hz = 100000;

        err = i2c_master_bus_add_device(
            lcd_i2c_bus_,
            &device_config,
            &lcd_i2c_device_
        );

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LCD I2C gagal: %s",
                     esp_err_to_name(err));

            i2c_del_master_bus(lcd_i2c_bus_);
            lcd_i2c_bus_ = nullptr;
            return false;
        }

        ESP_LOGI(TAG, "LCD I2C siap");
        return true;
    }

    bool InitializeCharacterLcd() {
        if (lcd_i2c_device_ == nullptr) return false;

        esp_rom_delay_us(50000);

        if (LCD_WriteExpander(lcd_backlight_) != ESP_OK) {
            return false;
        }

        LCD_Write4Bits(0x30);
        esp_rom_delay_us(4500);

        LCD_Write4Bits(0x30);
        esp_rom_delay_us(4500);

        LCD_Write4Bits(0x30);
        esp_rom_delay_us(150);

        LCD_Write4Bits(0x20);

        LCD_Command(0x28);
        LCD_Command(0x0C);
        LCD_Command(0x06);
        LCD_Clear();

        lcd_ready_ = true;

        LCD_Render("ATHENA AI", "STANDBY");

        ESP_LOGI(TAG, "LCD karakter siap");
        return true;
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

    void InitializeTools() {
        static LampController lamp(LAMP_GPIO);
    }

    // Implementasi Display untuk LCD karakter
    class CharacterLcdDisplay : public Display {
    private:
        CompactWifiBoardLCD* board_;

    public:
        explicit CharacterLcdDisplay(CompactWifiBoardLCD* board)
            : board_(board) {}

        bool Lock(int timeout_ms = 0) override {
            (void)timeout_ms;
            board_->lcd_mutex_.lock();
            return true;
        }

        void Unlock() override {
            board_->lcd_mutex_.unlock();
        }

        void SetStatus(const char* status) override {
            DisplayLockGuard lock(this);
            if (!lock) return;

            std::string value =
                (status != nullptr) ? status : "READY";

            board_->current_status_ = value;
            board_->current_role_ = "ATHENA AI";
            board_->current_message_ = value;

            board_->LCD_Render(
                board_->ShortText("ATHENA AI"),
                board_->ShortText(value)
            );
        }

        void ShowNotification(
            const char* notification,
            int duration_ms = 3000
        ) override {
            (void)duration_ms;

            DisplayLockGuard lock(this);
            if (!lock) return;

            std::string value =
                (notification != nullptr)
                ? notification : "";

            board_->current_role_ = "NOTIFICATION";
            board_->current_message_ = value;

            board_->LCD_Render(
                "NOTIFICATION",
                board_->ShortText(value)
            );
        }

        void SetChatMessage(
            const char* role,
            const char* content
        ) override {
            DisplayLockGuard lock(this);
            if (!lock) return;

            std::string role_text =
                (role != nullptr) ? role : "";

            std::string content_text =
                (content != nullptr) ? content : "";

            if (role_text == "user") {
                board_->current_role_ = "YOU:";
            } else if (role_text == "assistant") {
                board_->current_role_ = "ATHENA:";
            } else {
                board_->current_role_ = "SYSTEM:";
            }

            board_->current_message_ = content_text;

            board_->LCD_Render(
                board_->ShortText(board_->current_role_),
                board_->ShortText(board_->current_message_)
            );
        }

        void ClearChatMessages() override {
            DisplayLockGuard lock(this);
            if (!lock) return;

            board_->current_role_ = "ATHENA AI";
            board_->current_message_ = "STANDBY";

            board_->LCD_Render("ATHENA AI", "STANDBY");
        }
    };

public:
    CompactWifiBoardLCD()
        : boot_button_(BOOT_BUTTON_GPIO) {

        if (InitializeLcdI2c()) {
            InitializeCharacterLcd();
        }

        // Ganti NoDisplay dengan display LCD karakter.
        display_ = new CharacterLcdDisplay(this);

        if (!lcd_ready_) {
            ESP_LOGW(TAG, "LCD tidak aktif");
        }

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
