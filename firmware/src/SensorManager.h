#pragma once

#include <Arduino.h>
#include "DataManager.h"
#include "TimeManager.h"

// DHT22-Sensorik (Pflichtenheft 2.1 "SensorTask" / 3.6 "SensorManager"),
// kooperativ wie die uebrigen Manager (siehe docs/entscheidungen.md).
// Genau EIN Sensor - kein Sensor-2-Zweig, kein RJ45/Modulstecker (im
// Unterschied zum Sensormeter-Projekt, dessen SensorManager hier als
// Vorbild diente, siehe dortiges SensorManager.cpp).
class SensorManager {
 public:
  static constexpr uint32_t kReadIntervalMs = 60UL * 1000UL;  // Pflichtenheft: 60s

  SensorManager(DataManager& dataManager, TimeManager& timeManager);

  void begin();
  void loop();

 private:
  DataManager& _data;
  TimeManager& _time;

  uint32_t _lastReadMillis = 0;  // 0 = noch nie gelesen -> sofort beim ersten loop()
  long _lastRecordedHour = -1;

  void readSensor();
};
