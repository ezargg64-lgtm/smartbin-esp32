/*
 * camera_ai.h : OV2640 camera, flash LED and Edge Impulse (SmartBinV2) classification.
 */
#pragma once
#include <Arduino.h>

struct ClassResult {
  int         index;      // index of best class (-1 if failed)
  const char *label;      // model label of best class ("Anorganik", "B3", "Organik")
  float       score;      // confidence of best class 0..1
  float       scores[8];  // all class scores (model has 3)
  int         count;      // number of classes
  uint32_t    timeMs;     // DSP + inference time
};

namespace CameraAI {
  bool begin();                       // init camera + allocate buffers. false = camera failed
  bool ready();                       // camera initialised OK
  void led(bool on);                  // flash LED on (at default brightness) or off
  void setLedBrightness(uint8_t val); // set default flash brightness (0..255) and apply if on
  uint8_t ledBrightness();            // current configured brightness (0..255)
  void blinkLed(int times, int onMs = 80, int offMs = 80); // visual status/error blink
  bool ledState();
  bool classify(ClassResult &out);    // LED on -> discard frames -> capture -> classify -> LED off
  int  labelToSector(const char *label);
}
