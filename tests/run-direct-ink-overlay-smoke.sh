#!/usr/bin/env bash
# Isolated QtGui behavior test; no device, network, or vendor plugin required.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${QT_PREFIX:?Set QT_PREFIX to a Qt installation prefix}"
QT_MAJOR="${QT_MAJOR:-6}"
QT_INCLUDE="${QT_INCLUDE:-$QT_PREFIX/include/qt}"
CXX="${CXX:-c++}"
work=$(mktemp -d "${TMPDIR:-/tmp}/reweread-direct-ink-smoke.XXXXXX")
trap 'rm -rf "$work"' EXIT
"$CXX" -std=c++17 -fPIC -I"$QT_INCLUDE" -I"$QT_INCLUDE/QtCore" -I"$QT_INCLUDE/QtGui" \
  -Iapps/weread-qt apps/weread-qt/direct_ink_framebuffer.cpp \
  tests/direct_ink_overlay_smoke.cpp -L"$QT_PREFIX/lib" -Wl,-rpath,"$QT_PREFIX/lib" \
  -l"Qt${QT_MAJOR}Gui" -l"Qt${QT_MAJOR}Core" -ldl -o "$work/direct-ink-overlay"
"$work/direct-ink-overlay"
echo "direct ink overlay behavior ok"
