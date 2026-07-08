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
// (Port 514).
//
// Damit sind alle Phasen aus docs/implementierungsplan.html (P0-P7)
// umgesetzt.
// ============================================================================

#include <Arduino.h>

#include "ConfigManager.h"
#include "DataManager.h"
#include "DisplayManager.h"
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
SensorManager sensorManager(dataManager, timeManager);
DisplayManager displayManager(dataManager, configManager, networkManager, timeManager);
OtaManager otaManager;
WebServerManager webServerManager(dataManager, configManager, networkManager, otaManager, timeManager);
SNMPManager snmpManager(dataManager, configManager, networkManager);
SyslogManager syslogManager(dataManager, configManager, networkManager, timeManager);

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.print("=== Sensormeter WLAN ");
  Serial.print(DEVICE_FIRMWARE_VERSION);
  Serial.println(" ===");

  dataManager.begin();
  dataManager.setSystemState(SystemState::BOOT);

  storageManager.begin();
  configManager.begin();
  timeManager.begin();
  sensorManager.begin();
  displayManager.begin();
  syslogManager.begin();

  networkManager.begin();     // setzt Zustand auf INIT, dann WLAN_CHECK
  webServerManager.begin();   // async - kein eigener loop()-Aufruf noetig
  snmpManager.begin();
}

void loop() {
  networkManager.loop();
  timeManager.loop();
  sensorManager.loop();
  displayManager.loop();
  snmpManager.loop();
  syslogManager.loop();
  delay(50);
}
