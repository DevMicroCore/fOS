# SD-Karte und Konfiguration

Pfade beziehen sich auf die SD-Karte oder den Projekt-Hauptordner.
Für die Board-Auswahl gelten [CROWPANEL.md](CROWPANEL.md) und [WAVESHARE.md](WAVESHARE.md).

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

OTA and the official App Store use compact index files instead of repeatedly
listing GitHub directories through the REST API. After changing apps or OTA
binaries in the repository, run
`python3 tools/generate_repository_indexes.py /path/to/fOS-repository` and
commit the generated index files. Built-in indexes
provide an offline fallback for the release shipped with this source archive.

Check the Serial Monitor for `[OTA]` messages.

## GPIO38 button not working

* Connect the button between `GPIO38` and `GND`.
* Open the Serial Monitor.
* Verify GPIO state changes.

## Device restarts after Light Sleep

Some CrowPanel revisions reset when waking from Light Sleep.

Short press remains the recommended and most reliable display-off mode.

---

