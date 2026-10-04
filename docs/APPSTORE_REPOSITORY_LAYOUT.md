# fOS AppStore repository layout

The default AppStore source is:

`https://github.com/DevMicroCore/fOS/tree/main/apps`

Each app has one directory. Inside it, every published version has its own
semantic-version directory. The actual app directory is nested below that
version:

```text
apps/
  calculator/
    1.0.0/
      calculator/
        app.json
        layout.ui
        main.fapp
    1.0.1/
      calculator/
        app.json
        layout.ui
        main.fapp
    1.1.0/
      calculator/
        app.json
        layout.ui
        main.fapp
```

The `version` value in `app.json` must match the semantic version of its parent
directory. Use `min_fos` to declare the oldest compatible fOS release:

```json
{
  "id": "calculator",
  "name": "Calculator",
  "version": "1.1.0",
  "min_fos": "4.0.0"
}
```

Legacy apps may provide the same AppStore metadata in `app.cfg` instead:

```ini
name=Calculator
version=1.1.0
min_fos=4.0.0
```

When both files exist, `app.json` has priority. The AppStore also reads
`version=` from an installed app's `app.cfg`, so its installed version is shown
and compared correctly.

When the store is refreshed, fOS sorts the version directories from newest to
oldest and selects the first valid version whose `min_fos` is not newer than the
running fOS version. This keeps newer releases online while older fOS versions
automatically receive the newest release they can run.

For migration, the previous flat layout (`apps/calculator/app.json`) remains
readable. Existing `/system/apps/stores.txt` files that contain the former fOS
default URL are migrated automatically; custom store URLs are left unchanged.

The AppStore screen is rendered before GitHub access begins. Repository data is
loaded by a temporary background task, so opening the AppStore does not wait for
all version metadata to download.
