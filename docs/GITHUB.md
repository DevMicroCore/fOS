# App-Store und Firmware für GitHub

`github_repository_files` enthält die vorbereiteten gemeinsamen App-Dateien
und die getrennten Board-Pfade `Crowpanel_7"_esp32s3` und `Waveshare_7"_esp32p4`.
Inhalte bei Bedarf in das bestehende Repository übernehmen. Es wird nichts
automatisch veröffentlicht.

Versionierte App-Pakete liegen unter
`apps/<Name>/<Version>/<Name>/`. Ihre mitgelieferten README-/Anleitungsdateien
gehören zu den Paketgrößen im Store-Index und bleiben deshalb dort erhalten.
Bereits veröffentlichte Versionspakete nicht nachträglich verändern.

Nach Änderungen an veröffentlichten App-/Firmware-Paketen den Index mit dem
enthaltenen Generator neu erstellen, z. B. aus dem Projekt-Hauptordner:

```sh
python3 tools/generate_repository_indexes.py github_repository_files
```

Firmware-Dateien und zugehörige Größen-/Prüfsummen-Indizes gemeinsam übernehmen.
Lokale vorgebaute Firmware steht separat unter `firmware`; die Zuordnung steht
in [FIRMWARE_RECOVERY.md](FIRMWARE_RECOVERY.md).
