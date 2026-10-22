#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QT_PREFIX="${QT_PREFIX:-/opt/anaconda3}"
QT_INCLUDE="${QT_INCLUDE:-$QT_PREFIX/include/qt}"
QR_SOURCE="$ROOT/../downloads/sources/QR-Code-generator-1.8.0/cpp"
work=$(mktemp -d "${TMPDIR:-/tmp}/weread-downloader-ui.XXXXXX")
trap 'rm -rf "$work"' EXIT
"$QT_PREFIX/bin/moc" -I"$QT_INCLUDE" "$ROOT/native/controller.h" -o "$work/moc_controller.cpp"
"${CXX:-c++}" -std=c++17 -fPIC -Wall -Wextra -Werror \
  -I"$QT_INCLUDE" -I"$QT_INCLUDE/QtCore" -I"$QT_INCLUDE/QtGui" -I"$QT_INCLUDE/QtQuick" -I"$QT_INCLUDE/QtQml" -I"$QT_INCLUDE/QtNetwork" -I"$QT_INCLUDE/QtTest" \
  -I"$ROOT/native" -I"$QR_SOURCE" \
  "$ROOT/native/controller.cpp" "$ROOT/native/cover_store.cpp" "$ROOT/tests/controller_test.cpp" "$work/moc_controller.cpp" "$QR_SOURCE/qrcodegen.cpp" \
  -L"$QT_PREFIX/lib" -Wl,-rpath,"$QT_PREFIX/lib" -lQt5Core -lQt5Gui -lQt5Qml -lQt5Quick -lQt5Network -lQt5Test -o "$work/ui-test"
mkdir -p "$ROOT/build"
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software "$work/ui-test" "$ROOT/qml/DownloaderPage.qml" "$ROOT/build/synthetic-ui.png"
