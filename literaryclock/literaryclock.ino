// Literary Clock for the Cheap Yellow Display (ESP32-2432S028).
// Shows a passage of literature that mentions the current minute, with the
// time phrase in bold, and the book and author underneath.
//
// Data lives on the SD card in /literaryclock (see README.md). Time comes from
// WiFi + NTP when configured, otherwise it is set by touch.
//
// Touch: tap = another quote for this minute, long press = menu.

#include <WiFi.h>
#include <SD.h>
#include <time.h>
#include "config.h"
#include "storage.h"
#include "touch.h"
#include "timesrc.h"
#include "display.h"
#include "ui.h"

static const uint32_t WIFI_TIMEOUT_MS = 20000;
static const uint32_t NTP_WAIT_MS     = 12000;
static const uint32_t LONG_PRESS_MS   = 900;
static const uint32_t RELEASE_MS      = 120;   // finger must be absent this long to count as released

static int  lastMinute = -1;
static int  quoteOffset = 0;         // advanced by taps, reset each minute
static bool forceRedraw = true;
static std::vector<Quote> current;
static uint32_t lastWifiCheck = 0;

static void progressIndex(int pct) {
  showStatus("Literary Clock", "Indexing quotes...", String(pct) + "%");
}
static void progressWifi(int sec) {
  showStatus("Literary Clock", "Connecting to " + config.ssid, String(WIFI_TIMEOUT_MS / 1000 - sec) + " s");
}
static bool portalCancelPoll() {
  int16_t x, y;
  return touchGet(x, y);
}

static void applyBacklight(const struct tm& now) {
  uint8_t level = config.brightness;
  if (config.nightStart >= 0 && config.nightEnd >= 0) {
    int m = now.tm_hour * 60 + now.tm_min;
    bool night = config.nightStart <= config.nightEnd
                   ? (m >= config.nightStart && m < config.nightEnd)
                   : (m >= config.nightStart || m < config.nightEnd);
    if (night) level = config.nightBrightness;
  }
  setBacklight(level);
}

static void render() {
  time_t now = time(nullptr);
  struct tm t; localtime_r(&now, &t);
  int minute = t.tm_hour * 60 + t.tm_min;
  if (minute != lastMinute) {
    lastMinute = minute;
    quoteOffset = 0;
    current = quotesForMinute(minute, config.sfwOnly);
    Serial.printf("%02d:%02d -> %d quotes\n", t.tm_hour, t.tm_min, (int)current.size());
  }
  applyBacklight(t);
  if (current.empty()) {
    showNoQuote(t, wifiConnected());
  } else {
    int idx = quoteOffset % current.size();   // best-ranked quote first; taps step through alternates
    showQuote(current[idx], t, wifiConnected(), idx, current.size());
  }
  forceRedraw = false;
}

static void startWifiAndNtp() {
  if (config.ssid.isEmpty()) return;
  if (wifiConnect(WIFI_TIMEOUT_MS, progressWifi)) {
    ntpBegin();
    showStatus("Literary Clock", "Fetching the time...", WiFi.localIP().toString());
    uint32_t start = millis();
    while (!timeIsValid() && millis() - start < NTP_WAIT_MS) delay(100);
  } else {
    WiFi.disconnect(true);
  }
}

static void wifiSetupFlow() {
  showPortalScreen();
  waitForRelease();
  PortalResult r = runPortal(portalCancelPoll);
  if (r == PORTAL_SAVED) {
    showStatus("Literary Clock", "Saved. Restarting...");
    delay(1000);
    ESP.restart();
  }
  waitForRelease();
  forceRedraw = true;
}

static void ensureTimeInteractive() {
  while (!timeIsValid()) {
    String reason = config.ssid.isEmpty() ? "No WiFi network is configured."
                                          : "Could not reach " + config.ssid + ".";
    NoTimeChoice c = runNoTimePrompt(reason);
    if (c == NOTIME_WIFI_SETUP) wifiSetupFlow();
    else if (c == NOTIME_SET_TIME) { if (runSetTime()) break; }
    else startWifiAndNtp();
  }
}

void setup() {
  Serial.begin(115200);
  displayBegin();                    // sprite is allocated here, before WiFi claims heap
  showStatus("Literary Clock", "Starting...");
  touchBegin();

  while (!storageBegin()) {
    showStatus("Literary Clock", "SD card problem", storageError(), "Retrying...");
    delay(3000);
  }
  if (!loadConfig()) {
    Serial.println("No config.txt, writing defaults");
    saveConfig();
  }
  applyTimezone();

  while (!ensureIndex(progressIndex)) {
    showStatus("Literary Clock", "quotes.txt not found", "Copy the sdcard folder from the repo", "onto the card, then reinsert.");
    delay(3000);
    SD.end();
    storageBegin();
  }

  if (!config.touchCalibrated) runCalibration();

  startWifiAndNtp();
  ensureTimeInteractive();
  forceRedraw = true;
}

void loop() {
  // ---- touch: tap = next quote, long press = menu (debounced: resistive panels flicker) ----
  static bool     pressed     = false;   // debounced state
  static uint32_t pressedAt   = 0;
  static uint32_t lastSeenRaw = 0;
  static bool     longFired   = false;
  int16_t x, y;
  bool rawDown = touchGet(x, y);
  uint32_t now = millis();
  if (rawDown) lastSeenRaw = now;
  if (rawDown && !pressed) { pressed = true; pressedAt = now; longFired = false; }
  if (pressed && !rawDown && now - lastSeenRaw > RELEASE_MS) {
    pressed = false;
    if (!longFired) { quoteOffset++; forceRedraw = true; }          // short tap
  }
  if (pressed && !longFired && now - pressedAt > LONG_PRESS_MS) {
    longFired = true;
    MenuChoice c = runMenu();
    switch (c) {
      case MENU_NEXT_QUOTE: quoteOffset++; break;
      case MENU_SET_TIME:   runSetTime(); lastMinute = -1; break;
      case MENU_WIFI_SETUP: wifiSetupFlow(); break;
      case MENU_TOGGLE_24H: config.clock24h = !config.clock24h; saveConfig(); break;
      case MENU_CALIBRATE:  runCalibration(); break;
      case MENU_BACK: break;
    }
    waitForRelease();
    pressed = false;
    forceRedraw = true;
  }

  // ---- clock tick ----
  static uint32_t lastTick = 0;
  if (forceRedraw || millis() - lastTick > 250) {
    lastTick = millis();
    time_t now = time(nullptr);
    struct tm t; localtime_r(&now, &t);
    int minute = t.tm_hour * 60 + t.tm_min;
    if (forceRedraw || minute != lastMinute) render();
  }

  // ---- keep WiFi alive (NTP re-syncs itself hourly while connected) ----
  if (!config.ssid.isEmpty() && millis() - lastWifiCheck > 60000) {
    lastWifiCheck = millis();
    if (!wifiConnected()) {
      Serial.println("WiFi lost, reconnecting");
      WiFi.disconnect();
      WiFi.begin(config.ssid.c_str(), config.password.c_str());
    }
  }
  delay(10);
}
