> Source audit: 2026-10-21; fOS 4.1.0, fosc 0.14.0, bytecode 1.8.

# fScript, fosc and VM Manual

Applies to: fOS 4.1.0, fScript 1.0.0, fosc 0.14.0, fosrun 0.2.1, fAPP 1.0, bytecode 1.8

This manual documents the complete currently implemented feature set of
fScript, the `fosc` PC compiler and the fAPP virtual machine in fOS. It serves
as a language reference, build guide and runtime specification.

## 1. Overview

An fScript application consists of editable source files on the PC and one
compiled executable for the ESP32:

```text
main.fscript + layout.ui + app.json
             |
             | fosc check/build
             v
          main.fapp
             |
             | copy to SD card
             v
      /apps/my_app/ on fOS
```

`main.fscript` contains program logic, `layout.ui` describes the interface and
`app.json` contains metadata and permissions. `fosc` validates and compiles the
sources on macOS, Linux or Windows. fOS then loads `main.fapp` from the SD card
and executes it cooperatively in a bounded virtual machine. A fault terminates
only the affected app, not fOS.

## 2. Quick start on macOS

### 2.1 Requirements

Install Apple's Command Line Tools once:

```bash
xcode-select --install
```

Change into the compiler directory:

```bash
cd ~/Downloads/fOS4.1.0/fScript_Development_Environment/fosc
```

Adjust the path if the archive was extracted elsewhere. After typing `cd `,
you may also drag the folder from Finder into Terminal.

### 2.2 Build and test the compiler

```bash
make
make test
./build/fosc --version
```

Expected version:

```text
fosc 0.14.0
fScript 1.0.0
```

### 2.3 Check and build an app

Assuming the app directory contains `main.fscript`, `layout.ui` and
`app.json`:

```bash
./build/fosc check /path/to/app/main.fscript --ui /path/to/app/layout.ui
./build/fosc build /path/to/app/main.fscript \
  --ui /path/to/app/layout.ui \
  -o /path/to/app/main.fapp
./build/fosc verify /path/to/app/main.fapp
```

Copy the complete app directory to `/apps` on the SD card afterwards.

## 3. App directory and manifest

Recommended structure:

```text
/apps/weather/
├── app.json
├── app.cfg          # optional legacy fallback
├── layout.ui
├── main.fscript     # source; not required for execution on fOS
├── main.fapp        # compiled bytecode
├── state.txt        # optional app data
└── sound.mp3        # optional audio file
```

fOS reads up to **40** direct app subdirectories of `/apps`. The **7** home/launcher slots are a separate limit. Prefer `appdata` for app-owned state; Store updates preserve that directory.

### 3.1 `app.json`

```json
{
  "id": "org.example.my-app",
  "name": "My App",
  "version": "1.0.0",
  "min_fos": "4.1.0",
  "type": "ui",
  "icon": "APP",
  "layout": "layout.ui",
  "executable": "main.fapp",
  "scrollable": false,
  "permissions": ["ui", "network"]
}
```

| Field | Required | Meaning |
| --- | --- | --- |
| `id` | yes | Unique ID using letters, digits, `.`, `-`, `_` |
| `name` | yes | Launcher display name |
| `version` | yes | Exact `MAJOR.MINOR.PATCH` SemVer |
| `min_fos` | no | Oldest supported fOS version |
| `type` | no | Use `ui` for fScript apps; default is `ui` |
| `icon` | no | Short launcher text |
| `layout` | no | UI file; defaults to `layout.ui` |
| `executable` | no | Bytecode file; defaults to `main.fapp` |
| `scrollable` | no | Enables vertical scrolling of the app container |
| `permissions` | no | Explicit capabilities granted to the app |

The manifest is limited to 4096 bytes. `layout` and `executable` must be plain
filenames without `/`, `\` or `..`. fOS refuses to start an app when a present
manifest is invalid. If no manifest exists, `app.cfg` can supply legacy
metadata.

`id`, `name`, `layout` and `executable` are limited to 64 bytes each;
`version`, `min_fos`, `type` and `icon` to 16 bytes each. When a `permissions`
array is present, it replaces the default permission. A UI app must therefore
include `"ui"` explicitly in that array.

### 3.2 Permissions

| Permission | Operations |
| --- | --- |
| `ui` | UI properties, methods and events |
| `storage.read` | Read/list within the app folder |
| `storage.write` | Write within the app folder |
| `storage.read:/path` | Read/list that folder and subfolders |
| `storage.write:/path` | Write that folder and subfolders |
| `network` | HTTP/JSON, URL encoding, Wi-Fi status and audio streams |
| `audio` | Local playback and audio controls; streams also need network |
| `system.restart` | Request restart |

Folder grants use absolute SD paths. `/` grants access to the whole card
**except `/system`**, which is always protected, including its descendants.
Read and write grants are separate; write does not imply read. Unscoped storage
permissions apply only to the app directory. `audio` permits local playback
within the app; other folders require a matching read grant. Maximum 8 folder
grants using 512 path bytes in total, including terminators.

```json
"permissions": ["ui", "storage.read:/text", "storage.write:/text"]
```

File APIs and UI loaders enforce the same folder grants. Missing permissions produce a VM error or `false` from UI file methods; root listings omit `/system`. Store updates preserve `appdata`; explicit uninstall removes the app and its data. Details: [APP_RUNTIME.md](APP_RUNTIME.md).

### 3.3 Legacy `app.cfg`

```ini
name=My App
icon=APP
type=ui
layout=layout.ui
fapp=main.fapp
scrollable=false
```

Supported keys are `name`, `icon`, `content`, `layout`, `fapp`, `type`,
`scrollable`, `button_text` and `button_message`. New fScript apps should use
`app.json`, because capabilities can only be declared there.

## 4. `layout.ui`

Each nonempty line defines an object; `;` separates fields and lines starting with `#` are comments. There is no quoting/escaping syntax for semicolons.

