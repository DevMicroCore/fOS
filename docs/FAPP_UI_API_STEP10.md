# Step 10 – fAPP UI API

The VM has no LVGL dependency. `FAppUiApi` is the runtime-facing interface and
`LvglFAppUiApi` is its firmware adapter. This separation keeps VM tests
platform-independent and prevents scripts from receiving raw LVGL pointers.

Supported properties are `text`, `value`, `checked`, `hidden` and `enabled`.
Supported methods are `clear`, `focus`, `blur`, `scroll_to_top` and
`scroll_to_bottom`. Compatibility is checked by `fosc` and again by the
adapter. Every operation resolves a numeric object ID through the fixed UI
registry and requires the `ui` permission.

Layout objects include label, button, textarea, switch, checkbox, panel,
roller, dropdown and keyboard. Roller/dropdown `text` contains newline-separated
options and `value` is the selected zero-based index. `parent=` supports nested
panels, while a keyboard can be bound using `target=<textarea-id>`.
