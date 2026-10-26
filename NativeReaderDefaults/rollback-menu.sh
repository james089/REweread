#!/bin/sh
set -eu
root=/home/root/native-reader-defaults
stamp=${1:-}
case "$stamp" in ''|*[!0-9]*) exit 2;; esac
[ "$(cat "$root/.menu-install-ok" 2>/dev/null || true)" != "$stamp" ] || exit 0
[ "$(cat "$root/.owner")" = NativeReaderDefaults/v1 ]
link=/home/root/xovi/exthome/qt-resource-rebuilder/native-reader-menu.qmd
if [ "$(readlink "$link" 2>/dev/null || true)" = "$root/menu.qmd" ]; then rm -f "$link"; fi
backup="$root/before-menu-$stamp"
cp "$backup/pre-start.sh" "$root/pre-start.sh"
cp "$backup/install.sha256" "$root/install.sha256"
printf '%s\n' "$stamp" > "$root/.menu-rolled-back"
systemctl restart xochitl
