# fScript Parser – Step 4

## Scope

Step 4 adds the syntax frontend after the Step 3 lexer. It intentionally stops
at a typed abstract syntax tree. It does not generate bytecode or `.fapp` files.

Versions:

* fosc: `0.2.0`
* fScript language: `1.0.0`

## Supported grammar

Top-level and block statements:

```text
var name = expression
function name(parameter, ...)
    statements
end
on object.event
    statements
end
if expression then
    statements
else
    statements
end
return expression
expression
```

Expressions include literals, variables, grouping, unary and binary operators,
calls, member access and assignment. Assignments accept only variables and
member expressions as targets.

## Operator precedence

From lowest to highest:

1. assignment (`=`), right-associative
2. boolean OR (`or`, `||`)
3. boolean AND (`and`, `&&`)
4. equality (`==`, `!=`)
5. comparison (`<`, `<=`, `>`, `>=`)
6. addition and subtraction (`+`, `-`)
7. multiplication, division and modulo (`*`, `/`, `%`)
8. unary (`!`, `not`, unary `+`, unary `-`)
9. function calls and member access

## Diagnostics

| Code | Meaning |
| --- | --- |
| `FS201` | Unexpected token or missing expression |
| `FS202` | A required grammar token is missing |
| `FS203` | Assignment target is not a variable or member |
| `FS204` | Function/call exceeds 255 parameters/arguments |
| `FS205` | Event reference is not `object.event` |

Diagnostics retain the filename, line and column produced by the lexer. After a
syntax error, the parser synchronizes at statement boundaries so later errors
can still be reported.

## Build, test and inspect on macOS

```bash
xcode-select --install
cd fScript_Development_Environment/fosc
make
make test
./build/fosc parse examples/lexer_demo.fscript
```

The `parse` command exits with a non-zero status if lexer or parser diagnostics
were produced. On success it prints the AST, which makes grammar changes easy to
review before bytecode generation is introduced in Step 5.
