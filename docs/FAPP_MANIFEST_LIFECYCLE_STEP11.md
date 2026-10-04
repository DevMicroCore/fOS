# Step 11 – Manifest, Permissions and Lifecycle

New fAPP applications use `app.json`. Legacy `app.cfg` remains supported; when
both files exist, a valid JSON manifest supplies the authoritative metadata.

```json
{
  "id": "devmicro.counter",
  "name": "Counter",
  "version": "1.0.0",
  "min_fos": "4.0.0",
  "type": "ui",
  "layout": "layout.ui",
  "executable": "main.fapp",
  "scrollable": false,
  "permissions": ["ui"]
}
```

Required fields are `id`, `name` and SemVer `version`. `min_fos` is checked
against the centralized fOS 4.0.0 version. Paths must be plain safe filenames,
and manifests are limited to 4096 bytes.

Known permissions are `ui`, `storage.read`, `storage.write`, `network`,
`audio` and `system.restart`. The runtime enforces the matching permission for
each native operation. Date conversion, app-local timers and serial diagnostics
do not request an unrelated capability.

Bytecode 1.1 adds system handlers with object ID zero:

```text
on app.start
on app.close
on app.theme_changed
```

Initialization runs first, followed by `app.start`. Closing grants
`app.close` one final bounded execution window before UI deletion. Theme
changes enqueue `app.theme_changed` and no longer reload the VM, so globals and
call state remain intact.
