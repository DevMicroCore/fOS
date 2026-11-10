> Abgleich mit dem Quellcode: 2026-10-21. Paketstand: fOS 4.1.0, fosc 0.14.0, Bytecode 1.8. Relative Größen stehen in [UI_LAYOUTS.md](UI_LAYOUTS.md); gemeinsame Ordnerrechte in [APP_RUNTIME.md](APP_RUNTIME.md).

# fScript-, fosc- und VM-Handbuch

Stand: fOS 4.1.0, fScript 1.0.0, fosc 0.14.0, fosrun 0.2.1, fAPP 1.0, Bytecode 1.8

Dieses Handbuch beschreibt den vollständigen derzeit implementierten Umfang
von fScript, dem PC-Compiler `fosc` und der fAPP-VM in fOS. Es ist zugleich
Sprachreferenz, Bauanleitung und technische Laufzeitdokumentation.

## 1. Überblick

Eine fScript-App besteht aus bearbeitbaren Quelldateien auf dem PC und einer
kompilierten Datei für den ESP32:

```text
main.fscript + layout.ui + app.json
             |
             | fosc check/build
             v
          main.fapp
             |
             | auf die SD-Karte kopieren
             v
      /apps/meine_app/ auf fOS
```

`main.fscript` enthält Programmlogik, `layout.ui` beschreibt die Oberfläche,
`app.json` enthält Metadaten und Berechtigungen. `fosc` prüft und kompiliert die
Quellen auf einem Mac, Linux- oder Windows-PC. fOS lädt `main.fapp` anschließend
von der SD-Karte und führt ihn kooperativ in einer begrenzten virtuellen
Maschine aus. Ein Fehler beendet nur die betroffene App, nicht fOS.

## 2. Schnellstart auf dem Mac

### 2.1 Voraussetzungen

Einmalig die Apple Command Line Tools installieren:

```bash
xcode-select --install
```

Danach in den Compilerordner wechseln:

```bash
cd ~/Downloads/fOS4.1.0/fScript_Development_Environment/fosc
```

Liegt der entpackte Ordner woanders, den Pfad entsprechend anpassen. Ein Ordner
kann nach `cd ` auch aus dem Finder in das Terminal gezogen werden.

### 2.2 Compiler bauen und testen

```bash
make
make test
./build/fosc --version
```

Erwartete Version:

```text
fosc 0.14.0
fScript 1.0.0
```

### 2.3 Eine App prüfen und bauen

Im App-Ordner liegen beispielsweise `main.fscript`, `layout.ui` und
`app.json`:

```bash
./build/fosc check /Pfad/zur/App/main.fscript --ui /Pfad/zur/App/layout.ui
./build/fosc build /Pfad/zur/App/main.fscript \
  --ui /Pfad/zur/App/layout.ui \
  -o /Pfad/zur/App/main.fapp
./build/fosc verify /Pfad/zur/App/main.fapp
```

Anschließend den gesamten App-Ordner nach `/apps` auf der SD-Karte kopieren.

## 3. App-Verzeichnis und Manifest

Empfohlener Aufbau:

```text
/apps/wetter/
├── app.json
├── app.cfg          # optionaler Legacy-Fallback
├── layout.ui
├── main.fscript     # Quellcode; für fOS nicht zwingend erforderlich
├── main.fapp        # kompilierter Bytecode
├── state.txt        # optionale App-Daten
└── sound.mp3        # optionale Audiodatei
```

fOS liest bis zu **40** direkte App-Unterordner aus `/apps` ein. Davon getrennt sind die **7** Home-/Launcher-Plätze. App-Daten gehören vorzugsweise in `appdata`; Store-Updates erhalten diesen Ordner.

### 3.1 `app.json`

```json
{
  "id": "de.beispiel.meine-app",
  "name": "Meine App",
  "version": "1.0.0",
  "min_fos": "4.1.0",
  "type": "ui",
  "icon": "APP",
  "layout": "layout.ui",
  "executable": "main.fapp",
  "scrollable": false,
  "permissions": ["ui", "network"]
}
```

| Feld | Pflicht | Bedeutung |
| --- | --- | --- |
| `id` | ja | Eindeutige ID; Buchstaben, Ziffern, `.`, `-`, `_` |
| `name` | ja | Anzeigename im Launcher |
| `version` | ja | Exakte SemVer `MAJOR.MINOR.PATCH` |
| `min_fos` | nein | Kleinste erlaubte fOS-Version |
| `type` | nein | Für fScript-Apps `ui`; Standard ist `ui` |
| `icon` | nein | Kurzer Launchertext |
| `layout` | nein | UI-Datei; Standard `layout.ui` |
| `executable` | nein | Bytecode; Standard `main.fapp` |
| `scrollable` | nein | Vertikales Scrollen des App-Containers |
| `permissions` | nein | Explizite Fähigkeiten der App |

Das Manifest darf höchstens 4096 Byte groß sein. `layout` und `executable`
müssen einfache Dateinamen ohne `/`, `\` oder `..` sein. Ist ein vorhandenes
Manifest ungültig, startet fOS die App nicht. Fehlt es, kann `app.cfg` als
Legacy-Metadatei verwendet werden.

`id`, `name`, `layout` und `executable` sind auf je 64 Byte begrenzt;
`version`, `min_fos`, `type` und `icon` auf je 16 Byte. Sobald ein
`permissions`-Array vorhanden ist, ersetzt es die Standardberechtigung. Eine
App mit Oberfläche muss `"ui"` daher ausdrücklich in dieses Array aufnehmen.

### 3.2 Berechtigungen

| Berechtigung | Operationen |
| --- | --- |
| `ui` | UI-Eigenschaften, Methoden und Ereignisse |
| `storage.read` | Lesen/Auflisten im App-Ordner |
| `storage.write` | Schreiben im App-Ordner |
| `storage.read:/path` | Ordner und Unterordner lesen/auflisten |
| `storage.write:/path` | Ordner und Unterordner schreiben |
| `network` | HTTP/JSON, URL-Encoding, WLAN-Status und Audiostreams |
| `audio` | Lokale Wiedergabe und Audiosteuerung; Streams zusätzlich network |
| `system.restart` | Neustart anfordern |

Ordnerrechte verwenden absolute SD-Pfade. `/` erlaubt Zugriff auf die gesamte
SD-Karte **außer `/system`**, das einschließlich Unterordnern immer geschützt
bleibt. Lesen und Schreiben separat freigeben; Schreibrecht enthält kein Leserecht.
Ohne Pfad gelten Speicherrechte nur für den eigenen App-Ordner. `audio` erlaubt
lokale Wiedergabe aus der App; für andere Ordner ist eine passende Lesefreigabe nötig.
Maximal 8 Ordnerfreigaben mit zusammen 512 Pfadbytes einschließlich Terminatoren.

```json
"permissions": ["ui", "storage.read:/text", "storage.write:/text"]
```

Datei- und UI-Lademethoden prüfen dieselben Ordnerrechte. Fehlende Rechte führen zu einem VM-Fehler oder bei UI-Dateimethoden zu `false`; `/system` wird bei Root-Auflistungen ausgelassen. Der App-Store erhält `appdata` bei Updates, eine ausdrückliche Deinstallation entfernt die App samt Daten. Einzelheiten: [APP_RUNTIME.md](APP_RUNTIME.md).

### 3.3 Legacy-Datei `app.cfg`

```ini
name=Meine App
icon=APP
type=ui
layout=layout.ui
fapp=main.fapp
scrollable=false
```

Unterstützte Schlüssel sind `name`, `icon`, `content`, `layout`, `fapp`,
`type`, `scrollable`, `button_text` und `button_message`. Für neue fScript-Apps
ist `app.json` maßgeblich, weil nur dort Berechtigungen deklariert werden.

## 4. `layout.ui`

Jede nichtleere Zeile erzeugt ein Objekt; `;` trennt Felder und Zeilen mit `#` sind Kommentare. Keine String-Quoting-/Escape-Syntax für Semikolons.

