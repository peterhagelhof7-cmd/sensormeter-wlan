#include "DisplayManager.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <time.h>
#include "SystemState.h"
#include "pins.h"

#if __has_include("config.h")
#include "config.h"
#endif
#ifndef DEVICE_FIRMWARE_VERSION
#define DEVICE_FIRMWARE_VERSION "0.0.0"
#endif

static const int SCREEN_WIDTH = 128;
static const int SCREEN_HEIGHT = 64;
static const uint8_t SSD1306_I2C_ADDRESS = 0x3C;

// Feste Marken-/Typkennzeichnung (nicht der frei einstellbare Systemname) -
// analog zum "Systemtyp" im Sensormeter-Projekt (dort per SNMP als fester
// String "Sensormeter WLAN" ausgeliefert, siehe SNMPManager.cpp).
static const char* PRODUCT_NAME = "Sensormeter";
static const char* PRODUCT_TYPE = "WLAN";

static const unsigned long PAGE_INTERVAL_MS = 10UL * 1000UL;  // 10s (lastenheft.txt Abschnitt 11)
static const unsigned long COUNTDOWN_TICK_MS = 1000UL;        // 1x/s

// BOOT-Taster: ab dieser Haltedauer erscheint die Reset-Bestaetigung,
// danach laeuft der Countdown; wird der Taster bis zum Ende durchgehend
// gehalten, wird der Werksreset ausgeloest.
static const unsigned long BUTTON_RESET_HOLD_MS = 3000UL;
static const unsigned long BUTTON_RESET_COUNTDOWN_MS = 20000UL;
static const unsigned long BUTTON_TAP_MIN_MS = 50UL;  // Entprellung fuer kurze Tipps

static Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

DisplayManager::DisplayManager(DataManager& dataManager, ConfigManager& configManager,
                               NetworkManager& networkManager, TimeManager& timeManager)
    : _data(dataManager), _config(configManager), _network(networkManager), _time(timeManager) {}

void DisplayManager::begin() {
  pinMode(BUTTON_BOOT_PIN, INPUT_PULLUP);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  _initialized = display.begin(SSD1306_SWITCHCAPVCC, SSD1306_I2C_ADDRESS);
  if (!_initialized) {
    Serial.println("[DISPLAY] SSD1306 nicht gefunden (I2C 0x3C) - Anzeige deaktiviert");
    return;
  }
  display.cp437(true);
  display.setTextColor(SSD1306_WHITE);
  // Wir brechen Zeilen selbst um (mehrere setCursor()/println()-Aufrufe)
  // bzw. lassen zu lange Zeilen bewusst laufen (siehe drawIpPage()) - das
  // automatische Adafruit-GFX-Wrapping wuerde beides durcheinanderbringen.
  display.setTextWrap(false);
  drawBootScreen();
}

// Feste Zielschriftgroesse fuer alle rotierenden Seiten (Nutzerwunsch:
// "immer die grosse Schrift") - Zeilen, die dabei nicht auf einmal passen
// (z.B. lange SSIDs), laufen waagerecht durch statt die Schrift fuer alle
// zu schrumpfen, siehe drawScrollingLine().
static const int LINE_TEXT_SIZE = 2;

// Zeichnet eine einzelne Zeile bei fester Groesse, zentriert falls sie
// passt, sonst waagerecht durchlaufend gemaess progress (0.0 = Anfang,
// 1.0 = Ende - siehe Aufrufer fuer die Zeitbasis).
void DisplayManager::drawScrollingLine(const String& text, int y, int size, float progress) {
  int textWidth = static_cast<int>(text.length()) * 6 * size;
  display.setTextSize(size);
  if (textWidth <= SCREEN_WIDTH) {
    int x = max(0, (SCREEN_WIDTH - textWidth) / 2);
    display.setCursor(x, y);
    display.println(text);
    return;
  }
  if (progress < 0.0f) progress = 0.0f;
  if (progress > 1.0f) progress = 1.0f;
  int scrollDistance = textWidth - SCREEN_WIDTH;
  int x = -static_cast<int>(progress * scrollDistance);
  display.setCursor(x, y);
  display.println(text);
}

