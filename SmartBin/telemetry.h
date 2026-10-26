/*
 * telemetry.h : optional WiFi telemetry (ENABLE_WIFI). No-op when disabled.
 */
#pragma once
#include <Arduino.h>

namespace Telemetry {
  void begin();                                             // connect with timeout (never blocks forever)
  void sendResult(const char *label, float conf, int sector);
}