### 4.1 Supported objects

| `type` | Purpose |
| --- | --- |
| `label` | Text display |
| `button` | Button with text |
| `textarea` | Single/multiline text input |
| `switch` | On/off switch |
| `checkbox` | Checkbox |
| `panel` | Container |
| `roller` | Roller selection |
| `dropdown` | Drop-down selection |
| `keyboard` | Textarea keyboard |
| `canvas` | RGB565 drawing buffer in PSRAM |
| `slider` | Integer slider |
| `tabview` | Tabs control; active index in .value |
| `tab` | Page of a previously created tabview |

Only objects with `id=` are accessible to fScript. IDs must be unique fScript identifiers. Up to 64 numeric IDs follow named-object order in the layout; rebuild `main.fapp` when that order changes. Failed canvas/keyboard creation keeps its reserved ID slot. `parent` must already exist; otherwise the object falls back to the app root. A keyboard `target` is resolved **after** the whole layout and may appear later.

### 4.2 Fields and dimensions

| Field | Meaning |
| --- | --- |
| `type` | Object type from the list above |
| `id` | Unique fScript identifier; optional for display-only objects |
| `x, y` | Position: pixels, px or %; default 0 |
| `w, h` | Size: pixels, px or %; default 220/50 |
| `parent` | ID of a previously created container |
| `text` | Text/options; \n and \t are decoded |
| `bg, fg` | RGB hex color; theme/accent/surface/contrast/text and theme aliases |
| `font` | 20, 24, 40; other values use the object default |
| `align` | Text alignment left/center/right (label, roller) |
| `radius, border` | Corner radius and border width in pixels |
| `clickable, scrollable, hidden` | Boolean flags; false/true, 0/1, no/yes, off/on |
| `placeholder, one_line, max_length` | textarea; max_length applied for 1–1024 |
| `native_style` | textarea/roller: native LVGL styling; button with bg=theme: dynamic theme binding |
| `styleless` | panel: remove default LVGL styles |
| `value` | switch/checkbox: Boolean; slider: initial value |
| `min, max` | slider: defaults 0/100; -32768…32767 and min < max |
| `tab_height` | tabview: pixels or % of tabview height; default 50 |
| `target` | keyboard: textarea ID; may be declared later |

Percent `x`/`w` use the **parent content width**; percent `y`/`h` use its
content height. `tab_height` uses the tabview's own height. Use a decimal point
(`12.5%`), not a decimal comma. Bare numbers or `px` mean pixels; results round
to the nearest pixel. Invalid input, nonfinite numbers and values outside
±32767 use the fallback. Dimensions resolve when opening the app, without
continuous automatic reflow. Colors, font sizes, radii and pen widths stay
pixel/color values.

```text
type=tabview;id=tabs;x=0%;y=18%;w=100%;h=82%;tab_height=15%;font=20
type=tab;id=page;parent=tabs;text=Controls
type=slider;id=volume;parent=page;x=5%;y=10%;w=90%;h=8%;min=0;max=21;value=10
```

A `tab` needs a previously created `tabview` parent. The tabview manages
its page position/size; x/y/w/h do not provide free geometry for a tab page.
`switch` and `checkbox` use native widget sizes rather than w/h. `label` honors
an explicitly supplied h. `max_length` is an optional input-limit setter, not
the limit for `textarea.load_file()` or VM strings.

Keyboards are bounded overlays within the app area so small dialogs cannot
clip their keys. Defaults: w = parent width, h = 197 pixels, y = parent height
minus h. Focusing the target shows the keyboard. Hidden parents or targets
hide it; `.hidden` retains the script's explicit visibility request. At most
8 keyboards per app.

Canvas requires PSRAM for `2 × width × height` RGB565 bytes. CrowPanel's loader
allows at most 800×480; P4 allows 1024×600. Deletion frees the buffer. Allocation
failure is reported without shifting later IDs. Drawing and pointer
coordinates use actual local pixels.

See [UI_LAYOUTS.md](UI_LAYOUTS.md) for further details on both display sizes.

## 5. fScript language reference

### 5.1 Identifiers, case and statement endings

Identifiers start with a letter or `_`; following characters may also be
digits:

```fscript
var temperature_1 = 21.5
var _internal = true
```

Variable, function and UI names are case-sensitive. Keywords are lower-case.
Newlines separate statements; semicolons are optional:

```fscript
var a = 1
var b = 2; var c = 3
```

### 5.2 Comments

```fscript
-- Lua-style line comment
// JavaScript-style line comment
/* Multi-line
   block comment */
```

Block comments do not nest.

### 5.3 Values and literals

| Type | Examples | Runtime representation |
| --- | --- | --- |
| `nil` | `nil` | no value |
| Boolean | `true`, `false` | truth value |
| Integer | `0`, `-42`, `100000` | signed 32-bit |
| Float | `3.14`, `1.5e-3` | 64-bit floating point |
| String | `"Hello"`, `'Text'` | up to 255 payload bytes |

String escapes are `\n`, `\r`, `\t`, `\\`, `\"` and `\'`. A string literal
cannot span source lines directly.

At runtime, only `nil` and `false` are falsey. All other values are truthy. The
static checker nevertheless requires known Boolean operands for `if`, `and`,
`or` and `not`.

### 5.4 Variables and scopes

```fscript
var counter = 0
var status

function increment(step)
  var next = counter + step
  counter = next
  return counter
end
```

Top-level variables are global and persist across UI events and
`app.theme_changed`. Variables inside functions and event handlers are local.
A declaration without an initializer starts as `nil`.

The checker infers a variable's type from its first useful assignment. Later
assignments must be compatible. An Integer may be assigned to a Float target,
but not the other way around.

