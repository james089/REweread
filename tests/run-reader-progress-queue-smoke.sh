#!/usr/bin/env bash
# Isolated ReaderStore eraser test. Uses QTemporaryDir and never touches a
# device, network, account, or the normal rm-weread data location.
set -euo pipefail
cd "$(dirname "$0")/.."

QT_PREFIX="${QT_PREFIX:-/opt/anaconda3}"
QT_MAJOR="${QT_MAJOR:-5}"
QT_INCLUDE="${QT_INCLUDE:-$QT_PREFIX/include/qt}"
MOC="${MOC:-$QT_PREFIX/bin/moc}"
CXX="${CXX:-c++}"
work=$(mktemp -d "${TMPDIR:-/tmp}/reweread-free-ink-eraser.XXXXXX")
trap 'rm -rf "$work"' EXIT

"$MOC" -f reader_store.h apps/weread-qt/reader_store.h -o "$work/moc_reader_store.cpp"
"$CXX" -std=c++17 -fPIC \
  -I"$QT_INCLUDE" -I"$QT_INCLUDE/QtCore" -I"$QT_INCLUDE/QtGui" -Iapps/weread-qt \
  apps/weread-qt/reader_store.cpp tests/reader_progress_queue_smoke.cpp "$work/moc_reader_store.cpp" \
  -L"$QT_PREFIX/lib" -Wl,-rpath,"$QT_PREFIX/lib" \
  -l"Qt${QT_MAJOR}Core" -l"Qt${QT_MAJOR}Gui" -o "$work/reader_progress_queue"
"$work/reader_progress_queue"
echo "reader progress queue behavior ok"
