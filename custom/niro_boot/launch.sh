#!/bin/sh
set -eu
export NIRO_BOOT_ROOT="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
export XDG_RUNTIME_DIR=/var/tmp/weston
export QT_QPA_PLATFORM=wayland-egl
export QT_WAYLAND_DISABLE_WINDOWDECORATION=1
exec "$NIRO_BOOT_ROOT/niro-splash"