### 5.5 Functions

```fscript
function add(a, b)
  return a + b
end

var result = add(4, 5)
```

Functions may be declared before or after a call. Parameters are dynamically
typed; argument and parameter counts must match. Functions and event handlers
may only be declared at the top level. Recursion is technically possible, but
the VM limits call depth to 8.

`return` without a value returns `nil`. Top-level `return` is forbidden. Event
handlers may only use `return` without a result value.

### 5.6 Conditions

```fscript
if temperature > 30 then
  lbl_status.text = "Hot"
else
  lbl_status.text = "Normal"
end
```

`else` is optional. Additional branches can be expressed with `elseif`:

```fscript
if value < 0 then
  lbl_status.text = "Small"
elseif value == 0 then
  lbl_status.text = "Zero"
else
  lbl_status.text = "Large"
end
```

Loops use a closing `end`, just like `if`. `while` expects a Boolean condition:

```fscript
while counter < 10 then
  counter = counter + 1
end
```

`for` counts inclusively from the start value to the end value. With `var`, the
loop variable is declared for the current function or handler. Without `var`,
it must already exist. `step` is optional and defaults to `1`; negative steps
count down.

```fscript
for var i = 1 to 10 step 2 then
  if i == 5 then
    continue
  elseif i == 9 then
    break
  end
end
```

`break` exits the nearest surrounding loop. `continue` starts the next
iteration; in `for` it jumps to the increment step first, while in `while` it
jumps back to the condition.

### 5.7 Operators

| Group | Operators | Example |
| --- | --- | --- |
| Assignment | `=` | `counter = 1` |
| Or | `or`, `||` | `ready or cached` |
| And | `and`, `&&` | `online && enabled` |
| Equality | `==`, `!=` | `value != nil` |
| Comparison | `<`, `<=`, `>`, `>=` | `temperature >= 20` |
| Addition | `+` | `a + b`, `"Value: " + a` |
| Subtraction | `-` | `a - b` |
| Multiplication | `*` | `a * b` |
| Division | `/` | `a / b`; result is Float |
| Modulo | `%` | `counter % 10` |
| Unary | `not`, `!`, `+`, `-` | `not ready`, `-value` |
| Grouping | `()` | `(a + b) * 2` |

The table is ordered from lowest to highest precedence. Assignment associates
right-to-left; other binary operators associate left-to-right. `+` joins a
String with scalar values using deterministic conversion:

```fscript
lbl_status.text = "Temperature: " + temperature + " °C"
```

Integer overflow, division by zero and a joined string exceeding 255 bytes are
contained runtime faults. String ordering is currently accepted by the
compiler but not implemented by the VM and ends in `runtime type error`. Use
numbers with `<`, `<=`, `>` and `>=`; `==` and `!=` also work with Strings.

## 6. Events

```fscript
on object_id.event
  // statements
end
```

### 6.1 System events

| Event | Meaning |
| --- | --- |
| `app.start` | After top-level initialization |
| `app.close` | Bounded final timeslice when closing |
| `app.theme_changed` | Theme changed; globals remain intact |
| `app.timer` | Recurring: 60000 ms default; timer.start changes interval |

### 6.2 UI events

| Event | Objects |
| --- | --- |
| `click`, `clicked` | all named objects |
| `change`, `changed`, `value_changed` | textarea, switch, checkbox, roller, dropdown, slider, tabview |
| `pressed`, `released` | button, switch, checkbox, slider |
| `ready`, `cancel`, `cancelled` | keyboard |
| `pointer_down`, `pointer_move`, `pointer_up` | canvas, panel |

`change` aliases `changed`. An LVGL value change queues both `value_changed`
and `changed` when handlers exist. Only one handler per object/event. Setting
a slider value programmatically does not generate the corresponding user
event. `pointer_up` also covers lost touch when reported by the input driver.

`event_x()`/`event_y()` read the captured point relative to the object's top-left
corner. `event_screen_x()`/`event_screen_y()` use logical display coordinates.
`event_pressed()` reads the captured pressed state. Only pointer handlers have
this context; otherwise coordinates are 0 and pressed is false. These are not
live touch-polling functions. `.pressed` instead reads the current LVGL state.
Pointer coordinates may lie outside the object.

The event queue has 8 slots. Consecutive pending `pointer_move` events for the
same object coalesce. When full, `pointer_up` can replace a pending move for
that object; if no such slot exists, it too can be dropped. Keep handlers short
and do not assume every intermediate point is delivered.

## 7. Accessing the UI from fScript

All UI access requires `ui`. Generic .x/.y/.w/.h properties are not exposed.

### 7.1 Properties

| Property | Type | Objects | Access |
| --- | --- | --- | --- |
| `.text` | String | label, button, textarea, checkbox, roller, dropdown | read/write |
| `.value` | Boolean | switch, checkbox | read/write |
| `.value` | Integer | roller, dropdown, slider, tabview | read/write |
| `.checked` | Boolean | switch, checkbox | read/write |
| `.hidden`, `.enabled` | Boolean | all | read/write |
| `.pressed` | Boolean | all | read only |

Use `.text` for textarea contents. The compiler may accept textarea `.value`, but the firmware does not implement it. For roller/dropdown, writing `.text` sets the entire options list (with `\n`), while reading returns the selected option. `.value` is a zero-based selection/tab index or a slider value. Slider values use signed 16-bit range; scale long-media progress to 0–1000.

### 7.2 Methods

