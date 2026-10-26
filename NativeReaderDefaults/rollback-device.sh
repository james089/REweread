#!/bin/sh
set -eu
root=/home/root/native-reader-defaults
[ ! -f "$root/.install-ok" ] || exit 0
[ "$(cat "$root/.owner" 2>/dev/null)" = NativeReaderDefaults/v1 ] || exit 1
for link in /home/root/xovi/extensions.d/native-reader-defaults.so /home/root/xovi/exthome/qt-resource-rebuilder/native-reader-defaults.qmd /home/root/xovi/exthome/qt-resource-rebuilder/native-reader-menu.qmd /home/root/xovi/scripts/pre-start/31-native-reader-defaults.sh; do
    case "$(readlink "$link" 2>/dev/null || true)" in "$root/"*) rm -f "$link";; esac
done
printf '%s\n' 'default patch disabled after incomplete installation' > "$root/.rolled-back"
systemctl restart xochitl
