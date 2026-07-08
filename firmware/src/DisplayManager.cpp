#include "DisplayManager.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <time.h>
#include "SystemState.h"
#include "pins.h"

static const int SCREEN_WIDTH = 128;
static const int SCREEN_HEIGHT = 64;
static const uint8_t SSD1306_I2C_ADDRESS = 0x3C;

static const unsigned long PAGE_INTERVAL_MS = 10UL * 1000UL;  // 10s (lastenheft.txt Abschnitt 11)
static const unsigned long COUNTDOWN_TICK_MS = 1000UL;        // 1x/s

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

DisplayManager::DisplayManager(DataManager& dataManager, ConfigManager& configManager,
                               NetworkManager& networkManager, TimeManager& timeManager)
    : _data(dataManager), _config(configManager), _network(networkManager), _time(timeManager) {}

void DisplayManager::begin() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  _initialized = display.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDRESS);
  if (!_initialized) {
    Serial.println("[DISPLAY] SSD1306 nicht gefunden (I2C 0x3C) - Anzeige deaktiviert");
    return;
  }
  display.cp437(true);
  display.setTextColor(SSD1306_WHITE);
  drawBootScreen();
}

// Zeichnet 1-4 Textzeilen mit der groesstmoeglichen Schriftgroesse, bei der
// die laengste Zeile ohne Scrollen auf das Display passt (lastenheft.txt
// Abschnitt 11: "größtmögliche Schrift").
void DisplayManager::drawLines(const String lines[], int count) {
  size_t maxLen = 0;
  for (int i = 0; i < count; i++) maxLen = max(maxLen, (size_t)lines[i].length());

  int size = 2;
  if (maxLen * 6 * size > (size_t)SCREEN_WIDTH) size = 1;

  int lineHeight = size * 8 + 2;

  display.clearDisplay();
  display.setTextSize(size);
  for (int i = 0; i < count; i++) {
    display.setCursor(0, i * lineHeight);
    display.println(lines[i]);
  }
  display.display();
}

void DisplayManager::drawBootScreen() {
  String lines[2];
  lines[0] = _config.getConfig().systemName;
  lines[1] = String(_countdownValue);
  drawLines(lines, 2);
}

void DisplayManager::drawSystemNamePage() {
  String lines[1];
  lines[0] = _config.getConfig().systemName;
  drawLines(lines, 1);
}

void DisplayManager::drawIpPage() {
  String lines[1];
  lines[0] = "WLAN " + (_network.isWlanUp() ? _network.getWlanIp().toString() : String("---"));
  drawLines(lines, 1);
}

void DisplayManager::drawTimePage() {
  String lines[2];
  if (!_time.isSynced()) {
    lines[0] = "Zeit";
    lines[1] = "--:--:--";
  } else {
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char dateBuf[16];
    char timeBuf[16];
    strftime(dateBuf, sizeof(dateBuf), "%d.%m.%Y", &timeinfo);
    strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &timeinfo);
    lines[0] = dateBuf;
    lines[1] = timeBuf;
  }
  drawLines(lines, 2);
}

void DisplayManager::drawSensorPage() {
  SensorReading reading = _data.getSensor();

  String lines[1];
  lines[0] = reading.valid ? String(reading.temperature, 1) + "C " + String(reading.humidity, 0) + "%"
                            : String("Sensor --");
  drawLines(lines, 1);
}

void DisplayManager::drawStatusPage() {
  String lines[1];
  if (_network.isUsingFallbackWlan()) {
    lines[0] = "WLAN Fallback";
  } else {
    lines[0] = String("WLAN ") + (_network.isWlanUp() ? "OK" : "--");
  }
  drawLines(lines, 1);
}

void DisplayManager::drawPage(int page) {
  switch (page) {
    case 0: drawSystemNamePage(); break;
    case 1: drawIpPage(); break;
    case 2: drawTimePage(); break;
    case 3: drawSensorPage(); break;
    case 4: drawStatusPage(); break;
    default: break;
  }
}

void DisplayManager::loop() {
  if (!_initialized) return;

  SystemState state = _data.getSystemState();
  bool booting = (state == SystemState::BOOT || state == SystemState::INIT ||
                  state == SystemState::WLAN_CHECK);

  if (booting) {
    // Nur bei Countdown-Aenderung neu zeichnen (1x/s), nicht bei jedem
    // 50ms-Tick - ein volles I2C-Frame kostet spuerbare Zeit; bei jedem Tick
    // waere das unnoetige CPU-/I2C-Last waehrend eines bis zu 5 Minuten
    // langen WLAN_CHECK (siehe Vorbild im Sensormeter-Projekt).
    if (_lastCountdownTickMillis == 0) {
      _lastCountdownTickMillis = millis();
      drawBootScreen();
    } else if (millis() - _lastCountdownTickMillis >= COUNTDOWN_TICK_MS) {
      _lastCountdownTickMillis = millis();
      if (_countdownValue > 0) _countdownValue--;
      drawBootScreen();
    }
    return;
  }

  if (_lastPageSwitchMillis == 0) {
    _lastPageSwitchMillis = millis();
  } else if (millis() - _lastPageSwitchMillis >= PAGE_INTERVAL_MS) {
    _lastPageSwitchMillis = millis();
    _currentPage = (_currentPage + 1) % PAGE_COUNT;
  }
  drawPage(_currentPage);
}
