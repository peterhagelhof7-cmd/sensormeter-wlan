#include "MqttManager.h"

#include <ArduinoJson.h>

namespace {
const unsigned long RECONNECT_INTERVAL_MS = 5000;
}  // namespace

MqttManager::MqttManager(DataManager& dataManager, ConfigManager& configManager, NetworkManager& networkManager)
    : _data(dataManager), _config(configManager), _network(networkManager), _client(_transport) {}

void MqttManager::begin() {
  Serial.println("[MQTT] Grundgeruest bereit");
}

bool MqttManager::mqttEnabled() const {
  const DeviceConfig& cfg = _config.getConfig();
  return cfg.mqttEnabled && cfg.mqttServer.length() > 0;
}

String MqttManager::topicPrefix() const {
  return NetworkManager::sanitizeHostname(_config.getConfig().systemName);
}

void MqttManager::ensureConnected() {
  const DeviceConfig& cfg = _config.getConfig();

  // Server/Port koennten sich seit dem letzten setServer()-Aufruf geaendert
  // haben (Einstellungsseite) - PubSubClient uebernimmt neue Werte erst
  // nach dem naechsten setServer()-Aufruf, daher hier bei jedem
  // Verbindungsversuch neu setzen (billig, kein Netzwerkzugriff).
  _client.setServer(cfg.mqttServer.c_str(), cfg.mqttPort);

  unsigned long now = millis();
  if (now - _lastReconnectAttemptMillis < RECONNECT_INTERVAL_MS) return;
  _lastReconnectAttemptMillis = now;

  String clientId = "sensormeter-wlan-" + topicPrefix();
  bool ok;
  if (cfg.mqttUser.length() > 0) {
    ok = _client.connect(clientId.c_str(), cfg.mqttUser.c_str(), cfg.mqttPassword.c_str());
  } else {
    ok = _client.connect(clientId.c_str());
  }

  if (ok) {
    Serial.println("[MQTT] Verbunden mit Broker " + cfg.mqttServer);
    _discoverySent = false;  // nach jedem (Re-)Connect neu ankuendigen -
                              // guenstig genug, kein Persistenz-Aufwand noetig
  }
}

void MqttManager::publishDiscovery() {
  String prefix = topicPrefix();
  const DeviceConfig& cfg = _config.getConfig();
  String stateTopic = prefix + "/state";

  // Gemeinsamer "device"-Block, damit Home Assistant beide Entities einem
  // Geraet zuordnet statt zwei losen Sensoren.
  auto publishSensorDiscovery = [&](const char* key, const char* name, const char* deviceClass,
                                     const char* unit, const char* valueTemplate) {
    JsonDocument doc;
    doc["name"] = name;
    doc["device_class"] = deviceClass;
    doc["unit_of_measurement"] = unit;
    doc["state_topic"] = stateTopic;
    doc["value_template"] = valueTemplate;
    doc["unique_id"] = prefix + "_" + key;
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = prefix;
    device["name"] = cfg.systemName;
    device["manufacturer"] = "Sensormeter-Familie";
    device["model"] = "Sensormeter WLAN";

    String payload;
    serializeJson(doc, payload);
    String topic = "homeassistant/sensor/" + prefix + "/" + key + "/config";
    _client.publish(topic.c_str(), payload.c_str(), true);  // retain=true
  };

  publishSensorDiscovery("temperature", "Temperatur", "temperature", "°C", "{{ value_json.temperature }}");
  publishSensorDiscovery("humidity", "Luftfeuchte", "humidity", "%", "{{ value_json.humidity }}");

  _discoverySent = true;
  Serial.println("[MQTT] Discovery-Payloads gesendet");
}

void MqttManager::publishState() {
  SensorReading s = _data.getSensor();
  if (!s.valid) return;

  JsonDocument doc;
  doc["temperature"] = serialized(String(s.temperature, 1));
  doc["humidity"] = serialized(String(s.humidity, 1));

  String payload;
  serializeJson(doc, payload);
  String topic = topicPrefix() + "/state";
  _client.publish(topic.c_str(), payload.c_str());
}

void MqttManager::loop() {
  if (!mqttEnabled() || !_network.isWlanUp()) return;

  if (!_client.connected()) {
    ensureConnected();
    return;  // im selben Tick noch nicht weitermachen, erst naechster loop()
  }
  _client.loop();

  if (!_discoverySent) {
    publishDiscovery();
  }

  // State-Update bei jedem Sensorzyklus (Aenderung von lastReadMillis) -
  // gleiches Erkennungsmuster wie SyslogManager::loop().
  unsigned long currentReadMillis = _data.getSensor().lastReadMillis;
  if (currentReadMillis != 0 && currentReadMillis != _lastSensorReadMillisSeen) {
    _lastSensorReadMillisSeen = currentReadMillis;
    publishState();
  }
}
