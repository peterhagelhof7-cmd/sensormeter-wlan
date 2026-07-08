#pragma once

#include <Arduino.h>
#include "DataManager.h"
#include "NetworkManager.h"

// NTP-Zeitsynchronisation (docs/lastenheft.txt Abschnitt 8): de.pool.ntp.org,
// deutsche Zeitzone inkl. Sommerzeit, erster Versuch 60s nach Boot, danach
// alle 5h, zusaetzlich sofort bei einem WLAN-Reconnect (nicht bei der
// allerersten Verbindung - die faellt bereits unter den 60s-Timer, siehe
// docs/entscheidungen.md). Ohne Erfolg: Wiederholung alle 5 Minuten, Zeit
// bleibt bis dahin als "nicht synchronisiert" markiert statt zu blockieren.
class TimeManager {
 public:
  static constexpr uint32_t kInitialDelayMs = 60UL * 1000UL;
  static constexpr uint32_t kResyncIntervalMs = 5UL * 60UL * 60UL * 1000UL;
  static constexpr uint32_t kErrorRetryMs = 5UL * 60UL * 1000UL;

  TimeManager(DataManager& dataManager, NetworkManager& networkManager);

  void begin();
  void loop();

  bool isSynced() const { return _synced; }

 private:
  void triggerSync();
  void updateSyncedFlag();

  DataManager& _data;
  NetworkManager& _network;

  bool _synced = false;
  bool _wasWlanUp = false;
  uint32_t _beginMillis = 0;
  uint32_t _lastSyncMillis = 0; // 0 = noch nie versucht
};
