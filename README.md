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
| [docs/entscheidungen.md](docs/entscheidungen.md) | Entscheidungsprotokoll: Boardwahl, Pinbelegung, OTA-Partitionierung, SNMP-Kompatibilität |

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

Noch nicht begonnen — nächster Schritt laut
[Implementierungsplan](docs/implementierungsplan.html) ist P0
(Projektstruktur, Boot-Zustandsautomat).

## Zusammenhang mit den Schwesterprojekten

- SNMP-OID-Struktur bewusst identisch zur Basis des Sensormeter-Projekts
  (`.1.3.6.1.4.1.99999.x`), nur ohne Sensor-2-Zweig — das
  Sensormeter-Display-Projekt kann damit Geräte aus beiden Produktlinien
  ohne Codeänderung abfragen.
- Fallback-WLAN `installer`/`installer` wie beim Sensormeter-Projekt,
  für eine konsistente Nutzererfahrung über beide Produktlinien hinweg.

## Über dieses Projekt

Repo-Struktur und Dokumentation entstehen in Zusammenarbeit mit
[Claude](https://claude.com/claude-code) (Anthropic) als KI-Coding-Assistent.