| Method | Objects | Return | Behavior |
| --- | --- | --- | --- |
| `.clear()` | textarea | nil | Clear text |
| `.focus() / .blur()` | button, textarea, switch, checkbox, roller, dropdown, slider, tabview | nil | Set/remove focus; focus shows a bound keyboard |
| `.scroll_to_top() / .scroll_to_bottom()` | textarea, panel, roller | nil | Scroll to top/bottom |
| `.load_file(path)` | textarea | Boolean | Up to 4000 bytes + truncation marker; ui + read grant |
| `.save_file(path)` | textarea | Boolean | Save; create parent folders only inside the write grant |
| `.load_dir(path, root)` | roller | Boolean | Folders, files and .. within root; ui + read grant |
| `.load_dir_filtered(path, root, extensions)` | roller | Boolean | Like load_dir; filter file extensions and omit hidden entries |
| `.load_pairs(path, separator)` | roller | Boolean | Names from name/value file directly as options; ui + read grant |
| `.set_range(min, max)` | slider | Boolean | Integer range -32768…32767, min < max |
| `.clear(color)` | canvas | Boolean | Fill the entire canvas |
| `.pixel(x, y, color)` | canvas | Boolean | One pixel |
| `.line(x1, y1, x2, y2, color, width)` | canvas | Boolean | Line; width 1–64 |
| `.rect(x, y, w, h, color, filled)` | canvas | Boolean | Rectangle; w/h > 0, filled Boolean |
| `.circle(x, y, radius, color, filled)` | canvas | Boolean | Circle around center; radius > 0 |
| `.draw_text(x, y, text, color, font)` | canvas | Boolean | Text; font 20/24/40, otherwise default font |

Canvas arguments are integers except text and Boolean `filled`. Script colors are decimal RGB numbers 0–16777215 (white = 16777215), not layout-style hexadecimal notation. Coordinates are pixels; bound them and do not pass percentages to drawing methods.

Directory/file loaders also need read grants, and `save_file` needs write grants. `load_dir` bypasses the 255-byte string limit with up to 16384 option bytes. Folders precede files; order within each group is not guaranteed. `extensions` can be `.mp3,.wav,.ogg`, matched case-insensitively. `load_pairs` and `file_pair_value` share a reader: one separator character, split at its first occurrence, trim fields and skip invalid/empty/oversized rows. At most 40 valid rows from a 16 KiB file; name/value each at most 255 bytes.

### 7.3 Canvas example

```text
type=canvas;id=drawing;x=5%;y=20%;w=90%;h=70%;bg=0xFFFFFF
```

```fscript
on app.start
  drawing.clear(16777215)
  drawing.rect(10, 10, 50, 30, 255, false)
  drawing.line(0, 0, 40, 40, 0, 3)
  drawing.draw_text(15, 60, "Draw here", 0, 20)
end

on drawing.pointer_down
  drawing.circle(event_x(), event_y(), 2, 0, true)
end

on drawing.pointer_move
  if event_pressed() then
    drawing.pixel(event_x(), event_y(), 0)
  end
end
```

## 8. Native fOS APIs

### 8.1 Complete list

| Function | Return | Permission | Behavior |
| --- | --- | --- | --- |
| `http_get(url)` | Boolean | `network` | HTTP/HTTPS GET; true for HTTP 200–299 |
| `http_status()` | Integer | `network` | Last HTTP status or negative client error |
| `http_json(path)` | String | `network` | Scalar from the last response using a JSON path |
| `http_text(offset)` | String | `network` | Up to 255 bytes from a byte offset |
| `url_encode(text)` | String | `network` | Encode a URL query value |
| `date_weekday(date)` | String | — | ISO date YYYY-MM-DD → Sun…Sat |
| `file_read(path)` | String | `storage.read` | Up to 255 bytes; empty for missing/oversized files |
| `file_write(path, data)` | Boolean | `storage.write` | Replace file; parent directory must exist |
| `audio_play(path)` | Boolean | `audio + scope` | Local file; additional read grant outside the app |
| `wifi_status()` | Boolean | `network` | Connection status; does not start Wi-Fi |
| `system.restart()` | nil | `system.restart` | Request restart after the current VM timeslice |
| `timer.start(milliseconds)` | Boolean | — | Recurring app timer; integer 100–86400000 ms |
| `Serial.printf(text)` | nil | — | Exactly one string; no additional printf-format arguments |
| `text(value)` | String | — | Convert a scalar to text |
| `number_parse(text)` | Float | — | Parse a number; decimal point/comma |
| `round(number)` | Integer | — | Round; halfway cases away from zero |
| `string_length(text)` | Integer | — | Byte length, not a Unicode character count |
| `string_slice(text, start, length)` | String | — | Byte slice; zero-based start |
| `math_eval(expression)` | String | — | Evaluate a math expression; result as text |
| `string_last_index(text, needle)` | Integer | — | Last zero-based byte index; -1 if absent |
| `file_list(path)` | String | `storage.read` | Direct entries separated by newlines; omit /system |
| `event_x()` | Integer | — | Local X coordinate of the pointer event |
| `event_y()` | Integer | — | Local Y coordinate of the pointer event |
| `event_screen_x()` | Integer | — | X coordinate on the logical display |
| `event_screen_y()` | Integer | — | Y coordinate on the logical display |
| `event_pressed()` | Boolean | — | Pressed state in the pointer snapshot |
| `audio_stream(url)` | Boolean | `audio + network` | HTTP/HTTPS stream; false when offline/busy/start fails |
| `audio_stop()` | Boolean | `audio` | Stop audio owned by the active fApp |
| `audio_set_volume(value)` | Boolean | `audio` | Shared engine volume; integer 0–21 |
| `audio_volume()` | Integer | `audio` | Current engine volume |
| `audio_playing()` | Boolean | `audio` | Playback owned by the fApp; false after EOF |
| `audio_position()` | Integer | `audio` | Local position in seconds; otherwise 0 |
| `audio_duration()` | Integer | `audio` | Local duration in seconds; 0 if unknown/inactive/stream |
| `audio_seek(seconds)` | Boolean | `audio` | Integer 0–65535 seconds; active local file only |
| `file_pair_value(path, separator, index)` | String | `storage.read` | Value of valid name/value row at index 0+; one-character separator |