### 4.1 Unterstützte Objekte

| `type` | Verwendung |
| --- | --- |
| `label` | Textanzeige |
| `button` | Schaltfläche mit Text |
| `textarea` | Ein-/mehrzeilige Texteingabe |
| `switch` | Ein/Aus-Schalter |
| `checkbox` | Kontrollkästchen |
| `panel` | Container |
| `roller` | Rollbare Auswahlliste |
| `dropdown` | Aufklappliste |
| `keyboard` | Textarea-Tastatur |
| `canvas` | RGB565-Zeichenfläche im PSRAM |
| `slider` | Ganzzahliger Regler |
| `tabview` | Tab-Steuerung, aktiver Index in .value |
| `tab` | Seite einer vorher erzeugten tabview |

Nur Objekte mit `id=` sind aus fScript erreichbar. IDs müssen eindeutige fScript-Bezeichner sein. Die bis zu 64 numerischen IDs folgen der Reihenfolge benannter Objekte im Layout; nach Reihenfolgeänderungen `main.fapp` neu bauen. Ein fehlgeschlagenes Canvas-/Keyboard-Objekt behält seinen reservierten ID-Platz. `parent` muss bereits existieren, sonst wird das Objekt im App-Hauptbereich erzeugt. `target` einer Tastatur wird dagegen **nach** dem gesamten Layout aufgelöst und darf später stehen.

### 4.2 Felder und Größen

| Feld | Bedeutung |
| --- | --- |
| `type` | Objekttyp aus obiger Liste |
| `id` | Eindeutiger fScript-Bezeichner; optional für reine Anzeige |
| `x, y` | Position: Pixel, px oder %; Standard 0 |
| `w, h` | Größe: Pixel, px oder %; Standard 220/50 |
| `parent` | ID eines vorher erzeugten Containers |
| `text` | Text/Optionen; \n und \t werden dekodiert |
| `bg, fg` | RGB-Hexfarbe; theme/accent/surface/contrast/text und Theme-Aliase |
| `font` | 20, 24, 40; andere Werte verwenden Objekt-Standard |
| `align` | Textausrichtung left/center/right (label, roller) |
| `radius, border` | Eckenradius und Rahmenbreite in Pixeln |
| `clickable, scrollable, hidden` | Boolesche Objektflags; false/true, 0/1, no/yes, off/on |
| `placeholder, one_line, max_length` | textarea; max_length wird für 1–1024 gesetzt |
| `native_style` | textarea/roller: nativer LVGL-Stil; button mit bg=theme: dynamische Themebindung |
| `styleless` | panel: LVGL-Grundstil entfernen |
| `value` | switch/checkbox: Boolean; slider: Startwert |
| `min, max` | slider: Standard 0/100; -32768…32767 und min < max |
| `tab_height` | tabview: Pixel oder % der Tabview-Höhe; Standard 50 |
| `target` | keyboard: Textarea-ID; darf später im Layout stehen |

`x`/`w` in Prozent beziehen sich auf die **Inhaltsbreite des Elternobjekts**,
`y`/`h` auf dessen Inhaltshöhe. `tab_height` bezieht sich auf die eigene
Tabview-Höhe. Dezimalpunkt verwenden (`12.5%`), nicht Dezimalkomma. Ohne Einheit
oder mit `px` gelten Pixel; auf den nächsten Pixel runden. Ungültige Angaben,
nicht endliche Zahlen und Werte außerhalb ±32767 fallen auf den Standard zurück.
Die Auflösung erfolgt einmal beim Öffnen; kein automatisches laufendes Reflow.
Farben, Schriftgrößen, Radien und Stiftbreiten bleiben Pixel-/Farbwerte.

```text
type=tabview;id=tabs;x=0%;y=18%;w=100%;h=82%;tab_height=15%;font=20
type=tab;id=page;parent=tabs;text=Controls
type=slider;id=volume;parent=page;x=5%;y=10%;w=90%;h=8%;min=0;max=21;value=10
```

`tab` muss eine vorher erzeugte `tabview` als Elternobjekt haben. Seine
Position und Größe verwaltet die Tabview; x/y/w/h sind dort keine freie Geometrie.
`switch` und `checkbox` verwenden die nativen Widgetgrößen statt w/h. `label`
berücksichtigt h, wenn ausdrücklich gesetzt. `max_length` ist ein optionaler
Eingabelimit-Setter, kein Limit von `textarea.load_file()` oder VM-Strings.

Tastaturen sind begrenzte Overlays im App-Bereich, damit kleine Dialoge keine
Tasten abschneiden. Standard w ist die Elternbreite, h = 197 Pixel und y =
Elternhöhe minus h. Fokussieren des Ziels zeigt die Tastatur. Verborgene Eltern
oder Ziele verbergen sie; `.hidden` hält den expliziten Skriptzustand fest.
Maximal 8 Tastaturen pro App.

Canvas benötigt PSRAM für `2 × Breite × Höhe` Byte RGB565. Der CrowPanel-Loader
erlaubt höchstens 800×480, der P4-Loader 1024×600. Der Puffer wird beim Löschen
freigegeben. Fehlende PSRAM-Zuweisung wird diagnostiziert; spätere IDs bleiben
stabil. Zeichnen und Pointer-Koordinaten verwenden echte lokale Pixel.

[UI_LAYOUTS.md](UI_LAYOUTS.md) enthält weitere Hinweise zu beiden Displaygrößen.

## 5. fScript-Sprachreferenz

### 5.1 Bezeichner, Groß-/Kleinschreibung und Zeilen

Bezeichner beginnen mit Buchstabe oder `_`; danach sind auch Ziffern erlaubt:

```fscript
var temperatur_1 = 21.5
var _intern = true
```

Variablen-, Funktions- und UI-Namen sind groß-/kleinschreibungsabhängig.
Schlüsselwörter werden klein geschrieben. Zeilenumbrüche trennen Anweisungen;
ein Semikolon ist optional:

```fscript
var a = 1
var b = 2; var c = 3
```

### 5.2 Kommentare

```fscript
-- Lua-artiger Zeilenkommentar
// JavaScript-artiger Zeilenkommentar
/* Blockkommentar
   über mehrere Zeilen */
```

Blockkommentare werden nicht verschachtelt.

### 5.3 Werte und Literale

| Typ | Beispiele | Laufzeitdarstellung |
| --- | --- | --- |
| `nil` | `nil` | kein Wert |
| Boolean | `true`, `false` | Wahrheitswert |
| Integer | `0`, `-42`, `100000` | vorzeichenbehaftet, 32 Bit |
| Float | `3.14`, `1.5e-3` | 64-Bit-Fließkomma |
| String | `"Hallo"`, `'Text'` | maximal 255 Nutzbytes |

Unterstützte String-Escapes sind `\n`, `\r`, `\t`, `\\`, `\"` und `\'`.
Strings dürfen nicht direkt über mehrere Quellzeilen gehen.

Nur `nil` und `false` gelten zur Laufzeit als falsch. Alle anderen Werte sind
wahr. Die statische Prüfung verlangt für `if`, `and`, `or` und `not` dennoch
bekannte Boolean-Werte.

