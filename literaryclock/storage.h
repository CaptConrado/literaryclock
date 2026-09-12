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

bool        storageBegin();               // mounts the card; retries at a conservative SPI clock
const char* storageError();               // human-readable reason after a failed storageBegin
bool        quotesFilePresent();
// Loads quotes.idx, or rebuilds it by scanning quotes.txt when it is missing or stale.
bool        ensureIndex(void (*progress)(int percent));
int         quoteCountForMinute(int minuteOfDay);
std::vector<Quote> quotesForMinute(int minuteOfDay, bool sfwOnly);
