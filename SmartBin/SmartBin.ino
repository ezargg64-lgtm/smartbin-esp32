/*
 * SmartBin - IoT & AI Automated Waste Sorter  (Phase 1: Servo + ESP32-CAM, stepper modular)
 * -----------------------------------------------------------------------------------------
 * Board : AI Thinker ESP32-CAM  |  Core: esp32 v2.0.14  |  Partition: Huge APP  |  PSRAM on
 * Libs  : SmartBinV2_inferencing 1.0.5, ESP32Servo 1.2.1, AccelStepper 1.64, LiquidCrystal_I2C 2.0.0
 *
 * Sort cycle (one actuator at a time to avoid brownouts):
 *   waste rests on flap -> ultrasonic sees it for 500 ms -> wait 1.2 s -> LED + photo -> AI
 *   -> (confidence < 70%: retry once, then fallback Sector 2) -> turntable to sector
 *   -> flap opens 90 deg for 2 s -> flap closes -> wait until flap reads empty -> ready
 *
 * All settings: config.h      Calibration: Serial Monitor 115200, type 'h'
 * BEFORE POWER-ON: rotate the turntable by hand so Sector 0 (Organik) is under the flap.
 */
#include "config.h"
#include "camera_ai.h"
#include "flap.h"
#include "turntable.h"
#include "sensor.h"
#include "display.h"
#include "serial_cmd.h"
#include "telemetry.h"

enum State { STATE_IDLE, STATE_REARM };

static State         s_state        = STATE_IDLE;
static bool          s_busy         = false;
static bool          s_presentTimer = false;
static unsigned long s_presentSince = 0;
static bool          s_emptyTimer   = false;
static unsigned long s_emptySince   = 0;
static unsigned long s_rearmStart   = 0;
static uint32_t      s_sortedCount  = 0;

static const char *sectorName(int s) {
  switch (s) {
    case SECTOR_ORGANIK:   return "Organik";
    case SECTOR_ANORGANIK: return "Anorganik";
    case SECTOR_B3:        return "B3/Residu";
    default:               return "?";
  }
}

// ---------------------------------------------------------------------
void recalibrateSensor() {
#if ENABLE_ULTRASONIC
  if (Flap::angle() != FLAP_CLOSE_ANGLE) Flap::close();
  Sensor::calibrate();
#else
  DBGLN("[US] ultrasonic disabled (ENABLE_ULTRASONIC 0)");
#endif
}

void printStatus() {
  DBG("\n---- STATUS ----\n");
  DBG("State      : %s%s\n", s_state == STATE_IDLE ? "IDLE" : "REARM", s_busy ? " (busy)" : "");
  DBG("Camera     : %s\n", CameraAI::ready() ? "OK" : "FAILED (safe mode)");
  DBG("Flash LED  : %s (brightness %u/255)\n", CameraAI::ledState() ? "ON" : "OFF", CameraAI::ledBrightness());
  DBG("Flap angle : %d deg\n", Flap::angle());
  DBG("Turntable  : sector %d (%s), pos %ld, %s\n", Turntable::sector(),
      sectorName(Turntable::sector()), Turntable::position(),
      Turntable::enabled() ? "enabled" : "SIMULATED");
  DBG("Ultrasonic : %s, baseline %.1f cm, trigger < %.1f cm\n",
      Sensor::enabled() ? "on" : "off", Sensor::baseline(), Sensor::threshold());
  DBG("Sorted     : %lu items\n", (unsigned long)s_sortedCount);
  DBG("Heap free  : %u B, PSRAM free: %u B\n", ESP.getFreeHeap(), ESP.getFreePsram());
  DBG("----------------\n");
}

