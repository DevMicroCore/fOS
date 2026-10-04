# fScript Lexer – Step 3

## Purpose

The lexer is the first executable part of the PC-side `fosc` compiler. It
converts `.fscript` source text into a deterministic token sequence. It does not
build an AST and does not generate `.fapp` bytecode.

## Versions

* fOS: `4.0.0`
* fosc: `0.1.0`
* fScript language: `1.0.0`

## Source locations

Every token contains its original lexeme plus one-based line and column and a
zero-based byte offset. Decoded literal values are retained where applicable.
Diagnostics additionally contain the source filename and a stable error code.

## Token groups

The lexer recognizes identifiers, number and string literals, fScript
keywords, arithmetic/comparison/boolean operators and the punctuation required
for function calls, UI properties and events.

```fscript
on btn_ok.click
    lbl_status.text = "Hallo"
end
```

Lua-style `--` comments and JavaScript-style `//` and `/* ... */` comments are
supported.

## Deferred decisions

Grammar precedence, statements, type rules, symbol resolution and UI-ID
validation belong to the parser and compiler steps. The lexer deliberately
does not make those decisions.
