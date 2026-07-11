# Entscheidungsprotokoll — Sensormeter WLAN

Dieses Projekt hat (im Unterschied zu Sensormeter und Sensormeter Display)
keine vorgegebene Materialsammlung – Lastenheft, Pflichtenheft und BOM
wurden komplett neu entworfen. Dieses Dokument hält die dabei getroffenen
Entscheidungen und deren Begründung fest.

## Board: generisches ESP32-WROOM-32 DevKit statt WT32-ETH01

Das Sensormeter-Projekt (WT32-ETH01) braucht ein Spezialboard mit
Ethernet-PHY (LAN8720), das teurer und weniger verbreitet ist als ein
generisches ESP32-Dev-Board. Da dieses Projekt explizit WLAN-only sein
soll ("kein Modulstecker benötigt", keine Ethernet-Anforderung), fällt der
Hauptgrund für das Spezialboard weg. Ein generisches
ESP32-WROOM-32-DevKit (30- oder 38-Pin, CP2102/CH340) ist:
- deutlich günstiger (~3–6 € statt eines Spezialboards)
- bei sehr vielen Händlern verfügbar (kein Single-Source-Risiko)
- exakt der Chip, für den PlatformIOs `esp32dev`-Board-Definition gedacht
  ist – keine Sonderbehandlung im Build nötig

## I2C-Standardpins (GPIO21/22) statt Sonderbelegung

Beim Sensormeter-Projekt (WT32-ETH01) musste das Display-I2C auf IO32/IO33
ausweichen, weil IO21/IO22 fest mit dem Ethernet-PHY verdrahtet sind
(siehe dortiges `entscheidungen.md`). Da dieses Board **kein** Ethernet-PHY
hat, entfällt der Grund für die Sonderbelegung - die Arduino-ESP32-
Standardbelegung (SDA=GPIO21, SCL=GPIO22, aktiv bei `Wire.begin()` ohne
Argumente) kann unverändert genutzt werden. Einfacher zu dokumentieren und
zu verdrahten, keine Pin-Remapping-Logik im Code nötig.

## DHT22 auf GPIO4

GPIO4 ist kein Boot-Strapping-Pin (im Unterschied zu GPIO0/2/5/12/15, die
beim Boot eine definierte Rolle haben - siehe Recherche unten) und liegt
nicht im internen Flash-SPI-Bereich (GPIO6-11, auf DevKits nicht
herausgeführt). Gleicher Pin wie beim internen DHT11 des
Sensormeter-Projekts (WT32-ETH01) - keine inhaltliche Notwendigkeit,
schadet aber nicht und ist als Konvention leicht zu merken.

## Nur ein Sensor, kein Modulstecker/RJ45 - bewusste Abgrenzung, kein Kompromiss

Der Auftrag war ausdrücklich "kein Modulstecker benötigt" und "nur ein
DHT22 intern". Statt Erweiterungs-Hooks für einen zweiten Sensor
vorzusehen (wie beim Sensormeter-Projekt), wurde das bewusst NICHT
nachgebildet: wer zwei Sensoren oder eine Modularerweiterung braucht, hat
bereits Sensormeter/Sensormeter PRO (WT32-ETH01) zur Verfügung. Zwei
Produktlinien mit identischem erweiterten Funktionsumfang zu pflegen wäre
unnötiger Mehraufwand ohne Zusatznutzen.

## SNMP-OID-Struktur: gleiche Basis wie Sensormeter-Projekt, ohne Sensor-2-Zweig

Damit das Sensormeter-Display-Projekt (dessen SNMP-Client bereits fest auf
`.1.3.6.1.4.1.99999.x` programmiert ist) auch ein Sensormeter-WLAN-Gerät
abfragen kann, ohne dort Code ändern zu müssen, wurde bewusst dieselbe
OID-Basis und -Struktur übernommen (System/Netzwerk/Sensor1/Systemstatus),
nur der Sensor-2-Zweig (.4) und der LAN-Netzwerk-Zweig entfallen (kein
zweiter Sensor, kein Ethernet). Ein Sensormeter-Display, das versehentlich
auf den (nicht existenten) Sensor-2-Zweig eines Sensormeter-WLAN-Geräts
zugreift, bekommt schlicht keine Antwort statt eines falschen Werts.

## OTA-Update: Standard-Partitionsschema reicht, keine eigene Tabelle nötig

Lokales OTA (`.bin`-Upload, siehe lastenheft.txt Abschnitt 6) braucht zwei
App-Slots (`ota_0`/`ota_1`), damit Platz für das eingehende Update neben
der laufenden Firmware ist. Geprüft durch tatsächlichen Build des
Sensormeter-Projekts (WT32-ETH01, identisches `board = esp32dev`, keine
eigene `partitions.csv`) und Auslesen der generierten Partitionstabelle
(`gen_esp32part.py`): das PlatformIO-Standardschema für dieses Board bringt
bereits `ota_0`/`ota_1` (je 1280K) plus `spiffs`/LittleFS (1408K) und eine
`coredump`-Partition (64K) mit. Für Sensormeter WLAN ist also **keine**
eigene Partitionstabelle nötig – anders als zunächst beim
Sensormeter-Display-Projekt dokumentiert (dortige Aussage war falsch,
wird dort korrigiert, siehe dortiges `entscheidungen.md`).

## Fallback-WLAN "installer" übernommen

Gleiche Konvention wie beim Sensormeter-Projekt (Fallback-SSID/PSK
"installer") - konsistente Nutzererfahrung über beide Produktlinien
hinweg, ohne dass sich jemand zwei verschiedene Fallback-Vorgehen merken
muss.

## P0 — Grundgerüst & Zustandsmodell

### DataManager von Anfang an mutex-geschützt, direkt vom Sensormeter-Projekt übernommen
Struktur/Feldnamen/Locking-Muster 1:1 vom bewährten `DataManager` des
Sensormeter-Projekts übernommen, nur `sensor1`/`sensor2` zu einem einzigen
`sensor`-Feld zusammengefasst. Kein Grund, ein funktionierendes,
produktiv verifiziertes Muster neu zu erfinden.

### Fehlendes WLAN: volle 5 Minuten Wartezeit vor Fallback-AP (wie spezifiziert)
Ein frisch gebootetes Gerät ohne gespeicherte WLAN-Zugangsdaten wartet die
vollen 5 Minuten (lastenheft.txt Abschnitt 8) bis zum Wechsel auf den
Fallback-Access-Point "installer" - es gibt (anders als beim
Sensormeter-Projekt mit Ethernet als Rückfallebene) waehrend dieser Zeit
gar keine Erreichbarkeit. Das ist keine Uebernahme eines Sensormeter-Bugs,
sondern exakt das im eigenen Lastenheft spezifizierte Verhalten -
festgehalten, damit es beim ersten Praxistest nicht als Fehler
missverstanden wird.

### Partitionstabelle verifiziert statt nur behauptet
`gen_esp32part.py` gegen die tatsächlich aus P0 generierte `partitions.bin`
laufen lassen: bestätigt `ota_0`/`ota_1` (je 1280K) im Standardschema, wie
in `pflichtenheft.txt` Abschnitt 9 dokumentiert - keine eigene
`partitions.csv` nötig.

## P1 — NTP-Zeitsynchronisation

