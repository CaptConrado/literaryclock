#!/bin/sh
# Compile the firmware and, if a board is attached, upload it.
# Usage: tools/flash.sh [port]      default port /dev/cu.usbserial-110
set -e
cd "$(dirname "$0")/.."
PORT="${1:-/dev/cu.usbserial-110}"
FQBN="esp32:esp32:esp32:UploadSpeed=115200,CPUFreq=240,FlashFreq=80,FlashMode=qio,FlashSize=4M,PartitionScheme=huge_app"
arduino-cli compile --fqbn "$FQBN" --output-dir build literaryclock
if [ -e "$PORT" ]; then
  arduino-cli upload -p "$PORT" --fqbn "$FQBN" --input-dir build literaryclock
else
  echo "No board on $PORT; compiled only. Binaries are in build/."
fi
