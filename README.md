# fOS 1.5.0

fOS 1.5.0 is a touchscreen firmware for ESP32-S3 CrowPanel devices.
It combines local apps (file manager, text editor, radio player, calculator, clock, settings) with Wi-Fi-based features (weather, NTP time sync) in one LVGL user interface.

## What's New in 1.5.0

- SD App Launcher now loads up to 4 apps dynamically from `/apps` when `StartAppLauncher()` is opened.
- New `AppContent` flow: launcher pages show only app names; app content opens in a dedicated content screen.
- App types for SD apps:
  - `type=text` for text/e-book style content from files (scrollable)
  - `type=button` for interactive button demo apps
- New example SD app package in `example app/`:
  - `create_example_app.sh`
  - sample apps: `hello_fos`, `button_demo`, `ebook_demo`
- Stability improvements:
  - fixed SD iteration handle leaks by explicitly closing files in directory loops
  - reduced heap fragmentation in AppContent rendering
  - Wi-Fi reconnect now uses a persistent worker task instead of repeated task create/delete

## Prerequisites

### Hardware
- ESP32-S3 CrowPanel (default project config is `CrowPanel_70`)
- microSD card
- USB cable for flashing

### Software
- Arduino IDE 2.x
- ESP32 board package: `esp32 by Espressif Systems`
- Arduino libraries:
  - `lvgl`
  - `LovyanGFX`
  - `ESP32-audioI2S` (provides `Audio.h`)

### Notes
- The active panel type is set in `LGFX_CrowPanel.h`.
- Supported panel defines in this project:
  - `CrowPanel_70`
  - `CrowPanel_50`
  - `CrowPanel_43`
- SD chip-select is set to `SD_CS = 10` in `fOS1.0.ino`. Adjust if your board requires a different pin.

## Installation

1. Install Arduino IDE 2.x.
2. Install `esp32 by Espressif Systems` in Board Manager.
3. Install required libraries: `lvgl`, `LovyanGFX`, `ESP32-audioI2S`.
4. Open `fOS1.0.ino` in Arduino IDE.
5. Select your ESP32-S3 target board and serial port.
6. Verify panel define in `LGFX_CrowPanel.h`.
7. Build and upload firmware.
8. Prepare SD card as described below and insert it.
9. Reboot the device.

## SD Card Setup

### Required format
- Filesystem: `FAT32`
- Partition scheme: MBR (recommended)

Use a clean FAT32 card for first boot. Avoid exFAT and NTFS.

### SD folder and file layout

The firmware creates missing folders automatically on startup.

- `/text`
  - Text files used by file manager and text editor
- `/music/files`
  - Local audio files (`.mp3`, `.wav`, `.ogg`)
- `/music/webradio/webradio.txt`
  - Web radio station list, one entry per line in this format:

```text
Station Name|https://stream-url.example
```

Example:

```text
Lofi Radio|https://example.com/lofi-stream
News Live|https://example.com/news-stream
```

- `/apps`
  - SD launcher apps (max 4 loaded)
  - each app is a folder with at least `app.cfg`
  - for text apps, add content file (for example `content.txt` or `book.txt`)

`app.cfg` keys:

```text
name=Display Name
type=text
content=content.txt
```

or button app:

```text
name=Button Demo
type=button
button_text=Press me
button_message=Button works
```

Optional system-managed files (created/used by firmware):

- `/system/wifi/wlans.txt`
  - Wi-Fi profiles as `SSID|PASSWORD` (one per line)
- `/system/timezone/timezone.txt`
  - Saved timezone rule

## Included Apps

- Home
- App Launcher
- App Content (SD app content screen)
- File Manager (SD file list, delete)
- Text Editor (open, create, overwrite-save)
- Radio (local file player + web radio)
- Weather (current weather + forecast)
- Clock (real-time clock, calendar, stopwatch)
- Calculator
- Settings (Wi-Fi, system info, timezone)

## First Boot Behavior

On boot, fOS initializes display, SD card, UI, Wi-Fi, audio, and cached data.
If SD is available, missing directories are created automatically.

## Troubleshooting

- SD not detected:
  - Reformat SD to FAT32
  - Check card seating and hardware wiring
  - Verify `SD_CS` value in `fOS1.0.ino`
- SD apps not visible in launcher:
  - Ensure app folders are inside `/apps`
  - Ensure each app has a valid `app.cfg`
  - Maximum 4 apps are loaded
- No local music files listed:
  - Place files in `/music/files`
  - Use supported extensions (`.mp3`, `.wav`, `.ogg`)
- No web radio stations listed:
  - Ensure `/music/webradio/webradio.txt` exists
  - Validate `name|url` format
- Weather not loading:
  - Check Wi-Fi connectivity
  - Ensure internet access for geolocation and weather API requests