### 5.4 Variablen und Gültigkeitsbereiche

```fscript
var counter = 0
var status

function increment(step)
  var next = counter + step
  counter = next
  return counter
end
```

Top-Level-Variablen sind global und bleiben über UI-Ereignisse und
`app.theme_changed` erhalten. Variablen innerhalb von Funktionen und Events
sind lokal. Eine Deklaration ohne Initialwert beginnt mit `nil`.

Der Typ wird aus der ersten sinnvollen Zuweisung abgeleitet. Danach sind nur
kompatible Zuweisungen erlaubt; Integer kann einem Float-Ziel zugewiesen werden,
aber nicht umgekehrt.

### 5.5 Funktionen

```fscript
function add(a, b)
  return a + b
end

var result = add(4, 5)
```

Funktionen dürfen vor oder nach ihrem Aufruf deklariert sein. Parameter sind
dynamisch typisiert; Anzahl der Argumente und Parameter muss übereinstimmen.
Funktionen und Eventhandler sind nur auf oberster Ebene erlaubt. Rekursive
Aufrufe sind technisch möglich, aber die VM begrenzt die Aufruftiefe auf 8.

`return` ohne Wert liefert `nil`. Auf oberster Ebene ist `return` verboten.
Eventhandler dürfen nur `return` ohne Ergebnis verwenden.

### 5.6 Bedingungen

```fscript
if temperature > 30 then
  lbl_status.text = "Heiß"
else
  lbl_status.text = "Normal"
end
```

`else` ist optional. Zusätzliche Zweige können mit `elseif` formuliert werden:

```fscript
if value < 0 then
  lbl_status.text = "Klein"
elseif value == 0 then
  lbl_status.text = "Null"
else
  lbl_status.text = "Groß"
end
```

Schleifen verwenden wie `if` ein abschließendes `end`. `while` erwartet eine
boolesche Bedingung:

```fscript
while counter < 10 then
  counter = counter + 1
end
```

`for` zählt inklusiv vom Startwert bis zum Endwert. Mit `var` wird die
Zählvariable direkt für die aktuelle Funktion beziehungsweise den aktuellen
Handler deklariert. Ohne `var` muss sie bereits existieren. `step` ist optional
und standardmäßig `1`; negative Schritte zählen abwärts.

```fscript
for var i = 1 to 10 step 2 then
  if i == 5 then
    continue
  elseif i == 9 then
    break
  end
end
```

`break` verlässt die nächste umgebende Schleife. `continue` springt zum nächsten
Durchlauf; bei `for` zuerst zum Zählschritt, bei `while` zurück zur Bedingung.

### 5.7 Operatoren

| Gruppe | Operatoren | Beispiel |
| --- | --- | --- |
| Zuweisung | `=` | `counter = 1` |
| Oder | `or`, `||` | `ready or cached` |
| Und | `and`, `&&` | `online && enabled` |
| Gleichheit | `==`, `!=` | `value != nil` |
| Vergleich | `<`, `<=`, `>`, `>=` | `temperature >= 20` |
| Addition | `+` | `a + b`, `"Wert: " + a` |
| Subtraktion | `-` | `a - b` |
| Multiplikation | `*` | `a * b` |
| Division | `/` | `a / b`; Ergebnis Float |
| Modulo | `%` | `counter % 10` |
| Unär | `not`, `!`, `+`, `-` | `not ready`, `-value` |
| Gruppierung | `()` | `(a + b) * 2` |

Die Tabelle steht von niedriger zu höherer Bindung. Zuweisungen werden von
rechts nach links ausgewertet, die übrigen binären Operatoren von links nach
rechts. `+` verkettet Strings mit Skalaren deterministisch, zum Beispiel:

```fscript
lbl_status.text = "Temperatur: " + temperature + " °C"
```

Integerüberläufe, Division durch null und ein Stringergebnis über 255 Byte
werden als kontrollierte Laufzeitfehler behandelt. Ordnungsvergleiche zwischen
Strings werden derzeit zwar vom Compiler akzeptiert, aber noch nicht von der VM
ausgeführt und enden mit `runtime type error`. Für `<`, `<=`, `>` und `>=` sind
daher Zahlen zu verwenden; `==` und `!=` funktionieren auch mit Strings.

## 6. Ereignisse

```fscript
on object_id.event
  // statements
end
```

### 6.1 Systemereignisse

| Event | Wirkung |
| --- | --- |
| `app.start` | Nach Top-Level-Initialisierung |
| `app.close` | Begrenzter letzter Zeitschlitz beim Schließen |
| `app.theme_changed` | Theme geändert; Globals bleiben erhalten |
| `app.timer` | Wiederkehrend: 60000 ms Standard; timer.start ändert Intervall |

### 6.2 UI-Ereignisse

| Event | Objekte |
| --- | --- |
| `click`, `clicked` | alle benannten Objekte |
| `change`, `changed`, `value_changed` | textarea, switch, checkbox, roller, dropdown, slider, tabview |
| `pressed`, `released` | button, switch, checkbox, slider |
| `ready`, `cancel`, `cancelled` | keyboard |
| `pointer_down`, `pointer_move`, `pointer_up` | canvas, panel |

`change` ist Alias für `changed`. Ein LVGL-Wertwechsel stellt sowohl
`value_changed` als auch `changed` zu, falls Handler vorhanden sind. Pro
Objekt/Event nur ein Handler. Programmatisches Setzen von Sliderwerten erzeugt
kein entsprechendes Benutzerereignis. `pointer_up` wird auch bei Touch-Verlust
zugestellt, sofern der Eingangstreiber das Ereignis meldet.

`event_x()`/`event_y()` lesen den beim Ereignis gespeicherten Punkt relativ zur
linken oberen Objektecke. `event_screen_x()`/`event_screen_y()` sind logische
Displaykoordinaten. `event_pressed()` ist der gespeicherte Druckzustand. Nur
Pointer-Handler haben diesen Kontext; sonst sind Koordinaten 0 und pressed
false. Das ist kein allgemeines Live-Touch-Polling. `.pressed` liest dagegen
den aktuellen LVGL-Zustand. Pointerpunkte können außerhalb des Objekts liegen.

Die Warteschlange hat 8 Plätze. Aufeinanderfolgende pending `pointer_move`
desselben Objekts werden zusammengefasst. Bei voller Queue kann `pointer_up`
einen pending Move desselben Objekts ersetzen; ohne solchen Platz kann auch
dieses Event verloren gehen. Handler kurz halten und keine Vollständigkeit
aller Zwischenpunkte voraussetzen.

## 7. UI-Zugriff aus fScript

Alle UI-Zugriffe benötigen `ui`. Keine allgemeinen .x/.y/.w/.h-Eigenschaften.

### 7.1 Eigenschaften

| Eigenschaft | Typ | Objekte | Zugriff |
| --- | --- | --- | --- |
| `.text` | String | label, button, textarea, checkbox, roller, dropdown | read/write |
| `.value` | Boolean | switch, checkbox | read/write |
| `.value` | Integer | roller, dropdown, slider, tabview | read/write |
| `.checked` | Boolean | switch, checkbox | read/write |
| `.hidden`, `.enabled` | Boolean | all | read/write |
| `.pressed` | Boolean | all | read only |

Für Textarea-Inhalte `.text` verwenden. Der Compiler akzeptiert teilweise `.value` für Textareas, die Firmware implementiert dies aber nicht. Bei roller/dropdown setzt `.text` die gesamte Optionsliste (mit `\n`), liest jedoch die ausgewählte Option. `.value` ist ein nullbasierter Auswahl-/Tabindex bzw. Sliderwert. Sliderwerte sind signed 16-bit; für lange Medien einen skalierten Fortschritt 0–1000 verwenden.

