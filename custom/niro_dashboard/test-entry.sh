#!/bin/sh
set -eu
cd "$(dirname "$0")"
moc test_entry.cpp -o test_entry.moc
g++ -std=c++17 -fPIC test_entry.cpp -o /data/local/niro-dashboard/test-entry $(pkg-config --cflags --libs Qt5Widgets Qt5Core Qt5Gui)
QT_QPA_PLATFORM=offscreen NIRO_ENTRY_TEST=1 LD_PRELOAD="$PWD/libniro-entry.so" /data/local/niro-dashboard/test-entry
