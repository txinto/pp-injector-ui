#!/bin/bash
set -euo pipefail

if [ "$#" -lt 2 ]; then
  echo "Usage: $0 <build_dir> <serial_port>"
  echo "Example: $0 build_ppinjectorelecrow /dev/ttyACM0"
  exit 1
fi

BUILD_DIR="$1"
PORT="$2"
CURRENT_DIR="$(pwd)"

cd "$BUILD_DIR"
esptool.py --port "$PORT" --chip esp32s3 -b 460800 write_flash @flash_args
cd "$CURRENT_DIR"
"$(dirname "$0")/monitor_in_linux.sh" "$BUILD_DIR" "$PORT"
