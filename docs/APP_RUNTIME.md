# App-Rechte, Daten und Tastaturen

## App-Daten bei Updates

Der AppStore aktualisiert die App-Dateien, ohne den vorhandenen Datenordner
`/apps/<Appname>/appdata` zu ersetzen oder dessen Inhalte zu verändern.

Beispiel auf der SD-Karte:

```text
/apps/meine_app/main.fapp
/apps/meine_app/layout.ui
/apps/meine_app/app.json
/apps/meine_app/appdata/settings.json
```

Eine fScript-App kann mit den vorhandenen Dateibefehlen und den passenden
`fs.read`-/`fs.write`-Rechten relative Pfade wie `appdata/settings.json` verwenden.
Die bisherigen Manifest- und Ordnerrechte gelten weiterhin.
Ein separater Ordner `/appdata` direkt auf der SD-Karte wird vom AppStore
ebenfalls nicht angefasst, da App-Installationen nur in `/apps` stattfinden.

## Verhalten

- Bei einem Update werden Dateien unter `appdata` aus dem Repository weder
  heruntergeladen noch als neue Standarddaten installiert. Das gilt für den
  Store-Index und den GitHub-Contents-API-Fallback, auch für `AppData`/`APPDATA`
  auf der FAT-formatierten SD-Karte.
- Die neuen App-Dateien werden zunächst vollständig in einen temporären
  Ordner geladen. Erst danach wird die alte App gesichert und die neue App
  installiert. Der vorhandene Datenordner wird per Verzeichnisumbenennung
  übernommen; auch tief verschachtelte Daten bleiben dadurch erhalten.
- Bei einer Erstinstallation können mitgelieferte Standarddaten installiert
  werden. Fehlt ein Datenordner, wird ein leerer `appdata`-Ordner angelegt.
- App-Dateien, die in der neuen Version nicht mehr enthalten sind, werden
  wie bisher entfernt. Die Home-Screen-Zuweisung bleibt erhalten.
- Scheitert die Installation oder die Übernahme des Datenordners, wird die
  alte App wiederhergestellt. Scheitert auch diese Wiederherstellung, bleiben
  die alten App-Dateien und Daten im Sicherungsordner
  `/apps/.store_backup_<Appname>` erhalten. Ein weiterer Update-Versuch
  überschreibt diese Sicherung nicht, solange sie `appdata` enthält.
- Die ausdrückliche Deinstallation über den Löschen-Button bleibt unverändert
  und entfernt weiterhin den gesamten App-Ordner inklusive seiner Daten.


## Dateisystemrechte

## Ganze SD-Karte

```json
"permissions": [
  "ui",
  "storage.read:/",
  "storage.write:/"
]
```

Damit sind Lesen, Schreiben und Auflisten der SD-Karte erlaubt. `/system`
und sein gesamter Inhalt bleiben IMMER fuer fApps gesperrt. Die Sperre gilt
auch bei anderer Gross-/Kleinschreibung und nach Aufloesung von `..`.
`/system` wird beim Auflisten des Wurzelordners nicht zurueckgegeben.
`/system_backup` ist ein anderer, freigebbarer Ordner.

## Ausgewaehlte Ordner

```json
"permissions": [
  "ui",
  "storage.read:/",
  "storage.write:/text",
  "storage.write:/apps/meine_app/files"
]
```

Diese App darf ueberall ausserhalb von `/system` lesen, aber nur innerhalb
von `/text` und `/apps/meine_app/files` schreiben. `/text_backup` wird durch
`storage.write:/text` nicht freigegeben. Schreibrechte enthalten keine
Leserechte: Bei Bedarf beide Eintraege setzen.

Die bisherigen Eintraege `storage.read` und `storage.write` ohne Doppelpunkt
bleiben kompatibel und erlauben nur den eigenen App-Ordner samt Unterordnern.
Sie koennen mit den neuen Ordnerfreigaben kombiniert werden. Ein reines
`storage.write:/text` erlaubt daher nicht automatisch das Schreiben im
eigenen App-Ordner. `audio` erlaubt wie bisher lokale Wiedergabe; fuer Audio
an anderen SD-Pfaden wird zusaetzlich die passende `storage.read:/...`-Freigabe
benoetigt.

Ordnerrechte muessen absolute Pfade verwenden. Eine ausdrueckliche Freigabe
von `/system` oder einem Unterordner wird als ungueltiges Manifest abgewiesen.
Unterstuetzt werden bis zu acht Ordnerfreigaben mit zusammen maximal 512 Byte
Pfadtext einschliesslich Terminatoren. Normale Rechte wie `ui` zaehlen nicht
zu diesem Limit. Die Pfadpruefung verwendet keine dynamische Speicherbelegung;
Ordnerfreigaben werden nur fuer die aktive App vorgehalten.

