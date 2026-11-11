#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

// ============================================================
// AUDIO CONFIGURATION
// ============================================================

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_I2S_METHOD_SIMPLEX

#ifdef AUDIO_I2S_METHOD_SIMPLEX

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_5
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_6

#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_7
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_15
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_16

#else

#define AUDIO_I2S_GPIO_WS   GPIO_NUM_4
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_5
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_6
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_7

#endif

// ============================================================
// BOARD GPIO
// ============================================================

#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define TOUCH_BUTTON_GPIO       GPIO_NUM_NC
#define VOLUME_UP_BUTTON_GPIO   GPIO_NUM_NC
#define VOLUME_DOWN_BUTTON_GPIO GPIO_NUM_NC

// ============================================================
// ATHENA OLED SH1106 - I2C 128x64
// ESP32-S3 N16R8 GENERIC 44-PIN
// ============================================================

#define OLED_I2C_SDA_GPIO GPIO_NUM_41
#define OLED_I2C_SCL_GPIO GPIO_NUM_42

#define OLED_I2C_ADDRESS 0x3C

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

// ============================================================
// LEGACY GRAPHIC LCD CONFIGURATION
// ============================================================

#if 0

// Seluruh konfigurasi LCD TFT lama tetap disimpan
// tetapi tidak digunakan oleh Athena.

#endif

// ============================================================
// LAMP
// ============================================================

#define LAMP_GPIO GPIO_NUM_18

#endif // _BOARD_CONFIG_H_
