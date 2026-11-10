# RTC mit eingesetzter Batterie testen

Diese Fassung enthält die bisher fehlende VDDA/VBAT-Umschaltung und eine
projektlokale Anpassung des PMU-Schlafablaufs für Arduino-ESP32 **3.3.12**.
Die SquareLine-Dateien wurden gegenüber der letzten Fassung nicht verändert.

## Installation und Erstabgleich

1. Den Hauptsketch mit **Before v3.00**, **PSRAM**, **32 MB Flash**, den vorhandenen
   Custom-Partitionen und **USB-OTG (TinyUSB)** hochladen. Der Core bleibt 3.3.12;
   keine Änderungen an dessen Installation sind notwendig.
2. Im seriellen Monitor (115200 Baud) muss beim Start erscheinen:
   `[RTC] VBAT AUTO=latched mode=2 chip-rev=...; charger=OFF; PMU sleep patch=enabled.`
   Das bestätigt die Registereinstellung, noch nicht die Batteriepufferung.
   `FAILED` bedeutet: automatische Versorgungsauswahl wurde nicht bestätigt;
   in diesem Fall wird der angeforderte Batterie-Ruhezustand abgebrochen.
3. Einmal WLAN verbinden und auf
   `[RTC] NTP captured: ... anchor captured, NVS=OK; ...`
   warten. Danach WLAN in fOS ausschalten, damit der Test ohne NTP erfolgt.

## Batterie-Ruhezustand und Versorgungstrennung

1. Die **BOOT-Taste am Board (GPIO0) drei Sekunden halten** und loslassen.
   USB-Speicher am PC vorher auswerfen/deaktivieren; WLAN-Verbindungsversuche,
   AppStore und Updates müssen fertig sein. Andernfalls wird der Schlaf abgebrochen.
2. Erwartete Meldungen:
   `[RTC] Backup sleep checkpoint: UTC=... counter=...; NVS=OK.`
   `[RTC] Entering VBAT deep sleep. ...`
   Der Bildschirm wird dunkel. Zwei Sekunden warten, damit der P4 im Schlaf ist.
3. Jetzt die Hauptversorgung trennen. Alle USB-Kabel abziehen, die das Board
   versorgen könnten. Die RTC-Batterie bleibt eingesetzt. Eine Minute warten.
4. Hauptversorgung wieder anschließen, dann **BOOT kurz drücken und loslassen**.
   Hierfür nicht RESET verwenden: RESET löst einen Hardware-Neustart aus.
5. Die Uhrzeit muss vor WLAN starten und ungefähr um die Wartezeit weitergelaufen
   sein. Der erwartete Start ist `reset=8` / `DEEPSLEEP`, mit
   `[RTC] Restored before Wi-Fi from RTC memory: ...`
   oder `NVS anchor + live RTC counter`. Die letzte gespeicherte Uhrzeit allein
   wäre kein erfolgreicher Test: die stromlose Minute muss mitgerechnet werden.

Die bisherige **GPIO52-Taste** bleibt für Display/Light Sleep zuständig und
weckt diesen Modus wie bisher. GPIO52 unterstützt beim P4 kein Deep-Sleep-Wakeup;
für den Batterie-Ruhezustand wird deshalb BOOT/GPIO0 benutzt.
BOOT kurz zu drücken hat im laufenden fOS keine neue Aktion; erst Halten und
anschließendes Loslassen fordern den Batterie-Ruhezustand an.

## Software-Neustart und Fehlerdiagnose

Ein Software-Neustart bei bestehender Versorgung wird weiterhin aus dem laufenden
RTC-Zähler wiederhergestellt. Die Anzeige berücksichtigt die eingestellte Zeitzone.
Bei einem echten Power-on-Reset mit verlorener Zeitbasis bleibt `Time not set`
bis zum nächsten NTP-Abgleich. Alte Zeitstempel werden dabei nicht als aktuelle
Uhrzeit ausgegeben. Das NVS-Backup wird verworfen, damit es bei späterem Neustart
nicht mit einem Zähler aus einer anderen Versorgungsphase kombiniert wird.

Falls der Batterietest scheitert, die Zeilen **VBAT**, **Anchors**, **Boot**,
**Backup sleep checkpoint** und **Restored/No retained** zusammen aufbewahren.
`chip-rev=103` bedeutet Revision 1.3. Die Batterie vorher auf vorhandene Spannung
und richtigen Kontakt prüfen. Die Firmware lädt die eingesetzte Knopfzelle nicht.

## Verifikation und Grenze

Der Hauptsketch wurde mit dem angegebenen Core kompiliert. Registertests und
RTC-Tests mit simuliertem, weiterlaufendem Zähler sind bestanden. Der Linker
verwendet nachweislich die angepasste PMU-Funktion; der Umschaltcode liegt im IRAM.
Ein physisches Board war hier nicht angeschlossen: Batteriepufferung, Strombedarf
und Aufwachen sind deshalb **noch nicht am Gerät bestätigt**.

Espressif beschreibt den VBAT-Betrieb mit vorbereiteter Schlafphase. Es gibt zudem
einen offenen Fehlerbericht für frühe P4-Revisionen, in dem selbst dieser Ablauf
keine Retention über den Versorgungsausfall erreichte. Deshalb ist dies eine
implementierte, noch am Board zu verifizierende VBAT-Unterstützung und keine
Garantie für beliebiges Abziehen der Versorgung oder Betätigen von RESET.
Die AUTO-Auswahl wird bereits beim Start aktiviert und vor jedem Schlaf erneuert;
als erster Hardwaretest ist der oben beschriebene Ablauf maßgeblich.

Quellen:
- https://docs.espressif.com/projects/esp-iot-solution/en/latest/low_power_solution/esp32p4_vbat.html
- https://github.com/espressif/esp-idf/issues/18988


## PMU-/VBAT-Implementierung und Herkunft


Target: Arduino-ESP32 3.3.12, bundled ESP-IDF 5.5.5, Before v3.00 variant.

`FosPmuSleep.c` comes from:
https://raw.githubusercontent.com/espressif/esp-idf/v5.5.5/components/esp_hw_support/port/esp32p4/pmu_sleep.c
Upstream SHA256: 1ad1eb1da55fc49f0906ebf9101d29d5860ea3948db6eea7f64240b26453099a
License: Apache-2.0; original notices retained, LICENSE.ESP-IDF included.
Changes: add target/version guards and the FosVbat.h include; call
`fos_vbat_apply_power(dslp)` immediately before `pmu_ll_hp_set_sleep_enable`.
All other PMU functions remain upstream. The sketch archive supplies these
symbols before libesp_hw_support.a, so the SDK's pmu_sleep object is not pulled
in. No linker flags, installed-core patch or generated UI edits are required.
The actual build map was checked for this replacement, and the VBAT function
was disassembled to confirm internal RAM code, MMIO and ROM-delay-only calls.

`FosVbat.c` implements Espressif's AUTO switchover and LP battery selection for
deep sleep. Both charger switches are disabled. A bounded 1000-us latch check
replaces the unbounded loop in the vendor example. Failure is visible on boot;
preparing deliberate backup sleep fails safely if AUTO does not latch.

This only configures power switching; it does not measure battery capacity or
prove retention. See RTC.md and issue #18988.
