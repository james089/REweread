#!/usr/bin/env bash
# Installation is explicitly requested by the operator; no password collection.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MOVE_HOST="${MOVE_HOST:-root@10.11.99.1}"
MOVE_SSH_KEY="${MOVE_SSH_KEY:-$HOME/.ssh/weread_move_ed25519}"
[[ "$MOVE_HOST" =~ ^[A-Za-z0-9_@.:-]+$ && "$MOVE_HOST" != -* ]]
SSH=(ssh -i "$MOVE_SSH_KEY" -o IdentitiesOnly=yes -o BatchMode=yes -o ConnectTimeout=5)
SCP=(scp -i "$MOVE_SSH_KEY" -o IdentitiesOnly=yes -o BatchMode=yes -o ConnectTimeout=5)
node "$ROOT/../scripts/check-repository.mjs"
python3 "$ROOT/scripts/verify-replay.py" "$ROOT"
stamp=$(date -u +%Y%m%d%H%M%S)
stage="/home/root/weread-downloader.installing-$stamp"
package="$ROOT/build/install-$stamp"
mkdir -p "$package/lua" "$package/helper/lib" "$package/helper/tools"
cp "$ROOT/build/sdk/weread-downloader.so" "$ROOT/build/sdk/weread-downloader-worker" "$package/"
cp "$ROOT/lua/"*.lua "$package/lua/"
cp "$ROOT/native/sidebar.qmd" "$package/sidebar.qmd"
cp "$ROOT/scripts/install-device.sh" "$ROOT/scripts/rollback-device.sh" "$package/"
cp "$ROOT/../apps/weread-move/lib/"*.lua "$package/helper/lib/"
cp "$ROOT/../apps/weread-move/tools/login-qr.lua" "$package/helper/tools/"
python3 - "$package" <<'PY'
import hashlib, pathlib, sys
root = pathlib.Path(sys.argv[1])
files = ['weread-downloader.so', 'weread-downloader-worker', 'lua/adapter.lua', 'sidebar.qmd', 'install-device.sh', 'rollback-device.sh']
files += sorted(str(p.relative_to(root)) for p in (root / 'helper').rglob('*.lua'))
files += sorted(str(p.relative_to(root)) for p in (root / 'lua').glob('*.lua') if p.name != 'adapter.lua')
(root / 'install.sha256').write_text(''.join(hashlib.sha256((root / f).read_bytes()).hexdigest() + '  ' + f + '\n' for f in files))
PY
"${SSH[@]}" "$MOVE_HOST" "mkdir -m 700 '$stage' && mkdir -m 700 '$stage/lua'"
"${SCP[@]}" "$package/"*.so "$package/weread-downloader-worker" "$package/"*.sh "$package/"*.qmd "$package/install.sha256" "$MOVE_HOST:$stage/"
"${SCP[@]}" "$package/lua/"*.lua "$MOVE_HOST:$stage/lua/"
"${SCP[@]}" -r "$package/helper" "$MOVE_HOST:$stage/helper"
# Commit the guarded file transaction in a detached job. Restart Xochitl only
# after that job exits, so the service restart cannot kill its own rollback trap.
"${SSH[@]}" "$MOVE_HOST" "rm -f /home/root/weread-downloader/.install-restart-at && systemd-run --unit='weread-downloader-install-$stamp' --collect /bin/sh '$stage/install-device.sh' '$stamp'" || true
prepared=0
for attempt in {1..30}; do
  if "${SSH[@]}" "$MOVE_HOST" "test -s /home/root/weread-downloader/.install-restart-at"; then prepared=1; break; fi
  sleep 1
done
[[ "$prepared" = 1 ]]
# The device may close this SSH session while restarting Xochitl. The guarded
# rollback timer remains armed until both host and sidebar bindings are seen.
"${SSH[@]}" "$MOVE_HOST" "systemctl restart xochitl" || true
# Confirm fresh registration and host/sidebar QML bindings without returning
# raw device logs. Retry SSH across the expected restart disconnect.
ready=0
for attempt in {1..90}; do
  if "${SSH[@]}" "$MOVE_HOST" "set -e; since=\$(cat /home/root/weread-downloader/.install-restart-at); systemctl is-active --quiet xochitl; journalctl -u xochitl --since \"\$since\" --no-pager | grep -q 'WereadDownloaderPlugin: host-ready'; journalctl -u xochitl --since \"\$since\" --no-pager | grep -q 'WereadDownloaderPlugin: sidebar-ready'; test ! -f /home/root/weread-downloader/.install-rolled-back; printf '%s\\n' '$stamp' > /home/root/weread-downloader/.install-ok; systemctl stop 'weread-downloader-rollback-$stamp.timer' || true"; then ready=1; break; fi
  sleep 2
done
[[ "$ready" = 1 ]]
node "$ROOT/../scripts/check-repository.mjs"
echo 'Installed native sidebar plugin; account login and a selected real-book import still need user acceptance.'
