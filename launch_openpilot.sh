#!/usr/bin/env bash
set -euo pipefail
IFS=$'\n\t'

# This test branch never falls back to an older launcher/UI on startup errors.
C3_LAUNCH_SH="./sunnypilot/system/hardware/c3/launch_chffrplus.sh"

MODEL="$(tr -d '\0' < "/sys/firmware/devicetree/base/model")"
export MODEL

if [ "$MODEL" = "comma tici" ]; then
  # Force a failure if the launcher doesn't exist
  [ -x "$C3_LAUNCH_SH" ] || false

  # Personal C3 splash runs independently; the normal launcher never waits.
  export NIRO_BOOT_ROOT="$PWD/custom/niro_boot"
  export NIRO_BOOT_STATE_DIR="/data/local/niro-boot"
  if [ -x "$NIRO_BOOT_ROOT/niro-splash" ] && mkdir -p "$NIRO_BOOT_STATE_DIR"; then
    export PATH="$NIRO_BOOT_ROOT/bin:$PATH"
    /usr/bin/setsid "$NIRO_BOOT_ROOT/launch.sh" </dev/null >>"$NIRO_BOOT_STATE_DIR/boot.log" 2>&1 &
  fi

  # If it exists, run it
  exec "$C3_LAUNCH_SH"
fi

exec ./launch_chffrplus.sh
