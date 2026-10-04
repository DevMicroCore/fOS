# fAPP Step 2 Architecture

## Compatibility boundary

The existing `/apps`, `app.cfg` and `layout.ui` system remains the active app
system. fAPP support is attached to this path instead of replacing it.

Legacy UI app:

```text
/apps/MyApp/
├── app.cfg
└── layout.ui
```

Prepared fAPP UI app:

```text
/apps/MyApp/
├── app.cfg
├── layout.ui
└── main.fapp
```

The later `app.json` manifest will be added in its designated versioning step.
`main.ui` can already be selected through `layout=main.ui`; the legacy default
remains `layout.ui`.

## UI object IDs

The current one-line UI grammar is extended with an optional `id` field:

```text
type=button;id=btn_ok;x=20;y=50;w=150;h=50;text=OK
```

Only named objects are inserted into `FAppUiRegistry`. Numeric IDs start at 1
and follow the declaration order of named objects. The future PC compiler must
apply the same rule:

```text
btn_ok     -> 1
lbl_status -> 2
```

Names are not stored by the registry. The future `.fapp` file therefore uses
only numeric IDs.

## Registry lifecycle

1. `renderAppLayout()` creates an LVGL object.
2. If the UI declaration contains `id=`, the object is registered.
3. Future fAPP events resolve numeric IDs through the registry.
4. On app close or reload, the registry is cleared first.
5. LVGL child objects are deleted only after the registry is empty.

The registry has a fixed capacity of 64 entries, resolves IDs in constant time
and performs no dynamic memory allocation.

## Loader lifecycle

`FAppLoader::begin(SD)` attaches the existing SD filesystem. For a UI app,
`prepare()` builds bounded paths and checks the UI and executable files.

Current loader states distinguish:

* missing initialization
* invalid paths
* missing UI
* compatible legacy UI-only apps
* an available executable
* an executable that cannot be opened

Beginning with Step 5, the loader also validates the fixed `.fapp` header,
section bounds, format compatibility and CRC-32. It retains only compact
metadata; execution remains deferred until the runtime is implemented in Step
8.

## Future integration points

* Step 5: `fosc` emits numeric UI references and the final versioned fAPP
  header/bytecode sections.
* Steps 6–7: semantic checks, constant folding and complete bytecode
  verification now build on this stable representation.
* Step 8: `FAppRuntime` consumes files prepared by `FAppLoader`.
* Step 9: LVGL events dispatch directly to bytecode addresses.
* Step 10: a UI API adapter isolates the VM from direct LVGL calls.
