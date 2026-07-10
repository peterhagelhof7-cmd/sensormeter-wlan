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

// --- Taster: der bereits auf jedem ESP32-DevKit vorhandene BOOT-Knopf,
// kein zusaetzliches Bauteil noetig. Nach dem Booten ganz normal als
// Eingang lesbar (aktiv LOW, interner Pullup) - nur waehrend eines Resets
// beeinflusst sein Zustand den Bootmodus, siehe DisplayManager.h. ---
#define BUTTON_BOOT_PIN 0

// Bewusst vermiedene Pins fuer NEUE Peripherie (Boot-Strapping bzw. intern
// am Flash): GPIO0 (siehe oben, hier als bestehender Taster erlaubt), GPIO2,
// GPIO5, GPIO12, GPIO15 (Strapping), GPIO6-11 (internes Flash-SPI, auf
// DevKits nicht herausgefuehrt). Siehe entscheidungen.md.
