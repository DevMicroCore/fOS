# fOS 4.1.1 — Release notes

Date: 2026-11-13

## AppStore: no more blocking downloads

Both the **CrowPanel ESP32-S3** and **Waveshare ESP32-P4** previously ran
AppStore HTTPS transfers and SD-card installations directly in the LVGL
Download-button callback. Prolonged blocking could freeze LVGL and trigger
watchdog resets. In v4.1.1 the transfers run on a dedicated FreeRTOS worker
and the LVGL timer handles progress and completion on the UI thread.

- The download button no longer performs blocking HTTPS or SD-write operations.
- A status timer displays download progress and the installation phase.
- A second install, app deletion and launcher assignment editing are disabled
  until the current transfer has finished.
- Catalog refreshes are deferred during installs so they do not compete for
  TLS heap or access the same SD directory tree.
- Waveshare's SD/network busy guard now also covers AppStore downloads.
- Reopening the AppStore while a transfer is active does not start another
  catalog request.
- If an app update fails, the existing app and its `appdata` are preserved by
  the existing rollback/backup logic. New app downloads use their own `appdata`.
- Launcher assignments are preserved; a newly installed app only occupies a
  free launcher slot.
