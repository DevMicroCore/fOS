# fOS 1.3.1

fOS 1.3.1 is a touchscreen firmware for ESP32-S3 CrowPanel devices.
It combines local apps (file manager, text editor, radio player, calculator, clock, settings) with Wi-Fi-based features (weather, NTP time sync) in one LVGL user interface.

## What's New in 1.3.1

- Weather forecast extended from 5 days to 7 days.
- Fixed an issue where the device could freeze when no Wi-Fi network was found.

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

Optional system-managed files (created/used by firmware):

- `/system/wifi/wlans.txt`
  - Wi-Fi profiles as `SSID|PASSWORD` (one per line)
- `/system/timezone/timezone.txt`
  - Saved timezone rule

## Included Apps

- Home
- App Launcher
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
- No local music files listed:
  - Place files in `/music/files`
  - Use supported extensions (`.mp3`, `.wav`, `.ogg`)
- No web radio stations listed:
  - Ensure `/music/webradio/webradio.txt` exists
  - Validate `name|url` format
- Weather not loading:
  - Check Wi-Fi connectivity
  - Ensure internet access for geolocation and weather API requests
