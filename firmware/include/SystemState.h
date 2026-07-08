#pragma once

// Zustandsmodell gemaess docs/lastenheft.txt Abschnitt 12 (vereinfacht
// gegenueber dem Sensormeter-Projekt: kein LAN-Interface, daher kein
// DHCP_TEST/RESTORE_CONFIG-Zweig fuer eine NTP-Fehlerkette - eine
// Interface-Umschaltung gibt es hier nicht, siehe TimeManager):
//
//   BOOT -> INIT -> WLAN_CHECK
//     -> OK       -> RUN_NORMAL
//     -> FAIL     -> FALLBACK_MODE (Access Point "installer")
//   RUN_NORMAL:
//     NTP OK/FAIL beeinflusst NUR TimeManager-internen Status, keinen
//     Systemzustandswechsel (siehe lastenheft.txt Abschnitt 8).

enum class SystemState {
  BOOT,
  INIT,
  WLAN_CHECK,
  RUN_NORMAL,
  FALLBACK_MODE
};

inline const char* toString(SystemState state) {
  switch (state) {
    case SystemState::BOOT:          return "BOOT";
    case SystemState::INIT:          return "INIT";
    case SystemState::WLAN_CHECK:    return "WLAN_CHECK";
    case SystemState::RUN_NORMAL:    return "RUN_NORMAL";
    case SystemState::FALLBACK_MODE: return "FALLBACK_MODE";
    default:                         return "UNKNOWN";
  }
}
