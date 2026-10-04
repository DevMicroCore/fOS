# fosc and fosrun

`fosc` 0.12.1 is the PC-side compiler toolchain for fScript. Development Steps
3-12 contain the lexer, recursive-descent parser, typed AST, bytecode generator,
semantic analyzer, optimizer, versioned `.fapp` writer/reader, verifier and
disassembler. Step 13 adds `fosrun`, a PC runner for compiled `.fapp` logic
tests on Windows, Linux and macOS. `fosrun` 0.2.1 also includes a local browser
UI mode for clickable app tests.

Bytecode 1.5 contains the lifecycle handlers `app.start`, `app.close`,
`app.theme_changed` and `app.timer`, plus permission-guarded HTTP/JSON native
functions, ISO-date weekday conversion and keyboard `ready`/`cancel` events.
Step 19 adds app-scoped file access, audio playback, Wi-Fi status, controlled
restart, configurable app timers and safe serial output. Native function IDs
from bytecode 1.3 remain unchanged. Bytecode 1.5 adds general helpers:
`text()`, `number_parse()`, `round()`, `string_length()`, `string_slice()`,
`string_last_index()` and `math_eval()`.
`fosrun` implements these helpers with the same observable behavior as fOS,
including `math_eval()` operator precedence and decimal-comma output.
The embedded test suite also executes generated files in
the bounded fOS runtime and validates `app.json` manifests.

## Build and test

```bash
make
make test
```

The default build creates:

```text
build/fosc
build/fosrun
```

Alternatively use CMake:

```bash
cmake -S . -B build-cmake
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
```

## Use

```bash
build/fosc lex examples/lexer_demo.fscript
build/fosc parse examples/lexer_demo.fscript
build/fosc check examples/lexer_demo.fscript --ui examples/lexer_demo.ui
build/fosc build examples/lexer_demo.fscript --ui examples/lexer_demo.ui -o build/main.fapp
build/fosc verify build/main.fapp
build/fosc disasm build/main.fapp
build/fosc check examples/system_api_demo.fscript
build/fosc check "../../fOS4.0/example app/apps/fscript_calculator/main.fscript"
build/fosc build examples/fosrun_demo.fscript --ui examples/fosrun_demo.ui -o build/fosrun_demo.fapp
build/fosrun build/fosrun_demo.fapp --ui examples/fosrun_demo.ui --event btn_add.click
build/fosrun build/fosrun_demo.fapp --ui examples/fosrun_demo.ui --serve --open
```

Lexer errors use stable codes:

| Code | Meaning |
| --- | --- |
| `FS000` | Source file cannot be opened |
| `FS001` | Unexpected character |
| `FS002` | Unterminated string |
| `FS003` | Unknown escape sequence |
| `FS004` | Unterminated block comment |
| `FS005` | Malformed number exponent |
| `FS201` | Unexpected parser token |
| `FS202` | Required parser token is missing |
| `FS203` | Invalid assignment target |
| `FS204` | More than 255 parameters or arguments |
| `FS205` | Malformed event reference |
| `FS301–FS303` | Invalid, duplicate or excessive UI IDs |
| `FS304–FS330` | Code-generation, native-call and embedded-limit errors |
| `FS401–FS405` | fAPP serialization errors |
| `FS411–FS419` | Invalid, incompatible or damaged fAPP file |
| `FS420–FS422` | Build command/output errors |
| `FS501–FS520` | Semantic, type, function, native API and UI-compatibility errors |
| `FS601–FS608` | Instruction, index, control-flow and stack-verification errors |

Supported input includes identifiers, numeric and string literals, fScript
keywords, arithmetic/comparison/boolean operators and Lua/JavaScript-style
comments. Every token retains its source line, column and byte offset.

The parser supports variables, functions, event handlers, `if`/`else`,
`return`, calls, member access and assignments. Its expression precedence is:
assignment, `or`, `and`, equality, comparison, addition/subtraction,
multiplication/division/modulo, unary operators, then calls and member access.
Parser recovery reports multiple independent errors in one run where possible.

The code generator assigns numeric slots to globals, locals and functions. It
resolves supported UI properties, methods and events against the selected
`.ui` file and writes only numeric UI object IDs. `.fapp` 1.0 contains an
80-byte header, constant/function/event/code sections, a source hash, minimum
fOS version and CRC-32. `fosc build` validates its own output before reporting
success.

The semantic pass infers known primitive types, keeps untyped parameters
dynamic, validates function signatures and applies object-specific UI rules.
Constant expressions are folded conservatively by default; use
`--no-optimize` to compare unoptimized bytecode. The verifier walks every
instruction and control-flow edge before an output file is accepted.

String concatenation accepts scalar values, so status labels can use
expressions such as `"Klicks: " + counter`.

Named layouts support `label`, `button`, `textarea`, `switch`, `checkbox`,
`panel`, `roller`, `dropdown` and `keyboard`. The optional `parent=` field
nests controls inside a previously declared panel; `target=` attaches a
keyboard to a previously declared textarea. `font=20|24|40`, `hidden=`,
`radius=`, `border=`, `placeholder=` and `align=` provide bounded styling.
Keyboard handlers use `on keyboard_id.ready` for the checkmark and
`on keyboard_id.cancel` for the cancel key.
On fOS, touching or focusing the target textarea also shows the attached
keyboard and brings it to the foreground.

## Complete manuals

The firmware documentation contains two complete references:

* `../../fOS4.0/docs/FSCRIPT_FOSC_VM_HANDBUCH_DE.md` (Deutsch)
* `../../fOS4.0/docs/FSCRIPT_FOSC_VM_MANUAL_EN.md` (English)

They document all fScript statements and operators, every `fosc` and `fosrun`
command, layouts, manifests, UI and native APIs, bytecode, VM limits and
troubleshooting.
The runner-specific reference is `../../fOS4.0/docs/FOSRUN_STEP13.md`.

## macOS Terminal

The only required build dependency is Apple's command-line toolchain:

```bash
xcode-select --install
cd /path/to/fOS4.0/fScript_Development_Environment/fosc
make
make test
./build/fosc parse examples/lexer_demo.fscript
./build/fosc check examples/lexer_demo.fscript --ui examples/lexer_demo.ui
./build/fosc build examples/lexer_demo.fscript --ui examples/lexer_demo.ui -o build/main.fapp
./build/fosc verify build/main.fapp
./build/fosc disasm build/main.fapp
./build/fosc build examples/fosrun_demo.fscript --ui examples/fosrun_demo.ui -o build/fosrun_demo.fapp
./build/fosrun build/fosrun_demo.fapp --ui examples/fosrun_demo.ui --event btn_add.click
./build/fosrun build/fosrun_demo.fapp --ui examples/fosrun_demo.ui --serve --open
```
