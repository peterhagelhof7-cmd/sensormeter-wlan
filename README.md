# Sensormeter WLAN

ESP32-basierter Umweltsensor (Temperatur/Luftfeuchte, DHT22) auf einem
generischen, günstigen ESP32-WROOM-32-DevKit (reines WLAN, kein
Ethernet). Bewusst reduzierte, kostengünstigere Variante des
[Sensormeter](https://github.com/peterhagelhof7-cmd/sensormeter)-Projekts
(WT32-ETH01): genau ein interner Sensor, kein Modulstecker/RJ45, keine
Erweiterbarkeit — wer zwei Sensoren oder eine Modularerweiterung braucht,
nutzt weiterhin Sensormeter bzw. Sensormeter PRO.

**Schwesterprojekte:**
[Sensormeter](https://github.com/peterhagelhof7-cmd/sensormeter) (WT32-ETH01, Ethernet + bis zu 2 Sensoren) ·
[Sensormeter Display](https://github.com/peterhagelhof7-cmd/sensormeter-display) (ESP32-Touchdisplay, fragt Sensormeter-Geräte per SNMP ab)

## Dokumentation

| Datei | Inhalt |
|---|---|
| [docs/lastenheft.txt](docs/lastenheft.txt) | Fachliche Anforderungen: Webseite, Einstellungen, SNMP-OIDs, Netzwerklogik, Zustandsmodell |
| [docs/pflichtenheft.txt](docs/pflichtenheft.txt) | Technische Umsetzung: FreeRTOS-Tasks, Softwaremodule, Speicherlayout, Fehlerbehandlung |
| [docs/implementierungsplan.html](docs/implementierungsplan.html) | Visueller Implementierungsplan P0–P7 (lokal im Browser öffnen) |
| [docs/stueckliste.md](docs/stueckliste.md) | Bauteile pro Gerät + Preisschätzung |
| [docs/entscheidungen.md](docs/entscheidungen.md) | Entscheidungsprotokoll: Boardwahl, Pinbelegung, OTA-Partitionierung, SNMP-Kompatibilität, bekannte Abweichungen |
| [docs/verdrahtung.html](docs/verdrahtung.html) | Pin-Tabelle + Verdrahtungsskizze (DHT22, OLED) |
| [docs/admin-guide.html](docs/admin-guide.html) | Admin-Guide: Inbetriebnahme, OLED-Anzeige, Weboberfläche, SNMP/Syslog |
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

## Firmware

`firmware/` ist ein PlatformIO-Projekt (Board `esp32dev`, Framework Arduino).
Aktueller Stand: **P0–P7 code-vollständig, bauen fehlerfrei, noch nicht auf
echter Hardware getestet** (siehe
[docs/implementierungsplan.html](docs/implementierungsplan.html) und
[docs/entscheidungen.md](docs/entscheidungen.md)).

Am schnellsten per PowerShell-Skript einrichten (installiert Python/Git/
PlatformIO bei Bedarf automatisch, klont/aktualisiert das Repo, baut und
flasht):

```
scripts\flash.ps1 -Project wlan
```

Details siehe [docs/admin-guide.html](docs/admin-guide.html). Manuelle
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
  übernommenes, bewährtes Muster
- `NetworkManager`: Boot-Zustandsautomat (BOOT → INIT → WLAN_CHECK →
  RUN_NORMAL/FALLBACK_MODE), WLAN-Verbindungsaufbau
- `TimeManager`: NTP-Sync (de.pool.ntp.org, CET/CEST), erster Versuch 60s
  nach Boot, danach alle 5h, zusätzlich bei WLAN-Reconnect; ohne Erfolg
  Wiederholung alle 5 Minuten, kein Einfluss auf den Systemzustand
- `ConfigManager`: `config.xml` auf LittleFS (tinyxml2, vendored),
  Laden/Speichern mit Default-Fallback, XML-Import/-Export, sicheres
  Schreiben über `.tmp`-Datei + Rename
- `StorageManager`: LittleFS-Mount
- `SensorManager`: DHT22-Abfrage alle 60s mit Plausibilitätsprüfung
- `DisplayManager`: OLED SSD1306, Boot-Countdown + 5 rotierende Infoseiten
- `WebServerManager`/`OtaManager`: Hauptseite, passwortgeschützte
  Einstellungsseite, REST-API, lokales OTA per `.bin`-Upload
- `SNMPManager`: SNMP v1/v2c read-only unter `.1.3.6.1.4.1.99999.x`
- `SyslogManager`: Statusreport je Sensorzyklus + sofortige Fehler-Events per UDP 514

Partitionstabelle bereits verifiziert (`gen_esp32part.py`): das
Standardschema für `esp32dev` bringt `ota_0`/`ota_1` von Haus aus mit,
keine eigene `partitions.csv` nötig (siehe `docs/entscheidungen.md`).

## Zusammenhang mit den Schwesterprojekten

- SNMP-OID-Struktur bewusst identisch zur Basis des Sensormeter-Projekts
  (`.1.3.6.1.4.1.99999.x`), nur ohne Sensor-2-Zweig — das
  Sensormeter-Display-Projekt kann damit Geräte aus beiden Produktlinien
  ohne Codeänderung abfragen.
- Fallback-WLAN-Konvention `installer`/`installer` wie beim
  Sensormeter-Projekt übernommen — in beiden Projekten aktuell als
  WLAN-Client-Beitritt statt als eigener Access Point umgesetzt (bekannte
  Abweichung von der ursprünglichen Spezifikation, siehe
  `docs/entscheidungen.md` und `docs/admin-guide.html` Abschnitt 2.2).

## Über dieses Projekt

Repo-Struktur und Dokumentation entstehen in Zusammenarbeit mit
[Claude](https://claude.com/claude-code) (Anthropic) als KI-Coding-Assistent.
