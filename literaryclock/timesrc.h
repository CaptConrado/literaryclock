// WiFi, NTP, timezone, manual time and the captive-portal setup page.
#pragma once
#include <Arduino.h>

void   applyTimezone();                                  // sets TZ from config, no network needed
String resolveTimezone(const String& nameOrPosix);       // IANA name -> POSIX string, else passthrough
bool   wifiConnect(uint32_t timeoutMs, void (*progress)(int secondsElapsed));
bool   wifiConnected();
void   ntpBegin();
bool   timeIsValid();
void   setLocalHourMinute(int hour, int minute);

enum PortalResult { PORTAL_SAVED, PORTAL_CANCELLED };
// Hosts the "LiteraryClock" access point and a setup page; blocks until saved or cancelPoll() returns true.
PortalResult runPortal(bool (*cancelPoll)());
const char* portalApName();
String      portalIp();
