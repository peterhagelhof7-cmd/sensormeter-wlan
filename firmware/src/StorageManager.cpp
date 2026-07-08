#include "StorageManager.h"

#include <LittleFS.h>

void StorageManager::begin() {
  _mounted = LittleFS.begin(true); // true = bei Bedarf formatieren
  if (!_mounted) {
    Serial.println("[STORAGE] LittleFS-Mount fehlgeschlagen (auch nach Formatierungsversuch)");
  }
}
