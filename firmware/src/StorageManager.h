#pragma once

#include <Arduino.h>

// Dateisystem-Zugriff (LittleFS), siehe docs/pflichtenheft.txt Abschnitt 3.5.
// P0: nur Mount. values.csv-Schreiben (P3) und config.xml (P2) nutzen
// LittleFS direkt in ihren jeweiligen Managern - StorageManager stellt hier
// nur sicher, dass das Dateisystem beim Boot einmal bereitsteht.
class StorageManager {
 public:
  void begin();

  bool isMounted() const { return _mounted; }

 private:
  bool _mounted = false;
};
