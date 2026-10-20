#!/bin/sh
# Wait for the normal UI before showing the optional dashboard.
set -eu
cd "$(dirname "$0")"
for attempt in $(seq 1 120); do
  if ../niro_boot/niro-splash --check >/dev/null 2>&1; then exec ./launch.sh; fi
  sleep 1
done
