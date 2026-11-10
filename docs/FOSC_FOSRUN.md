# fosc and fosrun

The shared compiler is fosc **0.14.0** and emits bytecode **1.8** with minimum
firmware version **4.1.0**. The same compiled fAPP files run on both boards.
Run the commands below in `fScript_Development_Environment/fosc`.
The source, tests, CMake project and examples remain in that directory.
The host runtime build uses shared modules from the included P4 source tree;
it does not compile or upload P4 firmware.

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
`panel`, `roller`, `dropdown`, `keyboard`, `canvas`, `slider`, `tabview` and `tab`. The optional `parent=` field
nests controls inside a previously declared panel; `target=` attaches a
keyboard to its target textarea, including targets declared later in the layout. `font=20|24|40`, `hidden=`,
`radius=`, `border=`, `placeholder=` and `align=` provide bounded styling.
Keyboard handlers use `on keyboard_id.ready` for the checkmark and
`on keyboard_id.cancel` for the cancel key.
On fOS, touching or focusing the target textarea also shows the attached
keyboard and brings it to the foreground.

## Complete manuals

The firmware documentation contains two complete references:

* `FSCRIPT_FOSC_VM_HANDBUCH_DE.md` (Deutsch)
* `FSCRIPT_FOSC_VM_MANUAL_EN.md` (English)

They document all fScript statements and operators, every `fosc` and `fosrun`
command, layouts, manifests, UI and native APIs, bytecode, VM limits and
troubleshooting.
The runner command reference is included in the complete manuals above.

## macOS Terminal

The only required build dependency is Apple's command-line toolchain:

```bash
xcode-select --install
cd /path/to/fOS4.1.0/fScript_Development_Environment/fosc
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


## Preview both display sizes

```sh
./build/fosrun ../../apps/radio/main.fapp --ui ../../apps/radio/layout.ui --size 800x480 --serve
./build/fosrun ../../apps/radio/main.fapp --ui ../../apps/radio/layout.ui --size 1024x600 --serve
```

`--sd-root <directory>` provides read-only local SD fixtures. Audio is simulated,
not decoded or played. See [Radio and audio APIs](FSCRIPT_RADIO_AUDIO.md).
