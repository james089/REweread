#!/bin/sh
# Removes/restores only this plugin. Never touches a library or account cache.
set -eu
root=/home/root/weread-downloader
extension=/home/root/xovi/extensions.d/weread-downloader.so
qmd=/home/root/xovi/exthome/qt-resource-rebuilder/weread-downloader.qmd
backup=${1:-first}
[ ! -f "$root/.install-ok" ] || exit 0
[ "$(cat "$root/.plugin-owner" 2>/dev/null)" = WereadDownloaderPlugin/v1 ] || exit 1
case "$backup" in first|/home/root/weread-downloader.before-[0-9]*) ;; *) exit 1;; esac
if [ -L "$extension" ] && [ "$(readlink "$extension")" = "$root/weread-downloader.so" ]; then rm -f "$extension"; fi
if [ -f "$qmd" ] && grep -q 'wereadDownloaderSidebar' "$qmd"; then rm -f "$qmd"; fi
if [ "$backup" != first ]; then
  [ "$(cat "$backup/.plugin-owner" 2>/dev/null)" = WereadDownloaderPlugin/v1 ] || exit 1
  failed="$root.failed-$(date +%s)"
  mv "$root" "$failed"
  mv "$backup" "$root"
  ln -s "$root/weread-downloader.so" "$extension"
  cp "$root/sidebar.qmd" "$qmd"
else
  printf '%s\n' 'automatic rollback completed' > "$root/.install-rolled-back"
fi
systemctl restart xochitl
