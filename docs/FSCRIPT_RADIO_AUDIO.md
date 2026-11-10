> Alle Paketpfade und Befehle beziehen sich auf den Projektordner `fOS4.1.0`, sofern kein anderes Arbeitsverzeichnis genannt ist.

# fOS 4.1.0 – Radio als fScript-App / allgemeine Audio- und UI-APIs

Die App setzt fOS **4.1.0** voraus (`min_fos=4.1.0`, auch im fAPP-Header).
Die App liegt unter `apps/radio/` und ist eine echte `type=ui`-fApp:
`layout.ui` baut die Widgets, `main.fscript` enthält die Bedienlogik und
`main.fapp` ist bereits kompiliert. Sie ruft keinen nativen Radio-Renderer auf.
Die vorhandene fest eingebaute Radio-App bleibt zum Vergleich verfügbar.

## Installation

1. Die Firmware aus diesem Projekt mit den bisherigen Board-/Bibliothekseinstellungen
   neu kompilieren und auf das jeweilige Board flashen. Nur das Kopieren der App reicht
   nicht: ältere Firmware kennt die neuen APIs bzw. Bytecode 1.8 noch nicht.
2. Den kompletten Ordner `apps/radio` nach `/apps/radio` auf die SD kopieren.
3. Den Launcher neu laden bzw. fOS neu starten und die App **Radio** öffnen.
   Die vorhandene `radio_demo` ist weiterhin die native Radio-App.

Die gemeinsam genutzten Datenpfade bleiben exakt gleich:

| Inhalt | SD-Pfad |
| --- | --- |
| Musik und Unterordner | `/music/files` |
| Senderkonfiguration | `/music/webradio/webradio.txt` |

Die vorhandenen Musikdateien und die Konfiguration weder verschieben noch durch
Beispieldateien ersetzen. Die App benötigt ausschließlich Lesezugriff auf `/music`:
`permissions=["ui","audio","network","storage.read:/music"]`.
Sie schreibt keine Einstellungen in die gemeinsamen Datenverzeichnisse.
`/system` bleibt wie bei allen fApps geschützt.

Senderformat, unverändert zur eingebauten App:

```text
MDR Sachsen|https://dein-bisheriger-stream
Hit Radio RTL|https://dein-bisheriger-stream
```

Leere Zeilen und ungültige Einträge werden übersprungen, Name und URL werden
getrimmt; maximal 40 gültige Sender. Die Namen gelangen direkt in den Roller,
auch wenn die gesamte Konfigurationsdatei länger als die 255-Byte-VM-Strings ist.
Die Konfiguration wird beim App-Start gelesen; nach einer Änderung die App neu öffnen.

## Bedienung

Die Geometrie wurde aus `ui_Screen1.c` in AppContent-Koordinaten übertragen:
Start/Stopp oben, Lautstärke, Fortschritt und darunter die Tabs **File Player** /
**Web Radio** mit jeweils einem Roller. Menü und App-Container stellt fOS bereit.
Die Widgets verwenden das vorhandene LVGL-/fOS-Systemtheme.
Bei Tabviews folgt `LV_PART_ITEMS | LV_STATE_CHECKED` mit `BORDER_COLOR`/
`BORDER_OPA` der Themefarbe. Bei Rollern folgt `LV_PART_SELECTED |
LV_STATE_DEFAULT` mit `BG_COLOR`/`BG_OPA` ebenfalls der Themefarbe, auch mit
`native_style=true`. Die Bindungen aktualisieren sich bei System-Themewechseln.

Der Dateibrowser beginnt in `/music/files`. Einen Ordner auswählen und `>` drücken;
`..` führt zurück, jedoch nie über diesen Musikordner hinaus. Angezeigt werden
Unterordner und `.mp3`, `.wav`, `.ogg`, `.aac`, `.m4a` (auch Großbuchstaben).
Versteckte Dateien werden ausgelassen. Für die unterstützten Codecs gelten dieselben
Grenzen der installierten Audio-Bibliothek wie bei der nativen App.

`>` startet die ausgewählte Datei bzw. den Sender. `||` beendet die Wiedergabe,
wie in der eingebauten Radio-App; der nächste Start beginnt wieder am Anfang.
Ein Tabwechsel allein beendet die laufende Wiedergabe nicht.
Die Lautstärke ist 0–21 und übernimmt beim Öffnen den aktuellen Engine-Wert.
Der Fortschritt wird alle 200 ms aktualisiert, solange der Regler nicht gedrückt ist.
Loslassen (auch Touch-Verlust) spult lokale Dateien. Beim Webradio ist Spulen deaktiviert.
Das Dateiende setzt den Button wieder auf `>`; Schließen beendet App-Audio.

Der Fortschritt verwendet 0–1000 statt die Dateidauer direkt im LVGL-Regler abzulegen.
Damit funktionieren auch Laufzeiten über 32767 Sekunden ohne signed-16-bit-Überlauf.
Die Audio-Bibliothek kann über ihre vorhandene `setAudioPlayTime(uint16_t)`-API
bis 65535 Sekunden anspringen; größere Ziele werden auf diesen Wert begrenzt.

## Allgemeine native Befehle (Bytecode 1.8)

