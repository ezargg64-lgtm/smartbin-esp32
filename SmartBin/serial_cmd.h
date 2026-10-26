/*
 * serial_cmd.h : Serial Monitor calibration interface (115200 baud).
 * Works with any line ending setting (also "No line ending").
 */
#pragma once
#include <Arduino.h>

namespace SerialCmd {
  void begin();
  void poll();        // call often from loop()
  void printHelp();
}

// Implemented in SmartBin.ino
void runSortCycle();
void printStatus();
void recalibrateSensor();
