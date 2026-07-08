#include "SensorManager.h"

#include <DHT.h>
#include "pins.h"

// Plausibilitaetsgrenzen laut DHT22/AM2302-Datenblatt (-40..80 C,
// 0..100% rF) - identisch zur Grenze des externen DHT22-Zweigs im
// Sensormeter-Projekt (dort SensorManager.cpp, plausibleDht22()).
static bool plausibleDht22(float t, float h) {
  return t >= -40.0f && t <= 80.0f && h >= 0.0f && h <= 100.0f;
}

static DHT dht(DHT_PIN, DHT_TYPE);

SensorManager::SensorManager(DataManager& dataManager, TimeManager& timeManager)
    : _data(dataManager), _time(timeManager) {}

void SensorManager::begin() {
  dht.begin();
  Serial.println("[SENSOR] DHT22 initialisiert");
}

void SensorManager::readSensor() {
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();

  SensorReading reading;
  reading.lastReadMillis = millis();

  if (isnan(humidity) || isnan(temperature)) {
    _data.pushLogEntry("Sensor: Fehler beim Lesen des DHT22", 3);
    _data.setSensor(reading);  // reading.valid bleibt false
    return;
  }
  if (!plausibleDht22(temperature, humidity)) {
    _data.pushLogEntry("Sensor: unplausibler Wert verworfen", 3);
    _data.setSensor(reading);
    return;
  }

  reading.temperature = temperature;
  reading.humidity = humidity;
  reading.valid = true;
  _data.setSensor(reading);

  // Stuendliche Ringpuffer-Speicherung - nur wenn Zeit per NTP synchronisiert
  // ist (Lastenheft 5.2 Graph / Pflichtenheft 4.1 Ringpuffer), sonst waere
  // der Zeitstempel bedeutungslos (Uptime statt echter Uhrzeit).
  if (!_time.isSynced()) return;

  time_t now = time(nullptr);
  long hourIndex = now / 3600;
  if (hourIndex != _lastRecordedHour) {
    HourValue hv;
    hv.timestamp = now;
    hv.temperature = temperature;
    hv.humidity = humidity;
    _data.pushHourValue(hv);
    _lastRecordedHour = hourIndex;
  }
}

void SensorManager::loop() {
  // Erster Durchlauf (_lastReadMillis == 0): sofort lesen statt 60s zu warten.
  if (_lastReadMillis != 0 && millis() - _lastReadMillis < kReadIntervalMs) return;
  _lastReadMillis = millis();

  readSensor();
}
