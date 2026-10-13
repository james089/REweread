#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK_VERSION="${SDK_VERSION:-5.8.203}"
SDK_VOLUME="${SDK_VOLUME:-rm_chiappa_sdk}"
SDK_DEST="${SDK_DEST:-/opt/codex/chiappa/$SDK_VERSION}"
SDK_IMAGE="${SDK_IMAGE:-python:3.12-bookworm}"
DOWNLOAD_DIR="${DOWNLOAD_DIR:-$ROOT_DIR/downloads/official-sdk}"
SDK_FILE="remarkable-production-image-${SDK_VERSION}-chiappa-public-aarch64-toolchain.sh"
SDK_URL="${SDK_URL:-https://storage.googleapis.com/remarkable-codex-toolchain/3.28.0.172/$SDK_FILE}"
SDK_SHA256="${SDK_SHA256:-ae0dcec8ff161a90f72b5bbeb77f11c5a14c6c6045c1655584561935994c1832}"

mkdir -p "$DOWNLOAD_DIR"

if [[ ! -s "$DOWNLOAD_DIR/$SDK_FILE" ]]; then
  curl -fL --retry 3 --retry-delay 2 "$SDK_URL" -o "$DOWNLOAD_DIR/$SDK_FILE"
fi

actual_sha="$(shasum -a 256 "$DOWNLOAD_DIR/$SDK_FILE" | awk '{print $1}')"
if [[ "$actual_sha" != "$SDK_SHA256" ]]; then
  echo "SDK checksum mismatch." >&2
  echo "Expected: $SDK_SHA256" >&2
  echo "Actual:   $actual_sha" >&2
  echo "Verify the current file on https://developer.remarkable.com/links before updating this script." >&2
  exit 1
fi

# A native aarch64 Linux host can run the official SDK without Docker.
if [[ "$(uname -s)" == "Linux" && "$(uname -m)" == "aarch64" ]] && ! command -v docker >/dev/null; then
  if [[ ! -f "$SDK_DEST/environment-setup-cortexa55-remarkable-linux" ]]; then
    bash "$DOWNLOAD_DIR/$SDK_FILE" -y -d "$SDK_DEST"
  fi
  test -f "$SDK_DEST/environment-setup-cortexa55-remarkable-linux"
  echo "SDK ready at $SDK_DEST"
  exit 0
fi

docker volume create "$SDK_VOLUME" >/dev/null

if docker run --rm --platform linux/arm64 \
  -v "$SDK_VOLUME:/opt/codex/chiappa" \
  "$SDK_IMAGE" \
  test -f "$SDK_DEST/environment-setup-cortexa55-remarkable-linux"; then
  echo "SDK already installed in Docker volume $SDK_VOLUME at $SDK_DEST"
  exit 0
fi

docker run --rm --platform linux/arm64 \
  -v "$SDK_VOLUME:/opt/codex/chiappa" \
  -v "$DOWNLOAD_DIR:/sdk:ro" \
  "$SDK_IMAGE" \
  bash "/sdk/$SDK_FILE" -y -d "$SDK_DEST"

docker run --rm --platform linux/arm64 \
  -v "$SDK_VOLUME:/opt/codex/chiappa" \
  "$SDK_IMAGE" \
  test -f "$SDK_DEST/environment-setup-cortexa55-remarkable-linux"

echo "Installed chiappa SDK $SDK_VERSION in Docker volume $SDK_VOLUME"
