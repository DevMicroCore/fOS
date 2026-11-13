# Firmware und Recovery

**Hinweis für fOS 4.1.1:** Alle bereits im Ordner `firmware/` enthaltenen
`.bin`-Dateien stammen aus dem hochgeladenen Vorgängerpaket. Sie wurden
**nicht** mit den 4.1.1-Änderungen neu kompiliert. Beide Hauptsketche aus
diesem Paket müssen mit der Arduino IDE neu gebaut und als neue `.bin`-Dateien
exportiert werden, bevor sie als 4.1.1 per OTA bereitgestellt werden. Die
Recovery-Images und Board-Partitionen bleiben unverändert.


Alle Pfade und Befehle in diesem Dokument beziehen sich auf den Projekt-Hauptordner.
Die Binärdateien sind Einzelkomponenten, keine zusammengeführten Flash-Images.
Für den ersten Haupt-Sketch-Upload die Arduino IDE verwenden.

| Board | Hauptanwendung | Recovery | app0 | app1 |
| --- | --- | --- | --- | --- |
| CrowPanel ESP32-S3 | `firmware/CrowPanel/fOS4.0.1-CrowPanel.ino.bin` | `firmware/CrowPanel/recovery.ino.bin` | 0x10000 | 0x2E0000 |
| Waveshare ESP32-P4 | `firmware/fOS_Waveshare_P4.ino.bin` | `firmware/recovery_p4.ino.bin` | 0x10000 | 0x1910000 |

Die Dateien `firmware/bootloader.bin`, `firmware/partitions.bin` und
`firmware/boot_app0.bin` gehören ausschließlich zum Waveshare-P4.
Firmware und Recovery verschiedener Boards sind nicht austauschbar.

## Waveshare


Built with ESP32 core 3.3.12 for Before v3.00 P4 silicon, 32MB flash and PSRAM.
These are separate components, not a merged image. The recommended first step
is uploading the main sketch from Arduino IDE with the settings in ../README.md.

Main application-only image offset: 0x10000.
Optional recovery application-only image offset: 0x1910000.
Partition table offset: 0x8000. Bootloader offset: 0x2000.
boot_app0.bin offset: 0xE000 (initial OTA selection; do not rewrite during updates).

Use the included partition table for this port. See WAVESHARE.md for recovery
installation. `firmware/SHA256SUMS.txt` covers the five .bin files.

This table allocates app0 25 MiB, app1 1 MiB and SPIFFS 5.9375 MiB.
Upload the main sketch normally when migrating from the previous layout. Do
not update only partitions.bin via an application OTA update. Reinstall recovery
at 0x1910000; the old 0xC10000 location is now inside the enlarged main slot.

## CrowPanel


`fOS4.0.1-CrowPanel.ino.bin` ist die frisch gebaute ESP32-S3-Anwendung mit
Prozent-Layoutunterstützung. Kein komplettes/zusammengeführtes Flash-Image;
keinen P4-Bootloader verwenden. Am einfachsten den enthaltenen Sketch gemäß
[CROWPANEL.md](CROWPANEL.md) in der Arduino IDE kompilieren und laden.

Build: ESP32-Core 3.3.12, LVGL 8.3.11, LovyanGFX 1.2.29, AudioI2S 3.4.7,
4MB Flash, OPI PSRAM, mitgelieferte CrowPanel-Partitionstabelle.
Das Programm passt in deren 0x2D0000 große Hauptpartition.
Die beim Build verwendete LVGL-Konfiguration steht nur als Referenz unter
`../firmware/CrowPanel/lv_conf_build_example.h`. Die bestehende funktionierende
Konfiguration muss für diese Layoutkorrektur nicht ersetzt werden.
Kein physischer Hardwaretest durchgeführt.

### CrowPanel-Recovery


`recovery.ino.bin` ist das unveränderte Recovery-Anwendungsimage aus dem
ursprünglich gelieferten CrowPanel-Projekt. Quellcode: `recovery`.
Es wurde beim Zusammenstellen dieses Pakets nicht neu kompiliert.
Die CrowPanel-Partitionstabelle legt die Recovery in app1 bei 0x2E0000 ab
(Slotgröße 0x080000). Diese Datei gehört ausschließlich zum ESP32-S3.
Die neu kompilierte Hauptanwendung ist `fOS4.0.1-CrowPanel.ino.bin`.

### CrowPanel-Recovery bauen und verteilen

The recovery sketch must be compiled with the same ESP32-S3 board, flash size,
Arduino-ESP32 Core and partition layout as the main fOS sketch.

For the current project use Arduino-ESP32 Core 3.3.12, export the compiled
binary from `recovery/recovery.ino`, and publish it as:

`Crowpanel_7"_esp32s3/update/recovery.ino.bin`

Then regenerate and commit the repository indexes:

```sh
python3 tools/generate_repository_indexes.py /path/to/fOS-repository
```

The main fOS updater installs this recovery image into `app1`. Do not upload the
recovery sketch with the normal Arduino Upload button: that would place it in
the normal application slot instead of the dedicated recovery partition.

