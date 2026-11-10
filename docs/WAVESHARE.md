> All source and firmware paths below are relative to the package root `fOS4.1.0`.

# fOS 4.1.0 — Waveshare ESP32-P4 7-inch port

Start with **`fOS_Waveshare_P4/fOS_Waveshare_P4.ino`** in Arduino IDE.
This package ports the supplied CrowPanel project and incorporates the supplied
`ui(1).zip` UI (Connections settings with USB storage) at **1024 x 600 / LVGL 9.2.2**.

## Build and first start

Use a new extraction directory. Keep all bundled folders: the sketch includes
LVGL 9.2.2 and ESP32-audioI2S 3.4.7 source, so neither library needs a separate
Library Manager installation. LovyanGFX is no longer used.

| Arduino IDE option | Value |
| --- | --- |
| Board package | esp32 by Espressif Systems **3.3.12** |
| Board | **ESP32P4 Dev Module** |
| Chip Variant | **Before v3.00** for your ROM `esp32p4-eco2-20240710` |
| PSRAM | **Enabled** |
| Flash Size | **32MB (256Mb)** |
| Flash Mode | **QIO** |
| Flash Frequency | **80MHz** |
| Partition Scheme | **Custom**, using the included `partitions.csv` |
| USB Mode | **USB-OTG (TinyUSB)** |
| MSC On Boot | **Disabled** (fOS implements the SD-card MSC) |
| Upload Mode | **UART0 / Hardware CDC** |
| USB CDC On Boot | **Disabled** when using USB TO UART |

Connect the board's **USB TO UART** Type-C port, select the port and upload the
main sketch. Serial Monitor: **115200 baud**. The selected chip variant must
match the silicon; firmware in `firmware/` was built for **Before v3.00**.

Insert the existing FAT32 microSD card. The folder layout, fAPP permissions,
`/system` protection and `/appdata` preservation remain in place. Example apps
remain in `fOS_Waveshare_P4/example app/apps`.

**Wi-Fi and email passwords must be entered again on the P4.** The original
project encrypts credentials with a secret bound to the old MCU's eFuse MAC/NVS.
Copying its SD card does not copy that device key. No plaintext credential
migration or security bypass was added.

## Display orientation

Output is rotated **180 degrees**, including recovery. SquareLine still uses
1024 x 600 logical coordinates; its generated screen files are unchanged.
The touch transform is inverted on both axes to match the rotated output.

## Onboard RTC and offline time

The internal P4 RTC now has explicit **VDDA/VBAT AUTO selection**, with the
coin-cell charger disabled. `src/p4_vbat/FosPmuSleep.c` is a project-local copy of
the ESP-IDF 5.5.5 PMU sleep implementation. It reapplies the documented switchover
immediately before the PMU sleep-enable instruction, after SDK sleep defaults
have overwritten the power banks. The installed Arduino core is unchanged.
The port deliberately requires Arduino-ESP32 3.3.12 / ESP-IDF 5.5.5; revalidate
it before changing either version. Provenance/license are beside the source.

SNTP captures UTC, raw RTC ticks, clock calibration, source and CRC in retained
memory and NVS. Startup restores the calendar **before Wi-Fi**, including elapsed
time from a live counter. No frozen last-known flash date is presented as current
time. Power-on resets, reset counters and damaged records reject stale mappings.
Diagnostics now report both anchors, chip revision and the latched VBAT mode;
`anchor captured` does not imply that battery retention has been hardware tested.

For the documented battery scenario, synchronize once and disable Wi-Fi.
**Hold the board's BOOT button (GPIO0) for 3 seconds and release** to save a fresh
checkpoint and enter VBAT deep sleep. Unload/close apps and audio and unmount SD
before sleeping. USB ownership, Wi-Fi connection and update workers must be idle;
otherwise sleep is cancelled. Remove main power only after sleep entry. Restore
main power before pressing BOOT again. GPIO52 keeps its existing display/light-
sleep behavior: it cannot wake this chip from deep sleep. Follow the detailed
German test in **RTC.md**.

The full main build and host tests passed; **physical VBAT/power-cut retention
has not been verified here**. Espressif's documented scenario uses prepared sleep.
Its open issue #18988 reports retention failure on early v1.3 silicon despite
switchover configuration. Arbitrary active power cuts and hardware RESET are not
guaranteed by this package. If retention fails, send boot/sleep/VBAT diagnostics;
a saved NTP timestamp cannot reconstruct an unmeasured power-off interval.

## USB storage of the microSD card

In **Settings > Connections**, change **USBStorageSwitch** and press the existing
**Save connections** button (`SaveWifiConnection`). It saves the USB setting in
NVS even if Wi-Fi is disabled, no network is selected or no SD card is present.
Default is off. The setting is restored at startup.

