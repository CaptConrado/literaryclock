// Settings read from /literaryclock/config.txt on the SD card.
#pragma once
#include <Arduino.h>

struct Config {
  String   ssid;
  String   password;
  String   timezone       = "UTC0";   // IANA name from the table in timesrc.cpp, or a POSIX TZ string
  uint8_t  rotation       = 1;        // TFT_eSPI rotation: 1 landscape, 0 portrait, 2/3 flipped
  bool     clock24h       = true;
  bool     showTime       = true;     // small digital time in the footer
  bool     sfwOnly        = false;    // skip quotes flagged not-safe-for-work
  uint8_t  brightness     = 255;
  uint8_t  nightBrightness = 30;
  int16_t  nightStart     = -1;       // minutes after midnight, -1 = no night dimming
  int16_t  nightEnd       = -1;
  // Touch calibration, one set per display rotation (index = rotation 0..3), measured in
  // that rotation's own screen frame. Raw 12-bit values at the screen edges; min may exceed max.
  struct TouchCal { bool valid = false; bool swapXY = false; int16_t xMin = 0, xMax = 4095, yMin = 0, yMax = 4095; };
  TouchCal touch[4];
};

extern Config config;
extern const char* CONFIG_PATH;

bool loadConfig();   // false if the file is missing or unreadable
bool saveConfig();   // writes every key with comments
