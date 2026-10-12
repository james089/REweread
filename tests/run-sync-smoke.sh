#!/usr/bin/env bash
# Isolated QtCore behavior tests; no device, network or real account is used.
set -euo pipefail
cd "$(dirname "$0")/.."
: "${QT_PREFIX:?Set QT_PREFIX to a Qt installation prefix}"
QT_MAJOR="${QT_MAJOR:-6}"
QT_INCLUDE="${QT_INCLUDE:-$QT_PREFIX/include/qt}"
MOC="${MOC:-$QT_PREFIX/bin/moc}"
CXX="${CXX:-c++}"
work=$(mktemp -d "${TMPDIR:-/tmp}/reweread-sync-smoke.XXXXXX")
trap 'rm -rf "$work"' EXIT
for pair in \
  reading_time_store:reading_time_store_smoke.cpp \
  progress_sync_store:progress-sync-store-smoke.cpp \
  notes_store:personal_notes_cache_smoke.cpp \
  download_store:download_store_result_protocol_smoke.cpp; do
  store=${pair%%:*}
  testfile=${pair#*:}
  "$MOC" -f "$store.h" "apps/weread-qt/$store.h" -o "$work/moc_$store.cpp"
  "$CXX" -std=c++17 -fPIC -I"$QT_INCLUDE" -I"$QT_INCLUDE/QtCore" \
    -Iapps/weread-qt "apps/weread-qt/$store.cpp" "tests/$testfile" \
    "$work/moc_$store.cpp" -L"$QT_PREFIX/lib" -Wl,-rpath,"$QT_PREFIX/lib" \
    -l"Qt${QT_MAJOR}Core" -o "$work/$store"
  "$work/$store"
  echo "$store behavior ok"
done
