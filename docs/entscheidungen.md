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

Beim Schreiben des Admin-Guides (siehe `docs/admin-guide.html` Abschnitt 2.2)
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

- Firmware (P0-P7) ist code-vollständig und für jede Phase per `pio run`
  fehlerfrei gebaut worden, aber **noch auf keiner realen Hardware
  getestet** - die bestellten ESP32-WROOM-32-Boards sind zum Zeitpunkt
  dieser Runde noch nicht geliefert. Insbesondere ungetestet: DHT22-Timing
  auf echtem GPIO4, SSD1306-I2C-Ansprache, WLAN-Fallback-AP-Übergang nach
  5 Minuten, SNMP-Antworten gegen einen echten Client (Sensormeter-Display),
  Syslog-Empfang auf einem echten Server, OTA-Upload-Flow über die
  Einstellungsseite.
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
