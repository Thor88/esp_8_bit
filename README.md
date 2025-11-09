# ESP_8_BIT: NES on your TV with an ESP32
## Supports NTSC/PAL color composite video output and Bluetooth gamepads via Bluepad32

![ESP_8_BIT](img/esp8bit.jpg)

**ESP_8_BIT** is designed to run on the ESP32 within the Arduino IDE framework. See it in action on [Youtube](https://www.youtube.com/watch?v=qFRkfeuTUrU). Schematic is pretty simple:

```
    -----------
    |         |
    |      25 |------------------> video out
    |         |
    |      18 |---/\/\/\/----|---> audio out
    |         |     1k       |
    |         |             ---
    |  ESP32  |             --- 10nf
    |         |              |
    |         |              v gnd
    |         |
    |         |     3.3v <--+-+   IR Receiver
    |         |      gnd <--|  )  TSOP4838 etc.
    |       0 |-------------+-+   (Optional)
    -----------

```
Audio is on pin 18 by default but can be remapped.

Before you compile the sketch set the video standard:
```
//  Choose one of the video standards: PAL or NTSC
#define VIDEO_STANDARD NTSC
```
Build and run the sketch and connect to an old-timey composite input. The first time the sketch runs in will auto-populate the file system with a selection of fine old and new homebrew games and demos. This process only happens once and takes about ~20 seconds so don't be frightened by the black screen.

# The Emulated

## Nintendo Entertainment System
Based on [nofrendo](http://www.baisoku.org/).

| Keyboard | NES |
| ---------- | ----------- |
| Arrow Keys | D-Pad |
| Left Shift | Button A |
| Option | Button B |
| Return | Start |
| Tab | Select |

## Controllers (Bluepad32)
Supported Bluetooth gamepads are handled via Bluepad32.

- D-pad: navigate menu / move
- A: select in menu; NES A in-game
- B: back (closes menu) in menu; NES B in-game
- Start: Start
- Select: Select
- Y: toggle menu

# How it works

## Composite Video
Much as been written on generating color composite video with microcontrollers. [Rickard Gunée](https://elinux.org/images/e/eb/Howtocolor.pdf) kicked it off in 2003, with many fun variants emerging over the years. I enjoyed building the [RBox](https://hackaday.com/2010/10/27/smallest-gaming-console-ever-ever/) in 2010 that used a Cortex M0 and a R2R DAC to generate color NTSC, then in 2015 the [Arduinocade](https://hackaday.com/2015/09/17/retro-games-on-arduinocade-just-shouldnt-be-possible/) on an Arduino with upgraded crystal that used its SPI port to create the colors. Now that 2020 has given us all a little extra time at home its time to do another one.

Now that we have fancy devices like the ESP8266/ESP32 we can be a little more ambitious. We have an order of magnitude more compute, two orders of magnitude more RAM, and three orders of magnitude more storage than an Arduino. And we have lots of new exotic new peripherals to play with. Using this newfound power [CNLohr](https://hackaday.com/2016/03/01/color-tv-broadcasts-are-esp8266s-newest-trick/) pulled off an amazing 1-bit Nyquist folding trick to produce a color NTSC broadcast on an ESP8266. [Bitluni](https://bitluni.net/esp32-color-pal) clocked the ESP32's DAC up to 13.3333Mhz and manged to create color PAL. Bitluni overview of how PAL works, his online visualization tools, his findings from spelunking around in the ESP32, and his other great projects are really worth exploring.

The principle figure of merit for generating nice looking PAL or NTSC color is the accuracy and stability of the synthesized color carrier. Bitluni's clever technique used DDS to produce PAL carrier with ~3 DAC samples per cycle - close enough for most TVs to lock. Phase Alternate Line helps a lot here.

NTSC is a lot more fussy about phase stability, jitter and frequency of its color carrier. DAC + DDS at 13.33Mhz produces a beautiful looking waveform but very sketchy color if any on most TVs.

Turns out the ESP32 has a great tool for creating rock solid color carriers: The **Audio Phase Locked Loop**.

### The magic of the Audio PLL
The ESP32 has a ultra-low-noise fractional-N PLL. It can be tuned to produce DAC sample rates up to **~20Mhz with very accurate frequency control**:

![Audio PLL Formula](img/apll.png)
*Fig.1 Audio PLL formula permits precise control over DAC frequency*

Its intended use is to be able to synchronize audio sources running at slightly different frequencies but is just the thing for creating accurate color carriers.

| Standard | (Carrier Frequency)*4 | APLL Frequency |
| ---------- | ----------- | ----------- |
| NTSC | 14.318182Mhz | 14.318180Mhz |
| PAL | 17.734475Mhz | 17.734476Mhz |

As you can see the APLL frequencies can be tuned to be incredibly close to the desired frequencies. Now we have a DAC running at an integer multiple of the color carrier we are off to the races with stable color on NTSC and PAL. From this point it is easy to construct color palettes that map indexed color to carrier phases / amplitudes.

![colorburst](img/colorburst.png)

*Fig.1 NTSC Sync and Colorburst*

The APLL is great for lots of other applications. It gives you considerably more headroom above 13.33Mhz - 20Mhz is rock solid, lots of potential for DDS/RF etc.

![312.5khz dds signal from DAC with 64 step sine table at 20mhz](img/312.5khz_dds_20mhz.png)

*Fig.2 64 step DDS sinewave from DAC driven at 20Mhz*

![10mhz signal from DAC at 20mhz](img/10mhz_dds_20mhz.png)

*Fig.3 20Mhz DAC output still looks nice and clean*

### No Free Lunch
Exciting though it is, the APLL has one drawback. It seems that if you use a non integer denominator (`sdm1 != 0 || smd2 != 0`) in the APLL settings at high frequencies there seems to be a clock domain conflict between the DAC and I2S. If you split the DACs and try to use the `I2S_CONF_SINGLE_DATA_REG` to write audio samples to the other DAC channel this conflict manifests as dropouts in both DAC outputs. I would love to know what is going on here: It might have something to do with running the DAC way out of spec. Perhaps someone at Espressif will take pity on me an let me know.

APLL / DAC video looks great but it appears we need another source of audio besides the second DAC channel.

## Making noises - lots of options
There are a lot of different options to create sound with an ESP32 besides the DAC. I2S1, SPI, PWM ....

### PDM
If you want to build a high quality 1-bit DAC then Pulse Density Modulation is a great choice. The ESP32 I2S0 hardware has one built-in, unfortunately we are using I2S0 for video. It is fast/easy to do your own modulation and send it out any high speed digital channel: SPI with DMA, bitbanging gpio etc. Using both SPI ports would give you stereo.

[Jan Ostman](https://www.hackster.io/janost/audio-hacking-on-the-esp8266-fa9464) has done some lovely work on this on microcontrollers large and small. Also check out [Super Audio CD](https://en.wikipedia.org/wiki/Super_Audio_CD) that used PDM right before people gave up on the idea of physical media.
PDM is used in digital mems microphones and is a great way of attaching lots of microphones to microcontrollers. You could easily attach 8 of them to a gpio port on a ESP32 and build a nice 3D beamforming microphone: just add a few CIC filters and a little [delay-and-sum](http://www.labbookpages.co.uk/audio/beamforming/delaySum.html) and off you go.

### PWM
PWM is really a special case of PDM. It does not do a great job of shaping noise but for our purposes it is really convenient. With up to 16 outputs from the LED PWM hardware stereo would be no problem and it only takes a couple of lines of code to get it going. Its limited dynamic range is more than enough to reproduce those classic sounds from the 80's.

You will want to add a simple rc filter to the output pin of either PWM or PDM to avoid becoming a tiny radio station and interfering with the nice video are producing.

## Bringing it Together

The audio/video system uses a double buffered I2S DMA to send video data line by line to the DAC. The interrupt keeps time at the line rate (15720hz for NTSC, 15600hz for PAL). A single audio sample is fed to the LED PWM and a single line of video is converted from index color to phase/amplitude at each interrupt.

This a/v pump is fed by the emulator running asynchronously producing frames of video and audio. The emulator may be running on a different core and may occasionally take longer than a frame time to produce a frame (SPI paging / FS etc). The interrupt driven pump won't care, it just keeps emitting the last frame.

## Controllers

Bluepad32 handles pairing and input for modern Bluetooth gamepads.

## Big Cartridges

How do you fit a 512k game cartridge into a device that has 384k of RAM, nearly all of which is consumed with screen buffers and emulator memory?

Short answer is you don't. When a large cart is selected it gets copied into `CrapFS`; an aptly named simple filesystem that takes over the `app1` partition at first boot. One copied `CrapFS` uses `esp_partition_mmap()` to map the part of the partition occupied by the cartridge into the data space of the CPU.

> We are using an Audio PLL to create color composite video and a LED PWM peripheral to make the audio, a Bluetooth radio or a single GPIO pin for the joysticks and keyboard, and gpio for the IR even though there is a perfectly good peripheral for that. We are using virtual memory on a microcontroller. And it all fits on a single core. Oh how I love the ESP32.


# Keyboards & Controllers
This build targets gamepads via Bluepad32 only.

# Time to Play

If you would like to upload your own media copy them into the appropriate subfolder named for each of the emulators in the data folder. Note that the SPIFFS filesystem is fussy about filenames, keep them short, no spaces allowed. Use '[ESP32 Sketch Data Upload](https://randomnerdtutorials.com/install-esp32-filesystem-uploader-arduino-ide/)' from the 'Tools' menu to copy a prepared data folder to ESP32.

Play through the included demos. Load up your own. Finally get around to finishing that NES classic.

Enjoy,

rossum
