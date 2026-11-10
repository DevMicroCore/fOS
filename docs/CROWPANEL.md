# CrowPanel: relative App-Oberflächen, fOS 4.1.0

Die CrowPanel-Firmware ist hier bereits angepasst. Es muss kein Python-Patch
mehr ausgeführt werden. Die Systemversion bleibt **4.1.0**.

## Ursache und Änderung

Der vorherige CrowPanel-Loader verwendete `String.toInt()` für x/y/w/h.
Dadurch wurde beispielsweise `w=85%` als nur 85 Pixel interpretiert. Jetzt
berechnet der Loader Prozente anhand der tatsächlichen Inhaltsbreite/-höhe des
Elternelements: 85% ergeben bei 800 Pixeln 680 Pixel und bei 1024 Pixeln 870 Pixel.
Verschachtelte Panels und Dialoge beziehen sich auf ihre eigene Inhaltsfläche.
Die bestehenden reinen Pixelangaben bleiben verwendbar. Textfelder,
Tab-Leisten, Zeichenfläche und Tastatur verwenden die aufgelösten Pixelgrößen.
Auch der ältere native Radio-Eintrag hat jetzt relative Geometrie.

## Auf dem CrowPanel installieren

1. Vom Projekt-Hauptordner aus den ganzen Ordner `fOS4.0` verwenden und darin
   `fOS4.0.ino` in der Arduino IDE öffnen.
2. Wie beim bisherigen CrowPanel-Projekt ESP32-S3, ESP32-Core **3.3.12**,
   **OPI PSRAM**, LVGL **8.3.11**, LovyanGFX **1.2.29** und
   ESP32-audioI2S **3.4.7** verwenden. Die bisherigen funktionierenden
   Flash-/Display-Einstellungen sowie die LVGL-Konfiguration beibehalten.
   LVGL 9.2.2 gehört zum separaten P4-Projekt.
3. Neu kompilieren und die Firmware per USB auf das CrowPanel laden.
   Nur die SD-Dateien auszutauschen behebt diesen Loader-Fehler nicht.
4. Die bereits gelieferten relativen Apps weiterverwenden; identische Kopien
   liegen hier in `fOS4.0/example app/apps`. Bei Bedarf nur die Dateien der
   jeweiligen App aktualisieren; eigene `appdata`-Ordner beibehalten.

Beim Öffnen einer App muss im seriellen Monitor stehen:

```text
[FAPP-LAYOUT] Relative units enabled; viewport=800x480
```

Damit lässt sich die korrigierte Firmware von älteren Builds mit derselben
Versionsnummer 4.1.0 unterscheiden. Für das Waveshare bleibt das vorhandene
P4-Projekt zuständig. Die `layout.ui`-Dateien sind auf beiden Geräten identisch.

Die SquareLine-Dateien, CrowPanel-Pinbelegung und Displaytreiber bleiben
unverändert. Diese Korrektur erweitert die Auswertung der App-Layouts im Loader.

## Prüfung

Der tatsächliche CrowPanel-Renderer wurde mit echter LVGL 8.3.11 und den neun
relativen App-Layouts bei 800x480 geprüft: Objektgrenzen, verschachtelte Dialoge,
Zeichenpuffer, Tastaturgröße und Fokus. Native Radio-Geometrie, alte Pixelangaben
und ungültige Größenangaben sind ebenfalls geprüft. Die bisherigen Prüfergebnisse sind in [VALIDATION.md](VALIDATION.md) zusammengefasst. Das ersetzt keinen Test am physischen Gerät.

Test wiederholen:

```sh
python3 tools/tests/crowpanel_relative_layout/build.py --lvgl8 /pfad/zu/lvgl-8.3.11 --build /tmp/fos-crow-layout
```

Der vollständige Arduino-Build mit den oben genannten Versionen ist erfolgreich.
Sketch uses 2871471 bytes (17%) of program storage space. Maximum is 16777216 bytes.
Die Anwendung passt in die mitgelieferte CrowPanel-Hauptpartition (0x2D0000).
