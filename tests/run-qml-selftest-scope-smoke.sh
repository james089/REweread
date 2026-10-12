#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${QT_PREFIX:?Set QT_PREFIX to a Qt installation prefix}"
QT_MAJOR="${QT_MAJOR:-6}"
QT_INCLUDE="${QT_INCLUDE:-$QT_PREFIX/include/qt}"
work=$(mktemp -d "${TMPDIR:-/tmp}/reweread-qml-scope.XXXXXX")
trap 'rm -rf "$work"' EXIT
"${CXX:-c++}" -std=c++17 -fPIC -I"$QT_INCLUDE" -I"$QT_INCLUDE/QtCore" -I"$QT_INCLUDE/QtQml" \
  tests/qml_selftest_scope_smoke.cpp -L"$QT_PREFIX/lib" -Wl,-rpath,"$QT_PREFIX/lib" \
  -l"Qt${QT_MAJOR}Core" -l"Qt${QT_MAJOR}Qml" -o "$work/qml-scope"
"$work/qml-scope"
