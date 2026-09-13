#include "buttons.h"
#include "board.h"

struct Level { int value; Button button; };
static const Level LADDER_A[] = { { 3470, BTN_BACK }, { 2655, BTN_CONFIRM }, { 1470, BTN_LEFT }, { 3, BTN_RIGHT } };
static const Level LADDER_B[] = { { 2205, BTN_UP }, { 3, BTN_DOWN } };
static const int TOLERANCE = 300;

void buttonsBegin() {
  pinMode(BTN_POWER, INPUT_PULLUP);
  analogReadResolution(12);
}

static Button match(int adc, const Level* levels, int n) {
  if (adc > 3900) return BTN_NONE;                       // idle: pulled to the rail
  for (int i = 0; i < n; i++) if (abs(adc - levels[i].value) < TOLERANCE) return levels[i].button;
  return BTN_NONE;
}

Button buttonsRead() {
  if (digitalRead(BTN_POWER) == LOW) return BTN_PWR;
  Button b = match(analogRead(ADC_BTN_A), LADDER_A, 4);
  if (b != BTN_NONE) return b;
  return match(analogRead(ADC_BTN_B), LADDER_B, 2);
}

static Button   lastStable = BTN_NONE, candidate = BTN_NONE;
static uint32_t candidateSince = 0, powerDownAt = 0;

Button buttonsEvent() {
  Button now = buttonsRead();
  uint32_t t = millis();
  if (now != candidate) { candidate = now; candidateSince = t; }
  if (t - candidateSince < 40) return BTN_NONE;           // debounce
  if (candidate == lastStable) return BTN_NONE;
  lastStable = candidate;
  if (candidate == BTN_PWR) powerDownAt = t;
  if (candidate == BTN_NONE) powerDownAt = 0;
  return candidate;                                        // press (or BTN_NONE on release)
}

bool powerHeldFor(uint32_t ms) {
  return lastStable == BTN_PWR && powerDownAt && millis() - powerDownAt >= ms;
}

const char* buttonName(Button b) {
  static const char* names[] = { "none", "back", "confirm", "left", "right", "up", "down", "power" };
  return names[b];
}
