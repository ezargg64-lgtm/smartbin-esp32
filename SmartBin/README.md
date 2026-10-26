# SmartBin firmware

AI waste sorter on an AI-Thinker ESP32-CAM: the Edge Impulse model **SmartBinV2** (Organik / Anorganik / B3), an MG995 flap, an NEMA17 turntable, an HC-SR04 trigger and a 16x2 LCD.

## Arduino IDE settings
| Setting | Value |
|---|---|
| Board | **AI Thinker ESP32-CAM** |
| ESP32 core | **2.0.14** (do NOT use 3.x) |
| Partition Scheme | **Huge APP (3MB No OTA/1MB SPIFFS)** |
| Serial Monitor | 115200 baud, any line ending |

Libraries: SmartBinV2_inferencing 1.0.5, ESP32Servo 1.2.1, AccelStepper 1.64, LiquidCrystal_I2C 2.0.0

## Files
| File | Purpose |
|---|---|
| `config.h` | **All** pins, angles, timings, thresholds, feature flags |
| `SmartBin.ino` | setup, trigger state machine, sort cycle |
| `camera_ai.*` | camera, flash LED, Edge Impulse classification |
| `flap.*` | MG995 servo (LEDC timer 1, smooth moves) |
| `turntable.*` | A4988/NEMA17, absolute position, shortest path |
| `sensor.*` | HC-SR04, median filter, empty-flap baseline |
| `display.*` | LCD screens (shown on Serial when LCD disabled) |
| `serial_cmd.*` | calibration commands |
| `telemetry.*` | optional WiFi (off) |

## Feature flags (`config.h`)
| Flag | Phase 1 | Phase 2 (stepper) | Production |
|---|---|---|---|
| `ENABLE_SERIAL_DEBUG` | 1 | 1 | **0** |
| `ENABLE_STEPPER` | 0 | **1** | 1 |
| `ENABLE_ULTRASONIC` | 1 | 1 | 1 |
| `ENABLE_LCD` | 0 | 0 | **1** |
| `ENABLE_WIFI` | 0 | 0 | 0 / 1 |

## Wiring
| ESP32-CAM | Connects to | Notes |
|---|---|---|
| 5V / GND | LM2596 OUT+ / OUT- (5.0 V) | common ground with everything |
| GPIO 13 | MG995 signal (orange) | servo red to 5V, brown to GND, **470–1000 µF** across servo 5V/GND |
| GPIO 4 | onboard flash LED | — |
| GPIO 12 | A4988 STEP | **10 kΩ pull-down to GND** (otherwise the ESP32 won't boot) |
| GPIO 14 | A4988 DIR | — |
| GPIO 15 | HC-SR04 TRIG | HC-SR04 VCC = 5V |
| GPIO 2 | HC-SR04 ECHO | **through a 1k/2k divider** (ECHO → 1k → GPIO2 → 2k → GND) |
| GPIO 1 / 3 | LCD SDA / SCL | **production only**, via logic level converter. Unplug while uploading |
| 3.3V | A4988 VDD + MS1 + MS2 + MS3 | 1/16 microstep, 3.3V logic |

A4988: VMOT = 12V with a **100 µF cap**, **RESET bridged to SLEEP**, EN → GND, Vref ≈ 0.65 V (Rs = 0.1 Ω → 0.81 A). Fit the heatsink.

## Before every power-on
1. Turn the turntable by hand so **Sector 0 (Organik)** is under the flap.
2. Make sure the flap is **empty**, because the ultrasonic baseline is measured at boot.

## Serial commands
`o` open · `c` close · `0..180` angle · `t` flap test · `x` classify only · `l` toggle LED · `f0..f255` set flash brightness (0..255) · `d` distance · `b` re-baseline · `s0/s1/s2` sector · `z` set sector 0 here · `r` full cycle · `?` status · `h` help

## Test order
1. `o` / `c` / angles: calibrate `FLAP_OPEN_ANGLE` / `FLAP_CLOSE_ANGLE`
2. `x`: check AI scores inside the closed chamber
3. `d` with an empty flap and with waste: tune `TRIGGER_DELTA_CM`
4. `r`, then drop real waste: full cycle (stepper simulated)
5. NEMA17 arrives: wiring above, `ENABLE_STEPPER 1`, test `s1`, `s2`, `s0`, then the full cycle
6. Production: `ENABLE_SERIAL_DEBUG 0`, `ENABLE_LCD 1`
