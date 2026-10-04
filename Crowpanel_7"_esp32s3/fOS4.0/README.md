# fOS 4.0.0

fOS 4.0.0 is a touchscreen firmware for ESP32-S3 CrowPanel devices.

---

## fAPP and fScript Development Status

fOS 4.0.0 introduces the foundation for SD-card applications with compiled
program logic. The current development milestone contains:

* a fixed-size UI object registry without dynamic allocation
* optional `id=` fields in existing `layout.ui` files
* deterministic numeric object IDs for fScript bytecode
* an `FAppLoader` foundation that detects `main.fapp` without loading the
  complete file into RAM
* a centralized fOS version definition
* full compatibility with existing `app.cfg` applications
* the first PC-side `fosc` tool with a complete fScript lexer
* a recursive-descent parser with typed abstract syntax tree (AST)
* precedence-aware expressions, functions, events and conditions
* a bytecode generator and versioned `.fapp` 1.0 writer
* numeric globals, locals, functions, event bindings and UI references
* CRC-32-protected constant, function, event and code sections
* an fAPP reader and human-readable disassembler
* a semantic analyzer with inferred types and function/UI compatibility checks
* conservative compile-time constant folding
* a full bytecode verifier for indices, jumps, stack depth and control flow
* ESP32-side header, compatibility, bounds and streaming CRC validation
* stable lexer diagnostics with filename, line and column
* stable parser diagnostics and recovery after syntax errors
* automated lexer, parser, semantic, compiler, optimizer, verifier, format and
  embedded-loader tests
* a fixed-resource cooperative ESP32 virtual machine for bytecode 1.5
* bounded LVGL event delivery and a VM-independent UI adapter
* `app.json`, SemVer compatibility checks and capability permissions
* `app.start`, `app.close` and state-preserving `app.theme_changed` handlers
* a compiled `fscript_counter` example and runtime/manifest end-to-end tests
* permission-guarded HTTP/HTTPS GET, JSON-path extraction and URL encoding
* a one-minute `app.timer` lifecycle event for periodic script work
* keyboard `ready`/`cancel` events and `date_weekday()` for native weather-app parity
* on-demand PSRAM-backed VM values so inactive fAPP support no longer consumes
  the internal heap required by OTA TLS/SHA
* app-scoped `file_read()`/`file_write()`/`file_list()`, `audio_play()`, `wifi_status()`,
  `system.restart()`, `timer.start()` and safe `Serial.printf()` APIs
* general fScript helpers `text()`, `number_parse()`, `round()`,
  `string_length()`, `string_slice()`, `string_last_index()` and `math_eval()`
* `fosrun`, a PC-side runner for compiled `.fapp` logic tests with simulated
  UI state, native fOS APIs and a local browser UI test mode

Development Steps 2 through 13 and fOS API Step 19 are now integrated. `fosc`
can check, optimize, compile, verify and disassemble fScript applications.
`fosrun` can execute compiled apps on a PC for fast event/UI logic checks and
can load the `.ui` in a local browser surface for clickable tests. fOS validates
and executes the same bytecode cooperatively on the ESP32.
Runtime faults terminate only the affected application and leave the operating
system responsive.

### Build and run the fScript frontend

```bash
cd fScript_Development_Environment/fosc
make
make test
./build/fosc lex examples/lexer_demo.fscript
./build/fosc parse examples/lexer_demo.fscript
./build/fosc check examples/lexer_demo.fscript --ui examples/lexer_demo.ui
./build/fosc build examples/lexer_demo.fscript --ui examples/lexer_demo.ui -o build/main.fapp
./build/fosc verify build/main.fapp
./build/fosc disasm build/main.fapp
./build/fosc build examples/fosrun_demo.fscript --ui examples/fosrun_demo.ui -o build/fosrun_demo.fapp
./build/fosrun build/fosrun_demo.fapp --ui examples/fosrun_demo.ui --event btn_add.click
./build/fosrun build/fosrun_demo.fapp --ui examples/fosrun_demo.ui --serve --open
```

