// Xteink X4 (ESP32-C3) pin map. Sources and caveats: ../HARDWARE.md
#pragma once

// E-paper: Good Display GDEQ0426T82, SSD1677, 800x480, SPI mode 0
static const int EPD_SCK  = 8;
static const int EPD_MOSI = 10;
static const int EPD_CS   = 21;
static const int EPD_DC   = 4;
static const int EPD_RST  = 5;
static const int EPD_BUSY = 6;

// microSD shares the SPI bus with the panel
static const int SD_MISO  = 7;
static const int SD_CS    = 12;

// Buttons: power is a plain GPIO, the rest are two resistor ladders on ADC pins
static const int BTN_POWER = 3;      // active low, internal pull-up
static const int ADC_BTN_A = 1;      // Back ~3470, Confirm ~2655, Left ~1470, Right ~3 (12-bit), idle ~4095
static const int ADC_BTN_B = 2;      // Up ~2205, Down ~3

// Power
static const int POWER_LATCH = 13;   // drive HIGH early or the board browns out on battery
static const int BAT_ADC     = 0;    // 10k/10k divider
static const int USB_DETECT  = 20;   // HIGH when USB connected
