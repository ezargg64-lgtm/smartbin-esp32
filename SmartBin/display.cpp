/*
 * display.cpp : 16x2 LCD (PCF8574 backpack, addr 0x27) on SDA=GPIO1, SCL=GPIO3 (production only).
 * Backpack pull-ups are 5V -> use a logic level converter to protect the ESP32 pins.
 * Unplug the LCD from GPIO1/3 while uploading firmware.
 */
#include "display.h"
#include "config.h"

#if ENABLE_LCD
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
static LiquidCrystal_I2C s_lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
#endif

void Display::begin() {
#if ENABLE_LCD
  // Must start Wire on our pins BEFORE lcd.init(): the library calls Wire.begin() with no
  // arguments, which would otherwise pick default pins 21/22 (used by the camera!).
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  s_lcd.init();
  s_lcd.backlight();
  s_lcd.clear();
#endif
}

void Display::show(const char *line1, const char *line2) {
#if ENABLE_LCD
  char buf[LCD_COLS + 1];
  s_lcd.clear();
  snprintf(buf, sizeof(buf), "%-16s", line1);
  s_lcd.setCursor(0, 0);
  s_lcd.print(buf);
  snprintf(buf, sizeof(buf), "%-16s", line2);
  s_lcd.setCursor(0, 1);
  s_lcd.print(buf);
#endif
  DBG("[LCD] | %-16s |\n[LCD] | %-16s |\n", line1, line2);
}

void Display::ready()     { show("SmartBin Ready", "Drop waste here"); }
void Display::analyzing() { show("Analyzing...", "Hold on...."); }

void Display::result(const char *label, float conf, int sector) {
  char l1[LCD_COLS + 1], l2[LCD_COLS + 1];
  snprintf(l1, sizeof(l1), "%s (%d%%)", label, (int)(conf * 100.0f + 0.5f));
  snprintf(l2, sizeof(l2), "Bin -> Sector %d", sector);
  show(l1, l2);
}

void Display::unknown(int sector) {
  char l2[LCD_COLS + 1];
  snprintf(l2, sizeof(l2), "Bin -> Sector %d", sector);
  show("Unknown / Residu", l2);
}

void Display::error(const char *line2) { show("!! ERROR !!", line2); }