Arduino options: **USB-OTG (TinyUSB)**, **MSC On Boot disabled**, CDC On Boot
disabled, upload through **USB TO UART**. Core 3.3.12 selects the P4's **native
USB OTG High-Speed port** (TinyUSB RHPORT 1): on LCD-7B this is the **Type-A
USB OTG 2.0 HS connector**, using a suitable USB data connection to the computer.
The Type-C USB TO UART socket is only the programming/debug bridge; neither it
nor the Type-C USB1.1 FS/Serial-JTAG connection provides this HS mass-storage
interface. The port assignment follows the unmodified core's USB implementation.

Insert the FAT32 SD card before enabling USB storage. Once the computer mounts
the USB device, fOS waits for existing Wi-Fi/App Store/OTA workers to finish,
closes the active fAPP and audio, and unmounts its local FAT filesystem. The
computer receives the **whole physical SD card**, including the partition table.
All local SD-dependent work and application timers are paused under a modal
USB screen. Incoming partial and multi-sector USB requests use bounded sector
read/modify/write; a mutex prevents remounting while a transfer is in flight.
The computer has access to the complete SD card, including `/system`; fAPP
permissions still apply only to fAPP access inside fOS.

**Eject the drive on the computer first.** An eject or USB disconnect returns
the card to fOS with a fresh filesystem mount and rebuilt launcher. The USB
setting remains enabled after eject, ready for another connection. The overlay's
**Disable USB storage** button saves off and detaches the USB interface; you can
also disable it in Connections after ejecting. Sleep input is ignored during
USB card ownership. No formatting of the card occurs. If the SD is missing or
remount fails, local storage remains unavailable and diagnostics are printed.

## Sleep button on GPIO52

Wire a normally-open momentary button between **GPIO52 and GND**. Internal
pull-up is enabled; no external voltage is applied to the button input.
The LCD-7B schematic routes GPIO52 to the expansion header and does not assign
it to an onboard peripheral. This firmware does not initialize Ethernet.

The existing button behavior is retained:

- Short press: backlight off and touch suspended; busy audio/update work continues.
- Press again: display on, saved brightness restored.
- Long press: requests light sleep once no audio/update operation is busy; the
  button is configured as the wake source. This remains a hardware test item.
- No deep-sleep mode was added. In display-off mode the MIPI scan engine stays
  active; this is not a minimum-power shutdown of the complete board.

## Hardware mapping

| Function | GPIO / interface |
| --- | --- |
| Display | EK79007, 1024 x 600, two-lane MIPI-DSI |
| LCD reset / backlight | 33 / 32, active low |
| Touch and codec control | Shared I2C: SDA 7, SCL 8 |
| GT911 | Automatic 0x5D / 0x14 detection |
| SDMMC | CLK 43, CMD 44, D0 39, D1 40, D2 41, D3 42 |
| SD power | LDO4 and active-low GPIO45 via ESP32 core 3.3.12 |
| ESP32-C6 Wi-Fi SDIO | CLK 18, CMD 19, D0..D3 14..17, reset 54 |
| ES8311 I2S | BCLK 12, LRCK 10, DOUT 9, MCLK 13 |
| Speaker amplifier | GPIO53 |
| Sleep button | GPIO52 to GND |

The codec uses the same I2C master bus as touch. Audio is resampled to a fixed
48 kHz / 32-bit I2S format, so the codec's clock dividers remain correct when a
file/stream has another sample rate. Mono output is used for the single speaker.
If codec setup fails, audio start requests fail while the rest of fOS remains usable.

Wi-Fi uses Arduino's ESP-Hosted support for the onboard C6. It requires a
compatible ESP-Hosted slave firmware on that C6. This package changes the P4
application; it does not silently reflash the C6. If the main UI/SD work but
ESP-Hosted fails to initialize, retain the Serial diagnostics for that separate
C6 compatibility check.

## UI updates

The complete new export lives in **`fOS_Waveshare_P4/ui/`**, next to the `.ino`.
All supplied export files are byte-identical **except `ui_events.c`**, which
connects the exported events to the real fOS functions.

1. Replace `ui/` with your new SquareLine export (LVGL 9.2.2, RGB565, 1024 x 600).
2. Merge your `ui_events.c` function bodies into the new file.
3. From the sketch directory run `python3 tools/update_ui_build.py` when exported
   source filenames or referenced fonts change. Running it after every export is fine.
4. Recompile in Arduino IDE.

`ui.h`, `lvgl/lvgl.h`, and `src/squareline_*` are separate compatibility/build
adapters. The duplicate exported `uic_Update` custom alias is renamed only
while compiling the StorageManager source. No screen .c/.h patch is required.

The new export does not include the existing App Store and Email designs.
Their retained, LVGL-9-adapted sources are under `native_ui/`, outside `ui/`.
They are instantiated through the existing fOS application functions. Native
full-screen containers use the P4 dimensions; existing 800 x 480 fAPP layouts
retain their own coordinates and can be redesigned independently for 1024 x 600.
Canvas allocation and keyboard bounds support the new resolution.

