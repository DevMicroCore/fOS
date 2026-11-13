# fOS 4.1.1

fOS is a touch interface for the **7-inch CrowPanel with ESP32-S3** and the
**7-inch Waveshare with ESP32-P4**. It combines built-in applications,
SD card management and installable fScript apps with Wi-Fi, audio and
firmware updates.

## Features available on both boards

The following features are included in both the CrowPanel and Waveshare projects.

**4.1.1:** App downloads and installations now run in a separate FreeRTOS
worker instead of blocking the LVGL UI. See [release notes](docs/RELEASE_NOTES_4.1.1.md).
The precompiled binaries in `firmware/` are inherited from the earlier package
and **do not contain these 4.1.1 fixes**. Build and export a new binary for
**each board** before performing OTA; do not publish the inherited binaries as
4.1.1.
**2026-11-11 follow-up:** Waveshare app deletion is now also handled by the
AppStore worker to prevent `loopTask` stack overflows. CrowPanel failed HTTPS
connections now report Wi-Fi and TLS-heap diagnostics. See the release notes
for what is and is not verified on hardware.

| Area | Features | Detailed documentation |
| --- | --- | --- |
| Interface and launcher | Touch controls, on-screen keyboards, home screen and launching installed apps | [Apps](docs/APPS.md), [Keyboards](docs/APP_RUNTIME.md) |
| Settings | Theme selection, saved display brightness, time zone and system information | [Configuration](docs/SD_AND_CONFIGURATION.md) |
| Wi-Fi | Network scanning, multiple saved network profiles, enable/disable control and automatic reconnection | [Configuration](docs/SD_AND_CONFIGURATION.md) |
| Clock and calendar | Date and time with NTP synchronization over Wi-Fi, time zone selection, monthly calendar, stopwatch and countdown timer | [Configuration](docs/SD_AND_CONFIGURATION.md) |
| Storage management | FAT32 microSD, folder navigation and deletion with confirmation; shared directories for apps and user files | [SD card](docs/SD_AND_CONFIGURATION.md) |
| Text editor | Open, edit and save text files; file selection and subfolders within `/text` | [Text app](docs/APPS.md#texteditor) |
| Calculator and drawing | Calculator and a fScript drawing app with a touch canvas | [Apps](docs/APPS.md) |
| Weather | Location search and weather forecasts with an internet connection | [Weather app](docs/APPS.md) |
| Music and web radio | Local music with folder navigation, internet radio stations, play/stop, volume and seeking in local files | [Radio and audio](docs/FSCRIPT_RADIO_AUDIO.md) |
| Email | IMAP or POP3 reception, SMTP sending over direct TLS, separate inbox/outgoing lists and a local message cache | [Email setup](docs/SD_AND_CONFIGURATION.md#email-setup) |
| App Store | Install, update and uninstall apps; select compatible versions. App updates preserve existing `appdata` folders | [App data and updates](docs/APP_RUNTIME.md), [Store files](docs/GITHUB.md) |
| Firmware updates | OTA updates staged on the SD card with a separate recovery application; dedicated firmware paths for each board | [Firmware and recovery](docs/FIRMWARE_RECOVERY.md) |
| fScript apps | Compiled app logic in the VM, UI events, timers, HTTP/JSON, file and audio access controlled by app permissions | [English manual](docs/FSCRIPT_FOSC_VM_MANUAL_EN.md), [German manual](docs/FSCRIPT_FOSC_VM_HANDBUCH_DE.md) |
| Relative app layouts | The same `layout.ui` files adapt to 800×480 and 1024×600, including nested dialogs, keyboards and canvases | [UI layouts](docs/UI_LAYOUTS.md) |
| Credentials and app permissions | Device-bound encryption of Wi-Fi/email passwords, separate read/write permissions per app and a protected `/system` directory | [Credentials](docs/SD_AND_CONFIGURATION.md#email-setup), [App permissions](docs/APP_RUNTIME.md) |

Text editing, the calculator, drawing and local music do not require Wi-Fi.
Weather, web radio, mail transfers, the online App Store and online updates
require an appropriate network connection. The clock first needs a valid
system time; Waveshare-specific RTC restoration is described below.

The radio and text applications are also supplied as fScript apps. Native
demo entries such as `radio_demo` remain available. You can compile your own
apps with **fosc** and test them on a computer with **fosrun**, including
previews for both display sizes: [fosc and fosrun](docs/FOSC_FOSRUN.md).

## Board-specific and exclusive features

**“Waveshare only” refers to the implementation in this project.**
It does not describe every feature that could be developed for other hardware.

| Feature | CrowPanel ESP32-S3 | Waveshare ESP32-P4 | Details |
| --- | --- | --- | --- |
| SD card exposed to a computer as a USB drive | Not implemented in the included sketch | **Waveshare only:** Set USBStorageSwitch in Connections and apply it with Save connections. Local SD access pauses while the computer owns the card | [USB storage](docs/WAVESHARE.md#usb-storage-of-the-microsd-card) |
| RTC/VBAT time restoration before Wi-Fi starts | No equivalent battery-backed RTC path integrated | **Waveshare only:** Capture a time anchor after NTP synchronization and restore time at startup using the live RTC counter | [RTC and battery operation](docs/RTC.md) |
| Prepared battery backup sleep | Not implemented | **Waveshare only:** Hold BOOT/GPIO0 for three seconds and release to enter controlled VBAT deep sleep | [RTC procedure](docs/RTC.md) |
| Extended 32 MB partition layout | Uses its own CrowPanel partition table | **Waveshare only:** 25 MiB main application, 1 MiB recovery and a reserved 5.9375 MiB SPIFFS data partition | [Partitions](docs/WAVESHARE.md#partitions-and-recovery) |

P4 RTC/VBAT support is implemented; battery retention and wake-up still need
verification on the physical board. Arbitrary power loss during operation has
not been confirmed to preserve time. Follow the procedure in
[RTC.md](docs/RTC.md). The SPIFFS partition is reserved and is not yet mounted
automatically as additional file storage; fOS files continue to use the SD card.

**Display button on both boards:** A short press turns the display off or back
on; a long press requests light sleep when running tasks allow it. The button
uses **GPIO38** on CrowPanel and **GPIO52** on Waveshare. Sleep and wake-up
behavior depends on the board; see the [board guides](docs/INDEX.md).
P4 battery backup sleep through BOOT is a separate function.

## Arduino IDE: open the appropriate sketch

| Board | Main sketch | Recovery sketch | ESP32 core | LVGL |
| --- | --- | --- | --- | --- |
| CrowPanel, 800×480 | `fOS4.0/fOS4.0.ino` | `recovery/recovery.ino` | 3.3.12 | 8.3.11 |
| Waveshare, 1024×600 | `fOS_Waveshare_P4/fOS_Waveshare_P4.ino` | `recovery_p4/recovery_p4.ino` | 3.3.12 | 9.2.2, bundled with the sketch |

Sketch folders retain their existing names to preserve build paths. Both main
sketches already support the shared relative app layouts. Board settings,
connections and flashing instructions are documented in the
[CrowPanel](docs/CROWPANEL.md) and [Waveshare](docs/WAVESHARE.md) guides.

## Folder overview

| Folder | Contents |
| --- | --- |
| `apps` | Shared apps to copy into `/apps` on the SD card |
| `firmware` | **Earlier-version prebuilt images (not 4.1.1)**; CrowPanel images are in the `CrowPanel` subfolder |
| `fScript_Development_Environment/fosc` | Shared fosc/fosrun source code, examples and tests |
| `github_repository_files` | Prepared App Store and OTA files for both boards |
| `tools` | Index generator and required tests |
| `docs` | Central guides, manuals and current release notes |

## Further documentation

- [Installing and using apps](docs/APPS.md)
- [SD card and Wi-Fi/email configuration](docs/SD_AND_CONFIGURATION.md)
- [Firmware and recovery](docs/FIRMWARE_RECOVERY.md)
- [fosc and fosrun](docs/FOSC_FOSRUN.md)
- [Build and verification records](docs/VALIDATION.md)
- [All documentation](docs/INDEX.md)

Preserve your own `appdata` folders when updating apps. Device-bound Wi-Fi
and email credentials must be entered again when moving to a different board.


## CrowPanel AppStore HTTPS follow-up (2026-11-11)

See [`docs/CROWPANEL_APPSTORE_HTTPS_411.md`](docs/CROWPANEL_APPSTORE_HTTPS_411.md) for the PSRAM catalog-cache optimization, error diagnostics and retest procedure. These source changes require a new CrowPanel build; bundled old binaries were not rebuilt.
