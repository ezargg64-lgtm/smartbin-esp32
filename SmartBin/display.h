/*
 * display.h : 16x2 I2C LCD screens. When ENABLE_LCD is 0 the screens are printed to Serial.
 */
#pragma once
#include <Arduino.h>

namespace Display {
  void begin();
  void show(const char *line1, const char *line2);  // raw 2-line message
  void ready();                                     // "SmartBin Ready" / "Drop waste here"
  void analyzing();                                 // "Analyzing..."   / "Hold on...."
  void result(const char *label, float conf, int sector);
  void unknown(int sector);                         // "Unknown / Residu"
  void error(const char *line2);
}
