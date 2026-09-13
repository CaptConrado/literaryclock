// Interactive touch screens. Each runs its own loop and returns when done.
#pragma once
#include <Arduino.h>

enum MenuChoice { MENU_NEXT_QUOTE, MENU_ROTATE, MENU_SET_TIME, MENU_TOGGLE_24H,
                  MENU_WIFI_SETUP, MENU_CALIBRATE, MENU_BRIGHTNESS, MENU_BACK };
enum NoTimeChoice { NOTIME_WIFI_SETUP, NOTIME_SET_TIME, NOTIME_RETRY };

MenuChoice   runMenu();
bool         runSetTime();                       // true if the user set a time
void         runTouchTest();
void         runCalibration();                   // three-point calibration, saves to config.txt
NoTimeChoice runNoTimePrompt(const String& reason);
void         showPortalScreen();
void         waitForRelease();
