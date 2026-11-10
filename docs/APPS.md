# Apps auf der SD-Karte

Den gewünschten Ordner aus `apps` nach `/apps` auf die SD kopieren. Die
enthaltenen `main.fapp`-Dateien sind fertig kompiliert; ein eigener Build ist
nur nach Änderungen an `main.fscript` nötig. Eigene `appdata`-Ordner beibehalten.

Relative Layouts: [UI_LAYOUTS.md](UI_LAYOUTS.md). Ordnerrechte und Updates:
[APP_RUNTIME.md](APP_RUNTIME.md). Compiler: [FOSC_FOSRUN.md](FOSC_FOSRUN.md).

| Ordner | Funktion |
| --- | --- |
| `fscript_calculator` | Taschenrechner |
| `fscript_counter` | Zähler |
| `fscript_drawing` | Zeichnen über Touch |
| `fscript_weather` | Wetter und Standortsuche; WLAN erforderlich |
| `radio` | fScript-Radio mit lokalen Dateien und Webradio |
| `text` | Texteditor mit Zugriff auf `/text` |
| `button_demo`, `hello_fos`, `ui_demo` | UI-Beispiele |
| weitere Demo-Ordner | Beispiele für native App-Typen und Textinhalte |

## Taschenrechner und Zeichnen

Der Taschenrechner liegt einschließlich Quellcode und kompiliertem Programm
unter `apps/fscript_calculator`. Die Zeichen-App nutzt `canvas`,
`pointer_down`, `pointer_move`, `pointer_up`, `event_x()` und `event_y()`.
Ihr Zeichenpuffer wird während der laufenden App im PSRAM gehalten.

## Radio

Die fScript-Radio-App liegt unter `apps/radio` und verwendet `type=ui`.
Datei-Player und Webradio haben getrennte Tabs; Start/Stopp, Lautstärke,
Unterordnernavigation, Fortschritt und lokales Spulen sind enthalten.
`radio_demo` ist der separate native Radio-Eintrag.

| Inhalt | SD-Pfad |
| --- | --- |
| Musikdateien und Unterordner | `/music/files` |
| Senderliste | `/music/webradio/webradio.txt` |

Die Senderliste verwendet pro Zeile `Name|URL`. Vorhandene Musik und
Senderkonfiguration beim Kopieren der Apps beibehalten. Die Radio-App hat die
eigene App-Version 2.0.2; das System bleibt fOS 4.1.0.
[Bedienung und Audio-/UI-APIs](FSCRIPT_RADIO_AUDIO.md).

## Texteditor

- `Open New File` leert den Editor und behält den bisherigen Dateinamen.
- `Save as . . .` öffnet Dateinamenfeld und Tastatur. Eine fehlende `.txt`-Endung
  wird ergänzt; vorhandene Dateien werden überschrieben. Unterordner innerhalb
  von `/text` können beim Speichern angelegt werden.
- `Open file` zeigt Ordner vor Dateien und `..` zum übergeordneten Verzeichnis.
  Der Browser bleibt auf `/text` begrenzt. Erfolgreiches Laden schließt den Dialog.
- `Cancel` schließt den Dialog und führt die Tastatur zum Editor zurück.
- Die App nutzt dieselben Dateien unter `/text` wie der native Editor.

Die Rechte stehen in `app.json`:

```json
"permissions": ["ui", "storage.read:/text", "storage.write:/text"]
```

Textarea-Dateimethoden laden bis 4000 Byte; größere Texte werden mit
`[Datei gekuerzt]` markiert. Verzeichnislisten können bis zu 16384 Byte an
Optionsnamen aufnehmen und beachten die Manifest-Leserechte.

## Eigene Änderungen kompilieren

Zuerst fosc wie in [FOSC_FOSRUN.md](FOSC_FOSRUN.md) bauen. Danach beispielsweise
im Projekt-Hauptordner die Wetter-App neu erzeugen und verifizieren:

```sh
fScript_Development_Environment/fosc/build/fosc build apps/fscript_weather/main.fscript --ui apps/fscript_weather/layout.ui -o apps/fscript_weather/main.fapp
fScript_Development_Environment/fosc/build/fosc verify apps/fscript_weather/main.fapp
```

Die Wetter-App benötigt WLAN für Standortsuche und Wetterdaten.
