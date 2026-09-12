#include "ui.h"
#include "display.h"
#include "touch.h"
#include "timesrc.h"
#include "config.h"

struct Btn { int x, y, w, h; const char* label; };

static void drawButtons(const Btn* btns, int n, int pressed = -1) {
  for (int i = 0; i < n; i++) drawButton(btns[i].x, btns[i].y, btns[i].w, btns[i].h, btns[i].label, i == pressed);
}

static int hit(const Btn* btns, int n, int x, int y) {
  for (int i = 0; i < n; i++) {
    if (x >= btns[i].x && x < btns[i].x + btns[i].w && y >= btns[i].y && y < btns[i].y + btns[i].h) return i;
  }
  return -1;
}

void waitForRelease() {
  int16_t x, y;
  uint32_t quiet = millis();
  while (millis() - quiet < 60) {       // released for 60 ms continuously
    if (touchGet(x, y)) quiet = millis();
    delay(5);
  }
}

// Blocks until a button is pressed and released. Returns -1 after timeoutMs of no touch.
static int waitButton(const Btn* btns, int n, void (*redraw)(int pressed), uint32_t timeoutMs = 0) {
  int16_t x, y;
  int down = -1;
  uint32_t last = millis();
  redraw(-1);
  while (true) {
    bool pressed = touchGet(x, y);
    if (pressed) {
      last = millis();
      int h = hit(btns, n, x, y);
      if (h != down) { down = h; redraw(down); }
    } else if (down >= 0) {
      int chosen = down;
      redraw(-1);
      return chosen;
    }
    if (timeoutMs && millis() - last > timeoutMs) return -1;
    delay(10);
  }
}

// ---------------- main menu ----------------
static const Btn MENU_BTNS[] = {
  {  16,  58, 138, 44, "Another quote" }, { 166,  58, 138, 44, "Set time" },
  {  16, 112, 138, 44, "WiFi setup" },    { 166, 112, 138, 44, "12h / 24h" },
  {  16, 166, 138, 44, "Touch test" },    { 166, 166, 138, 44, "Back" },
};

static void drawMenu(int pressed) {
  spr.fillSprite(C_PAPER);
  drawCentered("Literary Clock", 14, &FreeSerifBold12pt7b, C_ACCENT);
  drawButtons(MENU_BTNS, 6, pressed);
  spr.setTextFont(2); spr.setTextDatum(BC_DATUM); spr.setTextColor(C_GREY, C_PAPER);
  spr.drawString(String("Time is ") + (config.clock24h ? "24h" : "12h") + (wifiConnected() ? ", wifi connected" : ", no wifi"), 160, 236);
  spr.pushSprite(0, 0);
}

MenuChoice runMenu() {
  waitForRelease();
  int c = waitButton(MENU_BTNS, 6, drawMenu, 20000);
  return c < 0 ? MENU_BACK : (MenuChoice)c;
}

// ---------------- set time ----------------
static int setH, setM;
static const Btn TIME_BTNS[] = {
  {  40,  40, 80, 44, "+" }, { 200,  40, 80, 44, "+" },
  {  40, 150, 80, 44, "-" }, { 200, 150, 80, 44, "-" },
  {  16, 204, 138, 30, "Cancel" }, { 166, 204, 138, 30, "Set" },
};

static void drawSetTime(int pressed) {
  spr.fillSprite(C_PAPER);
  drawCentered("Set the time", 8, &FreeSerif9pt7b, C_GREY);
  char hh[4], mm[4];
  snprintf(hh, sizeof(hh), "%02d", setH);
  snprintf(mm, sizeof(mm), "%02d", setM);
  spr.setFreeFont(&FreeSerifBold24pt7b); spr.setTextDatum(TC_DATUM); spr.setTextColor(C_INK, C_PAPER);
  spr.drawString(hh, 80, 100);
  spr.drawString(":", 160, 100);
  spr.drawString(mm, 240, 100);
  drawButtons(TIME_BTNS, 6, pressed);
  spr.pushSprite(0, 0);
}

