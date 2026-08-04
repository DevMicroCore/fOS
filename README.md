# fOS 3.0.0

fOS 3.0.0 is a touchscreen firmware for ESP32-S3 CrowPanel devices.

---

# What's New in 3.0.0

## 🎨 Theme System

fOS now supports customizable system themes.

Features:

* Select between **Blue**, **Orange**, and **Green** themes.
* Themes are applied consistently across all native applications.
* SD applications also automatically use the selected theme.
* Custom UI applications can use theme-aware colors:

```text
bg=theme
bg=surface
fg=contrast
```

These values automatically adapt to the currently selected system theme.

---

## 📁 File System

The file system has been expanded.

New features:

* Create your own folders.
* The Text application now supports subfolders.
* Files can be saved and loaded using paths such as:

```text
notes/todo.txt
projects/demo/readme.txt
```

---

## 📻 Radio Application

The Radio app has been redesigned.

New features:

* Browse folders inside the File Player.
* Open folders using the **">"** button.
* Volume slider.
* Playback progress bar.
* Seek within audio files using the progress bar.

---

## ⏰ Clock

Improvements:

* Calendar now starts with **Monday**.
* Timers can play an optional alarm sound from:

```text
/system/timer.mp3
```

---

## 🚀 Performance

System improvements:

* Faster boot time.
* Reduced RAM usage.
* Improved overall responsiveness.

---

## 🖥 Applications

* Updated UI Demo application.
---

# What's New in 3.0.0-beta.1

* Theme selection (Blue, Orange, Green) for native and SD apps.
* Theme-aware UI: `bg=theme`, `bg=surface`, `fg=contrast`.
* Custom folders.
* Text app supports subfolders using paths like `folder/file.txt`.
* Updated UI Demo.
* Clock calendar starts on Monday.

---

# What's New in 2.5.0

* Wi-Fi improvements:

  * Wi-Fi can now be enabled or disabled directly from Settings.
  * The selected Wi-Fi state is saved automatically.
  * The saved state is restored during every boot.

* Improved Wi-Fi setup:

  * available wireless networks are shown in a dropdown list
  * SSIDs can be selected directly after scanning
  * manual SSID entry is no longer required for nearby networks

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
2. Install `esp32 2.0.15 by Espressif Systems`.
3. Install the required libraries:

   * lvgl 8.3.11
   * LovyanGFX
   * ESP32-audioI2S
4. Open `fOS2.0.ino`.
5. Select your ESP32-S3 board.
6. Configure:

   * custom partition scheme (`app0/app1`) or, if it's not available, the Huge App
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

Theme-aware color values are also supported:

```
bg=theme
bg=surface
fg=contrast
```

If `bg` is omitted for `button`, `panel`, `textarea`, `switch`, or `checkbox`, the default background stays black (`0x000000`).

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
    * Enable / Disable Wi-Fi
    * Scan for nearby networks
    * Select network from dropdown
    * Password entry
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

## v3.0.0

### Added

* Selectable Blue, Orange and Green themes.
* Theme support for native applications.
* Theme support for SD applications.
* Theme-aware UI values:
  * `bg=theme`
  * `bg=surface`
  * `fg=contrast`
* Custom folder creation.
* Text application subfolder support.
* Timer alarm sound support using `/system/timer.mp3`.
* Radio application folder navigation.
* Radio application volume slider.
* Radio application playback progress bar with seeking.

### Changed

* Updated UI Demo application.
* Calendar now starts on Monday.
* Faster boot process.
* Reduced RAM usage.
* Improved system responsiveness.

## v2.5.0

* Added Wi-Fi enable/disable option.
* Wi-Fi state is saved and restored after reboot.
* Added dropdown list for selecting scanned Wi-Fi networks.
* Simplified Wi-Fi configuration.

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