## Pfade in fScript

```fscript
var entries = file_list("/")
var content = file_read("/text/notiz.txt")
var written = file_write("/text/notiz.txt", "Hallo")
var loaded = txt_editor.load_file("/text/notiz.txt")
var saved = txt_editor.save_file("/text/notiz.txt")
```

Absolute Pfade beginnen am SD-Wurzelordner. Relative Pfade bleiben relativ
zum aktiven App-Ordner: `files/notiz.txt` bezeichnet weiterhin
`/apps/<app>/files/notiz.txt`. Wiederholte Schraegstriche sowie `.` und `..`
werden vor der Rechtepruefung vereinheitlicht; die Dateisystemfunktionen sehen
nur den aufgeloesten Pfad. Rueckstriche, Steuerzeichen und mehrdeutige
Dateinamen mit fuehrenden Leerzeichen oder abschliessenden Punkten/Leerzeichen
werden abgewiesen.

Die Grenzen fuer Dateiinhalte bleiben unveraendert: `file_read()` liefert
hoechstens 255 Byte, Textarea-Dateimethoden erlauben weiterhin groessere Texte.
`file_write()` benoetigt einen existierenden Elternordner. `save_file()` kann
fehlende Elternordner innerhalb des freigegebenen Schreibbereichs anlegen;
fehlende Vorfahren ausserhalb der Freigabe muss fOS vorher anlegen.

Bei unzulaessigen Pfaden liefern Textarea-Dateimethoden `false`. Native
Datei-/Audio-Aufrufe melden einen Fehler; die VM behandelt ihn wie bisher
als einen abgefangenen App-Fehler. Keine dieser Fehlerstrecken oeffnet oder
veraendert die geschuetzte Datei.

## Verwendung

Die enthaltenen Haupt-Sketches und Haupt-Images unterstützen diese Rechte
bereits. Nach Anpassungen an `app.json` die App bzw. den Launcher neu laden.
Ein konkretes Beispiel ist die Text-App mit `storage.read:/text` und
`storage.write:/text`. Vorhandene fAPP-Bytecode-Dateien bleiben kompatibel.

fosrun liest ein `app.json` neben dem Layout ebenfalls ein und prueft dieselben
Ordnerrechte. Sein Dateisystem bleibt eine Simulation; es schreibt damit keine
Datei auf die echte SD-Karte oder den Rechner.


## Tastaturen aus layout.ui


Nur Tastaturen aus `layout.ui` werden vom neuen `FAppKeyboardController`
verwaltet. Die nativen Tastaturen fuer WLAN, E-Mail, den eingebauten Texteditor
und andere bestehende fOS-Funktionen verwenden ihren bisherigen Code.

- Tastaturen liegen im sichtbaren App-Bereich. Ihre Layout-Eltern bestimmen
  weiterhin Koordinaten und Sichtbarkeit, koennen die Tasten aber nicht mehr
  mit einem kleinen Dialogrand abschneiden.
- `target=` wird nach dem vollstaendigen Laden der UI aufgeloest und als
  Textarea validiert. Die Reihenfolge von Tastatur und Textfeld ist beliebig.
- Anklicken, `focus()`, `hidden`, Ready und Cancel bleiben erhalten.
- Verborgene Eltern und Textfelder verbergen auch die zugeordnete Tastatur.
  Die von fScript gesetzte `hidden`-Eigenschaft bleibt dabei erhalten.
- Ungueltige Bildschirmpositionen werden in den App-Bereich begrenzt.
- Die Tastatur wird beim Oeffnen in den Vordergrund geholt. Sie wird nicht in
  jeder Schleifenrunde erneut ueber spaeter geoeffnete Dateidialoge gelegt.
- Beim Entladen werden die Bindungen entfernt; geloeschte Eltern/Textfelder
  hinterlassen keine Tastatur mit einem ungueltigen Textfeldzeiger.
- Numerische UI-IDs und vorhandene fAPP-Dateien bleiben kompatibel. Es werden
  wie bisher hoechstens acht Tastaturen verwaltet. Beim Ueberschreiten bleibt
  die Nummerierung spaeterer UI-Objekte trotzdem korrekt.


Weiterführend: [fScript-Handbuch](FSCRIPT_FOSC_VM_HANDBUCH_DE.md).
