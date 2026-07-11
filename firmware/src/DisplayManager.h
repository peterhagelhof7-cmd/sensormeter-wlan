#pragma once

#include <Arduino.h>
#include "BrandingManager.h"
#include "ConfigManager.h"
#include "DataManager.h"
#include "NetworkManager.h"
#include "TimeManager.h"

// OLED-Anzeige (Pflichtenheft-Task "DisplayTask"): SSD1306 128x64 I2C auf
// GPIO21 (SDA)/GPIO22 (SCL, ESP32-Standardbelegung - kein Ethernet-PHY, der
// diese Pins blockiert, siehe pins.h/docs/entscheidungen.md), Adresse 0x3C.
// Rotierende Infoseiten alle 10s (lastenheft.txt Abschnitt 11: Systemname /
// WLAN-IP / Uhrzeit / Sensorwerte / Status WLAN, ergaenzt um eine
// WLAN-Signal-Seite - kein LAN-Zweig, kein zweiter Sensor, im Unterschied
// zum Vorbild im Sensormeter-Projekt). Die
// "Systemname"-Seite zeigt den frei editierbaren Systemnamen (reagiert auf
// Aenderungen ueber die Weboberflaeche); die feste Marke/Typkennzeichnung
// ("Sensormeter" / "WLAN") erscheint stattdessen auf der Status-Seite
// (dritte Zeile) sowie waehrend des Bootens (BOOT/INIT/WLAN_CHECK), dort
// plus Countdown 100->0 bis das Netzwerk bereit ist. Im Fallback-WLAN
// ("installer") stattdessen ausschliesslich die IP in voller
// Displaybreite - das ist der einzige Wert, den man zum Einrichten braucht,
// die Seitenrotation waere hier nur ablenkend.
//
// BOOT-Taster (GPIO0, aktiv LOW) doppelt genutzt: kurzer Tipp (<3s) schaltet
// manuell zur naechsten Seite; ab 3s gehalten erscheint eine Reset-
// Bestaetigung mit 20s-Countdown, danach "Loslassen zum Bestaetigen". Der
// Werksreset (nur Einstellungen, config.xml auf Defaults - der Verlauf
// bleibt erhalten) wird bewusst erst beim tatsaechlichen LOSLASSEN nach
// Ablauf der vollen Haltezeit ausgeloest, nicht schon waehrend des Haltens
// - Fail-Safe gegen einen verklemmten/defekten Taster, der sonst von
// selbst (ohne echtes Loslassen-Ereignis) einen Reset ausloesen koennte.
// Funktioniert in jedem Systemzustand (auch waehrend Boot/Fallback), da er
// als Recovery-Weg ganz ohne Netzwerkzugriff gedacht ist.

class DisplayManager {
 public:
  DisplayManager(DataManager& dataManager, ConfigManager& configManager,
                 NetworkManager& networkManager, TimeManager& timeManager,
                 BrandingManager& brandingManager);

  void begin();
  void loop();

 private:
  DataManager& _data;
  ConfigManager& _config;
  NetworkManager& _network;
  TimeManager& _time;
  BrandingManager& _branding;

  bool _initialized = false;

  unsigned long _lastPageSwitchMillis = 0;
  int _currentPage = 0;
  // Seite 6 (Branding) ist nur Teil der Rotation, wenn tatsaechlich ein
  // Vendor-Name oder ein Logo konfiguriert ist (siehe pageCount()) - im
  // unkonfigurierten Default-Fall (die ueberwiegende Mehrheit der Geraete)
  // erscheint dadurch keine leere Zusatzseite in der Rotation.
  static const int BASE_PAGE_COUNT = 6;
  int pageCount() const { return _branding.isActive() ? BASE_PAGE_COUNT + 1 : BASE_PAGE_COUNT; }

  unsigned long _lastCountdownTickMillis = 0;
  int _countdownValue = 100;

  // 0 = Taster nicht gedrueckt, sonst Zeitpunkt (millis()), seit dem er
  // durchgehend gedrueckt gehalten wird - siehe handleButton().
  unsigned long _buttonPressStartMillis = 0;

  // Horizontal+vertikal zentriert, feste groessere Schrift (Groesse 2) -
  // einheitlich auf allen Screens. Zeilen, die dabei nicht auf einmal
  // passen (z.B. lange SSIDs), laufen waagerecht durch statt geschrumpft
  // zu werden - siehe drawScrollingLine().
  void drawLines(const String lines[], int count);
  // progress: 0.0 (Start) bis 1.0 (Ende) - vom Aufrufer berechnet, damit
  // sowohl "einmal durchlaufen und am Ende halten" (rotierende Seiten,
  // synchron zum Seitenwechsel-Timer) als auch "dauerhaft wiederholen"
  // (Fallback-Seite, keine Wechsel-Deadline) denselben Zeichencode nutzen
  // koennen.
  void drawScrollingLine(const String& text, int y, int size, float progress);
  void drawBootScreen();
  void drawPage(int page);
  void drawSystemNamePage();
  void drawIpPage();
  void drawTimePage();
  void drawSensorPage();
  void drawStatusPage();
  void drawSignalPage();
  void drawBrandingPage();
  void drawFallbackIpPage();
  // true, solange der Taster >=3s gehalten wird (Reset-Bestaetigung/
  // Countdown wird angezeigt bzw. Reset wurde ausgeloest) - loop() zeigt in
  // diesem Fall keine normale Seite an.
  bool handleButton();
};
