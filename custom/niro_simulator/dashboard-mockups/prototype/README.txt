NIRO EV / Electric Studio — HTML/WebGL prototype

Open http://127.0.0.1:8772/dashboard-mockups/prototype/index.html while the local preview server is running.
Restart preview from the comma-plosion project root: python3 -m http.server 8772 --bind 127.0.0.1
Use the user's native cmux workspace workflow for a persistent server.
ES modules require the HTTP preview; opening index.html through file:// is not supported.

Controls: drag the car, scroll to zoom, choose views, toggle battery cutaway and sensor annotations, adjust simulated speed/SOC/power, and choose scenarios.
All readings are simulated. No device communication or driving-control changes.
The default car is the locally stored Sketchfab GLB by Xeeter. The original asset is unchanged in assets/niro-original.glb. Runtime changes: grey paint (Paint_Color), surface-following blue bumper trims, normalization and front orientation. This is still a Hybrid exterior, not an exact e-Niro conversion. Procedural form study is available as a fallback. Battery geometry is schematic, not a measured battery layout.
The local GLB importer accepts self-contained GLB files (under 100 MB). Optimize assets first. User-imported materials are retained. Default Niro materials are customized at runtime. Sensor anchors are approximate; exact EV fascia and hardware performance need further work.

Sketchfab reference: 2021 Kia Niro FE Eco Hybrid by Xeeter
https://sketchfab.com/3d-models/2021-kia-niro-fe-eco-hybrid-36999e25cf854948be95459eda809a10
Public model metadata reports downloadable, CC Attribution (CC BY 4.0), 205580 faces.
https://creativecommons.org/licenses/by/4.0/
User provided the downloaded 2K GLB on 2026-10-19. A local copy is bundled with attribution in assets/ATTRIBUTION.txt. The optional official iframe loads only when requested. Credit is shown in the UI.
The Hybrid exterior differs from the e-Niro reference; adapting the front fascia is required in addition to recoloring.

Dependencies: Three.js 0.160.1, MIT license in vendor/LICENSE. Runtime dependencies are stored locally. Optional Sketchfab embed requires internet.

Checks: real WebGL render; driving/regen/park/low-pressure scenarios; fixed status and gear updates; cutaway; labels toggle; invalid GLB error retains placeholder; no horizontal overflow at 1440px or 390px. Not benchmarked on comma hardware.
Native migration will reuse design, scene assets and sensor semantics, but requires a separate renderer integration and actual data adapter; HTML is not automatically deployable as native sunnypilot UI.

Display sizing: fixed 2160 x 1080 landscape dashboard matching the existing comma 3 native boot/UI surface. Browser scales the complete dashboard proportionally; no mobile reflow. Fullscreen letterboxes other aspect ratios. Workbench controls stay outside the device surface. Typography and view buttons enlarged for the small physical display. Final legibility still needs checking on the device.
