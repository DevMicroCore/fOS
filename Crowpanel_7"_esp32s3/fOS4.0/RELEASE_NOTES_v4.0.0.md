# fOS 4.0.0 – fAPP Foundation

## Development milestones: Steps 2 through 12

This milestone adds the first embedded foundations for fScript/fAPP
applications while preserving all existing SD-card applications.

### Added

* Central fOS version definition in `src/core/FOSVersion.h`.
* Fixed-capacity `FAppUiRegistry` for mapping numeric object IDs to LVGL
  objects.
* Optional `id=` values in the existing `layout.ui` syntax.
* Declaration-order numeric IDs for named UI objects.
* `FAppLoader` foundation with fixed-size path buffers.
* Detection of optional `main.fapp` files without loading them into RAM.
* Optional `fapp=` key in legacy `app.cfg` files.

### Compatibility

* Existing `/apps` directories remain unchanged.
* Existing `app.cfg` files remain supported.
* Existing `layout.ui` files without IDs render as before.
* Existing native Calculator, Radio, Clock, Weather, Email and Text apps remain
  unchanged.

### GitHub App Store and OTA reliability

* Replaced repeated GitHub directory API scans with compact App Store and OTA
  index files.
* Added flash-resident indexes for the apps and binaries shipped with this
  release, so a temporary GitHub API `403` does not hide updates or apps.
* Direct file downloads retain a narrowly scoped GitHub Contents API fallback.
* HTTP `403`, `404` and `429` responses are no longer retried in a tight loop.
* Added a repository index generator and ready-to-upload index files.
* Recovery now validates the ESP image size and header before erasing `app0`, writes
  the target partition directly, restores the boot header last and verifies
  every flashed byte against the SD file before activating the image.
* OTA now checks the downloaded application size against `fos-ota.index` and
  validates every ESP image segment before installing the recovery image.
  Recovery reports all segment offsets and lengths before erasing `app0`.
* Recovery no longer rewrites flash every five seconds after a deterministic
  failure.
* OTA refuses to continue if Arduino Core selects a different implicit update
  partition than the one requested by fOS.

### Memory behavior

* The UI registry uses a fixed capacity of 64 named UI objects.
* Numeric object lookup uses direct indexing instead of a linear search.
* No dynamic allocation is used by `FAppLoader` or `FAppUiRegistry`.
* UI names are used only while parsing `layout.ui` and are not retained in the
  registry.
* The loader reads the fixed header and verifies CRC-32 with a 256-byte
  streaming buffer; it never loads the complete executable into RAM.

### fScript lexer

* Added the PC-side `fosc` terminal program.
* Added the fScript 1.0.0 token model.
* Added identifiers, integers, floating-point numbers, exponent notation,
  strings, booleans and `nil`.
* Added function, condition, event and return keywords.
* Added arithmetic, comparison, assignment and boolean operators.
* Added Lua-style `--` comments and JavaScript-style line and block comments.
* Added source locations with filename, line, column and byte offset.
* Added stable diagnostics `FS000` through `FS005`.
* Added Make and CMake build configurations.
* Added automated tests and a lexer example program.

### fScript parser and AST

* Updated the PC-side compiler to `fosc` 0.2.0.
* Added a recursive-descent parser with syntax-error recovery.
* Added typed AST nodes with explicit source locations.
* Added variables, functions, `on object.event` handlers, conditions,
  `return`, calls, member access and assignments.
* Added precedence-aware arithmetic, comparison and boolean expressions.
* Added stable parser diagnostics `FS201` through `FS205`.
* Added a human-readable AST printer through `fosc parse`.
* Added automated parser tests for complete programs, precedence, calls,
  assignments and diagnostic cases.

### fAPP bytecode and compiler

* Updated the PC-side compiler to `fosc` 0.3.0.
* Added `fosc build` for producing executable `main.fapp` files.
* Added `fosc disasm` for validating and inspecting compiled files.
* Defined the stable fAPP format 1.0 and bytecode version 1.0.
* Added an 80-byte little-endian header with minimum fOS version and source hash.
* Added constant, function, event and code sections with bounded entry counts.
* Added numeric global, local and function slots.
* Added code generation for literals, assignments, arithmetic, comparisons,
  conditions, calls and returns.
* Added numeric UI property, method and event instructions.
* Added deterministic UI-ID resolution from `main.ui` or `layout.ui`.
* UI source names are not retained in generated fAPP files.
* Added CRC-32 protection and complete output self-validation.
* Extended the ESP32 loader with header, version, section-range and streaming
  checksum validation using a 256-byte buffer.
* Added compiler, binary round-trip, damage-detection and embedded-loader tests.

### Semantic analysis

* Updated the PC-side compiler through `fosc` 0.5.0.
* Added `fosc check` for semantic validation without writing bytecode.
* Added inferred primitive types while keeping untyped parameters dynamic.
* Added duplicate-symbol, unknown-symbol and function-arity checks.
* Added operator, condition, assignment and return-type validation.
* Added object-specific UI property, method and event compatibility checks.
* Added stable semantic diagnostics `FS501` through `FS518`.

