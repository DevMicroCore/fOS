# fOS v3.3.0 Release Notes

## Highlights

fOS 3.3.0 is a security-focused release for ESP32-S3 CrowPanel devices. Wi-Fi
and email passwords stored on the SD card are now protected with authenticated,
device-bound encryption. Existing plain-text credentials are migrated
automatically.

## Credential Security

### Encryption

* Wi-Fi and email passwords are encrypted with AES-256-GCM.
* A random 256-bit device secret is generated once and stored in the ESP32's
  internal NVS.
* The final encryption key is derived from the NVS secret and the ESP32's eFuse
  MAC address.
* Each password receives a new random 96-bit nonce and a 128-bit authentication
  tag.
* Encrypted values use the versioned `enc:v1:...` format on the SD card.
* Authentication detects modified, damaged, or foreign encrypted values before
  they are used.

### Automatic migration

Plain-text passwords are migrated automatically in:

```text
/system/wifi/wlans.txt
/system/email/login.txt
```

* New Wi-Fi profiles are encrypted before they are appended to `wlans.txt`.
* Existing plain-text Wi-Fi and email passwords are encrypted when first read.
* The configuration is replaced only after the encrypted temporary file has
  been written successfully.
* A backup file permits recovery if a rename is interrupted by a reset or power
  loss.
* If encryption or secure replacement fails, fOS keeps the original file but
  does not use its plain-text credentials.
* Once migration is complete, the Wi-Fi credential file is no longer rewritten
  during every connection attempt.

### Compatibility and recovery

* Existing plain-text files remain compatible and require no manual conversion.
* Existing `password=...`, `password:...`, and legacy one-line email
  configurations are supported.
* Passwords containing `=` are handled correctly with either key/value syntax.
* Moving the SD card to another ESP32 does not make its encrypted credentials
  usable on that device.
* Erasing NVS or replacing the ESP32 makes the existing encrypted credentials
  unrecoverable. Enter the passwords again in that case.

The device binding primarily protects a removed or copied SD card. For
protection against physical readout of the ESP32's flash itself, enable ESP32
Flash Encryption/NVS Encryption in production.

## Fixes

* Corrected the Base64 output buffer size used during credential encryption.
  The encoder now receives the complete allocation, including space for the
  terminating null byte.
* Improved parsing of email passwords and legacy server values containing
  colons or equals signs.

## Email Application Compatibility

The Email application introduced in fOS 3.2.0 remains fully available.

### Receiving messages

* Supports IMAP reception over SSL/TLS, normally using port 993.
* Supports POP3 reception over SSL/TLS, normally using port 995.
* Up to 10 new messages are fetched during one synchronization run.
* Up to 30 cached messages are displayed in the Inbox.
* Empty legacy cache entries are fetched again automatically.

### MIME and message content

* Supports MIME multipart messages.
* Plain-text content is preferred when available.
* HTML content is converted to readable text as a fallback.
* Decodes quoted-printable and Base64 content.
* Decodes common encoded sender and subject headers.
* Long message bodies can be scrolled vertically by touch.
* The message view returns to the top when another message is opened.

### Sending messages

* Supports SMTP sending over direct SSL/TLS, normally using port 465.
* Provides configurable sender name, From address, and Reply-To address.
* Encodes outgoing UTF-8 message bodies as quoted-printable.
* Validates recipient and sender addresses.
* Protects against likely technical sender addresses that can fail
  SPF/DMARC policy checks.
* STARTTLS on port 587 is not supported in this release.

### Inbox and Outgoing interface

* Provides separate **Inbox** and **Outgoing** tabs.
* Provides an individual roller for each list.
* The newest messages are displayed first; older messages follow below.
* Sent messages are stored in `/email/outgoing` and shown immediately after
  sending.
* Received messages are cached in `/email/inbox`.
* The selected message is opened from the currently active tab.

### Theme integration

* The Email application follows the global Blue, Orange, or Green system theme.
* Theme colors are applied to buttons, selected roller entries, focused text
  fields, and the message scrollbar.
* The active Inbox or Outgoing tab uses the theme color for its text and lower
  border; the tab background remains neutral.
* No separate theme selector is included in the Email application.

## Configuration

Email account data is read from:

```text
/system/email/login.txt
```

Example IMAP configuration:

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

For POP3 reception, set `protocol=pop3` and configure `pop3_server`,
`pop3_port`, and `pop3_ssl` instead of the IMAP values.

Wi-Fi and email passwords are stored as device-bound `enc:v1:...` values using
AES-256-GCM. Existing plain-text values are migrated automatically when fOS
first reads them. The encryption key is derived from a random secret held in
the ESP32's internal NVS and the chip's eFuse MAC, so another ESP32 cannot use
the encrypted credentials simply by inserting the SD card.

Erasing NVS or replacing the ESP32 makes the existing encrypted credentials
unrecoverable; enter the passwords again in that case. A dedicated email app
password remains recommended when supported by the provider.

The device binding protects credentials on a removed or copied SD card. For
protection against physical readout of the ESP32's internal flash itself,
enable ESP32 Flash Encryption/NVS Encryption in production.

## Storage

The following directories are created automatically when needed:

```text
/apps/email
/email/inbox
/email/outgoing
/system/email
```

## Compatibility

* The Weather location search introduced in fOS 3.1.0 remains available.
* Existing fOS system themes and SD applications remain supported.
* Existing OTA, recovery, storage, audio, clock, and display functions remain
  unchanged.

## Known Limits

* SMTP requires direct SSL/TLS; STARTTLS on port 587 is not available.
* The displayed body is limited for embedded-memory safety; very large or
  attachment-heavy messages can be truncated.
* Attachments are not downloaded or opened.
* The Email application requires Wi-Fi, a valid system time, and a correctly
  configured mail provider account.
