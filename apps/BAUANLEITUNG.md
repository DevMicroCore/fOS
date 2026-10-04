# Weather fScript bauen

Dieses Paket enthält bewusst nur die unkompilierten App-Dateien. Vor dem
Kopieren auf die SD-Karte muss aus `main.fscript` die Datei `main.fapp` erzeugt
werden.

Voraussetzung ist der mitgelieferte fOS-4.0.0-Projektstand mit `fosc 0.12.1`
oder neuer. Ältere Compiler kennen die Tastaturereignisse, `date_weekday()`,
`string_slice()` und die nativen fOS-APIs nicht vollständig.

Version 1.2.4 übernimmt Geometrie und Bedienablauf direkt aus der bereitgestellten
`ui_Weather.c`: große Temperaturanzeige, Vorhersage-Roller, bildschirmfüllende
Suche, Dropdown, Tastatur und separater Confirm-Button. Das Tastatur-Häkchen
sucht Orte; Confirm übernimmt den ausgewählten Treffer. Beim Öffnen der Suche
wird die Bildschirmtastatur ausdrücklich zusammen mit dem Suchpanel
eingeblendet und nach einer Suche sichtbar gehalten. Die Tastatur liegt als
eigenes Overlay im App-Hauptbereich, bleibt im Layout zunächst verborgen und
wird beim Drücken von Search sichtbar in den Vordergrund geholt. Der
Vorhersage-Roller zeigt sieben kurze Tageszeilen.

## Auf dem Mac kompilieren

1. `fOS4.0.zip` entpacken.
2. Dieses Verzeichnis `fscript_weather` beispielsweise in den entpackten
   Projektordner kopieren.
3. Terminal öffnen und zum Compiler wechseln:

```bash
cd ~/Downloads/fOS4.0/fScript_Development_Environment/fosc
xcode-select --install
make
make test
```

4. Die App zunächst prüfen. Passe den Pfad hinter `WEATHER_APP` an den
   tatsächlichen Speicherort des Quellordners an:

```bash
WEATHER_APP="$HOME/Downloads/fscript_weather"
./build/fosc check "$WEATHER_APP/main.fscript" --ui "$WEATHER_APP/layout.ui"
```

5. `main.fapp` erzeugen und anschließend verifizieren:

```bash
./build/fosc build "$WEATHER_APP/main.fscript" \
  --ui "$WEATHER_APP/layout.ui" \
  -o "$WEATHER_APP/main.fapp"

./build/fosc verify "$WEATHER_APP/main.fapp"
```

6. Den vollständigen Ordner einschließlich der neu erzeugten `main.fapp` auf
   die SD-Karte kopieren:

```text
/apps/fscript_weather/app.json
/apps/fscript_weather/app.cfg
/apps/fscript_weather/layout.ui
/apps/fscript_weather/main.fscript
/apps/fscript_weather/main.fapp
```

Die App benötigt eine aktive WLAN-Verbindung. Ihr Manifest fordert nur die
Berechtigungen `ui` und `network` an. Standortsuche und Wetterdaten stammen aus
den öffentlichen Diensten IP-API und Open-Meteo.

## Verwendete fOS-HTTP-API

```text
http_get(url)       -> Boolean; führt HTTP/HTTPS GET aus
http_status()       -> Integer; letzter HTTP-Statuscode
http_json(path)     -> String; JSON-Skalar über Pfad, z. B. results[0].name
http_text(offset)   -> String; Antwortabschnitt ab Byteposition
url_encode(text)    -> String; URL-kodierter Text
date_weekday(date)  -> String; Wochentag für YYYY-MM-DD
```

Antworten sind auf 32768 Bytes, einzelne fScript-Strings auf 255 Bytes und
Verbindungs-/Lesezeiten auf 5/7 Sekunden begrenzt. Pro App ist immer nur die
zuletzt geladene HTTP-Antwort verfügbar.
