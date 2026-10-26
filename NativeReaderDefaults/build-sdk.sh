#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK_PATH="${SDK_PATH:-/opt/codex/chiappa/5.8.203}"
if [[ -f "$SDK_PATH/environment-setup-cortexa55-remarkable-linux" ]]; then
    . "$SDK_PATH/environment-setup-cortexa55-remarkable-linux"
    export LANG=C.UTF-8 LC_ALL=C.UTF-8
    cmake -S "$ROOT/NativeReaderDefaults" -B "$ROOT/NativeReaderDefaults/build/sdk"
    cmake --build "$ROOT/NativeReaderDefaults/build/sdk" -j2
else
    docker run --rm --platform linux/arm64 -e LANG=C.UTF-8 -e LC_ALL=C.UTF-8 \
        -e READER_SDK_PATH="$SDK_PATH" \
        -v "${SDK_VOLUME:-rm_chiappa_sdk}:/opt/codex/chiappa" \
        -v "$ROOT:/work" -w /work "${SDK_IMAGE:-python:3.12-bookworm}" \
        bash -lc '. "$READER_SDK_PATH/environment-setup-cortexa55-remarkable-linux" && cmake -S NativeReaderDefaults -B NativeReaderDefaults/build/sdk && cmake --build NativeReaderDefaults/build/sdk -j2'
fi
