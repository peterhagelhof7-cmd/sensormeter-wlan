#pragma once

#include <Arduino.h>
#include "ConfigManager.h"
#include "DataManager.h"
#include "NetworkManager.h"
#include "TimeManager.h"

// OLED-Anzeige (Pflichtenheft-Task "DisplayTask"): SSD1306 128x64 I2C auf
// GPIO21 (SDA)/GPIO22 (SCL, ESP32-Standardbelegung - kein Ethernet-PHY, der
// diese Pins blockiert, siehe pins.h/docs/entscheidungen.md), Adresse 0x3C.
// Rotierende Infoseiten alle 10s (lastenheft.txt Abschnitt 11: Systemname /
// WLAN-IP / Uhrzeit / Sensorwerte / Status WLAN - kein LAN-Zweig, kein
// zweiter Sensor, im Unterschied zum Vorbild im Sensormeter-Projekt).
// Waehrend des Bootens (BOOT/INIT/WLAN_CHECK) stattdessen Systemname +
// Countdown 100->0 bis das Netzwerk bereit ist.

class DisplayManager {
 public:
  DisplayManager(DataManager& dataManager, ConfigManager& configManager,
                 NetworkManager& networkManager, TimeManager& timeManager);

  void begin();
  void loop();

 private:
  DataManager& _data;
  ConfigManager& _config;
  NetworkManager& _network;
  TimeManager& _time;

  bool _initialized = false;

  unsigned long _lastPageSwitchMillis = 0;
  int _currentPage = 0;
  static const int PAGE_COUNT = 5;

  unsigned long _lastCountdownTickMillis = 0;
  int _countdownValue = 100;

  void drawLines(const String lines[], int count);
  void drawBootScreen();
  void drawPage(int page);
  void drawSystemNamePage();
  void drawIpPage();
  void drawTimePage();
  void drawSensorPage();
  void drawStatusPage();
};
