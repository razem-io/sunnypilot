#!/bin/bash
set -eu
cd "$(dirname "$0")"
export COMMA_CACHE="$PWD/cache"
mkdir -p "$COMMA_CACHE"
unset OPENPILOT_PREFIX
export NIRO_REPLAY_PANDA_PORT=15042
export NIRO_REPLAY_VIPC_PORT=19000
exec ./build/replay --demo --no-hw-decoder -c 1 --allow accelerometer,gyroscope,carState,carControl,deviceState,selfdriveState,selfdriveStateSP,controlsState,carParams,liveParameters,modelV2,roadCameraState,roadEncodeIdx,pandaStates,liveCalibration,driverMonitoringState,driverStateV2,wideRoadCameraState,longitudinalPlan "$@"
