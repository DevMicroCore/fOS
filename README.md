# fOS 3.2.0

fOS 3.2.0 is a touchscreen firmware for ESP32-S3 CrowPanel devices.

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
4. Open `fOS3.0.ino`.
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
* The password is stored as plain text on the SD card. Protect the card and use
  a dedicated app password if the mail provider supports one.
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
