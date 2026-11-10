# fOS 4.1.0

Aktueller Paketstand für CrowPanel ESP32-S3 (800×480) und Waveshare ESP32-P4
(1024×600). Die Systemversion bleibt auf beiden Geräten 4.1.0.

- Gemeinsame fScript-Apps mit relativen Größen; korrigierter CrowPanel-Loader.
- Angepasste Geometrie auch für die native Radio-App.
- fScript-Radio mit File Player, Web Radio, Lautstärke, Fortschritt und Seek;
  aktive Tab-Rahmen und Roller-Auswahl folgen der Themefarbe.
- fosc 0.14.0, Bytecode 1.8 und passende Audio-/UI-APIs.
- App-spezifische SD-Ordnerrechte; `/system` bleibt für fApps geschützt.
- App-Updates erhalten `appdata`; Tastaturen verwenden den integrierten Loader.
- Waveshare-Port mit Display-/Touch-Drehung, RTC-/VBAT-Unterstützung,
  USB-SD-Speicher und 32-MB-Partitionierung.

Board-Builds und bisherige Tests: [VALIDATION.md](VALIDATION.md).
Hardware-Prüfgrenzen für RTC und Schlafbetrieb: [RTC.md](RTC.md).

## Wichtiger Hinweis zu den Firmware-Binaries

Die Firmware-Quelltexte verwenden bereits fOS 4.1.0. Die aus dem ursprünglichen ZIP übernommenen `.bin`-Dateien enthalten jedoch teilweise noch die Versionskennung 4.0.1. Sie wurden in diesem App-/Metadaten-Update **nicht neu kompiliert**. Vor dem Veröffentlichen der Firmware-Downloads beide Hauptanwendungen neu bauen, die Binaries ersetzen und die OTA-Indizes, Build-Metadaten sowie SHA256-Prüfsummen aktualisieren.