## Partitions and recovery

The P4 main and P4 recovery sketches use this identical 32MB-compatible partition map:

| Partition | Offset | Size |
| --- | --- | --- |
| nvs | 0x9000 | 0x5000 |
| otadata | 0xE000 | 0x2000 |
| app0: fOS | 0x10000 | 0x1900000 (25 MiB) |
| app1: P4 recovery | 0x1910000 | 0x100000 (1 MiB) |
| SPIFFS | 0x1A10000 | 0x5F0000 (5.9375 MiB) |

Sizes use binary MiB (1 MiB = 1,048,576 bytes). The first 64 KiB contains the
bootloader, partition table, NVS and OTA metadata. The final SPIFFS partition
ends exactly at 0x2000000 (32 MiB). Usable file capacity is lower because of
filesystem overhead. It is reserved for future internal storage; existing fOS
file handling and fAPP paths still use the SD card. This change does not mount,
format or redirect data to SPIFFS automatically.

**Migration from the previous 12/4 MiB layout:** upload the main sketch through
Arduino IDE with Custom partitions so the new table is actually written. A
normal OTA application update does not replace the partition table. The recovery
has moved from 0xC10000 to **0x1910000**: reinstall it at the new offset before
using it. An old OTA boot-selection record can point to app1; a normal Arduino
main upload resets OTA selection using boot_app0.bin. Do not update only the
partition table while still booting the old recovery.

Normal Arduino upload of the **main sketch** programs app0. The recovery sketch
can also be compiled in Arduino IDE, but its **application-only `.ino.bin` goes
at app1 offset 0x1910000**. A normal Upload of the recovery sketch would instead
replace app0. For initial optional recovery installation, an esptool command is:

```sh
python3 -m esptool --chip esp32p4 --port YOUR_PORT write-flash 0x1910000 firmware/recovery_p4.ino.bin
```

This command uses the root package directory as the working directory and
requires esptool installed in that Python environment. The prebuilt recovery is
for Before v3.00 hardware and the exact partition map above.

The existing SD-staged recovery workflow is retained. OTA uses the separate
repository paths:

- `Waveshare_7"_esp32p4/ota/`
- `Waveshare_7"_esp32p4/update/`

`github_repository_files/` contains a staging copy with matching P4 application
binaries and indexes. Publishing these files to your GitHub repository is still
required for online updates; nothing was published by this porting task.
If the new paths are not available, no S3 image is offered as a fallback.

Both main OTA and recovery reject firmware with a different chip ID or an
unsupported silicon revision **before erasing/writing the destination**. The
original segment, checksum/hash and flash read-back verification remain.
Do not use old CrowPanel S3 binaries or its partition map on this port.

## Verification and next hardware check

`BUILD_CHECK.txt` records actual Arduino builds and host regression checks.
The port builds for your Before v3.00 silicon. GUI creation/navigation, canvas
operations and keyboard lifecycle were exercised with real LVGL 9.2.2 on the host.
The compiler/VM, storage policy and App Store data preservation tests were also run.

No physical Waveshare board was connected. On-device display/touch orientation,
SD, C6 Wi-Fi, audible playback, light-sleep resume and the complete recovery
flash/reboot cycle still require checking on your board. A sensible first test
is boot with SD, then settings/touch, Wi-Fi, radio playback and the GPIO52 button.

## Source references

- [ESP32-P4 VBAT limitations](https://docs.espressif.com/projects/esp-iot-solution/en/latest/low_power_solution/esp32p4_vbat.html).
- [Core 3.3.12 native USB HS implementation](https://github.com/espressif/arduino-esp32/blob/3.3.12/cores/esp32/esp32-hal-tinyusb.c).
- [Arduino SD-to-USB example](https://github.com/espressif/arduino-esp32/blob/3.3.12/libraries/SD_MMC/examples/SD2USBMSC/SD2USBMSC.ino).
- [Waveshare LCD-7B schematic](https://files.waveshare.com/wiki/ESP32-P4-WIFI6-Touch-LCD-7B/ESP32-P4-WIFI6-Touch-LCD-7B.pdf).
- [Waveshare Arduino profile](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B/tree/main/examples/arduino), commit `9fcd49a8c927a00f5583c22d568b3448addbe51c`.
- [ESP32-audioI2S 3.4.7](https://github.com/schreibfaul1/ESP32-audioI2S/tree/3.4.7), commit `59bf36b214580ad2d1157748cd0268c9376d6640`, license bundled.
- [LVGL 9.2.2](https://github.com/lvgl/lvgl/tree/v9.2.2), MIT license bundled.

The original CrowPanel README is retained as a historical reference under
`fOS_Waveshare_P4/docs/CROWPANEL_REFERENCE.md`; its S3 hardware/LVGL-8 settings
do not apply to this package.