| Befehl | Ergebnis | Rechte / Verhalten |
| --- | --- | --- |
| `audio_play(path)` | Boolean | Bestehend; `audio`; SD-Pfad muss durch den Storage-Scope lesbar sein |
| `audio_stream(url)` | Boolean | `audio` **und** `network`; HTTP/HTTPS, false bei fehlendem WLAN, OTA oder Startfehler |
| `audio_stop()` | Boolean | `audio`; beendet das Audio der aktiven fApp |
| `audio_set_volume(value)` | Boolean | `audio`; Integer 0–21 |
| `audio_volume()` | Integer | `audio`; aktueller gemeinsamer Lautstärkewert |
| `audio_playing()` | Boolean | `audio`; aktive Wiedergabe der fApp, bei EOF false |
| `audio_position()` | Integer | `audio`; lokale Position in Sekunden, bei Streams/inaktiver App 0 |
| `audio_duration()` | Integer | `audio`; lokale Gesamtdauer in Sekunden, sonst 0; kann anfangs unbekannt sein |
| `audio_seek(seconds)` | Boolean | `audio`; Integer 0–65535; nur aktive lokale Dateien |
| `file_pair_value(path, separator, index)` | String | `storage.read` mit passendem Pfad-Scope; Wert einer gültigen Name/Wert-Zeile, Index ab 0 |

`file_pair_value` und `roller.load_pairs` sind allgemeine Befehle für Konfigurationen,
kein spezieller Radio-Loader. Separator: ein Zeichen, z. B. `|` oder `=`.
Beide verwenden denselben Leser und dieselben Indizes: Split am ersten Separator,
Trimmen beider Felder, leere/ungültige Zeilen überspringen, maximal 40 gültige Zeilen.
Grenzen: 16 KiB je Datei und 255 Bytes je Name bzw. Wert; überlange Felder werden
übersprungen. Fehlende Datei/Zeile liefert einen leeren Wert; unerlaubte native
Pfadzugriffe erzeugen den bestehenden Berechtigungsfehler vor jedem SD-Zugriff.

## Allgemeine UI-Erweiterungen

| UI | Verwendung |
| --- | --- |
| `type=slider;min=0;max=21;value=15` | Regler mit Integer-`value`; `pressed` ist lesbar |
| `slider.set_range(min, max)` | Dynamischer Bereich; Integer, -32768–32767 und min < max |
| `type=tabview;tab_height=50` | Tabview, `value` ist der aktive Tabindex ab 0 |
| `type=tab;parent=<tabview-id>;text=...` | Seite in einer zuvor deklarierten Tabview |
| `roller.load_dir_filtered(path, root, extensions)` | Unterordner, `..` innerhalb root und Dateien mit passenden Endungen |
| `roller.load_pairs(path, separator)` | Laden der Namen einer Name/Wert-Datei direkt als Optionen |
| `object.pressed` | Boolean, nur lesbar; aktueller LVGL-Pressed-Zustand |

`extensions` ist z. B. `.mp3,.wav,.ogg,.aac,.m4a`; Vergleich ohne Beachtung der
Groß-/Kleinschreibung. `load_dir` bleibt mit seinen bisherigen zwei Argumenten
unverändert verfügbar. Beide neuen Roller-Lademethoden benötigen `ui` und
`storage.read` mit passendem Pfad-Scope und liefern Boolean.
Slider unterstützen `changed`, `value_changed`, `pressed`, `released`;
Tabviews `changed` / `value_changed`. Programmatische Slider-Updates lösen
keine Lautstärke-/Seek-Schleifen aus.

Neue numerische IDs wurden nur angehängt. Bestehende fApps bis Bytecode 1.7
sind weiterhin ladbar; Compiler 0.14.0 erzeugt Bytecode 1.8 mit Mindest-Firmware 4.1.0.

## Selbst kompilieren / am PC prüfen

Im Verzeichnis `fScript_Development_Environment/fosc`:

```bash
make
build/fosc build "../../fOS4.0/example app/apps/radio/main.fscript" \
  --ui "../../fOS4.0/example app/apps/radio/layout.ui" \
  -o "../../fOS4.0/example app/apps/radio/main.fapp"
build/fosc verify "../../fOS4.0/example app/apps/radio/main.fapp"
build/fosrun "../../fOS4.0/example app/apps/radio/main.fapp" \
  --ui "../../fOS4.0/example app/apps/radio/layout.ui" \
  --sd-root "../../fOS4.0/example app" --serve --open
```

`--sd-root` bindet einen lokalen SD-Abbild-Ordner für Musiklisten und
Senderkonfiguration **lesend** ein; Symlink-Ausbrüche werden zurückgewiesen.
`fosrun` zeigt Slider und Tabs und simuliert Audiozustand/Seek (feste Beispieldauer
180 s), spielt jedoch keine Audiodateien/Streams ab. Für Timerprüfungen das Event
`app.timer` auslösen; Audioqualität und echte Stream-/Codec-Unterstützung am Board testen.

## Prüfung

- Compiler, Bytecode-Verifier, Embedded-Loader, VM, Rechte-/Manifest- und native API-Tests.
- Vollständiger Firmware-Layout-Loader mit echter LVGL 8.3.11 und kompilierter Radio-fApp:
  UI-Geometrie, Tabs, Ordnernavigation/Filter, 40 Sender mit ungültigen Zeilen,
  Start/Stopp, Lautstärke, EOF, Fortschritt/Dragging, Seek, lange Dateien und App-Schließen.
- Bestehende Text-/Wetter-/Keyboard-Regressionen laufen im selben LVGL-Test weiter.
- Auf dem CrowPanel konnten Flashen, tatsächliche Tonwiedergabe und WLAN-Streams
  in dieser Umgebung nicht geprüft werden.