### 7.2 Methoden

| Methode | Objekte | Rückgabe | Verhalten |
| --- | --- | --- | --- |
| `.clear()` | textarea | nil | Text leeren |
| `.focus() / .blur()` | button, textarea, switch, checkbox, roller, dropdown, slider, tabview | nil | Fokus setzen/entfernen; focus zeigt zugeordnete Tastatur |
| `.scroll_to_top() / .scroll_to_bottom()` | textarea, panel, roller | nil | Nach oben/unten scrollen |
| `.load_file(path)` | textarea | Boolean | Bis 4000 Byte + Kürzungsmarker; ui + Leserechte |
| `.save_file(path)` | textarea | Boolean | Speichern; Elternordner nur im erlaubten Schreibbereich erzeugen |
| `.load_dir(path, root)` | roller | Boolean | Ordner, Dateien und .. innerhalb root; ui + Leserechte |
| `.load_dir_filtered(path, root, extensions)` | roller | Boolean | Wie load_dir; Dateiendungen filtern, versteckte Einträge auslassen |
| `.load_pairs(path, separator)` | roller | Boolean | Namen aus Name/Wert-Datei direkt als Optionen; ui + Leserechte |
| `.set_range(min, max)` | slider | Boolean | Integer-Bereich -32768…32767, min < max |
| `.clear(color)` | canvas | Boolean | Gesamte Zeichenfläche füllen |
| `.pixel(x, y, color)` | canvas | Boolean | Ein Pixel |
| `.line(x1, y1, x2, y2, color, width)` | canvas | Boolean | Linie; width 1–64 |
| `.rect(x, y, w, h, color, filled)` | canvas | Boolean | Rechteck, w/h > 0, filled Boolean |
| `.circle(x, y, radius, color, filled)` | canvas | Boolean | Kreis um Mittelpunkt; radius > 0 |
| `.draw_text(x, y, text, color, font)` | canvas | Boolean | Text; Schrift 20/24/40, sonst Standardschrift |

Alle Canvas-Argumente sind Integer außer Text und dem Boolean `filled`. Farben im Script sind dezimale RGB-Zahlen 0–16777215 (z. B. Weiß = 16777215), nicht die Hex-Schreibweise der Layoutdatei. Koordinaten sind Pixel; Parameter begrenzen und nicht mit Prozentangaben an Zeichenmethoden übergeben.

Verzeichnis-/Dateimethoden benötigen zusätzlich Leserechte bzw. `save_file` Schreibrechte. `load_dir` umgeht das 255-Byte-Stringlimit mit bis zu 16384 Optionsbytes. Ordner stehen zuerst, dann Dateien; keine zugesicherte Sortierung innerhalb dieser Gruppen. `extensions` ist z. B. `.mp3,.wav,.ogg`, ohne Beachtung der Großschreibung. `load_pairs` und `file_pair_value` verwenden denselben Leser: ein Separatorzeichen, Split am ersten Separator, Felder trimmen, ungültige/leere/überlange Zeilen überspringen. Maximal 40 gültige Zeilen aus 16 KiB, Name/Wert je höchstens 255 Byte.

### 7.3 Canvas-Beispiel

```text
type=canvas;id=drawing;x=5%;y=20%;w=90%;h=70%;bg=0xFFFFFF
```

```fscript
on app.start
  drawing.clear(16777215)
  drawing.rect(10, 10, 50, 30, 255, false)
  drawing.line(0, 0, 40, 40, 0, 3)
  drawing.draw_text(15, 60, "Draw here", 0, 20)
end

on drawing.pointer_down
  drawing.circle(event_x(), event_y(), 2, 0, true)
end

on drawing.pointer_move
  if event_pressed() then
    drawing.pixel(event_x(), event_y(), 0)
  end
end
```

## 8. Native fOS-APIs

### 8.1 Übersicht

| Funktion | Rückgabe | Berechtigung | Verhalten |
| --- | --- | --- | --- |
| `http_get(url)` | Boolean | `network` | HTTP/HTTPS GET; true bei HTTP 200–299 |
| `http_status()` | Integer | `network` | Letzter HTTP-Status oder negativer Clientfehler |
| `http_json(path)` | String | `network` | Skalar aus letzter Antwort über JSON-Pfad |
| `http_text(offset)` | String | `network` | Bis 255 Byte ab Byteoffset |
| `url_encode(text)` | String | `network` | URL-Abfragewert kodieren |
| `date_weekday(date)` | String | — | ISO-Datum YYYY-MM-DD → Sun…Sat |
| `file_read(path)` | String | `storage.read` | Bis 255 Byte; leer bei fehlender/zu großer Datei |
| `file_write(path, data)` | Boolean | `storage.write` | Datei ersetzen; Elternordner muss existieren |
| `audio_play(path)` | Boolean | `audio + scope` | Lokale Datei; außerhalb der App zusätzliche Lesefreigabe |
| `wifi_status()` | Boolean | `network` | Verbindungsstatus, startet kein WLAN |
| `system.restart()` | nil | `system.restart` | Neustart nach aktuellem VM-Zeitschlitz anfordern |
| `timer.start(milliseconds)` | Boolean | — | Wiederkehrender App-Timer, Integer 100–86400000 ms |
| `Serial.printf(text)` | nil | — | Genau ein String; kein printf-Format mit weiteren Argumenten |
| `text(value)` | String | — | Skalar in Text umwandeln |
| `number_parse(text)` | Float | — | Zahl aus String; Dezimalpunkt/-komma |
| `round(number)` | Integer | — | Runden; halbe Werte von null weg |
| `string_length(text)` | Integer | — | Bytelänge, keine Unicode-Zeichenzahl |
| `string_slice(text, start, length)` | String | — | Bytebereich; nullbasierter Start |
| `math_eval(expression)` | String | — | Matheausdruck auswerten; Ergebnistext |
| `string_last_index(text, needle)` | Integer | — | Letzter Byteindex ab 0; -1 falls nicht gefunden |
| `file_list(path)` | String | `storage.read` | Direkte Einträge, durch Zeilenumbrüche getrennt; /system auslassen |
| `event_x()` | Integer | — | Lokale X-Koordinate des Pointer-Ereignisses |
| `event_y()` | Integer | — | Lokale Y-Koordinate des Pointer-Ereignisses |
| `event_screen_x()` | Integer | — | X-Koordinate auf dem logischen Display |
| `event_screen_y()` | Integer | — | Y-Koordinate auf dem logischen Display |
| `event_pressed()` | Boolean | — | Gedrückt-Zustand im Pointer-Snapshot |
| `audio_stream(url)` | Boolean | `audio + network` | HTTP/HTTPS-Stream; false bei fehlendem WLAN/OTA/Startfehler |
| `audio_stop()` | Boolean | `audio` | Audio der aktiven fApp beenden |
| `audio_set_volume(value)` | Boolean | `audio` | Gemeinsame Lautstärke; Integer 0–21 |
| `audio_volume()` | Integer | `audio` | Aktuelle Lautstärke |
| `audio_playing()` | Boolean | `audio` | Aktive Wiedergabe der fApp; false nach EOF |
| `audio_position()` | Integer | `audio` | Lokale Position in Sekunden, sonst 0 |
| `audio_duration()` | Integer | `audio` | Lokale Dauer in Sekunden; 0 wenn unbekannt/inaktiv/Stream |
| `audio_seek(seconds)` | Boolean | `audio` | Integer 0–65535 Sekunden; nur aktive lokale Datei |
| `file_pair_value(path, separator, index)` | String | `storage.read` | Wert der gültigen Name/Wert-Zeile ab Index 0; Separator ein Zeichen |

