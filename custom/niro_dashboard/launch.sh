#!/bin/sh
set -eu
export XDG_RUNTIME_DIR=/var/tmp/weston
export QT_QPA_PLATFORM=wayland-egl
# Use the same Qualcomm GLES driver as the stock UI, not the desktop Mesa dispatcher.
export LD_PRELOAD=/usr/lib/aarch64-linux-gnu/libEGL_adreno.so:/usr/lib/aarch64-linux-gnu/libGLESv2_adreno.so
export QT_WAYLAND_DISABLE_WINDOWDECORATION=1
mkdir -p /data/local/niro-dashboard
cd "$(dirname "$0")"
export PWD="$PWD"
exec ./niro-dashboard "$@"
