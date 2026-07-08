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
  static const size_t RINGBUFFER_SIZE = 168;  // 7 Tage * 24 Stunden
  static const size_t LOG_CAPACITY = 5;       // Lastenheft 5.3: "letzte 5 Meldungen"

  void begin();

  SystemState getSystemState();
  void setSystemState(SystemState state);

  SensorReading getSensor();
  void setSensor(const SensorReading& reading);

  void pushHourValue(const HourValue& value);
  size_t getRingbuffer(HourValue* out, size_t maxCount);

  void pushLogEntry(const String& message, int severity = 6);
  size_t getLogEntries(LogEntry* out, size_t maxCount);  // neueste zuerst
  size_t getLogEntriesAfter(unsigned long afterSequence, LogEntry* out, size_t maxCount);

 private:
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
