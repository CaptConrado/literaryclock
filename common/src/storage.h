// SD card access: quotes.txt + quotes.idx lookup, personal.txt overrides.
#pragma once
#include <Arduino.h>
#include <vector>

struct Quote {
  String text;      // '{' and '}' bracket the time phrase; '\n' is a paragraph break
  String title;
  String author;
  char   sfw;       // '1' safe, '0' not, '?' unknown
  bool   personal;  // came from personal.txt
};

#include <SPI.h>
// Mounts the card on an SPI bus the sketch has already begun (pins are board-specific).
bool        storageBegin(SPIClass& spi, int csPin);
const char* storageError();               // human-readable reason after a failed storageBegin
bool        quotesFilePresent();
// Loads quotes.idx, or rebuilds it by scanning quotes.txt when it is missing or stale.
bool        ensureIndex(void (*progress)(int percent));
int         quoteCountForMinute(int minuteOfDay);
std::vector<Quote> quotesForMinute(int minuteOfDay, bool sfwOnly);   // best quote first
int         quoteScore(const Quote& q);