`scope` means the folder grants in section 3.2. Pointer helpers are explained in section 6. Missing capabilities cause `permission denied` even where the native API could otherwise return `false`.

### 8.2 HTTP and JSON

```fscript
var ok = http_get("https://api.example.org/data.json")

if ok then
  var temperature = http_json("current.temperature")
  lbl_status.text = "Temperature: " + temperature
else
  lbl_status.text = "HTTP error: " + http_status()
end
```

`http_get()` accepts `http://` and `https://`, retains the latest response and
returns `true` only for HTTP 200 through 299. `http_status()` returns the latest
HTTP status or a negative client error. Connection and read timeouts are 5 and
7 seconds. Responses are limited to 32768 bytes. While OTA owns the network,
`http_get()` returns a contained `false` result.

`http_json()` reads scalar values using dot and array paths:

```fscript
var city = http_json("results[0].name")
var first_day = http_json("daily.time[0]")
```

Strings, numbers, booleans and `null` are returned as fScript Strings. A missing
path returns `""`. Objects and arrays cannot be returned directly. JSON Unicode
escapes in the form `\uXXXX` currently become `?`.

`http_text(offset)` returns up to 255 bytes starting at a byte offset:

```fscript
var first = http_text(0)
var next = http_text(255)
```

`url_encode()` prepares a value for a URL query:

```fscript
var place = url_encode("Nova Gorica")
var ok = http_get("https://example.org/search?q=" + place)
```

HTTPS currently uses compatibility mode without certificate-chain validation.
HTTP is synchronous, so a request can pause the app until its bounded timeout.

### 8.3 Date

```fscript
var day = date_weekday("2026-08-04")  // "Tue"
```

The accepted form is `YYYY-MM-DD`, beginning with year 1970. The result is
`Sun`, `Mon`, `Tue`, `Wed`, `Thu`, `Fri` or `Sat`.

### 8.4 Files

```fscript
var old_state = file_read("data/state.txt")
var saved = file_write("data/state.txt", "counter=" + counter)
```

Paths can be absolute from the SD root or relative to the active app directory.
Folder grants in `app.json` use `storage.read:/folder` and `storage.write:/folder`.
Each grant covers that folder and its descendants. `/` grants the whole card,
except `/system`, which is always protected from reading and writing. Legacy
`storage.read` and `storage.write` grant only the active app directory.
Paths are normalized before permission checks; protected paths cannot be
accessed through `..` or different letter casing. Backslashes, control characters
and ambiguous trailing dots/spaces are rejected. `file_write()` requires
existing parent directories.

`file_read()` reads at most 255 bytes. A missing or oversized file returns `""`,
which is indistinguishable from a genuinely empty file. `file_write()` replaces
the file contents and returns success.
`file_list()` returns direct child entries separated by newlines and appends
`/` to directory names. Text editors can use `textarea.load_file(path)` and
`textarea.save_file(path)` to bypass the 255-byte VM string limit. Both methods
use the same folder grants. `save_file()` can create parent directories within
its granted write area. `file_list("/")` omits the protected `system` entry.

### 8.5 Audio, Wi-Fi, restart, timer and serial output

```fscript
var playing = audio_play("sounds/notify.mp3")
var online = wifi_status()
var timer_ok = timer.start(10000)
Serial.printf("Online: " + online + "\n")
```

`audio_play()` plays an existing file through the existing fOS audio engine.
The `audio` permission retains app-local playback; other SD folders also require
a matching `storage.read:/folder` grant. `/system` remains inaccessible. Previous playback is stopped; closing the app stops
its playback. New playback does not begin while OTA is active.

`wifi_status()` only observes the current state; it does not initiate a
connection. `timer.start()` sets the recurring `app.timer` interval from 100 to
86400000 milliseconds. The default remains 60000 milliseconds if it is never
called.

`system.restart()` schedules a restart after the current VM time slice.
`Serial.printf()` takes exactly one pre-composed String. Percent characters are
printed safely as data:

```fscript
Serial.printf("Progress: 100%\n")
```

Native C formatting such as `Serial.printf("%d", value)` is intentionally not
supported.

### 8.6 Text, number and math helpers

```fscript
var value = number_parse("12,5")
lbl_result.text = "Value: " + text(value)
lbl_round.text = "Rounded: " + round(value)

var short_name = string_slice("Calculator", 0, 4)
var comma_pos = string_last_index("1,2+3,4", ",")
lbl_result.text = math_eval("2+3*4")
```

`text()` converts a scalar value to deterministic text. `number_parse()`
accepts decimal points and decimal commas; invalid text returns `0.0`.
`string_length()` counts bytes, `string_slice()` returns a bounded substring and
`string_last_index()` returns the final index or `-1`. `math_eval()` evaluates
`+`, `-`, `*` and `/` with operator precedence, accepts comma or point decimals
and returns the text `Math Error` for malformed expressions or division by
zero.


### 8.7 Audio streams and bounded configuration files

`audio_stream(url)` requires both `audio` and `network`. It accepts HTTP/HTTPS;
offline/OTA/start failures return false. Local `audio_play()` uses folder scope.
Closing the app stops its playback. `audio_stop()` ends playback rather than
pausing for a resume. The volume is shared with the native engine, range 0–21.
Position/duration use seconds; duration may initially be unknown (0). Seeking
works only for an active local file, with integer seconds 0–65535. Streams do
not expose a seek position. Audio shares the underlying engine with native fOS.

`file_pair_value(path, separator, index)` reads a value from the same validated
rows used by `roller.load_pairs()`. Indices start at 0. Missing files/rows return
an empty string; denied native paths raise an error before SD access. Pair files
need a matching `storage.read` grant. For concrete Radio usage see
[FSCRIPT_RADIO_AUDIO.md](FSCRIPT_RADIO_AUDIO.md).

