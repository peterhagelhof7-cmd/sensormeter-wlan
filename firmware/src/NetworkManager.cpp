#include "NetworkManager.h"

NetworkManager* NetworkManager::_instance = nullptr;

// Lastenheft Abschnitt 8: 5 Minuten ohne WLAN-IP -> eigener Fallback-Access-
// Point "installer" (Konvention wie beim Sensormeter-Projekt, siehe
// docs/entscheidungen.md).
static const unsigned long WLAN_CHECK_TIMEOUT_MS = 5UL * 60UL * 1000UL;
// Kurzer Timeout, wenn ueber die Einstellungsseite im Fallback-AP gerade
// neue WLAN-Zugangsdaten eingegeben wurden (DeviceConfig::wlanPendingTest) -
// schnelles Feedback statt 5 Minuten Wartezeit, siehe begin().
static const unsigned long WLAN_TEST_TIMEOUT_MS = 30UL * 1000UL;
static const unsigned long FALLBACK_RETRY_INTERVAL_MS = 30UL * 1000UL;
// Aus dem Fallback-AP heraus alle 2 Minuten aktiv einen erneuten Verbindungs-
// versuch ins konfigurierte WLAN anstossen (Backstop zusaetzlich zum Core-Auto-
// Reconnect, der im AP_STA-Modus ohnehin weiterlaeuft). Sobald es klappt, wird
// der Fallback-AP abgeschaltet (siehe loop()/startFallbackAp()).
static const unsigned long FALLBACK_REJOIN_INTERVAL_MS = 2UL * 60UL * 1000UL;
// Aktiver WLAN-Reconnect-Versuch alle 20s, solange im WLAN_CHECK noch kein
// GOT_IP kam - die Firmware wartet damit nicht mehr passiv auf den ESP32-
// Core-Auto-Reconnect, sondern stoesst selbst regelmaessig einen erneuten
// Verbindungsversuch an (siehe loop()).
static const unsigned long RECONNECT_RETRY_INTERVAL_MS = 20UL * 1000UL;
static const char* FALLBACK_WLAN_SSID = "installer";
static const char* FALLBACK_WLAN_PSK = "installer";

// Eigener Access Point statt Beitritt zu einem bestehenden Netz (siehe
// lastenheft.txt Abschnitt 8/12) - nur IP + Subnetzmaske konfiguriert, kein
// Gateway/DNS, da der AP nicht ins Internet weiterleitet. DHCP-Server laeuft
// automatisch (ESP32-Arduino-Core startet ihn implizit mit WiFi.softAP()).
static const IPAddress FALLBACK_AP_IP(192, 168, 4, 1);
static const IPAddress FALLBACK_AP_SUBNET(255, 255, 255, 0);

NetworkManager::NetworkManager(DataManager& dataManager, ConfigManager& configManager)
    : _data(dataManager), _config(configManager) {
  _instance = this;
}

String NetworkManager::sanitizeHostname(const String& name) {
  String out;
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
      out += c;
    } else if ((c == ' ' || c == '_') && out.length() > 0 && out[out.length() - 1] != '-') {
      out += '-';
    }
  }
  while (out.length() > 0 && out[out.length() - 1] == '-') out.remove(out.length() - 1);
  while (out.length() > 0 && out[0] == '-') out.remove(0, 1);
  if (out.isEmpty()) out = "sensormeter-wlan";
  return out;
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
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.println("[NET] Client mit Fallback-Access-Point verbunden");
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.println("[NET] Client vom Fallback-Access-Point getrennt");
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

void NetworkManager::startFallbackAp() {
  // AP_STA statt AP-only: der eigene Access-Point ist erreichbar, die Station
  // versucht PARALLEL weiter, das konfigurierte WLAN zu erreichen (Core-Auto-
  // Reconnect + aktiver Rejoin in loop()). So heilt sich der Fallback selbst,
  // sobald das WLAN zurueckkommt - ohne Reboot. (Frueher: WIFI_MODE_AP +
  // WiFi.disconnect(true) schaltete die Station komplett ab -> Sackgasse, aus
  // der das Geraet nur per Reboot/Neukonfiguration wieder herausfand.)
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.softAPConfig(FALLBACK_AP_IP, FALLBACK_AP_IP, FALLBACK_AP_SUBNET);
  _apActive = WiFi.softAP(FALLBACK_WLAN_SSID, FALLBACK_WLAN_PSK);

  DeviceConfig cfg = _config.getConfig();
  if (cfg.wlanSsid.length() > 0) {
    WiFi.begin(cfg.wlanSsid.c_str(), cfg.wlanPsk.c_str());
    applyWlanConfig();
  }
  _lastFallbackRejoinMillis = millis();

  if (_apActive) {
    Serial.print("[NET] Fallback-Access-Point \"");
    Serial.print(FALLBACK_WLAN_SSID);
    Serial.print("\" gestartet (AP_STA, Station versucht weiter), IP: ");
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println("[NET] Fallback-Access-Point konnte nicht gestartet werden");
  }
}

