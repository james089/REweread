#!/bin/sh
# An OS update must disable this exact-firmware patch rather than break Xochitl.
set -eu
root=/home/root/native-reader-defaults
if [ "$(cat /etc/version)" = 20260827113527 ] &&
   [ "$(sha256sum /usr/bin/xochitl | cut -c 1-64)" = 5ba79d1b5656df1a771217d29a8d3938c40256be53361b10a0d17cd4752807f4 ] &&
   [ "$(sha256sum /home/root/xovi/exthome/qt-resource-rebuilder/hashtab | cut -c 1-64)" = d947afca520b16a06efc0e5462d9c4db68928f2b4591ac67d59bc2e1f519c27d ]; then
    exit 0
fi
for link in /home/root/xovi/extensions.d/native-reader-defaults.so /home/root/xovi/exthome/qt-resource-rebuilder/native-reader-defaults.qmd /home/root/xovi/exthome/qt-resource-rebuilder/native-reader-menu.qmd; do
    case "$(readlink "$link" 2>/dev/null || true)" in "$root/"*) rm -f "$link";; esac
done
