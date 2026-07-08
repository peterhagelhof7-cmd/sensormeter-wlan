#include "ConfigManager.h"

void ConfigManager::begin() {
  // P0: Defaults reichen (kein WLAN konfiguriert -> NetworkManager faellt
  // sofort in FALLBACK_MODE, was fuer einen frisch gebooteten, noch nicht
  // eingerichteten Sensor korrektes Verhalten ist). Echtes Laden aus
  // config.xml folgt in P2.
  _config = DeviceConfig();
}

void ConfigManager::setConfig(const DeviceConfig& config) {
  _config = config;
  // Speichern auf LittleFS folgt in P2.
}
