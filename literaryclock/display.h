// Rendering: a 4-bit palette sprite covering the whole 320x240 panel.
#pragma once
#include <Arduino.h>
#include <TFT_eSPI.h>
#include <time.h>
#include <storage.h>

extern TFT_eSPI    tft;
extern TFT_eSprite spr;
extern int         SCR_W, SCR_H;      // current screen size (320x240 landscape, 240x320 portrait)

// Palette indices (the sprite is 4-bit, colours are indexes into this palette).
enum Colour : uint8_t {
  C_PAPER = 0, C_INK, C_ACCENT, C_GREY, C_RULE, C_WHITE, C_BLACK, C_BUTTON, C_BUTTON_DOWN, C_GOOD, C_BAD,
};

void   displayBegin();
void   displaySetRotation(uint8_t rotation);   // re-creates the sprite for the new size
void   setBacklight(uint8_t level);
String formatTime(const struct tm& now);

void showStatus(const String& title, const String& l1, const String& l2 = "", const String& l3 = "");
void showQuote(const Quote& q, const struct tm& now, bool wifiOk, int index, int count);
void showNoQuote(const struct tm& now, bool wifiOk);

// Helpers shared with ui.cpp
void drawFooterStatus(const struct tm& now, bool wifiOk, const String& right);
void drawButton(int x, int y, int w, int h, const String& label, bool pressed = false);
void drawCentered(const String& text, int y, const GFXfont* font, uint8_t colour);

