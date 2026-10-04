# Step 12 – Complete fAPP Example and End-to-End Validation

`example app/apps/fscript_counter` is a complete installable SD-card app. Copy
that folder into `/apps` on the SD card. It contains:

* `app.json` and a legacy `app.cfg` fallback
* `layout.ui` with four deterministic object IDs
* editable `main.fscript` source
* compiled and verified `main.fapp`

The app initializes through `app.start`, preserves a global counter across
button events, changes label text through the UI adapter, handles theme changes
and cleans up through `app.close`.

To rebuild it on macOS:

```bash
cd fScript_Development_Environment/fosc
make
./build/fosc check "../../fOS4.0/example app/apps/fscript_counter/main.fscript" \
  --ui "../../fOS4.0/example app/apps/fscript_counter/layout.ui"
./build/fosc build "../../fOS4.0/example app/apps/fscript_counter/main.fscript" \
  --ui "../../fOS4.0/example app/apps/fscript_counter/layout.ui" \
  -o "../../fOS4.0/example app/apps/fscript_counter/main.fapp"
./build/fosc verify "../../fOS4.0/example app/apps/fscript_counter/main.fapp"
```

`make test` includes a host-side end-to-end suite that compiles an app, writes
the fAPP image, validates it through the embedded loader, runs initialization
and lifecycle handlers in the real VM, dispatches a click through the event
table, checks persistent global state and exercises manifest rejection.
