# Niro dashboard checkpoint — 20 October 2026

Reviewed native desktop dashboard with full-window road rendering, persistent
status icons and gears, original settings shortcut, Niro model, ghost lead
vehicle, and a left lane-change replay loop (199–225 seconds).

This directory is a source checkpoint of the development workspace. It does
not replace the vehicle UI or install anything on a comma. The existing
`custom/niro_dashboard` package supplies the native mesh and textures; model
credits are retained there and in the HTML prototype's assets directory.

## Contents

- `simulator-runtime`: native UI, replay build recipes, HTML adapter and tests.
- `dashboard-mockups/prototype`: original local web prototype and model.
- `patches/replay-source.patch`: changes against the sunnypilot source revision
  in `versions.json`, including legacy replay migration and interval looping.
- `patches/msgq.patch`: local-only publisher and macOS port adaptations.
- `patches/device-workaround-unverified.patch`: separately preserved device
  process-lifecycle experiment. **Not validated on the device.**
- `snapshot-sha256.json`: checksums of copied workspace sources and assets.

Build outputs, downloaded route caches, local device parameters, logs and signed
download URLs are deliberately excluded. The current live workspace remains
unchanged. This is a development checkpoint, not a portable binary release.

## Restore the workspace layout

In a new development directory, copy `simulator-runtime` and `dashboard-mockups`
from this checkpoint, and create a `sunnypilot` symlink to this repository checkout.
The existing scripts expect these three sibling paths.

Clone sunnypilot into `simulator-runtime/source`, check out its exact revision
from `versions.json`, and initialize its submodules. Apply
`patches/replay-source.patch` to that clone and `patches/msgq.patch` inside its
`msgq_repo` submodule using `git apply`. Fetch Git LFS UI assets in the clone.
Use the recorded libyuv revision for `simulator-runtime/libyuv`.

The native desktop adapter's generator expects the preserved device source
experiment: apply `patches/device-workaround-unverified.patch` only to the
development `sunnypilot` checkout if regenerating that adapter. This is not an
instruction to deploy it to a vehicle.

Follow the runtime and native READMEs for Homebrew dependencies and builds.
Create `simulator-runtime/.venv` with `pycapnp` and `pyzmq` for Python tools.
`native/generate_build.py` regenerates the machine-specific `sources.cmake`.
Replay fetches the official demo using `--demo`; cache-dependent regression
tests additionally require a freshly obtained demo file manifest and full logs.

Run the scene with:

```sh
simulator-runtime/start-replay.sh --start 199 --loop-end 225
simulator-runtime/native/start.sh
```

Run these in separate persistent terminals. The development route is a Toyota
Corolla recording rendered with a Niro model. The replay displacement regression
measures approximately 3.06 m left with one lane-index reassignment.
