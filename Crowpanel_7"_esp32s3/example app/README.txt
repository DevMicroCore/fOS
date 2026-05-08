Sample Apps for fOS (SD Card)

Includes six apps:
- hello_fos (type=ui, mehrere UI-Elemente per layout.ui)
- button_demo (type=ui, Button/Panels per layout.ui)
- ebook_demo (type=text, langer scrollbarer Inhalt aus book.txt)
- ui_demo (frei definierbare UI-Elemente in layout.ui)
- calculator_demo (type=calculator, + - * /, Komma, Math Error bei /0)
- radio_demo (type=radio, Dateien + Webradio mit Start/Stop)

Option A: Use the script
1) Insert the SD card
2) Run the script, e.g.:
   ./create_example_app.sh /Volumes/YOUR_SD

Option B: Copy directly
- Copy the “apps” folder from this directory to the SD card.
- The result on the SD card should be:
  /apps/hello_fos/...
  /apps/button_demo/...
  /apps/ebook_demo/...
  /apps/ui_demo/...
  /apps/calculator_demo/...
  /apps/radio_demo/...
- For radio web streams also copy:
  /music/webradio/webradio.txt

app.cfg keys:
- name=Display name in the launcher
- icon=Optionales Symbol/kurzer Text für AppL1-6
- type=ui | text | button | calculator | radio
- content=Filename for text apps (e.g., content.txt or book.txt)
- layout=Filename für UI-Apps (z. B. layout.ui)
- scrollable=true|false (für UI-Apps)
- button_text=Button label (only for type=button)
- button_message=Text displayed after clicking (only for type=button)

layout.ui format (eine Zeile pro Element):
- type=label;x=40;y=40;w=720;h=40;text=Hallo
- type=button;x=40;y=100;w=240;h=70;text=Start;bg=0x2095F6;fg=0xFFFFFF
- type=textarea;x=40;y=190;w=420;h=140;text=Zeile1\\nZeile2
- type=switch;x=500;y=120;value=true
- type=checkbox;x=500;y=180;text=Option;value=false
- type=panel;x=20;y=20;w=760;h=430;bg=0xF2F2F2

Then open the AppLauncher in fOS.

Webradio-Datei:
- Pfad: `/music/webradio/webradio.txt`
- Format pro Zeile: `Sender|URL`
- Beispiel:
  `SomaFM Groove Salad|http://ice1.somafm.com/groovesalad-128-mp3`
- Eine fertige Beispiel-Datei liegt hier:
  `example app/music/webradio/webradio.txt`

Audio-Dateien:
- Lege lokale Audio-Dateien (`.mp3`, `.wav`, `.ogg`, `.aac`, `.m4a`) nach:
  `/music/files/`