`scope` bezeichnet die zusätzlichen Ordnerrechte aus Kapitel 3.2. Die Pointer-Helfer sind in Kapitel 6 erklärt. Fehlende Fähigkeiten erzeugen `permission denied`, auch wenn ein nativer Aufruf sonst `false` zurückgeben könnte.

### 8.2 HTTP und JSON

```fscript
var ok = http_get("https://api.example.org/data.json")

if ok then
  var temperature = http_json("current.temperature")
  lbl_status.text = "Temperatur: " + temperature
else
  lbl_status.text = "HTTP-Fehler: " + http_status()
end
```

`http_get()` akzeptiert `http://` und `https://`, speichert die letzte Antwort
und liefert nur bei HTTP 200 bis 299 `true`. `http_status()` gibt den letzten
HTTP-Status oder negativen Clientfehler zurück. Verbindungsaufbau und Lesen sind
auf 5 bzw. 7 Sekunden begrenzt. Antworten dürfen höchstens 32768 Byte groß sein.
Während OTA das Netzwerk besitzt, liefert `http_get()` kontrolliert `false`.

`http_json()` liest skalare Werte mit Punkt- und Arraypfaden:

```fscript
var city = http_json("results[0].name")
var first_day = http_json("daily.time[0]")
```

String, Zahl, Boolean und `null` werden als fScript-String zurückgegeben. Ein
nicht vorhandener Pfad liefert `""`. Objekt- oder Arraywerte sind nicht direkt
auslesbar. Unicode-JSON-Escapes `\uXXXX` werden derzeit als `?` dargestellt.

`http_text(offset)` liefert ab dem Byteoffset maximal 255 Byte der Antwort:

```fscript
var first = http_text(0)
var next = http_text(255)
```

`url_encode()` kodiert einen Wert für eine URL-Abfrage:

```fscript
var place = url_encode("Nova Gorica")
var ok = http_get("https://example.org/search?q=" + place)
```

HTTPS verwendet derzeit Kompatibilitätsmodus ohne Zertifikatskettenprüfung.
HTTP ist synchron; längere Abrufe können die App bis zum Timeout anhalten.

### 8.3 Datum

```fscript
var day = date_weekday("2026-08-04")  // "Tue"
```

Erwartet wird `YYYY-MM-DD` ab 1970. Die Rückgabe ist `Sun`, `Mon`, `Tue`,
`Wed`, `Thu`, `Fri` oder `Sat`.

### 8.4 Dateien

```fscript
var old_state = file_read("data/state.txt")
var saved = file_write("data/state.txt", "counter=" + counter)
```

Pfade können absolut vom SD-Wurzelordner oder relativ zum aktiven App-Ordner
angegeben werden. Ordnerrechte in `app.json` verwenden `storage.read:/ordner`
und `storage.write:/ordner`; sie gelten einschließlich aller Unterordner.
`/` gibt die gesamte SD-Karte frei, außer `/system`: Dieser Ordner bleibt immer
für Lesen und Schreiben gesperrt. Die bisherigen Rechte `storage.read` und
`storage.write` ohne Pfadangabe bleiben auf den eigenen App-Ordner begrenzt.
Pfade werden vor der Rechteprüfung vereinheitlicht; `..` und andere Groß- oder
Kleinschreibung umgehen den Schutz nicht. Rückstriche, Steuerzeichen und
mehrdeutige abschließende Punkte/Leerzeichen werden abgewiesen.
`file_write()` benötigt bereits vorhandene Elternordner.

`file_read()` liest höchstens 255 Byte. Eine fehlende oder zu große Datei
liefert `""`; damit ist `""` nicht von einer tatsächlich leeren Datei zu
unterscheiden. `file_write()` ersetzt den Dateiinhalt und liefert den Erfolg.
`file_list()` liefert die direkten Einträge eines App-Unterordners, getrennt
durch Zeilenumbrüche; Ordnernamen enden mit `/`. Für Texteditoren umgehen
`textarea.load_file(path)` und `textarea.save_file(path)` das 255-Byte-Limit
der VM-Strings. Sie beachten dieselben Ordnerrechte; `save_file()` darf Elternordner
innerhalb des freigegebenen Schreibbereichs anlegen. `file_list("/")` lässt
den geschützten Eintrag `system` aus.

### 8.5 Audio, WLAN, Neustart, Timer und Serial

```fscript
var playing = audio_play("sounds/notify.mp3")
var online = wifi_status()
var timer_ok = timer.start(10000)
Serial.printf("Online: " + online + "\n")
```

`audio_play()` spielt eine vorhandene Datei über das bestehende fOS-Audiosystem.
`audio` erlaubt lokale Wiedergabe; andere SD-Ordner benötigen zusätzlich ein
passendes `storage.read:/ordner`-Recht. `/system` bleibt unzugänglich. Vorherige Wiedergabe wird beendet; Schließen der App
stoppt ihre Wiedergabe. Während OTA beginnt keine neue Wiedergabe.

`wifi_status()` prüft nur den aktuellen Zustand und startet keine Verbindung.
`timer.start()` setzt das wiederkehrende `app.timer`-Intervall auf 100 bis
86400000 Millisekunden. Ohne Aufruf bleiben 60000 Millisekunden aktiv.

`system.restart()` plant den Neustart. fOS führt ihn erst nach dem aktuellen
VM-Zeitschlitz aus. `Serial.printf()` erwartet genau einen bereits
zusammengesetzten String. Prozentzeichen werden sicher als Text behandelt:

```fscript
Serial.printf("Fortschritt: 100%\n")
```

Native C-Formatparameter wie `Serial.printf("%d", value)` werden bewusst nicht
unterstützt.

### 8.6 Text-, Zahlen- und Mathe-Helfer

```fscript
var value = number_parse("12,5")
lbl_result.text = "Wert: " + text(value)
lbl_round.text = "Gerundet: " + round(value)

var short_name = string_slice("Calculator", 0, 4)
var comma_pos = string_last_index("1,2+3,4", ",")
lbl_result.text = math_eval("2+3*4")
```

`text()` wandelt einen skalaren Wert deterministisch in Text um.
`number_parse()` akzeptiert Dezimalpunkt und Dezimalkomma; ungültiger Text wird
als `0.0` zurückgegeben. `string_length()` zählt Bytes, `string_slice()` liefert
einen begrenzten Ausschnitt und `string_last_index()` liefert den letzten Index
oder `-1`. `math_eval()` wertet `+`, `-`, `*`, `/` mit Operatorrangfolge aus,
akzeptiert Komma und Punkt und gibt `Math Error` als Text zurück, wenn der
Ausdruck ungültig ist oder durch null geteilt wird.


### 8.7 Audiostreams und begrenzte Konfigurationsdateien

`audio_stream(url)` benötigt `audio` und `network`. HTTP/HTTPS sind erlaubt;
ohne WLAN, während OTA oder bei Startfehlern wird false zurückgegeben. Lokales
`audio_play()` verwendet die Ordnerrechte. Schließen der App stoppt ihre
Wiedergabe. `audio_stop()` beendet statt pausiert; es gibt hier kein Resume.
Die Lautstärke 0–21 wird mit der nativen Audio-Engine geteilt. Position/Dauer
sind Sekunden; eine noch unbekannte Dauer ist 0. Spulen gilt nur für eine aktive
lokale Datei, mit Integer-Sekunden 0–65535. Streams haben keine Seek-Position.

