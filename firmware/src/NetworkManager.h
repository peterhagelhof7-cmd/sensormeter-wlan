#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "ConfigManager.h"
#include "DataManager.h"

// Treibt den Boot-Zustandsautomaten an (docs/lastenheft.txt Abschnitt 8+12):
//   BOOT -> INIT -> WLAN_CHECK -> RUN_NORMAL bzw. FALLBACK_MODE
//
// Deutlich einfacher als beim Sensormeter-Projekt: kein Ethernet, daher
// kein Parallelbetrieb zweier Interfaces und kein "welches Interface hat
// zuerst eine IP"-Vorrang - hier zaehlt nur WLAN.
//
// P0-Stand: WLAN-Verbindungsversuch mit den (in P0 noch leeren, ab P2 aus
// config.xml geladenen) gespeicherten Zugangsdaten; nach 5 Minuten ohne IP
// automatischer Wechsel auf das Recovery-WLAN "installer"/"installer"
// (siehe docs/entscheidungen.md). Statische IP-Anwendung ist bereits
// vorbereitet, wird aber erst mit echten Config-Werten ab P2 sinnvoll
// nutzbar.

class NetworkManager {
 public:
  NetworkManager(DataManager& dataManager, ConfigManager& configManager);

  void begin();
  void loop();

  bool isWlanUp() const { return _wlanGotIp; }
  bool isUsingFallbackWlan() const { return _inFallbackWlan && _wlanGotIp; }

  // Implementiert in .cpp, damit dieser Header schlank bleibt.
  IPAddress getWlanIp() const;
  IPAddress getWlanGateway() const;
  IPAddress getWlanDns() const;
  String getWlanMac() const;
  String getWlanSsid() const;
  int getWlanRssi() const;

 private:
  DataManager& _data;
  ConfigManager& _config;

  unsigned long _networkCheckStartedMillis = 0;
  unsigned long _lastFallbackJoinAttemptMillis = 0;
  bool _inFallbackWlan = false;

  void applyWlanConfig();
  bool networkOk() const { return _wlanGotIp; }

  static NetworkManager* _instance;
  static void onNetworkEvent(WiFiEvent_t event);

  volatile bool _wlanGotIp = false;
};
