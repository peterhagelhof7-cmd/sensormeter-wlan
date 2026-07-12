# Zabbix-Integration

Das Board beantwortet SNMP-v1/v2c-GET-Anfragen direkt (siehe
`docs/entscheidungen.md`) – Zabbix kann es also wie jedes andere
SNMP-Gerät abfragen, ganz ohne zusätzliche Software auf dem Board oder
einem Gateway. SNMP ist read-only: Zabbix kann nichts am Gerät verändern.

## OIDs (Basis `.1.3.6.1.4.1.99999`)

**Wichtig:** Die Basis-OID-Struktur ist jetzt identisch zu den
Schwesterprojekten Sensormeter (WT32-ETH01) und Sensormeter PoE - nur die
Zweige, für die dieses Gerät keine Hardware hat, bleiben unbeantwortet:
`.2.1.0` (LAN-IP, kein LAN-Interface vorhanden) und der komplette
`.4.x`-Sensor-2-Zweig (kein zweiter Sensor möglich). Ein Sensormeter
Display kann dadurch alle drei SNMP-Agent-Varianten mit denselben
OID-Offsets abfragen. Dieses Template ist bereits auf dieses Schema
abgestimmt – bei Sensormeter/Sensormeter PoE nicht wiederverwenden, dort
liegt ein eigenes Template vor.

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

1. In Zabbix: **Data collection → Templates → Import**
2. Datei [`zabbix-template-sensormeter-wlan.yaml`](zabbix-template-sensormeter-wlan.yaml) auswählen
3. Import bestätigen

## Host anlegen

1. **Data collection → Hosts → Create host**
2. Name vergeben (z. B. "Sensor Wohnzimmer")
3. Template **"Sensormeter WLAN"** zuweisen
4. Interface hinzufügen: Typ **SNMP**, IP-Adresse des Boards, Port `161`,
   SNMP-Version **SNMPv2** (das Gerät antwortet unabhängig davon auch
   v1-Clients korrekt), Community `public` (oder dein eigener Wert)
5. Falls du die Community auf der Einstellungsseite des Geräts geändert
   hast: Host-Makro `{$SNMP_COMMUNITY}` im Host auf denselben Wert setzen

## Mehrere Boards

Jedes Board bekommt in Zabbix einen eigenen Host mit dem gleichen
Template, nur mit unterschiedlicher IP-Adresse im SNMP-Interface. Der
Systemname (Einstellungsseite) hilft dir, die Boards in Zabbix
wiederzuerkennen (taucht auch als eigener Item-Wert "System: Name" auf).

## Mitgelieferte Trigger

Schwellwerte über Host-Makros anpassbar (`{$TEMP_MAX_C}`,
`{$HUMIDITY_MAX_PERCENT}`, `{$HEAP_MIN_BYTES}`):

- Temperatur zu hoch
- Luftfeuchtigkeit zu hoch
- Freier Heap niedrig
- Keine Daten seit 10 Minuten (Board offline oder Sensor defekt)

## Testen ohne Zabbix

Mit Net-SNMP-Tools (`apt install snmp` unter Linux):

```
snmpget -v1 -c public <board-ip> .1.3.6.1.4.1.99999.3.2.0
snmpget -v2c -c public <board-ip> .1.3.6.1.4.1.99999.3.3.0
```

## Siehe auch

[docs/PRTG.md](PRTG.md) – gleichwertiges Geräte-Template für PRTG
Network Monitor (gleiches, projektspezifische OID-Schema, unabhängiges
Werkzeug).
