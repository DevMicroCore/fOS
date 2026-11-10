# Relative App-Oberflächen

Die enthaltenen CrowPanel- und Waveshare-Hauptsketches unterstützen bereits
Prozentwerte in `layout.ui`. Ein zusätzlicher Patch ist nicht nötig.
Vor Nutzung der relativen Apps die passende enthaltene Firmware flashen;
eine ältere Firmware mit derselben Versionsnummer 4.1.0 kann den Loader noch
nicht enthalten. Die CrowPanel-Firmware meldet beim Öffnen einer App:

```text
[FAPP-LAYOUT] Relative units enabled; viewport=800x480
```

Rechner, Counter, Zeichnen, Wetter, Radio, Text und die drei UI-Demos verwenden
auf beiden Displays dieselben Layoutdateien. Schriftgrößen bleiben Pixelgrößen.
Der native Radio-Eintrag `radio_demo` verwendet ebenfalls passende Geometrie.

## Prozentangaben bearbeiten

```text
type=button;id=example;x=20%;y=15%;w=60%;h=12%;text=Example
```

- x/w beziehen sich auf die **Inhaltsbreite des jeweiligen Elternobjekts**.
- y/h beziehen sich auf dessen **Inhaltshöhe**, nicht grundsätzlich den Bildschirm.
- Dezimalwerte wie 12.5% sind erlaubt. `tab_height` ist relativ zur Tabview-Höhe.
- Ohne Einheit bleiben Werte Pixel; auch `20px` ist möglich. Bestehende Pixel-
  Layouts funktionieren weiter. Ungültige/überlaufende Angaben fallen auf den
  bisherigen Standard zurück.
- Prozentwerte werden beim Öffnen in Pixel umgerechnet. So erhalten Canvas-
  Puffer ihre echte Größe, und Tastaturen bleiben an das richtige Feld gebunden.
  Bei einem Wechsel der App-Flächengröße die App neu öffnen.
- Schriftgrößen, Farben, Rundungen und Zeichenstiftbreite behalten ihre bisherigen
  Werte; diese Umstellung betrifft Positionen und Elementgrößen.

## Vorschau mit fosrun

Die aktualisierte Entwicklungsumgebung liegt im vollständigen Projekt. Dort
`make` im Ordner fScript_Development_Environment/fosc ausführen. Beispiele:

```sh
./build/fosrun /Pfad/apps/text/main.fapp --ui /Pfad/apps/text/layout.ui --size 800x480 --serve
./build/fosrun /Pfad/apps/text/main.fapp --ui /Pfad/apps/text/layout.ui --size 1024x600 --serve
```

Ohne --size bleiben 800×480 voreingestellt. Die Browser-Vorschau simuliert
Verhalten und Größen; die LVGL-Theme-Paddings können im Browser abweichen.
Die eigentlichen Layouttests verwenden deshalb echtes LVGL.


Die Befehle für fosrun werden im Ordner `fScript_Development_Environment/fosc` ausgeführt.
