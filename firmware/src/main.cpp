// ============================================================================
// Sensormeter WLAN - Phase P0: Grundgerüst & Zustandsmodell
//
// DataManager (mutex-geschuetzt, voll funktionsfaehig), ConfigManager/
// StorageManager/TimeManager als Geruest (Defaults bzw. No-Op, volle
// Implementierung folgt phasenweise), NetworkManager treibt den
// Boot-Zustandsautomaten (BOOT -> INIT -> WLAN_CHECK -> RUN_NORMAL /
// FALLBACK_MODE) bereits echt an: WLAN-Verbindungsversuch, nach 5 Minuten
// ohne IP automatischer Wechsel auf das Recovery-WLAN "installer".
//
// Naechste Phasen (siehe docs/implementierungsplan.html): P1 NTP + WLAN-
// Vervollstaendigung, P2 config.xml, P3 DHT22, P4 OLED, P5 Webserver+OTA,
// P6 SNMP, P7 Syslog.
// ============================================================================

#include <Arduino.h>

#include "ConfigManager.h"
#include "DataManager.h"
#include "NetworkManager.h"
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

  networkManager.begin();  // setzt Zustand auf INIT, dann WLAN_CHECK
}

void loop() {
  networkManager.loop();
  timeManager.loop();
  delay(50);
}
