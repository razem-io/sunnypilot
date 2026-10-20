#!/bin/sh
set -eu
cd "$(dirname "$0")"
qt_include=$(pkg-config --variable=includedir Qt5Gui)
qt_version=$(pkg-config --modversion Qt5Gui)
g++ -std=c++17 -O2 -fPIC -I"$qt_include/QtGui/$qt_version/QtGui" dashboard.cpp -o niro-dashboard.next $(pkg-config --cflags --libs Qt5Widgets Qt5Gui Qt5Core Qt5Network wayland-client)
mv niro-dashboard.next niro-dashboard
