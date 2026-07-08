#pragma once

#include <Arduino.h>

// Laufzeitkonfiguration gemaess docs/lastenheft.txt Abschnitt 10 (config.xml
// auf LittleFS). P0: nur Gerüst mit Defaults im RAM - echtes Laden/Speichern
// auf LittleFS + XML-Import/-Export folgt in P2 (siehe
// docs/implementierungsplan.html).
//
// Anders als beim Sensormeter-Projekt: kein <lan>-Abschnitt (kein Ethernet),
// kein <sensors><sensor2>-Abschnitt (genau ein Sensor).
//
// Ziel-Schema (config.xml, ab P2):
//
// <config>
//   <network>
//     <wlan dhcp="true" ssid="" psk="" ip="" mask="" gateway=""/>
//   </network>
//   <system>
//     <name>Sensormeter WLAN</name>
//     <password>installer</password>
//   </system>
//   <syslog>
//     <server>0.0.0.0</server>
//   </syslog>
//   <snmp community="public"/>
// </config>

struct DeviceConfig {
  String systemName = "Sensormeter WLAN";
  String settingsPassword = "installer";

  bool wlanDhcp = true;
  String wlanIp;
  String wlanMask;
  String wlanGateway;
  String wlanSsid;
  String wlanPsk;

  String syslogServer = "0.0.0.0";

  String snmpCommunity = "public";
};

class ConfigManager {
 public:
  // P0: setzt nur Defaults (kein LittleFS-Zugriff). Ab P2: laedt config.xml,
  // legt bei Fehlen/Ungueltigkeit Defaults an und speichert sie sofort.
  void begin();

  const DeviceConfig& getConfig() const { return _config; }

  // Ab P5 (Einstellungsseite) genutzt; speichert ab P2 sofort auf LittleFS.
  void setConfig(const DeviceConfig& config);

 private:
  DeviceConfig _config;
};
