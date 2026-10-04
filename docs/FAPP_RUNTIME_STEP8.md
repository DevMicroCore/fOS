# Step 8 – Bounded fAPP Runtime

`FAppRuntime` executes verified fAPP bytecode on the ESP32-S3. It reads code and
constants from the SD card on demand and stores only bounded metadata and VM
state in RAM.

## Resource limits

| Resource | Limit |
| --- | ---: |
| String constants | 128 |
| Functions | 48 |
| Event bindings | 64 |
| Globals | 32 |
| Operand values | 64 |
| Local values | 96 |
| Call depth | 8 |
| Pending events | 8 |
| Runtime string | 255 bytes plus terminator |

The firmware calls `update(256, 4000)` from the normal fOS loop. A script can
therefore consume at most 256 instructions or approximately 4 ms per pass.
Runtime faults stop only the current fAPP and are shown inside the app screen;
fOS itself continues running.

The global, operand and local value areas are allocated only while a fAPP is
active. On ESP32-S3 they use PSRAM when available and are released completely
when the app closes. This avoids permanently fragmenting internal RAM needed by
the TLS/SHA implementation used for OTA updates.

All bytecode 1.5 instructions are implemented, including calls, locals,
globals, arithmetic, comparisons, branches, string concatenation, UI access
and returns.
