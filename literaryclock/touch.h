// Bit-banged XPT2046 touch (the hardware VSPI bus belongs to the SD card).
#pragma once
#include <Arduino.h>

void touchBegin();
// Screen coordinates in the landscape orientation used by the clock. True while pressed.
bool touchGet(int16_t& x, int16_t& y);
bool touchGetRaw(uint16_t& xRaw, uint16_t& yRaw, uint16_t& z);
