# Build- und Prüfnachweise

Die Dokumentationsbereinigung ändert weder Firmware-/Compiler-Quellcode noch
SquareLine-Dateien, App-Logik, Layouts, Partitionstabellen oder Firmware-Images.
Die Prüfsummen der enthaltenen Binärdateien werden beim Packen kontrolliert.

## Bisherige Prüfungen dieses Projektstands

- CrowPanel-Hauptsketch: vollständiger Arduino-Build mit Core 3.3.12,
  LVGL 8.3.11, LovyanGFX 1.2.29 und AudioI2S 3.4.7 erfolgreich.
- Waveshare-Hauptsketch und Recovery: Core 3.3.12, LVGL 9.2.2,
  Before v3.00, PSRAM und 32 MB Flash erfolgreich gebaut.
- Tatsächliche LVGL-Renderer mit den relativen Apps geprüft; Objektgrenzen,
  Dialoge, Tastatur und Zeichenpuffer sowie native Radio-Geometrie abgedeckt.
- Gemeinsame fosc-/fosrun-Entwicklungsumgebung aus dem Gesamtordner gebaut;
  Rechner-App-Test `7 + 8 = 15` erfolgreich.
- Das CrowPanel-Recovery-Image ist der unverändert übernommene ursprüngliche
  Build und wurde beim Zusammenführen nicht neu kompiliert.
- Kein physischer Board-Test in dieser Umgebung. P4-Batteriepufferung und
  Schlaf-/Aufwachverhalten müssen am Gerät geprüft werden.

CrowPanel-Buildausgabe:

```text
Sketch uses 2871471 bytes (17%) of program storage space. Maximum is 16777216 bytes.
Global variables use 92160 bytes (28%) of dynamic memory, leaving 235520 bytes for local variables. Maximum is 327680 bytes.
lvgl                  8.3.11  /workspace/scratch/d210890d37f0/crowpanel-build/libraries/lvgl
LovyanGFX             1.2.29  /workspace/scratch/d210890d37f0/crowpanel-build/libraries/LovyanGFX
ESP32-audioI2S-master 3.4.7   /workspace/scratch/d210890d37f0/crowpanel-build/libraries/ESP32-audioI2S
esp32:esp32   3.3.12  /workspace/scratch/d210890d37f0/responsive-build/data/packages/esp32/hardware/esp32/3.3.12

```

## Prüfwerkzeuge

Aus dem Projekt-Hauptordner:

```sh
python3 tools/tests/crowpanel_relative_layout/build.py --lvgl8 /pfad/zu/lvgl-8.3.11 --build /tmp/fos-crow-layout
python3 tools/tests/relative_layout/build.py --lvgl8 /pfad/zu/lvgl-8.3.11 --build /tmp/fos-layout
python3 tools/tests/test_appstore_appdata.py
```

Die übrigen Plattformtests stehen unter `tools/tests`. Compiler-/VM-Tests
werden im Ordner `fScript_Development_Environment/fosc` mit `make test` gestartet.

## Handbuch-Abgleich vom 21.10.2026

- Deutsch- und Englisch-Handbuch anhand Compiler, Semantikprüfung, Firmware-
  Layout-Loader, UI-Adapter, Storage-Policy und Runtime überarbeitet.
- Vollständige Inventare abgeglichen: 35 native Funktionen, 16 UI-Methoden-IDs,
  6 Eigenschaften, 14 Ereignis-IDs und 13 UI-Typen.
- Relative Layouts, Pointer-Snapshots, Canvas, Slider, Tabviews, Audio und
  Konfigurationsdateien in die Hauptreferenz integriert.
- Zählerbeispiel aus beiden Handbüchern geprüft, kompiliert, bytecode-verifiziert
  und bei 800x480 sowie 1024x600 ausgeführt: Ergebnis `Count: 1`.
- Canvas-Beispiel aus beiden Handbüchern kompiliert/verifiziert und bei beiden
  Displaygrößen in fosrun gestartet.
- Sechs mitgelieferte fScript-Apps neu in temporäre Ausgaben kompiliert und
  verifiziert; die vorhandenen App-Dateien bleiben dabei unverändert.
- `make test` bestanden: SD-Ordnerrechte, Lexer, Parser, Semantik, Compiler,
  Optimierer/Verifier, Embedded-Loader, Runtime/Manifest und native APIs sowie
  fosrun-/Browser-/Rechner-Smoke-Tests.
- Kein neuer Board-Firmwarebuild und kein Hardwaretest erforderlich oder
  durchgeführt; nur Dokumentation und das neue Dokumentationsbeispiel geändert.
