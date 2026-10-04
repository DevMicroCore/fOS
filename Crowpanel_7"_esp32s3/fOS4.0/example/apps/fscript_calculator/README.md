# fScript Calculator

This folder contains the uncompiled fScript version of the native fOS
calculator app.

Files:

* `app.json` - fAPP manifest for fOS 4.0.0
* `app.cfg` - legacy launcher metadata
* `layout.ui` - UI layout with stable object IDs
* `main.fscript` - calculator logic

Build from `fScript_Development_Environment/fosc`:

```bash
make
./build/fosc check "../../fOS4.0/example app/apps/fscript_calculator/main.fscript"
./build/fosc build "../../fOS4.0/example app/apps/fscript_calculator/main.fscript" -o "../../fOS4.0/example app/apps/fscript_calculator/main.fapp"
./build/fosc verify "../../fOS4.0/example app/apps/fscript_calculator/main.fapp"
```

Copy the complete `fscript_calculator` folder to `/apps/fscript_calculator` on
the SD card after building. Keep `main.fapp` beside `app.json` and `layout.ui`.
