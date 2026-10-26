/*
 * flap.cpp : MG995 servo on GPIO 13 using ESP32Servo v1.2.1 (core 2.0.x LEDC API).
 *
 * LEDC timer rule: camera XCLK uses LEDC_TIMER_0 / LEDC_CHANNEL_0.
 * ESP32PWM::allocateTimer(SERVO_PWM_TIMER) reserves ONLY timer 1 for the servo library,
 * so it never touches timer 0 (prevents "Camera probe failed").
 */
#include "flap.h"
#include "config.h"
#include <ESP32Servo.h>

static Servo s_servo;
static int   s_angle = FLAP_CLOSE_ANGLE;

static int angleToUs(int angle) {
  angle = constrain(angle, 0, 180);
  return map(angle, 0, 180, SERVO_MIN_US, SERVO_MAX_US);
}

static void writeAngle(int angle) {
  s_servo.writeMicroseconds(angleToUs(angle));
  s_angle = angle;
}

void Flap::begin() {
  ESP32PWM::allocateTimer(SERVO_PWM_TIMER);   // servo lib may only use timer 1
  s_servo.setPeriodHertz(SERVO_FREQ_HZ);
  // attach() leaves duty at 0 (no pulse) until the first write, so write CLOSED right away
  s_servo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
  writeAngle(FLAP_CLOSE_ANGLE);
  delay(500);                                 // give the flap time to reach closed position
  DBG("[FLAP] ready on GPIO %d (LEDC timer %d), closed at %d deg\n",
      SERVO_PIN, SERVO_PWM_TIMER, FLAP_CLOSE_ANGLE);
}

void Flap::moveTo(int target) {
  target = constrain(target, 0, 180);
  DBG("[FLAP] %d -> %d deg\n", s_angle, target);
  // Smooth ramp: limits current spikes (brownout) and keeps waste from bouncing
  while (s_angle != target) {
    int diff = target - s_angle;
    int step = constrain(diff, -FLAP_STEP_DEG, FLAP_STEP_DEG);
    writeAngle(s_angle + step);
    delay(FLAP_STEP_DELAY_MS);
  }
}

void Flap::open()  { moveTo(FLAP_OPEN_ANGLE); }
void Flap::close() { moveTo(FLAP_CLOSE_ANGLE); }
int  Flap::angle() { return s_angle; }

void Flap::testCycle() {
  open();
  delay(FLAP_OPEN_HOLD_MS);
  close();
}
