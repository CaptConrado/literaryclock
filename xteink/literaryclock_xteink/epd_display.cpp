#include "epd_display.h"
#include "board.h"
#include <config.h>
#include <SPI.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeSerif12pt7b.h>
#include <Fonts/FreeSerif18pt7b.h>
#include <Fonts/FreeSerif24pt7b.h>
#include <Fonts/FreeSerifBold12pt7b.h>
#include <Fonts/FreeSerifBold18pt7b.h>
#include <Fonts/FreeSerifBold24pt7b.h>
#include <Fonts/FreeSerifItalic12pt7b.h>
#include <Fonts/FreeSerifItalic9pt7b.h>
#include <Fonts/FreeSerif9pt7b.h>
#include <vector>

// Full-frame buffer: 800*480/8 = 48,000 bytes. The C3 has no PSRAM but this fits comfortably.
static GxEPD2_BW<GxEPD2_426_GDEQ0426T82, GxEPD2_426_GDEQ0426T82::HEIGHT>
  display(GxEPD2_426_GDEQ0426T82(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

static const int MARGIN = 36, TOP = 30, FOOTER_H = 84;
#define W (display.width())
#define H (display.height())
#define BODY_W (W - 2 * MARGIN)
#define BODY_H (H - FOOTER_H - TOP)

static const GFXfont* REGULAR[] = { &FreeSerif24pt7b, &FreeSerif18pt7b, &FreeSerif12pt7b };
static const GFXfont* BOLD[]    = { &FreeSerifBold24pt7b, &FreeSerifBold18pt7b, &FreeSerifBold12pt7b };
static const int      LEADING[] = { 6, 5, 3 };
static const int      SIZES     = 3;

void epdBegin() {
  SPI.begin(EPD_SCK, SD_MISO, EPD_MOSI, EPD_CS);
  display.epd2.selectSPI(SPI, SPISettings(20000000, MSBFIRST, SPI_MODE0));
  display.init(0, true, 2, false);
  display.setRotation(0);
  display.setTextWrap(false);
}

// config.txt stores TFT_eSPI-style codes (1 landscape, 3 flipped, 0 portrait, 2 portrait flipped).
// The panel's native frame is 800 wide, so landscape is GxEPD2 rotation 0.
void epdSetOrientation(uint8_t r) {
  switch (r & 3) {
    case 1:  display.setRotation(0); break;
    case 3:  display.setRotation(2); break;
    case 0:  display.setRotation(3); break;
    default: display.setRotation(1); break;
  }
}
int epdWidth()  { return W; }
int epdHeight() { return H; }

// ---- text measurement with GFX fonts (advance width, like TFT_eSPI's textWidth) ----
static int advance(const GFXfont* f, const String& s) {
  int w = 0;
  uint8_t first = pgm_read_byte(&f->first), last = pgm_read_byte(&f->last);
  for (unsigned i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (c < first || c > last) continue;
    GFXglyph* g = &f->glyph[c - first];
    w += pgm_read_byte(&g->xAdvance);
  }
  return w;
}
static int ascent(const GFXfont* f) {          // max ascent above baseline, like TFT_eSPI's glyph_ab
  int ab = 0;
  uint8_t first = pgm_read_byte(&f->first), last = pgm_read_byte(&f->last);
  for (int c = first; c <= last; c++) {
    int yo = (int8_t)pgm_read_byte(&f->glyph[c - first].yOffset);
    if (-yo > ab) ab = -yo;
  }
  return ab;
}
static void drawText(const GFXfont* f, int x, int topY, const String& s) {   // top-left datum
  display.setFont(f);
  display.setCursor(x, topY + ascent(f));
  display.print(s);
}
static void drawCentered(const GFXfont* f, int topY, const String& s) {
  drawText(f, (W - advance(f, s)) / 2, topY, s);
}

// ---- word wrap with a bold time phrase (mirrors display.cpp on the CYD) ----
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
    if (c == ' ' || c == '\n') { flush(); if (c == '\n') toks.push_back({ "", false, true }); continue; }
    if (inPhrase) curBold = true;
    cur += c;
  }
  flush();
}

