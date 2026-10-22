"""Verify an exact-firmware local QML replay without packaging stock resources."""
import hashlib
import pathlib
import sys

root = pathlib.Path(sys.argv[1]).resolve()
resources = root.parent / 'downloads/wrdp-device-resources'
if hashlib.sha256((resources / 'xochitl').read_bytes()).hexdigest() != '5ba79d1b5656df1a771217d29a8d3938c40256be53361b10a0d17cd4752807f4':
    raise SystemExit('Unexpected firmware binary')
if hashlib.sha256((resources / 'hashtab').read_bytes()).hexdigest() != 'd947afca520b16a06efc0e5462d9c4db68928f2b4591ac67d59bc2e1f519c27d':
    raise SystemExit('Unexpected resource map')
sidebar = (root / 'build/replay/qml/device/view/navigator/Sidebar.qml').read_text()
main = (root / 'build/replay/qml/device/view/main/MainView.qml').read_text()
for token in ['wereadDownloaderSidebar', 'openAppLoad', 'Downloader.placeBelowAppLoad']:
    if token not in sidebar:
        raise SystemExit('Sidebar replay is missing a required peer or plugin anchor')
for token in ['wereadDownloaderHost', 'Downloader.attachSettings', 'DownloaderPage.qml']:
    if token not in main:
        raise SystemExit('Main view replay is incomplete')
# Both markers refer to the current patch, not a stale prior build.
patch = (root / 'native/sidebar.qmd').read_bytes()
record = root / 'build/replayed-patch.sha256'
if not record.exists() or record.read_text().strip() != hashlib.sha256(patch).hexdigest():
    raise SystemExit('Replay this exact patch again before installation')
print('Exact-firmware sidebar/host peer replay verified')
