/*
 * camera_ai.cpp : OV2640 camera + Edge Impulse SmartBinV2 classifier.
 *
 * Based on the Edge Impulse esp32_camera example, with fixes:
 *  - snapshot buffer allocated ONCE in PSRAM (no malloc/free per loop -> no leak/fragmentation)
 *  - flash LED used during capture (chamber is dark; dataset was captured with LED on)
 *  - stale frames discarded before the real capture
 *  - new pin_sccb_* field names (no deprecation warnings on core 2.0.14)
 *
 * NOTE: <SmartBinV2_inferencing.h> must be included in ONLY this .cpp file
 *       (it defines model arrays; including it twice causes linker errors).
 */
#include "camera_ai.h"
#include "config.h"

#include <SmartBinV2_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "Invalid model for current sensor"
#endif

// ---- AI-Thinker ESP32-CAM camera pins ----
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

#define RAW_COLS   320   // QVGA
#define RAW_ROWS   240
#define RAW_BPP      3   // RGB888

static bool     s_ready      = false;
static bool     s_ledOn      = false;
static uint8_t  s_brightness = LED_DEFAULT_BRIGHTNESS;
static uint8_t *s_snapshot   = nullptr;   // RGB888 buffer (also holds the resized 96x96 image)

// ---------------------------------------------------------------------
void CameraAI::led(bool on) {
  s_ledOn = on;
  ledcWrite(LED_LEDC_CHANNEL, on ? s_brightness : 0);
}

void CameraAI::setLedBrightness(uint8_t val) {
  s_brightness = val;
  if (s_ledOn) {
    ledcWrite(LED_LEDC_CHANNEL, s_brightness);
  }
}

uint8_t CameraAI::ledBrightness() { return s_brightness; }

void CameraAI::blinkLed(int times, int onMs, int offMs) {
  bool prev = s_ledOn;
  for (int i = 0; i < times; i++) {
    ledcWrite(LED_LEDC_CHANNEL, s_brightness > 0 ? s_brightness : LED_DEFAULT_BRIGHTNESS);
    delay(onMs);
    ledcWrite(LED_LEDC_CHANNEL, 0);
    if (i < times - 1) delay(offMs);
  }
  led(prev);
}

bool CameraAI::ledState() { return s_ledOn; }
bool CameraAI::ready()    { return s_ready; }

// ---------------------------------------------------------------------
bool CameraAI::begin() {
  // Use LEDC Channel 4 (Timer 2) for PWM brightness control on GPIO 4 (core v2.0.14 API)
  ledcSetup(LED_LEDC_CHANNEL, LED_PWM_FREQ_HZ, LED_PWM_RES_BITS);
  ledcAttachPin(LED_PIN, LED_LEDC_CHANNEL);
  led(false);

  camera_config_t cfg = {};
  cfg.pin_pwdn     = PWDN_GPIO_NUM;
  cfg.pin_reset    = RESET_GPIO_NUM;
  cfg.pin_xclk     = XCLK_GPIO_NUM;
  cfg.pin_sccb_sda = SIOD_GPIO_NUM;
  cfg.pin_sccb_scl = SIOC_GPIO_NUM;
  cfg.pin_d7 = Y9_GPIO_NUM;  cfg.pin_d6 = Y8_GPIO_NUM;
  cfg.pin_d5 = Y7_GPIO_NUM;  cfg.pin_d4 = Y6_GPIO_NUM;
  cfg.pin_d3 = Y5_GPIO_NUM;  cfg.pin_d2 = Y4_GPIO_NUM;
  cfg.pin_d1 = Y3_GPIO_NUM;  cfg.pin_d0 = Y2_GPIO_NUM;
  cfg.pin_vsync = VSYNC_GPIO_NUM;
  cfg.pin_href  = HREF_GPIO_NUM;
  cfg.pin_pclk  = PCLK_GPIO_NUM;
  cfg.xclk_freq_hz = 20000000;
  cfg.ledc_timer   = LEDC_TIMER_0;     // camera owns timer 0 / channel 0
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.frame_size   = FRAMESIZE_QVGA;   // 320x240, same as the Edge Impulse example
  cfg.jpeg_quality = 12;
  cfg.fb_count     = 1;
  cfg.fb_location  = CAMERA_FB_IN_PSRAM;
  cfg.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

  if (!psramFound()) {
    DBGLN("[CAM] WARNING: PSRAM not found - check board settings (PSRAM Enabled)");
    cfg.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    DBG("[CAM] init failed: 0x%x (check ribbon cable / 5V supply)\n", err);
    s_ready = false;
    return false;
  }

  // Allocate RGB888 snapshot buffer once (230 KB -> PSRAM)
  const size_t bufSize = RAW_COLS * RAW_ROWS * RAW_BPP;
  s_snapshot = (uint8_t *)(psramFound() ? ps_malloc(bufSize) : malloc(bufSize));
  if (!s_snapshot) {
    DBGLN("[CAM] ERROR: cannot allocate snapshot buffer");
    esp_camera_deinit();
    s_ready = false;
    return false;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s && s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);
    s->set_brightness(s, 1);
    s->set_saturation(s, 0);
  }

  s_ready = true;
  DBG("[CAM] ready. Model: %d classes, input %dx%d\n",
      EI_CLASSIFIER_LABEL_COUNT, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);
  return true;
}

