#include "TimeManager.h"

#include <time.h>

namespace {
// M3.5.0 = letzter Sonntag im Maerz (Sommerzeit-Beginn),
// M10.5.0/3 = letzter Sonntag im Oktober, 3 Uhr (Sommerzeit-Ende).
constexpr const char* kTimezone = "CET-1CEST,M3.5.0,M10.5.0/3";
constexpr const char* kNtpServer = "de.pool.ntp.org";
} // namespace

TimeManager::TimeManager(DataManager& dataManager, NetworkManager& networkManager)
    : _data(dataManager), _network(networkManager) {}

void TimeManager::begin() {
  _beginMillis = millis();
}

void TimeManager::triggerSync() {
  Serial.println("[TIME] NTP-Sync angestossen (de.pool.ntp.org)");
  configTzTime(kTimezone, kNtpServer);
}

void TimeManager::updateSyncedFlag() {
  struct tm t;
  bool hadSync = _synced;
  // Kurzer Timeout (10ms): nur den aktuell im System hinterlegten Zeitwert
  // abfragen, nicht auf einen frischen NTP-Round-Trip warten (das macht
  // configTzTime() intern asynchron).
  if (getLocalTime(&t, 10) && (t.tm_year + 1900) > 2020) {
    _synced = true;
  } else {
    _synced = false;
  }
  if (_synced && !hadSync) {
    _data.pushLogEntry("Zeit: NTP-Sync erfolgreich", 6);
  }
}

void TimeManager::loop() {
  bool wlanUp = _network.isWlanUp();
  bool cameUp = wlanUp && !_wasWlanUp;
  _wasWlanUp = wlanUp;

  if (!wlanUp) {
    return; // ohne Netzwerk gibt es nichts zu tun
  }

  uint32_t now = millis();
  bool firstSyncDue = (_lastSyncMillis == 0) && (now - _beginMillis >= kInitialDelayMs);
  bool periodicDue = (_lastSyncMillis != 0) && (now - _lastSyncMillis >= kResyncIntervalMs);
  bool reconnectDue = (_lastSyncMillis != 0) && cameUp;
  bool notYetSyncedRetryDue =
      !_synced && _lastSyncMillis != 0 && (now - _lastSyncMillis >= kErrorRetryMs);

  if (firstSyncDue || periodicDue || reconnectDue || notYetSyncedRetryDue) {
    triggerSync();
    _lastSyncMillis = now;
  }

  updateSyncedFlag();
}
