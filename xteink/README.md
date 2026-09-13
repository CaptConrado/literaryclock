# Xteink X4 port

Second target for the literary clock: the Xteink X4 e-paper reader.
`literaryclock_xteink/` compiles for the ESP32-C3 (`tools/flash_xteink.sh`) and
shares config, SD storage and WiFi/NTP code with the CYD build via `../common`.
Status: compiled, not yet run on the device. `HARDWARE.md` has the pin map
and flashing notes with sources.

Buttons: Confirm = another quote, Left/Right = rotate, Back = full refresh,
Power held 3 s = WiFi setup portal. v1 stays awake and expects USB power.

## Hardware (from the Papyrix and CrossPoint community firmware docs)

| | X4 | X4 Pro |
|---|---|---|
| MCU | ESP32-C3, ~380 KB RAM | ESP32-S3, 8 MB PSRAM |
| Flash | 16 MB | 16 MB |
| Panel | 4.26" 800x480 e-paper, SSD1677 controller | 800x480, UC8279/UC8179 |
| Input | buttons only, power button on GPIO3 | GT911 touch + buttons |
| Storage | microSD over SPI | microSD over 1-bit SDMMC |
| Extras | battery, BQ27220 gauge, DS3231 RTC detected on some units | same |

Pinouts are not in the docs; take them from a community firmware's board
support code (Papyrix `BoardSupport`, or CrossPoint / Biscuit).

## What is shared with the CYD build

- Everything on the SD card: `quotes.txt`, `quotes.idx`, `config.txt`,
  `personal.txt`, and the tools that generate them.
- `config.*`, `storage.*` (SPI pins change), `timesrc.*` (WiFi, NTP,
  timezone, captive portal) run unchanged on C3 and S3.
- The tokenizer and word-wrap logic in `display.cpp`; only the drawing
  surface changes.

## What the port needs

1. **Display module** for the SSD1677 panel: a 1-bit 800x480 framebuffer
   (48 KB), partial refresh each minute, full refresh every ~10 updates to
   clear ghosting. Bold is the only emphasis on 1-bit e-paper.
2. **Fonts**: with 4x the pixels, use larger type and revisit accented
   glyphs (see the deferred font note in the main README).
3. **Input module**: buttons replace the touch menu. Next quote, settings,
   set time, WiFi setup.
4. **Power loop**: deep sleep between minutes, wake on timer, NTP sync a few
   times a day. Use the DS3231 if present so time survives power-off.
5. **Build**: one source tree with a board switch (`BOARD_CYD` /
   `BOARD_XTEINK`) rather than a fork. FQBN will be `esp32:esp32:esp32c3`
   or `esp32s3` with a 16 MB flash partition scheme.

## Open questions

- Which community firmware's display driver to borrow, and its licence.
- Whether the stock bootloader allows flashing over USB directly or needs
  a button combination on boot.
- Battery life target: an hourly NTP sync plus one refresh a minute should
  give days, but needs measuring.

## References

- https://github.com/bigbag/papyrix-reader (device-specifications.md)
- https://github.com/yattsu/biscuit
- https://github.com/Vaulco/x4-firmware
- CrossPoint Reader firmware (X4 / X4 Pro)
