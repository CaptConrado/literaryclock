#include "display.h"
#include <config.h>
#include <vector>

#ifndef TFT_BL
#define TFT_BL 21
#endif

TFT_eSPI    tft;
TFT_eSprite spr(&tft);

int SCR_W = 320, SCR_H = 240;
#define W SCR_W
#define H SCR_H
static const int MARGIN_X = 12, TOP = 10, FOOTER_H = 40;
#define BODY_W (W - 2 * MARGIN_X)
#define BODY_H (H - FOOTER_H - TOP - 4)
static uint8_t curRotation = 1;
static uint16_t palette[16];

static const GFXfont* REGULAR[] = { &FreeSerif12pt7b, &FreeSerif9pt7b };
static const GFXfont* BOLD[]    = { &FreeSerifBold12pt7b, &FreeSerifBold9pt7b };
static const int      LEADING[] = { 2, 1 };      // pixels added to each font's yAdvance
static const int      SIZES     = 2;

void displayBegin() {
  tft.init();
  tft.setRotation(1);                         // landscape, USB on the right
  tft.fillScreen(TFT_BLACK);
  ledcAttach(TFT_BL, 5000, 8);
  setBacklight(255);

  uint16_t pal[16] = {
    tft.color565(0xF4, 0xEE, 0xDD),   // C_PAPER
    tft.color565(0x24, 0x22, 0x20),   // C_INK
    tft.color565(0x8A, 0x1C, 0x1C),   // C_ACCENT
    tft.color565(0x80, 0x78, 0x70),   // C_GREY
    tft.color565(0xC9, 0xC0, 0xAE),   // C_RULE
    TFT_WHITE, TFT_BLACK,
    tft.color565(0xE4, 0xDC, 0xC8),   // C_BUTTON
    tft.color565(0x8A, 0x1C, 0x1C),   // C_BUTTON_DOWN
    tft.color565(0x2E, 0x7D, 0x32),   // C_GOOD
    tft.color565(0xB0, 0x30, 0x20),   // C_BAD
    TFT_BLACK, TFT_BLACK, TFT_BLACK, TFT_BLACK, TFT_BLACK,
  };
  memcpy(palette, pal, sizeof(palette));
  spr.setColorDepth(4);
  displaySetRotation(1);
}

void displaySetRotation(uint8_t rotation) {
  curRotation = rotation & 3;
  tft.setRotation(curRotation);
  SCR_W = (curRotation & 1) ? 320 : 240;
  SCR_H = (curRotation & 1) ? 240 : 320;
  spr.deleteSprite();                       // also frees the palette, so it is re-applied below
  if (!spr.createSprite(W, H)) Serial.println("Sprite allocation failed");
  spr.createPalette(palette, 16);
  spr.setTextWrap(false);
  tft.fillScreen(TFT_BLACK);
}

// Rotation 1 is the calibrated landscape frame. Derived from the ILI9341 MADCTL flags
// TFT_eSPI uses (rot 0: MX, rot 1: MV, rot 2: MY, rot 3: MX|MY|MV) with native column c
// and row r: landscape is x=r, y=c; rot 0 is x=239-c, y=r; rot 2 is x=c, y=319-r.
void landscapeToScreen(int xl, int yl, int& x, int& y) {
  switch (curRotation) {
    case 1:  x = xl;        y = yl;        break;
    case 0:  x = 239 - yl;  y = xl;        break;
    case 3:  x = 319 - xl;  y = 239 - yl;  break;
    default: x = yl;        y = 319 - xl;  break;   // 2
  }
}
void screenToLandscape(int x, int y, int& xl, int& yl) {
  switch (curRotation) {
    case 1:  xl = x;        yl = y;        break;
    case 0:  xl = y;        yl = 239 - x;  break;
    case 3:  xl = 319 - x;  yl = 239 - y;  break;
    default: xl = 319 - y;  yl = x;        break;   // 2
  }
}

void setBacklight(uint8_t level) { ledcWrite(TFT_BL, level); }

String formatTime(const struct tm& now) {
  char buf[12];
  if (config.clock24h) {
    strftime(buf, sizeof(buf), "%H:%M", &now);
  } else {
    int h = now.tm_hour % 12; if (h == 0) h = 12;
    snprintf(buf, sizeof(buf), "%d:%02d %s", h, now.tm_min, now.tm_hour < 12 ? "am" : "pm");
  }
  return String(buf);
}

void drawCentered(const String& text, int y, const GFXfont* font, uint8_t colour) {
  spr.setFreeFont(font);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(colour);
  spr.drawString(text, W / 2, y);
}

void drawButton(int x, int y, int w, int h, const String& label, bool pressed) {
  spr.fillRoundRect(x, y, w, h, 6, pressed ? C_BUTTON_DOWN : C_BUTTON);
  spr.drawRoundRect(x, y, w, h, 6, pressed ? C_BUTTON_DOWN : C_GREY);
  spr.setFreeFont(&FreeSans9pt7b);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(pressed ? C_WHITE : C_INK, pressed ? C_BUTTON_DOWN : C_BUTTON);
  spr.drawString(label, x + w / 2, y + h / 2);
}

void showStatus(const String& title, const String& l1, const String& l2, const String& l3) {
  spr.fillSprite(C_PAPER);
  int y0 = H / 2 - 80;
  drawCentered(title, y0, &FreeSerifBold18pt7b, C_ACCENT);
  spr.drawFastHLine(W / 5, y0 + 42, W - 2 * (W / 5), C_RULE);
  drawCentered(l1, y0 + 64, &FreeSerif12pt7b, C_INK);
  drawCentered(l2, y0 + 100, &FreeSerif9pt7b, C_GREY);
  drawCentered(l3, y0 + 126, &FreeSerif9pt7b, C_GREY);
  spr.pushSprite(0, 0);
}

