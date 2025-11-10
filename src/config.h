// Centralized configuration for ESP_8_BIT (NES build)
#pragma once

// Audio options
#ifndef AUDIO_DITHER
#define AUDIO_DITHER 0
#endif
#ifndef AUDIO_GAIN_NUM
#define AUDIO_GAIN_NUM 1
#endif
#ifndef AUDIO_GAIN_DEN
#define AUDIO_GAIN_DEN 1
#endif

// PWM setup defaults (used by LEDC)
#ifndef AUDIO_PWM_FREQ
#define AUDIO_PWM_FREQ 1250000
#endif
#ifndef AUDIO_PWM_RES_BITS
#define AUDIO_PWM_RES_BITS 6
#endif

// Audio pin (can be remapped to a free GPIO)
#ifndef AUDIO_PIN
#define AUDIO_PIN 26
#endif

// Keycode mappings used by GUI and adapters
#define KEYCODE_UP            82
#define KEYCODE_DOWN          81
#define KEYCODE_LEFT          80
#define KEYCODE_RIGHT         79

#define KEYCODE_START         40   // Return
#define KEYCODE_SELECT        43   // Tab

#define KEYCODE_MENU_TOGGLE   58   // F1

#define KEYCODE_A_EMU        225   // Emu A (Left Shift)
#define KEYCODE_B_EMU        226   // Emu B (Option)

// Bluepad32 options
#ifndef BLUEPAD_ENABLE_NEW_CONNECTIONS
#define BLUEPAD_ENABLE_NEW_CONNECTIONS 1
#endif
#ifndef BLUEPAD_FORGET_KEYS_ON_BOOT
#define BLUEPAD_FORGET_KEYS_ON_BOOT 1
#endif
// Filesystem selection (mirrors Arduino nofrendo's hw_config)
#ifndef FSROOT
#define FSROOT "/"
#endif

#define FILESYSTEM_SPIFFS            0
#define FILESYSTEM_FFAT              1
#define FILESYSTEM_SD_MMC_1BIT       2
#define FILESYSTEM_SD_MMC_4BIT       3
#define FILESYSTEM_SD_SPI_DEFAULT    4
#define FILESYSTEM_SD_SPI_CUSTOM     5

#ifndef FILESYSTEM_IMPL
#define FILESYSTEM_IMPL FILESYSTEM_SD_SPI_DEFAULT
#endif

#ifndef FILESYSTEM_SPIFFS_FORMAT_ON_FAIL
#define FILESYSTEM_SPIFFS_FORMAT_ON_FAIL false
#endif

#ifndef FILESYSTEM_FFAT_FORMAT_ON_FAIL
#define FILESYSTEM_FFAT_FORMAT_ON_FAIL false
#endif

#ifndef FILESYSTEM_SD_SPI_DEFAULT_SS
#define FILESYSTEM_SD_SPI_DEFAULT_SS SS
#endif
#ifndef FILESYSTEM_SD_SPI_DEFAULT_FREQ_HZ
#define FILESYSTEM_SD_SPI_DEFAULT_FREQ_HZ 8000000
#endif

#ifndef FILESYSTEM_SD_SPI_BUS
#define FILESYSTEM_SD_SPI_BUS HSPI
#endif
#ifndef FILESYSTEM_SD_SPI_CS
#define FILESYSTEM_SD_SPI_CS 5
#endif
#ifndef FILESYSTEM_SD_SPI_SCLK
#define FILESYSTEM_SD_SPI_SCLK 18
#endif
#ifndef FILESYSTEM_SD_SPI_MISO
#define FILESYSTEM_SD_SPI_MISO 19
#endif
#ifndef FILESYSTEM_SD_SPI_MOSI
#define FILESYSTEM_SD_SPI_MOSI 23
#endif
#ifndef FILESYSTEM_SD_SPI_FREQ_HZ
#define FILESYSTEM_SD_SPI_FREQ_HZ 8000000
#endif
