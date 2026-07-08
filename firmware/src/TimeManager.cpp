#include "TimeManager.h"

TimeManager::TimeManager(DataManager& dataManager, NetworkManager& networkManager)
    : _data(dataManager), _network(networkManager) {}

void TimeManager::begin() {
  // P1: configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "de.pool.ntp.org") sobald
  // WLAN steht, plus Re-Sync alle 5h (siehe lastenheft.txt Abschnitt 8).
}

void TimeManager::loop() {
  // P1: NTP-Sync anstossen/pruefen, sobald NetworkManager RUN_NORMAL meldet.
}
