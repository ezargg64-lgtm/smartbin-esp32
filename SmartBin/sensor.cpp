/*
 * sensor.cpp : HC-SR04 on TRIG=GPIO15, ECHO=GPIO2.
 * ECHO is 5V -> MUST go through a 1k/2k voltage divider (or use HC-SR04P at 3.3V).
 */
#include "sensor.h"
#include "config.h"

static float s_baseline  = -1.0f;
static float s_threshold = TRIGGER_FALLBACK_CM;

bool Sensor::enabled() { return ENABLE_ULTRASONIC; }
float Sensor::baseline()  { return s_baseline; }
float Sensor::threshold() { return s_threshold; }

void Sensor::begin() {
#if ENABLE_ULTRASONIC
  pinMode(US_TRIG_PIN, OUTPUT);
  digitalWrite(US_TRIG_PIN, LOW);
  pinMode(US_ECHO_PIN, INPUT);
#endif
}

static float readOnceCm() {
  digitalWrite(US_TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(US_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(US_TRIG_PIN, LOW);
  unsigned long us = pulseIn(US_ECHO_PIN, HIGH, US_TIMEOUT_US);
  if (us == 0) return -1.0f;          // timeout / no echo
  return us / 58.0f;                  // sound: ~58 us per cm (round trip)
}

static void sortFloats(float *a, int n) {
  for (int i = 1; i < n; i++) {
    float k = a[i];
    int j = i - 1;
    while (j >= 0 && a[j] > k) { a[j + 1] = a[j]; j--; }
    a[j + 1] = k;
  }
}

float Sensor::readCm() {
#if ENABLE_ULTRASONIC
  float v[US_SAMPLES];
  int n = 0;
  for (int i = 0; i < US_SAMPLES; i++) {
    float d = readOnceCm();
    if (d > 0) v[n++] = d;
    delay(10);                        // let echoes die out between pings
  }
  if (n == 0) return -1.0f;
  sortFloats(v, n);
  return v[n / 2];                    // median rejects spikes
#else
  return -1.0f;
#endif
}

bool Sensor::calibrate() {
#if ENABLE_ULTRASONIC
  float v[US_BASELINE_SAMPLES];
  int n = 0;
  for (int i = 0; i < US_BASELINE_SAMPLES; i++) {
    float d = readCm();
    if (d > 0) v[n++] = d;
    delay(20);
  }
  if (n < US_BASELINE_SAMPLES / 2) {
    s_baseline  = -1.0f;
    s_threshold = TRIGGER_FALLBACK_CM;
    DBG("[US] baseline FAILED (%d valid). Using fallback threshold %.1f cm. Check wiring/divider!\n",
        n, s_threshold);
    return false;
  }
  sortFloats(v, n);
  s_baseline  = v[n / 2];
  s_threshold = s_baseline - TRIGGER_DELTA_CM;
  DBG("[US] baseline (empty flap) = %.1f cm, trigger below %.1f cm\n", s_baseline, s_threshold);
  return true;
#else
  return false;
#endif
}

bool Sensor::objectPresent(float cm) {
  return cm > 0 && cm < s_threshold;
}
