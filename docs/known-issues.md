# Known Issues — Sensormeter WLAN

Bekannte, noch nicht (oder nur teilweise) behobene Probleme. Details und
Herleitung jeweils in `docs/entscheidungen.md` (Datum siehe Verweis).

## Offen

### OTA-Upload bricht wegen WLAN-Verbindungsabbruch ab (seit mind. 2026-07-30)

**Status:** Teilweise behoben, weiterhin reproduzierbar fehlgeschlagen (3/3
Testversuche am 2026-07-30).

**Symptom:** Ein Firmware-Update per `POST /api/ota/upload` (Einstellungsseite
oder direkt per curl) bricht mitten im Upload ab - der Client bekommt keine
Antwort mehr (curl: "Server returned nothing" / Empfangsfehler).

**Ursache:** Das WLAN des Geraets verliert waehrend des Uploads kurzzeitig
(2-20s beobachtet) die Verbindung und baut sie neu auf, was die laufende
TCP-Verbindung des Uploads kappt. Ein Teilfix (`Update.begin()` mit der
echten Content-Laenge statt `UPDATE_SIZE_UNKNOWN` aufrufen, verhindert ein
unnoetig grosses Vorab-Loeschen der gesamten OTA-Partition) ist bereits
eingebaut, behebt das Problem aber nicht vollstaendig.

**Workaround:** Firmware-Updates seriell einspielen
(`pio run --target upload --upload-port <COMx>`), nicht per OTA.

**Details:** `docs/entscheidungen.md`, Eintrag "2026-07-30 — OTA-Upload
bricht mit WLAN-Verbindungsabbruch ab".

**Moeglicherweise betroffen:** sensormeter, sensormeter-poe,
sensormeter-display, ESP-BMC (gleiches `UPDATE_SIZE_UNKNOWN`-Muster im
OTA-Upload-Handler vermutet, nicht ueberprueft).

## Behoben

(bislang keine Eintraege - neue Liste seit 2026-07-30)
