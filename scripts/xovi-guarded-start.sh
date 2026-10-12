#!/bin/sh
# Fail-closed XOVI launcher.  It deliberately has no dependency on systemd
# state: it must be safe to invoke while xochitl is the service being started.
set -eu

# systemd's service environment is intentionally sparse.  Use only the
# device's standard command locations, never a caller-supplied PATH.
PATH=/usr/sbin:/usr/bin:/sbin:/bin
export PATH

if [ "${XOVI_GUARD_TEST_MODE:-}" = 1 ]; then
  ROOT="${XOVI_GUARD_ROOT:-/home/root/xovi}"
  STOCK_BIN="${XOVI_GUARD_STOCK_BIN:-/usr/bin/xochitl}"
  OS_ID_FILE="${XOVI_GUARD_OS_ID_FILE:-/etc/version}"
  RECEIPT="${XOVI_GUARD_RECEIPT:-$ROOT/compatibility-receipt.env}"
  MARKER="${XOVI_GUARD_MARKER:-$ROOT/.xovi-injection-in-flight}"
  ACTIVE_EXTENSIONS="${XOVI_GUARD_ACTIVE_EXTENSIONS:-$ROOT/services/xochitl.service/extensions.d}"
  RESOURCE_TREE="${XOVI_GUARD_RESOURCE_TREE:-$ROOT/services/xochitl.service/exthome/qt-resource-rebuilder}"
else
  ROOT=/home/root/xovi
  STOCK_BIN=/usr/bin/xochitl
  OS_ID_FILE=/etc/version
  RECEIPT="$ROOT/compatibility-receipt.env"
  MARKER="$ROOT/.xovi-injection-in-flight"
  ACTIVE_EXTENSIONS="$ROOT/services/xochitl.service/extensions.d"
  RESOURCE_TREE="$ROOT/services/xochitl.service/exthome/qt-resource-rebuilder"
fi
XOVI_SO="$ROOT/xovi.so"
QRR_SO="$ROOT/extensions.d/qt-resource-rebuilder.so"
INJECTED_XOVI_ROOT="$ROOT/services/xochitl.service"

# The systemd drop-in clears these before the shell starts.  Clear them again
# before any fallback or child decision so they cannot leak into stock Xochitl.
unset LD_PRELOAD XOVI_ROOT

say() { printf '%s\n' "xovi-guard: $*" >&2; }
hash_file() { sha256sum "$1" | awk '{print $1}'; }
is_hash() { case "$1" in *[!0123456789abcdef]*|'') return 1;; *) [ "${#1}" -eq 64 ];; esac; }

receipt_value() {
  key="$1"
  value=""
  [ -f "$RECEIPT" ] || return 1
  while IFS= read -r line || [ -n "$line" ]; do
    case "$line" in "$key="*)
      [ -z "$value" ] || return 1
      value=${line#*=}
      ;;
    esac
  done < "$RECEIPT"
  [ -n "$value" ] || return 1
  printf '%s\n' "$value"
}

resource_inventory() {
  [ -d "$RESOURCE_TREE" ] || return 1
  # Symlinks make a receipt dependent on files outside the reviewed tree.
  # Reject them rather than following an attacker-controlled target.
  if find "$RESOURCE_TREE/." -type l -print | grep -q .; then return 1; fi
  (
    cd "$RESOURCE_TREE"
    find . -type f -print | LC_ALL=C sort | while IFS= read -r item; do
      sha256sum "$item"
    done
  ) | sha256sum | awk '{print $1}'
}

