#!/bin/sh
set -eu
cd /data/openpilot/custom/niro_dashboard
# Strip the native UI extension from the separate dashboard process.
unset LD_PRELOAD
exec ./launch.sh "$@" >>/data/local/niro-dashboard/runtime.log 2>&1
