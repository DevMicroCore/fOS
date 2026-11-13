# CrowPanel AppStore HTTPS diagnostics (fOS 4.1.1, 2026-11-11)

## Observed device log

```text
[APPSTORE] GET failed (-1, connection refused, try 1): https://raw.githubusercontent.com/DevMicroCore/fOS/main/apps/fos-appstore.index
[APPSTORE] HTTPS diagnostic: error=-1, WiFi=3, RSSI=-51 dBm, internal free=62748, largest=31732 bytes
[APPSTORE] Transfer result=5, files=0
```

`WiFi=3` (`WL_CONNECTED`) and RSSI -51 dBm show the CrowPanel is associated with its Wi-Fi network. They do **not** establish Internet reachability or prove an mbedTLS out-of-memory error. HTTPClient's `-1` reports failure to establish a connection; it cannot distinguish DNS/TCP/TLS failures on its own.

## Source changes

The CrowPanel `fOS4.0/AppStoreRuntime.cpp` now:

- Reuses the exact default AppStore catalog selected during list refresh, avoiding a redundant HTTPS request at installation time.
- Stores a successfully received online catalog in **external PSRAM**, not internal heap. The built-in flash fallback is reused without networking. The cache is invalidated when the user refreshes the catalog.
- Retains the existing strict index version selection, file download URL handling, update rollback and `appdata` preservation. An online catalog is **not** silently replaced with a different flash version if the PSRAM cache cannot be allocated.
- Logs `WiFiClientSecure::lastError()` on failed text and file requests, before the HTTP client is closed.
- After a final HTTPS failure, probes DNS and plain TCP/443 to distinguish network reachability from TLS failure. The actual downloads continue to use HTTPS; TCP probes do not transfer app files or credentials.

This **eliminates a redundant TLS handshake**, but does **not yet guarantee** that HTTPS file transfers will succeed. Those still need a TLS handshake. If `TCP/443=reachable` but TLS errors persist, obtain the new `TLS details` or `File TLS details` line to distinguish memory pressure from other handshake errors. If TCP/443 is unreachable, troubleshoot DNS/router/network access. If `lastError()` reports allocation failure, a more intrusive memory optimization will be required.

## Retest

1. Rebuild and flash the CrowPanel sketch `fOS4.0/fOS4.0.ino` with your usual compatible ESP32-S3 + OPI PSRAM settings. Old precompiled `.bin` files are *not* this patch.
2. Open AppStore. Check whether the index refresh loads online or falls back to the built-in catalog.
3. Choose an app and press Download. Inspect these serial log lines:
   - `[APPSTORE] Using PSRAM-cached AppStore index for installation.` or `[APPSTORE] Using built-in AppStore index for installation.`
   - `[APPSTORE] File GET failed ...`, `[APPSTORE] File TLS details ...`, `[APPSTORE] Network check ...` if download continues to fail.
4. Verify the installed app and its appdata directory; update an app and ensure user data survives.

Only CrowPanel's `AppStoreRuntime.cpp` is modified in this follow-up patch; the existing Waveshare deletion fix remains as shipped in the previous fOS 4.1.1 AppStore fix archive.

All 17 existing host AppStore install/update/rollback tests and the two-board async static checks passed. These tests do not exercise real ESP32 TLS/SD/Wi-Fi hardware. A firmware compilation and physical-board test were not available in this environment.

## RAM/TCPIP corrections (11 Nov 2026)

- Confirmed on device: `File TLS details: error=-32512` with the largest
  internal block only 18,420 bytes. This is `MBEDTLS_ERR_SSL_ALLOC_FAILED`,
  not a GitHub outage. Creating the old 12 KiB download task in DRAM was
  directly responsible for the drop from ~31 KiB to ~18 KiB.
- AppStore download task now prefers a 12 KiB **PSRAM stack** via
  `xTaskCreateWithCaps` when `CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM`
  is enabled in the Arduino/ESP-IDF core. It uses the matching
  `vTaskDeleteWithCaps`, and never changes the OTA flash-writing tasks.
- The automatic fallback uses an 8 KiB internal stack (the same size as
  the existing AppStore refresh task). The fallback may still fail TLS on
  very fragmented heaps. Inspect the new `Download worker stack:` log.
- NTP server names are resolved before `configTzTime`, and only numeric
  addresses are supplied to SNTP. This avoids a reported Arduino-ESP32
  DNS cache / TCPIP core-lock race after Wi-Fi reconnection (issue #13055,
  fix #13092). Timezone settings and DST handling remain unchanged.
- No workaround bypassing TLS or installing unencrypted HTTP files was added.

**Validation:** source-level/host unit tests only; neither board has been
flashed or tested with this version in this environment. The included old
firmware `.bin` files must not be distributed as this version.
