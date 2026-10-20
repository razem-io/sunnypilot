#!/bin/bash
set -eu
NIRO_RUNTIME=$(cd "$(dirname "$0")/.." && pwd)
export NIRO_RUNTIME
export NIRO_PYTHON="$NIRO_RUNTIME/.venv/bin/python"
export NIRO_ASSETS_ROOT="$(dirname "$NIRO_RUNTIME")/sunnypilot/custom/niro_dashboard"
export PARAMS_ROOT="$NIRO_RUNTIME/native-params"
export LOG_ROOT="$NIRO_RUNTIME/native-logs"
export QT_SCALE_FACTOR=0.5
export NIRO_ROAD_SCENE=${NIRO_ROAD_SCENE:-1}
export ZMQ=1
export NIRO_REPLAY_PANDA_PORT=15042
export NIRO_REPLAY_VIPC_PORT=19000
export NIRO_REPLAY_ASSUME_MADS_OFF=${NIRO_REPLAY_ASSUME_MADS_OFF:-1}
unset OPENPILOT_PREFIX
mkdir -p "$PARAMS_ROOT" "$LOG_ROOT"
cd "$NIRO_RUNTIME/source/selfdrive/ui"
if [[ "${NIRO_DEBUG:-0}" == 1 ]]; then
 exec lldb --batch -o run -o "bt all" -- "$NIRO_RUNTIME/native-build/Niro Native Replay.app/Contents/MacOS/sunnypilot-ui"
fi
exec "$NIRO_RUNTIME/native-build/Niro Native Replay.app/Contents/MacOS/sunnypilot-ui"
