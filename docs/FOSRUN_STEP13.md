# Step 13: fosrun PC Runner

`fosrun` is a PC-side runner for compiled `.fapp` files. It uses the same
bounded `FAppRuntime` implementation as fOS, but replaces LVGL and native fOS
services with deterministic PC stubs.

The tool is intended for fast logic tests before copying an app to the ESP32:

* run `app.start`
* set simulated UI properties
* queue UI and system events
* inspect final UI state
* load the `.ui` in a local browser test surface
* click buttons, edit textareas, select dropdowns and use keyboard controls
* trace simulated native API calls
* override `http_json()` values for repeatable tests

The browser mode is a practical UI runner, not a pixel-perfect LVGL emulator.
It maps the fOS `.ui` fields to HTML controls and executes the same `.fapp`
runtime behind them.

## Build

```bash
cd fScript_Development_Environment/fosc
make
make test
```

The default build now creates both tools:

```text
build/fosc
build/fosrun
```

## Basic Run

Build a `.fapp` first:

```bash
./build/fosc build examples/fosrun_demo.fscript \
  --ui examples/fosrun_demo.ui \
  -o build/fosrun_demo.fapp
```

Run it:

```bash
./build/fosrun build/fosrun_demo.fapp --ui examples/fosrun_demo.ui
```

`app.start` runs automatically. The final UI dump shows every named object:

```text
fosrun completed
Instructions: 8
UI state
  #1 lbl_status (label) text="Ready" value=0 hidden=false enabled=true
  #2 btn_add (button) text="Add" value=0 hidden=false enabled=true
```

## Browser UI Mode

Start the local UI server:

```bash
./build/fosrun build/fosrun_demo.fapp \
  --ui examples/fosrun_demo.ui \
  --serve --open
```

Without `--open`, open this URL manually:

```text
http://127.0.0.1:8765/
```

Use another port if needed:

```bash
./build/fosrun build/fosrun_demo.fapp \
  --ui examples/fosrun_demo.ui \
  --serve --port 9876
```

The browser runner supports these layout objects:

| `.ui` type | Browser control |
| --- | --- |
| `label` | Text label |
| `button` | Clickable button, sends `.click` |
| `textarea` | Editable text field |
| `dropdown` | Select box, sends `.changed` |
| `roller` | Multi-row select box, sends `.changed` |
| `switch` | Checkbox-style toggle, sends `.changed` |
| `checkbox` | Checkbox, sends `.changed` |
| `panel` | Positioned container |
| `keyboard` | On-screen keyboard, sends `.ready` on Enter |

Supported visual fields include `x`, `y`, `w`, `h`, `font`, `radius`, `bg`,
`fg`, `hidden`, `parent`, `placeholder` and `target`. Child elements with
`parent=panel_id` are rendered inside the parent panel, so full-screen search
layers and their keyboards can be tested.

## Trigger Events

```bash
./build/fosrun build/fosrun_demo.fapp \
  --ui examples/fosrun_demo.ui \
  --event btn_add.click
```

Events use the same names as fScript:

| Event | Example |
| --- | --- |
| Button click | `--event btn_add.click` |
| Keyboard ready | `--event keyboard_search.ready` |
| Keyboard cancel | `--event keyboard_search.cancel` |
| Timer event | `--event app.timer` |
| Theme event | `--event app.theme_changed` |

Events are executed in the order given on the command line.

## Set UI Values

Use `--set object.property=value` before events:

```bash
./build/fosrun main.fapp \
  --ui layout.ui \
  --set txt_search.text=Berlin \
  --event keyboard_search.ready
```

Supported simulated properties are:

| Property | Type |
| --- | --- |
| `.text` | string |
| `.value` | integer or string, depending on object |
| `.checked` | boolean |
| `.hidden` | boolean |
| `.enabled` | boolean |

## Native API Stubs

`fosrun` grants all fAPP permissions and stubs native calls:

| API | Runner behavior |
| --- | --- |
| `http_get(url)` | returns `true` |
| `http_status()` | returns `200` |
| `http_json(path)` | returns built-in demo values or `--json path=value` |
| `http_text(path)` | returns `{}` |
| `url_encode(text)` | percent-encodes the text |
| `date_weekday(date)` | returns English weekday abbreviation |
| `file_read(path)` | returns simulated file content |
| `file_write(path, text)` | writes to simulated in-memory storage |
| `audio_play(path)` | returns `true` |
| `wifi_status()` | returns `true`, or `false` with `--wifi-off` |
| `system.restart()` | records a simulated restart |
| `timer.start(ms)` | records a simulated timer interval |
| `Serial.printf(text)` | prints `[Serial] text` |
| `text(value)` | converts a scalar value to text |
| `number_parse(text)` | parses decimal-point and decimal-comma numbers |
| `round(value)` | rounds like the fOS API |
| `string_length(text)` | returns the byte length |
| `string_slice(text, start, length)` | uses the same start/length behavior as fOS |
| `string_last_index(text, search)` | returns the final index or `-1` |
| `math_eval(expression)` | evaluates `+`, `-`, `*`, `/` with precedence and decimal-comma output |

Trace native calls with:

```bash
./build/fosrun main.fapp --ui layout.ui --trace-native
```

Override JSON values:

```bash
./build/fosrun weather.fapp \
  --ui layout.ui \
  --json current.temperature_2m=19 \
  --json current.relative_humidity_2m=61
```

## Weather App Example

```bash
./build/fosc build /path/to/fscript_weather/main.fscript \
  --ui /path/to/fscript_weather/layout.ui \
  -o build/weather.fapp

./build/fosrun build/weather.fapp \
  --ui /path/to/fscript_weather/layout.ui \
  --event btn_open_search.click \
  --set txt_search.text=Berlin \
  --event keyboard_search.ready
```

Expected result: `panel_search.hidden=false`,
`keyboard_search.hidden=false` and `dropdown_places` contains the simulated
Berlin result.

Interactive browser run:

```bash
./build/fosrun build/weather.fapp \
  --ui /path/to/fscript_weather/layout.ui \
  --serve --open
```

Click `Search`, type into the browser keyboard and press `Enter` to send
`keyboard_search.ready`.

## Calculator Example

```bash
./build/fosc build "../../fOS4.0/example app/apps/fscript_calculator/main.fscript" \
  --ui "../../fOS4.0/example app/apps/fscript_calculator/layout.ui" \
  -o build/calculator.fapp

./build/fosrun build/calculator.fapp \
  --ui "../../fOS4.0/example app/apps/fscript_calculator/layout.ui" \
  --event btn_7.click
```

Expected result: `txt_display.text="7"`.

Expression evaluation is covered by the runner regression tests:

```bash
./build/fosrun build/calculator.fapp \
  --ui "../../fOS4.0/example app/apps/fscript_calculator/layout.ui" \
  --event btn_7.click \
  --event btn_plus.click \
  --event btn_8.click \
  --event btn_equals.click
```

Expected result: `txt_display.text="15"`.

Interactive browser run:

```bash
./build/fosrun build/calculator.fapp \
  --ui "../../fOS4.0/example app/apps/fscript_calculator/layout.ui" \
  --serve --open
```
