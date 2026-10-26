/*
 * flap.h : MG995 servo driving the acrylic drop flap door.
 */
#pragma once
#include <Arduino.h>

namespace Flap {
  void begin();                 // allocate LEDC timer 1, attach servo, go to CLOSED
  void moveTo(int angle);       // smooth move (blocking) to angle 0..180
  void open();                  // smooth move to FLAP_OPEN_ANGLE
  void close();                 // smooth move to FLAP_CLOSE_ANGLE
  void testCycle();             // open -> hold -> close
  int  angle();                 // last commanded angle
}