check_compatibility() {
  [ -r "$OS_ID_FILE" ] && [ -x "$STOCK_BIN" ] && [ -r "$XOVI_SO" ] || return 1
  [ -d "$ACTIVE_EXTENSIONS" ] && [ -r "$QRR_SO" ] || return 1
  [ -s "$RESOURCE_TREE/hashtab" ] || return 1
  [ -f "$RECEIPT" ] || return 1

  format=$(receipt_value format) || return 1
  [ "$format" = 1 ] || return 1
  wanted_os=$(receipt_value os_identity) || return 1
  actual_os=$(tr -d '\r\n' < "$OS_ID_FILE")
  [ "$wanted_os" = "$actual_os" ] || return 1

  for key in os_identity_sha256 xochitl_sha256 xovi_sha256 appload_sha256 qrr_sha256 resource_inventory_sha256; do
    wanted=$(receipt_value "$key") || return 1
    is_hash "$wanted" || return 1
  done
  wanted_os_identity_sha256=$(receipt_value os_identity_sha256) || return 1
  wanted_xochitl_sha256=$(receipt_value xochitl_sha256) || return 1
  wanted_xovi_sha256=$(receipt_value xovi_sha256) || return 1
  wanted_appload_sha256=$(receipt_value appload_sha256) || return 1
  wanted_qrr_sha256=$(receipt_value qrr_sha256) || return 1
  wanted_resource_inventory_sha256=$(receipt_value resource_inventory_sha256) || return 1
  [ "$(hash_file "$OS_ID_FILE")" = "$wanted_os_identity_sha256" ] || return 1
  [ "$(hash_file "$STOCK_BIN")" = "$wanted_xochitl_sha256" ] || return 1
  [ "$(hash_file "$XOVI_SO")" = "$wanted_xovi_sha256" ] || return 1
  [ "$(hash_file "$QRR_SO")" = "$wanted_qrr_sha256" ] || return 1

  # Only the two explicitly reviewed extensions are allowed to affect Xochitl.
  names=$(find "$ACTIVE_EXTENSIONS/." -mindepth 1 -maxdepth 1 -print | sed 's#.*/##' | LC_ALL=C sort | tr '\n' ' ')
  [ "$names" = 'appload.so qt-resource-rebuilder.so ' ] || return 1
  for name in appload.so qt-resource-rebuilder.so; do
    extension_path="$ACTIVE_EXTENSIONS/$name"
    [ -r "$extension_path" ] || return 1
    # A symlink is allowed only when it resolves under ROOT and to a regular file.
    if [ -L "$extension_path" ]; then
      target=$(readlink -f "$extension_path" 2>/dev/null || true)
      case "$target" in "$ROOT"/*) [ -f "$target" ] || return 1;; *) return 1;; esac
    elif [ ! -f "$extension_path" ]; then
      return 1
    fi
  done
  [ "$(hash_file "$ACTIVE_EXTENSIONS/appload.so")" = "$wanted_appload_sha256" ] || return 1
  [ "$(hash_file "$ACTIVE_EXTENSIONS/qt-resource-rebuilder.so")" = "$wanted_qrr_sha256" ] || return 1
  [ "$(resource_inventory)" = "$wanted_resource_inventory_sha256" ] || return 1
  return 0
}

run_stock() {
  unset LD_PRELOAD XOVI_ROOT
  exec "$STOCK_BIN" "$@"
}

if [ "${1:-}" = --check ]; then
  shift
  if [ -e "$MARKER" ]; then say 'in-flight marker present; refusing injection'; exit 1; fi
  if check_compatibility; then say 'receipt accepted'; exit 0; fi
  say 'receipt or installed resources are not an exact approved match'; exit 1
fi

if [ -e "$MARKER" ]; then
  say 'stale in-flight marker; starting stock Xochitl'
  run_stock "$@"
fi
if ! check_compatibility; then
  say 'unsupported or unverified resources; starting stock Xochitl'
  run_stock "$@"
fi

umask 077
tmp="$MARKER.$$"
if ! printf '%s\n' "pid=$$" > "$tmp"; then
  say 'could not create in-flight marker; starting stock Xochitl'
  rm -f "$tmp" 2>/dev/null || true
  run_stock "$@"
fi
if ! mv -f "$tmp" "$MARKER"; then
  say 'could not install in-flight marker; starting stock Xochitl'
  rm -f "$tmp" 2>/dev/null || true
  run_stock "$@"
fi
if ! sync "$MARKER" 2>/dev/null; then
  say 'could not durably persist in-flight marker; starting stock Xochitl'
  run_stock "$@"
fi

child_pid=""
terminating=0
forward_term() {
  terminating=1
  [ -n "$child_pid" ] && kill -TERM "$child_pid" 2>/dev/null || true
}
trap forward_term INT TERM HUP

# The test-only override is intentionally unavailable unless explicitly opted in.
if [ "${XOVI_GUARD_TEST_MODE:-}" = 1 ] && [ -n "${XOVI_GUARD_CHILD_CMD:-}" ]; then
  child_cmd=$XOVI_GUARD_CHILD_CMD
else
  child_cmd=$STOCK_BIN
fi
[ "$terminating" = 0 ] || { rm -f "$MARKER"; exit 0; }
env LD_PRELOAD="$XOVI_SO" XOVI_ROOT="$INJECTED_XOVI_ROOT" \
  QML_DISABLE_DISK_CACHE=1 QML_XHR_ALLOW_FILE_WRITE=1 QML_XHR_ALLOW_FILE_READ=1 \
  "$child_cmd" "$@" &
child_pid=$!
[ "$terminating" = 0 ] || kill -TERM "$child_pid" 2>/dev/null || true
child_status=0
wait "$child_pid" || child_status=$?

if [ "$terminating" = 1 ]; then
  # A trapped signal can interrupt wait before the child has actually exited.
  # Do not clear the durable marker until that child is gone.
  while kill -0 "$child_pid" 2>/dev/null; do
    wait "$child_pid" || child_status=$?
  done
  child_pid=""
  rm -f "$MARKER"
  exit 0
fi
child_pid=""
if [ "$child_status" -eq 0 ]; then
  rm -f "$MARKER"
  exit 0
fi

# Preserve the marker so the next boot cannot re-inject after a crash.  The
# fallback is exec'd, so systemd observes the stock GUI rather than this error.
say "injected Xochitl exited status=$child_status; starting stock Xochitl"
run_stock "$@"