### "Link-Up-Event" zaehlt erst nach dem ersten erfolgreichen Sync-Versuch
lastenheft.txt Abschnitt 8 nennt drei Ausloeser fuer einen NTP-Sync: 60s
nach Boot, alle 5h, und "nach Link-Up Events". Wortwoertlich umgesetzt
haette die allererste WLAN-Verbindung selbst schon als "Link-Up-Event"
gezaehlt und einen Sync-Versuch VOR Ablauf der 60s ausgeloest - das
widerspraeche dem 60s-Mindestabstand. `TimeManager` behandelt daher nur
WLAN-Reconnects NACH dem ersten Sync-Versuch als Link-Up-Event; die
allererste Synchronisation haengt ausschliesslich am 60s-Timer, unabhaengig
davon, wann WLAN tatsaechlich verbunden ist.

### Kein Zustandswechsel bei NTP-Fehlern (anders als beim Sensormeter-Projekt)
Das Sensormeter-Projekt hat fuer NTP-Fehler eine eigene Fehlerkette mit
Zustandswechseln (DHCP_TEST/RESTORE_CONFIG), weil dort zwei Interfaces
(LAN/WLAN) existieren, zwischen denen umgeschaltet werden kann. Hier gibt
es nur WLAN - ein NTP-Fehler kann nichts an der Netzwerkkonfiguration
aendern. Stattdessen: einfacher 5-Minuten-Retry-Takt, `isSynced()` bleibt
false bis zum naechsten Erfolg, kein Einfluss auf `SystemState` (siehe
lastenheft.txt Abschnitt 8, bereits in P0 vereinfacht dokumentiert).

## P2 — Konfigpersistenz

### tinyxml2 vendored (identisch zum Sensormeter-Projekt übernommen)
Gleiches Problem wie dort: der PlatformIO-Registry-Fork
`sepastian/tinyxml2` lässt sich unter Windows nicht installieren (kaputter
Symlink), ein direkter Git-Checkout des Original-Repos bringt unnötigen
Flash-Ballast (`contrib/`, Testsuite, ~220 KB) mit. Daher dieselben zwei
vendorten Dateien (`lib/tinyxml2/tinyxml2.{h,cpp}`) 1:1 übernommen statt
das Problem ein zweites Mal separat zu lösen.

### config.xml-Schema und Speicherlogik direkt übernommen, nur WLAN-only gekürzt
Lade-/Speicherlogik (inkl. sicherem Schreiben über eine `.tmp`-Datei mit
anschließendem Rename, damit ein Stromausfall während des Schreibens nicht
die bisherige funktionierende Konfiguration zerstört) 1:1 vom
Sensormeter-Projekt übernommen. Entfernt: `<lan>`-Abschnitt (kein
Ethernet), `<sensors><sensor2>`-Abschnitt und die davon abgeleitete
`systemType`-Logik ("Sensormeter" vs. "Sensormeter PRO") - hier gibt es
nur eine Variante, siehe lastenheft.txt Abschnitt 4.

## P3 — Sensorik

### Plausibilitätsgrenzen und Ringpuffer-Logik 1:1 vom Sensormeter-Projekt übernommen
`plausibleDht22()` (-40..80 °C, 0..100 % rF laut Datenblatt) sowie die
stündliche, NTP-Zeit-gated Ringpuffer-Ablage sind identisch zum externen
DHT22-Zweig des Sensormeter-Projekts - bewährtes Muster, keine
Sonderbehandlung nötig, da hier ohnehin nur ein Sensor existiert statt
eines optionalen zweiten.

## P4 — Display

### Rotationsseiten reduziert (kein LAN, kein Sensor 2)
`DisplayManager` übernimmt Boot-Countdown- und Seitenrotationslogik vom
Sensormeter-Projekt, aber mit nur 5 statt 6 Seiten (Systemname/WLAN-IP/
Uhrzeit/Sensor/Status) - kein LAN-Seiten-Zweig, da kein Ethernet vorhanden.

## P5 — Webserver & lokales OTA

### Seitenaufbau, REST-API und OTA-Streaming 1:1 vom Sensormeter-Projekt übernommen
`WebServerManager`/`OtaManager` sind bewusst nahezu unverändert vom
Sensormeter-Projekt übernommen (gleiches Seitenlayout, gleiche
API-Routen), nur um die LAN-Felder (Einstellungen, `/api/network`) und das
Sensor-2-Formular gekürzt. Kein HTTPS-Client/Remote-Versionscheck, gleiche
Begründung wie dort (Flash-Budget) - hier zusätzlich unnötig, da das
Flash-Budget durch den Wegfall von LAN/Sensor-2-Code ohnehin entspannter
ist (76,6 % nach P7 statt der beim Sensormeter-Projekt knapperen Auslastung).

## P6 — SNMP

### OID-Struktur exakt wie in lastenheft.txt Abschnitt 7 festgelegt
`.1.3.6.1.4.1.99999.2` hat hier nur zwei statt drei Einträge (WLAN-IP=.1,
RSSI=.2 - kein LAN-IP-Eintrag), abweichend von der Nummerierung im
Sensormeter-Projekt (dort LAN=.1, WLAN=.2, RSSI=.3). Systemtyp ist ein
fester String `"Sensormeter WLAN"` statt eines Konfigurationsfelds, da
`ConfigManager` hier kein `systemType` kennt (nur eine Produktvariante).

## P7 — Syslog

### Statusreport-Format gekürzt (kein LAN-IP, kein Sensor-2-Feld)
`SyslogManager` 1:1 vom Sensormeter-Projekt übernommen, Pipe-Format im
Statusreport aber ohne LAN-IP- und Sensor-2-Spalte:
`Systemname | WLAN-IP | RSSI | Sensor | ISO-Zeit | Uptime`.

## Bekannte Abweichung: Fallback-"Access Point" ist tatsächlich ein WLAN-Client-Beitritt

Beim Schreiben des Admin-Guides (siehe `docs/admin-guide.pdf` Abschnitt 2.2)
aufgefallen: `docs/lastenheft.txt` Abschnitt 8 beschreibt den Fallback als
"eigener Access Point, WLAN SSID 'installer', PSK 'installer'" - das Gerät
sollte selbst ein WLAN aufspannen, dem man sich zur Einrichtung verbindet.
`NetworkManager.cpp` setzt das aber nicht so um: es bleibt durchgehend im
`WIFI_MODE_STA` und versucht per `WiFi.begin("installer", "installer")`
einem **bereits vorhandenen** Netz mit diesem Namen beizutreten, statt
`WiFi.softAP(...)` aufzurufen. Ohne einen tatsächlich vorhandenen Hotspot
namens "installer"/"installer" ist das Gerät im Fallback-Fall gar nicht
erreichbar - die im Lastenheft beschriebene Einstellungsseite über den
Access Point wird nie erreicht.

Dasselbe Muster (STA-Beitritt statt SoftAP) steckt identisch im
Sensormeter-Projekt (WT32-ETH01), dort ebenfalls unverändert seit der
ursprünglichen Implementierung.

**Entscheidung dieser Runde:** Nicht gefixt, sondern der Admin-Guide
beschreibt bewusst das tatsächliche (Join-)Verhalten inkl. der praktischen
Einschränkung (Hotspot mit passendem Namen muss vorher bereitstehen). Ein
echter SoftAP-Fix (`WiFi.softAP("installer","installer")` statt
`WiFi.begin(...)`) ist für eine spätere Runde vorgemerkt - betrifft
potenziell auch das Sensormeter-Projekt.

