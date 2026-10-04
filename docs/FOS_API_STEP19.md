# Step 19 – fOS API

Bytecode 1.5 adds native APIs without changing the numeric IDs of the
six existing HTTP/date functions. Existing bytecode up to version 1.4 remains
loadable by fOS 4.0.0.

| fScript call | Result | Required `app.json` permission |
| --- | --- | --- |
| `file_read(path)` | String | `storage.read` |
| `file_write(path, data)` | Boolean | `storage.write` |
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

## Files and audio

`path` is always relative to the active app folder. Absolute paths, backslashes,
empty path segments, `.` and `..` are rejected. Apps therefore cannot use these
calls to read or overwrite files belonging to fOS or another app.

`file_read()` returns at most 255 bytes, matching the bounded runtime string.
A missing or oversized file returns an empty string without terminating the
app. `file_write()` replaces the target file and returns whether all bytes were
written. `audio_play()` accepts an existing file from the same app folder and
uses the existing fOS audio engine; closing the app stops its playback.

## Wi-Fi, restart, timer and serial

`wifi_status()` reports whether the Wi-Fi interface is currently connected. It
does not initiate a connection. `system.restart()` only schedules a restart;
fOS performs it after the current VM update has yielded.

`timer.start()` changes the recurring interval of the existing `app.timer`
event. The allowed range is 100 through 86,400,000 milliseconds. If it is never
called, the compatible default remains 60,000 milliseconds.

`Serial.printf()` accepts exactly one already-composed string. fOS prints it
with a fixed `"%s"` format, so percent characters from app data are never
interpreted as native format directives:

```fscript
Serial.printf("Wi-Fi: " + wifi_status() + "\n")
```

## General helpers

The additional helper functions are deliberately small and deterministic so
apps can format values without large script-side support code:

```fscript
var value = number_parse("12,5")
lbl_result.text = text(value)
lbl_rounded.text = "Rounded: " + round(value)

var source = "Calculator"
var short_name = string_slice(source, 0, 4)
var last_a = string_last_index(source, "a")

lbl_result.text = math_eval("2+3*4")
```

`math_eval()` accepts decimal points and decimal commas, understands `+`, `-`,
`*` and `/`, and formats the result with a decimal comma. Division by zero or a
malformed expression returns the string `Math Error` instead of crashing the VM.

An app using all guarded APIs declares:

```json
"permissions": [
  "storage.read",
  "storage.write",
  "audio",
  "network",
  "system.restart"
]
```

Compile and inspect the included example on macOS, Linux or Windows:

```bash
cd fScript_Development_Environment/fosc
make
./build/fosc check examples/system_api_demo.fscript
./build/fosc build examples/system_api_demo.fscript -o build/system_api_demo.fapp
./build/fosc disasm build/system_api_demo.fapp
```
