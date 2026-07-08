#pragma once

// Pinbelegung Sensormeter WLAN (generisches ESP32-WROOM-32 DevKit).
// Anders als beim Sensormeter-Projekt (WT32-ETH01) gibt es keinen
// Ethernet-PHY, der Pins blockiert - daher Arduino-ESP32-Standardbelegung
// fuer I2C, siehe docs/entscheidungen.md.

// --- OLED SSD1306 (I2C, Standardpins) ---
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// --- DHT22 (genau ein Sensor, kein RJ45/Modulstecker) ---
#define DHT_PIN 4
#define DHT_TYPE DHT22

// Bewusst vermiedene Pins (Boot-Strapping bzw. intern am Flash):
// GPIO0, GPIO2, GPIO5, GPIO12, GPIO15 (Strapping), GPIO6-11 (internes
// Flash-SPI, auf DevKits nicht herausgefuehrt). Siehe entscheidungen.md.
