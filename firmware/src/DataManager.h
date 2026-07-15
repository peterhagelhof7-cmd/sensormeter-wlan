#pragma once

#include <Arduino.h>
#include <freertos/semphr.h>
#include "SystemState.h"

// Zentrale Datenhaltung (Pflichtenheft 3.1), mutex-geschuetzt von Anfang an
// (Vorbild: DataManager im Sensormeter-Projekt) - async Webserver (P5) und
// Sensor-/Netzwerk-Polling greifen aus unterschiedlichen Kontexten zu.
//
// Anders als beim Sensormeter-Projekt: genau EIN Sensor (kein sensor2),
// kein LAN-Statusfeld (kein Ethernet).

struct HourValue {
  time_t timestamp = 0;
  float temperature = NAN;
  float humidity = NAN;
};

struct SensorReading {
  float temperature = NAN;
  float humidity = NAN;
  bool valid = false;
  unsigned long lastReadMillis = 0;
};

struct LogEntry {
  time_t timestamp = 0;
  String message;
  int severity = 6;             // Syslog-Konvention: 3 = Error, 6 = Informational
  unsigned long sequence = 0;   // fortlaufend, damit SyslogManager (P7) neue
                                 // Eintraege erkennen kann, ohne zu pollen
};

class DataManager {
 public:
  // Benannte Severity-Stufen (RFC5424-artige Skala, wie bereits in
  // SyslogManager verwendet) statt roher Zahlen an den Aufrufstellen.
  // WARNING (4) ist neu: fuer transiente, selbstheilende Ereignisse (z.B.
  // ein kurzer WLAN-Aussetzer, der sich von selbst erholt) - getrennt von
  // ERROR (3), das weiterhin echten, admin-relevanten Fehlern vorbehalten
  // bleibt.
  static const int SEVERITY_ERROR = 3;
  static const int SEVERITY_WARNING = 4;
  static const int SEVERITY_INFO = 6;

  static const size_t RINGBUFFER_SIZE = 168;  // 7 Tage * 24 Stunden
  static const size_t LOG_CAPACITY = 5;       // Lastenheft 5.3: "letzte 5 Meldungen"
  // Persistenter Log-Puffer auf LittleFS (/log.txt, bei Ueberschreiten
  // umbenannt nach /log.old.txt - siehe appendLogFile()): 32 KB je Datei,
  // max. 64 KB gesamt. Die LittleFS-Datenpartition (min_spiffs.csv) hat nur
  // ~128 KB insgesamt; config.xml/history.csv/Logo belegen zusammen nur
  // wenige KB, das laesst >60 KB Reserve. Bei ~80-100 Byte je Zeile sind das
  // ca. 350-400 Eintraege je Datei (700-800 gesamt) - reicht auch bei
  // starkem WLAN-Flapping fuer mehrere Tage Verlauf.
  static const size_t LOG_FILE_MAX_BYTES = 32UL * 1024UL;

  void begin();

  SystemState getSystemState();
  void setSystemState(SystemState state);

  SensorReading getSensor();
  void setSensor(const SensorReading& reading);

  // pushHourValue() persistiert bei jedem Aufruf (1x/Stunde) nach
  // /history.csv auf LittleFS, damit der 7-Tage-Verlauf einen Neustart
  // uebersteht - vernachlaessigbarer Flash-Verschleiss bei stuendlicher
  // Schreibfrequenz (siehe docs/entscheidungen.md, Vorbild: Sensormeter-
  // Projekt).
  void pushHourValue(const HourValue& value);
  size_t getRingbuffer(HourValue* out, size_t maxCount);

  // Muss erst NACH StorageManager::begin() (LittleFS-Mount) aufgerufen
  // werden - main.cpp ruft dataManager.begin() bewusst frueher auf.
  void loadRingbuffer();

  void pushLogEntry(const String& message, int severity = SEVERITY_INFO);
  size_t getLogEntries(LogEntry* out, size_t maxCount);  // neueste zuerst
  size_t getLogEntriesAfter(unsigned long afterSequence, LogEntry* out, size_t maxCount);

 private:
  void saveRingbuffer();
  // Haengt einen formatierten Eintrag an /log.txt an (Rotation nach
  // /log.old.txt bei Ueberschreiten von LOG_FILE_MAX_BYTES) - siehe
  // DataManager.cpp fuer das Zeilenformat. Unabhaengig vom RAM-Ringpuffer
  // oben (_log/LOG_CAPACITY), der weiterhin nur fuer die "letzte 5
  // Meldungen"-Webseite und den SyslogManager-Versand dient.
  void appendLogFile(time_t timestamp, int severity, const String& message);

  SemaphoreHandle_t _mutex = nullptr;

  SystemState _systemState = SystemState::BOOT;
  SensorReading _sensor;

  HourValue _ringbuffer[RINGBUFFER_SIZE];
  size_t _ringbufferCount = 0;
  size_t _ringbufferNextIndex = 0;

  LogEntry _log[LOG_CAPACITY];
  size_t _logCount = 0;
  size_t _logNextIndex = 0;
  unsigned long _logSequenceCounter = 0;
};
