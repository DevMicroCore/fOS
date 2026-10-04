# fScript-, fosc- und VM-Handbuch

Stand: fOS 4.0.0, fScript 1.0.0, fosc 0.12.1, fosrun 0.2.1, fAPP 1.0, Bytecode 1.5

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
cd ~/Downloads/fOS4.0/fScript_Development_Environment/fosc
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
fosc 0.12.1
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

fOS liest derzeit maximal sieben direkte Unterordner aus `/apps` in den
Launcher ein.

### 3.1 `app.json`

```json
{
  "id": "de.beispiel.meine-app",
  "name": "Meine App",
  "version": "1.0.0",
  "min_fos": "4.0.0",
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

| Berechtigung | Erlaubte Operationen |
| --- | --- |
| `ui` | UI lesen, verändern und UI-Methoden aufrufen |
| `storage.read` | `file_read()`, `file_list()` |
| `storage.write` | `file_write()` |
| `network` | HTTP/JSON, URL-Encoding und `wifi_status()` |
| `audio` | `audio_play()` |
| `system.restart` | `system.restart()` |

`date_weekday()`, `timer.start()` und `Serial.printf()` benötigen keine
zusätzliche Berechtigung. Fehlende Berechtigungen führen zu einem kontrollierten
VM-Fehler `permission denied`; fOS selbst läuft weiter.

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

Jede nichtleere Zeile beschreibt ein Objekt. Felder werden mit Semikolon
getrennt; Zeilen mit `#` sind Kommentare.

```text
type=button;id=btn_ok;x=40;y=100;w=240;h=70;text=OK;bg=theme;fg=contrast
```

### 4.1 Unterstützte Objekte

| `type` | Verwendung |
| --- | --- |
| `label` | Textanzeige |
| `button` | Schaltfläche mit Text |
| `textarea` | Ein- oder mehrzeilige Texteingabe |
| `switch` | Ein/Aus-Schalter |
| `checkbox` | Kontrollkästchen mit Text |
| `panel` | Container für untergeordnete Objekte |
| `roller` | Rollbare Auswahlliste |
| `dropdown` | Aufklappbare Auswahlliste |
| `keyboard` | Bildschirmtastatur für eine Textarea |

Nur Objekte mit `id=` sind aus fScript erreichbar. Die IDs müssen gültige
fScript-Bezeichner sein und eindeutig bleiben. Unterstützte benannte Objekte
bekommen in Dateireihenfolge die numerischen IDs 1 bis 64. Deshalb darf die
Reihenfolge nach dem Kompilieren nicht geändert werden, ohne `main.fapp` neu zu
bauen.

### 4.2 Allgemeine UI-Felder

| Feld | Bedeutung / Werte |
| --- | --- |
| `type` | Objekttyp |
| `id` | fScript-Name; optional für rein visuelle Objekte |
| `x`, `y` | Position, Standard 0 |
| `w`, `h` | Breite/Höhe, Standard 220/50 |
| `text` | Text bzw. Optionen; `\n` und `\t` werden dekodiert |
| `bg`, `fg` | Hexfarbe, z. B. `0x2196F3` |
| `bg`, `fg` | außerdem `theme`, `accent`, `surface`, `contrast`, `text` |
| `font` | derzeit 20, 24 oder 40 |
| `align` | `left`, `center`, `right` |
| `parent` | ID eines vorher definierten Panels |
| `radius` | Eckenradius |
| `border` | Rahmenbreite |
| `clickable` | `true/false`, `1/0`, `yes/no`, `on/off` |
| `scrollable` | Scrollfähigkeit des Objekts |
| `hidden` | Anfangs unsichtbar |

Objektspezifische Felder:

| Objekt | Zusätzliche Felder |
| --- | --- |
| `textarea` | `placeholder`, `one_line`, `max_length` (maximal 1024) |
| `switch`, `checkbox` | `value=true` für anfangs aktiviert |
| `panel` | `styleless=true` entfernt den LVGL-Grundstil |
| `keyboard` | `target=textarea_id`; Ziel muss vorher angelegt sein |
| `roller`, `dropdown` | Optionen in `text`, getrennt durch `\n` |

Eine Tastatur mit `target=` wird beim Antippen oder Fokussieren der Ziel-Textarea
automatisch sichtbar gemacht und in den Vordergrund verschoben. Das Script kann
die Tastatur zusätzlich mit `keyboard_id.hidden` steuern.

Die wichtigsten Darstellungsfelder werden derzeit wie folgt angewendet:

| Objekt | Wirksame Darstellungsfelder |
| --- | --- |
| `label` | `x`, `y`, `w`, `text`, `fg`, `font`, `align` |
| `button` | `x`, `y`, `w`, `h`, `text`, `bg`, `fg`, `font` |
| `textarea` | `x`, `y`, `w`, `h`, `text`, `bg`, `fg`, `font` plus Eingabefelder |
| `switch` | `x`, `y`, `value`; Größe und Farben folgen dem fOS-Stil |
| `checkbox` | `x`, `y`, `text`, `fg`, `value` |
| `panel` | `x`, `y`, `w`, `h`, `bg`, `styleless` |
| `roller`, `dropdown` | `x`, `y`, `w`, `h`, `text`, `bg`, `fg`, `font` |
| `keyboard` | `x`, `y`, `w`, `h`, `font`, `target` |

