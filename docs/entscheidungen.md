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

## Noch offen / nicht Teil dieser Runde

- Kein Repository, keine Firmware, kein Implementierungsplan - der
  Auftrag war ausdrücklich nur "Infos sammeln und in den Projektordner
  schreiben" (Lastenheft, Pflichtenheft, BOM)
- Exaktes Strombudget/Netzteilempfehlung noch nicht im Detail berechnet
  (siehe `stueckliste.md`) - sinnvoll erst nach Festlegung von
  Display-Helligkeit/WLAN-Sendeverhalten in der Firmware
- Konkrete Preisrecherche (siehe `stueckliste.md`) ist eine
  Marktabschätzung, keine verifizierten Händlerpreise
