# Sensormeter WLAN

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="docs/projektfamilie-dark.png">
  <source media="(prefers-color-scheme: light)" srcset="docs/projektfamilie-light.png">
  <img alt="Sensormeter Projektfamilie: Sensormeter (LAN), Sensormeter WLAN (WLAN) und Sensormeter Display (Touchscreen), verbunden über gemeinsame Architektur und SNMP" src="docs/projektfamilie-light.png">
</picture>

ESP32-basierter Umweltsensor (Temperatur/Luftfeuchte, DHT22) auf einem
generischen, günstigen ESP32-WROOM-32-DevKit (reines WLAN, kein
Ethernet). Zeigt Werte lokal auf einem OLED an, stellt sie über eine
Weboberfläche, SNMP und Syslog bereit und meldet sich optional per
MQTT-Discovery selbstständig bei Home Assistant an. Bewusst reduzierte,
kostengünstigere Variante des
[Sensormeter](https://github.com/peterhagelhof7-cmd/sensormeter)-Projekts
(WT32-ETH01): genau ein interner Sensor, kein Modulstecker/RJ45, keine
Erweiterbarkeit — wer zwei Sensoren oder eine Modularerweiterung braucht,
nutzt weiterhin Sensormeter bzw. Sensormeter PRO.

[**One-Pager (PDF)**](docs/sensormeter-wlan-onepager.pdf) — kompakte Projektübersicht auf einer Seite.

**Schwesterprojekte:**
[Sensormeter](https://github.com/peterhagelhof7-cmd/sensormeter) (WT32-ETH01, Ethernet + bis zu 2 Sensoren) ·
[Sensormeter Display](https://github.com/peterhagelhof7-cmd/sensormeter-display) (ESP32-Touchdisplay, fragt Sensormeter-Geräte per SNMP ab)

## Dokumentation

| Datei | Inhalt |
|---|---|
| [docs/sensormeter-wlan-onepager.pdf](docs/sensormeter-wlan-onepager.pdf) | One-Pager: Projektübersicht, Architektur, Kennzahlen auf einer Seite |
| [docs/projektfamilie.html](docs/projektfamilie.html) | Architekturskizze: wie die drei Sensormeter-Projekte zusammenhängen |
| [docs/lastenheft.txt](docs/lastenheft.txt) | Fachliche Anforderungen: Webseite, Einstellungen, SNMP-OIDs, Netzwerklogik, Zustandsmodell |
| [docs/pflichtenheft.txt](docs/pflichtenheft.txt) | Technische Umsetzung: FreeRTOS-Tasks, Softwaremodule, Speicherlayout, Fehlerbehandlung |
| [docs/implementierungsplan.html](docs/implementierungsplan.html) | Visueller Implementierungsplan P0–P7 (lokal im Browser öffnen) |
| [docs/stueckliste.md](docs/stueckliste.md) | Bauteile pro Gerät + Preisschätzung |
| [docs/entscheidungen.md](docs/entscheidungen.md) | Entscheidungsprotokoll: Boardwahl, Pinbelegung, OTA-Partitionierung, SNMP-Kompatibilität, bekannte Abweichungen |
| [docs/verdrahtung.pdf](docs/verdrahtung.pdf) | Pin-Tabelle + Verdrahtungsskizze (DHT22, OLED) |
| [docs/admin-guide.pdf](docs/admin-guide.pdf) | Admin-Guide: Inbetriebnahme, OLED-Anzeige, Weboberfläche, SNMP/Syslog/MQTT |
| [docs/PRTG.md](docs/PRTG.md) | PRTG-Integration: OIDs, Geräte-Template-Import, Sensor-Übersicht |
| [docs/prtg-template-sensormeter-wlan.odt](docs/prtg-template-sensormeter-wlan.odt) | Fertiges PRTG-Geräte-Template für Auto-Discovery |
| [docs/ZABBIX.md](docs/ZABBIX.md) | Zabbix-Integration: OIDs, Template-Import, Host-Einrichtung, Trigger |
| [docs/zabbix-template-sensormeter-wlan.yaml](docs/zabbix-template-sensormeter-wlan.yaml) | Fertiges Zabbix-Template |
| [scripts/flash.ps1](scripts/flash.ps1) | PowerShell-Skript (fragt zuerst nach Projekt: Sensormeter/WLAN/Display): Abhängigkeiten installieren, Repo holen, bauen, flashen |

Dieses Projekt hat (im Unterschied zu den beiden Schwesterprojekten) keine
vorgegebene Materialsammlung – Lastenheft, Pflichtenheft und BOM wurden
komplett neu entworfen, angelehnt an die bewährte Architektur des
Sensormeter-Projekts.

## Hardware

- Generisches ESP32-WROOM-32-DevKit (30- oder 38-Pin, reines WLAN)
- DHT22/AM2302, 3-Pin-Modul, an GPIO4
- OLED SSD1306, 0,96", 128×64, I2C an GPIO21 (SDA) / GPIO22 (SCL) —
  ESP32-Standardbelegung, da kein Ethernet-PHY diese Pins blockiert
  (Unterschied zum Sensormeter-Projekt, siehe `docs/entscheidungen.md`)
- Kein zusätzliches Bauteil für die Bedienung nötig: der ohnehin auf jedem
  DevKit vorhandene **BOOT-Taster** (GPIO0) dient zusätzlich als
  Eingabe — kurzer Tipp blättert manuell durch die OLED-Seiten, langes
  Halten (3s + 20s Countdown, Auslösung erst beim Loslassen als
  Fail-Safe) löst einen Werksreset der Einstellungen aus. Der zweite
  Taster (**EN**) ist reiner Hardware-Reset und lässt sich softwareseitig
  nicht nutzen.

## Firmware

`firmware/` ist ein PlatformIO-Projekt (Board `esp32dev`, Framework Arduino).

**Version:** `0.9.0-rc3` (Beta) — Versionsschema siehe
[docs/entscheidungen.md](docs/entscheidungen.md#versionierung).

Aktueller Stand: **P0–P7 code-vollständig, Board-Bringup abgeschlossen** —
erstes Gerät läuft über mehrere Test-/Update-Zyklen stabil auf echter
Hardware (DHT22, OLED, WLAN inkl. Fallback-AP, Taster, Webserver, SNMP,
Syslog alle verifiziert), siehe
[docs/implementierungsplan.html](docs/implementierungsplan.html) und
[docs/entscheidungen.md](docs/entscheidungen.md). MQTT/Home-Assistant-
Anbindung ist geflasht und bootet sauber (deaktiviert per Default), aber
noch nicht gegen einen echten Broker/Home-Assistant-Instanz
durchgetestet — siehe `docs/entscheidungen.md`.

Am schnellsten per PowerShell-Skript einrichten (installiert Python/Git/
PlatformIO bei Bedarf automatisch, klont/aktualisiert das Repo, baut und
flasht):

```
scripts\flash.ps1 -Project wlan
```

Details siehe [docs/admin-guide.pdf](docs/admin-guide.pdf). Manuelle
Alternative ohne Skript:

```
cd firmware
cp include/config.h.example include/config.h
pio run              # bauen
pio run --target upload   # flashen
pio device monitor   # seriellen Log ansehen (115200 Baud)
```

Enthalten (P0–P7, siehe [docs/implementierungsplan.html](docs/implementierungsplan.html)):
- `DataManager`: zentrale, mutex-geschützte Datenhaltung (Sensorwert,
  Systemstatus, 7-Tage-Ringpuffer, Log) — direkt vom Sensormeter-Projekt
  übernommenes, bewährtes Muster; Ringpuffer wird bei jedem Stundenwert
  nach `/history.csv` auf LittleFS persistiert, übersteht also einen
  Neustart
- `NetworkManager`: Boot-Zustandsautomat (BOOT → INIT → WLAN_CHECK →
  RUN_NORMAL/FALLBACK_MODE), WLAN-Verbindungsaufbau; im Fallback-Fall
  spannt das Gerät einen eigenen Access Point auf (SSID/PSK `installer`,
  DHCP, nur eigene IP + Subnetzmaske), statt einem bestehenden Netz
  beizutreten; mDNS unter `<systemname>.local`
- `TimeManager`: NTP-Sync (de.pool.ntp.org, CET/CEST), erster Versuch 60s
  nach Boot, danach alle 5h, zusätzlich bei WLAN-Reconnect; ohne Erfolg
  Wiederholung alle 5 Minuten, kein Einfluss auf den Systemzustand
- `ConfigManager`: `config.xml` auf LittleFS (tinyxml2, vendored),
  Laden/Speichern mit Default-Fallback, XML-Import/-Export, sicheres
  Schreiben über `.tmp`-Datei + Rename; Werksreset (nur Einstellungen oder
  Einstellungen + Verlaufsdaten) über die Einstellungsseite
- `StorageManager`: LittleFS-Mount
- `SensorManager`: DHT22-Abfrage alle 60s mit Plausibilitätsprüfung, konfigurierbare
  Kalibrierkorrektur (°C/%, wirkt auf Anzeige, SNMP und CSV gleichermaßen),
  Zeitpunkt der letzten Kalibrierung wird persistent mitgeführt
- `DisplayManager`: OLED SSD1306, Boot-Countdown + 6 rotierende Infoseiten,
  zentrierte Darstellung mit fester, größerer Schrift - zu lange Zeilen
  (z.B. lange WLAN-SSIDs) laufen waagerecht durch statt zu schrumpfen;
  BOOT-Taster zusätzlich als Bedienelement (Seitenwechsel/Werksreset)
- `WebServerManager`/`OtaManager`: Hauptseite, passwortgeschützte
  Einstellungsseite (inkl. Sensor-Kalibrierkorrektur, nicht-blockierendem
  WLAN-Scan mit Direktverbindung-und-Test, Werksreset), REST-API, lokales
  OTA per `.bin`-Upload, Design an das Sensormeter-Display-Projekt angepasst
- `SNMPManager`: SNMP v1/v2c read-only unter `.1.3.6.1.4.1.99999.x`
- `SyslogManager`: Statusreport je Sensorzyklus + sofortige Fehler-Events per UDP 514
- `MqttManager`: optionale Home-Assistant-Anbindung per MQTT-Discovery
  (Sensor-Rolle, Temperatur/Luftfeuchte) — deaktiviert, solange kein
  Broker konfiguriert ist; siehe `docs/entscheidungen.md`

Partitionstabelle bereits verifiziert (`gen_esp32part.py`): das
Standardschema für `esp32dev` bringt `ota_0`/`ota_1` von Haus aus mit,
keine eigene `partitions.csv` nötig (siehe `docs/entscheidungen.md`).

## Zusammenhang mit den Schwesterprojekten

- SNMP-OID-Struktur bewusst identisch zur Basis des Sensormeter-Projekts
  (`.1.3.6.1.4.1.99999.x`), nur ohne Sensor-2-Zweig — das
  Sensormeter-Display-Projekt kann damit Geräte aus beiden Produktlinien
  ohne Codeänderung abfragen.
- Fallback-WLAN-Konvention `installer`/`installer` wie beim
  Sensormeter-Projekt übernommen — hier bereits als echter, selbst
  aufgespannter Access Point umgesetzt (DHCP, nur eigene IP + Subnetzmaske);
  das Sensormeter-Projekt (WT32-ETH01) joint stattdessen noch ein
  bestehendes Netz mit diesem Namen, siehe `docs/entscheidungen.md`.

## Über dieses Projekt

Repo-Struktur und Dokumentation entstehen in Zusammenarbeit mit
[Claude](https://claude.com/claude-code) (Anthropic) als KI-Coding-Assistent.
