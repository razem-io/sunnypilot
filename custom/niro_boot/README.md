# Kia Niro EV startup screen — original comma three

Personal `niro-ev-tici` branch based on official `staging-tici` commit
`1f7233cb983f3c13fdbba47a86f8fbc796ad9211` (sunnypilot 0.10.1, 2025-10-13).
This is the C3/TICI line, not the newer C3X/C4 release line. AGNOS 12.8.
[Official supported branches](https://community.sunnypilot.ai/t/recommended-branches/235/1).

`launch_openpilot.sh` starts this splash in the background only on `comma tici`.
The C3 launcher continues immediately. No driving logic, firmware, models,
or safety settings are changed. The earlier firmware logo remains.

## Sequence

- Graphic resolves by 4.3 seconds.
- The graphic stays still for 3.5 seconds.
- `Systeme werden hochgefahren` fades in between 7.8 and 9.6 seconds.
- The final graphic holds until sunnypilot's native UI heartbeat is ready.

The player never loops. A per-kernel-boot marker and process lock in `/dev/shm`
prevent duplicate playback. If the UI is ready sooner, it takes over immediately;
startup is not delayed to finish the animation. Touch dismisses the splash.

The native UI's 2160×1080 surface and 270° Wayland buffer transform are used.
Playback uses lightweight 836×471 frames with wall-clock selection, then the
full-resolution final PNG. The artwork's aspect ratio is preserved.

The `python3` shim forwards all normal invocations to AGNOS's existing venv.
Only `./spinner.py` in the C3 UI directory is handled by the proxy so the
startup spinner does not cover the splash. When neither splash nor ready UI
exists, the original spinner runs. Error windows are not intercepted.

## Build and verification

The ARM64 executable is included, consistent with the prebuilt upstream branch.
To rebuild **on AGNOS 12.8** using its existing Qt toolchain:

```sh
custom/niro_boot/build.sh
custom/niro_boot/niro-splash --check
custom/niro_boot/niro-splash --test
```

`--check` reports native UI readiness without showing a window. `--test` uses
Qt's offscreen backend, plays the full sequence, saves frames in the ignored
`test/` directory and exits after the final hold. Normal operation uses Wayland.

The approved graphic, rendering source, and MP4 preview are in `artwork/`.
The rendering source requires Python, Pillow, NumPy, FFmpeg and the specified
macOS Helvetica Neue font; it is an authoring tool, not a device dependency.
The deployed player needs no video decoder or font rendering at startup.

Logs go to `/data/local/niro-boot/boot.log`, outside this Git checkout.
A missing or failed player does not prevent the normal C3 launcher from running.

## Migration and rollback

The device previously used an external `/data/continue.sh` splash wrapper.
When moving to this branch, restore the preserved original `continue.sh`
so this branch is the only startup integration. Keep the old wrapper and
assets as a rollback backup outside Git. Never overwrite an unrecognized
launcher automatically.

Returning to upstream `staging-tici` with the original `continue.sh` removes
this startup hook. Restoring the saved pre-migration wrapper restores the
previous external installation. No firmware flashing is involved.

Physical C3 testing confirmed the landscape orientation and full delayed-text
sequence. Boot logs confirmed one playback and successful handoff to the native
sunnypilot UI. Hardware testing of vehicle control is outside this cosmetic change.
