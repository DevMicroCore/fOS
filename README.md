# fOS 2.4.0

fOS 2.4.0 is a touchscreen firmware for ESP32-S3 CrowPanel devices.

This release introduces a safer file management workflow with a delete confirmation dialog and expands the Clock application with an integrated countdown timer while keeping the production OTA + Recovery architecture introduced in earlier releases.

---

# What's New in 2.4.0

* File Manager improvements:

  * confirmation dialog before deleting files
  * prevents accidental file removal

* Clock application:

  * integrated countdown timer
  * start, stop and reset controls

* Hardware display/sleep button on `GPIO38`:

  * short press turns display output and backlight off
  * short press while display is off turns it back on
  * firmware reduces normal background work while display is off

* Long-press sleep override:

  * holding the button for 3 seconds forces ESP32-S3 Light Sleep
  * Light Sleep starts after releasing the button to avoid immediate wakeup
  * update installs and music playback block forced sleep

* Serial diagnostics:

  * boot reset/wakeup cause is printed at startup
  * button level changes on `GPIO38` are logged
  * sleep/display state transitions are logged

* Existing runtime features remain included:

  * production OTA + Recovery workflow with `app0/app1`
  * SD-staged update files in `/system/update/`
  * display brightness persistence
  * SD app runtime with calculator, radio, clock (including timer), and weather apps

---

# 2.2 Foundation

* OTA architecture with dedicated partitions:

  * `app0` for the main fOS firmware
  * `app1` for a minimal recovery firmware
  * no SPIFFS, SD card based update staging

* Boot safety logic:

  * `pending_update` + `boot_attempt_counter` tracking
  * automatic fallback to recovery after repeated failed boots

* Display settings:

  * brightness save in settings
  * minimum brightness is limited to `5%`
  * persistent value stored in `/system/display/brightness.txt`
  * value is loaded on startup

---

# Prerequisites

## Hardware

* ESP32-S3 CrowPanel (default project config is `CrowPanel_70`)
* Momentary button on `GPIO38` and `GND` for display/sleep control
* microSD card
* USB cable for flashing

## Software

* Arduino IDE 2.x
* ESP32 board package:

  * `esp32 by Espressif Systems`

Required libraries:

* `lvgl`
* `LovyanGFX`
* `ESP32-audioI2S` (provides `Audio.h`)

## Notes

* Active panel type is selected in `LGFX_CrowPanel.h`.
* Supported panel definitions:

  * `CrowPanel_70`
  * `CrowPanel_50`
  * `CrowPanel_43`
* OTA partition layout is defined in `partitions.csv`.
* Arduino IDE should use:

  * `PSRAM: OPI PSRAM`
* SD chip-select is configured as:

  * `SD_CS = 10`
* `GPIO38` is used as the display/sleep button.
* Short press disables only the display while the system continues running.
* Long press requests Light Sleep.

---

# Installation

1. Install Arduino IDE 2.x.
2. Install `esp32 by Espressif Systems`.
3. Install the required libraries:

   * lvgl
   * LovyanGFX
   * ESP32-audioI2S
4. Open `fOS2.0.ino`.
5. Select your ESP32-S3 board.
6. Configure:

   * custom partition scheme (`app0/app1`)
   * `PSRAM = OPI PSRAM`
7. Verify the correct panel define in `LGFX_CrowPanel.h`.
8. Compile and upload.
9. Prepare the SD card.
10. Insert the SD card.
11. Reboot the device.

---

# SD Card Setup

## Required format

* FAT32
* MBR partition table recommended

Avoid exFAT and NTFS.

The firmware automatically creates missing system folders during startup.

Required folders:

```
/apps
/text
/music/files
/music/webradio
/system
```

Example web radio file:

`/music/webradio/webradio.txt`

```
Station Name|https://stream-url.example
```

Example:

```
SomaFM Groove Salad|http://ice1.somafm.com/groovesalad-128-mp3
ByteFM|https://stream.byte.fm/stream/bytefm_www
```

Optional system files:

```
/system/wifi/wlans.txt
/system/timezone/timezone.txt
/system/display/brightness.txt
/system/update/update.bin
/system/update/recovery.bin
```

---

# SD App Format

Each application is stored inside:

```
/apps/<app_name>/
```

and must contain at least:

```
app.cfg
```

## app.cfg

```
name=Display Name
icon=Optional Tile Icon
type=ui|text|button|calculator|radio|clock|weather
scrollable=true|false
```

Additional keys:

### UI

```
layout=layout.ui
```

### Text

```
content=content.txt
```

### Button

```
button_text=...
button_message=...
```

### Clock

No additional settings required.

### Weather

No additional settings required.

---

# layout.ui

Example:

```
type=label;x=40;y=40;w=720;h=40;text=Hello
type=button;x=40;y=100;w=240;h=70;text=Start;bg=0x2095F6;fg=0xFFFFFF
type=textarea;x=40;y=190;w=420;h=140;text=Line1\nLine2
type=switch;x=500;y=120;value=true
type=checkbox;x=500;y=180;text=Option;value=false
type=panel;x=20;y=20;w=760;h=430;bg=0xF2F2F2
```

Supported elements:

* label
* button
* textarea
* switch
* checkbox
* panel

---

# Included Screens / Features

* Home
* Settings

  * Wi-Fi
  * Timezone
  * System Information
* Storage Manager

  * folder navigation
  * delete confirmation before removing files
* Text Editor

  * create
  * edit
  * overwrite existing files
* App Launcher (`AppL1`–`AppL6`)
* App Content runtime

Included applications:

* Calculator
* Radio
* Clock

  * digital clock
  * integrated countdown timer
* Weather

Hardware features:

* GPIO38 display control
* Display backlight control
* Light Sleep
* OTA + Recovery
* Persistent display brightness
* SD card application system

---

# Example Bundle

Included examples:

* hello_fos
* button_demo
* ebook_demo
* ui_demo
* calculator_demo
* radio_demo
* clock_demo
* weather_demo
* text

Web radio example:

```
example app/music/webradio/webradio.txt
```

---

# Troubleshooting

## SD card not detected

* Format as FAT32.
* Check SD wiring.
* Verify `SD_CS`.

## Apps not visible

* Ensure every app has a valid `app.cfg`.
* Store apps inside `/apps`.
* Maximum of six apps are displayed.

## Audio files missing

Place music files inside:

```
/music/files
```

## Web radio stations missing

Verify:

```
/music/webradio/webradio.txt
```

Format:

```
Station|URL
```

## Weather unavailable

* Connect to Wi-Fi.
* Verify internet connectivity.
* Restart the Weather app.

## OTA list appears slowly

OTA loading runs asynchronously.

Check the Serial Monitor for `[OTA]` messages.

## GPIO38 button not working

* Connect the button between `GPIO38` and `GND`.
* Open the Serial Monitor.
* Verify GPIO state changes.

## Device restarts after Light Sleep

Some CrowPanel revisions reset when waking from Light Sleep.

Short press remains the recommended and most reliable display-off mode.

---

# Version History

## v2.4.0

* Added delete confirmation dialog for file deletion.
* Added integrated countdown timer to the Clock application.
* Improved usability and data safety.

## v2.3.0

* Hardware display/sleep button.
* Light Sleep support.
* Display-off idle mode.
* Extended serial diagnostics.

## v2.2.0

* OTA + Recovery architecture.
* Automatic rollback.
* Persistent display settings.