void NetworkManager::begin() {
  _data.setSystemState(SystemState::INIT);
  Serial.println("[NET] Init: WLAN");

  WiFi.onEvent(onNetworkEvent);
  WiFi.mode(WIFI_MODE_STA);
  // Core-seitigen Auto-Reconnect aktivieren, damit die Verbindung nach einem
  // kurzen WLAN-Aussetzer selbsttaetig wieder aufgebaut wird (zusaetzlich zum
  // aktiven Reconnect in loop()).
  WiFi.setAutoReconnect(true);
  // WLAN-Powersave (Modem-Sleep) abschalten - der ESP32-Default vertraegt sich
  // mit manchen APs schlecht und fuehrt zu haeufigen kurzen Verbindungsabbruechen
  // (auf Hardware beobachtet: ~alle 20s ein Drop). Ohne Sleep bleibt die
  // Verbindung deutlich stabiler, Kosten: minimal hoehere Stromaufnahme.
  WiFi.setSleep(false);
  WiFi.setHostname(sanitizeHostname(_config.getConfig().systemName).c_str());

  DeviceConfig cfg = _config.getConfig();

  // Einmal-Flag konsumieren: wurde ueber die Einstellungsseite im
  // Fallback-AP gerade ein neues WLAN eingetragen, diesen einen Boot mit
  // kurzem Timeout pruefen, dann das Flag sofort wieder loeschen (siehe
  // DeviceConfig::wlanPendingTest) - unabhaengig vom Ergebnis nur ein
  // Versuch mit dem kurzen Timeout.
  if (cfg.wlanPendingTest) {
    _wlanCheckTimeoutMs = WLAN_TEST_TIMEOUT_MS;
    cfg.wlanPendingTest = false;
    _config.setConfig(cfg);
    Serial.println("[NET] Neu eingetragenes WLAN wird getestet (kurzer Timeout)");
  } else {
    _wlanCheckTimeoutMs = WLAN_CHECK_TIMEOUT_MS;
  }

  if (cfg.wlanSsid.length() > 0) {
    WiFi.begin(cfg.wlanSsid.c_str(), cfg.wlanPsk.c_str());
    applyWlanConfig();
    Serial.println("[NET] WLAN-Verbindungsversuch gestartet (konfiguriertes Netz)");
  } else {
    Serial.println("[NET] Kein WLAN konfiguriert - warte auf Fallback-Timeout");
  }

  _data.setSystemState(SystemState::WLAN_CHECK);
  _networkCheckStartedMillis = millis();
  _lastReconnectAttemptMillis = millis();
}

