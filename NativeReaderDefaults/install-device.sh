#!/bin/sh
set -eu
umask 077
stamp=${1:-}
case "$stamp" in ''|*[!0-9]*) exit 2;; esac
root=/home/root/native-reader-defaults
stage=$(CDPATH= cd "$(dirname "$0")" && pwd)
[ "$stage" = "$root.installing-$stamp" ]
[ ! -e "$root" ] # First install only. Never overwrite an unreviewed installation.
[ "$(uname -m)" = aarch64 ]
sh "$stage/pre-start.sh"
# pre-start disables an existing patch on mismatch; installation must reject it.
[ "$(cat /etc/version)" = 20260827113527 ]
[ "$(sha256sum /usr/bin/xochitl | cut -c 1-64)" = 5ba79d1b5656df1a771217d29a8d3938c40256be53361b10a0d17cd4752807f4 ]
[ "$(sha256sum /home/root/xovi/exthome/qt-resource-rebuilder/hashtab | cut -c 1-64)" = d947afca520b16a06efc0e5462d9c4db68928f2b4591ac67d59bc2e1f519c27d ]
test ! -f /home/root/.local/share/weread-downloader/imports.lock
! pidof rm_weread_qt >/dev/null
for link in /home/root/xovi/extensions.d/native-reader-defaults.so /home/root/xovi/exthome/qt-resource-rebuilder/native-reader-defaults.qmd /home/root/xovi/exthome/qt-resource-rebuilder/native-reader-menu.qmd /home/root/xovi/scripts/pre-start/31-native-reader-defaults.sh; do
    [ ! -e "$link" ] && [ ! -L "$link" ]
done
(cd "$stage"; sha256sum -c install.sha256 >/dev/null)
printf '%s\n' NativeReaderDefaults/v1 > "$stage/.owner"
mv "$stage" "$root"
systemd-run --unit="native-reader-defaults-rollback-$stamp" --on-active=180 /bin/sh "$root/rollback-device.sh" >/dev/null
ln -s "$root/native-reader-defaults.so" /home/root/xovi/extensions.d/native-reader-defaults.so
ln -s "$root/defaults.qmd" /home/root/xovi/exthome/qt-resource-rebuilder/native-reader-defaults.qmd
if [ -f "$root/menu.qmd" ]; then
    ln -s "$root/menu.qmd" /home/root/xovi/exthome/qt-resource-rebuilder/native-reader-menu.qmd
fi
ln -s "$root/pre-start.sh" /home/root/xovi/scripts/pre-start/31-native-reader-defaults.sh
date '+%Y-%m-%d %H:%M:%S' > "$root/.restart-at"
printf '%s\n' 'default patch prepared; guarded soft restart pending'
