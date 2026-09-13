// Literary Clock for the Xteink X4 e-paper reader (ESP32-C3, 800x480 SSD1677).
// Shares config.txt / quotes.txt / WiFi / NTP code with the CYD build (../../common).
//
// Buttons:  Confirm = another quote for this minute   Left / Right = rotate the display
//           Back    = full refresh (clears ghosting)  Power held 3 s = WiFi setup portal
//
// v1 stays awake and expects USB power; battery deep-sleep is a later step (see ../README.md).

#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <config.h>
#include <storage.h>
#include <timesrc.h>
#include "board.h"
#include "buttons.h"
#include "epd_display.h"

static const uint32_t WIFI_TIMEOUT_MS  = 20000;
static const uint32_t NTP_WAIT_MS      = 12000;
static const int      FULL_REFRESH_EVERY = 10;   // partial updates between full (flashing) refreshes

static int  lastMinute = -1;
static int  quoteIndex = 0;
static int  partialCount = 0;
static bool forceRedraw = true, forceFull = true;
static std::vector<Quote> current;
static uint32_t lastWifiCheck = 0;

static void progressIndex(int pct) { if (pct % 25 == 0) epdShowStatus("Literary Clock", "Indexing quotes...", String(pct) + "%"); }
static void progressWifi(int sec)  { if (sec == 0) epdShowStatus("Literary Clock", "Connecting to " + config.ssid, "up to 20 seconds"); }
static bool portalCancelPoll()     { return buttonsRead() == BTN_BACK; }

static void render() {
  time_t now = time(nullptr);
  struct tm t; localtime_r(&now, &t);
  int minute = t.tm_hour * 60 + t.tm_min;
  if (minute != lastMinute) {
    lastMinute = minute;
    current = quotesForMinute(minute, config.sfwOnly);
    int pool = 0;
    while (pool < (int)current.size() && current[pool].personal) pool++;
    if (pool == 0) pool = current.size();
    quoteIndex = pool > 0 ? esp_random() % pool : 0;
    Serial.printf("%02d:%02d -> %d quotes, showing #%d\n", t.tm_hour, t.tm_min, (int)current.size(), quoteIndex);
  }
  bool full = forceFull || partialCount >= FULL_REFRESH_EVERY;
  if (current.empty()) epdShowNoQuote(t, wifiConnected(), full);
  else                 epdShowQuote(current[quoteIndex], t, wifiConnected(), full);
  partialCount = full ? 0 : partialCount + 1;
  forceRedraw = forceFull = false;
}

static void startWifiAndNtp() {
  if (config.ssid.isEmpty()) return;
  if (wifiConnect(WIFI_TIMEOUT_MS, progressWifi)) {
    ntpBegin();
    epdShowStatus("Literary Clock", "Fetching the time...", WiFi.localIP().toString());
    uint32_t start = millis();
    while (!timeIsValid() && millis() - start < NTP_WAIT_MS) delay(100);
  } else {
    WiFi.disconnect(true);
  }
}

static void wifiSetupFlow() {
  epdShowStatus("WiFi setup", String("Join the network ") + portalApName(), "then open http://192.168.4.1", "Back button cancels");
  PortalResult r = runPortal(portalCancelPoll);
  if (r == PORTAL_SAVED) { epdShowStatus("Literary Clock", "Saved. Restarting..."); delay(1000); ESP.restart(); }
  forceRedraw = forceFull = true;
}

void setup() {
  pinMode(POWER_LATCH, OUTPUT);
  digitalWrite(POWER_LATCH, HIGH);           // keep the battery MOSFET on before anything else
  Serial.begin(115200);
  buttonsBegin();
  epdBegin();
  epdShowStatus("Literary Clock", "Starting...");

  while (!storageBegin(SPI, SD_CS)) {        // SD shares the panel's SPI bus
    epdShowStatus("Literary Clock", "SD card problem", storageError(), "Retrying...");
    delay(3000);
  }
  if (!loadConfig()) saveConfig();
  applyTimezone();
  epdSetOrientation(config.rotation);

  while (!ensureIndex(progressIndex)) {
    epdShowStatus("Literary Clock", "quotes.txt not found", "Copy the sdcard folder onto the card", "and reinsert it.");
    delay(3000);
    SD.end();
    storageBegin(SPI, SD_CS);
  }

  startWifiAndNtp();
  while (!timeIsValid()) {
    epdShowStatus("Literary Clock",
                  config.ssid.isEmpty() ? "No WiFi network configured" : "Could not reach " + config.ssid,
                  "Hold the power button 3 s for WiFi setup,", "or edit config.txt on the card.");
    uint32_t t0 = millis();
    while (millis() - t0 < 30000) {
      buttonsEvent();
      if (powerHeldFor(3000)) { wifiSetupFlow(); break; }
      delay(20);
    }
    if (!timeIsValid()) startWifiAndNtp();
  }
  forceRedraw = forceFull = true;
}

void loop() {
  Button b = buttonsEvent();
  switch (b) {
    case BTN_CONFIRM: if (current.size() > 1) { quoteIndex = (quoteIndex + 1) % current.size(); forceRedraw = true; } break;
    case BTN_LEFT:    config.rotation = (config.rotation + 1) & 3; epdSetOrientation(config.rotation); saveConfig(); forceRedraw = forceFull = true; break;
    case BTN_RIGHT:   config.rotation = (config.rotation + 3) & 3; epdSetOrientation(config.rotation); saveConfig(); forceRedraw = forceFull = true; break;
    case BTN_BACK:    forceRedraw = forceFull = true; break;
    default: break;
  }
  if (powerHeldFor(3000)) { wifiSetupFlow(); while (buttonsRead() == BTN_PWR) delay(20); }

  static uint32_t lastTick = 0;
  if (forceRedraw || millis() - lastTick > 500) {
    lastTick = millis();
    time_t now = time(nullptr);
    struct tm t; localtime_r(&now, &t);
    if (forceRedraw || t.tm_hour * 60 + t.tm_min != lastMinute) render();
  }

  if (!config.ssid.isEmpty() && millis() - lastWifiCheck > 60000) {
    lastWifiCheck = millis();
    if (!wifiConnected()) { WiFi.disconnect(); WiFi.begin(config.ssid.c_str(), config.password.c_str()); }
  }
  delay(20);
}
