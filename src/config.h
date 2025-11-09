// Centralized configuration for ESP_8_BIT (NES-only build)
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
#define AUDIO_PIN 18
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
// Optional: refresh ROM list while menu is open
// Optional: refresh ROM list while menu is open
#ifndef KEYCODE_REFRESH
#define KEYCODE_REFRESH 62  // F5
#endif
