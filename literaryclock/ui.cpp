#include "ui.h"
#include "display.h"
#include "touch.h"
#include <timesrc.h>
#include <config.h>

// All screens lay themselves out from SCR_W / SCR_H so they work in portrait and landscape.
struct Btn { int x, y, w, h; String label; };

// Fill `out` with a grid of buttons between yTop and yBottom.
static void gridButtons(Btn* out, const char* const* labels, int n, int cols, int yTop, int yBottom) {
  int rows = (n + cols - 1) / cols;
  int gap = 10, mx = 16;
  int w = (SCR_W - 2 * mx - (cols - 1) * gap) / cols;
  int h = (yBottom - yTop - (rows - 1) * gap) / rows;
  if (h > 48) h = 48;
  for (int i = 0; i < n; i++) {
    out[i] = { mx + (i % cols) * (w + gap), yTop + (i / cols) * (h + gap), w, h, labels[i] };
  }
}

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
static const char* const MENU_LABELS[] = { "Another quote", "Rotate", "Set time", "12h / 24h",
                                           "WiFi setup", "Calibrate", "Brightness", "Back" };
static Btn menuBtns[8];

static void drawMenu(int pressed) {
  spr.fillSprite(C_PAPER);
  drawCentered("Literary Clock", 12, &FreeSerifBold12pt7b, C_ACCENT);
  gridButtons(menuBtns, MENU_LABELS, 8, 2, 50, SCR_H - 26);
  drawButtons(menuBtns, 8, pressed);
  spr.setTextFont(2); spr.setTextDatum(BC_DATUM); spr.setTextColor(C_GREY);
  spr.drawString(String(config.clock24h ? "24h" : "12h") + (wifiConnected() ? ", wifi connected" : ", no wifi") +
                 ", brightness " + String(config.brightness * 100 / 255) + "%", SCR_W / 2, SCR_H - 4);
  spr.pushSprite(0, 0);
}

MenuChoice runMenu() {
  waitForRelease();
  gridButtons(menuBtns, MENU_LABELS, 8, 2, 50, SCR_H - 26);
  int c = waitButton(menuBtns, 8, drawMenu, 20000);
  return c < 0 ? MENU_BACK : (MenuChoice)c;
}

// ---------------- set time ----------------
static int setH, setM;
static Btn timeBtns[6];

static void layoutTimeButtons() {
  int cw = SCR_W / 2, bw = 80, bh = 44;
  int cy = SCR_H / 2 - 20;                              // vertical centre of the digits
  timeBtns[0] = { cw / 2 - bw / 2,       cy - 90, bw, bh, "+" };
  timeBtns[1] = { cw + cw / 2 - bw / 2,  cy - 90, bw, bh, "+" };
  timeBtns[2] = { cw / 2 - bw / 2,       cy + 46, bw, bh, "-" };
  timeBtns[3] = { cw + cw / 2 - bw / 2,  cy + 46, bw, bh, "-" };
  int bw2 = (SCR_W - 16 * 2 - 10) / 2;
  timeBtns[4] = { 16,            SCR_H - 40, bw2, 32, "Cancel" };
  timeBtns[5] = { 16 + bw2 + 10, SCR_H - 40, bw2, 32, "Set" };
}

static void drawSetTime(int pressed) {
  spr.fillSprite(C_PAPER);
  drawCentered("Set the time", 8, &FreeSerif9pt7b, C_GREY);
  char hh[4], mm[4];
  snprintf(hh, sizeof(hh), "%02d", setH);
  snprintf(mm, sizeof(mm), "%02d", setM);
  int cy = SCR_H / 2 - 20;
  spr.setFreeFont(&FreeSerifBold24pt7b); spr.setTextDatum(MC_DATUM); spr.setTextColor(C_INK);
  spr.drawString(hh, SCR_W / 4, cy);
  spr.drawString(":", SCR_W / 2, cy);
  spr.drawString(mm, 3 * SCR_W / 4, cy);
  drawButtons(timeBtns, 6, pressed);
  spr.pushSprite(0, 0);
}

