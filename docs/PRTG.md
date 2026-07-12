# PRTG-Integration

Das Board beantwortet SNMP-v1/v2c-GET-Anfragen direkt (siehe
[docs/entscheidungen.md](entscheidungen.md)) – PRTG kann es daher wie jedes
andere SNMP-Gerät abfragen, ganz ohne zusätzliche Software auf dem Board
oder einem Gateway. SNMP ist read-only: PRTG kann nichts am Gerät verändern.

## OIDs (Basis `.1.3.6.1.4.1.99999`)

**Wichtig:** Die Basis-OID-Struktur ist jetzt identisch zu den
Schwesterprojekten Sensormeter (WT32-ETH01) und Sensormeter PoE - nur die
Zweige, für die dieses Gerät keine Hardware hat, bleiben unbeantwortet:
`.2.1.0` (LAN-IP, kein LAN-Interface vorhanden) und der komplette
`.4.x`-Sensor-2-Zweig (kein zweiter Sensor möglich). Ein Sensormeter
Display kann dadurch alle drei SNMP-Agent-Varianten mit denselben
OID-Offsets abfragen. Das mitgelieferte Template ist bereits auf dieses
Schema abgestimmt – bei Sensormeter/Sensormeter PoE nicht wiederverwenden,
dort liegt ein eigenes Template vor.

| OID | Bedeutung | Typ |
|---|---|---|
| `.1.99999.1.1.0` | Systemname | String |
| `.1.99999.1.2.0` | Firmwareversion | String |
| `.1.99999.1.3.0` | Systemtyp (fest "Sensormeter WLAN") | String |
| `.1.99999.2.2.0` | WLAN-IP | String |
| `.1.99999.2.3.0` | WLAN-Signalstärke | Integer, dBm |
| `.1.99999.3.1.0` | Sensor Name (fest "DHT22") | String |
| `.1.99999.3.2.0` | Temperatur | Integer, ×10 (235 = 23.5 °C) |
| `.1.99999.3.3.0` | Luftfeuchte | Integer, ×10 |
| `.1.99999.5.1.0` | Uptime | TimeTicks (Zentisekunden) |
| `.1.99999.5.2.0` | Freier Heap | Gauge32, Bytes |

(OIDs oben mit `.1.3.6.1.4.1` abgekürzt.) Community-String ist auf der
Einstellungsseite des Geräts konfigurierbar (Default `public`).

## Template importieren

PRTG-Geräte-Templates liegen als `.odt`-Datei im Ordner
`devicetemplates` der PRTG-Installation (Standard:
`C:\Program Files (x86)\PRTG Network Monitor\devicetemplates\`) – anders
als beim Zabbix-Import gibt es keinen "Datei hochladen"-Dialog in der
PRTG-Weboberfläche.

1. Datei [`prtg-template-sensormeter-wlan.odt`](prtg-template-sensormeter-wlan.odt)
   in den `devicetemplates`-Ordner des PRTG-Servers kopieren (per RDP/
   Dateifreigabe auf den Server, auf dem PRTG läuft)
2. Neue Templates werden von PRTG automatisch beim nächsten
   Auto-Discovery-Lauf erkannt (kein Neustart des PRTG-Core-Dienstes
   nötig, kann aber ein bis zwei Minuten dauern)

## Gerät anlegen

1. Im gewünschten Probe/Ordner/Gruppe: **Add Device**
2. Name vergeben (z. B. "Sensor Wohnzimmer"), IPv4/DNS-Name des Boards
   eintragen
3. Bei **Device Template** den Punkt **"Automatic Device Identification
   (recommended)"** abwählen und stattdessen manuell **"Sensormeter
   WLAN"** auswählen
4. Unter **SNMP Credentials** die Community eintragen (Default `public`),
   SNMP-Version **v2c** wählen (das Gerät antwortet unabhängig davon auch
   v1-Clients korrekt), Port `161`
5. **Continue** → PRTG führt die Auto-Discovery aus und legt alle 11
   Sensoren aus dem Template an (passend zu diesem Board, kein manuelles
   Nachentfernen nicht zutreffender Sensoren nötig)

## Mitgelieferte Sensoren

| Sensor | PRTG-Sensortyp | Skalierung |
|---|---|---|
| Ping | Ping | – |
| System: Name / Firmwareversion / Systemtyp | SNMP Custom String | – |
| Netzwerk: WLAN-IP | SNMP Custom String | – |
| Netzwerk: WLAN-Signalstärke | SNMP Custom (dBm) | ÷1 |
| Sensor 1: Name | SNMP Custom String | – |
| Sensor 1: Temperatur | SNMP Custom (°C) | ÷10 |
| Sensor 1: Luftfeuchtigkeit | SNMP Custom (%) | ÷10 |
| Status: Uptime | SNMP Custom (Sekunden) | ÷100 (TimeTicks sind Zentisekunden) |
| Status: Freier Heap | SNMP Custom (Bytes) | ÷1 |

## Warnschwellwerte

Das Template legt bewusst keine Limits/Trigger vorkonfiguriert an –
nach dem Import direkt am jeweiligen Sensor unter **Channels → Limits**
setzen (empfohlen: Temperatur-Ober-/Untergrenze, Luftfeuchtigkeit-
Obergrenze, Freier Heap Untergrenze ~20000 Bytes).

## Mehrere Boards

Jedes Board bekommt in PRTG ein eigenes Gerät mit demselben Template,
nur mit unterschiedlicher IP-Adresse. Der Systemname (Einstellungsseite)
hilft, die Boards wiederzuerkennen.

## Testen ohne PRTG

Mit Net-SNMP-Tools (`apt install snmp` unter Linux):

```
snmpget -v1 -c public <board-ip> .1.3.6.1.4.1.99999.3.2.0
snmpget -v2c -c public <board-ip> .1.3.6.1.4.1.99999.3.3.0
```

## Technischer Hintergrund zum Template

Das Geräte-Template-Dateiformat (`.odt`) ist von Paessler nicht offiziell
dokumentiert. Dieses Template wurde daher nicht aus der Dokumentation
abgeleitet, sondern anhand eines echten, veröffentlichten PRTG-Templates
für einen vergleichbaren ESP32-basierten SNMP-Umweltsensor nachgebaut und
verifiziert (wohlgeformtes XML, korrekte `kind`-Werte
`snmpcustom`/`snmpcustomstring`/`ping`, funktionierende Skalierung über
`factord`) – alle OIDs zusätzlich direkt gegen `SNMPManager.cpp`
gegengeprüft. Vor dem produktiven Einsatz trotzdem einmal testweise
importieren und die Sensorwerte gegen `snmpget` (siehe oben) gegenprüfen.

## Siehe auch

[docs/ZABBIX.md](ZABBIX.md) – gleichwertiges Template für Zabbix (gleiches
OID-Schema, unabhängiges Werkzeug).
