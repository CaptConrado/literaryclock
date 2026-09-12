// Interactive touch screens. Each runs its own loop and returns when done.
#pragma once
#include <Arduino.h>

enum MenuChoice { MENU_NEXT_QUOTE, MENU_SET_TIME, MENU_WIFI_SETUP, MENU_TOGGLE_24H, MENU_TOUCH_TEST, MENU_BACK };
enum NoTimeChoice { NOTIME_WIFI_SETUP, NOTIME_SET_TIME, NOTIME_RETRY };

MenuChoice   runMenu();
bool         runSetTime();                       // true if the user set a time
void         runTouchTest();
NoTimeChoice runNoTimePrompt(const String& reason);
void         showPortalScreen();
void         waitForRelease();
