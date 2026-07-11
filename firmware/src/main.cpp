// ============================================================================
// Sensormeter WLAN - Phase P7: Syslog
//
// Verdrahtet alle Module. ConfigManager laedt/speichert config.xml auf
// LittleFS; NetworkManager bringt WLAN (DHCP/statisch, Fallback-AP) hoch und
// treibt den Boot-Zustandsautomaten aus docs/lastenheft.txt an; TimeManager
// haengt sich mit der NTP-Sync-Kette daran; SensorManager liest DHT22 im
// 60s-Takt; DisplayManager zeigt Boot-Countdown und rotierende Infoseiten;
// WebServerManager stellt Hauptseite, Einstellungsseite, REST-API und
// lokalen OTA-Upload bereit (async, Port 80); SNMPManager beantwortet
// SNMP-v1/v2c-GET-Anfragen read-only (Port 161); SyslogManager sendet bei
// jedem Sensorzyklus einen Statusreport sowie Fehler-Events sofort per UDP
// (Port 514); MqttManager veroeffentlicht bei aktivierter Home-Assistant-
// Anbindung Discovery- und State-Payloads per MQTT (siehe
// sensormeter-poe/repo/docs/lastenheft.txt Abschnitt 16, docs/entscheidungen.md);
// BrandingManager haelt den optionalen Anbieter-Namen/das Logo (Weisslabel),
// das DisplayManager als eigene OLED-Seite und WebServerManager im
// Seiten-Header zeigt, sobald konfiguriert.
//
// Damit sind alle Phasen aus docs/implementierungsplan.html (P0-P7)
// umgesetzt.
// ============================================================================

#include <Arduino.h>
#include <ESPmDNS.h>

#include "BrandingManager.h"
#include "ConfigManager.h"
#include "DataManager.h"
#include "DisplayManager.h"
#include "MqttManager.h"
#include "NetworkManager.h"
#include "OtaManager.h"
#include "SNMPManager.h"
#include "SensorManager.h"
#include "StorageManager.h"
#include "SyslogManager.h"
#include "SystemState.h"
#include "TimeManager.h"
#include "WebServerManager.h"

#if __has_include("config.h")
#include "config.h"
#else
#error "config.h fehlt! Kopiere include/config.h.example nach include/config.h."
#endif

DataManager dataManager;
ConfigManager configManager;
StorageManager storageManager;
NetworkManager networkManager(dataManager, configManager);
TimeManager timeManager(dataManager, networkManager);
SensorManager sensorManager(dataManager, timeManager, configManager);
BrandingManager brandingManager(configManager);
DisplayManager displayManager(dataManager, configManager, networkManager, timeManager, brandingManager);
OtaManager otaManager;
WebServerManager webServerManager(dataManager, configManager, networkManager, otaManager, timeManager,
                                   brandingManager);
SNMPManager snmpManager(dataManager, configManager, networkManager);
SyslogManager syslogManager(dataManager, configManager, networkManager, timeManager);
MqttManager mqttManager(dataManager, configManager, networkManager);

// Serial-Wiederherstellungskommando "dhcp" (+ Enter): stellt WLAN auf DHCP
// um und startet neu - fuer den Fall, dass das Geraet nur per USB, aber
// nicht per Netzwerk erreichbar ist (z.B. andere statische IP als das
// Bediengeraet, kein Routing dazwischen). Bewusst dasselbe
// Vertrauensmodell wie der bestehende BOOT-Taster-Werksreset (physischer
// Zugriff = vertrauenswuerdig, kein Web-Passwort noetig) - anders als
// dieser aber NICHT destruktiv: nur die WLAN-Netzwerkfelder werden
// veraendert (DHCP an, statische IP/Maske/Gateway/DNS geloescht), alle
// uebrigen Einstellungen sowie der 7-Tage-Verlauf bleiben unangetastet.
// Reiner Zeilen-Reader ueber Serial.available()/read() - diese Firmware
// hatte bisher ueberhaupt keinen Serial-Eingabepfad.
void handleSerialCommands() {
  static String line;
  while (Serial.available()) {
    char c = static_cast<char>(Serial.read());
    if (c == '\r') continue;
    if (c != '\n') {
      line += c;
      continue;
    }
    line.trim();
    if (line.equalsIgnoreCase("dhcp")) {
      DeviceConfig cfg = configManager.getConfig();
      cfg.wlanDhcp = true;
      cfg.wlanIp = "";
      cfg.wlanMask = "";
      cfg.wlanGateway = "";
      cfg.wlanDns = "";
      configManager.setConfig(cfg);
      Serial.println("[SERIAL] WLAN auf DHCP umgestellt, starte neu...");
      delay(300);
      ESP.restart();
    }
    line = "";
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.print("=== Sensormeter WLAN ");
  Serial.print(DEVICE_FIRMWARE_VERSION);
  Serial.println(" ===");
  Serial.println("[SERIAL] Kommando \"dhcp\" (+ Enter) stellt WLAN auf DHCP um und startet neu");

  dataManager.begin();
  dataManager.setSystemState(SystemState::BOOT);

  storageManager.begin();
  dataManager.loadRingbuffer();
  configManager.begin();
  timeManager.begin();
  sensorManager.begin();
  brandingManager.begin();
  displayManager.begin();
  syslogManager.begin();
  mqttManager.begin();

  networkManager.begin();     // setzt Zustand auf INIT, dann WLAN_CHECK
  webServerManager.begin();   // async - kein eigener loop()-Aufruf noetig
  snmpManager.begin();
}

void loop() {
  handleSerialCommands();
  networkManager.loop();
  timeManager.loop();
  sensorManager.loop();
  displayManager.loop();
  snmpManager.loop();
  syslogManager.loop();
  mqttManager.loop();

  // Einmaliger mDNS-Start, sobald eine WLAN-IP vorliegt (auch im Fallback-
  // Netz "installer") - vor RUN_NORMAL ist noch keine IP vergeben.
  static bool mdnsStarted = false;
  if (!mdnsStarted && networkManager.isWlanUp()) {
    String hostname = NetworkManager::sanitizeHostname(configManager.getConfig().systemName);
    if (MDNS.begin(hostname.c_str())) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[NET] mDNS gestartet: http://%s.local/\n", hostname.c_str());
    } else {
      Serial.println("[NET] mDNS-Start fehlgeschlagen");
    }
    mdnsStarted = true;
  }

  delay(50);
}
