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
//     <wlan dhcp="true" ssid="" psk="" ip="" mask="" gateway="" dns="" pendingTest="false"/>
//   </network>
//   <system>
//     <name>Sensormeter WLAN</name>
//     <password>installer</password>
//   </system>
//   <syslog>
//     <server>0.0.0.0</server>
//   </syslog>
//   <sensor tempOffset="0.0" humOffset="0.0" calibratedTs="0"/>
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
  // Wall-Clock-Zeitpunkt (time(nullptr)), zu dem die Offsets zuletzt
  // TATSAECHLICH geaendert wurden (nicht nur gespeichert - siehe
  // WebServerManager::handleApiConfigPost()). 0 = noch nie kalibriert.
  uint32_t sensorCalibratedTs = 0;

  bool wlanDhcp = true;
  String wlanIp;
  String wlanMask;
  String wlanGateway;
  String wlanDns;  // leer = Gateway als DNS verwenden (siehe NetworkManager::applyWlanConfig)
  String wlanSsid;
  String wlanPsk;
  // Einmal-Flag: nach Eingabe neuer WLAN-Zugangsdaten ueber die
  // Einstellungsseite im Fallback-Access-Point gesetzt, damit
  // NetworkManager den anschliessenden Verbindungsversuch nur kurz statt
  // 5 Minuten abwartet (schnelles Feedback), bevor er wieder auf den
  // Fallback-AP zurueckfaellt. Wird beim naechsten Boot sofort gelesen und
  // geloescht - ueberlebt also nur genau einen Neustart (siehe
  // NetworkManager::begin()).
  bool wlanPendingTest = false;

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