### Optimization and bytecode verification

* Added conservative constant folding for numeric, string, comparison and
  boolean expressions.
* Added `--no-optimize` for bytecode comparison and diagnostics.
* Added `fosc verify` for standalone fAPP bytecode verification.
* Build output is verified before it is written to disk.
* Disassembly rejects structurally valid but unsafe instruction streams.
* Added instruction decoding, operand-boundary and index validation.
* Added jump-target and instruction-boundary validation.
* Added control-flow-aware stack analysis, join consistency and return checks.
* Added stable verifier diagnostics `FS601` through `FS608`.
* Added semantic, optimizer and verifier regression suites.

### Runtime and integration (Steps 8–12)

* Added a cooperative fixed-resource `FAppRuntime` with instruction and time
  budgets, bounded stacks, calls, locals, globals, strings and event queues.
* Implemented every bytecode 1.3 instruction on the ESP32 side.
* Runtime faults stop only the affected app and appear inside the fOS UI.
* Added `FAppEventBridge` for asynchronous LVGL event delivery.
* Added the LVGL-independent `FAppUiApi` and the guarded `LvglFAppUiApi` adapter.
* Added `app.json` with identifiers, SemVer, fOS compatibility, safe paths and
  explicit permissions while retaining `app.cfg` fallback support.
* Added `app.start`, `app.close` and `app.theme_changed` lifecycle handlers.
* Theme changes preserve VM variables and execution state.
* Updated the PC compiler to `fosc` 0.8.0 and bytecode 1.3.
* Added scalar-to-string concatenation for dynamic status text.
* Added the complete, compiled `fscript_counter` SD-card example.
* Added runtime, lifecycle, permission and manifest end-to-end regression tests.
* Added guarded `http_get`, `http_status`, `http_json`, `http_text` and
  `url_encode` native functions for general internet data access.
* Added `app.timer`, emitted once per minute, for periodic script refreshes.
* Added bounded response, URL, timeout and runtime-string limits plus HTTP/JSON
  regression tests.
* Serialized OTA and fScript network access so concurrent TLS clients cannot
  exhaust ESP32 memory; OTA connection failures now remain visible and retry.
* Cached OTA listings independently of the update screen lifecycle, so a list
  fetched during boot is shown immediately when the update screen opens.
* Added fScript layout support for rollers, dropdowns, keyboards, nested panels
  and the font/visibility styling required by the original weather UI.
* Added keyboard checkmark/cancel events plus `date_weekday()` so the fScript
  weather app reproduces the original search and forecast interaction.
* Moved the VM value area out of the permanent global object. It is allocated
  in PSRAM only while a script app is active and released on close, restoring
  the contiguous internal heap required by ESP32 TLS/SHA during OTA requests.
* Release the retained fScript HTTP response buffer when an app closes and log
  free/largest internal heap immediately before every OTA TLS attempt.

### fOS API (Step 19)

* Updated the PC toolchain to `fosc` 0.12.1, added `fosrun` 0.2.1 and kept
  bytecode 1.5.
* Added Step 13 `fosrun`, a runner that executes compiled `.fapp` files with
  simulated UI state, event dispatch, native fOS API stubs and a local browser
  UI test mode.
* Fixed `fosrun` helper parity for `math_eval()` and `string_slice()` so the
  fScript calculator can evaluate expressions such as `7+8=15` like it does on
  fOS.
* Added app-scoped `file_read()` and `file_write()` with traversal protection.
* Added local-file `audio_play()` using the existing audio subsystem.
* Added `wifi_status()`, controlled `system.restart()`, configurable
  `timer.start()` and format-string-safe `Serial.printf()`.
* Added general fScript helper APIs: `text()`, `number_parse()`, `round()`,
  `string_length()`, `string_slice()`, `string_last_index()` and `math_eval()`.
* Added the uncompiled `fscript_calculator` sample app that mirrors the native
  calculator layout and uses fScript for its UI logic.
* Cached the active `.fapp` image in PSRAM/RAM while the script app is open, so
  VM instruction fetch no longer reopens the SD file for every bytecode read.
* Increased the cooperative VM update budget to `256` instructions / `4000 us`
  now that bytecode dispatch normally runs from RAM.
* Fixed fAPP keyboard visibility by moving shown UI objects to the foreground
  and automatically showing a keyboard when its `target=` textarea is touched
  or focused.
* Activated per-function checks for `storage.read`, `storage.write`, `audio`,
  `network` and the new `system.restart` permission.
* Kept native bytecode IDs 1–6 unchanged for existing HTTP/weather apps.
* Added compiler, verifier, manifest, runtime, permission and native API
  regression coverage for all new calls.
* Added complete German and English manuals for fScript, fosc, app packaging,
  UI/native APIs, bytecode and the embedded VM system.