## 9. Every `fosc` command

### `fosc --help`

```bash
./build/fosc --help
```

Prints all commands and options.

### `fosc --version`

```bash
./build/fosc --version
```

Prints compiler and language versions.

### `fosc lex`

```bash
./build/fosc lex main.fscript
```

Runs only the lexer and prints token kind, line, column, lexeme and decoded
literal value. Use it to diagnose invalid characters, strings and comments. It
does not create an output file.

### `fosc parse`

```bash
./build/fosc parse main.fscript
```

Runs lexer and parser and prints the abstract syntax tree (AST). It does not yet
perform semantic UI or type validation.

### `fosc check`

```bash
./build/fosc check main.fscript
./build/fosc check main.fscript --ui layout.ui
```

Checks lexical syntax, grammar, variables, functions, types, UI objects,
properties, methods, events and native API signatures without producing a
`.fapp`. Without `--ui`, fosc looks beside the script for `main.ui` first, then
`layout.ui`. A UI file is optional if the program references no UI objects.

### `fosc build`

```bash
./build/fosc build main.fscript
./build/fosc build main.fscript --ui layout.ui -o main.fapp
./build/fosc build main.fscript --output main.fapp
./build/fosc build main.fscript --no-optimize -o debug.fapp
```

The full pipeline is:

1. Read the source file.
2. Produce tokens.
3. Parse an AST.
4. Resolve UI names to deterministic numeric IDs.
5. Validate semantics and types.
6. Fold side-effect-free constant expressions by default.
7. Generate bytecode and metadata tables.
8. Write the fAPP header, source hash and CRC-32.
9. Read and structurally verify the generated image again.
10. Write the destination only after every check succeeds.

Without `-o`, the source extension is replaced with `.fapp`. `--no-optimize`
only disables conservative constant folding; runtime behavior should remain the
same.

### `fosc verify`

```bash
./build/fosc verify main.fapp
```

Checks magic, versions, size, ranges, CRC, tables, indices, opcodes, jump
targets, stack flow, function endings and declared maximum stack. It executes
nothing.

### `fosc disasm`

```bash
./build/fosc disasm main.fapp
```

Verifies the image first, then prints the header, constants, functions, event
bindings and human-readable bytecode. This helps confirm that an `http_get` or
UI operation was actually compiled.

### Build-system commands

```bash
make             # build fosc and fosrun
make test        # run all host and runtime regression tests
make clean       # remove the build directory
```

CMake alternative:

```bash
cmake -S . -B build-cmake
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
```

## 9.1 Every `fosrun` command

`fosrun` is a PC-side runner for already compiled `.fapp` files. It uses the
same VM as fOS, but simulates UI objects and native fOS APIs. This makes app
logic quick to test without reflashing the ESP32. Starting with version 0.2.1,
`fosrun` can also load the `.ui` in a local browser surface for clickable tests.

```bash
./build/fosrun main.fapp --ui layout.ui
```

`app.start` runs automatically by default. Afterwards `fosrun` prints the final
state of every named UI object.

Load the UI in a browser:

```bash
./build/fosrun main.fapp --ui layout.ui --serve --open
```

Without `--open`, open this address manually:

```text
http://127.0.0.1:8765/
```

Use another port:

```bash
./build/fosrun main.fapp --ui layout.ui --serve --port 9876
```

Trigger an event:

```bash
./build/fosrun main.fapp --ui layout.ui --event btn_ok.click
./build/fosrun main.fapp --ui layout.ui --event app.timer
./build/fosrun main.fapp --ui layout.ui --event keyboard_search.ready
```

Set a simulated UI value before an event:

```bash
./build/fosrun main.fapp \
  --ui layout.ui \
  --set txt_search.text=Berlin \
  --event keyboard_search.ready
```

Override simulated `http_json()` responses:

```bash
./build/fosrun weather.fapp \
  --ui layout.ui \
  --json current.temperature_2m=19 \
  --json current.relative_humidity_2m=61
```

Additional options:

| Option | Effect |
| --- | --- |
| `--ui <file>` | Layout file (required) |
| `--size WIDTHxHEIGHT` | Preview size; default 800x480, each dimension 64–4096 |
| `--sd-root <directory>` | Read-only SD image access; audio remains simulated |
| `--no-start` | Do not run `app.start` automatically |
| `--no-dump` | Do not print the final UI state |
| `--trace-native` | Print simulated native API calls |
| `--wifi-off` | Make `wifi_status()` return `false` |
| `--serve` | Start the local browser UI |
| `--port <number>` | Port for `--serve`, default `8765` |
| `--open` | Open the browser automatically after startup |

Limitation: Browser mode is not a pixel-perfect LVGL emulator. It maps `.ui`
elements to HTML controls and checks the logic behind controls, event bindings
and UI/native API access.

## 10. How the compiler works

The lexer converts characters into tokens while retaining file, line, column
and byte offset. The recursive-descent parser builds a typed AST and attempts
to recover after syntax errors so it can report more than one issue. The
semantic pass first collects global symbols and function signatures, then
analyzes initialization, functions and events. This permits forward calls.

The code generator assigns numeric slots to globals, locals, functions and UI
objects. UI source names are not stored in `main.fapp`. String constants are
deduplicated; Integer and Float literals are embedded in instructions. The
optimizer evaluates safe constant expressions on the PC. Finally, the bytecode
verifier proves properties such as jumps landing only on instruction boundaries
and control-flow paths joining with consistent stack depth.

Compiler exit codes:

| Code | Meaning |
| ---: | --- |
| 0 | Command succeeded |
| 1 | Source, validation, format or build error |
| 2 | Missing or invalid `check`/`build` command-line option |

