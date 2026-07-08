#pragma once

#include <Arduino.h>
#include "DataManager.h"
#include "NetworkManager.h"

// NTP-Zeitsynchronisation (docs/lastenheft.txt Abschnitt 8). P0: nur
// Geruest (isSynced() liefert immer false) - echter NTP-Sync gegen
// de.pool.ntp.org inkl. CET/CEST folgt in P1, sobald NetworkManager
// zuverlaessig WLAN-Verbindungen herstellt.
class TimeManager {
 public:
  TimeManager(DataManager& dataManager, NetworkManager& networkManager);

  void begin();
  void loop();

  bool isSynced() const { return _synced; }

 private:
  DataManager& _data;
  NetworkManager& _network;
  bool _synced = false;
};
