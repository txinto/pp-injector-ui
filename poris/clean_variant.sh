#!/usr/bin/env bash
# poris/clean_variant.sh
# Clean a variant build directory while backing up sdkconfig (which may contain secrets).
set -euo pipefail

usage() {
  echo "usage: $0 [--complete] <VariantId>" >&2
  echo "example: $0 S3WifiLCD" >&2
  echo "         $0 --complete S3WifiLCD   # remove build dir without restoring sdkconfig" >&2
  exit 2
}

COMPLETE=0
if [ "${1:-}" = "--complete" ]; then
  COMPLETE=1
  shift
fi

[ $# -eq 1 ] || usage
VAR="$1"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

SLUG="$(echo "$VAR" | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9]+//g')"
[ -n "$SLUG" ] || { echo "Invalid variant id: $VAR" >&2; exit 3; }

BUILD_DIR="${ROOT}/build_${SLUG}"
SDK_SRC="${BUILD_DIR}/sdkconfig"
BACKUP_DIR="${ROOT}/buildcfg/sdkconfig_backups"

# Backup sdkconfig if present
if [ -f "${SDK_SRC}" ]; then
  mkdir -p "${BACKUP_DIR}"
  TS="$(date +%Y%m%d-%H%M%S)"
  BACKUP="${BACKUP_DIR}/sdkconfig.${VAR}.${TS}.bak"
  cp "${SDK_SRC}" "${BACKUP}"
  echo "Backed up sdkconfig to: ${BACKUP}"
else
  echo "(info) No sdkconfig found at ${SDK_SRC}; nothing to back up."
fi

echo "Removing build directory: ${BUILD_DIR}"
rm -rf "${BUILD_DIR}"
RESTORED=0
if [ "${COMPLETE}" -eq 0 ] && [ -n "${BACKUP:-}" ] && [ -f "${BACKUP}" ]; then
  mkdir -p "${BUILD_DIR}"
  cp "${BACKUP}" "${SDK_SRC}"
  echo "Restored sdkconfig into fresh ${SDK_SRC}"
  RESTORED=1
fi

echo "Done."
if [ "${RESTORED}" -eq 1 ]; then
  echo "Next: re-run prebuild/build for ${VAR} (VS Code build button or: bash poris/prebuild.sh \"${VAR}\")."
else
  if [ "${COMPLETE}" -eq 1 ]; then
    echo "(complete clean) No sdkconfig restored. Re-run prebuild/build for ${VAR} and adjust menuconfig if needed."
  else
    echo "(info) No sdkconfig to restore. Re-run prebuild/build for ${VAR} and adjust menuconfig if needed."
  fi
fi
