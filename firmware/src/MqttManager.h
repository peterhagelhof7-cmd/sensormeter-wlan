#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include "ConfigManager.h"
#include "DataManager.h"
#include "NetworkManager.h"

// Home-Assistant-Anbindung ueber MQTT-Discovery (siehe
// sensormeter-poe/repo/docs/lastenheft.txt Abschnitt 16 fuer das
// vollstaendige Feature-Design, hier auf die Sensor-Rolle beschraenkt -
// kein Relais/Aktor, da dieses Board keinen RJ45-Modularanschluss hat,
// siehe docs/entscheidungen.md). Deaktiviert, solange kein Broker
// konfiguriert ist (mqttEnabled=false, Default). Publiziert bei jedem
// Sensorzyklus (erkannt wie bei SyslogManager an einer Aenderung von
// lastReadMillis) - Discovery-Payload nur einmal je (Re-)Connect.

class MqttManager {
 public:
  MqttManager(DataManager& dataManager, ConfigManager& configManager, NetworkManager& networkManager);

  void begin();
  void loop();

 private:
  DataManager& _data;
  ConfigManager& _config;
  NetworkManager& _network;

  WiFiClient _transport;
  PubSubClient _client;

  bool _discoverySent = false;
  unsigned long _lastSensorReadMillisSeen = 0;
  unsigned long _lastReconnectAttemptMillis = 0;

  bool mqttEnabled() const;
  void ensureConnected();
  void publishDiscovery();
  void publishState();
  String topicPrefix() const;
};
