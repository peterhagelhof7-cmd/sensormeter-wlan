#include "NetworkManager.h"

NetworkManager* NetworkManager::_instance = nullptr;

// Lastenheft Abschnitt 8: 5 Minuten ohne WLAN-IP -> Wechsel auf das
// Recovery-WLAN "installer" (Konvention wie beim Sensormeter-Projekt,
// siehe docs/entscheidungen.md).
static const unsigned long WLAN_CHECK_TIMEOUT_MS = 5UL * 60UL * 1000UL;
static const unsigned long FALLBACK_RETRY_INTERVAL_MS = 30UL * 1000UL;
static const char* FALLBACK_WLAN_SSID = "installer";
static const char* FALLBACK_WLAN_PSK = "installer";

NetworkManager::NetworkManager(DataManager& dataManager, ConfigManager& configManager)
    : _data(dataManager), _config(configManager) {
  _instance = this;
}

void NetworkManager::onNetworkEvent(WiFiEvent_t event) {
  if (!_instance) return;

  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[NET] WLAN verbunden");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.print("[NET] WLAN-IP erhalten: ");
      Serial.println(WiFi.localIP());
      _instance->_wlanGotIp = true;
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("[NET] WLAN-Verbindung verloren");
      _instance->_wlanGotIp = false;
      break;
    default:
      break;
  }
}

void NetworkManager::applyWlanConfig() {
  const DeviceConfig& cfg = _config.getConfig();
  if (cfg.wlanDhcp) return;

  IPAddress ip, mask, gateway;
  if (ip.fromString(cfg.wlanIp) && mask.fromString(cfg.wlanMask) && gateway.fromString(cfg.wlanGateway)) {
    // WiFi.config() setzt DNS NUR, wenn dns1/dns2 != 0.0.0.0 sind (siehe
    // WiFiGeneric.cpp::set_esp_interface_dns) - ohne explizite Angabe bliebe
    // bei statischer IP sonst gar kein DNS-Server gesetzt und die
    // NTP-Hostnamensaufloesung ("de.pool.ntp.org", siehe TimeManager) wuerde
    // fehlschlagen. Leeres/ungueltiges DNS-Feld -> Gateway als DNS
    // verwenden (bei den meisten Heimroutern ohnehin ein funktionierender
    // DNS-Resolver).
    IPAddress dns;
    if (cfg.wlanDns.length() == 0 || !dns.fromString(cfg.wlanDns)) {
      dns = gateway;
    }
    WiFi.config(ip, gateway, mask, dns);
    Serial.println("[NET] WLAN: statische IP angewendet (DNS: " + dns.toString() + ")");
  } else {
    Serial.println("[NET] WLAN: statische IP konfiguriert, aber ungueltig -> bleibe bei DHCP");
  }
}

void NetworkManager::begin() {
  _data.setSystemState(SystemState::INIT);
  Serial.println("[NET] Init: WLAN");

  WiFi.onEvent(onNetworkEvent);
  WiFi.mode(WIFI_MODE_STA);

  const DeviceConfig& cfg = _config.getConfig();
  if (cfg.wlanSsid.length() > 0) {
    WiFi.begin(cfg.wlanSsid.c_str(), cfg.wlanPsk.c_str());
    applyWlanConfig();
    Serial.println("[NET] WLAN-Verbindungsversuch gestartet (konfiguriertes Netz)");
  } else {
    Serial.println("[NET] Kein WLAN konfiguriert - warte auf Fallback-Timeout");
  }

  _data.setSystemState(SystemState::WLAN_CHECK);
  _networkCheckStartedMillis = millis();
}

void NetworkManager::loop() {
  SystemState state = _data.getSystemState();

  if (state == SystemState::WLAN_CHECK) {
    if (networkOk()) {
      _data.setSystemState(SystemState::RUN_NORMAL);
    } else if (millis() - _networkCheckStartedMillis > WLAN_CHECK_TIMEOUT_MS) {
      _data.pushLogEntry("Netzwerk: kein WLAN nach 5 Minuten, wechsle auf Recovery-WLAN \"installer\"", 3);
      WiFi.disconnect();
      WiFi.begin(FALLBACK_WLAN_SSID, FALLBACK_WLAN_PSK);
      _inFallbackWlan = true;
      _lastFallbackJoinAttemptMillis = millis();
      _data.setSystemState(SystemState::FALLBACK_MODE);
    }
  } else if (state == SystemState::FALLBACK_MODE) {
    if (networkOk()) {
      _data.setSystemState(SystemState::RUN_NORMAL);
    } else if (millis() - _lastFallbackJoinAttemptMillis > FALLBACK_RETRY_INTERVAL_MS) {
      WiFi.begin(FALLBACK_WLAN_SSID, FALLBACK_WLAN_PSK);
      _lastFallbackJoinAttemptMillis = millis();
    }
  } else if (state == SystemState::RUN_NORMAL && !networkOk()) {
    _data.pushLogEntry("Netzwerk: WLAN-Verbindung verloren", 3);
    _inFallbackWlan = false;
    _data.setSystemState(SystemState::WLAN_CHECK);
    _networkCheckStartedMillis = millis();
  }
}

IPAddress NetworkManager::getWlanIp() const {
  return _wlanGotIp ? WiFi.localIP() : IPAddress(0, 0, 0, 0);
}

IPAddress NetworkManager::getWlanGateway() const {
  return _wlanGotIp ? WiFi.gatewayIP() : IPAddress(0, 0, 0, 0);
}

IPAddress NetworkManager::getWlanDns() const {
  return _wlanGotIp ? WiFi.dnsIP() : IPAddress(0, 0, 0, 0);
}

String NetworkManager::getWlanMac() const {
  return WiFi.macAddress();
}

String NetworkManager::getWlanSsid() const {
  return _wlanGotIp ? WiFi.SSID() : String("");
}

int NetworkManager::getWlanRssi() const {
  return _wlanGotIp ? WiFi.RSSI() : 0;
}
