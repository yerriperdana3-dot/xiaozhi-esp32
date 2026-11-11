#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/display.h"
#include "display/oled_display.h"
#include "system_reset.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "mcp_server.h"
#include "lamp_controller.h"
#include "led/single_led.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_panel_sh1106.h>

#define TAG "CompactWifiBoardLCD"

class CompactWifiBoardLCD : public WifiBoard {
private:

    Button boot_button_;

    Display* display_ = nullptr;

    i2c_master_bus_handle_t display_i2c_bus_ = nullptr;
    esp_lcd_panel_io_handle_t panel_io_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;

    // =====================================================
    // INITIALIZE I2C
    // =====================================================

    void InitializeDisplayI2c() {

        i2c_master_bus_config_t bus_config = {};

        bus_config.i2c_port = I2C_NUM_0;
        bus_config.sda_io_num = OLED_I2C_SDA_GPIO;
        bus_config.scl_io_num = OLED_I2C_SCL_GPIO;
        bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
        bus_config.glitch_ignore_cnt = 7;
        bus_config.flags.enable_internal_pullup = true;

        ESP_ERROR_CHECK(
            i2c_new_master_bus(
                &bus_config,
                &display_i2c_bus_
            )
        );

        ESP_LOGI(TAG, "OLED I2C bus initialized");
    }

    // =====================================================
    // INITIALIZE SH1106 OLED
    // =====================================================

    void InitializeOledDisplay() {

        // I2C communication configuration
        esp_lcd_panel_io_i2c_config_t io_config = {};

        io_config.dev_addr = OLED_I2C_ADDRESS;
        io_config.scl_speed_hz = 400 * 1000;
        io_config.control_phase_bytes = 1;
        io_config.dc_bit_offset = 6;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;

        ESP_ERROR_CHECK(
            esp_lcd_new_panel_io_i2c(
                display_i2c_bus_,
                &io_config,
                &panel_io_
            )
        );

        ESP_LOGI(TAG, "Installing SH1106 driver");

        // SH1106 panel configuration
        esp_lcd_panel_dev_config_t panel_config = {};

        panel_config.reset_gpio_num = GPIO_NUM_NC;
        panel_config.bits_per_pixel = 1;

        esp_lcd_panel_sh1106_config_t sh1106_config = {};

        panel_config.vendor_config = &sh1106_config;

        ESP_ERROR_CHECK(
            esp_lcd_new_panel_sh1106(
                panel_io_,
                &panel_config,
                &panel_
            )
        );

        ESP_LOGI(TAG, "SH1106 driver installed");

        // Reset OLED
        ESP_ERROR_CHECK(
            esp_lcd_panel_reset(panel_)
        );

        // Initialize OLED
        esp_err_t init_result = esp_lcd_panel_init(panel_);

        if (init_result != ESP_OK) {

            ESP_LOGE(
                TAG,
                "Failed to initialize SH1106 OLED: %s",
                esp_err_to_name(init_result)
            );

            display_ = new NoDisplay();
            return;
        }

        // Normal display
        ESP_ERROR_CHECK(
            esp_lcd_panel_invert_color(
                panel_,
                false
            )
        );

        // Turn OLED ON
        ESP_ERROR_CHECK(
            esp_lcd_panel_disp_on_off(
                panel_,
                true
            )
        );

        // Connect OLED to XiaoZhi display system
        display_ = new OledDisplay(
            panel_io_,
            panel_,
            OLED_WIDTH,
            OLED_HEIGHT,
            false,
            false
        );

        ESP_LOGI(TAG, "Athena SH1106 OLED ready");
    }

    // =====================================================
    // BUTTON
    // =====================================================

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

    // =====================================================
    // TOOLS
    // =====================================================

    void InitializeTools() {

        static LampController lamp(LAMP_GPIO);

    }

public:

    CompactWifiBoardLCD()
        : boot_button_(BOOT_BUTTON_GPIO) {

        InitializeDisplayI2c();

        InitializeOledDisplay();

        InitializeButtons();

        InitializeTools();
    }

    // =====================================================
    // LED
    // =====================================================

    virtual Led* GetLed() override {

        static SingleLed led(BUILTIN_LED_GPIO);

        return &led;
    }

    // =====================================================
    // AUDIO - TIDAK DIUBAH
    // =====================================================

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

    // =====================================================
    // DISPLAY
    // =====================================================

    virtual Display* GetDisplay() override {

        return display_;
    }

    virtual Backlight* GetBacklight() override {

        return nullptr;
    }

};

DECLARE_BOARD(CompactWifiBoardLCD);
