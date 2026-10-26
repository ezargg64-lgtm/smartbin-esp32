/*
 * turntable.cpp : 3-sector turntable, NEMA17 17HS4401 + A4988, 1/16 microstep, direct drive.
 *
 * Wiring reminders (see README.md):
 *  - A4988 VDD + MS1/MS2/MS3 -> ESP32-CAM 3.3V (logic HIGH must match 3.3V GPIO)
 *  - RESET bridged to SLEEP, EN -> GND, 100uF across VMOT/GND, Vref ~0.65 V (Rs = 0.1 ohm)
 *  - 10k pull-down on GPIO 12 (STEP) so the ESP32 boots
 *
 * Position math: 3200 microsteps / 3 sectors = 1066.67 (not an integer!).
 * Each sector target is computed from an ABSOLUTE position: round(sector * 3200 / 3)
 * -> sector 0 = 0, sector 1 = 1067, sector 2 = 2133. Rounding error never accumulates.
 * Moves always take the shortest direction (max 120 deg).
 */
#include "turntable.h"
#include "config.h"

#if ENABLE_STEPPER
#include <AccelStepper.h>
static AccelStepper s_stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);
#endif

static int  s_sector   = SECTOR_ORGANIK;
static long s_position = 0;   // absolute microstep position, kept normalized 0..STEPS_PER_REV-1

static long sectorToSteps(int sector) {
  return lround((double)sector * STEPS_PER_REV / NUM_SECTORS);
}

bool Turntable::enabled() { return ENABLE_STEPPER; }
int  Turntable::sector()  { return s_sector; }
long Turntable::position(){ return s_position; }

void Turntable::begin() {
  // Drive STEP/DIR LOW immediately (GPIO 12 must not float HIGH)
  pinMode(STEP_PIN, OUTPUT);
  digitalWrite(STEP_PIN, LOW);
  pinMode(DIR_PIN, OUTPUT);
  digitalWrite(DIR_PIN, LOW);

#if ENABLE_STEPPER
  s_stepper.setMinPulseWidth(STEPPER_MIN_PULSE_US);
  s_stepper.setMaxSpeed(STEPPER_MAX_SPEED);
  s_stepper.setAcceleration(STEPPER_ACCEL);
  s_stepper.setCurrentPosition(0);
  DBG("[TURN] ready: %d steps/rev, sectors at 0/%ld/%ld. ASSUMING sector 0 is aligned!\n",
      STEPS_PER_REV, sectorToSteps(1), sectorToSteps(2));
#else
  DBGLN("[TURN] stepper DISABLED (ENABLE_STEPPER 0) - moves are simulated only");
#endif
  s_sector = SECTOR_ORGANIK;
  s_position = 0;
}

void Turntable::zero() {
#if ENABLE_STEPPER
  s_stepper.setCurrentPosition(0);
#endif
  s_sector = SECTOR_ORGANIK;
  s_position = 0;
  DBGLN("[TURN] current position set as sector 0 (Organik)");
}

void Turntable::goToSector(int sector) {
  if (sector < 0 || sector >= NUM_SECTORS) {
    DBG("[TURN] invalid sector %d\n", sector);
    return;
  }
  if (sector == s_sector) {
    DBG("[TURN] already at sector %d - no move\n", sector);
    return;
  }

  long target = sectorToSteps(sector);
  long delta  = target - s_position;
  // shortest path: wrap into (-REV/2, +REV/2]
  if (delta >  STEPS_PER_REV / 2) delta -= STEPS_PER_REV;
  if (delta <= -STEPS_PER_REV / 2) delta += STEPS_PER_REV;

  DBG("[TURN] sector %d -> %d (%+ld steps, %+.1f deg)\n",
      s_sector, sector, delta, delta * 360.0 / STEPS_PER_REV);

#if ENABLE_STEPPER
  s_stepper.setCurrentPosition(s_position);
  s_stepper.runToNewPosition(s_position + delta);   // blocking, with acceleration
  delay(STEPPER_SETTLE_MS);
#endif

  s_position = target;   // re-normalize to exact absolute sector position
  s_sector   = sector;
}