`id`, `parent`, `radius`, `border`, `clickable`, `scrollable` und `hidden`
werden nach der Objekterzeugung allgemein verarbeitet. Nicht zur Objektart
passende Felder werden ignoriert.

Beispiel mit Suchpanel und Tastatur:

```text
type=panel;id=panel_search;x=10;y=10;w=760;h=420;bg=0x101418
type=textarea;id=txt_search;parent=panel_search;x=15;y=15;w=570;h=55;placeholder=Ort suchen;one_line=true
type=button;id=btn_search;parent=panel_search;x=600;y=15;w=140;h=55;text=Suchen;bg=theme
type=keyboard;id=keyboard_search;parent=panel_search;target=txt_search;x=0;y=190;w=740;h=220
```

`parent` und `target` werden beim zeilenweisen Aufbau nur unter bereits
erzeugten benannten Objekten gesucht. Elternobjekt und Ziel müssen deshalb
oberhalb des abhängigen Objekts stehen.

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

Syntax:

```fscript
on objekt_id.ereignis
  // Anweisungen
end
```

### 6.1 Systemereignisse

| Ereignis | Zeitpunkt |
| --- | --- |
| `app.start` | Nach der Top-Level-Initialisierung |
| `app.close` | Beim Schließen; begrenztes letztes Ausführungsfenster |
| `app.theme_changed` | Nach einem fOS-Themewechsel, ohne VM-Neustart |
| `app.timer` | Wiederkehrend, standardmäßig alle 60 Sekunden |

```fscript
on app.start
  timer.start(5000)
end

on app.timer
  Serial.printf("Timer ausgelöst\n")
end
```

### 6.2 UI-Ereignisse

| Ereignis | Geeignete Objekte |
| --- | --- |
| `click` oder `clicked` | alle benannten UI-Objekte |
| `change` oder `changed` | textarea, switch, checkbox, roller, dropdown |
| `value_changed` | textarea, switch, checkbox, roller, dropdown |
| `pressed` | button, switch, checkbox |
| `released` | button, switch, checkbox |
| `ready` | keyboard, z. B. Häkchentaste |
| `cancel` oder `cancelled` | keyboard, Abbruchtaste |

Ein LVGL-Wertwechsel stellt `value_changed` und `changed` in die Warteschlange,
sofern entsprechende Handler existieren. Pro Kombination aus Objekt und Event
darf es nur einen Handler geben.

## 7. UI-Zugriff aus fScript

Jeder UI-Zugriff benötigt `"ui"` im Manifest.

### 7.1 Eigenschaften

| Eigenschaft | Typ | Objekte | Beispiel |
| --- | --- | --- | --- |
| `.text` | String | label, button, textarea, checkbox, roller, dropdown | `lbl.text = "OK"` |
| `.value` | Boolean | switch, checkbox | `toggle.value = true` |
| `.value` | Integer | roller, dropdown | `places.value = 2` |
| `.checked` | Boolean | switch, checkbox | `box.checked = false` |
| `.hidden` | Boolean | alle | `panel.hidden = true` |
| `.enabled` | Boolean | alle | `btn.enabled = false` |

Lesen und Schreiben verwenden dieselbe Punktnotation:

```fscript
var selected = places.value
var query = txt_search.text
lbl_result.text = query
```

Für Textareas ist `.text` zu verwenden. `.value` wird von der aktuellen
Laufzeit nicht als Textarea-Inhalt umgesetzt.

Bei `roller.text` und `dropdown.text` wird die komplette Optionsliste mit
Zeilenumbrüchen gesetzt; beim Lesen liefert `.text` die ausgewählte Option.

### 7.2 Methoden

Die Dateimethoden erwarten einen relativen Pfad; alle anderen UI-Methoden sind argumentlos:

| Methode | Objekte | Wirkung |
| --- | --- | --- |
| `.clear()` | textarea | Text leeren |
| `.focus()` | button, textarea, switch, checkbox, roller, dropdown | LVGL-Fokus setzen und Objekt sichtbar nach vorn holen |
| `.blur()` | dieselben | Fokus entfernen |
| `.scroll_to_top()` | textarea, panel, roller | nach oben scrollen |
| `.scroll_to_bottom()` | textarea, panel, roller | nach unten scrollen |
| `.load_file(path)` | textarea | App-Datei bis 4000 Byte direkt laden |
| `.save_file(path)` | textarea | Text direkt in eine App-Datei speichern |

```fscript
on btn_clear.click
  txt_input.clear()
  txt_input.focus()
end
```

## 8. Native fOS-APIs

### 8.1 Übersicht

