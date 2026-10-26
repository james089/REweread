#!/bin/sh
# Installs/upgrades menu resources in the already verified defaults installation.
set -eu
umask 077
root=/home/root/native-reader-defaults
stamp=${1:-}
case "$stamp" in ''|*[!0-9]*) exit 2;; esac
stage=$(CDPATH= cd "$(dirname "$0")" && pwd)
[ "$stage" = "$root.menu-installing-$stamp" ]
[ "$(cat "$root/.owner")" = NativeReaderDefaults/v1 ]
test -s "$root/.install-ok"
[ "$(cat /etc/version)" = 20260827113527 ]
[ "$(sha256sum /usr/bin/xochitl | cut -c 1-64)" = 5ba79d1b5656df1a771217d29a8d3938c40256be53361b10a0d17cd4752807f4 ]
[ "$(sha256sum /home/root/xovi/exthome/qt-resource-rebuilder/hashtab | cut -c 1-64)" = d947afca520b16a06efc0e5462d9c4db68928f2b4591ac67d59bc2e1f519c27d ]
test ! -f /home/root/.local/share/weread-downloader/imports.lock
! pidof rm_weread_qt >/dev/null
link=/home/root/xovi/exthome/qt-resource-rebuilder/native-reader-menu.qmd
if [ -e "$link" ] || [ -L "$link" ]; then
    [ -L "$link" ] && [ "$(readlink "$link")" = "$root/menu.qmd" ]
    test -f "$root/menu.qmd"
    test -s "$root/.menu-install-ok"
fi
(cd "$stage"; sha256sum -c install.sha256 >/dev/null)
(cd "$root"; sha256sum -c install.sha256 >/dev/null)
backup="$root/before-menu-$stamp"
mkdir "$backup"
cp "$root/pre-start.sh" "$root/install.sha256" "$backup/"
if [ -f "$root/menu.qmd" ]; then cp "$root/menu.qmd" "$backup/menu.qmd"; fi
cp "$stage/rollback-menu.sh" "$root/rollback-menu.sh"
systemd-run --unit="native-reader-menu-rollback-$stamp" --on-active=180 /bin/sh "$root/rollback-menu.sh" "$stamp" >/dev/null
cp "$stage/pre-start.sh" "$root/pre-start.sh.installing"
chmod +x "$root/pre-start.sh.installing"
mv "$root/pre-start.sh.installing" "$root/pre-start.sh"
cp "$stage/menu.qmd" "$root/menu.qmd.installing"
mv "$root/menu.qmd.installing" "$root/menu.qmd"
if [ ! -L "$link" ]; then ln -s "$root/menu.qmd" "$link"; fi
(cd "$root"; sha256sum defaults.qmd install-device.sh native-reader-defaults.so pre-start.sh rollback-device.sh menu.qmd > install.sha256)
date '+%Y-%m-%d %H:%M:%S' > "$root/.menu-restart-at"
printf '%s\n' 'menu files prepared; guarded soft restart pending'