static int layout(const std::vector<Tok>& toks, int size, std::vector<Line>& lines) {
  lines.clear();
  int spaceW = advance(REGULAR[size], " ");
  Line cur; int x = 0;
  for (const Tok& t : toks) {
    if (t.paragraph) { lines.push_back(cur); cur = Line(); x = 0; continue; }
    int w = advance(t.bold ? BOLD[size] : REGULAR[size], t.word);
    if (x > 0 && x + w > BODY_W) { lines.push_back(cur); cur = Line(); x = 0; }
    cur.runs.push_back({ x, t.word, t.bold });
    x += w + spaceW;
  }
  if (!cur.runs.empty()) lines.push_back(cur);
  return lines.size();
}

static String timeString(const struct tm& now) {
  char buf[12];
  if (config.clock24h) strftime(buf, sizeof(buf), "%H:%M", &now);
  else { int h = now.tm_hour % 12; if (!h) h = 12; snprintf(buf, sizeof(buf), "%d:%02d %s", h, now.tm_min, now.tm_hour < 12 ? "am" : "pm"); }
  return String(buf);
}

static void drawFooter(const String& title, const String& author, const struct tm& now, bool wifiOk) {
  int y = H - FOOTER_H;
  display.drawFastHLine(MARGIN, y, BODY_W, GxEPD_BLACK);
  String t = title;
  while (t.length() > 4 && advance(&FreeSerifItalic12pt7b, t) > BODY_W) t = t.substring(0, t.length() - 4) + "...";
  drawText(&FreeSerifItalic12pt7b, W - MARGIN - advance(&FreeSerifItalic12pt7b, t), y + 10, t);
  drawText(&FreeSerif12pt7b, W - MARGIN - advance(&FreeSerif12pt7b, author), y + 44, author);
  String left = config.showTime ? timeString(now) : "";
  if (!wifiOk) left += left.isEmpty() ? "no wifi" : "   no wifi";
  drawText(&FreeSerif12pt7b, MARGIN, y + 44, left);
}

void epdShowStatus(const String& title, const String& l1, const String& l2, const String& l3) {
  display.setFullWindow();
  display.firstPage();
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  int y0 = H / 2 - 110;
  drawCentered(&FreeSerifBold24pt7b, y0, title);
  display.drawFastHLine(W / 4, y0 + 70, W / 2, GxEPD_BLACK);
  drawCentered(&FreeSerif18pt7b, y0 + 96, l1);
  drawCentered(&FreeSerif12pt7b, y0 + 150, l2);
  drawCentered(&FreeSerif12pt7b, y0 + 184, l3);
  display.nextPage();
}

void epdShowQuote(const Quote& q, const struct tm& now, bool wifiOk, bool fullRefresh) {
  std::vector<Tok> toks; tokenize(q.text, toks);
  std::vector<Line> lines;
  int size, n = 0, lineH = 0;
  for (size = 0; size < SIZES; size++) {
    n = layout(toks, size, lines);
    lineH = pgm_read_byte(&REGULAR[size]->yAdvance) + LEADING[size];
    if (n * lineH <= BODY_H) break;
  }
  if (size == SIZES) {
    size = SIZES - 1;
    lineH = pgm_read_byte(&REGULAR[size]->yAdvance) + LEADING[size];
    int maxLines = BODY_H / lineH;
    lines.resize(maxLines); n = maxLines;
    if (!lines.back().runs.empty()) lines.back().runs.back().text += " ...";
  }
  if (fullRefresh) display.setFullWindow(); else display.setPartialWindow(0, 0, W, H);
  display.firstPage();
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  int y = TOP + (BODY_H - n * lineH) / 2;
  for (const Line& ln : lines) {
    for (const Run& r : ln.runs) drawText(r.bold ? BOLD[size] : REGULAR[size], MARGIN + r.x, y, r.text);
    y += lineH;
  }
  drawFooter(q.title, q.author, now, wifiOk);
  display.nextPage();
}

void epdShowNoQuote(const struct tm& now, bool wifiOk, bool fullRefresh) {
  if (fullRefresh) display.setFullWindow(); else display.setPartialWindow(0, 0, W, H);
  display.firstPage();
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  int y0 = (H - FOOTER_H) / 2 - 60;
  drawCentered(&FreeSerifBold24pt7b, y0, timeString(now));
  drawCentered(&FreeSerif12pt7b, y0 + 90, "No one has written about this minute yet.");
  drawCentered(&FreeSerif12pt7b, y0 + 124, "Add a line to personal.txt on the card.");
  drawFooter("", "", now, wifiOk);
  display.nextPage();
}

void epdSleep() { display.hibernate(); }
