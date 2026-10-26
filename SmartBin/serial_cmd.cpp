/*
 * serial_cmd.cpp : calibration commands so the flap/turntable can be tuned inside the
 * gallon chamber without re-uploading code.
 */
#include "serial_cmd.h"
#include "config.h"
#include "flap.h"
#include "turntable.h"
#include "sensor.h"
#include "camera_ai.h"

#if ENABLE_SERIAL_DEBUG

static char          s_buf[24];
static uint8_t       s_len = 0;
static unsigned long s_lastChar = 0;

void SerialCmd::begin() { printHelp(); }

void SerialCmd::printHelp() {
  Serial.println();
  Serial.println(F("========= SmartBin Serial Commands ========="));
  Serial.println(F(" o        open flap (90 deg)"));
  Serial.println(F(" c        close flap (0 deg)"));
  Serial.println(F(" 0..180   move servo to angle"));
  Serial.println(F(" t        flap test cycle (open, hold, close)"));
  Serial.println(F(" x        classify once (no movement)"));
  Serial.println(F(" l        toggle flash LED ON/OFF"));
  Serial.println(F(" f0..f255 set flash LED brightness (0..255) & turn ON"));
  Serial.println(F(" d        read ultrasonic distance"));
  Serial.println(F(" b        re-measure ultrasonic baseline (flap empty!)"));
  Serial.println(F(" s0/s1/s2 turntable to sector (0=Organik 1=Anorganik 2=B3)"));
  Serial.println(F(" z        set current turntable position as sector 0"));
  Serial.println(F(" r        run full sort cycle now"));
  Serial.println(F(" ?        status"));
  Serial.println(F(" h        this help"));
  Serial.println(F("============================================"));
}

static bool isNumber(const char *s) {
  if (!*s) return false;
  for (; *s; s++) if (!isdigit((unsigned char)*s)) return false;
  return true;
}

static void execute(char *cmd) {
  // trim spaces
  while (*cmd == ' ') cmd++;
  for (int i = strlen(cmd) - 1; i >= 0 && cmd[i] == ' '; i--) cmd[i] = 0;
  if (!*cmd) return;

  Serial.printf("> %s\n", cmd);

  if (isNumber(cmd)) {
    int a = atoi(cmd);
    if (a < 0 || a > 180) { Serial.println(F("Angle must be 0..180")); return; }
    Flap::moveTo(a);
    return;
  }

  if ((cmd[0] == 's' || cmd[0] == 'S') && isdigit((unsigned char)cmd[1]) && cmd[2] == 0) {
    Turntable::goToSector(cmd[1] - '0');
    return;
  }

  if ((cmd[0] == 'f' || cmd[0] == 'F') && isNumber(cmd + 1)) {
    int b = constrain(atoi(cmd + 1), 0, 255);
    CameraAI::setLedBrightness((uint8_t)b);
    CameraAI::led(b > 0);
    Serial.printf("Flash LED brightness = %d/255 (%s)\n", b, CameraAI::ledState() ? "ON" : "OFF");
    return;
  }

  if (cmd[1] != 0) { Serial.println(F("Unknown command. Type h for help.")); return; }

  switch (tolower(cmd[0])) {
    case 'o': Flap::open(); break;
    case 'c': Flap::close(); break;
    case 't': Flap::testCycle(); break;
    case 'x': {
      ClassResult r;
      if (CameraAI::classify(r)) {
        Serial.printf("Best: %s %.1f%% -> sector %d%s\n", r.label, r.score * 100.0f,
                      CameraAI::labelToSector(r.label),
                      r.score < CONF_THRESHOLD ? " (LOW confidence -> would go to fallback)" : "");
      } else {
        Serial.println(F("Classification failed"));
      }
      break;
    }
    case 'l': CameraAI::led(!CameraAI::ledState());
              Serial.printf("LED %s\n", CameraAI::ledState() ? "ON" : "OFF"); break;
    case 'd': {
      float cm = Sensor::readCm();
      Serial.printf("Distance: %.1f cm | baseline %.1f | trigger < %.1f | %s\n", cm,
                    Sensor::baseline(), Sensor::threshold(),
                    Sensor::objectPresent(cm) ? "OBJECT" : "empty");
      break;
    }
    case 'b': recalibrateSensor(); break;
    case 'z': Turntable::zero(); break;
    case 'r': runSortCycle(); break;
    case '?': printStatus(); break;
    case 'h': SerialCmd::printHelp(); break;
    default:  Serial.println(F("Unknown command. Type h for help."));
  }
}

void SerialCmd::poll() {
  while (Serial.available()) {
    char ch = Serial.read();
    s_lastChar = millis();
    if (ch == '\n' || ch == '\r') {
      if (s_len) { s_buf[s_len] = 0; execute(s_buf); s_len = 0; }
    } else if (s_len < sizeof(s_buf) - 1) {
      s_buf[s_len++] = ch;
    }
  }
  // "No line ending" mode: execute after 150 ms of silence
  if (s_len && millis() - s_lastChar > 150) {
    s_buf[s_len] = 0;
    execute(s_buf);
    s_len = 0;
  }
}

#else  // Serial disabled (production, GPIO1/3 used by LCD)

void SerialCmd::begin() {}
void SerialCmd::poll() {}
void SerialCmd::printHelp() {}

#endif
