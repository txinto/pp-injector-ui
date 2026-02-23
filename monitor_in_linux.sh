#!/bin/bash
set -euo pipefail

if [ "$#" -lt 2 ]; then
  echo "Usage: $0 <build_dir> <serial_port>"
  echo "Example: $0 build_ppinjectorelecrow /dev/ttyACM0"
  exit 1
fi

BUILD_DIR="$1"
PORT="$2"

if [ -z "${IDF_PYTHON_ENV_PATH:-}" ] || [ -z "${IDF_PATH:-}" ]; then
  echo "ERROR: IDF environment is not loaded."
  echo "Run: source <esp-idf>/export.sh"
  exit 1
fi

# Prefer app ELF (<project>.elf) at the root of the build directory.
ELF_PATH="$(find "$BUILD_DIR" -maxdepth 1 -type f -name '*.elf' | head -n 1)"
if [ -z "$ELF_PATH" ]; then
  echo "ERROR: No ELF found in $BUILD_DIR"
  exit 1
fi

"$IDF_PYTHON_ENV_PATH/bin/python" "$IDF_PATH/tools/idf_monitor.py" \
  -p "$PORT" \
  -b 115200 \
  --toolchain-prefix xtensa-esp32s3-elf- \
  --make "$IDF_PYTHON_ENV_PATH/bin/python $IDF_PATH/tools/idf.py" \
  --target esp32s3 \
  "$ELF_PATH"
