# Bundled fOS toolchain

Sources from the provided fOS 4.0.1 project: fosc 0.14.0 and fosrun using the
actual fOS VM, with manifest and storage permission checks. The extension builds
both tools locally and stores them in its VS Code cache. No SD or ESP32 libraries
are required; desktop stubs are included. The runner's local HTTP transport
supports POSIX and Winsock. `src/toolchain.js` uses `build.json`; a C++17 compiler
is sufficient. The bundled runner uses English empty-list messages.
