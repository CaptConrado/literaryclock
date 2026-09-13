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
  else if (key == "clock_24h")        config.clock24h = parseBool(val);
  else if (key == "show_time")        config.showTime = parseBool(val);
  else if (key == "sfw_only")         config.sfwOnly = parseBool(val);
  else if (key == "brightness")       config.brightness = constrain(val.toInt(), 5, 255);
  else if (key == "night_brightness") config.nightBrightness = constrain(val.toInt(), 0, 255);
  else if (key == "night_start")      config.nightStart = parseHHMM(val);
  else if (key == "night_end")        config.nightEnd = parseHHMM(val);
  else if (key == "touch_x_min")      config.touchXMin = val.toInt();
  else if (key == "touch_x_max")      config.touchXMax = val.toInt();
  else if (key == "touch_y_min")      config.touchYMin = val.toInt();
  else if (key == "touch_y_max")      config.touchYMax = val.toInt();
  else if (key == "touch_calibrated") config.touchCalibrated = parseBool(val);
  else if (key == "touch_swap_xy")    config.touchSwapXY = parseBool(val);
  else if (key == "touch_invert_x")   config.touchInvertX = parseBool(val);
  else if (key == "touch_invert_y")   config.touchInvertY = parseBool(val);
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
  f.printf("clock_24h = %s\n", config.clock24h ? "true" : "false");
  f.printf("show_time = %s\n", config.showTime ? "true" : "false");
  f.printf("sfw_only = %s\n\n", config.sfwOnly ? "true" : "false");
  f.print("# Backlight 5-255, and optional dimming between two times of day.\n");
  f.printf("brightness = %u\n", config.brightness);
  f.printf("night_brightness = %u\n", config.nightBrightness);
  f.printf("night_start = %s\n", fmtHHMM(config.nightStart).c_str());
  f.printf("night_end = %s\n\n", fmtHHMM(config.nightEnd).c_str());
  f.print("# Touch calibration, written by the on-screen calibration (menu > Calibrate).\n");
  f.print("# Set touch_calibrated = false to run it again at next boot.\n");
  f.printf("touch_calibrated = %s\n", config.touchCalibrated ? "true" : "false");
  f.printf("touch_x_min = %u\ntouch_x_max = %u\n", config.touchXMin, config.touchXMax);
  f.printf("touch_y_min = %u\ntouch_y_max = %u\n", config.touchYMin, config.touchYMax);
  f.printf("touch_swap_xy = %s\n", config.touchSwapXY ? "true" : "false");
  f.printf("touch_invert_x = %s\ntouch_invert_y = %s\n",
           config.touchInvertX ? "true" : "false", config.touchInvertY ? "true" : "false");
  f.close();
  return true;
}
