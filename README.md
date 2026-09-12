# Literary Clock for the Cheap Yellow Display

A clock that tells the time with literature. Every minute it shows a passage
that mentions the current time, with the time phrase in bold red, and the book
and author underneath. It runs on the ESP32-2432S028 "Cheap Yellow Display"
(320x240 ILI9341 with resistive touch) and keeps its quote database on the
board's SD card, so quotes can be edited without reflashing.

## What is on the card

Copy the whole `sdcard/literaryclock` folder to the root of a FAT32 card:

```
/literaryclock/
  quotes.txt     3,527 quotes covering 1,432 of the 1,440 minutes (generated)
  quotes.idx     lookup index for quotes.txt (generated; rebuilt by the clock if stale)
  config.txt     WiFi, timezone, brightness, touch calibration
  personal.txt   your own quotes, shown ahead of the database
```

`quotes.txt` and `quotes.idx` come from `tools/build_quotes.py`, which cleans
`data/litclock_annotated.csv` (curly quotes and dashes to ASCII, `<br>` to
paragraph breaks, fake-italic Unicode letters to plain ones) and drops quotes
over 450 characters that cannot fit the panel. Rerun it after editing the CSV.

## Time

The clock has no backup battery, so it gets the time over WiFi from NTP and
re-syncs hourly. Fill in `wifi_ssid`, `wifi_password` and `timezone` in
`config.txt`, or leave them blank and use the on-screen **WiFi setup**: the
clock hosts a network called `LiteraryClock`; join it from a phone and a page
opens where you pick your network and timezone. It writes the same
`config.txt` and restarts.

Without WiFi, the boot screen offers **Set time** by touch. The ESP32 keeps
time while powered but loses it on power cut.

Timezones accepted by name: UTC, Europe/London, Dublin, Lisbon, Paris, Berlin,
Madrid, Rome, Amsterdam, Brussels, Vienna, Zurich, Stockholm, Oslo, Copenhagen,
Prague, Warsaw, Budapest, Athens, Helsinki, Kyiv, Bucharest, Istanbul, Moscow;
America/New_York, Toronto, Detroit, Chicago, Mexico_City, Denver, Phoenix,
Los_Angeles, Vancouver, Anchorage, Sao_Paulo, Argentina/Buenos_Aires, Bogota,
Lima, Santiago; Pacific/Honolulu, Auckland; Asia/Tokyo, Seoul, Shanghai,
Hong_Kong, Singapore, Taipei, Manila, Bangkok, Jakarta, Kolkata, Karachi, Dubai,
Tehran, Jerusalem; Australia/Sydney, Melbourne, Brisbane, Adelaide, Perth;
Africa/Johannesburg, Cairo, Lagos, Nairobi. Anything else is treated as a POSIX
TZ string.

## Touch

- Tap: another quote for this minute (the footer shows e.g. `2/5`).
- Long press: menu with Another quote, Set time, WiFi setup, 12h/24h toggle,
  and a Touch test screen that shows raw readings for calibrating the
  `touch_*` keys in `config.txt`. If the crosshair moves along the wrong axis,
  set `touch_swap_xy = true`; if it moves the wrong way, use `touch_invert_x`/`_y`.

## Building and flashing

Requires `arduino-cli` with the `esp32` core and these libraries in the
sketchbook: TFT_eSPI (with the CYD pin map in `User_Setup.h`) and
XPT2046_Bitbang_Slim. Touch is bit-banged because the SD card owns the second
hardware SPI bus.

```
tools/flash.sh                      # compile, upload if the board is on /dev/cu.usbserial-110
tools/flash.sh /dev/cu.usbserial-XX # other port
```

Upload must be at 115200 baud (already in the FQBN); 921600 fails on this board.

## Layout of the code

```
literaryclock/
  literaryclock.ino   boot sequence, main loop, touch gestures
  config.*            config.txt parsing and writing
  storage.*           SD mount, quotes.idx load/rebuild, per-minute lookup, personal.txt
  timesrc.*           WiFi, NTP, timezone table, manual time, captive portal
  display.*           4-bit palette sprite, word wrap with bold phrase, footer
  ui.*                menu, set-time, touch-test and no-time screens
tools/build_quotes.py CSV -> quotes.txt + quotes.idx
```

## Data licence

Quotes come from the [literature-clock](https://github.com/JohsEnevoldsen/literature-clock)
project, originally crowd-sourced by the Guardian, and are licensed
CC BY-NC-SA 2.5. This project inherits that licence for the data.
