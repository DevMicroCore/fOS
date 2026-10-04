# fAPP Bytecode and File Format – Step 5

Step 5 defines the first stable binary format produced by `fosc`. The format is
little-endian and deliberately section-based so the ESP32 can validate and read
individual ranges directly from SD without loading the complete file into RAM.

## Build command

```bash
fosc build main.fscript --ui layout.ui -o main.fapp
fosc disasm main.fapp
```

When `--ui` is omitted, `fosc` searches beside the source for `main.ui` and then
`layout.ui`. A layout is optional for programs that do not reference UI objects.

## UI symbol resolution

Named, supported UI objects receive IDs in declaration order, exactly like the
firmware registry:

```text
type=button;id=btn_ok;...
type=label;id=lbl_status;...
```

becomes:

```text
btn_ok     -> 1
lbl_status -> 2
```

Supported named `button`, `label`, `textarea`, `switch`, `checkbox`, `panel`,
`roller`, `dropdown` and `keyboard` lines consume IDs. The compiler rejects
duplicate or invalid names and more than 64 named objects. UI names are not
written to `.fapp`; event bindings and UI instructions contain only the
numeric ID.

## Header

Every `.fapp` starts with an 80-byte header:

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 4 | Magic `FAPP` |
| 4 | 2 | fAPP format major/minor |
| 6 | 2 | bytecode major/minor |
| 8 | 2 | header size (`80`) |
| 10 | 2 | flags |
| 12 | 4 | complete file size |
| 16 | 4 | CRC-32 |
| 20 | 8 | constant-section offset and size |
| 28 | 2 | constant count |
| 30 | 2 | global variable count |
| 32 | 8 | function-table offset and size |
| 40 | 2 | function count |
| 42 | 2 | initialization-function index |
| 44 | 8 | event-table offset and size |
| 52 | 2 | event count |
| 56 | 8 | bytecode offset and size |
| 64 | 4 | FNV-1a source hash |
| 68 | 6 | minimum fOS version |
| 74 | 6 | reserved |

CRC-32 covers the complete file while treating the CRC field itself as zero.
The current format version is `1.0`, the current bytecode version is `1.4`, and
the minimum system version is fOS `4.0.0`.

## Sections

The constant section stores length-prefixed UTF-8 string literals. Integer and
floating-point literals are encoded directly in instructions. The function
table uses fixed 16-byte records containing code range, arity, local count,
maximum stack depth and flags. The event table uses fixed 8-byte records with a
numeric UI object ID, event type and handler-function index.

The instruction stream supports:

* nil, boolean, signed 32-bit integer, 64-bit float and string values
* global and local loads/stores
* arithmetic, comparison and boolean operations
* conditional and unconditional relative jumps
* numeric function calls
* numeric UI property reads/writes and UI method calls
* function returns

Function declarations and forward calls resolve to numeric indices. The
initialization function is always function `0`; named functions follow in source
order, then event handlers.

## Firmware validation

`FAppLoader` now reads only the fixed header, validates all section ranges and
version fields, then verifies CRC-32 in a 256-byte streaming buffer. Invalid,
damaged or newer incompatible executables are rejected before a future runtime
can execute them. Legacy UI apps without `main.fapp` remain supported.

Execution is intentionally not part of Step 5. The VM arrives in Step 8.
