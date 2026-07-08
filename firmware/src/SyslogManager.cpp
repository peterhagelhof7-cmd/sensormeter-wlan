#include "SyslogManager.h"

#include <esp_timer.h>
#include <time.h>

static const int SYSLOG_PORT = 514;
static const int SYSLOG_FACILITY = 16;  // local0
static const int SEVERITY_INFO = 6;

SyslogManager::SyslogManager(DataManager& dataManager, ConfigManager& configManager, NetworkManager& networkManager,
                              TimeManager& timeManager)
    : _data(dataManager), _config(configManager), _network(networkManager), _time(timeManager) {}

void SyslogManager::begin() {
  Serial.println("[SYSLOG] Grundgeruest bereit (UDP Port 514)");
}

bool SyslogManager::syslogEnabled() const {
  const String& server = _config.getConfig().syslogServer;
  return server.length() > 0 && server != "0.0.0.0";
}

void SyslogManager::sendSyslog(int severity, const String& message) {
  if (!syslogEnabled()) return;
  if (!_network.isWlanUp()) return;

  IPAddress serverIp;
  if (!serverIp.fromString(_config.getConfig().syslogServer)) return;

  int priority = SYSLOG_FACILITY * 8 + severity;
  // Minimal an RFC 5424 angelehnt (PRI + Version + "-" Platzhalter fuer
  // Timestamp/Procid/Msgid) - die eigentliche Nutzinformation steht im
  // MSG-Teil im Pipe-Format, analog zum Sensormeter-Projekt.
  String packet =
      "<" + String(priority) + ">1 - " + _config.getConfig().systemName + " sensormeter-wlan - - " + message;

  _udp.beginPacket(serverIp, SYSLOG_PORT);
  _udp.print(packet);
  _udp.endPacket();
}

void SyslogManager::sendStatusReport() {
  const DeviceConfig& cfg = _config.getConfig();

  String wlanIp = _network.isWlanUp() ? _network.getWlanIp().toString() : String("-");
  String rssi = _network.isWlanUp() ? String(_network.getWlanRssi()) : String("n/a");

  SensorReading s = _data.getSensor();
  String sensorValue = s.valid ? (String(s.temperature, 1) + "C/" + String(s.humidity, 0) + "%") : String("--");

  String isoTime = "unsynced";
  if (_time.isSynced()) {
    time_t now = time(nullptr);
    struct tm ti;
    localtime_r(&now, &ti);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S%z", &ti);
    isoTime = buf;
  }

  unsigned long uptimeSec = (unsigned long)(esp_timer_get_time() / 1000000ULL);
  char uptimeBuf[16];
  snprintf(uptimeBuf, sizeof(uptimeBuf), "%02lu:%02lu:%02lu", uptimeSec / 3600, (uptimeSec / 60) % 60, uptimeSec % 60);

  // Format analog zum Sensormeter-Projekt, ohne LAN-IP/Sensor-2:
  // Systemname | WLAN-IP | RSSI | Sensor | ISO-Zeit | Uptime
  String message =
      cfg.systemName + " | " + wlanIp + " | " + rssi + " | " + sensorValue + " | " + isoTime + " | " + String(uptimeBuf);

  sendSyslog(SEVERITY_INFO, message);
}

void SyslogManager::checkForNewLogEntries() {
  LogEntry entries[DataManager::LOG_CAPACITY];
  size_t count = _data.getLogEntriesAfter(_lastSeenLogSequence, entries, DataManager::LOG_CAPACITY);
  for (size_t i = 0; i < count; i++) {
    sendSyslog(entries[i].severity, entries[i].message);
    _lastSeenLogSequence = entries[i].sequence;
  }
}

void SyslogManager::loop() {
  checkForNewLogEntries();  // Fehler-Events: sofort (naechster Loop-Tick)

  // Statusreport bei jedem Sensorzyklus (Pflichtenheft: "UDP Versand bei
  // jedem Sensorzyklus") - erkannt an einer Aenderung von lastReadMillis,
  // statt eines eigenen, potenziell abdriftenden Timers.
  unsigned long currentReadMillis = _data.getSensor().lastReadMillis;
  if (currentReadMillis != 0 && currentReadMillis != _lastSensorReadMillisSeen) {
    _lastSensorReadMillisSeen = currentReadMillis;
    sendStatusReport();
  }
}
