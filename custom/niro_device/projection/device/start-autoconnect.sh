#!/bin/bash
# Called by /data/continue.sh. Keep the read-only AGNOS system image unchanged.
set -euo pipefail
lab=/data/openpilot/custom/niro_device/projection
[[ -f "$lab/autoconnect.enabled" ]] || exit 0
sudo -n systemctl link --runtime --force "$lab/device/aa-projection.service"
sudo -n systemctl daemon-reload
sudo -n systemctl start --no-block aa-projection.service