bool runSetTime() {
  time_t now = time(nullptr);
  struct tm t; localtime_r(&now, &t);
  setH = timeIsValid() ? t.tm_hour : 12;
  setM = timeIsValid() ? t.tm_min : 0;
  layoutTimeButtons();
  waitForRelease();
  while (true) {
    int c = waitButton(timeBtns, 6, drawSetTime, 60000);
    if (c < 0 || c == 4) return false;
    if (c == 5) { setLocalHourMinute(setH, setM); return true; }
    if (c == 0) setH = (setH + 1) % 24;
    if (c == 1) setM = (setM + 1) % 60;
    if (c == 2) setH = (setH + 23) % 24;
    if (c == 3) setM = (setM + 59) % 60;
  }
}

// ---------------- touch test (diagnostics) ----------------
void runTouchTest() {
  waitForRelease();
  uint32_t last = millis();
  Btn done = { SCR_W - 100, SCR_H - 40, 90, 32, "Done" };
  while (millis() - last < 45000) {
    uint16_t raw[4]; int irq;
    touchDebugRead(raw, irq);
    uint16_t xr = 0, yr = 0, z = 0; int16_t x = -1, y = -1;
    bool pressed = touchGetRaw(xr, yr, z);
    if (pressed) { touchGet(x, y); last = millis(); }
    spr.fillSprite(C_PAPER);
    drawCentered("Touch test", 8, &FreeSerif9pt7b, C_GREY);
    spr.setTextFont(2); spr.setTextDatum(TL_DATUM); spr.setTextColor(C_INK);
    char buf[80];
    snprintf(buf, sizeof(buf), "IRQ pin 36: %s", irq ? "HIGH (idle)" : "LOW (touch!)");
    spr.drawString(buf, 16, 36);
    snprintf(buf, sizeof(buf), "Z1 %04X Z2 %04X X %04X Y %04X", raw[0], raw[1], raw[2], raw[3]);
    spr.drawString(buf, 16, 56);
    snprintf(buf, sizeof(buf), "x %u y %u z %u %s", xr, yr, z, pressed ? "PRESSED" : "");
    spr.drawString(buf, 16, 76);
    if (pressed) {
      snprintf(buf, sizeof(buf), "screen %d, %d", x, y);
      spr.drawString(buf, 16, 96);
      spr.drawFastHLine(0, y, SCR_W, C_ACCENT);
      spr.drawFastVLine(x, 0, SCR_H, C_ACCENT);
    }
    drawButton(done.x, done.y, done.w, done.h, done.label);
    spr.pushSprite(0, 0);
    if (pressed && hit(&done, 1, x, y) >= 0) { waitForRelease(); return; }
    delay(100);
  }
}

// ---------------- touch calibration ----------------
// Runs in the current orientation and stores a calibration for this rotation only.
static bool sampleRaw(int tx, int ty, uint32_t& rx, uint32_t& ry) {
  waitForRelease();
  uint32_t start = millis();
  while (true) {
    spr.fillSprite(C_PAPER);
    drawCentered("Touch calibration", 8, &FreeSerif9pt7b, C_GREY);
    drawCentered("Press and hold the centre", SCR_H / 2 - 20, &FreeSerif9pt7b, C_INK);
    drawCentered("of the target", SCR_H / 2 + 2, &FreeSerif9pt7b, C_INK);
    spr.drawCircle(tx, ty, 10, C_ACCENT); spr.drawCircle(tx, ty, 3, C_ACCENT);
    spr.drawFastHLine(tx - 16, ty, 33, C_ACCENT); spr.drawFastVLine(tx, ty - 16, 33, C_ACCENT);
    spr.pushSprite(0, 0);
    uint16_t xr, yr, z;
    if (touchGetRaw(xr, yr, z)) {
      delay(120);                                  // let the press settle
      uint32_t sx = 0, sy = 0; int n = 0;
      for (int i = 0; i < 16; i++) {
        if (touchGetRaw(xr, yr, z)) { sx += xr; sy += yr; n++; }
        delay(15);
      }
      if (n >= 10) { rx = sx / n; ry = sy / n; return true; }
    }
    if (millis() - start > 60000) return false;
    delay(40);
  }
}

