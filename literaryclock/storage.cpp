#include "storage.h"
#include <SPI.h>
#include <SD.h>
#include <algorithm>

// CYD SD slot is on VSPI with these pins. Touch is bit-banged so the bus is ours.
static const int SD_SCK = 18, SD_MISO = 19, SD_MOSI = 23, SD_CS = 5;
static const char* QUOTES_PATH   = "/literaryclock/quotes.txt";
static const char* INDEX_PATH    = "/literaryclock/quotes.idx";
static const char* PERSONAL_PATH = "/literaryclock/personal.txt";

static SPIClass  sdSPI(VSPI);
static String    lastError;
static uint32_t  idxOffset[1440];
static uint16_t  idxCount[1440];
static bool      indexReady = false;

bool storageBegin() {
  sdSPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  const uint32_t speeds[] = { 8000000, 4000000, 1000000 };
  for (uint32_t speed : speeds) {
    for (int attempt = 0; attempt < 2; attempt++) {
      if (SD.begin(SD_CS, sdSPI, speed)) {
        Serial.printf("SD mounted at %lu Hz, type %d, %llu MB\n",
                      (unsigned long)speed, SD.cardType(), SD.cardSize() / (1024ULL * 1024ULL));
        return true;
      }
      SD.end();
      delay(200);
    }
  }
  lastError = "No SD card found. Insert a FAT32 card with a literaryclock folder.";
  return false;
}

const char* storageError() { return lastError.c_str(); }

bool quotesFilePresent() { return SD.exists(QUOTES_PATH); }

static bool loadIndexFile(uint32_t quotesSize) {
  File f = SD.open(INDEX_PATH, FILE_READ);
  if (!f) return false;
  uint8_t hdr[8];
  bool ok = f.read(hdr, 8) == 8 && memcmp(hdr, "LCIX", 4) == 0;
  uint32_t recorded = hdr[4] | (hdr[5] << 8) | (hdr[6] << 16) | ((uint32_t)hdr[7] << 24);
  ok = ok && recorded == quotesSize;
  for (int i = 0; ok && i < 1440; i++) {
    uint8_t e[6];
    if (f.read(e, 6) != 6) { ok = false; break; }
    idxOffset[i] = e[0] | (e[1] << 8) | (e[2] << 16) | ((uint32_t)e[3] << 24);
    idxCount[i]  = e[4] | (e[5] << 8);
  }
  f.close();
  return ok;
}

static bool rebuildIndex(uint32_t quotesSize, void (*progress)(int)) {
  File f = SD.open(QUOTES_PATH, FILE_READ);
  if (!f) return false;
  memset(idxOffset, 0, sizeof(idxOffset));
  memset(idxCount, 0, sizeof(idxCount));
  static uint8_t buf[1024];
  uint32_t pos = 0, lineStart = 0;
  int lastPct = -1;
  bool atLineStart = true;
  char head[5]; int headLen = 0;
  while (true) {
    int n = f.read(buf, sizeof(buf));
    if (n <= 0) break;
    for (int i = 0; i < n; i++, pos++) {
      uint8_t c = buf[i];
      if (atLineStart) {
        head[headLen++] = c;
        if (headLen == 5) {
          atLineStart = false;
          if (head[2] == ':' && isdigit(head[0]) && isdigit(head[1]) && isdigit(head[3]) && isdigit(head[4])) {
            int minute = ((head[0] - '0') * 10 + head[1] - '0') * 60 + (head[3] - '0') * 10 + head[4] - '0';
            if (minute < 1440) {
              if (idxCount[minute] == 0) idxOffset[minute] = lineStart;
              idxCount[minute]++;
            }
          }
        }
      }
      if (c == '\n') { atLineStart = true; headLen = 0; lineStart = pos + 1; }
    }
    int pct = (int)((uint64_t)pos * 100 / (quotesSize ? quotesSize : 1));
    if (pct != lastPct && progress) { progress(pct); lastPct = pct; }
  }
  f.close();

  File out = SD.open(INDEX_PATH, FILE_WRITE);
  if (out) {
    uint8_t hdr[8] = { 'L', 'C', 'I', 'X',
                       (uint8_t)quotesSize, (uint8_t)(quotesSize >> 8),
                       (uint8_t)(quotesSize >> 16), (uint8_t)(quotesSize >> 24) };
    out.write(hdr, 8);
    for (int i = 0; i < 1440; i++) {
      uint8_t e[6] = { (uint8_t)idxOffset[i], (uint8_t)(idxOffset[i] >> 8),
                       (uint8_t)(idxOffset[i] >> 16), (uint8_t)(idxOffset[i] >> 24),
                       (uint8_t)idxCount[i], (uint8_t)(idxCount[i] >> 8) };
      out.write(e, 6);
    }
    out.close();
  }
  return true;
}

