#define USE_BUILTIN_FONT

#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <GxEPD2_BW.h>
#ifndef USE_BUILTIN_FONT
#include <Fonts/FreeSans9pt7b.h>
#endif
#include <qrcode.h>
#include "Parameters.h"

#define SPI_MOSI 23
#define SPI_MISO -1
#define SPI_CLK 18

#define ELINK_SS 5
#define ELINK_BUSY 4
#define ELINK_RST 16
#define ELINK_DC 17

#define BTN_PIN 39
#define BTN_LEVEL LOW

#define LED_PIN 19
#define LED_LEVEL HIGH

#define QRCODE_VERSION 11 // 61x61

const int8_t DELTA_X = 0;
const int8_t DELTA_Y = 6;

const uint8_t OFFSET_X = 4;
const uint8_t OFFSET_Y = 0;

#ifdef USE_BUILTIN_FONT
const int8_t FONT_HEIGHT = 8;
#else
const int8_t FONT_HEIGHT = -12;
#endif
const uint8_t FONT_GAP = 2;

const char CP_SSID[] = "ESP32_BADGE";
const char CP_PSWD[] = "1029384756";

const paraminfo_t PARAMS[] = {
  PARAM_STR_CUSTOM("text", "Text", 321, "Your ad could be\there!", EDITOR_TEXTAREA(20, 12, 320, false, false, false))
};

Parameters params(PARAMS, ARRAY_SIZE(PARAMS));
GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(GxEPD2_213_B73(/*CS=*/ ELINK_SS, /*DC=*/ ELINK_DC, /*RST=*/ ELINK_RST, /*BUSY=*/ ELINK_BUSY));

static uint8_t getCharWidth(char c) {
  char str[2];
  int16_t x;
  uint16_t w, h;

  str[0] = c;
  str[1] = '\0';
  display.getTextBounds(str, 0, 0, &x, &x, &w, &h);
  return w;
}

static void rectPrint(const char *str, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, int8_t fontHeight = 8, uint8_t fontGap = 2) {
  int16_t x, y;
  uint8_t col;

  x = x1;
  y = y1;
  if (fontHeight < 0) // Custom font, Y is baseline
    y -= fontHeight;
  col = 0;
  display.setCursor(x, y);
  while (*str) {
    x = display.getCursorX();
    y = display.getCursorY();
    if (*str == '\t') {
      uint8_t w;

      w = getCharWidth(' ');
      do {
        if (x + w >= x2) {
          x = x1;
          y += (abs(fontHeight) + fontGap);
          if (y + (fontHeight < 0 ? 0 : fontHeight) < y2)
            display.setCursor(x, y);
          col = 0;
          break;
        }
        display.print(' ');
        x = display.getCursorX();
        y = display.getCursorY();
      } while (++col % 8);
      if (y + (fontHeight < 0 ? 0 : fontHeight) >= y2)
        break;
    } else if (*str == '\n') {
      y += (abs(fontHeight) + fontGap);
      if (y + (fontHeight < 0 ? 0 : fontHeight) >= y2)
        break;
      display.setCursor(x, y);
    } else if (*str == '\r') {
      x = x1;
      col = 0;
      display.setCursor(x, y);
    } else {
      if (x + getCharWidth(*str) >= x2) {
        x = x1;
        y += (abs(fontHeight) + fontGap);
        if (y + (fontHeight < 0 ? 0 : fontHeight) >= y2)
          break;
        col = 0;
        display.setCursor(x, y);
      } else {
        ++col;
      }
      display.print(*str);
    }
    ++str;
  }
}

static void drawQrCode(const char *text) {
  display.fillScreen(GxEPD_WHITE);
  if (text && *text) {
    QRCode qrcode;
    uint8_t *qrcodeData;

    qrcodeData = new uint8_t[qrcode_getBufferSize(QRCODE_VERSION)];
    if (qrcodeData) {
      qrcode_initText(&qrcode, qrcodeData, QRCODE_VERSION, 0, text);
      for (uint8_t y = 0; y < qrcode.size; ++y) {
        for (uint8_t x = 0; x < qrcode.size; ++x) {
          if (qrcode_getModule(&qrcode, x, y)) {
            display.fillRect(DELTA_X + x * 2, DELTA_Y + y * 2, 2, 2, GxEPD_BLACK);
          }
        }
      }
      delete[] qrcodeData;

      rectPrint(text, DELTA_X + qrcode.size * 2 + OFFSET_X, DELTA_Y + OFFSET_Y, display.width(), display.height(), FONT_HEIGHT, FONT_GAP);
    }
  }
  display.display();
}

static void halt(const char *msg) {
  display.fillScreen(GxEPD_WHITE);
  if (FONT_HEIGHT < 0)
    display.setCursor(DELTA_X, DELTA_Y - FONT_HEIGHT);
  else
    display.setCursor(DELTA_X, DELTA_Y);
  display.print(msg);
  display.display();

  display.powerOff();
  esp_deep_sleep_start();
}

void setup() {
  pinMode(BTN_PIN, INPUT_PULLUP);

  SPI.begin(SPI_CLK, SPI_MISO, SPI_MOSI, ELINK_SS);
  display.init();
  display.setRotation(1);
#ifndef USE_BUILTIN_FONT
  display.setFont(&FreeSans9pt7b);
#endif
  display.setTextColor(GxEPD_BLACK);

  if (! params.begin())
    halt("EEPROM ERROR!");

  if (digitalRead(BTN_PIN) == BTN_LEVEL) { // Button pressed, start captive portal
    display.fillScreen(GxEPD_WHITE);
    if (FONT_HEIGHT < 0)
      display.setCursor(DELTA_X, DELTA_Y - FONT_HEIGHT);
    else
      display.setCursor(DELTA_X, DELTA_Y);
    display.print("Starting captive portal \"");
    display.print(CP_SSID);
    display.println("\" for 60 seconds...");
    display.display();
    if (! paramsCaptivePortal(&params, CP_SSID, CP_PSWD, 60, LED_PIN, LED_LEVEL))
      halt("CP ERROR!");
  }

  drawQrCode((char*)params.value("text"));

  display.powerOff();
  esp_deep_sleep_start();
}

void loop() {}
