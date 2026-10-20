# Niro dashboard and native home entry

Target: original comma three, AGNOS 12.8, sunnypilot 0.10.1 staging-tici release `1f7233cb983f3c13fdbba47a86f8fbc796ad9211` (source commit `737a6c4236e843034680c951005b38d15815363f`).

## Navigation

The native right-hand Firehose prompt card is replaced with **Niro Dashboard → Dashboard öffnen**. This is the existing Qt widget and layout, not a floating window. The entry starts the separate dashboard. **Menü** hides the dashboard and returns to stock UI. The resident process retains its loaded model and graphics; subsequent entry clicks send an `open` request over a user-restricted local socket. The first launch after reboot still initializes the bridge and graphics. The stock UI keeps running throughout. There is no floating return control and no automatic dashboard launch at boot.

The release contains a compiled UI rather than its Qt sources. `native_entry.cpp` is a small Qt startup extension that finds only the exact `WiFiPromptWidget` class with its expected two labels and one button, replaces those labels, disconnects only the old clicked signal, and attaches the dashboard action. A mismatch leaves the original UI unchanged. The reference source is `selfdrive/ui/qt/widgets/wifi.cc` at the source commit above. The extension is attached as a DT_NEEDED library to this exact UI build by `install-entry.py`; no binary instruction offsets, driving logic or control messages are altered.

Installation requires fresh confirmed offroad state and exact original UI SHA256 `424c259c88f275c71e34c34f35aed7e38b535d7bfbcf12d5057a69640a84b675`. Original backup: `/data/local/niro-dashboard/ui.before-native-entry`. Patched UI SHA256: `17d923a76bedb43417ea70477f303b96b43b481e781f9f6439ed5408d219f0e2`. The extension library must remain at `/data/openpilot/custom/niro_dashboard/libniro-entry.so` while this UI is installed. Restore the original UI before removing that library. `patchelf` was installed from PyPI into `/data/local/niro-dashboard/tools`, without changing system packages.

## Rendering and data

The native Qt/OpenGL dashboard uses a 2160×1080 surface and the stock UI's Wayland 270° buffer transform. Drag the 3D car to rotate only while parked. Original model: Xeeter's “2021 Kia Niro FE Eco Hybrid”, CC BY 4.0; see assets/ATTRIBUTION.txt. Grey hybrid body with directional key/fill light, view-dependent specular highlights, softbox reflections and gamma/tone mapping. Browser blue trim, cutaway and full PBR environment have not yet been ported.

Speed uses valid live carState.vEgo. While confirmed offroad, unmapped EV fields show clearly labeled static DEMO readings (78%, 284 km, 18 kW, 15.8 kWh/100 km, 42°C motor, 26°C battery, 13.8 V and 2.5 bar). Speed has no demo fallback: only fresh valid CAN speed is displayed, otherwise a dash. Onroad no simulated readings are shown; unmapped fields return to dashes. `bridge.py` subscribes read-only to deviceState, carState, carControl, selfdriveState and selfdriveStateSP. Onroad all required streams must be fresh/valid, controls and MADS disabled, and alerts absent. Unknown/stale state or active assistance yields to stock UI. Healthy stock UI heartbeat and positive display brightness are also required. Sleeping hides the overlay so taps reach stock UI. Brightness is never forced.

The dashboard hides after 300 ms without bridge output. The bridge terminates it after a GUI heartbeat stall of one second. Only first graphics initialization while fresh offroad/no-control status is confirmed gets up to five seconds; the grace ends after the first frame. This resolved the first-render watchdog termination. Final in-vehicle assist transitions still require physical verification.

## Build and checks

- `build.sh`: native dashboard.
- `build-entry.sh`: Qt entry library.
- `test-entry.sh`: real Qt widget test in offscreen mode, confirms replacement and click routing without launching a dashboard or changing Firehose settings.
- `test_policy.py`: checks manual, stale, active control/MADS and alert eligibility.
- `install-entry.py`: exact-build, offroad-only reversible installation.

Verified on device: native UI restarted with healthy heartbeat and screenshot showing the integrated entry. Earlier dashboard render and stale-reader recovery passed. The user confirmed the native-entry round trip in both directions. The user also confirmed faster resident reopening and a stable view with the updated lighting/demo appearance.

Runtime log/state/screenshots: `/data/local/niro-dashboard/`. For rollback while parked: restore `ui.before-native-entry` atomically to `selfdrive/ui/ui` and restart only the UI; terminate the dashboard and bridge if running. No reboot or control-process changes are required.

## Live speed and 3D steering

`telemetry.py` converts fresh valid CAN carState.vEgo to km/h and exposes steeringAngleDeg. The actual front-wheel meshes rotate about their hubs using steeringAngleDeg / CarParams.steerRatio. Both front wheels use the same estimated angle (no Ackermann correction); this is not a measurement of each individual wheel angle. There is no demo fallback for speed or steering. Missing, invalid or stale values display dashes; unknown wheel angle shows the model wheels neutral with an explicit unknown-state label. CarParams is re-read every five seconds to support connecting the car later.

At the user's request, all pressure cards and the pressure footer are currently hidden. Other previously requested offroad sensor demos remain labeled. No TPMS implementation or physics engine is included. Real vehicle connection, steering direction and assistance transitions still need in-car verification.

`split_wheels.py` separates complete welded components inside each front-wheel envelope. `niro-steering.mesh` retains all 205551 source triangles and assigns 19122 triangles to each front wheel. Position and normals use the same rigid rotation. The user confirmed the model wheel steering appearance before removal of its demo fallback. `test_telemetry.py` covers unit conversion, steering sign, stale/invalid CAN, nonfinite values and absent ratio.