bool ensureIndex(void (*progress)(int)) {
  File q = SD.open(QUOTES_PATH, FILE_READ);
  if (!q) { lastError = "quotes.txt missing in /literaryclock"; return false; }
  uint32_t size = q.size();
  q.close();
  indexReady = loadIndexFile(size);
  if (!indexReady) {
    Serial.println("Index missing or stale, rebuilding");
    indexReady = rebuildIndex(size, progress);
  }
  return indexReady;
}

int quoteCountForMinute(int minuteOfDay) {
  if (!indexReady || minuteOfDay < 0 || minuteOfDay >= 1440) return 0;
  return idxCount[minuteOfDay];
}

static void unescape(String& s) { s.replace("\\n", "\n"); }

// Line format: HH:MM|text|title|author[|sfw]
static bool parseLine(const String& line, Quote& q, bool personal) {
  int p1 = line.indexOf('|');
  int p2 = p1 < 0 ? -1 : line.indexOf('|', p1 + 1);
  int p3 = p2 < 0 ? -1 : line.indexOf('|', p2 + 1);
  if (p3 < 0) return false;
  int p4 = line.indexOf('|', p3 + 1);
  q.text   = line.substring(p1 + 1, p2);
  q.title  = line.substring(p2 + 1, p3);
  q.author = p4 < 0 ? line.substring(p3 + 1) : line.substring(p3 + 1, p4);
  q.sfw    = p4 < 0 ? '?' : (line.charAt(p4 + 1) == '0' ? '0' : line.charAt(p4 + 1) == '1' ? '1' : '?');
  q.personal = personal;
  q.text.trim(); q.title.trim(); q.author.trim();
  unescape(q.text);
  return q.text.length() > 0;
}

static void appendPersonal(int minuteOfDay, std::vector<Quote>& out) {
  File f = SD.open(PERSONAL_PATH, FILE_READ);
  if (!f) return;
  char want[6];
  snprintf(want, sizeof(want), "%02d:%02d", minuteOfDay / 60, minuteOfDay % 60);
  while (f.available()) {
    String line = f.readStringUntil('\n');
    if (!line.startsWith(want)) continue;
    Quote q;
    if (parseLine(line, q, true)) out.push_back(q);
  }
  f.close();
}

// Higher is better: a readable length, safe, single paragraph, phrase not at the very start.
int quoteScore(const Quote& q) {
  int s = 0, n = q.text.length();
  if (n >= 90 && n <= 330) s += 3; else if (n < 60 || n > 400) s -= 2;
  if (q.sfw == '0') s -= 3; else if (q.sfw == '1') s += 1;
  if (q.text.indexOf('\n') >= 0) s -= 1;
  if (q.text.startsWith("{")) s -= 1;
  if (q.personal) s += 10;
  return s;
}

std::vector<Quote> quotesForMinute(int minuteOfDay, bool sfwOnly) {
  std::vector<Quote> out;
  appendPersonal(minuteOfDay, out);
  int count = quoteCountForMinute(minuteOfDay);
  if (count == 0) return out;
  File f = SD.open(QUOTES_PATH, FILE_READ);
  if (!f) return out;
  if (!f.seek(idxOffset[minuteOfDay])) { f.close(); return out; }
  for (int i = 0; i < count && f.available(); i++) {
    String line = f.readStringUntil('\n');
    Quote q;
    if (!parseLine(line, q, false)) continue;
    if (sfwOnly && q.sfw == '0') continue;
    out.push_back(q);
  }
  f.close();
  std::stable_sort(out.begin(), out.end(), [](const Quote& a, const Quote& b) { return a.personal && !b.personal; });
  return out;
}