// ---------------------------------------------------------------------
// Full blocking sort cycle. Called by the ultrasonic trigger or Serial 'r'.
void runSortCycle() {
  if (s_busy) return;
  if (!CameraAI::ready()) {
    Display::error("Camera failed");
    DBGLN("[SORT] refused: camera not ready");
    return;
  }
  s_busy = true;
  DBGLN("\n[SORT] ===== cycle start =====");

  Display::analyzing();
  delay(SETTLE_MS);                       // let the waste lie still on the flap

  ClassResult r;
  bool confident = false;
  bool gotResult = false;
  for (int attempt = 0; attempt <= MAX_RETRIES; attempt++) {
    gotResult = CameraAI::classify(r);
    if (gotResult && r.score >= CONF_THRESHOLD) { confident = true; break; }
    DBG("[SORT] attempt %d: %s\n", attempt + 1,
        gotResult ? "low confidence" : "capture/classify failed");
    if (attempt < MAX_RETRIES) {
      Display::show("Unknown / Residu", "Retrying...");
      delay(300);
    }
  }

  int sector;
  if (confident) {
    sector = CameraAI::labelToSector(r.label);
    Display::result(r.label, r.score, sector);
    DBG("[SORT] -> %s (%.1f%%) -> sector %d\n", r.label, r.score * 100.0f, sector);
  } else {
    sector = FALLBACK_SECTOR;
    Display::unknown(sector);
    DBG("[SORT] -> UNKNOWN (best %.1f%%) -> fallback sector %d\n",
        gotResult ? r.score * 100.0f : 0.0f, sector);
  }
  Telemetry::sendResult(confident ? r.label : "Unknown", gotResult ? r.score : 0.0f, sector);
  delay(RESULT_SHOW_MS);

  Turntable::goToSector(sector);          // 1) align bin (no-op if already there)
  Flap::open();                           // 2) drop waste
  delay(FLAP_OPEN_HOLD_MS);
  Flap::close();                          // 3) close

  s_sortedCount++;
  DBG("[SORT] ===== cycle done (total %lu) =====\n", (unsigned long)s_sortedCount);

  s_state        = STATE_REARM;           // wait until flap reads empty before next trigger
  s_rearmStart   = millis();
  s_emptyTimer   = false;
  s_presentTimer = false;
  s_busy         = false;
}

// ---------------------------------------------------------------------
void setup() {
#if ENABLE_SERIAL_DEBUG
  Serial.begin(115200);
  delay(200);
#endif
  DBGLN("\n\n=== SmartBin firmware (Phase 1) ===");

  Turntable::begin();                     // STEP/DIR LOW first (GPIO 12 strapping pin)
  Display::begin();
  Display::show("SmartBin", "Booting...");

  bool camOk = CameraAI::begin();         // camera owns LEDC timer 0
  Flap::begin();                          // servo on LEDC timer 1, flap closed

  Sensor::begin();
#if ENABLE_ULTRASONIC
  delay(300);                             // flap fully closed & still before baseline
  Sensor::calibrate();                    // flap must be EMPTY at boot
#endif

  Telemetry::begin();
  SerialCmd::begin();

  if (!camOk) {
    Display::error("Camera failed");
    CameraAI::blinkLed(5, 100, 100);        // 5 fast blinks = camera error
    DBGLN("[BOOT] SAFE MODE: camera failed. Servo/turntable commands still work.");
  } else {
    CameraAI::blinkLed(2, 80, 80);          // 2 short blinks = boot ready
    Display::ready();
    DBGLN("[BOOT] ready.");
  }
}

// ---------------------------------------------------------------------
void loop() {
  SerialCmd::poll();

  if (!CameraAI::ready()) {               // safe mode: only Serial calibration
    delay(10);
    return;
  }

#if ENABLE_ULTRASONIC
  static unsigned long lastPoll = 0;
  if (millis() - lastPoll < US_POLL_INTERVAL_MS) return;
  lastPoll = millis();

  float cm = Sensor::readCm();
  bool present = Sensor::objectPresent(cm);

  if (s_state == STATE_IDLE) {
    if (present) {
      if (!s_presentTimer) { s_presentTimer = true; s_presentSince = millis(); }
      else if (millis() - s_presentSince >= TRIGGER_HOLD_MS) {
        s_presentTimer = false;
        DBG("[TRIG] object on flap (%.1f cm)\n", cm);
        runSortCycle();
      }
    } else {
      s_presentTimer = false;
    }
  } else {  // STATE_REARM
    if (!present) {
      if (!s_emptyTimer) { s_emptyTimer = true; s_emptySince = millis(); }
      else if (millis() - s_emptySince >= REARM_HOLD_MS) {
        s_state = STATE_IDLE;
        s_emptyTimer = false;
        Display::ready();
      }
    } else {
      s_emptyTimer = false;
      if (millis() - s_rearmStart > REARM_TIMEOUT_MS) {
        DBGLN("[REARM] flap still not empty - item stuck? Re-arming (will retry sort).");
        Display::show("Check flap!", "Item not dropped");
        s_state = STATE_IDLE;
      }
    }
  }
#else
  if (s_state == STATE_REARM) {           // no sensor: ready again immediately
    s_state = STATE_IDLE;
    Display::ready();
  }
#endif
}
