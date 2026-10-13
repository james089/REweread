#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK_VOLUME="${SDK_VOLUME:-rm_chiappa_sdk}"
SDK_PATH="${SDK_PATH:-/opt/codex/chiappa/5.8.203}"
SDK_IMAGE="${SDK_IMAGE:-python:3.12-bookworm}"
BUILD_JOBS="${BUILD_JOBS:-2}"
QRCODEGEN_SOURCE_DIR="${QRCODEGEN_SOURCE_DIR:-$ROOT_DIR/downloads/sources/QR-Code-generator-1.8.0}"

"$ROOT_DIR/scripts/fetch-build-dependencies.sh"

if [[ -f "$SDK_PATH/environment-setup-cortexa55-remarkable-linux" ]]; then
  (
    cd "$ROOT_DIR/apps/weread-qt"
    . "$SDK_PATH/environment-setup-cortexa55-remarkable-linux"
    export LANG=C.UTF-8 LC_ALL=C.UTF-8
    sdk_qt="$(qmake -query QT_VERSION)"
    if [[ "$sdk_qt" != "6.10.3" ]]; then
      echo "SDK Qt mismatch: expected device Qt 6.10.3, found $sdk_qt" >&2
      exit 1
    fi
    rm -rf build
    cmake -S . -B build -DQRCODEGEN_SOURCE_DIR="$QRCODEGEN_SOURCE_DIR"
    cmake --build build -j"$BUILD_JOBS"
  )
else
  docker run --rm --platform linux/arm64 \
  -e BUILD_JOBS="$BUILD_JOBS" \
  -e QRCODEGEN_SOURCE_DIR=/work/downloads/sources/QR-Code-generator-1.8.0 \
  -v "$SDK_VOLUME:/opt/codex/chiappa" \
  -v "$ROOT_DIR:/work" \
  -w /work/apps/weread-qt \
  "$SDK_IMAGE" \
  bash -lc "set -euo pipefail; . '$SDK_PATH/environment-setup-cortexa55-remarkable-linux'; export LANG=C.UTF-8 LC_ALL=C.UTF-8; test \"\$(qmake -query QT_VERSION)\" = 6.10.3 || { echo 'SDK Qt mismatch: expected device Qt 6.10.3' >&2; exit 1; }; rm -rf build; cmake -S . -B build -DQRCODEGEN_SOURCE_DIR=\"\$QRCODEGEN_SOURCE_DIR\"; cmake --build build -j\"\$BUILD_JOBS\""
fi

echo "Built $ROOT_DIR/apps/weread-qt/build/rm_weread_qt"