`file_pair_value(path, separator, index)` liest den Wert aus denselben geprüften
Zeilen wie `roller.load_pairs()`. Index ab 0. Fehlende Dateien/Zeilen ergeben
leeren Text; unerlaubte native Pfade verursachen einen Fehler vor dem SD-Zugriff.
Passende `storage.read`-Freigabe ist nötig. Die konkrete Radio-Bedienung steht in
[FSCRIPT_RADIO_AUDIO.md](FSCRIPT_RADIO_AUDIO.md).

## 9. Alle `fosc`-Befehle

### `fosc --help`

Zeigt Befehle und Optionen:

```bash
./build/fosc --help
```

### `fosc --version`

```bash
./build/fosc --version
```

Zeigt Compiler- und Sprachversion.

### `fosc lex`

```bash
./build/fosc lex main.fscript
```

Führt nur den Lexer aus und zeigt Tokenart, Datei, Zeile, Spalte, Lexem und
dekodierten Literalwert. Nützlich bei ungültigen Zeichen, Strings oder
Kommentaren. Es entsteht keine Ausgabedatei.

### `fosc parse`

```bash
./build/fosc parse main.fscript
```

Führt Lexer und Parser aus und druckt den abstrakten Syntaxbaum (AST). UI-Namen
und Typen werden dabei noch nicht semantisch geprüft.

### `fosc check`

```bash
./build/fosc check main.fscript
./build/fosc check main.fscript --ui layout.ui
```

Prüft Lexer, Syntax, Variablen, Funktionen, Typen, UI-Objekte, Eigenschaften,
Methoden, Ereignisse und native API-Signaturen, schreibt aber keine `.fapp`.
Ohne `--ui` sucht `fosc` neben dem Script zuerst `main.ui`, danach
`layout.ui`. Eine UI-Datei ist optional, wenn kein UI-Objekt verwendet wird.

### `fosc build`

```bash
./build/fosc build main.fscript
./build/fosc build main.fscript --ui layout.ui -o main.fapp
./build/fosc build main.fscript --output main.fapp
./build/fosc build main.fscript --no-optimize -o debug.fapp
```

Vollständiger Ablauf:

1. Quelldatei lesen.
2. Token erzeugen.
3. AST parsen.
4. UI-Namen in deterministische numerische IDs übersetzen.
5. Semantik und Typen prüfen.
6. Konstante, nebenwirkungsfreie Ausdrücke standardmäßig falten.
7. Bytecode und Tabellen erzeugen.
8. fAPP-Header, Quellhash und CRC-32 schreiben.
9. Die eigene Ausgabe erneut einlesen und strukturell verifizieren.
10. Nur bei Erfolg die Zieldatei schreiben.

Ohne `-o` erhält die Quelldatei die Endung `.fapp`. `--no-optimize` deaktiviert
nur die konservative Konstantenfaltung; das Laufzeitverhalten soll gleich
bleiben.

### `fosc verify`

```bash
./build/fosc verify main.fapp
```

Prüft Magic, Versionen, Größe, Bereiche, CRC, Tabellen, Indizes, Opcodes,
Sprungziele, Stackverlauf, Funktionsenden und deklarierten Maximalstack. Es wird
nichts ausgeführt.

### `fosc disasm`

```bash
./build/fosc disasm main.fapp
```

Verifiziert die Datei zuerst und gibt anschließend Header, Konstanten,
Funktionen, Ereignisbindungen und lesbaren Bytecode aus. Damit lässt sich etwa
prüfen, ob `http_get` oder ein UI-Zugriff tatsächlich kompiliert wurde.

### Buildsystem-Befehle

```bash
make             # fosc und fosrun bauen
make test        # alle Host- und Runtime-Regressionstests
make clean       # Build-Ordner entfernen
```

Alternative mit CMake:

```bash
cmake -S . -B build-cmake
cmake --build build-cmake
ctest --test-dir build-cmake --output-on-failure
```

## 9.1 Alle `fosrun`-Befehle

`fosrun` ist ein PC-seitiger Runner für bereits kompilierte `.fapp` Dateien.
Er nutzt dieselbe VM wie fOS, simuliert aber UI-Objekte und native fOS-APIs.
Dadurch kann App-Logik schnell getestet werden, ohne den ESP32 neu zu flashen.
Ab Version 0.2.1 kann `fosrun` die `.ui` zusätzlich in einem lokalen Browser
anzeigen und klickbar machen.

```bash
./build/fosrun main.fapp --ui layout.ui
```

`app.start` wird standardmäßig automatisch ausgeführt. Danach gibt `fosrun` den
Endzustand aller benannten UI-Objekte aus.

Die Oberfläche im Browser laden:

```bash
./build/fosrun main.fapp --ui layout.ui --serve --open
```

Ohne `--open` diese Adresse manuell öffnen:

```text
http://127.0.0.1:8765/
```

Einen anderen Port verwenden:

```bash
./build/fosrun main.fapp --ui layout.ui --serve --port 9876
```

Ein Ereignis auslösen:

```bash
./build/fosrun main.fapp --ui layout.ui --event btn_ok.click
./build/fosrun main.fapp --ui layout.ui --event app.timer
./build/fosrun main.fapp --ui layout.ui --event keyboard_search.ready
```

Einen simulierten UI-Wert vor dem Ereignis setzen:

```bash
./build/fosrun main.fapp \
  --ui layout.ui \
  --set txt_search.text=Berlin \
  --event keyboard_search.ready
```

Simulierte `http_json()`-Antworten überschreiben:

```bash
./build/fosrun weather.fapp \
  --ui layout.ui \
  --json current.temperature_2m=19 \
  --json current.relative_humidity_2m=61
```

Weitere Optionen:

| Option | Wirkung |
| --- | --- |
| `--ui <file>` | Layoutdatei (erforderlich) |
| `--size WIDTHxHEIGHT` | Vorschaugröße, Standard 800x480; je Dimension 64–4096 |
| `--sd-root <directory>` | SD-Abbild lesend einbinden, Audio simulieren |
| `--no-start` | `app.start` nicht automatisch ausführen |
| `--no-dump` | finalen UI-Zustand nicht ausgeben |
| `--trace-native` | simulierte native API-Aufrufe anzeigen |
| `--wifi-off` | `wifi_status()` liefert `false` |
| `--serve` | lokale Browser-Oberfläche starten |
| `--port <nummer>` | Port für `--serve`, Standard `8765` |
| `--open` | Browser nach dem Start automatisch öffnen |

Grenze: Der Browsermodus ist keine pixelgenaue LVGL-Emulation. Er bildet die
`.ui`-Elemente als HTML-Controls ab und prüft die Logik hinter den Controls,
die Eventbindung und die UI-/Native-API-Zugriffe.

## 10. Wie der Compiler arbeitet

Der Lexer wandelt Zeichen in Tokens und bewahrt Datei, Zeile, Spalte und
Byteoffset. Der rekursive Parser erstellt daraus einen typisierten AST und kann
nach Syntaxfehlern weitere Anweisungen prüfen. Der Semantikpass sammelt zuerst
globale Symbole und Funktionssignaturen, danach analysiert er Initialisierung,
Funktionen und Events. Dadurch sind Vorwärtsaufrufe möglich.

