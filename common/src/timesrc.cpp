#include "timesrc.h"
#include "config.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <time.h>
#include <sys/time.h>

struct TzEntry { const char* name; const char* posix; };
static const TzEntry TZ_TABLE[] = {
  { "UTC",                      "UTC0" },
  { "Europe/London",            "GMT0BST,M3.5.0/1,M10.5.0" },
  { "Europe/Dublin",            "IST-1GMT0,M10.5.0,M3.5.0/1" },
  { "Europe/Lisbon",            "WET0WEST,M3.5.0/1,M10.5.0" },
  { "Europe/Paris",             "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Berlin",            "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Madrid",            "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Rome",              "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Amsterdam",         "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Brussels",          "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Vienna",            "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Zurich",            "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Stockholm",         "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Oslo",              "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Copenhagen",        "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Prague",            "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Warsaw",            "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Budapest",          "CET-1CEST,M3.5.0,M10.5.0/3" },
  { "Europe/Athens",            "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Europe/Helsinki",          "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Europe/Kyiv",              "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Europe/Bucharest",         "EET-2EEST,M3.5.0/3,M10.5.0/4" },
  { "Europe/Istanbul",          "<+03>-3" },
  { "Europe/Moscow",            "MSK-3" },
  { "America/New_York",         "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Toronto",          "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Detroit",          "EST5EDT,M3.2.0,M11.1.0" },
  { "America/Chicago",          "CST6CDT,M3.2.0,M11.1.0" },
  { "America/Mexico_City",      "CST6" },
  { "America/Denver",           "MST7MDT,M3.2.0,M11.1.0" },
  { "America/Phoenix",          "MST7" },
  { "America/Los_Angeles",      "PST8PDT,M3.2.0,M11.1.0" },
  { "America/Vancouver",        "PST8PDT,M3.2.0,M11.1.0" },
  { "America/Anchorage",        "AKST9AKDT,M3.2.0,M11.1.0" },
  { "Pacific/Honolulu",         "HST10" },
  { "America/Sao_Paulo",        "<-03>3" },
  { "America/Argentina/Buenos_Aires", "<-03>3" },
  { "America/Bogota",           "<-05>5" },
  { "America/Lima",             "<-05>5" },
  { "America/Santiago",         "<-04>4<-03>,M9.1.6/24,M4.1.6/24" },
  { "Asia/Tokyo",               "JST-9" },
  { "Asia/Seoul",               "KST-9" },
  { "Asia/Shanghai",            "CST-8" },
  { "Asia/Hong_Kong",           "HKT-8" },
  { "Asia/Singapore",           "<+08>-8" },
  { "Asia/Taipei",              "CST-8" },
  { "Asia/Manila",              "PST-8" },
  { "Asia/Bangkok",             "<+07>-7" },
  { "Asia/Jakarta",             "WIB-7" },
  { "Asia/Kolkata",             "IST-5:30" },
  { "Asia/Karachi",             "PKT-5" },
  { "Asia/Dubai",               "<+04>-4" },
  { "Asia/Tehran",              "<+0330>-3:30" },
  { "Asia/Jerusalem",           "IST-2IDT,M3.4.4/26,M10.5.0" },
  { "Australia/Sydney",         "AEST-10AEDT,M10.1.0,M4.1.0/3" },
  { "Australia/Melbourne",      "AEST-10AEDT,M10.1.0,M4.1.0/3" },
  { "Australia/Brisbane",       "AEST-10" },
  { "Australia/Adelaide",       "ACST-9:30ACDT,M10.1.0,M4.1.0/3" },
  { "Australia/Perth",          "AWST-8" },
  { "Pacific/Auckland",         "NZST-12NZDT,M9.5.0,M4.1.0/3" },
  { "Africa/Johannesburg",      "SAST-2" },
  { "Africa/Cairo",             "EET-2EEST,M4.5.5/0,M10.5.4/24" },
  { "Africa/Lagos",             "WAT-1" },
  { "Africa/Nairobi",           "EAT-3" },
};
static const int TZ_COUNT = sizeof(TZ_TABLE) / sizeof(TZ_TABLE[0]);

String resolveTimezone(const String& v) {
  for (int i = 0; i < TZ_COUNT; i++) {
    if (v.equalsIgnoreCase(TZ_TABLE[i].name)) return String(TZ_TABLE[i].posix);
  }
  return v.isEmpty() ? String("UTC0") : v;
}

void applyTimezone() {
  String tz = resolveTimezone(config.timezone);
  setenv("TZ", tz.c_str(), 1);
  tzset();
}

