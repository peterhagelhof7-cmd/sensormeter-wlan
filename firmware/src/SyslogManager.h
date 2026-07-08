#pragma once

#include <Arduino.h>
#include <WiFiUdp.h>
#include "ConfigManager.h"
#include "DataManager.h"
#include "NetworkManager.h"
#include "TimeManager.h"

// Syslog-Versand (Pflichtenheft "SyslogTask"): periodischer Statusreport bei
// jedem Sensorzyklus + sofortiger Versand von Fehler-Events. Fehler-Events
// kommen aus DataManager's Ereignisprotokoll - derselben Quelle, die auch die
// Webseite fuer "letzte 5 Meldungen" nutzt (siehe DataManager::pushLogEntry).
// SyslogManager erkennt neue Eintraege anhand einer fortlaufenden
// Sequenznummer, kein Observer-Pattern noetig. UDP Port 514, deaktiviert wenn
// syslogServer "0.0.0.0" ist (Default). Vorbild: SyslogManager im
// Sensormeter-Projekt, hier ohne LAN-Zweig und ohne Sensor-2-Wert im
// Statusreport (genau ein Sensor, kein Ethernet).

class SyslogManager {
 public:
  SyslogManager(DataManager& dataManager, ConfigManager& configManager, NetworkManager& networkManager,
                TimeManager& timeManager);

  void begin();
  void loop();

 private:
  DataManager& _data;
  ConfigManager& _config;
  NetworkManager& _network;
  TimeManager& _time;

  WiFiUDP _udp;
  unsigned long _lastSeenLogSequence = 0;
  unsigned long _lastSensorReadMillisSeen = 0;

  bool syslogEnabled() const;
  void sendSyslog(int severity, const String& message);
  void sendStatusReport();
  void checkForNewLogEntries();
};