The compiler sources also include a CMake project for Windows, Linux and macOS.

### Complete documentation

* [Deutsches fScript-, fosc- und VM-Handbuch](docs/FSCRIPT_FOSC_VM_HANDBUCH_DE.md)
* [English fScript, fosc and VM Manual](docs/FSCRIPT_FOSC_VM_MANUAL_EN.md)
* [fosrun PC Runner](docs/FOSRUN_STEP13.md)

Both manuals cover the complete language syntax, every compiler command,
layout and manifest formats, all UI/native APIs, bytecode, VM lifecycle,
resource limits, diagnostics, examples and app installation.

### Install the complete example

Copy `example app/apps/fscript_counter` to `/apps/fscript_counter` on the SD
card. The shipped `main.fapp` is already compiled and verified. Its source,
layout, manifest and legacy fallback configuration are included beside it.

---

## What's New in 3.3.0

### Device-Bound Credential Encryption

fOS now protects Wi-Fi and email passwords stored on the SD card.

* Passwords are encrypted with authenticated AES-256-GCM encryption.
* A random 256-bit device secret is generated once and stored in the ESP32's
  internal NVS.
* The encryption key is additionally bound to the ESP32's eFuse MAC address.
* Every password uses a new random 96-bit nonce and a 128-bit authentication
  tag.
* Encrypted values are stored in the versioned `enc:v1:...` format.
* Existing plain-text Wi-Fi and email passwords are migrated automatically when
  first read.
* New Wi-Fi profiles are encrypted before being written to the SD card.
* Credentials copied to another ESP32 cannot be decrypted there.
* Modified, damaged, or foreign encrypted values are rejected.
* Credential loading fails closed if a secure migration cannot be completed.
* Temporary and backup files protect the original configuration during an
  interrupted migration.
* Wi-Fi files are rewritten only when a plain-text password actually requires
  migration, reducing unnecessary SD-card writes.

The implementation uses the cryptographic and NVS components already included
with the ESP32 platform to keep additional flash and RAM usage low.

---

## What's New in 3.2.0

### Email Application

fOS now includes an Email application for receiving, reading, composing, and
sending messages directly on the CrowPanel.

* Receive messages using IMAP or POP3 over SSL/TLS.
* Send messages using SMTP over direct SSL/TLS on port 465.
* Separate **Inbox** and **Outgoing** tabs with their own message rollers.
* The newest messages are displayed first in both lists.
* Up to 30 cached messages are shown per list.
* Long message bodies can be scrolled vertically by touch.
* Sent messages are stored in `/email/outgoing` and appear immediately in the
  Outgoing list.
* Incoming messages are cached in `/email/inbox`.
* MIME multipart messages, `text/plain`, HTML fallback, quoted-printable, and
  Base64 content are decoded for display.
* Buttons, selected roller entries, active tab text and underline, text field
  focus borders, and the message scrollbar follow the selected system theme.
* Sender and Reply-To headers can be configured separately for correct mail
  delivery and DMARC alignment.

