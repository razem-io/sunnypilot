#!/bin/sh
set -eu
cd "$(dirname "$0")"
g++ -std=c++17 -O2 -fPIC -shared native_entry.cpp -o libniro-entry.so.next $(pkg-config --cflags --libs Qt5Widgets Qt5Core Qt5Gui)
mv libniro-entry.so.next libniro-entry.so