## 11. fAPP image and bytecode

The fAPP 1.0 format is little-endian and starts with a fixed 80-byte header. It
contains the `FAPP` magic, format and bytecode versions, total size, CRC-32,
constant/function/event/code sections, source hash and minimum fOS version.
fosc emits bytecode 1.8; fOS 4.1.0 continues to accept compatible older minor
versions.

| Offset | Size | Header field |
| ---: | ---: | --- |
| 0 | 4 | `FAPP` magic |
| 4 | 2 | fAPP format major/minor |
| 6 | 2 | bytecode major/minor |
| 8 | 2 | header size, currently 80 |
| 10 | 2 | flags |
| 12 | 4 | complete file size |
| 16 | 4 | CRC-32; treated as zero during calculation |
| 20 | 8 | constant section offset and size |
| 28 | 2 | constant count |
| 30 | 2 | global count |
| 32 | 8 | function table offset and size |
| 40 | 2 | function count |
| 42 | 2 | initialization function index |
| 44 | 8 | event table offset and size |
| 52 | 2 | event count |
| 56 | 8 | code offset and size |
| 64 | 4 | FNV-1a source hash |
| 68 | 6 | minimum fOS version |
| 74 | 6 | reserved |

### 11.1 Instruction set

| Group | Opcodes |
| --- | --- |
| Values | `PUSH_NIL`, `PUSH_FALSE`, `PUSH_TRUE`, `PUSH_INT`, `PUSH_FLOAT`, `PUSH_STRING` |
| Stack | `NOP`, `POP`, `DUP` |
| Variables | `LOAD_GLOBAL`, `STORE_GLOBAL`, `LOAD_LOCAL`, `STORE_LOCAL` |
| UI | `GET_UI_PROPERTY`, `SET_UI_PROPERTY`, `CALL_UI_METHOD` |
| Calls | `CALL_FUNCTION`, `CALL_NATIVE`, `RETURN` |
| Unary | `NEGATE`, `NOT` |
| Arithmetic | `ADD`, `SUBTRACT`, `MULTIPLY`, `DIVIDE`, `MODULO` |
| Comparison | `EQUAL`, `NOT_EQUAL`, `LESS`, `LESS_EQUAL`, `GREATER`, `GREATER_EQUAL` |
| Logic | `AND`, `OR` |
| Control | `JUMP`, `JUMP_IF_FALSE` |

Native function IDs were extended through bytecode 1.8 without renumbering existing entries. The complete current table follows:

| ID | Native function |
| ---: | --- |
| 1 | `http_get` |
| 2 | `http_status` |
| 3 | `http_json` |
| 4 | `http_text` |
| 5 | `url_encode` |
| 6 | `date_weekday` |
| 7 | `file_read` |
| 8 | `file_write` |
| 9 | `audio_play` |
| 10 | `wifi_status` |
| 11 | `system.restart` |
| 12 | `timer.start` |
| 13 | `Serial.printf` |
| 14 | `text` |
| 15 | `number_parse` |
| 16 | `round` |
| 17 | `string_length` |
| 18 | `string_slice` |
| 19 | `math_eval` |
| 20 | `string_last_index` |
| 21 | `file_list` |
| 22 | `event_x` |
| 23 | `event_y` |
| 24 | `event_screen_x` |
| 25 | `event_screen_y` |
| 26 | `event_pressed` |
| 27 | `audio_stream` |
| 28 | `audio_stop` |
| 29 | `audio_set_volume` |
| 30 | `audio_volume` |
| 31 | `audio_playing` |
| 32 | `audio_position` |
| 33 | `audio_duration` |
| 34 | `audio_seek` |
| 35 | `file_pair_value` |

## 12. The VM system in fOS

### 12.1 Load path

1. fOS reads `app.cfg` as a fallback and then authoritative `app.json` data.
2. It creates `layout.ui` and fills the registry with numeric object IDs.
3. `FAppLoader` validates paths, header, version, section ranges, minimum fOS
   version and CRC in a streaming 256-byte buffer.
4. The VM loads metadata and allocates value storage in PSRAM when available,
   falling back to internal RAM.
5. Top-level code runs as the initialization function.
6. `app.start` is delivered and the UI event bridge is attached.

### 12.2 States and lifecycle

| State | Meaning |
| --- | --- |
| `Stopped` | no active app; runtime storage released |
| `Ready` | loaded and waiting for an event |
| `Running` | executing a function or event |
| `Faulted` | app terminated by a contained fault |

The main loop normally grants the VM at most 256 instructions or 4000
microseconds per update. fOS then regains control of LVGL, audio, Wi-Fi and OTA.
Events enter an eight-entry FIFO queue. Full queues drop events; pointer moves can coalesce and pointer-up can replace a pending move as described in section 6.

Theme changes preserve globals and execution state. When closing, `app.close`
receives at most 512 instructions or 5000 microseconds. The runtime values,
HTTP response, registry and app LVGL objects are then released. A runtime fault
clears call/event state and displays an error inside the app.

### 12.3 Fixed resource limits

| Resource | Limit |
| --- | ---: |
| named UI objects | 64 |
| String constants | 128 |
| String payload | 255 bytes |
| functions including init/events | 48 |
| events | 64 |
| globals | 32 |
| operand stack | 64 values |
| local slots across active calls | 96 |
| call depth | 8 |
| event queue | 8 |
| `layout.ui` | 32000 characters |
| `app.json` | 4096 bytes |
| HTTP response | 32768 bytes |

| Resource | Limit |
| --- | --- |
| Installed apps / home slots | 40 / 7 |
| Keyboard overlays | 8 |
| Folder grants / path storage | 8 / 512 bytes |
| Canvas buffer | 2 × width × height bytes; PSRAM |
| Canvas maximum | 800×480 CrowPanel; 1024×600 P4 |
| Slider range | -32768…32767 |
| Textarea load_file | 4000 bytes + truncation marker |
| Roller directory options | 16384 bytes |
| Name/value configuration | 16384 bytes; 40 valid rows |
| Executable cache | 65536 bytes; larger executable falls back to file reads |


