#pragma once

#include <Arduino.h>
#include <SNMP_Agent.h>
#include <WiFiUdp.h>
#include "ConfigManager.h"
#include "DataManager.h"
#include "NetworkManager.h"

// SNMP v1 read-only Agent (lastenheft.txt Abschnitt 7, feste OID-Struktur
// unter .1.3.6.1.4.1.99999.x - bewusst dieselbe Basis-OID wie das
// Sensormeter-Projekt (WT32-ETH01), damit das Sensormeter-Display beide
// Produktvarianten ohne Codeaenderung abfragen kann). Die Bibliothek
// antwortet in der Version des eingehenden Requests (v1 oder v2c
// automatisch); "read-only" ist hier durch Konstruktion erzwungen - es wird
// nirgends isSettable=true gesetzt. Werte werden periodisch aktualisiert
// statt bei jedem GET neu berechnet (Pflichtenheft: "polling optimized, no
// continuous refresh").
//
// Anders als beim Sensormeter-Projekt: kein LAN-Zweig (kein Ethernet), fester
// Systemtyp-String statt Konfigurationsfeld (WLAN-Projekt hat kein
// systemType in ConfigManager), Sensor-2-Zweig (.4) bleibt unbeantwortet
// (siehe lastenheft.txt Abschnitt 7).

class SNMPManager {
 public:
  SNMPManager(DataManager& dataManager, ConfigManager& configManager, NetworkManager& networkManager);

  void begin();
  void loop();

 private:
  DataManager& _data;
  ConfigManager& _config;
  NetworkManager& _network;

  WiFiUDP _udp;
  SNMPAgent _agent;

  unsigned long _lastRefreshMillis = 0;

  // Werden periodisch befuellt (refreshValues()) und der Bibliothek als
  // Zeiger uebergeben - sie liest bei jedem GET live von dieser Adresse.
  char _systemName[33] = {0};
  char _wlanIp[16] = {0};
  char* _systemNamePtr = _systemName;
  char* _wlanIpPtr = _wlanIp;

  int _wlanRssi = 0;
  int _temperatureX10 = 0;
  int _humidityX10 = 0;
  uint32_t _uptimeTicks = 0;
  uint32_t _freeHeap = 0;

  void refreshValues();
};
