/*
 * SmartBin - Automated Waste Sorter
 * config.h : ALL tunable settings live here (pins, angles, timings, thresholds, feature flags).
 *
 * Target : AI-Thinker ESP32-CAM
 * Core   : esp32 Arduino core v2.0.14 (uses v2.x LEDC API - do NOT build with core v3.x)
 * Board  : "AI Thinker ESP32-CAM", Partition Scheme "Huge APP (3MB No OTA/1MB SPIFFS)"
 */
#pragma once
#include <Arduino.h>

// =====================================================================
// 1. FEATURE FLAGS  (1 = enabled, 0 = disabled)
// =====================================================================
#define ENABLE_SERIAL_DEBUG  1   // Phase 1: keep ON (Serial log + calibration commands on GPIO1/3)
#define ENABLE_STEPPER       0   // Phase 2: set to 1 once NEMA17 + A4988 are wired (VDD/MS1-3 on 3.3V!)
#define ENABLE_ULTRASONIC    1   // HC-SR04 auto trigger. 0 = trigger only via Serial command 'r'
#define ENABLE_LCD           0   // Production: LCD on GPIO1/3 -> requires ENABLE_SERIAL_DEBUG 0
#define ENABLE_WIFI          0   // Optional telemetry (offline by default for demo reliability)

#if ENABLE_LCD && ENABLE_SERIAL_DEBUG
#error "LCD uses GPIO1/GPIO3 (Serial TX/RX). Set ENABLE_SERIAL_DEBUG 0 when ENABLE_LCD 1."
#endif

// =====================================================================
// 2. PINOUT (AI-Thinker ESP32-CAM)
//    Camera reserved: 0,5,18,19,21,22,23,25,26,27,32,34,35,36,39 -> never use!
// =====================================================================
#define SERVO_PIN        13   // MG995 signal (orange)
#define LED_PIN           4   // Onboard flash LED (plain digital ON/OFF, no PWM timer)
#define STEP_PIN         12   // A4988 STEP  (strapping pin! needs 10k pull-down to GND)
#define DIR_PIN          14   // A4988 DIR
#define US_TRIG_PIN      15   // HC-SR04 TRIG
#define US_ECHO_PIN       2   // HC-SR04 ECHO (via 1k/2k divider -> 3.3V max!)
#define LCD_SDA_PIN       1   // Production only (U0TXD) - via level shifter
#define LCD_SCL_PIN       3   // Production only (U0RXD) - via level shifter
#define LCD_I2C_ADDR   0x27
#define LCD_COLS         16
#define LCD_ROWS          2

// =====================================================================
// 3. SERVO / FLAP DOOR (MG995)
// =====================================================================
#define SERVO_PWM_TIMER      1      // LEDC timer for servo. NEVER 0 (camera XCLK uses timer 0)
#define SERVO_FREQ_HZ       50
#define SERVO_MIN_US       500      // pulse at 0 deg
#define SERVO_MAX_US      2400      // pulse at 180 deg
#define FLAP_CLOSE_ANGLE     0      // horizontal, holds waste for inspection
#define FLAP_OPEN_ANGLE     90      // tilted down, drops waste onto turntable
#define FLAP_STEP_DEG        2      // smooth motion: degrees per step
#define FLAP_STEP_DELAY_MS  15      // smooth motion: delay per step (2deg/15ms ~ 0.7s for 90deg)
#define FLAP_OPEN_HOLD_MS 2000      // keep flap open so waste fully drops

// =====================================================================
// 4. ULTRASONIC TRIGGER (HC-SR04 looking down at the closed flap)
// =====================================================================
#define US_TIMEOUT_US        25000  // ~4.3 m max echo wait
#define US_SAMPLES               5  // median filter size per reading
#define US_POLL_INTERVAL_MS     60  // time between readings while idle
#define US_BASELINE_SAMPLES     15  // readings used for empty-flap baseline at boot
#define TRIGGER_DELTA_CM       3.0  // object detected if distance < baseline - delta
#define TRIGGER_HOLD_MS        500  // object must be present this long (debounce)
#define TRIGGER_FALLBACK_CM   11.0  // used if baseline measurement fails
#define REARM_HOLD_MS         1000  // flap must read "empty" this long before re-arming
#define REARM_TIMEOUT_MS     15000  // if still not empty -> warn and re-arm anyway

// =====================================================================
// 5. CAMERA + AI CLASSIFICATION
// =====================================================================
#define SETTLE_MS              1200  // wait for waste to lie still before photo
#define LED_WARMUP_MS           150  // LED on before capture
#define CAM_DISCARD_FRAMES        2  // drop stale frames (old buffer before LED/flap change)
#define CONF_THRESHOLD         0.70f // min confidence to trust a class
#define MAX_RETRIES               1  // extra capture attempts when confidence is low
#define RESULT_SHOW_MS          800  // show result on LCD before moving

// =====================================================================
// 6. TURNTABLE (NEMA17 17HS4401 + A4988, direct drive 1:1, 1/16 microstep)
// =====================================================================
#define MOTOR_FULL_STEPS     200
#define MICROSTEPS            16
#define STEPS_PER_REV       (MOTOR_FULL_STEPS * MICROSTEPS)   // 3200
#define NUM_SECTORS            3
#define STEPPER_MAX_SPEED   1500.0f  // steps/s
#define STEPPER_ACCEL       1000.0f  // steps/s^2
#define STEPPER_MIN_PULSE_US   2     // A4988 needs >= 1 us
#define STEPPER_SETTLE_MS    200     // let turntable stop wobbling before opening flap

// Sector map:  0 = Organik (0 deg / home), 1 = Anorganik (120 deg), 2 = B3 / Residu (240 deg)
#define SECTOR_ORGANIK        0
#define SECTOR_ANORGANIK      1
#define SECTOR_B3             2
#define FALLBACK_SECTOR       SECTOR_B3   // low-confidence items go to B3 / Residu

// =====================================================================
// 7. WIFI (only used when ENABLE_WIFI 1) - fill in privately, do not share
// =====================================================================
#define WIFI_SSID         "your-ssid"
#define WIFI_PASSWORD     "your-password"
#define WIFI_TIMEOUT_MS   15000

// =====================================================================
// 8. DEBUG MACROS
// =====================================================================
#if ENABLE_SERIAL_DEBUG
  #define DBG(...)    Serial.printf(__VA_ARGS__)
  #define DBGLN(s)    Serial.println(s)
#else
  #define DBG(...)    do {} while (0)
  #define DBGLN(s)    do {} while (0)
#endif
