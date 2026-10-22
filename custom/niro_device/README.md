# Niro device UI release

This is the production device package for this development branch, not the old
floating demo window. It contains the current GLES dashboard, articulated Niro,
vehicle lights/doors, camera reflections, display-sleep button, original settings,
and the shared-GPU Android Auto sender. Vehicle control processes are unchanged.

## Updates and startup

The C3 launcher installs this package **after** selecting an overlay update and
before starting the normal manager. `install.py` verifies the release manifest,
atomically installs `release/niro-ui` at `selfdrive/ui/ui`, and starts the USB
service. The normal manager supervises the same binary. There is no UI rollback,
no old prebuilt UI in the branch, and no permanently pinned external executable.
A failed integrity check stops startup instead of selecting an old version.

The branch carries the ARM64 release binaries so the comma does not have to
compile the renderer during a car startup. These binaries are built from the
sources and locked dependency patches in this directory. The existing `prebuilt`
branch marker remains responsible for the other sunnypilot components.

Update the **release and its manifest together** whenever changing the renderer,
assets, projection sender, build files or startup installer. `manifest.py --write`
records source and asset hashes; `manifest.py` verifies the package. Do not hand-edit
the installed UI or replace `/data/continue.sh` with a temporary trial launcher.

`/data/continue.sh` should simply change into `/data/openpilot` and execute
`./launch_openpilot.sh`. `DisableUpdates` must be false. The normal updater follows
`origin` and its selected branch. Old `/data/local/niro-current` pinning must be
removed once migrating to this branch package.

The USB service is linked at runtime from this checkout. It starts one sender
when the lower USB port is configured by the headunit and stops it on disconnect.
The upper port still requires adequate power. Display sleep remains a manual
moon-button action, independent of projection. TLS keys and mutable diagnostics
live in `/data/local/niro-projection`, outside Git. No user credentials are shipped.

## Build on ARM Linux / Apple Silicon Docker

The Qt UI dependency is locked to sunnypilot revision
`737a6c4236e843034680c951005b38d15815363f`, with msgq revision in `versions.json`.
This is intentional: the main branch has newer Python UI sources; the working
native dashboard uses the legacy Qt code with the device changes preserved in
`patches/`. These dependencies do not replace vehicle-control code on the comma.

1. Extract the device SDK using `collect-sdk.py` on the comma, redirect its tar
   stream to a local file, and extract it into a private directory. It contains
   the installed Qt/capnp headers, build tools and linked system libraries. It is
   not distributed in this repository. The validated target is comma 3 / AGNOS
   12.8 / Qt 5.12.8; a different OS/ABI needs a fresh device check.
2. Install Git LFS and Docker. Run `python3 prepare.py` once. This fetches pinned
   source dependencies and only the Qt resource assets. An existing source clone
   can be supplied via `--source-cache /absolute/path` to avoid downloads.
3. Run `NIRO_SDK=/absolute/sdk/path bash build.sh`. The ARM64 container compiles
   the full native UI and Qualcomm encoder. It does not depend on a neighboring
   simulator workspace, previous object files or the device's running processes.
4. Run `python3 -m unittest discover -s tests -v` and `python3 manifest.py`.
   Inspect the resulting UI on the test device, then commit the package, binaries,
   manifest and launcher together. Push to `niro-ev-tici` only after validation.

The included libyuv ARM64 library is the Ubuntu package
`libyuv0=0.0~git202401110.af6ac82-1`; its BSD license is in
`release/libyuv.COPYRIGHT`. To refresh it, extract that exact ARM64 package with
`dpkg-deb -x`, copy the real shared library into `release/libyuv.so.0`, retain its
copyright file and regenerate the manifest. GPU/Qt SDK libraries are not bundled.

Model attribution is in `assets/ATTRIBUTION.txt`. The exact validated optimized
mesh is versioned with the package, so installing an update cannot combine an old
mesh with a new renderer. The editable geometry experiments remain separate.
