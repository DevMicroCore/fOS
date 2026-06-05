# fOS 2.3.0

fOS 2.3.0 is a touchscreen firmware for ESP32-S3 CrowPanel devices.
This release adds a hardware sleep/display button workflow on `GPIO38`, while keeping the production OTA + Recovery architecture from 2.2.0.

## What's New in 2.3.0

- Hardware display/sleep button on `GPIO38`:
  - short press turns display output and backlight off
  - short press while display is off turns it back on
  - firmware reduces normal background work while display is off
- Long-press sleep override:
  - holding the button for 3 seconds forces ESP32-S3 Light Sleep
  - Light Sleep starts after releasing the button to avoid immediate wakeup
  - update installs and music playback block forced sleep
- Serial diagnostics added:
  - boot reset/wakeup cause is printed at startup
  - button level changes on `GPIO38` are logged
  - sleep/display state transitions are logged
- Existing 2.2 runtime features remain included:
  - production OTA + Recovery workflow with `app0/app1`
  - SD-staged update files in `/system/update/`
  - display brightness persistence
  - SD app runtime with calculator, radio, clock, and weather apps

## 2.2.0 Foundation

- OTA architecture with dedicated partitions:
  - `app0` for the main fOS firmware
  - `app1` for a minimal recovery firmware
  - no SPIFFS, SD card based update staging
- Boot safety logic:
  - `pending_update` + `boot_attempt_counter` tracking
  - automatic fallback to recovery after repeated failed boots
- Display settings:
  - brightness save in settings
  - minimum brightness is limited to `5%`
  - persistent value stored in `/system/display/brightness.txt`
  - value is loaded on startup

## Prerequisites

### Hardware
- ESP32-S3 CrowPanel (default project config is `CrowPanel_70`)
- Momentary button on `GPIO38` and `GND` for display/sleep control
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
- Active panel type is set in `LGFX_CrowPanel.h`.
- Supported panel defines in this project:
  - `CrowPanel_70`
  - `CrowPanel_50`
  - `CrowPanel_43`
- Partition layout for OTA/Recovery is provided in `partitions.csv`:
  - `app0`: `0x370000` (main system)
  - `app1`: `0x080000` (recovery)
- Arduino IDE board option `PSRAM` should be set to `OPI PSRAM`.
- SD chip-select is set to `SD_CS = 10` in `fOS2.0.ino`.
- `GPIO38` is used as the display/sleep button input in fOS 2.3.0.
- `GPIO38` can be electrically sensitive on some ESP32-S3 boards; short press uses a safe display-off idle mode, while 3-second long press explicitly opts into Light Sleep.

## Installation

1. Install Arduino IDE 2.x.
2. Install `esp32 by Espressif Systems` in Board Manager.
3. Install required libraries: `lvgl`, `LovyanGFX`, `ESP32-audioI2S`.
4. Open `fOS2.0.ino` in Arduino IDE.
5. Select your ESP32-S3 target board and serial port.
6. Set board options:
   - `Partition Scheme: use the custom project partition file (app0/app1 layout)`
   - `PSRAM: OPI PSRAM`
7. Verify panel define in `LGFX_CrowPanel.h`.
8. Build and upload firmware.
9. Prepare SD card as described below and insert it.
10. Reboot the device.

## SD Card Setup

### Required format
- Filesystem: `FAT32`
- Partition scheme: MBR (recommended)

Use a clean FAT32 card for first boot. Avoid exFAT and NTFS.

### Folder and file layout

The firmware creates missing system folders automatically on startup.

- `/apps`
  - SD apps (max 6 loaded)
- `/text`
  - text files for file manager and text editor
- `/music/files`
  - local audio files (`.mp3`, `.wav`, `.ogg`, `.aac`, `.m4a`)
- `/music/webradio/webradio.txt`
  - one station per line in this format:

```text
Sender Name|https://stream-url.example
```

Example:

