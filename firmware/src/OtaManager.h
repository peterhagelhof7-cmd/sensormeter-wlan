#pragma once

#include <Arduino.h>

// Lokales OTA-Update per .bin-Upload (siehe docs/entscheidungen.md: der
// GitHub-Versionscheck/-Direktinstall wurde bereits im Sensormeter-Projekt
// verworfen, um den HTTPS-Client (WiFiClientSecure/HTTPClient) und dessen
// mbedTLS-Flash-Bedarf zu sparen - hier von Anfang an ebenso weggelassen).

class OtaManager {
 public:
  // Streaming-Callback aus WebServerManager fuer den lokalen .bin-Upload.
  bool beginLocalUpdate(size_t contentLength);
  bool writeLocalUpdateChunk(uint8_t* data, size_t len);
  bool endLocalUpdate();
};