Der Codegenerator nummeriert Globals, Locals, Funktionen und UI-Objekte. Namen
von UI-Elementen werden nicht in `main.fapp` gespeichert. Stringkonstanten
werden dedupliziert; Integer und Float stehen direkt in Instruktionen. Der
Optimizer berechnet sichere konstante Ausdrücke bereits auf dem PC. Abschließend
beweist der Bytecode-Verifier unter anderem, dass kein Sprung mitten in eine
Instruktion führt und alle Kontrollflusspfade mit kompatibler Stacktiefe
zusammenlaufen.

Compiler-Rückgabecodes:

| Code | Bedeutung |
| ---: | --- |
| 0 | Befehl erfolgreich |
| 1 | Quell-, Prüf-, Format- oder Buildfehler |
| 2 | fehlende oder ungültige Kommandozeilenoption bei `check`/`build` |

## 11. fAPP-Datei und Bytecode

Das fAPP-Format 1.0 ist little-endian und besitzt einen festen 80-Byte-Header.
Er enthält `FAPP`-Magic, Format- und Bytecodeversion, Dateigröße, CRC-32,
Abschnitte für Konstanten/Funktionen/Events/Code, Quellhash und minimale
fOS-Version. Bytecode 1.8 wird erzeugt; fOS 4.1.0 akzeptiert kompatible ältere
Minor-Versionen weiterhin.

| Offset | Größe | Headerfeld |
| ---: | ---: | --- |
| 0 | 4 | Magic `FAPP` |
| 4 | 2 | fAPP-Format Major/Minor |
| 6 | 2 | Bytecode Major/Minor |
| 8 | 2 | Headergröße, derzeit 80 |
| 10 | 2 | Flags |
| 12 | 4 | vollständige Dateigröße |
| 16 | 4 | CRC-32; dieses Feld wird bei der Berechnung als null behandelt |
| 20 | 8 | Offset und Größe des Konstantenabschnitts |
| 28 | 2 | Anzahl Konstanten |
| 30 | 2 | Anzahl Globals |
| 32 | 8 | Offset und Größe der Funktionstabelle |
| 40 | 2 | Anzahl Funktionen |
| 42 | 2 | Index der Initialisierungsfunktion |
| 44 | 8 | Offset und Größe der Eventtabelle |
| 52 | 2 | Anzahl Events |
| 56 | 8 | Offset und Größe des Codes |
| 64 | 4 | FNV-1a-Hash des Quellcodes |
| 68 | 6 | minimale fOS-Version |
| 74 | 6 | reserviert |

### 11.1 Instruktionssatz

| Gruppe | Opcodes |
| --- | --- |
| Werte | `PUSH_NIL`, `PUSH_FALSE`, `PUSH_TRUE`, `PUSH_INT`, `PUSH_FLOAT`, `PUSH_STRING` |
| Stack | `NOP`, `POP`, `DUP` |
| Variablen | `LOAD_GLOBAL`, `STORE_GLOBAL`, `LOAD_LOCAL`, `STORE_LOCAL` |
| UI | `GET_UI_PROPERTY`, `SET_UI_PROPERTY`, `CALL_UI_METHOD` |
| Aufrufe | `CALL_FUNCTION`, `CALL_NATIVE`, `RETURN` |
| Unär | `NEGATE`, `NOT` |
| Rechnen | `ADD`, `SUBTRACT`, `MULTIPLY`, `DIVIDE`, `MODULO` |
| Vergleich | `EQUAL`, `NOT_EQUAL`, `LESS`, `LESS_EQUAL`, `GREATER`, `GREATER_EQUAL` |
| Logik | `AND`, `OR` |
| Kontrolle | `JUMP`, `JUMP_IF_FALSE` |

Native Funktions-IDs wurden bis Bytecode 1.8 nur ergänzt; bestehende IDs bleiben stabil. Die vollständige aktuelle Tabelle lautet:

| ID | Native Funktion |
| ---: | --- |
| 1 | `http_get` |
| 2 | `http_status` |
| 3 | `http_json` |
| 4 | `http_text` |
| 5 | `url_encode` |
| 6 | `date_weekday` |
| 7 | `file_read` |
| 8 | `file_write` |
| 9 | `audio_play` |
| 10 | `wifi_status` |
| 11 | `system.restart` |
| 12 | `timer.start` |
| 13 | `Serial.printf` |
| 14 | `text` |
| 15 | `number_parse` |
| 16 | `round` |
| 17 | `string_length` |
| 18 | `string_slice` |
| 19 | `math_eval` |
| 20 | `string_last_index` |
| 21 | `file_list` |
| 22 | `event_x` |
| 23 | `event_y` |
| 24 | `event_screen_x` |
| 25 | `event_screen_y` |
| 26 | `event_pressed` |
| 27 | `audio_stream` |
| 28 | `audio_stop` |
| 29 | `audio_set_volume` |
| 30 | `audio_volume` |
| 31 | `audio_playing` |
| 32 | `audio_position` |
| 33 | `audio_duration` |
| 34 | `audio_seek` |
| 35 | `file_pair_value` |

## 12. VM-System in fOS

### 12.1 Ladeweg

1. fOS liest `app.cfg` als Fallback und danach `app.json` als maßgebliche Quelle.
2. `layout.ui` wird erzeugt und die Registry mit numerischen Objekt-IDs gefüllt.
3. `FAppLoader` prüft Dateipfad, Header, Version, Abschnittsbereiche,
   Mindest-fOS-Version und CRC streaming mit einem 256-Byte-Puffer.
4. Die VM liest Metadaten und reserviert Wertestapel/-speicher bevorzugt im
   PSRAM, sonst im internen RAM.
5. Top-Level-Code läuft als Initialisierungsfunktion.
6. Danach wird `app.start` zugestellt und die UI-Eventbridge aktiviert.

### 12.2 Zustände und Lebenszyklus

| Zustand | Bedeutung |
| --- | --- |
| `Stopped` | keine App aktiv, Speicher freigegeben |
| `Ready` | geladen, wartet auf Ereignis |
| `Running` | eine Funktion oder ein Event wird ausgeführt |
| `Faulted` | App wegen eines kontrollierten Fehlers beendet |

Die Hauptschleife gibt der VM pro Update standardmaessig hoechstens 256
Instruktionen oder 4000 Mikrosekunden. Danach erhält fOS wieder Kontrolle über
LVGL, Audio, WLAN und OTA. Events werden in einer FIFO-Warteschlange gesammelt;
die Kapazität beträgt 8. Eine volle Warteschlange verwirft weitere Events; Pointer-Moves können jedoch zusammengefasst und durch Pointer-Up ersetzt werden (Kapitel 6).

Bei einem Themewechsel bleiben Globals und Ausführungszustand erhalten. Beim
Schließen erhält `app.close` maximal 512 Instruktionen bzw. 5000 Mikrosekunden.
Danach werden Runtimewerte, HTTP-Antwort, Registry und LVGL-Appobjekte
freigegeben. Ein Laufzeitfehler leert Aufruf- und Eventzustand und zeigt eine
Fehlermeldung innerhalb der App an.

### 12.3 Feste Ressourcenlimits

| Ressource | Grenze |
| --- | ---: |
| benannte UI-Objekte | 64 |
| Stringkonstanten | 128 |
| Stringnutzlänge | 255 Byte |
| Funktionen einschließlich Init/Events | 48 |
| Events | 64 |
| Globals | 32 |
| Operandenstack | 64 Werte |
| lokale Slots über aktive Aufrufe | 96 |
| Aufruftiefe | 8 |
| Eventwarteschlange | 8 |
| `layout.ui` | 32000 Zeichen |
| `app.json` | 4096 Byte |
| HTTP-Antwort | 32768 Byte |