void NetworkManager::loop() {
  SystemState state = _data.getSystemState();

  // --- Selbstheilung, solange der eigene Fallback-AP laeuft ---
  // Sobald _apActive gilt, meldet networkOk() dauerhaft "ok" (der AP zaehlt mit),
  // sodass die normale WLAN_CHECK/RUN_NORMAL-Logik das konfigurierte WLAN nie
  // wieder pruefen wuerde - ohne dies bliebe das Geraet bis zum Reboot im AP.
  // Deshalb hier explizit auf die Station-IP (_wlanGotIp) achten und periodisch
  // aktiv den Rejoin anstossen; der AP bleibt via AP_STA die ganze Zeit erreichbar.
  if (_apActive) {
    if (_wlanGotIp) {
      // Konfiguriertes WLAN (wieder) erreicht -> Fallback-AP abschalten.
      WiFi.softAPdisconnect(true);
      _apActive = false;
      _wlanDownSinceMillis = 0;
      _data.pushLogEntry("Netzwerk: konfiguriertes WLAN aus dem Fallback-AP wiederhergestellt",
                          DataManager::SEVERITY_INFO);
      _data.setSystemState(SystemState::RUN_NORMAL);
      return;
    }
    // Betrieb laeuft auf dem AP weiter (wie bisher: der AP zaehlt als RUN_NORMAL).
    if (state != SystemState::RUN_NORMAL) {
      _data.setSystemState(SystemState::RUN_NORMAL);
    }
    if (_config.getConfig().wlanSsid.length() > 0 &&
        millis() - _lastFallbackRejoinMillis > FALLBACK_REJOIN_INTERVAL_MS) {
      DeviceConfig cfg = _config.getConfig();
      Serial.println("[NET] Fallback-AP aktiv - erneuter Rejoin-Versuch ins konfigurierte WLAN");
      WiFi.mode(WIFI_MODE_APSTA);
      WiFi.begin(cfg.wlanSsid.c_str(), cfg.wlanPsk.c_str());
      applyWlanConfig();
      _lastFallbackRejoinMillis = millis();
    }
    return;
  }

  if (state == SystemState::WLAN_CHECK) {
    if (networkOk()) {
      _data.setSystemState(SystemState::RUN_NORMAL);
      // Nur loggen, wenn dies auf einen getrackten Ausfall folgt (nicht beim
      // allerersten Verbindungsaufbau nach dem Boot, siehe _wlanDownSinceMillis).
      if (_wlanDownSinceMillis != 0) {
        unsigned long downSec = (millis() - _wlanDownSinceMillis) / 1000UL;
        _data.pushLogEntry("Netzwerk: WLAN wieder verbunden (Ausfall " + String(downSec) + "s)",
                            DataManager::SEVERITY_INFO);
        _wlanDownSinceMillis = 0;
      }
    } else if (millis() - _networkCheckStartedMillis > _wlanCheckTimeoutMs) {
      _data.pushLogEntry("Netzwerk: kein WLAN, starte eigenen Access-Point \"installer\"",
                          DataManager::SEVERITY_ERROR);
      startFallbackAp();
      _lastFallbackJoinAttemptMillis = millis();
      _data.setSystemState(SystemState::FALLBACK_MODE);
    } else if (_config.getConfig().wlanSsid.length() > 0 &&
               millis() - _lastReconnectAttemptMillis > RECONNECT_RETRY_INTERVAL_MS) {
      // Aktiv erneut verbinden, statt nur auf den Core-Auto-Reconnect zu warten -
      // sonst bleibt das Geraet nach einem WLAN-Aussetzer bis zum 5-min-Timeout
      // auf 0.0.0.0 haengen und kippt dann unnoetig in den Fallback-AP.
      Serial.println("[NET] WLAN weg - aktiver Reconnect-Versuch");
      WiFi.reconnect();
      _lastReconnectAttemptMillis = millis();
    }
  } else if (state == SystemState::FALLBACK_MODE) {
    // Hierher kommt man nur, wenn WiFi.softAP() fehlschlug (_apActive==false;
    // sonst haette der Selbstheilungs-Block oben uebernommen) - erneut versuchen.
    if (millis() - _lastFallbackJoinAttemptMillis > FALLBACK_RETRY_INTERVAL_MS) {
      startFallbackAp();
      _lastFallbackJoinAttemptMillis = millis();
    }
  } else if (state == SystemState::RUN_NORMAL && !networkOk()) {
    // WARNING statt ERROR: ein einzelner Aussetzer ist ein erwartetes,
    // selbstheilendes Ereignis (aktiver Reconnect siehe oben) - kein Fehler,
    // der Admin-Aufmerksamkeit braucht. Die "wieder verbunden"-Gegenseite
    // steht oben im WLAN_CHECK-Zweig.
    _data.pushLogEntry("Netzwerk: WLAN-Verbindung verloren", DataManager::SEVERITY_WARNING);
    _apActive = false;
    _wlanCheckTimeoutMs = WLAN_CHECK_TIMEOUT_MS;
    _wlanDownSinceMillis = millis();
    _data.setSystemState(SystemState::WLAN_CHECK);
    _networkCheckStartedMillis = millis();
    _lastReconnectAttemptMillis = millis();
  }
}

IPAddress NetworkManager::getWlanIp() const {
  if (_apActive) return WiFi.softAPIP();
  return _wlanGotIp ? WiFi.localIP() : IPAddress(0, 0, 0, 0);
}

IPAddress NetworkManager::getWlanGateway() const {
  // Eigener Fallback-AP hat keinen Gateway/kein Routing ins Internet (siehe
  // startFallbackAp()) - eigene IP zurueckgeben, konsistent mit der
  // softAPConfig()-Einstellung.
  if (_apActive) return WiFi.softAPIP();
  return _wlanGotIp ? WiFi.gatewayIP() : IPAddress(0, 0, 0, 0);
}

IPAddress NetworkManager::getWlanDns() const {
  if (_apActive) return IPAddress(0, 0, 0, 0);
  return _wlanGotIp ? WiFi.dnsIP() : IPAddress(0, 0, 0, 0);
}

String NetworkManager::getWlanMac() const {
  return WiFi.macAddress();
}

String NetworkManager::getWlanSsid() const {
  if (_apActive) return String(FALLBACK_WLAN_SSID);
  return _wlanGotIp ? WiFi.SSID() : String("");
}

int NetworkManager::getWlanRssi() const {
  return _wlanGotIp ? WiFi.RSSI() : 0;
}