// ---------- word wrapping with a bold time phrase ----------
struct Tok { String word; bool bold; bool paragraph; };
struct Run { int x; String text; bool bold; };
struct Line { std::vector<Run> runs; };

static void tokenize(const String& text, std::vector<Tok>& toks) {
  bool inPhrase = false, curBold = false;
  String cur;
  auto flush = [&]() {
    if (cur.length()) { toks.push_back({ cur, curBold, false }); cur = ""; }
    curBold = false;
  };
  for (unsigned i = 0; i < text.length(); i++) {
    char c = text[i];
    if (c == '{') { inPhrase = true; continue; }
    if (c == '}') { inPhrase = false; continue; }
    if (c == ' ' || c == '\n') {
      flush();
      if (c == '\n') toks.push_back({ "", false, true });
      continue;
    }
    if (inPhrase) curBold = true;   // any part of a word inside the phrase makes the word bold
    cur += c;
  }
  flush();
}

// Lays text out at the given size; returns the number of lines.
static int layout(const std::vector<Tok>& toks, int size, std::vector<Line>& lines) {
  lines.clear();
  spr.setFreeFont(REGULAR[size]);
  // textWidth(" ") is 0 for GFX fonts (a lone space has no ink), so measure the advance instead.
  int spaceW = spr.textWidth(" .") - spr.textWidth(".");
  Line cur; int x = 0;
  for (const Tok& t : toks) {
    if (t.paragraph) { lines.push_back(cur); cur = Line(); x = 0; continue; }
    spr.setFreeFont(t.bold ? BOLD[size] : REGULAR[size]);
    int w = spr.textWidth(t.word);
    if (x > 0 && x + w > BODY_W) { lines.push_back(cur); cur = Line(); x = 0; }
    cur.runs.push_back({ x, t.word, t.bold });
    x += w + spaceW;
  }
  if (!cur.runs.empty()) lines.push_back(cur);
  return lines.size();
}

void drawFooterStatus(const struct tm& now, bool wifiOk, const String& right) {
  int y = H - FOOTER_H;
  spr.drawFastHLine(MARGIN_X, y, BODY_W, C_RULE);
  spr.setTextFont(2);
  spr.setTextDatum(BL_DATUM);
  String left;
  if (config.showTime) left = formatTime(now);
  if (!wifiOk) left += left.isEmpty() ? "no wifi" : "  no wifi";
  spr.setTextColor(wifiOk ? C_GREY : C_BAD);
  spr.drawString(left, MARGIN_X, H - 3);
  spr.setTextDatum(BR_DATUM);
  spr.setTextColor(C_GREY);
  spr.drawString(right, W - MARGIN_X, H - 3);
}

static void drawAttribution(const Quote& q) {
  String title = q.title;
  spr.setFreeFont(&FreeSerifItalic9pt7b);
  spr.setTextDatum(TR_DATUM);
  spr.setTextColor(C_INK);
  while (title.length() > 4 && spr.textWidth(title) > BODY_W - 4) title = title.substring(0, title.length() - 4) + "...";
  spr.drawString(title, W - MARGIN_X, H - FOOTER_H + 3);
}

void showQuote(const Quote& q, const struct tm& now, bool wifiOk, int index, int count) {
  std::vector<Tok> toks;
  tokenize(q.text, toks);
  std::vector<Line> lines;
  int size = 0, n = 0;
  int lineH = 0;
  for (size = 0; size < SIZES; size++) {
    n = layout(toks, size, lines);
    spr.setFreeFont(REGULAR[size]);
    lineH = spr.fontHeight() + LEADING[size];
    if (n * lineH <= BODY_H) break;
  }
  if (size == SIZES) {                       // still too long: truncate at the smallest size
    size = SIZES - 1;
    spr.setFreeFont(REGULAR[size]);
    lineH = spr.fontHeight() + LEADING[size];
    int maxLines = BODY_H / lineH;
    lines.resize(maxLines);
    if (!lines.back().runs.empty()) lines.back().runs.back().text += " ...";
    n = maxLines;
  }
  int total = n * lineH;
  int y = TOP + (BODY_H - total) / 2;

  spr.fillSprite(C_PAPER);
  spr.setTextDatum(TL_DATUM);
  for (const Line& ln : lines) {
    for (const Run& r : ln.runs) {
      spr.setFreeFont(r.bold ? BOLD[size] : REGULAR[size]);
      spr.setTextColor(r.bold ? C_ACCENT : C_INK);
      spr.drawString(r.text, MARGIN_X + r.x, y);
    }
    y += lineH;
  }
  drawAttribution(q);
  drawFooterStatus(now, wifiOk, q.author);
  spr.pushSprite(0, 0);
}

void showNoQuote(const struct tm& now, bool wifiOk) {
  spr.fillSprite(C_PAPER);
  int y0 = (H - FOOTER_H) / 2 - 40;
  drawCentered(formatTime(now), y0, &FreeSerifBold24pt7b, C_ACCENT);
  drawCentered("No one has written about", y0 + 60, &FreeSerif9pt7b, C_GREY);
  drawCentered("this minute yet. Add a line to", y0 + 80, &FreeSerif9pt7b, C_GREY);
  drawCentered("personal.txt on the card.", y0 + 100, &FreeSerif9pt7b, C_GREY);
  drawFooterStatus(now, wifiOk, "");
  spr.pushSprite(0, 0);
}
