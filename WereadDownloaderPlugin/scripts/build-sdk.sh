#!/usr/bin/env bash
# Builds the plugin and isolated test programs, without installing anything or
# rebuilding the existing reading client. Firmware replay is a separate gate.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SDK_PATH="${SDK_PATH:-/opt/codex/chiappa/5.8.203}"
if [[ -f "$SDK_PATH/environment-setup-cortexa55-remarkable-linux" ]]; then
  (
    . "$SDK_PATH/environment-setup-cortexa55-remarkable-linux"
    export LANG=C.UTF-8 LC_ALL=C.UTF-8
    test "$(qmake -query QT_VERSION)" = 6.10.3
    cmake -S "$ROOT/WereadDownloaderPlugin" -B "$ROOT/WereadDownloaderPlugin/build/sdk" -DBUILD_TESTING=ON -DWRDP_BUILD_NATIVE=ON
    cmake --build "$ROOT/WereadDownloaderPlugin/build/sdk" -j2
  )
else
  docker info >/dev/null 2>&1 || { echo "Docker/official SDK runtime is unavailable" >&2; exit 1; }
  docker run --rm --platform linux/arm64 \
    -e LANG=C.UTF-8 -e LC_ALL=C.UTF-8 -e WRDP_SDK_PATH="$SDK_PATH" \
    -v "${SDK_VOLUME:-rm_chiappa_sdk}:/opt/codex/chiappa" \
    -v "$ROOT:/work" -w /work/WereadDownloaderPlugin \
    "${SDK_IMAGE:-python:3.12-bookworm}" \
    bash -lc 'set -euo pipefail; . "$WRDP_SDK_PATH/environment-setup-cortexa55-remarkable-linux"; export LANG=C.UTF-8 LC_ALL=C.UTF-8; test "$(qmake -query QT_VERSION)" = 6.10.3; cmake -S . -B build/sdk -DBUILD_TESTING=ON -DWRDP_BUILD_NATIVE=ON; cmake --build build/sdk -j2'
fi
