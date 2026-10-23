#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
QT_PREFIX="${QT_PREFIX:-/opt/anaconda3}"
QT_MAJOR="${QT_MAJOR:-5}"
QT_INCLUDE="${QT_INCLUDE:-$QT_PREFIX/include/qt}"
work=$(mktemp -d "${TMPDIR:-/tmp}/weread-downloader-core.XXXXXX")
trap 'rm -rf "$work"' EXIT
python3 "$ROOT/tests/make_fixtures.py" "$work/fixtures"
"${CXX:-c++}" -std=c++17 -fPIC -Wall -Wextra -Werror \
  -I"$QT_INCLUDE" -I"$QT_INCLUDE/QtCore" -I"$QT_INCLUDE/QtXml" -I"$ROOT/src" \
  "$ROOT/src/epub.cpp" "$ROOT/src/import_registry.cpp" "$ROOT/tests/core_test.cpp" \
  -L"$QT_PREFIX/lib" -Wl,-rpath,"$QT_PREFIX/lib" \
  -l"Qt${QT_MAJOR}Core" -l"Qt${QT_MAJOR}Xml" -lz -o "$work/core-test"
"$work/core-test" "$work/fixtures"
"${CXX:-c++}" -std=c++17 -fPIC -Wall -Wextra -Werror \
  -I"$QT_INCLUDE" -I"$QT_INCLUDE/QtCore" -I"$ROOT/src" \
  "$ROOT/src/native_address.cpp" "$ROOT/tests/address_test.cpp" \
  -L"$QT_PREFIX/lib" -Wl,-rpath,"$QT_PREFIX/lib" -l"Qt${QT_MAJOR}Core" -o "$work/address-test"
"$work/address-test"

"${CXX:-c++}" -std=c++17 -fPIC -Wall -Wextra -Werror \
  -I"$QT_INCLUDE" -I"$QT_INCLUDE/QtCore" -I"$ROOT/native" \
  "$ROOT/tests/reading_time_store_test.cpp" \
  -L"$QT_PREFIX/lib" -Wl,-rpath,"$QT_PREFIX/lib" -l"Qt${QT_MAJOR}Core" -o "$work/reading-time-test"
"$work/reading-time-test"

"${CXX:-c++}" -std=c++17 -fPIC -Wall -Wextra -Werror \
  -I"$QT_INCLUDE" -I"$QT_INCLUDE/QtCore" -I"$QT_INCLUDE/QtNetwork" -I"$ROOT/src" \
  "$ROOT/tests/upload_test.cpp" -L"$QT_PREFIX/lib" -Wl,-rpath,"$QT_PREFIX/lib" \
  -l"Qt${QT_MAJOR}Core" -l"Qt${QT_MAJOR}Network" -o "$work/upload-test"
"$work/upload-test"

"${CXX:-c++}" -std=c++17 -fPIC -Wall -Wextra -Werror \
  -I"$QT_INCLUDE" -I"$QT_INCLUDE/QtCore" -I"$ROOT/src" \
  "$ROOT/tests/exit_sync_state_test.cpp" \
  -L"$QT_PREFIX/lib" -Wl,-rpath,"$QT_PREFIX/lib" -l"Qt${QT_MAJOR}Core" -o "$work/exit-state-test"
"$work/exit-state-test"
