/*
 * sensor.h : HC-SR04 ultrasonic looking down at the closed flap.
 * Detects waste resting on the flap: distance < baseline - TRIGGER_DELTA_CM for TRIGGER_HOLD_MS.
 */
#pragma once
#include <Arduino.h>

namespace Sensor {
  void  begin();
  float readCm();               // median-filtered distance in cm, -1 = no echo / invalid
  bool  calibrate();            // measure empty-flap baseline. false = failed (uses fallback)
  float baseline();
  float threshold();            // distance below which an object is "present"
  bool  objectPresent(float cm);
  bool  enabled();
}
