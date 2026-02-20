#!/usr/bin/env bash
# poris/build.sh
set -euo pipefail
shopt -s nullglob

usage() {
  echo "uso: $0 <VariantId|any-case> [idf.py args...]" >&2
  echo "ej.:  $0 c6devkit build" >&2
  echo "      $0 C6Devkit flash monitor" >&2
  exit 2
}

[ $# -ge 1 ] || usage
REQ_VAR="$1"; shift || true

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VARS_YML="${ROOT}/variants/variants.yml"

if [ ! -f "$VARS_YML" ]; then
  echo "No encuentro ${VARS_YML}" >&2
  exit 3
fi

# Resuelve el ID canónico (case-insensitive) desde variants.yml
CANON_ID="$(python3 - <<'PY' "$REQ_VAR" "$VARS_YML"
import sys, yaml, re, pathlib
req = sys.argv[1].lower()
cfg = yaml.safe_load(pathlib.Path(sys.argv[2]).read_text(encoding="utf-8")) or {}
vs  = cfg.get("variants", [])
for v in vs:
    vid = (v.get("id") or "")
    if vid.lower() == req:
        print(vid); break
PY
)"
if [ -z "${CANON_ID:-}" ]; then
  echo "Variante '${REQ_VAR}' no encontrada en ${VARS_YML}" >&2
  exit 4
fi

# Slug para el directorio de build
SLUG="$(echo "$CANON_ID" | tr '[:upper:]' '[:lower:]' | sed -E 's/[^a-z0-9]+//g')"
BUILD_DIR="${ROOT}/build_${SLUG}"

# 1) Prebuild (genera defaults/env/components cacheados)
bash "${ROOT}/poris/prebuild.sh" "$CANON_ID"

# 2) Carga config.env si existe (extra_env para kconfig, etc.)
if [ -f "${BUILD_DIR}/config.env" ]; then
  set -a
  . "${BUILD_DIR}/config.env"
  set +a
fi

# 2b) Alinear IDF_TARGET con la variante (si el YAML lo fija)
#     Lee buildcfg/<Var>.cmakecache.cmake: 'set(IDF_TARGET esp32cX)'
CACHE_FILE="${ROOT}/buildcfg/${CANON_ID}.cmakecache.cmake"
if [ -f "${CACHE_FILE}" ]; then
  # Extrae el segundo token de la línea 'set(IDF_TARGET <valor>)'
  TGT="$(awk -F'[ )]+' '/^set\(IDF_TARGET/{print $2}' "${CACHE_FILE}" || true)"
  if [ -n "${TGT:-}" ]; then
    export IDF_TARGET="${TGT}"
  fi
fi

# 2c) Rutas de sdkconfig: fuerza a usar SIEMPRE el del build dir + defaults de la variante
SDKCONFIG_PATH="${BUILD_DIR}/sdkconfig"
SDKDEFAULTS_PATH="${ROOT}/buildcfg/sdkconfig.${CANON_ID}.defaults"

if [ ! -f "${SDKDEFAULTS_PATH}" ]; then
  echo "No encuentro defaults de la variante: ${SDKDEFAULTS_PATH}" >&2
  echo "¿Has ejecutado prebuild? (poris/prebuild.sh ${CANON_ID})" >&2
  exit 5
fi

# 3) Llama a idf.py con el build dir y la variante; propaga args extra
exec idf.py -B "${BUILD_DIR}" -DVARIANT="${CANON_ID}" -DSDKCONFIG="${SDKCONFIG_PATH}" -DSDKCONFIG_DEFAULTS="${SDKDEFAULTS_PATH}" "$@"
