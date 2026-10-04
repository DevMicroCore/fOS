# fScript, fosc and VM Manual

Applies to: fOS 4.0.0, fScript 1.0.0, fosc 0.12.1, fosrun 0.2.1, fAPP 1.0, bytecode 1.5

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
cd ~/Downloads/fOS4.0/fScript_Development_Environment/fosc
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
fosc 0.12.1
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

fOS currently adds at most seven direct subdirectories of `/apps` to the
launcher.

### 3.1 `app.json`

```json
{
  "id": "org.example.my-app",
  "name": "My App",
  "version": "1.0.0",
  "min_fos": "4.0.0",
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

| Permission | Operations enabled |
| --- | --- |
| `ui` | Read/change UI and invoke UI methods |
| `storage.read` | `file_read()`, `file_list()` |
| `storage.write` | `file_write()` |
| `network` | HTTP/JSON, URL encoding and `wifi_status()` |
| `audio` | `audio_play()` |
| `system.restart` | `system.restart()` |

`date_weekday()`, `timer.start()` and `Serial.printf()` need no additional
permission. A missing permission causes a contained VM `permission denied`
fault; fOS continues running.

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

Each non-empty line describes one object. Fields are separated with semicolons;
lines starting with `#` are comments.

```text
type=button;id=btn_ok;x=40;y=100;w=240;h=70;text=OK;bg=theme;fg=contrast
```

### 4.1 Supported objects

| `type` | Purpose |
| --- | --- |
| `label` | Text display |
| `button` | Button with a text child |
| `textarea` | Single- or multi-line text input |
| `switch` | On/off switch |
| `checkbox` | Check box with text |
| `panel` | Container for child objects |
| `roller` | Scrolling selection list |
| `dropdown` | Drop-down selection list |
| `keyboard` | On-screen keyboard for a textarea |

Only objects with an `id=` are accessible from fScript. IDs must be valid,
unique fScript identifiers. Supported named objects receive numeric IDs 1 to
64 in file order. Do not reorder them after compilation without rebuilding
`main.fapp`.

### 4.2 Common UI fields

| Field | Meaning / values |
| --- | --- |
| `type` | Object type |
| `id` | fScript name; optional for visual-only objects |
| `x`, `y` | Position; defaults to 0 |
| `w`, `h` | Width/height; defaults to 220/50 |
| `text` | Text or options; `\n` and `\t` are decoded |
| `bg`, `fg` | Hex color such as `0x2196F3` |
| `bg`, `fg` | also `theme`, `accent`, `surface`, `contrast`, `text` |
| `font` | currently 20, 24 or 40 |
| `align` | `left`, `center`, `right` |
| `parent` | ID of a previously declared panel |
| `radius` | Corner radius |
| `border` | Border width |
| `clickable` | `true/false`, `1/0`, `yes/no`, `on/off` |
| `scrollable` | Object scrolling flag |
| `hidden` | Initially hidden |

Object-specific fields:

| Object | Additional fields |
| --- | --- |
| `textarea` | `placeholder`, `one_line`, `max_length` (up to 1024) |
| `switch`, `checkbox` | `value=true` to start checked |
| `panel` | `styleless=true` removes the base LVGL style |
| `keyboard` | `target=textarea_id`; target must already exist |
| `roller`, `dropdown` | Options in `text`, separated by `\n` |

A keyboard with `target=` is shown automatically and moved to the foreground
when its target textarea is touched or focused. Scripts can still control the
keyboard explicitly with `keyboard_id.hidden`.

The main presentation fields currently apply as follows:

| Object | Effective presentation fields |
| --- | --- |
| `label` | `x`, `y`, `w`, `text`, `fg`, `font`, `align` |
| `button` | `x`, `y`, `w`, `h`, `text`, `bg`, `fg`, `font` |
| `textarea` | `x`, `y`, `w`, `h`, `text`, `bg`, `fg`, `font` plus input fields |
| `switch` | `x`, `y`, `value`; size and colors follow the fOS style |
| `checkbox` | `x`, `y`, `text`, `fg`, `value` |
| `panel` | `x`, `y`, `w`, `h`, `bg`, `styleless` |
| `roller`, `dropdown` | `x`, `y`, `w`, `h`, `text`, `bg`, `fg`, `font` |
| `keyboard` | `x`, `y`, `w`, `h`, `font`, `target` |

