# fScript Semantic Analysis – Step 6

Step 6 inserts a dedicated semantic pass between parsing and bytecode
generation. `fosc build` now stops before producing a file if the source is
syntactically valid but semantically unsafe.

## Command

```bash
fosc check main.fscript --ui layout.ui
```

Like `build`, `check` automatically searches for `main.ui` and then
`layout.ui` beside the source when `--ui` is omitted.

## Type model

The inferred value types are:

* `nil`
* `boolean`
* signed 32-bit `integer`
* 64-bit `float`
* UTF-8 `string`
* `any` for values that cannot be known statically, such as untyped function
  parameters

Integer values can be assigned to float variables without narrowing. A
variable whose type is inferred from a known initializer cannot later receive
an incompatible known type. `nil` and dynamically unknown values remain
flexible.

## Checked rules

The analyzer validates:

* duplicate globals, functions, parameters and function-scoped locals
* unknown variables and functions
* function argument counts and return-type consistency
* boolean `if` conditions
* numeric, string, comparison and boolean operator combinations
* type-compatible variable and UI-property assignments
* top-level-only function and event declarations
* invalid top-level returns and event return values
* existing UI objects from the selected layout
* object-specific UI properties, methods and events

UI examples:

```text
lbl_status.text = "Ready"      -- string
toggle.checked = true          -- boolean
txt_name.clear()               -- textarea only
on btn_start.click             -- valid for a button
```

The semantic diagnostic range is `FS501` through `FS518`. Every error retains
the source file, line and column supplied by the lexer and AST.