| Funktion | Rückgabe | Berechtigung |
| --- | --- | --- |
| `http_get(url)` | Boolean | `network` |
| `http_status()` | Integer | `network` |
| `http_json(path)` | String | `network` |
| `http_text(offset)` | String | `network` |
| `url_encode(text)` | String | `network` |
| `date_weekday(date)` | String | keine |
| `file_read(path)` | String | `storage.read` |
| `file_write(path, data)` | Boolean | `storage.write` |
| `file_list(path)` | String | `storage.read` |
| `audio_play(path)` | Boolean | `audio` |
| `wifi_status()` | Boolean | `network` |
| `system.restart()` | nil | `system.restart` |
| `timer.start(milliseconds)` | Boolean | keine |
| `Serial.printf(text)` | nil | keine |
| `text(value)` | String | keine |
| `number_parse(text)` | Float | keine |
| `round(number)` | Integer | keine |
| `string_length(text)` | Integer | keine |
| `string_slice(text, start, length)` | String | keine |
| `string_last_index(text, needle)` | Integer | keine |
| `math_eval(expression)` | String | keine |

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

Pfade sind relativ zum aktiven App-Ordner. Unterordner sind erlaubt, müssen
aber bereits existieren. Absolute Pfade, Rückstriche, leere Segmente, `.` und
`..` werden abgewiesen. Eine App kann dadurch nicht auf fOS- oder fremde
App-Dateien zugreifen.

`file_read()` liest höchstens 255 Byte. Eine fehlende oder zu große Datei
liefert `""`; damit ist `""` nicht von einer tatsächlich leeren Datei zu
unterscheiden. `file_write()` ersetzt den Dateiinhalt und liefert den Erfolg.
`file_list()` liefert die direkten Einträge eines App-Unterordners, getrennt
durch Zeilenumbrüche; Ordnernamen enden mit `/`. Für Texteditoren umgehen
`textarea.load_file(path)` und `textarea.save_file(path)` das 255-Byte-Limit
der VM-Strings. Sie bleiben auf den aktiven App-Ordner begrenzt.

### 8.5 Audio, WLAN, Neustart, Timer und Serial

```fscript
var playing = audio_play("sounds/notify.mp3")
var online = wifi_status()
var timer_ok = timer.start(10000)
Serial.printf("Online: " + online + "\n")
```

`audio_play()` spielt eine vorhandene Datei aus dem App-Verzeichnis über das
bestehende fOS-Audiosystem. Vorherige Wiedergabe wird beendet; Schließen der App
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
fOS-Version. Bytecode 1.5 wird erzeugt; fOS 4.0.0 akzeptiert kompatible ältere
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

Native Funktions-IDs 1 bis 13 bleiben in Bytecode 1.5 unveraendert.
Dadurch bleiben vorhandene HTTP-, Wetter- und System-API-Apps binaer kompatibel.

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
die Kapazität beträgt 8. Eine volle Warteschlange verwirft weitere Events,
statt Speicher unbegrenzt wachsen zu lassen.

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

`layout.ui`:

```text
type=label;id=lbl_status;x=20;y=20;w=500;h=50;text=Bereit;fg=0xFFFFFF;font=24
type=button;id=btn_add;x=20;y=100;w=240;h=70;text=Erhöhen;bg=theme;fg=contrast
type=button;id=btn_reset;x=280;y=100;w=240;h=70;text=Reset;bg=0x555555;fg=0xFFFFFF
```

`main.fscript`:

```fscript
var counter = 0

function show_counter()
  lbl_status.text = "Zähler: " + counter
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
  Serial.printf("Aktueller Zähler: " + counter + "\n")
end
```

`app.json`:

```json
{
  "id": "de.beispiel.counter",
  "name": "Counter",
  "version": "1.0.0",
  "min_fos": "4.0.0",
  "type": "ui",
  "icon": "+1",
  "layout": "layout.ui",
  "executable": "main.fapp",
  "scrollable": false,
  "permissions": ["ui"]
}
```

Bauen:

```bash
./build/fosc check /Pfad/counter/main.fscript --ui /Pfad/counter/layout.ui
./build/fosc build /Pfad/counter/main.fscript --ui /Pfad/counter/layout.ui \
  -o /Pfad/counter/main.fapp
./build/fosc verify /Pfad/counter/main.fapp
```

Danach den Ordner `counter` vollständig nach `/apps/counter` auf die SD-Karte
kopieren und den fOS-App-Launcher neu öffnen.

## 15. Derzeitige Grenzen

fScript 1.0.0 besitzt noch keine Arrays, Maps, Klassen, Imports, Module,
Threads, frei definierbaren Systemzugriff oder Ausnahmebehandlung.
Native HTTP-Aufrufe sind synchron. UI und Runtime verwenden feste Grenzen für
vorhersagbaren RAM-Verbrauch. Die Quelldatei selbst ist nicht nötig, um eine
App auf fOS auszuführen; sie sollte dennoch für Wartung und erneutes Kompilieren
im App-Projekt aufbewahrt werden.

Bei Änderungen an der Reihenfolge benannter UI-Elemente, am Script oder an der
Compiler-/Bytecodeversion muss `main.fapp` neu gebaut und verifiziert werden.
