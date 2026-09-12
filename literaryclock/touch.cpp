#include "touch.h"
#include "config.h"
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
  x = constrain(sx, 0, 319);
  y = constrain(sy, 0, 239);
  return true;
}
