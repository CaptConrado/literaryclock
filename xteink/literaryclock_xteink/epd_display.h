// E-paper rendering for the Xteink X4: same layout rules as the CYD, drawn 1-bit on 800x480.
#pragma once
#include <Arduino.h>
#include <time.h>
#include <storage.h>

void epdBegin();
void epdSetOrientation(uint8_t tftRotation);   // takes the config.txt rotation code (1 landscape, 0 portrait, ...)
int  epdWidth();
int  epdHeight();

void epdShowStatus(const String& title, const String& l1, const String& l2 = "", const String& l3 = "");
// Draws a quote. fullRefresh clears ghosting (slow, flashes); otherwise a fast partial update.
void epdShowQuote(const Quote& q, const struct tm& now, bool wifiOk, bool fullRefresh);
void epdShowNoQuote(const struct tm& now, bool wifiOk, bool fullRefresh);
void epdSleep();                                // hibernate the panel (image is retained)
