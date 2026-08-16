# fOS v3.2.0 Release Notes

## Highlights

fOS 3.2.0 introduces a complete Email application for ESP32-S3 CrowPanel
devices. Messages can be received, read, composed, sent, and stored on the SD
card through a touch-optimized interface that follows the global fOS theme.

## Email Application

### Receiving messages

* Added IMAP reception over SSL/TLS, normally using port 993.
* Added POP3 reception over SSL/TLS, normally using port 995.
* Up to 10 new messages are fetched during one synchronization run.
* Up to 30 cached messages are displayed in the Inbox.
* Empty legacy cache entries are fetched again automatically.

### MIME and message content

* Added support for MIME multipart messages.
* Plain-text content is preferred when available.
* HTML content is converted to readable text as a fallback.
* Added quoted-printable and Base64 decoding.
* Added MIME header decoding for common encoded sender and subject fields.
* Long message bodies can be scrolled vertically by touch.
* The message view returns to the top when another message is opened.

### Sending messages

* Added SMTP sending over direct SSL/TLS, normally using port 465.
* Added configurable sender name, From address, and Reply-To address.
* Added quoted-printable encoding for outgoing UTF-8 message bodies.
* Added validation for recipient and sender addresses.
* Added protection against likely technical sender addresses that can fail
  SPF/DMARC policy checks.
* STARTTLS on port 587 is not supported in this release.

### Inbox and Outgoing interface

* Added separate **Inbox** and **Outgoing** tabs.
* Added an individual roller for each list.
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

The account password is stored as plain text on the SD card. A dedicated app
password is recommended when supported by the mail provider.

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
