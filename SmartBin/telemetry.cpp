/*
 * telemetry.cpp : placeholder for WiFi/HTTP telemetry. Disabled by default (ENABLE_WIFI 0)
 * so the demo runs fully offline. Add your HTTP/Firebase/Blynk call inside sendResult().
 */
#include "telemetry.h"
#include "config.h"

#if ENABLE_WIFI
#include <WiFi.h>
#endif

void Telemetry::begin() {
#if ENABLE_WIFI
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(false);
  DBG("[WIFI] connecting to %s", WIFI_SSID);
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_TIMEOUT_MS) {
    delay(500);
    DBG(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    DBG("\n[WIFI] connected, IP %s\n", WiFi.localIP().toString().c_str());
  } else {
    DBGLN("\n[WIFI] not connected - continuing OFFLINE");
  }
#endif
}

void Telemetry::sendResult(const char *label, float conf, int sector) {
#if ENABLE_WIFI
  if (WiFi.status() != WL_CONNECTED) return;
  // TODO: send {label, conf, sector} to your dashboard here
  DBG("[WIFI] (stub) result %s %.2f sector %d\n", label, conf, sector);
#else
  (void)label; (void)conf; (void)sector;
#endif
}