`id`, `parent`, `radius`, `border`, `clickable`, `scrollable` and `hidden` are
handled generally after object creation. Fields that do not apply to an object
type are ignored.

Search panel and keyboard example:

```text
type=panel;id=panel_search;x=10;y=10;w=760;h=420;bg=0x101418
type=textarea;id=txt_search;parent=panel_search;x=15;y=15;w=570;h=55;placeholder=Search place;one_line=true
type=button;id=btn_search;parent=panel_search;x=600;y=15;w=140;h=55;text=Search;bg=theme
type=keyboard;id=keyboard_search;parent=panel_search;target=txt_search;x=0;y=190;w=740;h=220
```

The renderer resolves `parent` and `target` only among named objects already
created above the current line.

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

Syntax:

```fscript
on object_id.event
  // statements
end
```

### 6.1 System events

| Event | Delivery |
| --- | --- |
| `app.start` | After top-level initialization |
| `app.close` | On close, with a final bounded execution window |
| `app.theme_changed` | After an fOS theme change, without VM reload |
| `app.timer` | Recurring; defaults to every 60 seconds |

```fscript
on app.start
  timer.start(5000)
end

on app.timer
  Serial.printf("Timer fired\n")
end
```

### 6.2 UI events

| Event | Suitable objects |
| --- | --- |
| `click` or `clicked` | all named UI objects |
| `change` or `changed` | textarea, switch, checkbox, roller, dropdown |
| `value_changed` | textarea, switch, checkbox, roller, dropdown |
| `pressed` | button, switch, checkbox |
| `released` | button, switch, checkbox |
| `ready` | keyboard, for example the checkmark key |
| `cancel` or `cancelled` | keyboard cancel key |

One LVGL value change queues both `value_changed` and `changed` when handlers
exist. Only one handler may exist for each object/event pair.

## 7. Accessing the UI from fScript

Every UI operation needs the `ui` permission.

### 7.1 Properties

| Property | Type | Objects | Example |
| --- | --- | --- | --- |
| `.text` | String | label, button, textarea, checkbox, roller, dropdown | `lbl.text = "OK"` |
| `.value` | Boolean | switch, checkbox | `toggle.value = true` |
| `.value` | Integer | roller, dropdown | `places.value = 2` |
| `.checked` | Boolean | switch, checkbox | `box.checked = false` |
| `.hidden` | Boolean | all | `panel.hidden = true` |
| `.enabled` | Boolean | all | `btn.enabled = false` |

Reading and writing use the same member notation:

```fscript
var selected = places.value
var query = txt_search.text
lbl_result.text = query
```

Use `.text` for textarea content. The current runtime does not implement
textarea content through `.value`.

For `roller.text` and `dropdown.text`, writing replaces the complete newline-
separated option list; reading returns the selected option.

### 7.2 Methods

The file methods take one relative path; all other UI methods take no arguments:

| Method | Objects | Effect |
| --- | --- | --- |
| `.clear()` | textarea | Clear text |
| `.focus()` | button, textarea, switch, checkbox, roller, dropdown | Set LVGL focus and bring the object forward visibly |
| `.blur()` | same objects | Remove focus |
| `.scroll_to_top()` | textarea, panel, roller | Scroll to top |
| `.scroll_to_bottom()` | textarea, panel, roller | Scroll to bottom |
| `.load_file(path)` | textarea | Load up to 4000 bytes directly from an app file |
| `.save_file(path)` | textarea | Save its text directly to an app file |

```fscript
on btn_clear.click
  txt_input.clear()
  txt_input.focus()
end
```

## 8. Native fOS APIs

### 8.1 Complete list

