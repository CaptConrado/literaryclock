# Xteink X4 hardware notes (research digest, 2026-09-13)

Compiled from the open firmware repos; each fact cites its source. See README.md in this folder for the port plan.

# Xteink X4 hardware research (Arduino esp32 core 3.x port)

Sources: PAP = github.com/bigbag/papyrix-reader; CP = github.com/crosspoint-reader/crosspoint-reader; SDK = github.com/open-x4-epaper/community-sdk; FI = github.com/Free-Ink/freeink-sdk; ADA = learn.adafruit.com/circuitpython-on-the-xteink-x4-ereader/pinouts; SCH = github.com/sunwoods/Xteink-X4 readme-img/05.jpg (schematic, viewed directly).

## 1. Panel (X4, ESP32-C3)
- Good Display GDEQ0426T82 4.26", 800x480, SSD1677; SPI mode 0, MSB first, 40 MHz (PAP docs/x4-specifications.md; ADA).
- SCK GPIO8, MOSI GPIO10, CS GPIO21, DC GPIO4, RST GPIO5, BUSY GPIO6 — agreed by ADA, CP lib/hal/HalGPIO.h (`EPD_SCLK 8 … EPD_BUSY 6`), PAP lib/BoardSupport/src/BoardProfiles.cpp and SCH (24-pin FPC). No panel power-enable pin (`display.power = kPinUnused`, PAP BoardProfiles.cpp).
- X4 Pro (ESP32-S3, 8 MB octal PSRAM): SCLK 12, MOSI 11, CS 13, DC 18, RST 14, BUSY 6, 20 MHz; UC8279/UC8179 (FI: SSD1677/UC8179/UC8279 auto-detected) (PAP BoardProfiles.cpp, docs/device-specifications.md).

## 2. Display driver
- CP, biscuit and Vaulco/x4-firmware use the custom `EInkDisplay` class from SDK `libs/display/EInkDisplay` (`EInkDisplay(sclk,mosi,cs,dc,rst,busy)`), wrapped by CP `lib/hal/HalDisplay.cpp`. PAP uses its own FreeInk-derived `lib/EInkDisplay` (`papyrix::hal::Display`). Only SDK `sample-firmware` uses GxEPD2 `GxEPD2_426_GDEQ0426T82` (rotation 3).
- Provenance caveat: `EInkDisplay.cpp` carries no attribution, but its init order (0x12, 0x18, 0x0C, 0x01, 0x3C, RAM area, 0x46/0x47) mirrors GxEPD2/Good Display reference code and `doc/SSD1677_GUIDE.md` cites "the GxEPD2_426_GDEQ0426T82 driver" as a reference.
- Modes: FULL ~1600 ms, HALF ~1720 ms, FAST ~600 ms (custom 111-byte LUT via 0x32), `displayWindow()` partial 50–100 ms (SDK SSD1677_GUIDE.md; PAP docs/ssd1677-driver.md).
- Ghosting: full refresh mandatory after init; keep BW RAM (0x24) and RED RAM (0x26) identical after each refresh; user "Refresh Frequency"/"Pages Per Refresh" full-refreshes every 1/5/10/15/30 pages (CP USER_GUIDE.md; PAP docs/user_guide.md). CP builds single-buffer (48,000 B; controller RED RAM = previous frame); dual = 96 KB.

## 3. SD card
- Shared SPI bus: SCK 8, MOSI 10, MISO GPIO7, CS GPIO12 (ADA; CP HalGPIO.h `SPI_MISO 7`; PAP `spiCs=12, spiMiso=7`; SCH).
- GPIO12 is the C3's SPIHD flash pin and GPIO13 (latch) is SPIWP — SCH marks CS "SPIHD?", README calls it "unusual". Flash mode must be DIO (Vaulco platformio.ini `board_build.flash_mode = dio`).
- Same SPI host, separate CS; biscuit serialises with a `RenderLock` mutex (deepwiki.com/yattsu/biscuit). Vaulco uses `greiman/SdFat ^2.3.1`.
- X4 Pro: 1-bit SDMMC CLK 41, CMD 42, D0 40, SD power GPIO5 (PAP BoardProfiles.cpp).

## 4. Buttons
- 7 buttons, no expander. Power = GPIO3, active-low, pull-up (ADA; PAP `power=3, activeHigh=false`). Reset button pulls CHIP_EN (SCH).
- Two resistor ladders on ADC: GPIO1 = Back/Confirm/Left/Right, GPIO2 = Up/Down (ADA, SCH). 12-bit values (github.com/CidVonHighwind/xteink-x4-sample): GPIO1 Back ≈3470, Confirm ≈2655, Left ≈1470, Right ≈3; GPIO2 Up ≈2205, Down ≈3; poll ~50 ms, windowed + debounced.
- X4 Pro: digital buttons — power GPIO3, side GPIO0/7 (PAP).