### 12.4 VM errors

| Message | Typical cause |
| --- | --- |
| `executable not prepared` | loader has no valid fAPP |
| `runtime resource limit exceeded` | stack, locals, depth, String or metadata too large |
| `executable read failed` | SD/file read error |
| `invalid executable metadata` | inconsistent tables or argument count |
| `invalid bytecode instruction` | unknown opcode or invalid jump target |
| `operand stack underflow/overflow` | damaged or invalid bytecode |
| `runtime type error` | dynamic value is invalid for an operation |
| `division by zero` | `/` or `%` with zero |
| `invalid runtime index` | invalid global/local/function index |
| `permission denied` | manifest permission is missing |
| `UI operation failed` | property/method does not match the LVGL object |
| `native API unavailable`/API text | API unavailable or input invalid |

## 13. `fosc` diagnostic codes

| Range | Meaning |
| --- | --- |
| `FS000` | file cannot be read |
| `FS001–FS005` | lexer: character, String, escape, comment, exponent |
| `FS201–FS205` | parser: token, missing syntax, target, count, event syntax |
| `FS301–FS303` | invalid/duplicate UI ID or more than 64 IDs |
| `FS304–FS330` | code generation: symbols, limits, operators, UI/API calls |
| `FS401–FS405` | writing the fAPP format |
| `FS411–FS419` | reading, version, CRC, sections and tables |
| `FS420–FS422` | CLI option, output and internal build verification |
| `FS501–FS520` | semantics: names, types, functions, events, UI and native APIs |
| `FS601–FS608` | bytecode: opcode, index, arity, jump, stack and control flow |

Diagnostics include file, line and column. Fix the earliest error first;
subsequent messages may be consequences of it.

## 14. Complete small example app

This complete example is also included under `docs/examples/counter`. Run the shell commands below in `fScript_Development_Environment/fosc`.

`layout.ui`:

```text
type=label;id=lbl_status;x=20%;y=18%;w=75%;h=12%;text=Count: 0;font=24;fg=0xFFFFFF
type=button;id=btn_add;x=20%;y=40%;w=30%;h=15%;text=+1;bg=theme
type=button;id=btn_reset;x=60%;y=40%;w=30%;h=15%;text=Reset;bg=theme
```

`main.fscript`:

```fscript
var counter = 0

function show_counter()
  lbl_status.text = "Count: " + counter
end

on app.start
  show_counter()
  timer.start(10000)
end

on btn_add.click
  counter = counter + 1
  show_counter()
end

on btn_reset.click
  counter = 0
  show_counter()
end

on app.timer
  Serial.printf("Count: " + counter + "\n")
end
```

`app.json`:

```json
{
  "id": "org.example.counter",
  "name": "Counter",
  "version": "1.0.0",
  "min_fos": "4.1.0",
  "type": "ui",
  "icon": "+1",
  "layout": "layout.ui",
  "executable": "main.fapp",
  "scrollable": false,
  "permissions": [
    "ui"
  ]
}
```

```sh
./build/fosc check ../../docs/examples/counter/main.fscript --ui ../../docs/examples/counter/layout.ui
./build/fosc build ../../docs/examples/counter/main.fscript --ui ../../docs/examples/counter/layout.ui -o ../../docs/examples/counter/main.fapp
./build/fosc verify ../../docs/examples/counter/main.fapp
./build/fosrun ../../docs/examples/counter/main.fapp --ui ../../docs/examples/counter/layout.ui --size 800x480 --event btn_add.click
./build/fosrun ../../docs/examples/counter/main.fapp --ui ../../docs/examples/counter/layout.ui --size 1024x600 --event btn_add.click
```

Expected label text: `Count: 1`. Copy `counter` with its generated `main.fapp` to `/apps/counter`. Geometry-only changes without changing named-object order do not require rebuilding bytecode; script/ID changes do.

## 15. Current limitations

fScript has no arrays, maps, classes, imports/modules, threads or exception
handling. HTTP remains synchronous and can delay the UI until timeout; the VM
budget cannot interrupt a native call already in progress. String ordering
with `< <= > >=` is not reliably implemented; compare numbers instead. UI
coordinates/dimensions are not exposed as script properties. Browser previews
simulate native APIs, do not play audio and are not pixel-exact LVGL emulation.
Verify physical touch, networking, codecs and device behavior on the board.

The fScript APIs are the same on both boards. P4 USB mass storage, RTC/VBAT and
BOOT battery sleep are firmware features, not exposed fScript commands. While
USB owns the P4 SD card, fOS pauses local apps/file work and app timers.
See [WAVESHARE.md](WAVESHARE.md).

The API reference and tables have been checked against the bundled compiler
and runtime sources. Further guides: [documentation index](INDEX.md).


### Reference sources for this revision

- [Compiler API IDs and signatures](../fScript_Development_Environment/fosc/bytecode/Bytecode.cpp)
- [UI semantic checks](../fScript_Development_Environment/fosc/semantic/SemanticAnalyzer.cpp)
- [Runtime limits and queue](../fOS_Waveshare_P4/src/fapp/FAppRuntime.h)
- [UI adapter](../fOS_Waveshare_P4/src/fapp/LvglFAppUiApi.cpp)
- [Folder policy](../fOS_Waveshare_P4/src/fapp/FAppStoragePolicy.h)
- [Copyable counter example](examples/counter/main.fscript)

The CrowPanel uses the same script-facing API with LVGL 8 rather than LVGL 9.
The main sketch supplies layout creation and board-specific geometry limits.
