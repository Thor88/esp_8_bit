/*hid_update
 * esp_8_bit
 *
 * Atari 8‑bit computers, NES and SMS game consoles on your TV with
 * nothing more than an ESP32 and a sense of nostalgia.  This version
 * includes optional support for modern wireless gamepads via the
 * Bluepad32 library.  When a supported controller (e.g. Xbox
 * Wireless) is paired, its inputs are mapped to the GUI navigation
 * keys and emulator actions.
 */

#include "esp_system.h"
#include "esp_spiffs.h"

// Include Arduino helpers for CPU frequency management.  These
// functions (setCpuFrequencyMhz() and getCpuFrequencyMhz()) are
// provided by the Arduino‑ESP32 core and replace the now‑removed
// rtc_clk_cpu_freq_set() and rtc_clk_cpu_freq_get() APIs.
#include "esp32-hal-cpu.h"

#define PERF  // some stats about where we spend our time
#include "src/emu.h"
#include "src/video_out.h"

// Include the Bluepad32 adapter.  This header declares the
// bluepad_setup() and bluepad_update() functions used below.
#include "bluepad_adapter.h"

//  Choose one of the video standards: PAL,NTSC
#define VIDEO_STANDARD NTSC

//  Choose one of the following emulators: EMU_NES,EMU_SMS,EMU_ATARI
#define EMULATOR EMU_NES

//  Many emus work fine on a single core (S2), file system access can cause a little flickering
//  #define SINGLE_CORE

// Create a new emulator, messy ifdefs ensure that only one links at a time
Emu* NewEmulator()
{
  return NewNofrendo(VIDEO_STANDARD);
}

Emu* _emu = 0;            // emulator running on core 0
uint32_t _frame_time = 0;
uint32_t _drawn = 1;
bool _inited = false;

void emu_init()
{
    // NES ROMs folder on SPIFFS
    std::string folder = "/NesRoms";
    gui_start(_emu,folder.c_str());
    _drawn = _frame_counter;
}

void emu_loop()
{
    // wait for blanking before drawing to avoid tearing
    video_sync();

    // Draw a frame, update sound, process hid events
    uint32_t t = xthal_get_ccount();
    gui_update();
    _frame_time = xthal_get_ccount() - t;
    uint8_t** vb = _emu->video_buffer();
    if (vb)
      _lines = vb;
    _drawn++;
}

// dual core mode runs emulator on comms core
void emu_task(void* arg)
{
    // Print CPU frequency using the Arduino helper functions.  The
    // rtc_clk_cpu_freq_get()/rtc_clk_cpu_freq_value() APIs used in the
    // original code were removed in ESP‑IDF 5.x.
    printf("emu_task %s running on core %d at %dmhz\n",
      _emu->name.c_str(), xPortGetCoreID(), getCpuFrequencyMhz());
    emu_init();
    for (;;)
      emu_loop();
}

esp_err_t mount_filesystem()
{
  printf("\n\n\nesp_8_bit\n\nmounting spiffs (will take ~15 seconds if formatting for the first time)....\n");
  uint32_t t = millis();
  esp_vfs_spiffs_conf_t conf = {
    .base_path = "",
    .partition_label = NULL,
    .max_files = 5,
    .format_if_mount_failed = true  // force?
  };
  esp_err_t e = esp_vfs_spiffs_register(&conf);
  if (e != 0)
    printf("Failed to mount or format filesystem: %d. Use 'ESP32 Sketch Data Upload' from 'Tools' menu\n",e);
  vTaskDelay(1);
  printf("... mounted in %d ms\n",millis()-t);
  return e;
}

void setup()
{
  // Set the CPU frequency to 240 MHz.  The rtc_clk_cpu_freq_set()
  // function was removed from modern ESP32 Arduino cores; use
  // setCpuFrequencyMhz() instead.
  setCpuFrequencyMhz(240);
  mount_filesystem();                       // mount the filesystem!
  _emu = NewEmulator();                     // create the emulator!
  // Seed a blank framebuffer so the A/V pump can start before a ROM is loaded
  {
    extern uint8_t** _lines;
    static uint8_t** _boot_lines = nullptr;
    static uint8_t* _boot_fb = nullptr;
    if (_lines == nullptr) {
      int w = _emu->width;
      int h = _emu->height;
      _boot_lines = (uint8_t**)MALLOC32(sizeof(uint8_t*) * h, "boot_lines");
      _boot_fb = (uint8_t*)MALLOC32(w * h, "boot_fb");
      memset(_boot_fb, 0, w * h);
      for (int y = 0; y < h; y++)
        _boot_lines[y] = _boot_fb + y * w;
      _lines = _boot_lines;
    }
  }
  bluepad_setup();                          // initialise Bluepad32 for gamepad support

  #ifdef SINGLE_CORE
  emu_init();
  video_init(_emu->cc_width,_emu->flavor,_emu->composite_palette(),_emu->standard); // start the A/V pump on app core
  #else
  xTaskCreatePinnedToCore(emu_task, "emu_task", EMULATOR == EMU_NES ? 5*1024 : 3*1024, NULL, 0, NULL, 0); // nofrendo needs 5k word stack, start on core 0
  #endif
}

#ifdef PERF
void perf()
{
  static int _next = 0;
  if (_drawn >= _next) {
    float elapsed_us = 120*1000000/(_emu->standard ? 60 : 50);
    _next = _drawn + 120;

printf("frame_time:%d drawn:%d displayed:%d blit_ticks:%d->%d, isr time:%2.2f%%\n",
    _frame_time/240, _drawn, _frame_counter,
    _blit_ticks_min, _blit_ticks_max,
    (_isr_us * 100) / elapsed_us);

    _blit_ticks_min = 0xFFFFFFFF;
    _blit_ticks_max = 0;
    _isr_us = 0;
  }
}
#else
void perf(){};
#endif

// this loop always runs on app_core (1).
void loop()
{
  #ifdef SINGLE_CORE
  emu_loop();
  #else
  // start the video after emu has started
  if (!_inited) {
    if (_lines) {
      printf("video_init\n");
      video_init(_emu->cc_width,_emu->flavor,_emu->composite_palette(),_emu->standard); // start the A/V pump
      _inited = true;
    } else {
      vTaskDelay(1);
    }
  }
  #endif

  // update modern gamepads via Bluepad32
  bluepad_update();

  // Dump some stats
  perf();
}


