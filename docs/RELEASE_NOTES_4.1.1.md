# fOS 4.1.1 — Release notes

Date: 2026-11-10

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

## Versioning and deployment

The application version in both `FOSVersion.h` files is now **4.1.1**.
No changes are needed to the recovery images, flash partition tables,
fScript bytecode, or existing app manifests (`min_fos: 4.1.0` remains valid).

**Compile both sketches to generate actual 4.1.1 firmware binaries.** The
precompiled `.bin` files and OTA indexes retained from the uploaded archive
are older artifacts, not 4.1.1 images. Do not rename them or publish them
as 4.1.1 without a real rebuild and fresh checksums.

## Verification

The existing host AppStore test passed all 17 appdata/install/rollback scenarios,
including both index-based and GitHub API download paths. Additional static
checks verify that the Download callback schedules a worker rather than doing
blocking downloads. No physical-board test or firmware compilation was possible
in this environment. To confirm hardware behavior, install a rebuilt 4.1.1
image and download then update an app on both boards, checking serial logs
for `[APPSTORE] Transfer result=`.

## Follow-up fix (2026-11-11): Waveshare uninstall and CrowPanel HTTPS diagnosis

- **Waveshare ESP32-P4:** Uninstall no longer calls recursive SD directory
  deletion from the LVGL `AppStoreDeleteApp` callback on `loopTask`. This was
  consistent with the reported `Core 1 panic'ed (Stack protection fault)`.
  Deletion now runs in the existing AppStore FreeRTOS transfer worker, and the
  LVGL progress timer applies launcher-slot and UI updates only on success.
  Download, uninstall, and catalog refresh cannot overlap. Removing an app
  still removes its own `appdata` along with the app directory; updating an
  app still preserves `appdata`.
- **CrowPanel ESP32-S3:** The display does **not** reboot on failed download.
  Its logs show HTTPS GET error `-1` (`connection refused`) and a failed
  transfer (`result=5`, `files=0`). This is not the P4 stack issue. If a GET
  fails, serial logs now report Wi-Fi status, RSSI and internal heap totals /
  largest allocatable block to distinguish common connection and TLS memory
  problems. Connection failures get a longer retry delay and the display
  explicitly says `Download failed: HTTPS connection` where applicable.
  A network or TLS failure cannot be guaranteed to be fixed without an
  actual-device test and updated serial diagnostic logs.

### Scope and verification

The version remains **4.1.1** in both `FOSVersion.h` files. The 17 host-side
AppStore installation/update/rollback scenarios still pass. Additional static
checks ensure that Waveshare deletion uses the worker rather than executing
`removeRecursively` in the LVGL button callback. No on-device build or hardware
validation has been performed. Older packaged firmware `.bin` files remain
older binaries; rebuild both board sketches before distributing OTA images.


### CrowPanel AppStore HTTPS follow-up (2026-11-11)

Reuses the online/default AppStore catalog from PSRAM (or flash fallback) for installation instead of re-downloading it. Adds underlying TLS-error and DNS/TCP reachability diagnostics to distinguish heap pressure from network errors. File HTTPS may still fail until hardware evidence identifies the actual handshake failure. Full details: [CROWPANEL_APPSTORE_HTTPS_411.md](CROWPANEL_APPSTORE_HTTPS_411.md).

### CrowPanel follow-up: TLS memory and lwIP workaround (11 Nov 2026)

- App download worker's FreeRTOS stack preferentially moved to PSRAM
  (`xTaskCreateWithCaps` where supported) to recover contiguous internal
  RAM required by mbedTLS. Conservative 8 KiB internal fallback.
- Resolve NTP hosts before initializing SNTP to avoid known DNS-cache
  callback race with HTTPS after Wi-Fi reconnection.
- Additional pre-task and pre-TLS heap diagnostics. Hardware verification
  still required; old bundled firmware binaries were not rebuilt.