bool runSetTime() {
  time_t now = time(nullptr);
  struct tm t; localtime_r(&now, &t);
  setH = timeIsValid() ? t.tm_hour : 12;
  setM = timeIsValid() ? t.tm_min : 0;
  waitForRelease();
  while (true) {
    int c = waitButton(TIME_BTNS, 6, drawSetTime, 60000);
    if (c < 0 || c == 4) return false;
    if (c == 5) { setLocalHourMinute(setH, setM); return true; }
    if (c == 0) setH = (setH + 1) % 24;
    if (c == 1) setM = (setM + 1) % 60;
    if (c == 2) setH = (setH + 23) % 24;
    if (c == 3) setM = (setM + 59) % 60;
  }
}

// ---------------- touch test ----------------
void runTouchTest() {
  waitForRelease();
  uint32_t last = millis();
  static const Btn done = { 220, 200, 90, 32, "Done" };
  while (millis() - last < 30000) {
    uint16_t xr, yr, z; int16_t x = -1, y = -1;
    bool pressed = touchGetRaw(xr, yr, z);
    if (pressed) { touchGet(x, y); last = millis(); }
    spr.fillSprite(C_PAPER);
    drawCentered("Touch test", 8, &FreeSerif9pt7b, C_GREY);
    spr.setTextFont(2); spr.setTextDatum(TL_DATUM); spr.setTextColor(C_INK, C_PAPER);
    if (pressed) {
      spr.drawString("raw x " + String(xr) + "   raw y " + String(yr) + "   z " + String(z), 16, 40);
      spr.drawString("screen " + String(x) + ", " + String(y), 16, 60);
      spr.drawFastHLine(0, y, 320, C_ACCENT);
      spr.drawFastVLine(x, 0, 240, C_ACCENT);
    } else {
      spr.drawString("Touch the corners; the lines should meet under your finger.", 16, 40);
      spr.drawString("Adjust touch_* keys in config.txt if they do not.", 16, 60);
    }
    drawButton(done.x, done.y, done.w, done.h, done.label);
    spr.pushSprite(0, 0);
    if (pressed && hit(&done, 1, x, y) >= 0) { waitForRelease(); return; }
    delay(30);
  }
}

// ---------------- no time / no wifi prompt ----------------
static String promptReason;
static const Btn NOTIME_BTNS[] = {
  { 16, 150, 138, 44, "WiFi setup" }, { 166, 150, 138, 44, "Set time" }, { 90, 204, 140, 30, "Retry WiFi" },
};

static void drawNoTime(int pressed) {
  spr.fillSprite(C_PAPER);
  drawCentered("Literary Clock", 20, &FreeSerifBold18pt7b, C_ACCENT);
  drawCentered(promptReason, 70, &FreeSerif9pt7b, C_INK);
  drawCentered("Set up WiFi to fetch the time automatically,", 100, &FreeSerif9pt7b, C_GREY);
  drawCentered("or set it by hand for now.", 120, &FreeSerif9pt7b, C_GREY);
  drawButtons(NOTIME_BTNS, 3, pressed);
  spr.pushSprite(0, 0);
}

NoTimeChoice runNoTimePrompt(const String& reason) {
  promptReason = reason;
  waitForRelease();
  int c = waitButton(NOTIME_BTNS, 3, drawNoTime);
  return (NoTimeChoice)c;
}

void showPortalScreen() {
  spr.fillSprite(C_PAPER);
  drawCentered("WiFi setup", 16, &FreeSerifBold18pt7b, C_ACCENT);
  drawCentered("On your phone, join the WiFi network", 70, &FreeSerif9pt7b, C_INK);
  drawCentered(portalApName(), 94, &FreeSerifBold12pt7b, C_INK);
  drawCentered("A setup page should open by itself.", 130, &FreeSerif9pt7b, C_GREY);
  drawCentered("If not, browse to http://" + portalIp(), 150, &FreeSerif9pt7b, C_GREY);
  drawCentered("Touch the screen to cancel.", 200, &FreeSerif9pt7b, C_GREY);
  spr.pushSprite(0, 0);
}