// Zeichnet 1-4 Textzeilen bei fester Groesse (LINE_TEXT_SIZE), horizontal
// UND vertikal zentriert (Nutzerwunsch) - laeuft eine Zeile ueber, scrollt
// sie synchron zum Seitenwechsel-Timer: einmal komplett durch, haelt dann
// am Ende an, bis die naechste Seite (PAGE_INTERVAL_MS) dran ist.
void DisplayManager::drawLines(const String lines[], int count) {
  int lineHeight = LINE_TEXT_SIZE * 8;
  int y0 = max(0, (SCREEN_HEIGHT - count * lineHeight) / 2);

  static const unsigned long kScrollHoldMs = 1500UL;
  unsigned long scrollWindowMs =
      PAGE_INTERVAL_MS > kScrollHoldMs ? PAGE_INTERVAL_MS - kScrollHoldMs : PAGE_INTERVAL_MS;
  unsigned long elapsed = millis() - _lastPageSwitchMillis;
  float progress = static_cast<float>(elapsed) / static_cast<float>(scrollWindowMs);

  display.clearDisplay();
  for (int i = 0; i < count; i++) {
    drawScrollingLine(lines[i], y0 + i * lineHeight, LINE_TEXT_SIZE, progress);
  }
  display.display();
}

void DisplayManager::drawBootScreen() {
  // Jede Zeile bekommt ihre eigene groesstmoegliche Schriftgroesse (statt
  // wie drawLinesCentered() eine gemeinsame): "Sensormeter" ist mit 11
  // Zeichen bei 128px Breite immer auf Groesse 1 begrenzt, WLAN und der
  // Countdown koennen aber deutlich groesser dargestellt werden - genau das
  // war der Wunsch nach "groesserer Schrift" fuer den Boot-Countdown.
  String line1 = PRODUCT_NAME;
  String line2 = PRODUCT_TYPE;
  // Auf 3 Stellen leerzeichen-aufgefuellt, damit die Zeilenlaenge (und damit
  // die Schriftgroesse) waehrend des Runterzaehlens 100->0 konstant bleibt,
  // statt bei jedem Ziffernwechsel (100->99->9) die Groesse springen zu
  // lassen.
  char countdownBuf[16];
  snprintf(countdownBuf, sizeof(countdownBuf), "%3d warte", _countdownValue);
  String line3 = countdownBuf;

  int size1 = max(1, SCREEN_WIDTH / (static_cast<int>(line1.length()) * 6));
  int size2 = max(1, SCREEN_WIDTH / (static_cast<int>(line2.length()) * 6));
  int size3 = max(1, SCREEN_WIDTH / (static_cast<int>(line3.length()) * 6));

  int h1 = size1 * 8, h2 = size2 * 8, h3 = size3 * 8;
  while (h1 + h2 + h3 > SCREEN_HEIGHT && (size1 > 1 || size2 > 1 || size3 > 1)) {
    if (size1 > 1) size1--;
    if (size2 > 1) size2--;
    if (size3 > 1) size3--;
    h1 = size1 * 8; h2 = size2 * 8; h3 = size3 * 8;
  }

  int y0 = max(0, (SCREEN_HEIGHT - (h1 + h2 + h3)) / 2);

  display.clearDisplay();
  auto drawCentered = [&](const String& text, int size, int y) {
    display.setTextSize(size);
    int w = static_cast<int>(text.length()) * 6 * size;
    int x = max(0, (SCREEN_WIDTH - w) / 2);
    display.setCursor(x, y);
    display.println(text);
  };
  drawCentered(line1, size1, y0);
  drawCentered(line2, size2, y0 + h1);
  drawCentered(line3, size3, y0 + h1 + h2);
  display.display();
}

void DisplayManager::drawSystemNamePage() {
  String lines[1];
  lines[0] = _config.getConfig().systemName;
  drawLines(lines, 1);
}

void DisplayManager::drawIpPage() {
  String lines[2];
  lines[0] = _network.isWlanUp() ? _network.getWlanSsid() : String("---");
  lines[1] = _network.isWlanUp() ? _network.getWlanIp().toString() : String("---");
  drawLines(lines, 2);
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
  String lines[3];
  if (_network.isUsingFallbackWlan()) {
    lines[0] = "WLAN Fallback";
  } else {
    lines[0] = String("WLAN ") + (_network.isWlanUp() ? "OK" : "--");
  }
  lines[1] = "v" DEVICE_FIRMWARE_VERSION;
  // Systemtyp - fester Wert, identisch zum SNMP-OID .1.3.6.1.4.1.99999.1.3.0
  // (siehe SNMPManager.cpp), nicht der frei editierbare Systemname.
  lines[2] = String(PRODUCT_NAME) + " " + PRODUCT_TYPE;
  drawLines(lines, 3);
}

void DisplayManager::drawSignalPage() {
  String lines[2];
  lines[0] = "WLAN-Signal";
  lines[1] = _network.isWlanUp() ? String(_network.getWlanRssi()) + "dB" : String("---");
  drawLines(lines, 2);
}

