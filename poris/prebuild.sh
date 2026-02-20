#!/usr/bin/env bash
set -euo pipefail
if [ $# -ne 1 ]; then
  echo "usage: $0 <VariantId>" >&2
  exit 2
fi
VAR="$1"
python3 poris/gen_variant.py --variant "$VAR"
python3 poris/ensure_variant.py --variant "$VAR"
echo "[prebuild] Variant $VAR prepared."
# echo "[prebuild] PORIS_COMPONENTS_LIST=${PORIS_COMPONENTS_LIST}"