| Function | Result | Permission |
| --- | --- | --- |
| `http_get(url)` | Boolean | `network` |
| `http_status()` | Integer | `network` |
| `http_json(path)` | String | `network` |
| `http_text(offset)` | String | `network` |
| `url_encode(text)` | String | `network` |
| `date_weekday(date)` | String | none |
| `file_read(path)` | String | `storage.read` |
| `file_write(path, data)` | Boolean | `storage.write` |
| `file_list(path)` | String | `storage.read` |
| `audio_play(path)` | Boolean | `audio` |
| `wifi_status()` | Boolean | `network` |
| `system.restart()` | nil | `system.restart` |
| `timer.start(milliseconds)` | Boolean | none |
| `Serial.printf(text)` | nil | none |
| `text(value)` | String | none |
| `number_parse(text)` | Float | none |
| `round(number)` | Integer | none |
| `string_length(text)` | Integer | none |
| `string_slice(text, start, length)` | String | none |
| `string_last_index(text, needle)` | Integer | none |
| `math_eval(expression)` | String | none |

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

Paths are relative to the active app directory. Subdirectories are allowed but
must already exist. Absolute paths, backslashes, empty segments, `.` and `..`
are rejected, preventing access to fOS or another app's files.

`file_read()` reads at most 255 bytes. A missing or oversized file returns `""`,
which is indistinguishable from a genuinely empty file. `file_write()` replaces
the file contents and returns success.
`file_list()` returns direct child entries separated by newlines and appends
`/` to directory names. Text editors can use `textarea.load_file(path)` and
`textarea.save_file(path)` to bypass the 255-byte VM string limit. Both methods
remain confined to the active app directory.

### 8.5 Audio, Wi-Fi, restart, timer and serial output

```fscript
var playing = audio_play("sounds/notify.mp3")
var online = wifi_status()
var timer_ok = timer.start(10000)
Serial.printf("Online: " + online + "\n")
```

`audio_play()` plays an existing file from the app directory through the
existing fOS audio engine. Previous playback is stopped; closing the app stops
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
fosc emits bytecode 1.5; fOS 4.0.0 continues to accept compatible older minor
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

Native function IDs 1 through 13 remain unchanged in bytecode 1.5, so existing
HTTP, weather and system API apps remain binary-compatible.

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
Events enter an eight-entry FIFO queue. Further events are dropped when the
queue is full rather than allocating unbounded memory.

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

`layout.ui`:

```text
type=label;id=lbl_status;x=20;y=20;w=500;h=50;text=Ready;fg=0xFFFFFF;font=24
type=button;id=btn_add;x=20;y=100;w=240;h=70;text=Increase;bg=theme;fg=contrast
type=button;id=btn_reset;x=280;y=100;w=240;h=70;text=Reset;bg=0x555555;fg=0xFFFFFF
```

`main.fscript`:

```fscript
var counter = 0

function show_counter()
  lbl_status.text = "Counter: " + counter
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
  Serial.printf("Current counter: " + counter + "\n")
end
```

`app.json`:

```json
{
  "id": "org.example.counter",
  "name": "Counter",
  "version": "1.0.0",
  "min_fos": "4.0.0",
  "type": "ui",
  "icon": "+1",
  "layout": "layout.ui",
  "executable": "main.fapp",
  "scrollable": false,
  "permissions": ["ui"]
}
```

Build it:

```bash
./build/fosc check /path/counter/main.fscript --ui /path/counter/layout.ui
./build/fosc build /path/counter/main.fscript --ui /path/counter/layout.ui \
  -o /path/counter/main.fapp
./build/fosc verify /path/counter/main.fapp
```

Copy the complete `counter` folder to `/apps/counter` on the SD card and reopen
the fOS app launcher.

## 15. Current limitations

fScript 1.0.0 currently has no arrays, maps, classes, imports, modules, threads,
arbitrary system access or exception handling. Native HTTP calls are
synchronous. UI and runtime resources use fixed limits for predictable memory
use. The source file is not needed to execute an app on fOS, but should remain
in the app project for maintenance and recompilation.

Rebuild and verify `main.fapp` whenever named UI elements are reordered, the
script changes, or the compiler/bytecode version changes.
