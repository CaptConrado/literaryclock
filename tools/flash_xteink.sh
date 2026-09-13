#!/bin/sh
# Compile the Xteink X4 firmware and, if the device is attached, upload it.
# Usage: tools/flash_xteink.sh [port]   (default: first /dev/cu.usbmodem*)
set -e
cd "$(dirname "$0")/.."
PORT="${1:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -1)}"
FQBN="esp32:esp32:esp32c3:CDCOnBoot=cdc,FlashMode=dio,FlashSize=16M,CPUFreq=160,PartitionScheme=default_8MB"
arduino-cli compile --fqbn "$FQBN" --library common --output-dir build/xteink xteink/literaryclock_xteink
if [ -n "$PORT" ] && [ -e "$PORT" ]; then
  arduino-cli upload -p "$PORT" --fqbn "$FQBN" --input-dir build/xteink xteink/literaryclock_xteink
else
  echo "No X4 on USB; compiled only. Binaries are in build/xteink/."
fi
