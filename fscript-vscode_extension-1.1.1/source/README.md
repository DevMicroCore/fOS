# fScript & fOS UI for Visual Studio Code

Extension **1.1.1**, matching **fOS 4.0.1 / fosc 0.14.0**.
Includes syntax highlighting and IntelliSense for `.fscript` and fOS `.ui` files,
the **fosc compiler**, and an interactive **fosrun preview** inside VS Code.
All extension commands, settings, help text, diagnostics, and bundled example
labels are in English. Requires VS Code 1.85 or later.

## Installation and updates

1. Extract the download ZIP.
2. In VS Code, open **Extensions** and click **…**.
3. Select **Install from VSIX…**.
4. Choose `fscript-fos-ui-1.1.1.vsix` from the extracted ZIP.
5. Reload the window if prompted, then open a `.fscript` or `layout.ui` file.

Installing this VSIX updates the existing `devmicrocore.fscript-fos-ui` extension;
there is no need to uninstall the previous version.
Alternatively, open the Command Palette (`Cmd+Shift+P` on macOS,
`Ctrl+Shift+P` on Windows/Linux) and select **Extensions: Install from VSIX…**.

The editor status bar should show **fScript** or **fOS UI**. If necessary, click
the language name and select **fOS UI** for an already open `.ui` file.
The extension registers `.ui` and `.fui`; you can still select another language
mode for Qt or other `.ui` formats.

## Suggestions and highlighting

- Keywords, comments, strings, numbers, functions, events, and UI fields use
  syntax colors from your selected VS Code theme.
- Typing `audio_` suggests audio commands, including `audio_stream`, `audio_seek`,
  and `audio_playing`.
- `timer.` suggests `start(...)`; `Serial.` suggests `printf(...)`.
- `roller_files.` suggests roller properties and methods, such as `text`,
  `value`, `load_dir_filtered`, and `load_pairs`.
- `on slider_volume.` suggests matching widget events; `on app.` suggests
  `start`, `close`, `theme_changed`, and `timer`.
- Parameter hints appear after an opening parenthesis or comma. Accept
  suggestions with **Enter/Tab** and move between argument placeholders with **Tab**.
- **Ctrl+Space** opens suggestions manually. Use **Trigger Parameter Hints**
  from the Command Palette to open parameter help.
- Hover over a command for its description, signature, return type, and
  required `app.json` permissions.

The extension locates the associated UI through `app.json` (`layout`). Without
that entry, it checks `<script-name>.ui`, then `layout.ui` in the same folder.
Unsaved changes in an open UI document also affect suggestions. Object IDs are
**case-sensitive**, matching the compiler. General API suggestions work in an
unsaved script; save it beside its UI to enable UI object suggestions.

**F12 / Go to Definition** on a UI ID opens its declaration in the layout.
Functions and variables in the current script are also suggested. This is
lightweight language assistance; `fosc` performs full type and scope checking.

## Editing `layout.ui`

After `type=`, suggestions list all 13 widget types. After `bg=`, suggestions
include `theme`, `surface`, and RGB colors. `parent=` and `target=` suggest suitable
object IDs. Fields such as `target`, `min`, `max`, and `tab_height` are offered
for the relevant widget type. Semicolons separate fields; `\n` adds line breaks
in text or options.

Use snippets such as `ui-button`, `ui-roller`, `ui-slider`, `ui-tabview`, `ui-tab`,
and `ui-keyboard`. Script snippets include `appstart`, `apptimer`, `event`,
`function`, `if`, `ifelse`, `while`, `for`, and `var`.

RGB values in `bg` and `fg` have color swatches and a color picker. `theme` and
`contrast` are dynamic fOS colors and appear as names.
UI diagnostics flag duplicate or invalid IDs, unknown widget types, missing
parents, and invalid keyboard targets. Keyboard targets may be declared later;
parent objects must be declared first, matching the firmware loader.

**Canvas color arguments:** fScript in fOS 4.0.1 expects decimal integers, such
as `16777215` for white. UI color fields also allow `0xFFFFFF`. Command suggestions
use the appropriate format.

## Bundled fosc and fosrun

You do not need to select tools from a separate fOS project folder. The extension
includes their source code and stores prepared tools in a versioned cache managed
by VS Code. Changes to the bundled toolchain use a separate cache. Custom
`fosc` and `fosrun` installations can still be selected in Settings.

### First setup on macOS (Intel and Apple Silicon)

1. Install the current VSIX and open your app folder in VS Code.
2. Run **fScript: Prepare Tools (fosc + fosrun)** from the Command Palette.
3. The extension builds both tools for your Mac. The first build may take a few
   minutes; a progress notification shows the current task. Later launches reuse
   the prepared tools.
4. Open `main.fscript` or its UI and click **▶** in the editor toolbar.

Local macOS builds require Apple's **Xcode Command Line Tools**. If they are
already installed, no additional setup is needed. Otherwise, run this once in
Terminal:

```bash
xcode-select --install
```

After installation, run **fScript: Prepare Tools (fosc + fosrun)** again.
You do not need to run `make` manually in the fOS folder. macOS binaries could
not be built in this Linux environment, so your Mac builds the bundled sources.

