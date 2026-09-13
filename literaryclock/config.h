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
  // Touch calibration: raw 12-bit values that map to the screen edges.
  uint16_t touchXMin = 200, touchXMax = 3700;
  uint16_t touchYMin = 240, touchYMax = 3800;
  bool     touchCalibrated = false;   // set by the on-screen calibration; false runs it at boot
  bool     touchSwapXY  = false;
  bool     touchInvertX = false;
  bool     touchInvertY = false;
};

extern Config config;
extern const char* CONFIG_PATH;

bool loadConfig();   // false if the file is missing or unreadable
bool saveConfig();   // writes every key with comments
