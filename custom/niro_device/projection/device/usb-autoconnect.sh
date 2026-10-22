#!/bin/bash
# Own only this gadget. Never replace another USB function or restart the UI.
set -euo pipefail
lab=/data/openpilot/custom/niro_device/projection
mode=/sys/devices/platform/soc/a600000.ssusb/mode
g=/sys/kernel/config/usb_gadget/aa_projection
exec 9>/run/aa-projection-usb.lock
flock -n 9 || exit 1
old=$(cat "$mode")
cleanup_gadget() {
  if [[ -d "$g" ]]; then
    echo '' > "$g/UDC" || true
    rm -f "$g/configs/c.1/accessory.usb0"
    rmdir "$g/configs/c.1/strings/0x409" "$g/configs/c.1" "$g/functions/accessory.usb0" "$g/strings/0x409" "$g"
  fi
}
# Recover only our own known gadget after an interrupted service.
if [[ -d "$g" ]]; then
  [[ $(cat "$g/strings/0x409/serialnumber") == comma-aa-projection ]] || exit 1
  cleanup_gadget
fi
[[ -z $(ls -A /sys/kernel/config/usb_gadget) ]] || { echo 'Another USB gadget is active; leaving it untouched'; exit 1; }
child=
cleanup() {
  trap '' TERM INT
  if [[ -n "$child" ]]; then kill -TERM "$child" 2>/dev/null || true; wait "$child" || true; fi
  cleanup_gadget
  echo "$old" > "$mode"
}
trap cleanup EXIT
trap 'exit 0' TERM INT
mkdir "$g"
echo 0x18d1 > "$g/idVendor"
echo 0x2d00 > "$g/idProduct"
mkdir "$g/strings/0x409"
echo comma-aa-projection > "$g/strings/0x409/serialnumber"
echo 'comma dashboard' > "$g/strings/0x409/manufacturer"
echo 'Android Auto projection' > "$g/strings/0x409/product"
mkdir "$g/configs/c.1" "$g/configs/c.1/strings/0x409"
echo Accessory > "$g/configs/c.1/strings/0x409/configuration"
echo 2 > "$g/configs/c.1/MaxPower"
mkdir "$g/functions/accessory.usb0"
ln -s "$g/functions/accessory.usb0" "$g/configs/c.1/accessory.usb0"
echo peripheral > "$mode"
echo a600000.dwc3 > "$g/UDC"
python3 -u "$lab/device/usb_autoconnect.py" & child=$!
wait "$child"
