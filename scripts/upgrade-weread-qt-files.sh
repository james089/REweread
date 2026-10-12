#!/usr/bin/env bash
# Update an existing installation only. No root remount, launcher reinstall,
# credential access or data-directory mutation. Launch/test separately.
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MOVE_HOST="${MOVE_HOST:-root@10.11.99.1}"
REMOTE_DIR=/home/root/weread-qt
STAMP="$(date +%Y%m%d-%H%M%S)-$$"
STAGE="$REMOTE_DIR.staging-$STAMP"
BACKUP="$REMOTE_DIR.backup-$STAMP"
BIN="$ROOT_DIR/apps/weread-qt/build/rm_weread_qt"
SSH=(ssh -o BatchMode=yes -o ConnectTimeout=8)
SCP=(scp -O -o BatchMode=yes -o ConnectTimeout=8)
# Optional known-host identity for the same verified device over USB.
if [[ -n "${MOVE_HOST_KEY_ALIAS:-}" ]]; then
  SSH+=(-o "HostKeyAlias=$MOVE_HOST_KEY_ALIAS")
  SCP+=(-o "HostKeyAlias=$MOVE_HOST_KEY_ALIAS")
fi
test -s "$BIN"
"${SSH[@]}" "$MOVE_HOST" "test -x '$REMOTE_DIR/rm_weread_qt' && test -d '$REMOTE_DIR/fonts' && test ! -e '$STAGE' && test ! -e '$BACKUP'"
"${SSH[@]}" "$MOVE_HOST" "REMOTE_DIR='$REMOTE_DIR' sh -s" <<'SPACE'
set -eu
required=$(du -sk "$REMOTE_DIR" | awk '{print $1}')
available=$(df -Pk "$REMOTE_DIR" | awk 'END {print $4}')
# Include the replacement executable and room for normal app writes.
[ "$available" -gt "$((required + 131072))" ] || {
  echo 'Insufficient storage for a reversible application upgrade' >&2
  exit 1
}
SPACE
staged=0
cleanup() {
  if [[ "$staged" == 1 ]]; then
    "${SSH[@]}" "$MOVE_HOST" "rm -rf '$STAGE'" || true
  fi
}
trap cleanup EXIT
# Retain all installed font/dependency files without downloading them again.
"${SSH[@]}" "$MOVE_HOST" "cp -a '$REMOTE_DIR' '$STAGE'"
staged=1
"${SCP[@]}" "$BIN" "$MOVE_HOST:$STAGE/rm_weread_qt"
"${SCP[@]}" "$ROOT_DIR/scripts/weread-qt-session.sh" "$MOVE_HOST:$STAGE/weread-qt-session.sh"
"${SCP[@]}" -r "$ROOT_DIR/apps/weread-move/lib" "$MOVE_HOST:$STAGE/helper/"
"${SCP[@]}" "$ROOT_DIR"/apps/weread-move/tools/*.lua "$MOVE_HOST:$STAGE/helper/tools/"
expected="$(shasum -a 256 "$BIN" | awk '{print $1}')"
actual="$("${SSH[@]}" "$MOVE_HOST" "sha256sum '$STAGE/rm_weread_qt'" | awk '{print $1}')"
[[ "$expected" == "$actual" ]] || { echo 'Staged binary checksum mismatch' >&2; exit 1; }
"${SSH[@]}" "$MOVE_HOST" "STAGE='$STAGE' BACKUP='$BACKUP' REMOTE_DIR='$REMOTE_DIR' sh -s" <<'REMOTE'
set -eu
committed=0
restore_installation() {
  if [ "$committed" = 0 ] && [ ! -e "$REMOTE_DIR" ] && [ -d "$BACKUP" ]; then
    mv "$BACKUP" "$REMOTE_DIR" || echo 'Restore needed from retained application backup' >&2
  fi
}
trap restore_installation EXIT
trap 'exit 1' HUP INT TERM
# Do not interfere with the official client. It is stopped separately for UI tests.
systemctl stop rm-weread-qt.service rm-weread-smoke.service 2>/dev/null || true
pids="$(pidof rm_weread_qt 2>/dev/null || true)"
[ -z "$pids" ] || kill $pids
for _ in 1 2 3 4 5; do
  pidof rm_weread_qt >/dev/null 2>&1 || break
  sleep 1
done
if pidof rm_weread_qt >/dev/null 2>&1; then
  echo 'Existing application has not exited; installation unchanged' >&2
  exit 1
fi
chmod +x "$STAGE/rm_weread_qt" "$STAGE/weread-qt-session.sh"
mv "$REMOTE_DIR" "$BACKUP"
if ! mv "$STAGE" "$REMOTE_DIR"; then
  mv "$BACKUP" "$REMOTE_DIR"
  exit 1
fi
committed=1
REMOTE
staged=0
echo "Application files updated; retained backup: $BACKUP"
echo "Binary SHA256: $expected"
echo 'Launcher, system configuration, fonts and user data preserved. UI validation still required.'
