#pragma once
#include <Arduino.h>

enum Button { BTN_NONE, BTN_BACK, BTN_CONFIRM, BTN_LEFT, BTN_RIGHT, BTN_UP, BTN_DOWN, BTN_PWR };

void   buttonsBegin();
Button buttonsRead();                 // raw, current state
Button buttonsEvent();                // debounced press event (fires once on press), BTN_NONE otherwise
bool   powerHeldFor(uint32_t ms);     // true while the power button has been held at least ms
const char* buttonName(Button b);
