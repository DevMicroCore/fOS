# fScript Canvas- und Pointer-API

Ab Bytecode-Version 1.6 kann ein Layout eine Zeichenflaeche anlegen:

```text
type=canvas;id=drawing;x=20;y=20;w=600;h=400;bg=0xFFFFFF;scrollable=false
```

Der Pixelpuffer wird beim Oeffnen der App passend zu `w * h` im PSRAM
angelegt und beim Loeschen des Canvas wieder freigegeben. Ohne ausreichend
PSRAM wird der Canvas nicht erstellt.

## Pointer-Ereignisse

Canvas und Panel unterstuetzen:

- `pointer_down`
- `pointer_move`
- `pointer_up`

Innerhalb des gerade ausgefuehrten Event-Handlers liefern folgende Funktionen
die Daten dieses Queue-Eintrags:

- `event_x()` und `event_y()`: Koordinaten relativ zum beruehrten Objekt
- `event_screen_x()` und `event_screen_y()`: absolute Displaykoordinaten
- `event_pressed()`: `true`, solange der Zeiger gedrueckt ist

Ausserhalb eines Pointer-Handlers liefern die Koordinatenfunktionen `0` und
`event_pressed()` liefert `false`.

## Zeichenmethoden

Farben werden als ganzzahlige RGB-Werte von `0` bis `16777215` angegeben.

```text
drawing.clear(color)
drawing.pixel(x, y, color)
drawing.line(x1, y1, x2, y2, color, width)
drawing.rect(x, y, width, height, color, filled)
drawing.circle(center_x, center_y, radius, color, filled)
drawing.draw_text(x, y, text, color, size)
```

`draw_text` verwendet die eingebauten Schriftgroessen 20, 24 und 40; bei
anderen Werten kommt die LVGL-Standardschrift zum Einsatz.

Ein vollstaendiges Beispiel liegt unter
`example app/apps/fscript_drawing`. `fosrun --serve` stellt Canvas und
Pointer-Ereignisse ebenfalls im Browser dar. Fuer automatisierte Tests nimmt
der HTTP-Endpunkt Koordinaten entgegen, zum Beispiel:

```text
/event?name=drawing.pointer_move&x=30&y=40&screen_x=48&screen_y=58&pressed=1
```