// ---------------------------------------------------------------------
// Edge Impulse signal callback: RGB888 buffer -> packed float pixels
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr) {
  size_t pixel_ix = offset * 3;
  for (size_t i = 0; i < length; i++) {
    // Swap BGR -> RGB (https://github.com/espressif/esp32-camera/issues/379)
    out_ptr[i] = (s_snapshot[pixel_ix + 2] << 16) + (s_snapshot[pixel_ix + 1] << 8) + s_snapshot[pixel_ix];
    pixel_ix += 3;
  }
  return 0;
}

// Capture one JPEG frame, convert to RGB888 and resize to model input size
static bool captureToSnapshot() {
  // Throw away stale frames (buffer may hold an image from before LED/flap change)
  for (int i = 0; i < CAM_DISCARD_FRAMES; i++) {
    camera_fb_t *old = esp_camera_fb_get();
    if (old) esp_camera_fb_return(old);
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    DBGLN("[CAM] capture failed");
    return false;
  }
  bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, s_snapshot);
  esp_camera_fb_return(fb);
  if (!converted) {
    DBGLN("[CAM] JPEG -> RGB888 conversion failed");
    return false;
  }

  if (EI_CLASSIFIER_INPUT_WIDTH != RAW_COLS || EI_CLASSIFIER_INPUT_HEIGHT != RAW_ROWS) {
    ei::image::processing::crop_and_interpolate_rgb888(
      s_snapshot, RAW_COLS, RAW_ROWS,
      s_snapshot, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT);
  }
  return true;
}

// ---------------------------------------------------------------------
bool CameraAI::classify(ClassResult &out) {
  out.index = -1;
  out.label = "none";
  out.score = 0.0f;
  out.count = 0;
  out.timeMs = 0;

  if (!s_ready) {
    DBGLN("[AI] camera not ready");
    return false;
  }

  // LED ON only during capture (avoid extra current draw while servo/stepper move)
  led(true);
  delay(LED_WARMUP_MS);
  bool ok = captureToSnapshot();
  led(false);
  if (!ok) return false;

  ei::signal_t signal;
  signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
  signal.get_data = &ei_camera_get_data;

  ei_impulse_result_t result = { 0 };
  EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
  if (err != EI_IMPULSE_OK) {
    DBG("[AI] run_classifier failed (%d)\n", err);
    return false;
  }

  out.count  = EI_CLASSIFIER_LABEL_COUNT;
  out.timeMs = result.timing.dsp + result.timing.classification;
  DBG("[AI] DSP %d ms, NN %d ms:", result.timing.dsp, result.timing.classification);
  for (int i = 0; i < EI_CLASSIFIER_LABEL_COUNT && i < 8; i++) {
    float v = result.classification[i].value;
    out.scores[i] = v;
    DBG("  %s=%.2f", result.classification[i].label, v);
    if (v > out.score) {
      out.score = v;
      out.index = i;
      out.label = result.classification[i].label;
    }
  }
  DBG("\n");
  return out.index >= 0;
}

// ---------------------------------------------------------------------
int CameraAI::labelToSector(const char *label) {
  if (!label) return FALLBACK_SECTOR;
  if (strcmp(label, "Organik") == 0)   return SECTOR_ORGANIK;
  if (strcmp(label, "Anorganik") == 0) return SECTOR_ANORGANIK;
  if (strcmp(label, "B3") == 0)        return SECTOR_B3;
  return FALLBACK_SECTOR;
}
