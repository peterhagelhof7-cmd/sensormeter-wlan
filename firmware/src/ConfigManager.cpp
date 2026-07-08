#include "ConfigManager.h"

#include <LittleFS.h>
#include <tinyxml2.h>

using namespace tinyxml2;

static const char* CONFIG_PATH = "/config.xml";
static const char* CONFIG_TMP_PATH = "/config.xml.tmp";

namespace {

bool parseBool(const char* value, bool fallback) {
  if (!value) return fallback;
  return strcmp(value, "true") == 0 || strcmp(value, "1") == 0;
}

String attrOrEmpty(const XMLElement* el, const char* name) {
  if (!el) return "";
  const char* v = el->Attribute(name);
  return v ? String(v) : String("");
}

String textOr(const XMLElement* parent, const char* childName, const String& fallback) {
  if (!parent) return fallback;
  const XMLElement* child = parent->FirstChildElement(childName);
  if (!child || !child->GetText()) return fallback;
  return String(child->GetText());
}

}  // namespace

void ConfigManager::begin() {
  if (!load()) {
    Serial.println("[CONFIG] Keine gueltige config.xml gefunden -> Defaults werden angelegt");
    _config = DeviceConfig();
    save();
  }
}

bool ConfigManager::load() {
  if (!LittleFS.exists(CONFIG_PATH)) return false;

  File file = LittleFS.open(CONFIG_PATH, "r");
  if (!file) return false;
  String xml = file.readString();
  file.close();

  return importXml(xml);
}

bool ConfigManager::importXml(const String& xml) {
  XMLDocument doc;
  if (doc.Parse(xml.c_str(), xml.length()) != XML_SUCCESS) {
    Serial.println("[CONFIG] XML-Parse-Fehler");
    return false;
  }

  const XMLElement* root = doc.FirstChildElement("config");
  if (!root) {
    Serial.println("[CONFIG] XML ohne <config>-Wurzelelement");
    return false;
  }

  // Beginnt mit Defaults - Felder, die im XML fehlen, bleiben auf Default
  // (Pflichtenheft: "Default-Werte fallback").
  DeviceConfig cfg;

  const XMLElement* network = root->FirstChildElement("network");
  if (network) {
    const XMLElement* wlan = network->FirstChildElement("wlan");
    if (wlan) {
      cfg.wlanDhcp = parseBool(wlan->Attribute("dhcp"), cfg.wlanDhcp);
      cfg.wlanSsid = attrOrEmpty(wlan, "ssid");
      cfg.wlanPsk = attrOrEmpty(wlan, "psk");
      cfg.wlanIp = attrOrEmpty(wlan, "ip");
      cfg.wlanMask = attrOrEmpty(wlan, "mask");
      cfg.wlanGateway = attrOrEmpty(wlan, "gateway");
    }
  }

  const XMLElement* system = root->FirstChildElement("system");
  cfg.systemName = textOr(system, "name", cfg.systemName);
  cfg.settingsPassword = textOr(system, "password", cfg.settingsPassword);

  const XMLElement* syslog = root->FirstChildElement("syslog");
  cfg.syslogServer = textOr(syslog, "server", cfg.syslogServer);

  const XMLElement* snmp = root->FirstChildElement("snmp");
  if (snmp) {
    String community = attrOrEmpty(snmp, "community");
    if (community.length() > 0) cfg.snmpCommunity = community;
  }

  _config = cfg;
  return true;
}

String ConfigManager::exportXml() const {
  XMLDocument doc;
  XMLElement* root = doc.NewElement("config");
  doc.InsertFirstChild(root);

  XMLElement* network = doc.NewElement("network");
  root->InsertEndChild(network);

  XMLElement* wlan = doc.NewElement("wlan");
  wlan->SetAttribute("dhcp", _config.wlanDhcp ? "true" : "false");
  wlan->SetAttribute("ssid", _config.wlanSsid.c_str());
  wlan->SetAttribute("psk", _config.wlanPsk.c_str());
  wlan->SetAttribute("ip", _config.wlanIp.c_str());
  wlan->SetAttribute("mask", _config.wlanMask.c_str());
  wlan->SetAttribute("gateway", _config.wlanGateway.c_str());
  network->InsertEndChild(wlan);

  XMLElement* system = doc.NewElement("system");
  root->InsertEndChild(system);
  XMLElement* name = doc.NewElement("name");
  name->SetText(_config.systemName.c_str());
  system->InsertEndChild(name);
  XMLElement* password = doc.NewElement("password");
  password->SetText(_config.settingsPassword.c_str());
  system->InsertEndChild(password);

  XMLElement* syslog = doc.NewElement("syslog");
  root->InsertEndChild(syslog);
  XMLElement* server = doc.NewElement("server");
  server->SetText(_config.syslogServer.c_str());
  syslog->InsertEndChild(server);

  XMLElement* snmp = doc.NewElement("snmp");
  snmp->SetAttribute("community", _config.snmpCommunity.c_str());
  root->InsertEndChild(snmp);

  XMLPrinter printer;
  doc.Print(&printer);
  return String(printer.CStr());
}

bool ConfigManager::save() {
  String xml = exportXml();

  File file = LittleFS.open(CONFIG_TMP_PATH, "w");
  if (!file) {
    Serial.println("[CONFIG] Konnte config.xml.tmp nicht oeffnen");
    return false;
  }
  size_t written = file.print(xml);
  file.close();

  if (written != xml.length()) {
    Serial.println("[CONFIG] Schreibfehler beim Speichern (unvollstaendig) - config.xml bleibt unveraendert");
    LittleFS.remove(CONFIG_TMP_PATH);
    return false;
  }

  // Erst die vollstaendig geschriebene Tmp-Datei an die Stelle der alten
  // config.xml verschieben, damit ein Stromausfall waehrend des Schreibens
  // nicht die bisherige, funktionierende Konfiguration zerstoert.
  LittleFS.remove(CONFIG_PATH);
  if (!LittleFS.rename(CONFIG_TMP_PATH, CONFIG_PATH)) {
    Serial.println("[CONFIG] Konnte config.xml.tmp nicht in config.xml umbenennen");
    return false;
  }

  Serial.println("[CONFIG] config.xml gespeichert");
  return true;
}

void ConfigManager::setConfig(const DeviceConfig& config) {
  _config = config;
  save();
}
