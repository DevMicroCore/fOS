# 1.1.1

- Translate all extension commands, settings, notifications, progress messages,
  hover help, API documentation, UI diagnostics, and snippet descriptions to English.
- Translate the README, installation instructions, changelog, bundled toolchain
  notes, and radio example labels to English.
- Translate empty-list messages in the bundled runner and rebuild Linux x64 tools.
- Preserve command IDs and settings so existing installations update in place.

# 1.1.0

- Bundle fosc and fosrun source code with a versioned tool cache.
- Add automatic local C++17 builds for macOS, Linux, and MinGW-w64, plus prebuilt
  Linux x64 tools.
- Add start/stop commands and an interactive app preview in a VS Code webview.
- Compile before launch and show Serial and runner output in the output channel.
- Run app timers automatically and update live state without overwriting active input.
- Support a local SD card image, offline Wi-Fi simulation, and custom tool paths.
- Clean up processes on stop, preview closure, and extension deactivation.

# 1.0.0

- Add syntax highlighting for fScript and fOS UI.
- Document 35 native fOS 4.0.1 commands with signatures and permissions.
- Add widget-specific properties, methods, events, and UI ID suggestions.
- Add hover help, parameter hints, definitions, outline, and block folding.
- Add UI templates, a color picker, and basic UI diagnostics.
- Support optional fosc checks and compilation with diagnostics in Problems.
- Provide a universal VSIX and complete source code.