void runCalibration() {
  const int M = 28;                                // target inset from the edges
  const int Wd = SCR_W, Hd = SCR_H;
  uint32_t ax, ay, bx, by, cx, cy;
  bool ok = sampleRaw(M, M, ax, ay) && sampleRaw(Wd - M, M, bx, by) && sampleRaw(M, Hd - M, cx, cy);
  if (!ok) return;
  // Moving along screen X changed one raw axis much more than the other: that axis is X.
  long dxX = labs((long)bx - (long)ax), dxY = labs((long)by - (long)ay);
  bool swap = dxY > dxX;
  long x0 = swap ? ay : ax, x1 = swap ? by : bx;   // raw X at screen M and Wd-M
  long y0 = swap ? ax : ay, y1 = swap ? cx : cy;   // raw Y at screen M and Hd-M
  float sx = (float)(x1 - x0) / (Wd - 2 * M);
  float sy = (float)(y1 - y0) / (Hd - 2 * M);
  Config::TouchCal& c = config.touch[config.rotation & 3];
  c.valid  = true;
  c.swapXY = swap;
  c.xMin   = constrain((long)(x0 - M * sx), 0, 4095);
  c.xMax   = constrain((long)(x0 + (Wd - 1 - M) * sx), 0, 4095);
  c.yMin   = constrain((long)(y0 - M * sy), 0, 4095);
  c.yMax   = constrain((long)(y0 + (Hd - 1 - M) * sy), 0, 4095);
  bool saved = saveConfig();
  Serial.printf("calibration rot %d: swap=%d x %d..%d y %d..%d saved=%d\n", config.rotation, swap, c.xMin, c.xMax, c.yMin, c.yMax, saved);

  waitForRelease();
  uint32_t last = millis();
  while (millis() - last < 6000) {
    int16_t x, y;
    bool pressed = touchGet(x, y);
    if (pressed) last = millis();
    spr.fillSprite(C_PAPER);
    drawCentered("Saved. Touch anywhere to", SCR_H / 2 - 24, &FreeSerif9pt7b, C_INK);
    drawCentered("check the crosshair.", SCR_H / 2 - 2, &FreeSerif9pt7b, C_INK);
    drawCentered(saved ? "Written to config.txt" : "Could not write config.txt!", SCR_H / 2 + 30, &FreeSerif9pt7b, saved ? C_GREY : C_BAD);
    if (pressed) { spr.drawFastHLine(0, y, SCR_W, C_ACCENT); spr.drawFastVLine(x, 0, SCR_H, C_ACCENT); }
    spr.pushSprite(0, 0);
    delay(30);
  }
}

// ---------------- no time / no wifi prompt ----------------
static String promptReason;
static const char* const NOTIME_LABELS[] = { "WiFi setup", "Set time", "Retry WiFi" };
static Btn notimeBtns[3];

static void drawNoTime(int pressed) {
  spr.fillSprite(C_PAPER);
  int y0 = SCR_H / 2 - 100;
  drawCentered("Literary Clock", y0, &FreeSerifBold18pt7b, C_ACCENT);
  drawCentered(promptReason, y0 + 50, &FreeSerif9pt7b, C_INK);
  drawCentered("Set up WiFi to fetch the time,", y0 + 80, &FreeSerif9pt7b, C_GREY);
  drawCentered("or set it by hand for now.", y0 + 100, &FreeSerif9pt7b, C_GREY);
  drawButtons(notimeBtns, 3, pressed);
  spr.pushSprite(0, 0);
}

NoTimeChoice runNoTimePrompt(const String& reason) {
  promptReason = reason;
  gridButtons(notimeBtns, NOTIME_LABELS, 2, 2, SCR_H / 2 + 30, SCR_H / 2 + 74);
  int bw = 140;
  notimeBtns[2] = { SCR_W / 2 - bw / 2, SCR_H - 40, bw, 30, "Retry WiFi" };
  waitForRelease();
  int c = waitButton(notimeBtns, 3, drawNoTime);
  return (NoTimeChoice)c;
}

void showPortalScreen() {
  spr.fillSprite(C_PAPER);
  int y0 = SCR_H / 2 - 100;
  drawCentered("WiFi setup", y0, &FreeSerifBold18pt7b, C_ACCENT);
  drawCentered("On your phone, join the WiFi", y0 + 54, &FreeSerif9pt7b, C_INK);
  drawCentered("network named", y0 + 74, &FreeSerif9pt7b, C_INK);
  drawCentered(portalApName(), y0 + 98, &FreeSerifBold12pt7b, C_INK);
  drawCentered("A setup page should open.", y0 + 134, &FreeSerif9pt7b, C_GREY);
  drawCentered("If not: http://" + portalIp(), y0 + 154, &FreeSerif9pt7b, C_GREY);
  drawCentered("Touch the screen to cancel.", SCR_H - 30, &FreeSerif9pt7b, C_GREY);
  spr.pushSprite(0, 0);
}
