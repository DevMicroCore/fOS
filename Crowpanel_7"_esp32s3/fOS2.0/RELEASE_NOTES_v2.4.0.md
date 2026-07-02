# fOS v2.4.0 Release Notes

Release date: 2026-07-02

## Highlights

fOS 2.4.0 improves usability and safety with two frequently requested features:
- confirmation dialog before deleting files
- integrated timer inside the Clock application

## Added

- Clock application:
  - integrated countdown timer
  - easy start/stop/reset controls
  - runs alongside the existing clock functionality

- File Manager:
  - confirmation dialog before deleting a file
  - helps prevent accidental file removal

## Changed

- File deletion is now a two-step process requiring explicit user confirmation.
- Clock app now combines time display and timer functionality in one application.

## Fixed

- Reduced risk of accidental data loss caused by unintended file deletion.
- Minor UI improvements related to file management.

## Compatibility Notes

- Existing SD app format remains fully supported:
  - `type=ui|text|button|calculator|radio|clock|weather`
- OTA + Recovery architecture from 2.2.0 remains unchanged.
- Display brightness settings continue to be stored in:
  - `/system/display/brightness.txt`
- Recommended SD card format remains FAT32 (MBR preferred).

## Known Limitations

- `GPIO38` can still be electrically sensitive on some ESP32-S3/CrowPanel boards.
- Internet-dependent features (OTA, weather, web radio) depend on network availability.