**Update 2026-07-10: gefixt.** Siehe „Fallback-Access-Point ist jetzt ein
echter SoftAP" weiter unten - `NetworkManager` spannt im Fallback-Fall
tatsächlich ein eigenes Netz auf, statt eines beizutreten. Das
Sensormeter-Projekt (WT32-ETH01) hat den Fix noch nicht erhalten, betrifft
also weiterhin nur dieses Projekt hier.

## Gefixt: DNS-Server bei statischer WLAN-IP fehlte komplett

Beim Beantworten einer Nutzerfrage ("welchen DNS nutzt Sensormeter WLAN im
Static-Modus?") aufgefallen: `NetworkManager::applyWlanConfig()` rief bisher
nur `WiFi.config(ip, gateway, mask)` auf (3-Parameter-Form). Die tatsächliche
Signatur ist `WiFi.config(ip, gateway, subnet, dns1=0.0.0.0, dns2=0.0.0.0)`,
und im ESP32-Arduino-Core (`WiFiGeneric.cpp::set_esp_interface_dns()`) wird
ein DNS-Server nur gesetzt, wenn die übergebene Adresse ungleich `0.0.0.0`
ist. Ohne explizite Angabe blieb bei statischer IP also **gar kein**
DNS-Server konfiguriert - die NTP-Hostnamensauflösung
(`configTzTime(..., "de.pool.ntp.org")` in `TimeManager`) hätte dauerhaft
fehlgeschlagen. Bei DHCP (Default) betrifft das nicht, da der DNS-Server
automatisch vom Router mitkommt.

**Fix:** Neues Konfigurationsfeld `wlanDns` (Web-Formular, `ConfigManager`,
XML-Schema `<wlan dns="...">`) - bei leerem/ungültigem Wert wird das
Gateway als DNS-Server verwendet (funktioniert bei den meisten
Heimroutern), sonst der eingetragene Server.

Update 2026-07-09: dieselbe Lücke wurde auch im Sensormeter-Projekt
(WT32-ETH01) gefunden und dort ebenfalls gefixt (LAN **und** WLAN, siehe
dortiges `docs/entscheidungen.md`) - kein Vorsprung mehr zwischen beiden
Projekten in diesem Punkt.

## Noch offen / nicht Teil dieser Runde

- **Update 2026-07-10:** Erstes ESP32-WROOM-32-Board erhalten und per USB
  gegen die reale Firmware getestet (siehe „Erststart auf echter Hardware"
  unten) - der Hardware-Test-Vorbehalt oben ist damit erledigt.
- Exaktes Strombudget/Netzteilempfehlung noch nicht im Detail berechnet
  (siehe `stueckliste.md`) - sinnvoll erst nach Praxistest von
  Display-Helligkeit/WLAN-Sendeverhalten auf echter Hardware
- Konkrete Preisrecherche (siehe `stueckliste.md`) ist eine
  Marktabschätzung, keine verifizierten Händlerpreise

## Flash-Skript vereinheitlicht (`flash.ps1` statt `flash-sensormeter-wlan.ps1`)

Analog zum Sensormeter-Projekt: `scripts/flash-sensormeter-wlan.ps1` wurde zu
`scripts/flash.ps1` verallgemeinert - das Skript fragt jetzt zuerst
(interaktiv oder per `-Project sensormeter|wlan|display`), welches der drei
Sensormeter-Schwesterprojekte geflasht werden soll. Liegt identisch in allen
drei Repos, siehe dortiges `docs/entscheidungen.md` für die volle
Begründung.

## Versionierung

Bisher war `DEVICE_FIRMWARE_VERSION` an die zuletzt abgeschlossene
Implementierungsphase gekoppelt (`0.1.0-p0` … `0.1.0-p7`) - ein Schema ohne
klare Fortsetzung, sobald alle Phasen umgesetzt sind. Zusätzlich hatte
keines der drei Sensormeter-Repos je einen Git-Tag oder eine GitHub-Release
angelegt, obwohl die Admin-Guides bereits auf „GitHub-Releases" für
lokale OTA-Updates verweisen.

**Umstellung auf Semantic Versioning** (`MAJOR.MINOR.PATCH[-PRERELEASE]`,
https://semver.org/lang/de/): aktueller Stand aller drei
Sensormeter-Projekte wird auf **`0.9.0-rc1`** (Beta) gesetzt - alle
Kernfunktionen aus dem Lastenheft sind umgesetzt (P0-P7), aber noch nicht
vollständig auf echter Hardware verifiziert, daher Release-Candidate-Status
statt `1.0.0`.

Die Versionsnummer lebt weiterhin als einzige Quelle der Wahrheit in
`firmware/include/config.h(.example)` (`DEVICE_FIRMWARE_VERSION`, dort auch
über `/api/status` und die Hauptseite live abrufbar) und wird zusätzlich in
README, One-Pager und Admin-Guide vermerkt, damit sie auch ohne Blick in
den Quellcode sichtbar ist.

**Noch nicht Teil dieser Änderung** (separates Thema, hier nur vorgemerkt):
tatsächliche Git-Tags + GitHub-Releases mit `.bin`-Artefakt pro Version,
damit der in den Admin-Guides beschriebene OTA-über-Releases-Workflow
wirklich benutzbar wird, statt nur beschrieben zu sein.

### Firmware-Version auch auf dem OLED nachgetragen

Beim Einführen der Versionierung nachgefragt und geprüft: Webserver
(Hauptseite + `/api/status`) und SNMP (`.1.3.6.1.4.1.99999.1.2.0`) zeigten
die Firmware-Version bereits, das OLED bislang nicht. Ergänzt als zweite
Zeile auf der Status-Seite (Seite 5, `DisplayManager::drawStatusPage()`) -
damit ist die Version über alle drei Schnittstellen einsehbar, nicht nur
über Web/SNMP.

## Kalibrierkorrektur für den Sensor + Webdesign an Sensormeter Display angepasst

Nutzerwunsch (identisch zum Sensormeter-Projekt, siehe dortiges
Entscheidungsprotokoll): fester Korrekturwert (°C/%, positiv oder negativ)
für den DHT22, der auch die SNMP-gemeldeten Werte erfasst; zusätzlich das
Webdesign an das inzwischen überarbeitete Sensormeter-Display-Projekt
angleichen.

### SensorManager brauchte erstmals eine ConfigManager-Referenz
Anders als im Sensormeter-Projekt (dessen `SensorManager` bereits
`ConfigManager&` für `sensor2Enabled` kannte) hatte `SensorManager` hier
bisher nur `DataManager&`/`TimeManager&` - keine Konfiguration nötig, da es
nur einen fest verdrahteten Sensor ohne Optionen gibt. Für die Korrektur
war ein drittes Konstruktorargument (`ConfigManager&`) nötig, entsprechend
in `main.cpp` angepasst.

### Korrektur direkt in SensorManager angewendet - wirkt automatisch auch auf SNMP
`DeviceConfig` bekam `sensorTempOffset`/`sensorHumOffset` (persistiert als
`<sensor tempOffset="" humOffset=""/>` in `config.xml`). Die Korrektur wird
in `SensorManager::readSensor()` NACH der Plausibilitätsprüfung auf den
Rohmesswert angewendet, bevor `DataManager::setSensor()` aufgerufen wird -
da `SNMPManager::refreshValues()` denselben `DataManager::getSensor()`
liest wie Webserver, OLED und Stundenwerte-Aufzeichnung, propagiert die
Korrektur automatisch überallhin, ohne `SNMPManager` separat anfassen zu
müssen. Luftfeuchte wird auf [0, 100] geklemmt, Temperatur bewusst nicht.

### Webdesign: gleiche Umstellung wie im Sensormeter-Projekt
`buildPageShell()` war (Copy-Paste-Vorlage aus dem Sensormeter-Projekt)
nahezu identisch aufgebaut - dieselbe Palette übernommen (Navy-Banner
`#0f1f3d`, Orange-Akzent `#c8622a`, warmes Creme `#f2f0e9`, Kartenrahmen
`#e4e1d8`), nur CSS geändert, HTML-Klassennamen unverändert. Kein
Lastenheft-Konflikt (gleiche Prüfung wie im Sensormeter-Projekt: 20pt/
Schwarz-Weiß war nur Stilentscheidung, keine Anforderung). Die
Einstellungsseite hatte bisher GAR KEINEN "Sensor"-Block (nur System/WLAN/
Syslog/SNMP) - neu ergänzt, direkt mit den beiden Korrekturfeldern.
Chart.js-Linienfarben auf `#a63d2e`/`#2a5ba0` geändert (identisch zum
Sensormeter-Projekt).

Mit `pio run` gebaut (erfolgreich, Flash 76,8 % / 1.007.125 B, RAM 15,8 % /
51.732 B). Nicht geflasht - kein Sensormeter-WLAN-Board angeschlossen (nur
das Sensormeter-Display-Board war über USB verfügbar, ein anderes Gerät -
Flashen dorthin hätte die falsche Firmware auf das falsche Board gebracht).

### Nachtrag: v0.9.0-rc2 + Doku aktualisiert

Nach der Kalibrierkorrektur+Webdesign-Änderung oben: Version auf
`0.9.0-rc2` (Beta) gesetzt (`config.h`/`config.h.example`, README).
`lastenheft.txt` bekam eine neue "Sensor:"-Kategorie in Abschnitt 6 (gab es
vorher gar nicht, da dieses Projekt bisher keine einstellbaren
Sensor-Parameter hatte) sowie den SNMP-Korrektur-Hinweis in Abschnitt 7.
`pflichtenheft.txt` Abschnitt 5.2 korrigiert ("dark mode" stimmt nicht
mehr). Admin-Guide und One-Pager (beide mit wiederherstellbarer
HTML-Quelle aus der Git-Historie, anders als beim Sensormeter-Projekt)
ebenfalls aktualisiert und neu als PDF exportiert.

Mit `pio run` neu gebaut zur Verifikation nach den Doku-Änderungen (Code
selbst unverändert seit dem letzten Build).

## Erststart auf echter Hardware

Erstes ESP32-WROOM-32-Board per USB (COM5) angeschlossen und über
`scripts/flash.ps1`-Vorgehen (`pio run --target upload`) geflasht - erster
echter Boot-/Feldtest dieser Firmware überhaupt. DHT22, SSD1306-OLED,
WLAN-Verbindungsaufbau, Webserver, SNMP-Agent und LittleFS-Persistenz
funktionierten beim Erststart wie erwartet. Alle folgenden Einträge in
diesem Abschnitt entstanden im direkten Test-Zyklus (Ändern → `pio run
--target upload` → `pio device monitor` zur Verifikation) an diesem Board.

## 7-Tage-Verlauf jetzt tatsächlich persistent

`DataManager::pushHourValue()` hielt den Ringpuffer bisher nur im RAM - ein
Neustart löschte den gesamten 7-Tage-Verlauf, obwohl `WebServerManager`
ihn bereits als Graph/CSV auslieferte. Neu: `saveRingbuffer()` schreibt bei
jedem stündlichen Eintrag den kompletten Puffer nach `/history.csv` auf
LittleFS (CSV: Zeitstempel, Temperatur, Feuchte), `loadRingbuffer()` liest
ihn beim Boot zurück - muss nach `StorageManager::begin()` (LittleFS-Mount)
aufgerufen werden, nicht schon in `DataManager::begin()`, da dessen Mutex
vor allen anderen Modulen initialisiert werden muss. Stündliche
Schreibfrequenz ist vernachlaessigbarer Flash-Verschleiss.

## mDNS ergänzt

Gerät jetzt unter `<systemname>.local` statt nur per IP erreichbar. Der
frei eingebbare Systemname wird dafür sanitisiert (Kleinschreibung, nur
a-z/0-9/-, siehe `NetworkManager::sanitizeHostname()`) - dieselbe Funktion
setzt jetzt auch `WiFi.setHostname()`, was vorher gar nicht gesetzt wurde.
mDNS startet erst, sobald irgendein WLAN-Interface eine IP hat (regulär
oder Fallback-AP).

## Kalibrier-Zeitstempel jetzt persistent

`DeviceConfig` bekam `sensorCalibratedTs` (Unix-Zeitstempel der letzten
tatsächlichen Änderung der Korrekturwerte, nicht jedes Speichern der
Einstellungsseite - Vergleich alt/neu vor dem Setzen). Persistiert als
`calibratedTs`-Attribut in `<sensor>`, auf der Einstellungsseite als
"Zuletzt kalibriert: TT.MM.JJJJ HH:MM" angezeigt.

## Fallback-Access-Point ist jetzt ein echter SoftAP

Löst die oben dokumentierte „Bekannte Abweichung" auf. `NetworkManager`
ruft im Fallback-Fall jetzt `WiFi.softAPConfig(192.168.4.1, 192.168.4.1,
255.255.255.0)` + `WiFi.softAP("installer", "installer")` auf, statt
`WiFi.begin("installer", "installer")` (Beitritt zu einem fremden Netz).
Bewusst nur eigene IP + Subnetzmaske konfiguriert, kein Gateway/DNS - der
AP leitet nicht ins Internet weiter, das wäre irreführend. DHCP-Server
läuft automatisch (ESP32-Arduino-Core startet ihn implizit mit
`WiFi.softAP()`, kein eigener Code nötig).

`isWlanUp()`/`isUsingFallbackWlan()`/`getWlanIp()`/`getWlanGateway()`/
`getWlanSsid()` unterscheiden jetzt intern, ob der STA- oder der AP-Zweig
aktiv ist. Das OLED zeigt im Fallback-Fall ausschließlich "Fallback aktiv"
+ die eigene IP (keine Seitenrotation - die wäre hier nur ablenkend, siehe
`DisplayManager::drawFallbackIpPage()`).

## WLAN-Scan blockierte den Async-Webserver-Task - Gerät stürzte im Fallback-AP ab

Beim ersten Test des Scan-Buttons auf der Einstellungsseite (während das
Gerät als Fallback-AP lief) startete das Gerät jedes Mal neu und die
Verbindung riss ab. Ursache: `WiFi.scanNetworks()` ist standardmäßig
blockierend (mehrere Sekunden) und wurde direkt im
AsyncWebServer-Request-Handler aufgerufen - das blockiert den
Async-TCP-Task, der eigentlich schnell zurückkehren muss, und der
notwendige interne Wechsel auf `WIFI_MODE_AP_STA` während ein Client am
Fallback-AP hängt, hat die Verbindung zusätzlich gestört.

**Fix:** `WiFi.scanNetworks(true)` (asynchron, kehrt sofort zurück).
`/api/wifi/scan` liefert je nach `WiFi.scanComplete()`-Status
`{"status":"started"|"running"|"done", networks:[...]}` zurück, die
Einstellungsseite pollt das per JavaScript alle 1,5s für bis zu ~20s.

## WLAN aus dem Fallback-AP heraus einrichten: "Verbinden & testen"

Damit sich der eben gefixte Fallback-AP auch tatsächlich zur Ersteinrichtung
nutzen lässt: neuer Button auf der Einstellungsseite speichert SSID/PSK,
setzt `DeviceConfig::wlanPendingTest` und startet sofort neu
(`/api/wifi/connect`). `NetworkManager::begin()` liest dieses Flag beim
nächsten Boot, löscht es sofort wieder (gilt nur für genau einen
Boot-Versuch) und wartet dann nur `WLAN_TEST_TIMEOUT_MS` (30s) statt der
regulären 5 Minuten, bevor es bei Misserfolg zurück in den Fallback-AP
fällt - schnelles Feedback statt langer Wartezeit nach einer bewussten
Nutzeraktion.

## Werksreset ergänzt

Neue Einstellungsseiten-Buttons (Bereich "Konfiguration"), je mit
Bestätigungsdialog: "nur Einstellungen" setzt `config.xml` per
`ConfigManager::setConfig(DeviceConfig())` auf Defaults zurück, "Einstellungen
+ Daten" löscht zusätzlich `/history.csv`. Beide starten danach neu
(`/api/factory-reset`, Formularfeld `scope=settings|all`). Praktischer
Auslöser: die WLAN-Zugangsdaten ließen sich sonst nur per komplettem
Flash-Erase über USB zurücksetzen, um den Fallback-Flow erneut zu testen.

## OLED-Anzeige überarbeitet: zentriert, feste größere Schrift, Scroll statt Schrumpfen

Mehrere Nutzerwünsche in Folge, alle in `DisplayManager`:

- Boot-Countdown zeigt jetzt 3 Zeilen ("Sensormeter" / "WLAN" / Countdown +
  "warte") statt bisher 2 (kompletter Systemname + nackte Zahl), jede
  Zeile mit eigener größtmöglicher Schriftgröße.
- Die "Systemname"-Seite der Seitenrotation zeigt jetzt dieselbe feste
  Marke/Typ-Kennzeichnung ("Sensormeter" / "WLAN", zentriert) statt des
  frei editierbaren Systemnamens - der ist bereits auf der Weboberfläche
  prominent sichtbar.
- WLAN-Info-Seite zeigt jetzt SSID (Zeile 1) + IP (Zeile 2) statt nur der
  IP.
- Status-Seite bekam eine dritte Zeile mit dem Systemtyp ("Sensormeter
  WLAN", identisch zum SNMP-OID `.1.3.6.1.4.1.99999.1.3.0`).
- Alle Seiten sind jetzt horizontal UND vertikal zentriert (vorher
  linksbündig) und nutzen einheitlich eine feste, bewusst größere
  Schriftgröße (2), statt sie an die jeweils längste Zeile anzupassen.
  Passt eine Zeile dabei nicht auf einmal (z.B. eine lange WLAN-SSID, WLAN-
  SSIDs dürfen bis zu 32 Zeichen haben), läuft sie waagerecht durch, bis
  sie komplett zu lesen war, statt die Schrift für alle Zeilen zu
  schrumpfen - synchronisiert auf den 10s-Seitenwechsel-Timer (einmal
  durchlaufen, dann am Ende halten). Die Fallback-Seite hat keine feste
  Wechsel-Deadline und scrollt daher stattdessen dauerhaft wiederholend.
- Nebenbei ein latenter Absturzpfad behoben: die alte
  Schriftgrößen-Berechnung rundete bei sehr langen Zeilen (>21 Zeichen bei
  Schriftgröße 1) per Integer-Division auf Größe 0 herunter, wurde von
  einer `max(1, ...)`-Klammer wieder auf Größe 1 hochgezwungen - obwohl
  selbst Größe 1 dann nicht mehr gepasst hätte - und hätte automatisch
  umgebrochen und die nächste Zeile überlagert.
- Zwei Iterationen später (Nutzerfeedback nach echtem Testen):
  "Systemname"-Seite zeigt wieder den frei editierbaren Namen statt der
  festen Marke/Typ-Kennzeichnung (die bleibt auf Status-Seite/Boot-Screen
  bestehen) - reagiert also wieder auf Änderungen über die Weboberfläche.
  Zusätzlich neue sechste Seite "WLAN-Signal" (RSSI in dBm,
  `NetworkManager::getWlanRssi()`).

## BOOT-Taster als Seiten-Navigation + Werksreset-Ausloeser genutzt

Generische ESP32-WROOM-32-DevKits bringen zwei Taster fest auf dem Board
mit: **EN** (Chip-Enable/Reset) und **BOOT** (GPIO0, für den
Flash-Modus). Beide sind in der ursprünglichen Pinbelegung
(`pins.h`) nicht als nutzbare Eingänge vorgesehen - `EN` ist reine
Hardware und **kann softwareseitig gar nicht abgefangen werden** (ein
Druck reißt den Chip einfach in einen Reset, ganz unabhängig vom
laufenden Programm). `BOOT`/GPIO0 ist dagegen nach dem Hochfahren ganz
normal als Eingang lesbar (`INPUT_PULLUP`, aktiv LOW) - nur der Pegel
*im Moment eines Resets* beeinflusst den Bootmodus (gehalten = Flash-
Modus), im laufenden Betrieb spielt das keine Rolle mehr. Sehr verbreitete
Praxis bei ESP32-Hobbyprojekten, diesen Taster zusätzlich als
"User-Button" zu verwenden - kein zusätzliches Bauteil nötig.

**Umsetzung** (`DisplayManager::handleButton()`, aufgerufen mit Vorrang
vor jeder anderen Anzeigelogik in `loop()`, funktioniert also auch
während BOOT/INIT/WLAN_CHECK und im Fallback-Modus):

- **Kurzer Tipp** (≥50ms Entprellung, <3s gehalten): schaltet manuell zur
  nächsten Seite der Rotation, setzt auch den Seiten-/Scroll-Timer zurück.
- **≥3s gehalten**: OLED zeigt "Werksreset?" mit einem von 20 auf 0
  herunterzählenden Sekunden-Countdown.
- **Fail-Safe gegen einen verklemmten/defekten Taster:** Der eigentliche
  Reset wird bewusst **nicht** automatisch ausgelöst, sobald der
  Countdown abgelaufen ist, sondern erst beim tatsächlichen **Loslassen**
  danach (Anzeige wechselt währenddessen auf "Loslassen zum
  Bestätigen"). Bleibt der Taster durchgehend gedrückt (z.B. durch einen
  mechanischen Defekt), passiert dadurch nie ein Reset - es braucht ein
  echtes Loslassen-Ereignis, das ein dauerhaft geschlossener Kontakt nie
  liefert.
- Der ausgelöste Reset ist bewusst der Umfang "nur Einstellungen"
  (`ConfigManager::setConfig(DeviceConfig())`, identisch zum
  gleichnamigen Button auf der Einstellungsseite) - der 7-Tage-Verlauf
  bleibt erhalten. Auslöser für dieses Feature: WLAN-Zugangsdaten ließen
  sich bis dahin nur per komplettem Flash-Erase über USB zurücksetzen,
  wenn das Gerät wegen falscher Zugangsdaten gar nicht mehr erreichbar
  war - jetzt geht das auch rein über den Taster, ganz ohne PC/USB.

`pins.h` bekam dafür `BUTTON_BOOT_PIN` (= 0) als benannte Konstante, mit
Kommentar, dass dies bewusst die eine Ausnahme von der sonst geltenden
"GPIO0 meiden"-Regel für neue Peripherie ist (die Regel gilt für neu
angeschlossene Bauteile, nicht für den ohnehin vorhandenen Taster).

**Nachtrag (Board-Fotos geprüft):** Die Produktfotos des verwendeten
DevKits im Projektordner (`*.jpg`) beschriften auf einer der
Herstellergrafiken **beide** Taster fälschlich mit "Boot" (Copy-Paste-
Fehler in der Marketinggrafik) - maßgeblich ist der tatsächliche
Silkscreen-Aufdruck auf der Platine selbst ("Boot" links neben dem
USB-C-Anschluss, "EN" rechts daneben, bestätigt auf den Detailfotos),
der die ursprüngliche Zuordnung (BOOT = GPIO0, softwareseitig nutzbar;
EN = reiner Hardware-Reset) bestätigt. Werden beide Taster zusammen
gedrückt und EN zuerst losgelassen (oder generell EN losgelassen,
während GPIO0/BOOT weiterhin auf LOW gehalten wird), sampelt der Chip
GPIO0 beim Reset als LOW und wechselt in den seriellen Flash-/Bootloader-
Modus statt normal zu starten - das Gerät bleibt dann scheinbar
reaktionslos (OLED dunkel, kein WLAN), bis entweder ein erneuter Reset
mit losgelassenem BOOT erfolgt oder tatsächlich neue Firmware per USB
geflasht wird. Exakt dieselbe Pin-Kombination nutzt auch der automatische
Reset-Schaltkreis beim Flashen über `pio run --target upload`.
`docs/admin-guide.html` Abschnitt 1.1 bekam dafür eine korrekt
proportionierte Board-Skizze (das Board ist hochkant, ca. 1:2 im
Seitenverhältnis, nicht breit wie in der ersten Skizzenfassung) mit
beiden Tastern markiert, plus einen Hinweis auf die fehlerhafte
Verkäufergrafik.

## Board-Bringup abgeschlossen, v0.9.0-rc3

Mit dieser Runde (WLAN-Fallback-AP-Fix, Werksreset, Taster-Bedienung,
OLED-Neugestaltung, Zabbix-taugliche Systemübersicht - siehe Einträge
oben) ist der praktische Erst-Feldtest auf echter Hardware
abgeschlossen: ein Board lief über zahlreiche Änderungs-/Flash-Zyklen
hinweg stabil, alle P0-P7-Funktionsblöcke wurden am realen Gerät
verifiziert (DHT22-Messung, OLED-Anzeige inkl. Taster, WLAN-Verbindung
und -Fallback, Webserver inkl. Einstellungen/OTA/Werksreset,
SNMP-Agent, mDNS, persistenter 7-Tage-Verlauf). Der vorherige Vorbehalt
"noch nicht auf echter Hardware getestet" (siehe "Noch offen / nicht
Teil dieser Runde" weiter oben) ist damit endgültig erledigt.

Version auf `0.9.0-rc3` (Beta) angehoben (`config.h`/`config.h.example`,
README, Admin-Guide). `docs/admin-guide.html` wurde zu
`docs/admin-guide.pdf` exportiert (HTML-Quelle danach wieder entfernt,
wie in allen drei Projekten üblich - bei Bedarf per `git show
<commit>:docs/admin-guide.html` aus der Historie wiederherstellbar).

## Sicherheits-Feature: Ping-Check vor statischer WLAN-IP-Vergabe

Beim Speichern der Einstellungsseite wird jetzt vor dem Übernehmen einer
neu gesetzten statischen WLAN-IP per Ping geprüft, ob im Netz bereits ein
anderes Gerät unter dieser Adresse antwortet. Ist das der Fall, werden
alle Einstellungen dieser Seite verworfen und eine Fehlerseite
("IP-Adresse belegt") angezeigt statt eine Adresskollision im Netz zu
riskieren. Identisches Muster wie bei Sensormeter (WT32-ETH01, dort
zusätzlich für LAN) und Sensormeter Display - Bibliothek
`marian-craciunescu/ESP32Ping` neu in `platformio.ini`, `Ping.ping(ip, 1)`
mit der Bibliotheks-Standardwartezeit von 1s (kurz genug für den
synchronen `AsyncWebServerRequest`-Handler, siehe die
`WiFi.scanNetworks()`-Erfahrung oben). Der Check läuft nur, wenn sich die
neue Adresse von der aktuell aktiven WLAN-IP unterscheidet. Admin-Guide
entsprechend ergänzt.

Werks-Zugangsdaten projektübergreifend geprüft: Sensormeter WLAN nutzte
bereits `installer` als Web-Passwort-Default, identisch zu Sensormeter.
Abweichend war Sensormeter Display (`admin`) - dort auf `installer`
vereinheitlicht (kein Aenderungsbedarf in diesem Projekt).

Mit `pio run` gebaut und verifiziert (erfolgreich, Flash 80,1 % /
1.049.497 B, RAM 16,4 % / 53.772 B).

## MQTT/Home-Assistant-Integration umgesetzt

Von den vier bei `sensormeter-poe` entworfenen und für Sensormeter
(WT32-ETH01) bereits geprüften Portierungs-Kandidaten (siehe dortiges
`entscheidungen.md`, Abschnitt "Portierungs-Kandidaten aus sensormeter-poe
geprüft") ist bei Sensormeter WLAN nur MQTT/Home Assistant anwendbar –
BOOT-Taster ist hier bereits vorhandene, ältere Funktionalität, Sensor-2-
Auto-Erkennung und Relais/Aktor entfallen strukturell, da dieses Board
keinen RJ45-Modularanschluss besitzt (siehe "Nur ein Sensor, kein
Modulstecker/RJ45" oben).

Umsetzung (neuer `MqttManager`, Architekturvorbild `SyslogManager` –
gleiches Muster: Statuspublikation erkannt an einer Änderung von
`lastReadMillis` statt über einen eigenen Timer):
- `ConfigManager`: neuer `<mqtt enabled="" server="" port="" user=""
  password=""/>`-Abschnitt in `config.xml`. Topic-Präfix wird NICHT
  gespeichert, sondern wie der mDNS-Hostname zur Laufzeit aus dem
  Systemnamen abgeleitet (`NetworkManager::sanitizeHostname`).
- `MqttManager`: `PubSubClient`, throttlter Reconnect (5s-Intervall),
  Home-Assistant-Discovery (retained, `homeassistant/sensor/<prefix>/...`)
  nach jedem (Wieder-)Verbindungsaufbau, Statuswerte unter
  `<prefix>/state` bei jedem Sensorzyklus. Nur Sensor-Rolle (Temperatur,
  Luftfeuchte) – kein Relais/Aktor, siehe oben.
- `WebServerManager`: neuer Einstellungsblock "MQTT (Home Assistant)"
  (Aktiv-Checkbox, Broker-Adresse, Port, Benutzername, Passwort), gleiches
  Formularmuster wie Syslog/SNMP.
- Deaktiviert per Default (`mqttEnabled=false`), solange keine
  Broker-Adresse eingetragen ist – kein Verhaltensunterschied für
  bestehende Installationen ohne Home Assistant.

Ressourcenkosten empirisch (nicht nur geschätzt) gemessen: gegenüber dem
letzten Eintrag oben (Flash 80,1 % / 1.049.497 B, RAM 16,4 % / 53.772 B)
liegt der fertige, real gebaute Stand bei **Flash 82,1 % / 1.075.673 B,
RAM 16,5 % / 53.972 B** – also **+26.176 Byte Flash, +200 Byte RAM**
für PubSubClient inklusive `MqttManager`, `ConfigManager`-Erweiterung und
Web-UI/API-Anbindung zusammen (mehr als die reine
Bibliotheks-Testmessung bei Sensormeter, da hier auch der komplette
Manager samt Discovery-/State-Publishing-Code enthalten ist, nicht nur
Server/Connect/Publish-Aufrufe). Bei rund 235 KB freiem Flash weiterhin
deutlich unkritisch.

Mit `pio run` gebaut und verifiziert, per `pio run --target upload
--upload-port COM5` auf das angeschlossene Board geflasht. Boot-Log
bestätigt sauberen Start (`[MQTT] Grundgeruest bereit` direkt nach dem
Syslog-Init, normaler WLAN-Connect/RUN_NORMAL-Übergang, kein Crash-Loop).
`docs/lastenheft.txt` (neuer Abschnitt 14) und `docs/pflichtenheft.txt`
(Abschnitte 2/3.7/4.3/11) sowie `docs/admin-guide.html`/`.pdf` und
`docs/sensormeter-wlan-onepager.html`/`.pdf` entsprechend ergänzt.

**Noch offen:** MQTT ist per Default deaktiviert (kein Broker
konfiguriert) und wurde bisher nur so getestet (sauberer Boot ohne
Verbindungsversuch). Ein Test gegen einen echten Broker/Home-Assistant-
Instanz (Discovery, Reconnect-Verhalten, Statuswerte in Home Assistant
sichtbar) steht noch aus.

## Anbieter-Branding (Weisslabel) implementiert - erstes Projekt der Familie

Auf Anfrage aus der Familien-Machbarkeitseinschaetzung heraus direkt
umgesetzt: Sensormeter WLAN ist das erste der vier Projekte, das
Anbieter-Branding (freier Vendor-Name + optionales Logo) auf der
OLED-Anzeige und im Webserver zeigt.

- **Vendor-Name**: neues `String`-Feld `brandingVendorName` in
  `DeviceConfig`/`config.xml` (`<branding vendorName=""/>`), identisches
  Muster zu `systemName`/`mqttServer` etc. Leer = Feature inaktiv.
- **Logo bewusst NICHT in config.xml**: Binaerdaten gehoeren nicht in ein
  XML-Dokument, das komplett im RAM ge-/entladen wird. Stattdessen eigene
  Datei auf LittleFS (`/branding-logo.bin`), verwaltet durch die neue
  Klasse `BrandingManager`.
- **Bewusst kein PNG/JPEG-Decoder** (haette laut Machbarkeitseinschaetzung
  30-80 KB Flash gekostet, bei diesem Projekt mit ~223 KB freiem Flash vor
  dieser Aenderung ein spuerbarer Anteil): stattdessen ein festes,
  einfaches Rohformat - exakt 128x64 Pixel, 1 Bit pro Pixel, MSB-zuerst je
  Zeile, 1024 Byte fest, identisch zu dem, was
  `Adafruit_GFX::drawBitmap()` fuer die rotierenden OLED-Seiten ohnehin
  erwartet. Eine extern (z. B. per Python/GIMP-Export) vorkonvertierte
  Datei muss exakt diese Groesse haben, jede Abweichung wird beim Upload
  abgelehnt statt ein verzerrtes Bild stillschweigend zu zeigen.
- **Upload-Streaming** (`beginLogoUpload`/`writeLogoUploadChunk`/
  `endLogoUpload`) folgt demselben Muster wie der bestehende lokale
  OTA-`.bin`-Upload bzw. der XML-Konfigurationsimport: Multipart-Formular,
  Chunks werden zunaechst in eine Tmp-Datei geschrieben, die erst bei
  exakt passender Endgroesse per Umbenennen an die Zielposition verschoben
  wird (identisches Sicherheits-Muster zu `ConfigManager::save()` - ein
  Stromausfall oder eine falsch grosse Datei mitten im Upload darf ein
  zuvor funktionierendes Logo nicht zerstoeren).
- **Web-Darstellung ohne Bild-Bibliothek**: das im Rohformat gespeicherte
  Logo wird beim Abruf von `/branding/logo.bmp` on-the-fly in einen
  minimalen 1-Bit-Windows-BMP verpackt (reiner Header-Bau per `memcpy`,
  ca. 60 Zeilen) statt eine PNG-Encoder-Bibliothek einzubinden. Negative
  Hoehe im BMP-Header waehlt Top-Down-Zeilenreihenfolge, damit die intern
  in Adafruit-GFX-Reihenfolge gespeicherten Bytes ohne Zeilenumkehr direkt
  uebernommen werden koennen. **Unabhaengig von echter Hardware
  verifiziert**: dieselbe Header-Logik 1:1 in Python nachgebaut, mit
  einem generierten 128x64-Testmuster zu einer BMP-Datei zusammengesetzt
  und mit Pillow (echte Bildbibliothek) geladen - Groesse, Format und alle
  8192 Pixel stimmen exakt mit dem Ausgangsmuster ueberein (kein
  Zeilen-Flip noetig).
- **OLED-Seite nur bei aktivem Branding Teil der Rotation**: `pageCount()`
  liefert 6 (Default) oder 7 (Branding aktiv) statt einer festen
  Konstante - ein unkonfiguriertes Geraet zeigt dadurch keine leere
  Zusatzseite. Zeigt bevorzugt das Logo vollflaechig (kein Platz mehr fuer
  Text daneben bei 128x64), ohne Logo den Vendor-Namen als Textzeile.
- **Web-Header**: `buildPageShell()` blendet Logo (falls vorhanden) und
  Vendor-Name oberhalb jeder Seite ein, sofern `isActive()` true ist -
  eine einzige Einfuegestelle deckt alle Seiten ab.
- **Gefundener und behobener Bug vor dem finalen Flash**: die
  Arduino-ESP32-LittleFS-Bibliothek implementiert `LittleFS.exists()`
  intern als `open(path,"r")` (siehe `LittleFS.cpp` im Framework) - das
  quittiert die ESP-IDF-VFS-Schicht bei jedem Fehlschlag mit einer
  `[E]`-Logzeile. Ein erster Testflash zeigte dadurch alle 10s (Takt des
  Seitenwechsels, da `pageCount()`→`isActive()`→`hasLogo()` bei jedem
  Wechsel neu prüfte) eine wiederkehrende
  `open(): /littlefs/branding-logo.bin does not exist`-Zeile im
  Boot-Log - technisch harmlos, aber ein dauerhaft wiederkehrender
  `[E]`-Eintrag haette den "sauberer Boot-Log"-Massstab dieses Projekts
  verfehlt. Behoben durch einen RAM-Cache (`_logoPresent`, einmalig in
  `begin()` geprüft, aktualisiert bei Upload/Löschen) statt eines
  LittleFS-Zugriffs bei jedem Seitenwechsel - spart nebenbei auch
  wiederholte Flash-Zugriffe. Nach dem Fix: die Zeile erscheint nur noch
  einmalig beim Boot (wenn kein Logo hinterlegt ist), danach nicht mehr.
- **Nicht auf echter Hardware End-to-End getestet**: Vendor-Name-Speichern,
  Logo-Upload und die `/branding/logo.bmp`-Auslieferung liessen sich
  NICHT per HTTP gegen das geflashte Board verifizieren - der
  Entwicklungsrechner befindet sich in einem anderen Netzsegment
  (192.168.231.x) als das WLAN, in dem das Board haengt (192.168.77.x),
  keine Route dazwischen. Verifiziert wurde stattdessen: (a) sauberer
  Boot inkl. WLAN-Connect/RUN_NORMAL ueber einen vollen
  Seitenrotations-Zyklus (80s Monitor-Mitschnitt), (b) die BMP-Bau-Logik
  unabhaengig per Python-Nachbau + Pillow (siehe oben). Ein echter Test
  von Upload/Anzeige im Browser bzw. auf dem OLED steht noch aus.

Mit `pio run` gebaut und verifiziert (Flash 82,5 % / 1.081.957 B, RAM
17,4 % / 57.172 B - gegenueber dem MQTT-Stand oben, 82,1 % / 1.075.673 B
Flash, 16,5 % / 53.972 B RAM, ein Zuwachs von +6.284 B Flash / +3.200 B
RAM fuer `BrandingManager` samt Web-UI/API-Anbindung und BMP-Synthese).
Per `pio run --target upload --upload-port COM5` auf das angeschlossene
Board geflasht.

## Serial-Wiederherstellungskommando "dhcp" ergänzt

Auf Anfrage: das Gerät war per HTTP nicht mehr erreichbar, weil es mit
einer statischen IP in einem Netzsegment lief, zu dem der Bedienrechner
keine Route hatte (siehe auch die entsprechenden Hinweise beim
Branding-Feature oben). Bisher hatte diese Firmware **keinen einzigen
Serial-Eingabepfad** - `Serial` wurde ausschließlich für Log-Ausgaben
verwendet (`Serial.println`/`printf`), nirgends `Serial.available()`/
`Serial.read()` aufgerufen.

Ergänzt: eine minimale Zeile `handleSerialCommands()` in `main.cpp`
(zuoberst in `loop()` aufgerufen), die eingehende Serial-Zeilen sammelt
und bei exakt `"dhcp"` (Groß-/Kleinschreibung egal) die WLAN-Konfiguration
auf DHCP umstellt (`wlanDhcp=true`, statische IP/Maske/Gateway/DNS
geleert), speichert und neu startet - identischer Codepfad wie
`ConfigManager::setConfig()`, sonst nichts Neues erfunden.

- **Bewusst dasselbe Vertrauensmodell wie der bestehende
  BOOT-Taster-Werksreset**: physischer/USB-Zugriff gilt als
  vertrauenswürdig, kein Web-Passwort nötig für dieses Kommando - anders
  als der Taster-Reset aber **nicht destruktiv**: nur die WLAN-Netzwerk-
  felder ändern sich, alle übrigen Einstellungen (Kalibrierung, MQTT,
  Branding, SNMP/Syslog-Community) und der 7-Tage-Verlauf bleiben
  unangetastet.
- Kein neuer Task/Thread, keine Interrupt-Handler - reines Polling in
  `loop()`, wie der Rest der Firmware auch.
- Getestet: per `pyserial`-Skript verbunden, `"dhcp\r\n"` gesendet,
  Neustart + erfolgreicher DHCP-Bezug im Log live mitverfolgt (siehe
  unten) - kein Crash-Loop, sauberer state-Übergang
  `INIT → WLAN_CHECK → RUN_NORMAL`.

Mit `pio run` gebaut und verifiziert (Flash 82,6 % / 1.082.933 B, RAM
17,5 % / 57.204 B - gegenüber dem Branding-Stand oben, ein Zuwachs von
nur +976 B Flash / +32 B RAM). Geflasht und live getestet: Kommando
`"dhcp"` gesendet, Gerät wechselte von der zuvor konfigurierten
statischen IP `192.168.77.9` sauber auf eine per DHCP bezogene neue IP
`192.168.77.47` - Boot-Log zeigt einen normalen Übergang ohne Fehler.

## Einstellungsseite: WLAN und IP-Einstellungen getrennt, DHCP als Menü statt Checkbox

Auslöser: Nutzer hat die IP im Webinterface selbst umgestellt, das Gerät
war danach unter der alten IP nicht mehr erreichbar (Ursache lt. Serial-
Log: DHCP hat schlicht eine andere IP vergeben als erwartet, kein Fehler -
siehe Abschnitt oben). Die kombinierte "WLAN"-Box, in der DHCP-Checkbox,
SSID/PSK **und** die vier statischen IP-Felder unabhängig vom
Checkbox-Zustand immer sichtbar nebeneinanderstanden, hat dazu beigetragen
- unklar, welche Felder bei DHCP überhaupt wirken.

Geändert in `WebServerManager::buildSettingsPageBody()`:

- Der bisherige eine `<div class="block"><h2>WLAN</h2>` mit allem drin
  ist jetzt zwei Blöcke: **"WLAN"** (SSID, PSK, Scan-Button,
  "Verbinden & testen") und **"IP-Einstellungen"** (Modus-Auswahl,
  statische Felder, "IP-Einstellungen übernehmen & neu starten").
- Die DHCP-Checkbox (`<input type="checkbox" name="wlanDhcp">`) ist
  ersetzt durch ein Auswahlmenü `<select name="wlanIpMode">` mit den
  Optionen "Automatisch (DHCP)" / "Statisch". Die vier statischen Felder
  (IP/Netzmaske/Gateway/DNS) stecken in einem eigenen
  `<div id="staticIpFields">`, das per `toggleStaticIpFields()`
  (JS, an `onchange` des Menüs gehängt und einmal beim Seitenaufbau
  aufgerufen) nur bei Modus "Statisch" sichtbar ist - bei "Automatisch"
  bleiben sie ausgeblendet, statt wirkungslos dazustehen.
- Backend: `handleApiConfigPost()` liest jetzt
  `request->getParam("wlanIpMode", true)->value() == "dhcp"` statt der
  reinen Checkbox-Anwesenheit (`hasParam("wlanDhcp", ...)`).
  `handleApiNetworkApply()` (separater Button/Codepfad, eigener
  `dhcp`-Parameter mit Wert "1"/"0") war bereits vom Checkbox-Feld
  unabhängig und musste serverseitig nicht geändert werden - nur die JS-
  Funktion `applyNetwork()` liest jetzt `document.getElementById(
  'wlanIpMode').value` statt der nicht mehr existierenden `.checked`-
  Eigenschaft.
- Die beiden schon vorher parallel existierenden Parameter-Namen
  (`wlanDhcp` boolean für `/api/config`, `dhcp`="1"/"0" für
  `/api/network/apply`) wurden bewusst **nicht** vereinheitlicht - beide
  Codepfade funktionieren unabhängig voneinander korrekt, eine
  Umbenennung hätte nur Risiko ohne Nutzen gebracht.

Mit `pio run` gebaut (Flash 82,7 % / 1.083.549 B, RAM 17,5 % / 57.204 B -
praktisch unverändert, reine HTML/JS/Parsing-Änderung ohne neue Logik)
und per USB auf das laufende Gerät geflasht. Boot-Log nach dem Flash zeigt
einen sauberen Durchlauf `INIT → WLAN_CHECK → RUN_NORMAL` ohne Fehler,
die zuvor gespeicherte statische IP `192.168.77.9` wurde unverändert aus
`config.xml` übernommen (Konfiguration übersteht den Reflash wie erwartet).
Die neue Einstellungsseite selbst konnte vom Bedienrechner aus mangels
Route in `192.168.77.0/24` nicht im Browser nachgeprüft werden (siehe
Netzwerk-Einschränkung oben) - nur der Build- und Boot-Erfolg sind damit
verifiziert, nicht das visuelle Auf-/Zuklappen im Browser selbst.