## 5. Battery / power
- X4: no fuel gauge, no RTC (PAP docs/device-support-matrix.md; FI README). 650 mAh cell via 10k/10k divider on GPIO0 ADC (SCH; ADA). USB/charge detect GPIO20 (UART0_RXD), HIGH = connected. Charger IC marked "4056" (TP4056 class) in teardown photo. Native USB-Serial/JTAG (`usb.nativeSerialJtag=true`).
- Power latch GPIO13, active-high, holds the battery MOSFET on (PAP `latchPin=13`). For sleep CP `lib/hal/HalPowerManager.cpp` drives it LOW "to cut battery power"; PAP PowerPolicy.cpp keeps it high only when USB-powered. Inferred: on battery, "off" is a hard power cut and the power button re-powers the C3 (cold boot); the GPIO wake path matters only on USB.
- Wake code (FI libs/hardware/PowerManager/src/PowerManager.cpp, used by CP): wait for release, `esp_deep_sleep_enable_gpio_wakeup(1<<3, ESP_GPIO_WAKEUP_GPIO_LOW)` on C3 (ext1 on S3), `esp_sleep_config_gpio_isolate()`, `gpio_deep_sleep_hold_en()`, `esp_deep_sleep_start()`. Panel hibernated first (0x10, 0x01); needs RST pulse to wake. Long-press ~0.5 s toggles power.
- X3 (same C3 binary): BQ27220 @0x55, DS3231 @0x68 on SDA 20/SCL 0. X4 Pro: CW2017 @0x63, BM8563 @0x51, GT911 @0x5D on SDA 39/SCL 38, charge status GPIO21, frontlight PWM GPIO8 cool/GPIO9 warm (PAP BoardProfiles.cpp).

## 6. Flashing
- Native USB-CDC/JTAG (`ARDUINO_USB_MODE=1`, `ARDUINO_USB_CDC_ON_BOOT=1`), enumerates as /dev/ttyACM0 (CP/Vaulco platformio.ini).
- No button combo: esptool and the web flasher (crosspointreader.com/#flash-tools, ex xteink.dve.al) auto-enter download mode; device must be awake or it won't enumerate (ADA install page; pocketink.io; tabletsage.com). After flashing: press Reset, hold Power ~3 s (CP USER_GUIDE.md). GPIO9 is on the schematic symbol with nothing wired. Disagreement: www-gem.codeberg.page/sys_x4 claims "Reset then Power 3 s = download mode; hold Power + Reset = boot" — unverified.
- OEM recovery: Power+Up at boot with update.bin on SD; some AliExpress units are USB-locked (use OTA unlocker) (pocketink.io).
- Offsets: app 0x10000 (`esptool --chip esp32c3 write_flash 0x10000 firmware.bin`); backup `read_flash 0x0 0x1000000`. Partitions (PAP partitions.csv): nvs 0x9000/0x5000, otadata 0xE000/0x2000, app0 0x10000/0x640000, app1 0x650000/0x640000, spiffs 0xC90000/0x360000, coredump 0xFF0000/0x10000.
- PlatformIO: `esp32-c3-devkitm-1`, 16 MB, DIO, gnu++17, 921600 baud (Vaulco espressif32 6.12.0; CP/PAP pioarduino). S3: `esp32-s3-devkitm-1` + PSRAM.
- arduino-cli (derived, no repo uses it): `esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio,FlashSize=16M,CPUFreq=160,PartitionScheme=…`; drop the partitions.csv above into the sketch folder. Verify option names in boards.txt.

## 7. Licences
papyrix MIT (+ FREEINK_LICENSE MIT © 2026 FreeInk); biscuit MIT; Vaulco MIT; crosspoint MIT; community-sdk MIT; freeink-sdk MIT. GxEPD2 is GPL-3.0 (github.com/ZinggJM/GxEPD2) — only sample-firmware links it; see §2 caveat on EInkDisplay provenance.

## 8. Gotchas
- No PSRAM, ~380 KB usable SRAM → one 48 KB framebuffer; CP/PAP use `-Oz`, LTO, no exceptions; PAP raises image-task stack to 12288 B.
- SSD1677: X/width multiples of 8; Y reversed; poll BUSY after 0x12/0x20 with ~10 s timeout; first write after init must be full refresh.
- GPIO20/21 (UART0) taken → log via USB-CDC. GPIO0–3 all consumed. GPIO12/13 repurposed → DIO flash only.
- Drive GPIO13 high at boot or the board browns out on battery; hold via `gpio_hold_en` when sleeping.
- Same binary must handle the X3 (UC8279, 792x528); CP/PAP detect at runtime.
