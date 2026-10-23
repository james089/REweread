#!/bin/sh
set -eu
PATH=/usr/sbin:/usr/bin:/sbin:/bin
export PATH
umask 077
stamp=${1:-}
case "$stamp" in ''|*[!0-9]*) exit 2;; esac
stage=$(CDPATH= cd "$(dirname "$0")" && pwd)
root=/home/root/weread-downloader
extension=/home/root/xovi/extensions.d/weread-downloader.so
resources=/home/root/xovi/exthome/qt-resource-rebuilder
qmd="$resources/weread-downloader.qmd"
[ "$stage" = "$root.installing-$stamp" ]
[ "$(uname -m)" = aarch64 ]
[ "$(cat /etc/version)" = 20260827113527 ]
[ "$(sha256sum /usr/bin/xochitl | awk '{print $1}')" = 5ba79d1b5656df1a771217d29a8d3938c40256be53361b10a0d17cd4752807f4 ]
[ "$(sha256sum "$resources/hashtab" | awk '{print $1}')" = d947afca520b16a06efc0e5462d9c4db68928f2b4591ac67d59bc2e1f519c27d ]
test -f /home/root/xovi/extensions.d/appload.so
test -x /home/root/xovi/exthome/appload/koreader/luajit
test -f "$stage/helper/lib/download_manager.lua"
test -f /home/root/xovi/exthome/appload/koreader/plugins/weread.koplugin/weread/lib/client.lua
test ! -f /home/root/.local/share/weread-downloader/imports.lock
if pidof rm_weread_qt >/dev/null; then echo 'Close the existing reader before installing' >&2; exit 1; fi
if [ -e "$root" ]; then [ "$(cat "$root/.plugin-owner" 2>/dev/null)" = WereadDownloaderPlugin/v1 ]; fi
if [ -e "$extension" ] || [ -L "$extension" ]; then
  [ -L "$extension" ] && [ "$(readlink "$extension")" = "$root/weread-downloader.so" ]
fi
if [ -e "$qmd" ]; then test -f "$root/.plugin-owner"; grep -q 'wereadDownloaderSidebar' "$qmd"; fi
(cd "$stage"; sha256sum -c install.sha256 >/dev/null)
printf '%s\n' WereadDownloaderPlugin/v1 > "$stage/.plugin-owner"
backup=first
if [ -d "$root" ]; then backup="$root.before-$stamp"; test ! -e "$backup"; mv "$root" "$backup"; fi
mv "$stage" "$root"
prepared=0
trap '[ "$prepared" = 1 ] || sh "$root/rollback-device.sh" "$backup"' EXIT
"$root/weread-downloader-worker" status >/dev/null
ln -s "$root/weread-downloader.so" "$extension.installing"
mv -f "$extension.installing" "$extension"
cp "$root/sidebar.qmd" "$qmd.installing"
mv -f "$qmd.installing" "$qmd"
systemd-run --unit="weread-downloader-rollback-$stamp" --on-active=180 \
  /bin/sh "$root/rollback-device.sh" "$backup" >/dev/null
date "+%Y-%m-%d %H:%M:%S" > "$root/.install-restart-at"
prepared=1
printf '%s\n' 'plugin files installed; guarded UI restart pending'