```text
SomaFM Groove Salad|http://ice1.somafm.com/groovesalad-128-mp3
ByteFM|https://stream.byte.fm/stream/bytefm_www
```

Optional system-managed files:

- `/system/wifi/wlans.txt`
  - Wi-Fi profiles as `SSID|PASSWORD` (one per line)
- `/system/timezone/timezone.txt`
  - saved timezone rule
- `/system/display/brightness.txt`
  - saved display brightness in percent (`5..100`)
- `/system/update/update.bin`
  - staged OTA image for `app0`
- `/system/update/recovery.bin`
  - staged recovery image for `app1`

## SD App Format

Each app is a folder under `/apps/<app_name>/` with at least `app.cfg`.

### app.cfg keys

```text
name=Display Name
icon=Optional short tile text/symbol
type=ui|text|button|calculator|radio|clock|weather
scrollable=true|false
```

Additional keys by type:

- `type=ui`
  - `layout=layout.ui`
- `type=text`
  - `content=content.txt`
- `type=button`
  - `button_text=...`
  - `button_message=...`
- `type=clock`
  - no extra keys required
- `type=weather`
  - no extra keys required

### layout.ui (for `type=ui`)

One element per line, semicolon-separated fields.

```text
type=label;x=40;y=40;w=720;h=40;text=Hello
type=button;x=40;y=100;w=240;h=70;text=Start;bg=0x2095F6;fg=0xFFFFFF
type=textarea;x=40;y=190;w=420;h=140;text=Line1\nLine2
type=switch;x=500;y=120;value=true
type=checkbox;x=500;y=180;text=Option;value=false
type=panel;x=20;y=20;w=760;h=430;bg=0xF2F2F2
```

Supported `type=` values in layout lines:
- `label`
- `button`
- `textarea`
- `switch`
- `checkbox`
- `panel`

## Included Screens / Features

- Home
- Settings (Wi-Fi, timezone, system info)
- Storage Manager (folder navigation + delete file)
- Text Editor (open/create/overwrite-save)
- App Launcher (`AppL1` to `AppL6`)
- App Content runtime area
- Runtime apps:
  - Calculator
  - Radio
  - Clock
  - Weather
- Hardware power workflow:
  - short press on `GPIO38`: display/backlight off
  - short press while off: display/backlight on
  - 3-second press: force Light Sleep unless OTA or music is active

## Example Bundle

See `example app/` for ready-to-copy examples:
- `hello_fos`
- `button_demo`
- `ebook_demo`
- `ui_demo`
- `calculator_demo`
- `radio_demo`
- `clock_demo`
- `weather_demo`
- `text` (AppContent text editor app)

Webradio example list:
- `example app/music/webradio/webradio.txt`

## Troubleshooting

- SD not detected:
  - Reformat SD to FAT32
  - Check card seating and wiring
  - Verify `SD_CS` in `fOS2.0.ino`
- SD apps not visible:
  - Ensure app folders are under `/apps`
  - Ensure each app has valid `app.cfg`
  - Max 6 apps are shown
- No local files in radio app:
  - Place audio files in `/music/files`
- No web stations in radio app:
  - Ensure `/music/webradio/webradio.txt` exists
  - Validate `Sender|URL` format
- No weather data in weather app:
  - Ensure device has Wi-Fi connection
  - Check internet access to `ip-api.com` and `api.open-meteo.com`
  - Re-open weather app after Wi-Fi reconnect
- OTA update list appears late:
  - list loading is asynchronous and no longer blocks the UI
  - check serial output for `[OTA]` logs if files do not appear
- GPIO38 button does nothing:
  - wire the button between `GPIO38` and `GND`
  - open Serial Monitor and check for `Sleep button GPIO38 changed`
  - if no level changes appear, verify the actual header pin and wiring
- Device boots after long-press sleep:
  - this means the board likely reset during Light Sleep wakeup
  - check `Boot diagnostics: reset=... wake=...` in Serial Monitor
  - use short press for the stable display-off mode, or move the button to a safer GPIO for real Light Sleep
