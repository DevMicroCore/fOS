# fOS v2.5.0 Release Notes

Release date: 2026-07-03

## Highlights

fOS 2.5.0 improves Wi-Fi management with persistent Wi-Fi enable/disable control and a simplified network selection workflow.

## Added

* Wi-Fi enable/disable switch in Settings.
* Wi-Fi state is automatically saved.
* The saved Wi-Fi state is restored during system startup.
* Available Wi-Fi networks can now be selected from a dropdown list after scanning.

## Changed

* Manual SSID entry is no longer required when connecting to nearby Wi-Fi networks.
* Wi-Fi configuration is now faster and more user-friendly.
* Network settings are automatically restored after reboot.

## Fixed

* Improved reliability of Wi-Fi initialization during boot.
* Reduced configuration errors caused by manually entering SSIDs.

## Compatibility Notes

* Existing SD app format remains fully supported:

  * `type=ui|text|button|calculator|radio|clock|weather`
* OTA + Recovery architecture remains unchanged.
* Display brightness persistence remains stored in:

  * `/system/display/brightness.txt`
* Recommended SD card format remains FAT32 (MBR preferred).

## Known Limitations

* Internet-dependent features (OTA, weather and web radio) require an active Wi-Fi connection.
* Network scanning only displays Wi-Fi networks currently within range.
