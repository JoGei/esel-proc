#!/usr/bin/env bash
set -euo pipefail

TARGET_CORE="award-winning:serv:serv:1.4.0"

FUSESOC_BUILD_DIR="$1"
OUT="$2"

__DIR__="${PWD}"
mkdir -p "$FUSESOC_BUILD_DIR"
mkdir -p "$(dirname "$OUT")"
cd "$FUSESOC_BUILD_DIR"
# per https://github.com/olofk/serv/README.md
export WORKSPACE="${PWD}"
fusesoc library add fusesoc_cores https://github.com/fusesoc/fusesoc-cores
fusesoc library add serv https://github.com/olofk/serv
export SERV="$WORKSPACE/fusesoc_libraries/serv"
fusesoc core list | grep -i serv
fusesoc run --setup --build-root "$FUSESOC_BUILD_DIR" --target=lint "${TARGET_CORE}"
fusesoc run --setup --build-root "$FUSESOC_BUILD_DIR" --tool=openlane --target=default "${TARGET_CORE}" 
cd ${__DIR__}

EDAM_FILE="$(find "$FUSESOC_BUILD_DIR" -name '*.eda.yml' | head -n 1)"

if [ -z "${EDAM_FILE}" ]; then
  echo "ERROR: Could not find generated .eda.yml under $FUSESOC_BUILD_DIR" >&2
  exit 1
fi

python3 "$(dirname "$0")/../utils/python/edam_to_single_sv.py" \
  --edam "$EDAM_FILE" \
  --out "$OUT"