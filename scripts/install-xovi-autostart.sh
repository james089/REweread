#!/usr/bin/env bash
set -euo pipefail
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MOVE_HOST="${MOVE_HOST:-root@10.11.99.1}"
SSH=(ssh -o BatchMode=yes -o ConnectTimeout=8)
SCP=(scp -O -o BatchMode=yes -o ConnectTimeout=8)
if [[ -n "${MOVE_HOST_KEY_ALIAS:-}" ]]; then
  SSH+=(-o "HostKeyAlias=$MOVE_HOST_KEY_ALIAS")
  SCP+=(-o "HostKeyAlias=$MOVE_HOST_KEY_ALIAS")
fi

# Check before any write/remount/restart; file presence alone is not compatibility.
"${SSH[@]}" "$MOVE_HOST" 'sh -s -- --check' < "$ROOT_DIR/scripts/xovi-guarded-start.sh"
[[ "${CHECK_ONLY:-0}" != 1 ]] || exit 0
echo 'Installing guarded startup; root will briefly be writable and the UI will restart.'
"${SCP[@]}" "$ROOT_DIR/scripts/xovi-guarded-start.sh" "$MOVE_HOST:/home/root/xovi/reweread-guard.sh.pending"
"${SSH[@]}" "$MOVE_HOST" 'sh -s' <<'REMOTE'
set -eu
guard=/home/root/xovi/reweread-guard.sh
dropin=/usr/lib/systemd/system/xochitl.service.d/99-xovi-appload.conf
sh "$guard.pending" --check
chmod 0755 "$guard.pending"
[ ! -f "$guard" ] || cp -p "$guard" "$guard.previous"
mv "$guard.pending" "$guard"
remounted_rw=0
cleanup() {
  if [ "$remounted_rw" = 1 ]; then
    sync
    mount -o remount,ro / || true
  fi
}
trap cleanup EXIT HUP INT TERM
mount -o remount,rw /
remounted_rw=1
mkdir -p "$(dirname "$dropin")"
[ ! -f "$dropin" ] || cp -p "$dropin" /home/root/xovi/reweread-dropin.previous
cat > "$dropin.pending" <<'EOF'
[Unit]
RequiresMountsFor=/home/root/xovi

[Service]
# The supervisor must never inherit the injected library.
Environment="LD_PRELOAD="
Environment="XOVI_ROOT="
ExecStart=
ExecStart=/bin/sh /home/root/xovi/reweread-guard.sh --system
EOF
chmod 0644 "$dropin.pending"
mv "$dropin.pending" "$dropin"
sync
mount -o remount,ro /
remounted_rw=0
systemctl daemon-reload
systemctl restart xochitl
systemctl is-active xochitl
# Service health is not proof of UI/firmware compatibility.
REMOTE
