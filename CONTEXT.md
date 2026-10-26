# Project Context: SmartBin Automated Waste Sorter (Firmware & Hardware)

## 1. Project Overview
SmartBin is an IoT and AI-powered automated waste-sorting prototype housed inside a modified 19L gallon container. 
- **Top Dome Chamber:** Contains the 15x12 cm waste inlet funnel, ESP32-CAM (facing down), illumination LED, Ultrasonic Sensor, and 16x2 I2C LCD.
- **Middle Sorting Chamber:** Contains an Acrylic Drop Flap Door driven by an MG995 Servo Motor, and a 3-Sector Turntable Disc below it.
- **Protected Dry Basement Chamber:** Houses the power distribution breadboard, LM2596 step-down buck converter (calibrated to 5.0V), A4988 Stepper Driver, and NEMA 17 Stepper Motor.

## 2. Current Development Phase (IMPORTANT FOR AGENT)
**Active Scope: Phase 1 — Servo MG995 & ESP32-CAM Integration Testing.**
- The NEMA 17 stepper motor has **not arrived yet**.
- The A4988 stepper driver is **currently disconnected** from the logic power rail to prevent short circuits during initial testing.
- **Immediate Goal:** Write, test, and refine C++ / Arduino firmware (`.ino`) for the ESP32-CAM to control the MG995 Servo Motor (Acrylic Drop Flap Door), test camera capture/streaming, and prepare modular code that can easily enable the A4988 stepper motor later via feature flags or modular functions.

## 3. Hardware & Power Architecture
- **Main Power Source:** 12V 2A DC Wall Adapter connected via a recessed female DC barrel jack.
- **12V Rail (Breadboard `gh` side):** Directly fed from the 12V DC Jack (Row 20 = `12V+`, Row 21 = `GND`). Reserved for NEMA 17 `VMOT` via A4988.
- **5V Logic & Servo Rail (Breadboard `ab` side):** Powered by an **LM2596 Buck Converter** (`IN+`/`IN-` connected to the 12V row; `OUT+`/`OUT-` calibrated strictly to **`5.0V`** feeding the `ab` power rails).
- **Common Ground:** ESP32-CAM `GND`, MG995 Servo `GND` (Brown wire), and LM2596 `OUT-` share a common ground on the breadboard `ab` negative rail.

## 4. Microcontroller & Environment Specs
- **Board:** AI-Thinker ESP32-CAM (programmed via ESP32-CAM-MB USB downloader or FTDI).
- **Arduino ESP32 Core Version:** `v2.0.14` (CRITICAL: Use `v2.0.x` LEDC PWM APIs `ledcSetup` / `ledcAttachPin`, NOT the breaking `v3.0.x` `ledcAttach` API).
- **Partition Scheme:** Huge APP (3MB No OTA / 1MB SPIFFS).
- **PSRAM:** Enabled.

## 5. Pinout & GPIO Mapping (AI-Thinker ESP32-CAM)
> **WARNING FOR AGENT:** Do NOT use pins reserved for the onboard OV2640 Camera (`GPIO 0, 5, 18, 19, 21, 22, 23, 25, 26, 27, 32, 34, 35, 36, 39`). SD Card is NOT used, freeing up `GPIO 2, 4, 12, 13, 14, 15`.

### Active Components (Phase 1):
- **MG995 Servo (Flap Door):**
  - `PWM Signal (Orange)`: **GPIO 13**
  - `VCC (Red)`: Breadboard 5V Rail (`LM2596 OUT+`)
  - `GND (Brown)`: Breadboard GND Rail (`LM2596 OUT-`)
- **Onboard Flash / Illumination LED:** **GPIO 4** (Note: Shares pin with SD Data 1; safe to use since SD card is omitted).

### Planned / Modular Components (Phase 2 - Keep Configurable):
- **A4988 Stepper Driver (Turntable):**
  - `STEP`: **GPIO 12** (Must not be pulled HIGH during boot/strapping)
  - `DIR`: **GPIO 14**
- **Ultrasonic Sensor (HC-SR04) / 16x2 I2C LCD:**
  - Available Free Pins: **GPIO 15**, **GPIO 2**, **GPIO 1 (TX)** / **GPIO 3 (RX)** (if Serial debugging is disabled in production).

## 6. Critical Technical Constraints for AI Agent
1. **LEDC Timer Conflict (Camera vs. Servo):**
   - The OV2640 camera driver uses `LEDC_TIMER_0` and `LEDC_CHANNEL_0` for `XCLK` generation.
   - When using `<ESP32Servo.h>`, or manual `ledcSetup()`, **explicitly allocate Timers 1, 2, or 3** (e.g., `ESP32PWM::allocateTimer(1);`) so the servo PWM does not overwrite the camera clock timer and cause `Camera probe failed` errors.
2. **MG995 Pulse Widths & Brownout Prevention:**
   - Standard frequency: `50Hz`.
   - Pulse width range: `500us` (0°) to `2400us` (180°).
   - Avoid instantaneous full-sweep jerks if the camera is capturing simultaneously; implement smooth angle transitions or sequential execution (Capture Image -> Classify -> Move Turntable -> Open Flap -> Close Flap).
3. **Flap Door Mechanical Logic:**
   - **Closed State (Hold Waste):** `0°` (Horizontal position holding waste inside the middle chamber for camera inspection).
   - **Open State (Drop Waste):** `90°` (Tilts down to drop waste onto the 3-sector turntable below).

## 7. Expected Code Structure & Deliverables
When asked to generate or update code, the Agent should:
1. Provide clean, well-commented `.ino` / C++ code compatible with **esp32 core v2.0.14**.
2. Include a `config.h` or top-level `#define` block so pins (`SERVO_PIN`, `STEP_PIN`, `DIR_PIN`) and mechanical angles (`FLAP_CLOSE_ANGLE`, `FLAP_OPEN_ANGLE`) can be tuned easily.
3. Provide a **Serial Monitor Command Interface** (e.g., typing `'o'` to open flap, `'c'` to close flap, `'t'` to run a full cycle test, or `'0'`–`'180'` to test custom servo angles) so hardware calibration inside the gallon chamber can be done interactively without re-uploading code.