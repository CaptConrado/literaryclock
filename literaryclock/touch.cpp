#include "touch.h"
#include "config.h"
#include "display.h"
#include <XPT2046_Bitbang.h>

static const int T_MOSI = 32, T_MISO = 39, T_CLK = 25, T_CS = 33;
static XPT2046_Bitbang panel(T_MOSI, T_MISO, T_CLK, T_CS);

void touchBegin() {
  panel.begin();
}

bool touchGetRaw(uint16_t& xRaw, uint16_t& yRaw, uint16_t& z) {
  TouchPoint p = panel.getTouch();
  digitalWrite(T_CS, HIGH);                        // the library leaves CS low on a no-pressure return
  if (p.zRaw == 0) return false;
  xRaw = p.xRaw; yRaw = p.yRaw; z = p.zRaw;
  return true;
}

bool touchGet(int16_t& x, int16_t& y) {
  uint16_t xr, yr, z;
  if (!touchGetRaw(xr, yr, z)) return false;
  // Command 0x91 (the library's "x") maps to screen X in landscape, matching the
  // XPT2046_Touchscreen calibration used on this board. touch_swap_xy flips it if not.
  if (config.touchSwapXY) { uint16_t t = xr; xr = yr; yr = t; }
  long sx = map((long)xr, config.touchXMin, config.touchXMax, 0, 319);
  long sy = map((long)yr, config.touchYMin, config.touchYMax, 0, 239);
  if (config.touchInvertX) sx = 319 - sx;
  if (config.touchInvertY) sy = 239 - sy;
  int xs, ys;
  landscapeToScreen(constrain(sx, 0, 319), constrain(sy, 0, 239), xs, ys);
  x = xs; y = ys;
  return true;
}

// ---- diagnostics: our own bit-bang read, returning the unshifted 16-bit words ----
static void dbgWrite(uint8_t cmd) {
  for (int i = 7; i >= 0; i--) {
    digitalWrite(T_MOSI, (cmd >> i) & 1);
    digitalWrite(T_CLK, LOW);  delayMicroseconds(5);
    digitalWrite(T_CLK, HIGH); delayMicroseconds(5);
  }
  digitalWrite(T_MOSI, LOW);
  digitalWrite(T_CLK, LOW);
}
static uint16_t dbgRead16() {
  uint16_t r = 0;
  for (int i = 15; i >= 0; i--) {
    digitalWrite(T_CLK, HIGH); delayMicroseconds(5);
    digitalWrite(T_CLK, LOW);  delayMicroseconds(5);
    r |= (uint16_t)digitalRead(T_MISO) << i;
  }
  return r;
}
void touchDebugRead(uint16_t raw[4], int& irqLevel) {
  static const uint8_t cmds[4] = { 0xB1, 0xC1, 0x91, 0xD0 };
  pinMode(36, INPUT);
  irqLevel = digitalRead(36);
  digitalWrite(T_CS, LOW);
  for (int i = 0; i < 4; i++) { dbgWrite(cmds[i]); raw[i] = dbgRead16(); }
  digitalWrite(T_CS, HIGH);
}
