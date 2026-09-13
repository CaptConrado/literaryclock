#include "config.h"
#include <SD.h>

Config config;
const char* CONFIG_PATH = "/literaryclock/config.txt";

static int parseHHMM(const String& v) {
  int colon = v.indexOf(':');
  if (colon < 0) return -1;
  int h = v.substring(0, colon).toInt();
  int m = v.substring(colon + 1).toInt();
  if (h < 0 || h > 23 || m < 0 || m > 59) return -1;
  return h * 60 + m;
}

static bool parseBool(const String& v) {
  String s = v; s.toLowerCase();
  return s == "true" || s == "yes" || s == "on" || s == "1";
}

static String fmtHHMM(int mins) {
  if (mins < 0) return "off";
  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", mins / 60, mins % 60);
  return String(buf);
}

static void applyKey(const String& key, const String& val) {
  if      (key == "wifi_ssid")        config.ssid = val;
  else if (key == "wifi_password")    config.password = val;
  else if (key == "timezone")         config.timezone = val;
  else if (key == "orientation") {
    String v = val; v.toLowerCase();
    if      (v == "landscape")         config.rotation = 1;
    else if (v == "portrait")          config.rotation = 0;
    else if (v == "landscape_flipped") config.rotation = 3;
    else if (v == "portrait_flipped")  config.rotation = 2;
  }
  else if (key == "clock_24h")        config.clock24h = parseBool(val);
  else if (key == "show_time")        config.showTime = parseBool(val);
  else if (key == "sfw_only")         config.sfwOnly = parseBool(val);
  else if (key == "brightness")       config.brightness = constrain(val.toInt(), 5, 255);
  else if (key == "night_brightness") config.nightBrightness = constrain(val.toInt(), 0, 255);
  else if (key == "night_start")      config.nightStart = parseHHMM(val);
  else if (key == "night_end")        config.nightEnd = parseHHMM(val);
  else if (key.startsWith("touch_cal_") && key.length() == 11) {
    // touch_cal_<rotation> = swap,xmin,xmax,ymin,ymax
    int r = key.charAt(10) - '0';
    if (r < 0 || r > 3) return;
    int v[5], n = 0, start = 0;
    while (n < 5) {
      int comma = val.indexOf(',', start);
      v[n++] = val.substring(start, comma < 0 ? val.length() : comma).toInt();
      if (comma < 0) break;
      start = comma + 1;
    }
    if (n == 5) {
      config.touch[r] = { true, v[0] != 0, (int16_t)v[1], (int16_t)v[2], (int16_t)v[3], (int16_t)v[4] };
    }
  }
}

bool loadConfig() {
  File f = SD.open(CONFIG_PATH, FILE_READ);
  if (!f) return false;
  config = Config();
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.isEmpty() || line[0] == '#') continue;
    int eq = line.indexOf('=');
    if (eq < 0) continue;
    String key = line.substring(0, eq); key.trim(); key.toLowerCase();
    String val = line.substring(eq + 1); val.trim();
    applyKey(key, val);
  }
  f.close();
  return true;
}

bool saveConfig() {
  SD.mkdir("/literaryclock");
  File f = SD.open(CONFIG_PATH, FILE_WRITE);
  if (!f) return false;
  f.print("# Literary Clock settings. Edit on a computer or via the WiFi setup screen.\n\n");
  f.print("# WiFi network used to fetch the time. Leave blank to set the time by touch.\n");
  f.printf("wifi_ssid = %s\n", config.ssid.c_str());
  f.printf("wifi_password = %s\n\n", config.password.c_str());
  f.print("# Timezone: a name such as Europe/London or America/New_York (see README\n");
  f.print("# for the supported list), or a POSIX TZ string like GMT0BST,M3.5.0/1,M10.5.0\n");
  f.printf("timezone = %s\n\n", config.timezone.c_str());
  f.print("# orientation: landscape, portrait, landscape_flipped or portrait_flipped\n");
  f.printf("orientation = %s\n", config.rotation == 1 ? "landscape" : config.rotation == 0 ? "portrait" :
                                  config.rotation == 3 ? "landscape_flipped" : "portrait_flipped");
  f.printf("clock_24h = %s\n", config.clock24h ? "true" : "false");
  f.printf("show_time = %s\n", config.showTime ? "true" : "false");
  f.printf("sfw_only = %s\n\n", config.sfwOnly ? "true" : "false");
  f.print("# Backlight 5-255, and optional dimming between two times of day.\n");
  f.printf("brightness = %u\n", config.brightness);
  f.printf("night_brightness = %u\n", config.nightBrightness);
  f.printf("night_start = %s\n", fmtHHMM(config.nightStart).c_str());
  f.printf("night_end = %s\n\n", fmtHHMM(config.nightEnd).c_str());
  f.print("# Touch calibration per display rotation, written by the on-screen calibration.\n");
  f.print("# Format: swap,xmin,xmax,ymin,ymax. Delete a line to recalibrate that rotation at next use.\n");
  for (int r = 0; r < 4; r++) {
    const Config::TouchCal& c = config.touch[r];
    if (c.valid) f.printf("touch_cal_%d = %d,%d,%d,%d,%d\n", r, c.swapXY ? 1 : 0, c.xMin, c.xMax, c.yMin, c.yMax);
  }
  f.close();
  return true;
}
