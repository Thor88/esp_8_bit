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
#include "src/config.h"
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <string>
#include <FS.h>
#include <SPIFFS.h>
#include <SD.h>
#include <SD_MMC.h>
#include <FFat.h>
#include <SPI.h>

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

//  Choose one of the video standards: PAL,NTSCc:\Users\turlo\Documents\GitHub\esp_8_bit\src\config.h
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

static std::string normalized_fs_root()
{
  std::string root = FSROOT;
  if (root.empty())
    root = "/";
  if (root.front() != '/')
    root.insert(root.begin(), '/');
  if (root.size() > 1 && root.back() == '/')
    root.pop_back();
  return root;
}

static std::string fs_join(const std::string& base, const std::string& child)
{
  if (base.empty() || base == "/")
    return "/" + child;
  return base + "/" + child;
}

void emu_init()
{
    std::string folder = fs_join(normalized_fs_root(), _emu->name);
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
    _lines = _emu->video_buffer();
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
  printf("\n\n\nesp_8_bit\n\nmounting filesystem at %s...\n", FSROOT);
  uint32_t start = millis();
  bool mounted = false;
  const char* fs_name = "FS";

#if FILESYSTEM_IMPL == FILESYSTEM_SPIFFS
  fs_name = "SPIFFS";
  mounted = SPIFFS.begin(FILESYSTEM_SPIFFS_FORMAT_ON_FAIL, FSROOT);
#elif FILESYSTEM_IMPL == FILESYSTEM_FFAT
  fs_name = "FFat";
  mounted = FFat.begin(FILESYSTEM_FFAT_FORMAT_ON_FAIL, FSROOT);
#elif FILESYSTEM_IMPL == FILESYSTEM_SD_MMC_1BIT
  fs_name = "SD_MMC (1-bit)";
  mounted = SD_MMC.begin(FSROOT, true);
  if (!mounted)
    mounted = SD_MMC.begin(FSROOT, true);
#elif FILESYSTEM_IMPL == FILESYSTEM_SD_MMC_4BIT
  fs_name = "SD_MMC (4-bit)";
  mounted = SD_MMC.begin(FSROOT, false);
  if (!mounted)
    mounted = SD_MMC.begin(FSROOT, false);
#elif FILESYSTEM_IMPL == FILESYSTEM_SD_SPI_DEFAULT
  fs_name = "SD (SPI default)";
  mounted = SD.begin(FILESYSTEM_SD_SPI_DEFAULT_SS, SPI, FILESYSTEM_SD_SPI_DEFAULT_FREQ_HZ, FSROOT);
#elif FILESYSTEM_IMPL == FILESYSTEM_SD_SPI_CUSTOM
  fs_name = "SD (SPI custom)";
  static SPIClass spi(FILESYSTEM_SD_SPI_BUS);
  spi.begin(FILESYSTEM_SD_SPI_SCLK, FILESYSTEM_SD_SPI_MISO, FILESYSTEM_SD_SPI_MOSI, FILESYSTEM_SD_SPI_CS);
  mounted = SD.begin(FILESYSTEM_SD_SPI_CS, spi, FILESYSTEM_SD_SPI_FREQ_HZ, FSROOT);
#else
#error Unknown FILESYSTEM_IMPL selection
#endif

  if (!mounted) {
    printf("Failed to mount %s. Please check filesystem settings or media.\n", fs_name);
    return ESP_FAIL;
  }

  vTaskDelay(1);
  printf("... %s mounted in %d ms\n", fs_name, millis()-start);
  return ESP_OK;
}

void setup()
{
  // Set the CPU frequency to 240 MHz.  The rtc_clk_cpu_freq_set()
  // function was removed from modern ESP32 Arduino cores; use
  // setCpuFrequencyMhz() instead.
  setCpuFrequencyMhz(240);
  if (mount_filesystem() != ESP_OK) {
    printf("Filesystem mount failed. Stopping setup.\n");
    while (true) {
      delay(1000);
    }
  }
  _emu = NewEmulator();                     // create the emulator!
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
