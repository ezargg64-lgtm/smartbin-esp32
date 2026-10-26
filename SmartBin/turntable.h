/*
 * turntable.h : NEMA17 + A4988 rotating the 3-sector bin turntable.
 * Compiled as a "simulation" (prints only) when ENABLE_STEPPER is 0.
 */
#pragma once
#include <Arduino.h>

namespace Turntable {
  void begin();                 // pins safe LOW, assume sector 0 is aligned under the flap
  void goToSector(int sector);  // shortest-path move (blocking). No move if already there
  void zero();                  // define current physical position as sector 0
  int  sector();                // current sector
  long position();              // absolute microstep position (0..STEPS_PER_REV-1)
  bool enabled();               // ENABLE_STEPPER flag
}
