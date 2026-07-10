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
// WLAN-Verbindungsversuch mit den aus config.xml geladenen Zugangsdaten;
// nach 5 Minuten ohne IP startet das Geraet einen eigenen Fallback-Access-
// Point (SSID/PSK "installer", DHCP aktiv, nur eigene IP + Subnetzmaske
// konfiguriert, kein Gateway/DNS - siehe lastenheft.txt Abschnitt 8/12 und
// docs/entscheidungen.md). Statische IP-Anwendung ist fuer die primaere
// WLAN-Verbindung vorbereitet.

class NetworkManager {
 public:
  NetworkManager(DataManager& dataManager, ConfigManager& configManager);

  void begin();
  void loop();

  bool isWlanUp() const { return _wlanGotIp || _apActive; }
  bool isUsingFallbackWlan() const { return _apActive; }

  // Implementiert in .cpp, damit dieser Header schlank bleibt.
  IPAddress getWlanIp() const;
  IPAddress getWlanGateway() const;
  IPAddress getWlanDns() const;
  String getWlanMac() const;
  String getWlanSsid() const;
  int getWlanRssi() const;

  // Oeffentlich fuer WebServerManager::handleApiNetworkApply(): wendet die
  // aktuell in ConfigManager gespeicherte WLAN-Konfiguration erneut live an
  // (WiFi.config(), ohne Reconnect) - genutzt, um nach einem gescheiterten
  // DHCP-Lease-Test (siehe dort) die laufende Verbindung auf den zuletzt
  // bekannten, funktionierenden Stand zurueckzusetzen, statt sie im
  // Test-Zwischenzustand (DHCP erzwungen) haengen zu lassen.
  void reapplyWlanConfig() { applyWlanConfig(); }

  // Leitet aus dem frei eingebbaren Systemnamen (ConfigManager) einen
  // DNS-/mDNS-tauglichen Hostnamen ab (nur a-z/0-9/-, keine Leerzeichen/
  // Umlaute/Grossbuchstaben) - wird sowohl fuer WiFi.setHostname() als auch
  // fuer den mDNS-Namen (main.cpp) verwendet, damit beide konsistent sind.
  static String sanitizeHostname(const String& name);

 private:
  DataManager& _data;
  ConfigManager& _config;

  unsigned long _networkCheckStartedMillis = 0;
  unsigned long _lastFallbackJoinAttemptMillis = 0;
  // Timeout fuer den aktuellen WLAN_CHECK-Durchlauf - normal 5 Minuten,
  // kurz (30s) direkt nach Eingabe neuer Zugangsdaten im Fallback-AP (siehe
  // begin() / DeviceConfig::wlanPendingTest).
  unsigned long _wlanCheckTimeoutMs = 0;

  void applyWlanConfig();
  // Startet den eigenen Fallback-Access-Point (SSID/PSK "installer", siehe
  // lastenheft.txt Abschnitt 8/12: "eigener Access Point", nicht Beitritt zu
  // einem bestehenden Netz).
  void startFallbackAp();
  bool networkOk() const { return _wlanGotIp || _apActive; }

  static NetworkManager* _instance;
  static void onNetworkEvent(WiFiEvent_t event);

  volatile bool _wlanGotIp = false;
  volatile bool _apActive = false;
};
