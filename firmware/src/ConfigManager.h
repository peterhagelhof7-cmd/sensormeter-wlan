#pragma once

#include <Arduino.h>

// Laufzeitkonfiguration gemaess docs/lastenheft.txt Abschnitt 10 (config.xml
// auf LittleFS), Persistenz per tinyxml2 (vendored, siehe lib/tinyxml2/).
//
// Anders als beim Sensormeter-Projekt: kein <lan>-Abschnitt (kein Ethernet),
// kein <sensors><sensor2>-Abschnitt (genau ein Sensor).
//
// Schema (config.xml):
//
// <config>
//   <network>
//     <wlan dhcp="true" ssid="" psk="" ip="" mask="" gateway="" dns=""/>
//   </network>
//   <system>
//     <name>Sensormeter WLAN</name>
//     <password>installer</password>
//   </system>
//   <syslog>
//     <server>0.0.0.0</server>
//   </syslog>
//   <sensor tempOffset="0.0" humOffset="0.0"/>
//   <snmp community="public"/>
// </config>

struct DeviceConfig {
  String systemName = "Sensormeter WLAN";
  String settingsPassword = "installer";

  // Kalibrierkorrektur (fester Grad-/Prozent-Versatz, positiv oder
  // negativ) - falls der DHT22 systematisch von einem Referenzwert
  // abweicht. Wird direkt in SensorManager auf den validierten Rohmesswert
  // angewendet, damit Anzeige, SNMP UND Stundenwerte/CSV immer denselben,
  // bereits korrigierten Wert sehen (siehe docs/entscheidungen.md).
  float sensorTempOffset = 0.0f;
  float sensorHumOffset = 0.0f;

  bool wlanDhcp = true;
  String wlanIp;
  String wlanMask;
  String wlanGateway;
  String wlanDns;  // leer = Gateway als DNS verwenden (siehe NetworkManager::applyWlanConfig)
  String wlanSsid;
  String wlanPsk;

  String syslogServer = "0.0.0.0";

  String snmpCommunity = "public";
};

class ConfigManager {
 public:
  // Laedt config.xml von LittleFS. Fehlt die Datei oder ist sie ungueltig,
  // werden Defaults verwendet und sofort als neue config.xml gespeichert.
  void begin();

  const DeviceConfig& getConfig() const { return _config; }

  // Uebernimmt eine neue Konfiguration und speichert sie sofort (fuer die
  // Einstellungsseite in P5).
  void setConfig(const DeviceConfig& config);

  // XML-Import/-Export. importXml uebernimmt nur bei erfolgreichem Parsen
  // und speichert dann.
  bool importXml(const String& xml);
  String exportXml() const;

  bool save();

 private:
  DeviceConfig _config;
  bool load();
};