| Ressource | Grenze |
| --- | --- |
| Installed apps / home slots | 40 / 7 |
| Keyboard overlays | 8 |
| Folder grants / path storage | 8 / 512 bytes |
| Canvas buffer | 2 × width × height bytes; PSRAM |
| Canvas maximum | 800×480 CrowPanel; 1024×600 P4 |
| Slider range | -32768…32767 |
| Textarea load_file | 4000 bytes + truncation marker |
| Roller directory options | 16384 bytes |
| Name/value configuration | 16384 bytes; 40 valid rows |
| Executable cache | 65536 bytes; larger executable falls back to file reads |


### 12.4 VM-Fehler

| Meldung | Typische Ursache |
| --- | --- |
| `executable not prepared` | Loader hat keine gültige fAPP |
| `runtime resource limit exceeded` | Stack, Locals, Aufruftiefe, String oder Metadaten zu groß |
| `executable read failed` | SD-/Dateilesefehler |
| `invalid executable metadata` | inkonsistente Tabellen oder Argumentzahl |
| `invalid bytecode instruction` | unbekannter Opcode oder ungültiges Sprungziel |
| `operand stack underflow/overflow` | beschädigter oder fehlerhafter Bytecode |
| `runtime type error` | dynamischer Wert passt nicht zur Operation |
| `division by zero` | `/` oder `%` mit null |
| `invalid runtime index` | ungültiger Global-/Local-/Funktionsindex |
| `permission denied` | Berechtigung fehlt im Manifest |
| `UI operation failed` | Eigenschaft/Methode passt nicht zum LVGL-Objekt |
| `native API unavailable`/API-Text | native API nicht verfügbar oder Eingabe ungültig |

## 13. Diagnosecodes von `fosc`

| Bereich | Bedeutung |
| --- | --- |
| `FS000` | Datei nicht lesbar |
| `FS001–FS005` | Lexer: Zeichen, String, Escape, Kommentar, Exponent |
| `FS201–FS205` | Parser: unerwartetes/fehlendes Token, Ziel, Anzahl, Eventsyntax |
| `FS301–FS303` | UI-ID ungültig, doppelt oder mehr als 64 |
| `FS304–FS330` | Codegenerator: Symbole, Limits, Operatoren, UI/API-Aufrufe |
| `FS401–FS405` | Schreiben des fAPP-Formats |
| `FS411–FS419` | Lesen, Version, CRC, Abschnitte und Tabellen |
| `FS420–FS422` | CLI-Option, Ausgabe und interne Buildverifikation |
| `FS501–FS520` | Semantik: Namen, Typen, Funktionen, Events, UI und native APIs |
| `FS601–FS608` | Bytecode: Opcode, Index, Arity, Sprung, Stack und Kontrollfluss |

Eine Diagnose nennt Datei, Zeile und Spalte. Zuerst den frühesten Fehler
beheben; spätere Meldungen können Folgefehler sein.

## 14. Vollständige kleine Beispiel-App

Dieses vollständige Beispiel liegt zusätzlich unter `docs/examples/counter`. Die folgenden Shell-Befehle werden in `fScript_Development_Environment/fosc` ausgeführt.

`layout.ui`:

```text
type=label;id=lbl_status;x=20%;y=18%;w=75%;h=12%;text=Count: 0;font=24;fg=0xFFFFFF
type=button;id=btn_add;x=20%;y=40%;w=30%;h=15%;text=+1;bg=theme
type=button;id=btn_reset;x=60%;y=40%;w=30%;h=15%;text=Reset;bg=theme
```

`main.fscript`:

```fscript
var counter = 0

function show_counter()
  lbl_status.text = "Count: " + counter
end

on app.start
  show_counter()
  timer.start(10000)
end

on btn_add.click
  counter = counter + 1
  show_counter()
end

on btn_reset.click
  counter = 0
  show_counter()
end

on app.timer
  Serial.printf("Count: " + counter + "\n")
end
```

`app.json`:

```json
{
  "id": "org.example.counter",
  "name": "Counter",
  "version": "1.0.0",
  "min_fos": "4.1.0",
  "type": "ui",
  "icon": "+1",
  "layout": "layout.ui",
  "executable": "main.fapp",
  "scrollable": false,
  "permissions": [
    "ui"
  ]
}
```

```sh
./build/fosc check ../../docs/examples/counter/main.fscript --ui ../../docs/examples/counter/layout.ui
./build/fosc build ../../docs/examples/counter/main.fscript --ui ../../docs/examples/counter/layout.ui -o ../../docs/examples/counter/main.fapp
./build/fosc verify ../../docs/examples/counter/main.fapp
./build/fosrun ../../docs/examples/counter/main.fapp --ui ../../docs/examples/counter/layout.ui --size 800x480 --event btn_add.click
./build/fosrun ../../docs/examples/counter/main.fapp --ui ../../docs/examples/counter/layout.ui --size 1024x600 --event btn_add.click
```

Erwarteter Labeltext: `Count: 1`. Den Ordner `counter` mit der erzeugten `main.fapp` nach `/apps/counter` kopieren. Nur Geometrieänderungen ohne Änderung der benannten Objektfolge benötigen keinen neuen Bytecode; Script- oder ID-Änderungen dagegen neu bauen und verifizieren.

## 15. Derzeitige Grenzen

fScript hat keine Arrays, Maps, Klassen, Imports/Module, Threads oder
Ausnahmebehandlung. HTTP bleibt synchron und kann die Oberfläche bis zum
Timeout verzögern; ein VM-Zeitbudget unterbricht keinen laufenden nativen Aufruf.
Zahlenoperationen `< <= > >=` auf Strings sind nicht zuverlässig implementiert;
Zahlen verwenden. UI-Koordinaten/Größen sind nicht als Script-Properties zugänglich.
Die Browser-Vorschau simuliert native APIs, spielt kein Audio und ist keine
pixelgenaue LVGL-Emulation. Hardware-, Touch-, WLAN- und Codec-Verhalten am Board prüfen.

Die fScript-APIs sind auf beiden Boards gleich. P4-USB-Massenspeicher, RTC-/VBAT-
Steuerung und der BOOT-Batterieruhezustand sind Firmwarefunktionen, keine hier
exponierten fScript-Befehle. Während USB die P4-SD besitzt, pausiert fOS lokale
Apps/Dateiarbeit und App-Timer. Siehe [WAVESHARE.md](WAVESHARE.md).

Neue APIs und Tabellen wurden mit dem enthaltenen Compiler/Runtime-Quellstand
abgeglichen. Weitere Anleitungen stehen im [Dokumentationsindex](INDEX.md).


### Quellen dieses Abgleichs

- [Compiler-API-IDs und Signaturen](../fScript_Development_Environment/fosc/bytecode/Bytecode.cpp)
- [Semantische UI-Prüfungen](../fScript_Development_Environment/fosc/semantic/SemanticAnalyzer.cpp)
- [Runtime-Grenzen und Queue](../fOS_Waveshare_P4/src/fapp/FAppRuntime.h)
- [UI-Adapter](../fOS_Waveshare_P4/src/fapp/LvglFAppUiApi.cpp)
- [Ordnerrechte](../fOS_Waveshare_P4/src/fapp/FAppStoragePolicy.h)
- [Kopierbares Zähler-Beispiel](examples/counter/main.fscript)

CrowPanel verwendet dieselbe fScript-API mit LVGL 8 statt LVGL 9.
Die Hauptsketches erstellen das Layout und setzen die Board-spezifischen Größenlimits.
