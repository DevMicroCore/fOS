# fAPP Optimization and Verification – Step 7

Step 7 completes the PC-side safety boundary before the virtual machine is
implemented.

## Constant folding

`fosc build` evaluates side-effect-free constant expressions at compile time.
Supported folding includes:

* integer and float arithmetic
* string concatenation
* unary numeric and boolean operations
* equality and ordered comparisons
* boolean expressions

Overflow, non-finite results, division by zero and modulo by zero are not
folded. They remain explicit bytecode so the future VM can apply its defined
runtime error behavior.

Optimization is enabled by default and can be disabled for diagnostics:

```bash
fosc build main.fscript --ui layout.ui --no-optimize -o main.fapp
```

## Bytecode verification

```bash
fosc verify main.fapp
```

The verifier runs automatically before `fosc build` writes its output and
before `fosc disasm` displays a file. It checks every function for:

* known, complete instructions
* valid constant, global, local, function and UI indices
* function arity at every call site
* valid UI property/method identifiers
* in-range jumps landing exactly on instruction boundaries
* consistent stack depths across control-flow joins
* operand-stack underflow
* balanced returns and reachable function endings
* sufficient declared maximum stack depth

The verifier diagnostic range is `FS601` through `FS608`. Structural fAPP and
CRC errors remain in `FS411` through `FS419`.

The ESP32 loader still performs its independent format, section, compatibility
and streaming CRC checks. The bounded VM itself remains Step 8.
