#!/bin/sh
# Build on the original comma three with the existing AGNOS 12.8 toolchain.
set -eu
cd "$(dirname "$0")"
qt_include=$(pkg-config --variable=includedir Qt5Gui)
qt_version=$(pkg-config --modversion Qt5Gui)
g++ -std=c++17 -O2 -fPIC -I"$qt_include/QtGui/$qt_version/QtGui" \
  niro-splash.cpp -o niro-splash.next \
  $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core wayland-client)
mv niro-splash.next niro-splash
