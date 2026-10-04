# Step 9 – LVGL Event Bridge

`FAppEventBridge` connects named objects from `layout.ui` to numeric bytecode
event bindings. The bridge translates LVGL events into a fixed eight-entry VM
queue:

| fScript event | LVGL event |
| --- | --- |
| `click` | `LV_EVENT_CLICKED` |
| `changed` | `LV_EVENT_VALUE_CHANGED` |
| `value_changed` | `LV_EVENT_VALUE_CHANGED` |
| `pressed` | `LV_EVENT_PRESSED` |
| `released` | `LV_EVENT_RELEASED` |

Callbacks never execute script code directly. They enqueue a compact object
ID/event pair; the regular fOS loop later runs the handler within the VM budget.
The bridge is detached before LVGL app objects are deleted.