Linux x64 includes prebuilt tools. If they cannot run on your system, the
extension builds locally using a C++17 compiler, as it does on other Linux
architectures. Windows local builds require **MinGW-w64** with `g++` on PATH;
the bundled runner uses Winsock on Windows. macOS and Windows builds have not
been tested in this environment.

### Commands

| Command | Action |
| --- | --- |
| **fScript: Prepare Tools (fosc + fosrun)** | Prepare the tools automatically |
| **fScript: Check Current App (fosc)** | Show compiler errors in **Problems** |
| **fScript: Compile Current App (fosc)** | Create `main.fapp`, or the `executable` specified in `app.json` |
| **fScript: Run App (fosrun)** / **▶** | Compile the script and open an interactive preview |
| **fScript: Stop Preview** | Stop the runner; closing the preview also stops it |
| **fScript: Show Compiler / Runner Output** | Show compiler diagnostics and `Serial.printf` output |
| **fScript: Rebuild Bundled Tools** | Rebuild the cached tools from source |
| **fScript: Open Associated UI** | Open the matching layout |

`fosrun` is the fOS runner, sometimes referred to as “frun”. It uses the actual
fOS VM and displays buttons, text areas, keyboards, rollers, tabs, sliders, and
canvases. The preview communicates directly with the extension host through
messages. `app.timer` runs automatically at the interval set in the script;
without `timer.start()`, it uses the fOS default of 60 seconds. State changes
appear live without overwriting active typing or slider movement.

**Run App** recompiles before each launch and saves changes to the script and
its associated open UI first. The runner stops when you stop the preview, close
it, or deactivate the extension. The output channel also shows live Serial output.

`fosrun` simulates hardware, network, and audio APIs. Audio is not played, and
the desktop audio model uses a sample file duration. Test real Wi-Fi, audio,
and ESP32 hardware behavior on the CrowPanel.

### Optional SD card image and custom tools

Set `fscript.sdRoot` to a folder containing your SD card structure, such as
`music/files` and `music/webradio/webradio.txt`. Relative paths use the open
workspace, or the app folder if no workspace is open. The runner enforces
storage permissions from `app.json`. It reads music lists and name/value station
files from this folder; other storage operations use the existing desktop
simulation model.

```json
{
  "fscript.compilerPath": "",
  "fscript.runnerPath": "",
  "fscript.sdRoot": "sd",
  "fscript.wifiOff": false,
  "fscript.checkOnSave": false
}
```

Empty tool paths select the bundled tools. Existing absolute tool paths remain
valid. Optionally set `fscript.cxxPath` to a custom C++17 compiler.

`fscript.checkOnSave=true` enables checks after saving. Prepare the tools once
first; an automatic save check does not initiate the first toolchain build.
Compiler and runner commands require a trusted workspace. Executables are
started without a shell, and paths containing spaces are supported.

### Using the tools in a terminal

If the tools are available on PATH, run these commands from your app folder:

```bash
fosc check main.fscript --ui layout.ui
fosc build main.fscript -o main.fapp --ui layout.ui
fosrun main.fapp --ui layout.ui --serve
```

Open `http://127.0.0.1:8765` in a browser and stop the runner with **Ctrl+C**.
Add `--sd-root "/path/to/sd-card"` to use a local SD card image. If the tools
are not on PATH, use their full executable paths. The VS Code commands locate
the bundled tools automatically; preparing them does not add them to PATH.

## Source code and packaging

The download includes complete source code in `source`. `src/catalog.json`
describes the API; `src/language.js` provides language logic; `src/extension.js`
registers VS Code providers. `src/toolchain.js` manages the tools and
`src/runner.js` manages the preview and its message bridge. The toolchain sources
are in `toolchain`, TextMate grammars in `syntaxes`, and templates in `snippets`.
`examples/radio` contains the radio example with English labels.

For development, open `source` in VS Code, press **F5**, and open a fScript project
in the Extension Development Host.

Tests (Node.js 18 or later):

```bash
npm test
```

Build the VSIX without downloading packages (Python 3.9 or later):

```bash
python3 scripts/build_vsix.py
```

Optionally rebuild and bundle native tools for the current platform:

```bash
node scripts/build_tools.js
python3 scripts/build_vsix.py
```

You can also package the source with Microsoft's `@vscode/vsce`. The included
VSIX is a standard VSIX 2.0 ZIP with `Microsoft.VisualStudio.Code.Manifest`
metadata for local installation.

## Validation

The test suite covers language assistance, compiler and runner paths, caching,
real fosc builds, start/stop behavior through mocked VS Code providers, webview
messages, VM events, and automatic `app.timer` handlers. Both bundled tools
were built and tested on Linux x64. Package contents and VSIX XML were checked.
The graphical VS Code installation and preview could not be tested here because
this environment has no VS Code or browser runtime. macOS and Windows require
validation on their target systems.

The implementation uses official APIs:
[Language features](https://code.visualstudio.com/api/language-extensions/programmatic-language-features),
[Syntax highlighting](https://code.visualstudio.com/api/language-extensions/syntax-highlight-guide),
[VSIX packages](https://code.visualstudio.com/api/working-with-extensions/publishing-extension).
