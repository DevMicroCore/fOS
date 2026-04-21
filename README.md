# fOS 1.1.0

fOS 1.0 is a touchscreen firmware project for ESP32-S3 CrowPanel devices.
It provides a graphical interface (LVGL) with apps for home/system info, text file management, audio playback (local files + web radio), clock/timezone, and settings.

## Prerequisites

### Hardware
- ESP32-S3 CrowPanel (configured in this project for `CrowPanel_70` by default)
- microSD card
- USB cable for flashing

### Software
- Arduino IDE 2.x (recommended)
- ESP32 board package for Arduino (Espressif)
- Required Arduino libraries:
  - `lvgl`
  - `LovyanGFX`
  - `Audio` (library providing `Audio.h`, e.g. ESP32 audio/I2S)

### Board/Project Notes
- This project targets ESP32-S3 panels (`LGFX_CrowPanel.h`).
- If you use a different CrowPanel size, switch the define in `LGFX_CrowPanel.h`:
  - `CrowPanel_70`
  - `CrowPanel_50`
  - `CrowPanel_43`
- SD chip select is currently set to `SD_CS = 10` in `fOS1.0.ino`. Change it if your hardware wiring differs.

## Installation Guide

1. Install Arduino IDE 2.x.
2. In Arduino IDE, install the **ESP32 by Espressif Systems** board package.
3. Install the required libraries (`lvgl`, `LovyanGFX`, `Audio`).
4. Open this project folder and load `fOS1.0.ino`.
5. Select the correct board (ESP32-S3 compatible target) and COM port.
6. Verify that `LGFX_CrowPanel.h` matches your panel size.
7. Flash/upload the firmware.
8. Insert a correctly formatted microSD card (see next section) and reboot.

## SD Card Format (Important)

Before first use, format the microSD card as:
- **FAT32** (recommended, MBR partition table)

Notes:
- Avoid exFAT/NTFS for this project.
- A clean FAT32 card is best for first boot.

## Where to Put Files on the SD Card

On startup, the firmware creates required folders automatically if missing.

### Text files
- Folder: `/text`
- Used by the Text/File Manager app
- Text files are stored/read as `.txt`

### Local music files
- Folder: `/music/files`
- Supported file extensions for listing/playback:
  - `.mp3`
  - `.wav`
  - `.ogg`

### Web radio station list
- File: `/music/webradio/webradio.txt`
- Format: one station per line
- Line format:

```text
Station Name|https://stream-url.example
```

Example:

```text
Lofi Radio|https://example.com/lofi-stream
News Live|https://example.com/news-stream
```

### Optional system files (managed by firmware/UI)
- Wi-Fi profiles: `/system/wifi/wlans.txt`
  - Format: `SSID|PASSWORD` (one per line)
- Timezone setting: `/system/timezone/timezone.txt`
  - Saved automatically by the firmware

## First Boot Behavior

At boot, fOS initializes SD, UI, Wi-Fi, audio, and app data.
If the SD card is detected, missing folders are created automatically.

## Troubleshooting

- **"SD card not detected"**:
  - Check card format (FAT32)
  - Check SD wiring/slot
  - Verify `SD_CS` pin in `fOS1.0.ino`
- **No music files shown**:
  - Put files in `/music/files`
  - Use supported extensions (`.mp3`, `.wav`, `.ogg`)
- **No web radio stations shown**:
  - Ensure `/music/webradio/webradio.txt` exists
  - Check `name|url` format per line
