// ============================================================================
// Sensormeter WLAN - Phase P4: OLED-Anzeige
//
// DataManager, NetworkManager, TimeManager, ConfigManager/StorageManager
// (config.xml auf LittleFS), SensorManager (DHT22) voll funktionsfaehig.
// DisplayManager zeigt jetzt rotierende Infoseiten auf dem SSD1306
// (Systemname/WLAN-IP/Uhrzeit/Sensorwerte/Status WLAN, 10s-Takt) sowie
// einen Boot-Countdown, siehe DisplayManager.h.
//
// Naechste Phasen (siehe docs/implementierungsplan.html): P5 Webserver+OTA,
// P6 SNMP, P7 Syslog.
// ============================================================================

#include <Arduino.h>

#include "ConfigManager.h"
#include "DataManager.h"
#include "DisplayManager.h"
#include "NetworkManager.h"
#include "SensorManager.h"
#include "StorageManager.h"
#include "SystemState.h"
#include "TimeManager.h"

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

  networkManager.begin();  // setzt Zustand auf INIT, dann WLAN_CHECK
}

void loop() {
  networkManager.loop();
  timeManager.loop();
  sensorManager.loop();
  displayManager.loop();
  delay(50);
}