void DisplayManager::drawFallbackIpPage() {
  String ip = _network.isWlanUp() ? _network.getWlanIp().toString() : String("---");

  // Anders als drawLines(): hier gibt es keine Seitenwechsel-Deadline (die
  // Seite bleibt, solange der Fallback-AP aktiv ist) - "Fallback aktiv" ist
  // bei Groesse 2 zu lang fuer eine Zeile, laeuft also dauerhaft wieder-
  // holend durch statt einmal bis zu einem Wechsel.
  static const unsigned long kCycleMs = 4000UL;
  float progress = static_cast<float>(millis() % kCycleMs) / static_cast<float>(kCycleMs);

  int lineHeight = LINE_TEXT_SIZE * 8;
  int y0 = max(0, (SCREEN_HEIGHT - 2 * lineHeight) / 2);

  display.clearDisplay();
  drawScrollingLine("Fallback aktiv", y0, LINE_TEXT_SIZE, progress);
  drawScrollingLine(ip, y0 + lineHeight, LINE_TEXT_SIZE, progress);
  display.display();
}

bool DisplayManager::handleButton() {
  bool pressed = (digitalRead(BUTTON_BOOT_PIN) == LOW);
  unsigned long now = millis();

  if (pressed && _buttonPressStartMillis == 0) {
    _buttonPressStartMillis = now;
  } else if (!pressed && _buttonPressStartMillis != 0) {
    unsigned long heldMs = now - _buttonPressStartMillis;
    _buttonPressStartMillis = 0;
    if (heldMs >= BUTTON_TAP_MIN_MS && heldMs < BUTTON_RESET_HOLD_MS) {
      // Kurzer Tipp: manuell zur naechsten Seite. Wirkt sich nur sichtbar
      // aus, wenn gerade die normale Seitenrotation laeuft (Boot/Fallback
      // haben keine Seiten zum Weiterschalten) - dort aber unschaedlich.
      _currentPage = (_currentPage + 1) % PAGE_COUNT;
      _lastPageSwitchMillis = now;
    } else if (heldMs >= BUTTON_RESET_HOLD_MS + BUTTON_RESET_COUNTDOWN_MS) {
      // Fail-Safe gegen einen verklemmten Taster: der Reset wird erst BEIM
      // tatsaechlichen Loslassen ausgeloest, nicht schon waehrend des
      // Haltens - ein Taster, der (z.B. durch einen Defekt) dauerhaft
      // gedrueckt bleibt, kann so nie von selbst einen Reset ausloesen, da
      // dafuer ein echtes Loslassen-Ereignis noetig ist.
      _data.pushLogEntry("Werksreset ueber Taster ausgeloest (nur Einstellungen)", 3);
      _config.setConfig(DeviceConfig());
      delay(300);
      ESP.restart();
    }
    // sonst (zwischen 3s und 3s+20s losgelassen): Reset-Bestaetigung ohne
    // Wirkung abgebrochen, naechster loop()-Durchlauf zeigt wieder normal an.
    return false;
  }

  if (_buttonPressStartMillis == 0) return false;

  unsigned long heldMs = now - _buttonPressStartMillis;
  if (heldMs < BUTTON_RESET_HOLD_MS) return false;

  unsigned long countdownElapsed = heldMs - BUTTON_RESET_HOLD_MS;
  String lines[2];
  if (countdownElapsed >= BUTTON_RESET_COUNTDOWN_MS) {
    // Zeit ist um, aber noch gehalten - wartet auf das Loslassen (siehe
    // Fail-Safe oben), loest hier bewusst noch nichts aus.
    lines[0] = "Loslassen zum";
    lines[1] = "Bestaetigen";
  } else {
    int secondsLeft = static_cast<int>((BUTTON_RESET_COUNTDOWN_MS - countdownElapsed) / 1000UL) + 1;
    lines[0] = "Werksreset?";
    lines[1] = String(secondsLeft) + "s halten";
  }
  drawLines(lines, 2);
  return true;
}

void DisplayManager::drawPage(int page) {
  switch (page) {
    case 0: drawSystemNamePage(); break;
    case 1: drawIpPage(); break;
    case 2: drawTimePage(); break;
    case 3: drawSensorPage(); break;
    case 4: drawStatusPage(); break;
    case 5: drawSignalPage(); break;
    default: break;
  }
}

void DisplayManager::loop() {
  if (!_initialized) return;

  // Taster hat Vorrang vor allem anderen (Boot/Fallback/Rotation) - er ist
  // als Recovery-Weg gedacht, der in jedem Systemzustand funktionieren muss.
  if (handleButton()) return;

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

  if (_network.isUsingFallbackWlan()) {
    // Keine Seitenrotation im Fallback-WLAN - siehe drawFallbackIpPage().
    drawFallbackIpPage();
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