Email account settings are read from `/system/email/login.txt`. See
[Email Setup](#email-setup) for the required format.

---

## What's New in 3.1.0

### Weather Application

The Weather application has been updated with a new **Search** button.

* Press **Search** to open the location search menu.
* Enter a location in the search field.
* Press the **✅** button to search for the location.
* All matching results are then displayed in the dropdown menu.
* Select the desired location from the dropdown menu.

---

## What's New in 3.0.0-beta.1

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

  * `esp32 by Espressif Systems` **3.3.12**

Required libraries:

* `lvgl` **8.3.11**
* `LovyanGFX` **1.2.29**
* `ESP32-audioI2S` **3.4.7** (provides `Audio.h`)

The existing LVGL 8 configuration must enable the Montserrat 16, 20, 24 and
40 fonts used by the generated UI. LVGL 9 is not supported by this migration.

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

   * lvgl 8.3.11
   * LovyanGFX 1.2.29
   * ESP32-audioI2S 3.4.7
4. Open `fOS4.0.ino`.
5. Select your ESP32-S3 board.
6. Configure:

   * custom partition scheme (`app0/app1`)
   * `Flash Size = 4MB` (verified; 16MB remains compatible)
   * `PSRAM = OPI PSRAM`
7. Verify the correct panel define in `LGFX_CrowPanel.h`.
8. Compile and upload.
9. Prepare the SD card.
10. Insert the SD card.
11. Reboot the device.

The complete Core 2.0.15 to 3.3.12 analysis, build comparison and remaining
hardware-test checklist are documented in `CORE_3_3_12_MIGRATION_REPORT.md`.

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
/email/inbox
/email/outgoing
/text
/music/files
/music/webradio
/system
/system/email
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
/system/email/login.txt
/system/update/update.bin
/system/update/recovery.bin
```

---

# Email Setup

The Email application creates `/apps/email`, `/email/inbox`,
`/email/outgoing`, and `/system/email` automatically when needed.

Create the following account file on the SD card:

```text
/system/email/login.txt
```

Recommended IMAP configuration:

```ini
protocol=imap
email=your-address@example.com
user=your-login-name
password=your-password
imap_server=imap.example.com
imap_port=993
imap_ssl=true
imap_folder=INBOX
smtp_server=smtp.example.com
smtp_port=465
smtp_ssl=true
from_email=your-address@example.com
reply_to=your-address@example.com
sender_name=fOS
```

For POP3 reception, use:

```ini
protocol=pop3
pop3_server=pop3.example.com
pop3_port=995
pop3_ssl=true
```

The remaining account and SMTP values are the same as in the IMAP example.

Important notes:

* SMTP sending in this release requires direct SSL/TLS, normally on port 465.
* STARTTLS on port 587 is not supported by this build.
* `from_email` must be a real address authorized for the configured SMTP
  account or domain. A technical mail-server hostname can be rejected by DMARC
  checks.
* On the first load, fOS automatically replaces a plain-text `password` value
  with an `enc:v1:...` value encrypted for this ESP32. Newly saved Wi-Fi
  passwords are encrypted immediately as well.
* Encryption uses AES-256-GCM. The device key is derived from a random secret in
  the ESP32's internal NVS and the chip's eFuse MAC, so moving the SD card to a
  different ESP32 does not expose or decrypt the passwords.
* Do not edit an `enc:v1:...` value. To change a password, enter the new value in
  plain text once; fOS encrypts it automatically on the next load.
* Erasing the ESP32's NVS or replacing the board destroys access to existing
  encrypted SD credentials. In that case, enter the passwords again.
* For protection against a physical readout of the ESP32's internal flash,
  enable ESP32 Flash Encryption/NVS Encryption for a production device. The
  device binding described here primarily protects a removed or copied SD card.
* A dedicated email app password is still recommended when supported by the
  mail provider.
* Wi-Fi and valid system time are required for mail transfer.

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
type=ui|text|button|calculator|radio|clock|weather|email
scrollable=true|false
```

Additional keys:

### UI

```
layout=layout.ui
fapp=main.fapp
```

`fapp` is optional. Existing UI apps without this entry continue to work. If
omitted, the future executable defaults to `main.fapp`.

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
type=label;id=lbl_title;x=40;y=40;w=720;h=40;text=Hello
type=button;id=btn_start;x=40;y=100;w=240;h=70;text=Start;bg=0x2095F6;fg=0xFFFFFF
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
* roller
* dropdown
* keyboard

Additional optional fields include `font=20|24|40`, `align=left|center|right`,
`hidden=true`, `radius=`, `border=`, `placeholder=`, `parent=` and `target=`.
`parent` can place an element inside a previously declared panel; `target`
connects a keyboard to a previously declared textarea.
When the target textarea is touched or focused, fOS makes the connected
keyboard visible and moves it to the foreground. Scripts can still control the
keyboard explicitly with `keyboard_id.hidden`.

The optional `id` field makes an element addressable by fScript code.
At runtime, textual IDs are not retained. Named objects receive compact numeric
IDs in declaration order. Elements without `id` remain compatible UI-only
elements and consume no registry entry.

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
* Email

  * IMAP or POP3 inbox reception
  * SMTP sending over direct SSL/TLS
  * separate Inbox and Outgoing lists
  * local SD cache for received and sent messages
  * MIME body decoding
  * touch-scrollable message view
  * automatic system-theme integration

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
* fscript_calculator
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

## Email account not loaded

* Verify that `/system/email/login.txt` exists.
* Check that `email`, `user`, `password`, the selected incoming server, and
  `smtp_server` are present.
* Use `protocol=imap` or `protocol=pop3`.

## Email sending fails

* Verify Wi-Fi connectivity and the system time.
* Use the provider's direct SSL/TLS SMTP endpoint, normally port 465.
* Set `smtp_ssl=true`.
* Ensure `from_email` is authorized for the SMTP account and domain.
* Check the Serial Monitor for `[EMAIL]` diagnostics.

## Email body is empty

Open the Email application with Wi-Fi enabled. Cached entries without a usable
body are fetched again automatically. Very large messages can exceed the
embedded mail buffer; plain-text messages and compact HTML messages work best.

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

# License & Open Source
This project is licensed under the **GNU GPLv3**.

**What does this mean for you?**
- 🏠 **Personal & In-House:** Use, modify, and customize fOS for yourself or your company however you like. You don’t have to disclose anything.
- 🚀 **Sharing & Selling:** You may modify fOS and use it in commercial projects.
- 🔄 **The only condition:** If you distribute or sell a modified version of fOS to customers, your changes to the fOS code must also be open source (under GPLv3).

---

# Version History

## v4.0.0

* Added the fixed-capacity fAPP UI object registry.
* Added optional UI element IDs and deterministic numeric references.
* Added the bounded `FAppLoader` foundation and optional `fapp=` app setting.
* Added the centralized fOS version definition.
* Added `fosc` 0.2.0 with the fScript 1.0.0 lexer, parser and typed AST.
* Added token locations, stable frontend error codes and automated tests.
* Added an AST printer for inspecting parsed fScript programs.
* Preserved compatibility with existing SD-card applications.

## v3.3.0

* Added device-bound AES-256-GCM encryption for Wi-Fi and email passwords.
* Added automatic migration of existing plain-text SD credentials.
* Added authenticated encryption, per-write random nonces, and recovery files
  for interrupted credential migrations.
* Added a random 256-bit device secret stored in NVS and bound it to the ESP32's
  eFuse MAC address.
* Added fail-closed handling for unsuccessful migrations and encrypted values
  that cannot be authenticated.
* Avoided unnecessary Wi-Fi credential file rewrites after migration.
* Fixed the Base64 output buffer size used during password encryption.

## v3.2.0

* Added the Email application.
* Added IMAP and POP3 reception over SSL/TLS.
* Added SMTP sending over direct SSL/TLS on port 465.
* Added MIME multipart, plain-text, HTML fallback, quoted-printable, and Base64
  decoding.
* Added separate Inbox and Outgoing rollers.
* Added local storage for sent messages.
* Sorted Inbox and Outgoing with the newest message first.
* Added vertically scrollable message bodies.
* Integrated the Email UI with the global system theme.
* Added configurable sender, Reply-To, and DMARC safety checks.

## v3.1.0

* Updated Weather application.
* Added **Search** button for location search.
* Added location search menu with text input.
* Search results are displayed in a dropdown menu after pressing **✅**.



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
