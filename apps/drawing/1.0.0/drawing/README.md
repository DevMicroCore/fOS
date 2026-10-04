# fScript-Zeichen-App

Dieses Beispiel verwendet das neue `canvas`-Element, die Ereignisse
`pointer_down`, `pointer_move` und `pointer_up` sowie `event_x()` und
`event_y()`. Der Canvas-Puffer wird auf dem Gerät nur während der laufenden App
im PSRAM gehalten.

Build aus `fScript_Development_Environment/fosc`:

```bash
./build/fosc build "../../fOS4.0/example app/apps/fscript_drawing/main.fscript" \
  --ui "../../fOS4.0/example app/apps/fscript_drawing/layout.ui" \
  -o "../../fOS4.0/example app/apps/fscript_drawing/main.fapp"
./build/fosc verify "../../fOS4.0/example app/apps/fscript_drawing/main.fapp"
```

Desktop-Vorschau:

```bash
./build/fosrun "../../fOS4.0/example app/apps/fscript_drawing/main.fapp" \
  --ui "../../fOS4.0/example app/apps/fscript_drawing/layout.ui" --serve
```