bool wifiConnect(uint32_t timeoutMs, void (*progress)(int)) {
  if (config.ssid.isEmpty()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.begin(config.ssid.c_str(), config.password.c_str());
  uint32_t start = millis();
  int lastSec = -1;
  while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
    int sec = (millis() - start) / 1000;
    if (sec != lastSec && progress) { progress(sec); lastSec = sec; }
    delay(100);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }

void ntpBegin() {
  String tz = resolveTimezone(config.timezone);
  configTzTime(tz.c_str(), "pool.ntp.org", "time.google.com", "time.nist.gov");
}

bool timeIsValid() { return time(nullptr) > 1700000000; }   // after Nov 2023

void setLocalHourMinute(int hour, int minute) {
  time_t now = time(nullptr);
  if (!timeIsValid()) now = 1767225600;   // 2026-01-01 00:00 UTC as a plausible date
  struct tm t;
  localtime_r(&now, &t);
  t.tm_hour = hour; t.tm_min = minute; t.tm_sec = 0; t.tm_isdst = -1;
  time_t target = mktime(&t);
  struct timeval tv = { .tv_sec = target, .tv_usec = 0 };
  settimeofday(&tv, nullptr);
}

// ---------------- Captive portal ----------------
static const char* AP_NAME = "LiteraryClock";
static WebServer* server = nullptr;
static DNSServer* dns = nullptr;
static String scannedSsids;      // pre-rendered <option> list
static bool portalSaved = false;

const char* portalApName() { return AP_NAME; }
String portalIp() { return WiFi.softAPIP().toString(); }

static String htmlEscape(const String& s) {
  String o; o.reserve(s.length() + 8);
  for (char c : s) {
    switch (c) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '"': o += "&quot;"; break;
      default: o += c;
    }
  }
  return o;
}

static void scanNetworks() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  scannedSsids = "";
  for (int i = 0; i < n && i < 30; i++) {
    String ssid = WiFi.SSID(i);
    if (ssid.isEmpty() || scannedSsids.indexOf("value=\"" + htmlEscape(ssid) + "\"") >= 0) continue;
    scannedSsids += "<option value=\"" + htmlEscape(ssid) + "\"";
    if (ssid == config.ssid) scannedSsids += " selected";
    scannedSsids += ">" + htmlEscape(ssid) + " (" + String(WiFi.RSSI(i)) + " dBm)</option>";
  }
  WiFi.scanDelete();
}

static void handleRoot() {
  String tzOptions;
  for (int i = 0; i < TZ_COUNT; i++) {
    tzOptions += "<option";
    if (config.timezone.equalsIgnoreCase(TZ_TABLE[i].name)) tzOptions += " selected";
    tzOptions += ">" + String(TZ_TABLE[i].name) + "</option>";
  }
  String page =
    "<!doctype html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Literary Clock setup</title><style>body{font-family:Georgia,serif;max-width:26em;margin:2em auto;padding:0 1em;"
    "background:#f6f1e7;color:#222}label{display:block;margin-top:1em}input,select{width:100%;padding:.5em;font-size:1em;"
    "margin-top:.3em}button{margin-top:1.5em;padding:.7em 1.4em;font-size:1em;background:#7a1f1f;color:#fff;border:0;border-radius:4px}"
    "small{color:#666}</style></head><body><h1>Literary Clock</h1>"
    "<form method=post action=/save>"
    "<label>WiFi network<select name=ssid>" + scannedSsids + "<option value=''>Other (type below)</option></select></label>"
    "<label>Other network name<input name=ssid_other placeholder='only if not listed'></label>"
    "<label>WiFi password<input name=password type=password value=\"" + htmlEscape(config.password) + "\"></label>"
    "<label>Timezone<select name=tz>" + tzOptions + "<option value=''>Other (POSIX string below)</option></select></label>"
    "<label>POSIX timezone string<input name=tz_other placeholder='e.g. GMT0BST,M3.5.0/1,M10.5.0'></label>"
    "<label><input type=checkbox name=h24 style='width:auto' " + String(config.clock24h ? "checked" : "") + "> 24-hour time in footer</label>"
    "<button>Save and restart</button></form>"
    "<p><small>Settings are written to config.txt on the SD card.</small></p></body></html>";
  server->send(200, "text/html", page);
}

static void handleSave() {
  String other = server->arg("ssid_other"); other.trim();
  config.ssid = other.isEmpty() ? server->arg("ssid") : other;
  config.password = server->arg("password");
  String tzOther = server->arg("tz_other"); tzOther.trim();
  String tz = tzOther.isEmpty() ? server->arg("tz") : tzOther;
  if (!tz.isEmpty()) config.timezone = tz;
  config.clock24h = server->hasArg("h24");
  bool ok = saveConfig();
  server->send(200, "text/html",
    String("<!doctype html><html><body style='font-family:Georgia,serif;margin:2em'><h1>") +
    (ok ? "Saved" : "Could not write to the SD card") +
    "</h1><p>The clock will restart and connect to <b>" + htmlEscape(config.ssid) + "</b>.</p></body></html>");
  portalSaved = ok;
}

static void handleNotFound() {
  server->sendHeader("Location", "http://" + portalIp() + "/", true);
  server->send(302, "text/plain", "");
}

PortalResult runPortal(bool (*cancelPoll)()) {
  portalSaved = false;
  scanNetworks();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_NAME);
  delay(200);
  dns = new DNSServer();
  dns->start(53, "*", WiFi.softAPIP());
  server = new WebServer(80);
  server->on("/", HTTP_GET, handleRoot);
  server->on("/save", HTTP_POST, handleSave);
  server->onNotFound(handleNotFound);   // catches captive-portal probes from phones
  server->begin();

  PortalResult result = PORTAL_CANCELLED;
  uint32_t savedAt = 0;
  while (true) {
    dns->processNextRequest();
    server->handleClient();
    if (portalSaved) {
      if (!savedAt) savedAt = millis();
      if (millis() - savedAt > 1500) { result = PORTAL_SAVED; break; }   // let the response flush
    } else if (cancelPoll && cancelPoll()) {
      break;
    }
    delay(5);
  }
  server->stop(); delete server; server = nullptr;
  dns->stop(); delete dns; dns = nullptr;
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  return result;
}
