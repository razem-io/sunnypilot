# Native sunnypilot Replay auf macOS

Originale sunnypilot-Qt-Oberfläche, das native Niro-QOpenGLWidget und eine native Straßenvisualisierung. Kein Browser und kein HTML-Renderer. Datenquelle ist das originale C++-Replay des passenden sunnypilot-Quellstands.

## Start und Build

- `simulator-runtime/native/build.sh`: CMake-Build mit Homebrew Qt 5.15.19, Cap'n Proto, ZeroMQ, Eigen und OpenSSL. Gleiche Basisbibliotheken wie beim Replay.
- `simulator-runtime/start-replay.sh --start 180`: aufgezeichnete Demo starten. Die Fahrzeugdaten stammen von einem Toyota Corolla, die Niro-Darstellung bleibt unsere Entwicklungsansicht.
- `simulator-runtime/native/start.sh`: natives Fenster starten. Eigene Params unter `native-params`, keine Geräteverbindung.
- Die Desktop-App startet dauerhaft im Niro-Dashboard. Die frühere Kamerabild-Umschaltung ist in diesem Einstieg entfernt.
- Das eigenständige Dashboard liest direkt die originalen UI-Nachrichten. Es startet keine Dashboard-Bridge mehr und setzt keinen MADS-Testzustand voraus. Assistenzstatus stammt weiterhin aus selfdriveState; separate MADS-Aktivität ist in dieser Original-Aufzeichnung nicht vorhanden.

Die Qt-Oberfläche lädt ihre Originalbilder und Schriften über Git LFS aus `source/selfdrive/assets`. Qt-Metadaten werden einzeln übersetzt, um die sunnypilot-Klassenaliasse zwischen Übersetzungseinheiten zu isolieren. `prepare_dashboard.py` erzeugt die Desktop-Anpassung direkt aus unserem nativen Dashboard-Quellcode. Gerätequellen/Binaries werden dabei nicht geändert. Die Desktop-Fassung nutzt OpenGL Core statt OpenGL ES/Wayland, proportional angepasste Eingaben und eine gestapelte Qt-Ansicht statt des unbestätigten Prozess-Neustart-Workarounds auf dem Gerät.

## Bedienung

Das Dashboard ist dauerhaft sichtbar, auch bei fehlenden Daten und Offroad. „Einstellungen“ rechts oben öffnet das originale SettingsWindowSP-Menü; dessen × führt zurück ins Dashboard. Kein äußerer Status-Farbrahmen. Native Warnungen bleiben als separate Ebene über der Szene erhalten.

Replay-Tasten in dessen cmux-Workspace: `d` nächste Abschaltung der Assistenz, `e` nächste Aktivierung, Leertaste Pause/Fortsetzen, `s/S` ±10 Sekunden, `m/M` ±60 Sekunden, `+/-` Geschwindigkeit. Pause führt absichtlich zum Ablauf der Datenfrische. Sehr langsame Wiedergabe kann ebenfalls die Zeitgrenzen überschreiten.

## Straßenansicht

- Daten: `modelV2.position`, `laneLines` mit `laneLineProbs`, `roadEdges` mit `roadEdgeStds`, aktuelles Führungsfahrzeug aus `leadsV3[0]` bei `probTime=0` und Wahrscheinlichkeit über 0,6. `carState` liefert die Geschwindigkeit.
- Die weiteren `leadsV3`-Einträge sind Zukunftshypothesen und werden NICHT als zusätzliche Autos gezeichnet. Keine erfundenen Nebenfahrzeuge, Fußgänger, Ampeln oder Kreuzungen.
- Dieselbe OpenGL-Szene bleibt bei manueller und aktiver Fahrt bestehen. Unser vorhandenes Niro-Mesh mit derselben Beleuchtung und gelenkten Vorderrädern bleibt geladen. Bei Aktivierung bewegt sich die Kamera mit 0,65 s Zeitkonstante hinter den Wagen, die Straße blendet ein; beim Deaktivieren geht sie zur manuellen Perspektive zurück. In der manuellen lokalen Vorschau ist Drehen per Maus möglich.
- Linienglättung 0,65 s; Erkennungshysterese 0,35/0,20. Erkannte Linien und Ränder bleiben bei Einzelerkennungsausfall fünf Sekunden bestehen und blenden dann über zwei Sekunden aus. Wiedererkennung blendet weich ein. Es handelt sich um visuelle Erinnerung, nicht eine neue Erkennung oder eine Kartierung von Seitenstraßen.
- Führungsfahrzeug: Abstand und seitlicher Versatz zum geglätteten Fahrweg werden mit 0,75 s Zeitkonstante geglättet. Die Fahrzeugausrichtung folgt der lokalen Fahrwegkurve. Bei Erkennungsausfall zwei Sekunden halten, dann eine Sekunde ausblenden; Wiedererkennung blendet weich ein. Große Positionssprünge (>25 m längs / >4 m seitlich) setzen die Positionsglättung zurück, um keinen langen falschen Übergang zu zeichnen. Kein verlässliches Objekt-Tracking über Identitäten.
- Der eigene Wagen nutzt das echte Niro-Modell. Das Führungsfahrzeug nutzt denselben Mesh als Platzhalter; Fahrzeugtyp und Abmessungen des erkannten Autos sind unbekannt. Die Darstellung beeinflusst keine Assistenzberechnung. Bewegte Wegmarkierungen sind ein Gestaltungselement.
- Bei ausbleibenden Modelldaten über 0,5 s werden Geometrie und Führungsfahrzeug ausgeblendet. Native sunnypilot-Warnungen bleiben als separate Ebene erhalten.
- Kein Lenken, Bremsen oder Senden von CAN-Nachrichten. Keine Fahrzeugphysiksimulation: Replay spielt vorhandene Berechnungen ab.

## Portkonflikte und Video

Die Originalports kollidierten lokal mit VSCodium (`pandaStates`, 50420) und OrbStack (Kamerabilder, 9000). Nur unsere Replay-Prozesse verwenden daher `NIRO_REPLAY_PANDA_PORT=15042` und `NIRO_REPLAY_VIPC_PORT=19000`. Die kleine Änderung im msgq-Untermodul liegt in `msgq-macos-ports.patch`; ZMQ-Publisher binden weiter nur an localhost. Fremde Prozesse bleiben unverändert.

`roadEncodeIdx` muss in der Replay-Allowlist stehen: Ohne Video-Indexnachrichten werden zwar Kamerapuffer angelegt, aber keine Bilder veröffentlicht. Zusätzlich sind die nativen UI-Dienste wie `pandaStates`, `liveCalibration` und `driverMonitoringState` freigegeben.

## Grenzen

Die macOS-Fassung kann Datenfluss und Qt-Ansichtswechsel testen, aber nicht den Qualcomm-/Wayland-Absturz auf dem comma reproduzieren oder dessen Behebung bestätigen. Im bisherigen Replay-Lauf trat vor der Video-/Portkorrektur auch ein Replay-Prozessabbruch auf; langfristige Stabilität ist deshalb nicht bestätigt. Der frühere Geräte-Workaround bleibt unbestätigt. Reale Niro-Türkontakte, Rückwärtsmanöver und weitere Sensoren benötigen passende Aufzeichnungen bzw. erneute Hardwaretests.

## Prüfung 20.10.2026

Native CMake-Version gebaut und mit Original-Replay gestartet. Manuelle Schrägansicht und aktive Heckansicht mit Niro-Mesh visuell geprüft. Native Warnungsebene bleibt erhalten. `test_visual_timing.cc` prüft die Fünf-plus-zwei-Sekunden-Linienausblendung, Zwei-plus-eins-Sekunden-Fahrzeugausblendung und bildratenunabhängige Glättung. Die Grenzen sind synthetisch getestet; ein kontrollierter Erkennungsverlust über exakt diese Dauer ist in der laufenden Aufzeichnung noch nicht separat vermessen. Keine Übertragung auf das comma erfolgt.

## Korrektur: vollständiges manuelles Layout beibehalten

Die native Szene übernimmt jetzt die feste Aufteilung der manuellen Replay-Ansicht: Geschwindigkeit, Lenkradwinkel, berechnete Radstellung und Pedalstatus links; Blinker, Türen, Gurt, Totwinkel und Parkbremse rechts; Gang im Kopfbereich. Ein gemeinsamer Zeichencode erzeugt diese Anzeigen in beiden Assistenzzuständen. Nur das mittlere Feld (1267 × 710 innerhalb 2160 × 1080) enthält Straße, Fahrzeug und Kamerafahrt. OpenGL-Viewport/Scissor und QPainter-Clipping halten die Straße aus den Seitenanzeigen heraus. Blinker werden als Status sowie an projizierten Fahrzeugankern dargestellt; Aufleuchten ist ein visueller Takt aus dem aufgezeichneten Blinkersignal. Ansichtsbuttons und Drehen stehen bei manueller Fahrt zur Verfügung. Die native Warnungsebene hat weiterhin Vorrang.

Aktive und manuelle Fahrt mit denselben Seitenanzeigen im Original-Replay visuell verglichen; dabei rechten Blinker und Bremse beobachtet. Die Radstellung verwendet CarParams, ersatzweise gültige aktuelle LiveParameters, entsprechend der manuellen Datenansicht. Keine erfundene Übersetzung bei fehlenden Parametern.

## Dauerhafte Hauptansicht und Linien

`desktop_main.cc` enthält nur die gemeinsame RoadScene plus die originale OnroadAlerts-Ebene. Netzwerk, Temperatur, Panda und Connect stehen im Fußbereich. Werte kommen aus deviceState/pandaStates und werden bei veralteten Nachrichten als unbekannt dargestellt. Der Connect-Ping wird gegen den aufgezeichneten Event.logMonoTime geprüft, nicht gegen die Uhr des Macs. Das verhindert falsche Online-Aussagen aus unterschiedlichen Boot-Zeitbasen. Die Zeile „Status aus Aufzeichnung · Replay“ bleibt sichtbar.

Spurlinien sind schematisch gestrichelt, Fahrbahnränder durchgezogen. modelV2 in diesem Quellstand liefert keine Klassifikation gestrichelter/durchgezogener Verkehrsmarkierungen und keine Spurwechselerlaubnis. Die Legende macht diese Grenze sichtbar. Der Bereich bis sechs Meter hinter dem Fahrzeugursprung wird aus der nahen Linientangente verlängert: gestalterische Fortsetzung, keine Erkennung hinter dem Auto. Linien/Ausblendung sonst unverändert.

Native Version gebaut; Hauptansicht ohne Farbrahmen, nach hinten verlängerte Fahrspur sowie Netzwerk/Temperatur/Panda/Connect visuell geprüft. Vollbildknopf und Rückkehr ins Fenster per Knopf geprüft. Weiterhin nur lokale Entwicklung, keine Installation auf dem comma.

## Einstellungen und bewegte Spurstriche

Der frühere Vollbildknopf wurde durch den direkten Einstellungen-Shortcut ersetzt. Die unveränderte SettingsWindowSP-Ansicht öffnet auf macOS als modales Fenster über dem Dashboard (separate Rasterfläche, um den Qt/OpenGL-Kompositionsfehler beim Seitenwechsel zu umgehen). × schließt das Menü und kehrt zum laufenden Dashboard zurück. Originale Einschränkungen bei laufender Fahrt bleiben im Einstellungs-Widget erhalten; es wurden beim Test keine Einstellungen geändert. Netzwerk, Temperatur, Panda und Connect bleiben im Fußbereich der Hauptansicht.

Gestrichelte Linien verschieben sich entlang der Straße mit integriertem carState.vEgo × dt. Eigene 9-m-Phase für 3-m-Strich / 6-m-Lücke verhindert Sprünge an der Periodengrenze; Teilstriche an den Sichtgrenzen werden abgeschnitten. Bei Stillstand oder ungültigen Geschwindigkeitsdaten bewegt sich die Phase nicht weiter. Keine Kopplung an einen festen Animationstakt. Menü öffnen, sichtbare Originalansicht und Rückkehr zum Dashboard wurden im nativen Replay geprüft; Build erfolgreich.

## Ruhigere Gestaltung und durchgehender Hintergrund

Aktueller Stand: Die Straße wird im gesamten Widget hinter dem HUD gezeichnet (die frühere Begrenzung auf das mittlere Feld ist aufgehoben). Geschwindigkeit und Status behalten ihre Positionen. Die drei Perspektivbuttons samt Klickbereichen sind entfernt; manuelles Drehen im mittleren Bereich bleibt erhalten. Einstellungen: Zahnrad ganz rechts oben, mit zugänglichem Namen und Tooltip. Gang steht links daneben.

Die Striche bleiben 3 m lang; die Lücke ist jetzt 18 m statt 6 m (dreifach). Eigene modulo-21-m-Phase, weiterhin Geschwindigkeit × Zeit. Gerätestatus kompakt: Netztyp wie 4G, Steckersymbol für Panda, Globus für Connect. Getrennt/unklar wird orange und durchgestrichen; Temperatur wird nur bei Warnung oder unbekanntem Datenstatus gezeigt. Statusdetails per Tooltip, Replay-Hinweis bleibt.

Eigener Niro: dunkler grauer Lack, türkisblaue Akzentstreifen an den Stoßfängern. Die Geometrie bleibt das verfügbare Hybrid-Modell, kein exakt nachgebauter EV-Stoßfänger. Führungsfahrzeug: entsättigte Farben und 62 % Deckkraft, zusätzlich zur bestehenden Ein-/Ausblendung. Build erfolgreich; durchgehende Straße hinter dem Fußbereich, grauer Niro und blauer Heckakzent visuell geprüft.

## Kompakte Anzeigen und Spurwechsel (Sichtprüfung ausstehend)

Panda- und Globus-Symbol auf 55 % der bisherigen Größe reduziert, auf die 4G-Schrift ausgerichtet. Zahnrad ohne Rahmen/Hintergrund, gedämpfte Farbe, stärkere Striche bei gleicher Größe. Lenkradwinkel- und Vorderradtexte entfernt; die Radmeshes bleiben weiterhin gelenkt.

Spurwechsel nutzt modelV2.meta.laneChangeState / laneChangeDirection. preLaneChange wird nur als bereit angezeigt. Während starting/finishing wird die Bewegung der erkannten nahen Spurmitte in ego-relativen Koordinaten in einen Darstellungsversatz integriert; Indexwechsel werden um die erkannte Spurbreite entrollt, Einzelbewegungen begrenzt, Fahrzeugposition und Kameranachlauf geglättet. Fahrzeugausrichtung folgt dem nahen vorhergesagten Fahrweg. Bei unbrauchbaren Linien keine erfundene zeitgesteuerte Querfahrt; kein Auslösen allein durch Blinker. Dies ist eine Visualisierung, keine weltkoordinatenbasierte Rekonstruktion. Überhol-/Wechselverlauf muss im Replay noch visuell geprüft werden.

Build erfolgreich; test_lane_motion.cc prüft Richtung, Indexwechsel, ungültige Breite und Sprungbegrenzung. Laufender Datenstrom gelesen. Abschließende UI-Prüfung durch gesperrten Mac verhindert; keine Screenshot-Bestätigung dieses Stands. Lokale Vorschau läuft in cmux:comma-plosion/native-replay; Original-Replay in cmux:comma-plosion/dashboard-simulator. Keine Übertragung auf das comma.

## Feste Statusplätze und Gangreihe

Netzwerk, Thermometer, Panda und Connect besitzen nun feste, gleichmäßig verteilte Plätze. Das Thermometer bleibt auch bei normaler Temperatur sichtbar in Grau; Warnungen behalten die Warnfarbe. Inaktive Panda-/Connect-Symbole bleiben grau mit Trennmarkierung sichtbar. Gangreihe P/R/N/D dauerhaft; nur der aktuell gültige Gang wird hell hervorgehoben. N verhindert eine mehrdeutige Anzeige im Leerlauf. Bei fehlenden Fahrzeugdaten sind alle Buchstaben grau und „Gang unbekannt“ erscheint. Gangbuchstaben sind in derselben 85-Pixel-Zeile wie das Zahnrad vertikal zentriert. Keine zusätzlichen Rot-/Blauzuordnungen.


## Camera vignette experiment (20 October 2026)

The top-row Kamera toggle blends the original road-camera VisionIPC stream into
the existing dashboard. The center is opaque; an elliptical feather returns the
image to the dashboard background at the edges. The video covers the canvas
without letterboxing. The transition uses a 0.55 s exponential blend.
Synthetic road, path and ghost lead fade out with the video; ego Niro, telemetry,
settings and the original alert overlay remain. If no camera frames arrive for
500 ms, the video fades out and the model scene returns.

This is a compositing experiment, not a calibrated augmented-reality projection:
the camera is mounted on the recorded vehicle, while the 3D camera looks from
behind the displayed Niro. No novel rear viewpoint or image stabilization is
reconstructed. Native build and toggling both directions were visually checked.
During development the replay model publisher stopped once; restarting replay
restored model messages. Long-term playback stability is not established.


### Calibrated ground-plane hybrid experiment

`road_camera.h` projects the original NV12 road-camera textures onto z=0 in
the native world using `FCAM_INTRINSIC_MATRIX * scene.view_from_calib` and
`liveCalibration.height[0]` (observed ~1.154 m). Missing height falls back to
1.22 m. Intrinsics assume the demo AR0231 camera, scaled to the received frame
size; other camera sensors require their matching intrinsic matrix.
This supersedes the flat vignette above. Ground x/y project into camera pixels
with z=height, then are viewed through the same 3D matrix as the road and ego.
The Niro and virtual viewpoint move 1.5 m rearward in hybrid mode for visual
placement; that offset is an artistic placement, not a measured camera-to-car
transform. Only the two inner model lane boundaries overlay the video.

Projective interpolation avoids treating the camera as a flat screen. Image
borders and the far plane fade softly. Pixels of elevated objects are still
flattened onto the ground: vehicles visibly stretch, and slopes are not modeled.
The current prototype tests ground alignment, not full scene reconstruction or
calibrated AR for arbitrary vehicles. Build, projection direction/inverse checks,
and both UI toggle directions passed; the hybrid is left active in the replay.


### Expanded hybrid image

The image now uses a 48x48 UV mesh. Calibrated rays intersect the ground up to
100 m; farther and upper rays meet a vertical backdrop at 100 m. This retains
the sky and upper scene previously discarded by ground-only projection, and
reduces feathering to 4.5% at each image border. The backdrop is an explicit
visual approximation, not reconstructed depth. Ground alignment is retained
in the near field. Build and the expanded live camera view were checked.


### Low hybrid viewpoint

Hybrid mode now blends to a low chase camera (height 1.65 m, x=11.5 m),
a nearly horizontal look direction, 38-degree vertical field of view and a
small lens shift. The pure 3D camera remains unchanged. This reduces vertical
stretching in the far field; nearby vehicles at the sides still deform because
the ground-plane interpretation lacks object depth. Native build and the live
view were checked. This is a visual improvement, not a depth reconstruction.


### Center-preserving hybrid compromise

Hybrid viewpoint raised to 2.8 m, looking at (-12, .8, 0), with 42-degree FOV.
Central image rays are now placed on a common 40 m image plane instead of the
ground. This preserves central traffic proportions. Horizontal smoothstep blends
back to ground toward the edges; the bottom 28% transitions into the foreground.
This is deliberately artistic warping: the central image and synthetic lane
boundaries are no longer guaranteed to coincide. Side and foreground distortion
remain visible. Native build passed and the central vehicle proportions were
visually checked in replay. Pure 3D mode retains its previous viewpoint.


### Straight full-image backdrop

The accepted 2.8 m viewpoint and 42-degree FOV are retained. The ego visual
placement is moved from 1.5 to 2.2 m rearward. All camera rays now intersect a
common 85 m plane, removing the differential depth warp at the image edges
and increasing the central visible image area. Soft border fading is retained.
This supersedes the center-only protection: the entire camera image now keeps
its projective proportions. It is a 3D-positioned video backdrop, not a ground
homography; synthetic lane overlays are approximate and are not pixel-registered
to the video. Build passed and the replay view was visually verified.

### Soft camera silhouette and view switching

The default view is pure 3D. The Camera toggle switches to a 28% larger,
softly feathered video backdrop, retaining the accepted camera angle and image
proportions. A stationary three-finger tap within 650 ms also toggles views;
Qt touch handling is implemented, but actual comma multitouch remains untested.
The original ModelRenderer draws calibrated path/lane overlays into camera pixel
coordinates before the shared video projection, replacing approximate synthetic
camera-mode lane overlays. This remains a video backdrop, not scene reconstruction.

### Ego-linked camera follow

The hybrid backdrop uses the centered chase view plus the ego vehicle's projected
screen displacement (compensated for the video's 1.28 enlargement). Thus the
85 m image plane no longer suppresses lateral motion relative to the nearby
Niro. Both follow the existing damped lane/camera offset; the calibrated image
and annotations share exactly the same transform. Pure 3D is unchanged.

### Shared heading and wider silhouette

The video plane now shares the Niro's filtered yaw (`-egoYaw`), pivoting around
its own center to avoid an exaggerated 85 m lever-arm swing. Original overlays
rotate with the video. Uniform image enlargement is 1.48 (previously 1.28),
retaining proportions, with a wider elliptical mask and broader smooth edge
feathering. Translation compensation uses the same shared scale constant.

### Recorded acceleration and oval camera mask

The demo's first segment contains 6,174 accelerometer and 6,131 gyroscope records
(approximately 100 Hz). Replay now publishes both. A dedicated nonblocking
SubMaster consumes acceleration for a visual body-motion approximation: upstream
locationd's device axis mapping, 3 s baseline removal for gravity/mount offset,
0.28 s smoothing, roll bounded to 1.2 degrees and pitch to 0.9 degrees.
Only non-wheel meshes tilt around a body pivot. Camera and wheel positions remain
stable. Invalid/stale sensors fade motion to neutral; replay rewind resets it.
This is not measured suspension travel or reconstructed vehicle attitude; gyro
is available but is not integrated into the effect. Live playback verified over
100 valid acceleration messages per second. Native build passed.
The camera mask now uses a flatter ellipse, revealing more side image and less
sky without changing video aspect ratio.

### Exaggerated motion tuning / steering-driven turn

For visual evaluation, acceleration gains are 4x the initial version, bounded to
4.5 degrees roll and 3.5 degrees pitch, smoothed over 0.22 s. The visual body yaw
now follows measured road-wheel angle with a 3.5x display gain and a 10-degree
limit above 1 m/s. Positive steering means left; draw's Y rotation uses that
same sign. This is an illustrative turn cue, not an integrated heading estimate.
It replaces the predicted-path tangent, which could already point back toward
the destination lane while the maneuver was still progressing. Camera lateral
follow is tightened from 1.1 s to 0.65 s to reduce the sideways orbit impression.

### Suspension and four-wheel rolling

The extra lateral lane-change offset/orbit is disabled; steering-based visual yaw
remains. Acceleration drives bounded mass/spring/damper roll, pitch and heave
(8 rad/s, damping ratio 0.65), with numerical substeps. Body travel is limited to
6.5 cm; wheels remain on the ground while the body moves around them. This is
an illustrative suspension driven by acceleration, not measured spring travel.
All four wheel meshes are separated by connected components using
`python3 simulator-runtime/native/split_suspension_wheels.py` (requires numpy).
The generated native/assets/niro-suspension.mesh is independent of device assets.
Wheel rotation uses individual optional carState wheelSpeeds in m/s and a model
radius of 0.3285 m; missing all-zero wheel speeds while moving fall back to vEgo.
Reverse changes the rolling direction. Front wheel steering remains independent.
The effect also runs in manual/orbit view. Wheels may visually alias at high
speed because rotation is rendered at the display frame rate.
`test_suspension.cc` checks settling, frame-rate consistency, bounds and reset.

### Desktop button input

Empty alert overlays are masked to avoid covering the dashboard hit area.
All three controls route accepted Qt touch events to their respective button.
Settings use a direct Qt signal connection; C toggles camera and M opens settings.
A full clean rebuild was required after the menu callback fault. Verified:
settings button opens native settings, close returns, camera toggles on and C
returns to 3D, free demo button still works. No physical comma test performed.

### macOS pointer scaling correction

Removed forced QT_SCALE_FACTOR=0.5: it caused inconsistent rendering/hit positions
across display scale changes. Runtime now uses scale 1. Settings are a normal
nonmodal window instead of a macOS sheet and have their own Escape shortcut.
Actual mouse event tracing confirmed QPushButton presses after this change.
Optional NIRO_TRACE_CLICKS=1 logs input positions only within this app for diagnostics.

### Optional camera suspension experiment

The lower-right “Kamera federt” toggle couples the camera backdrop (including
its annotations) to the same filtered body roll/pitch about the car's body pivot.
Heave uses the vehicle anchor's screen displacement to remain perceptible despite
the distant video plane. Toggle strength eases over 0.4 s; default is off, so the
stable-camera baseline remains available. This uses acceleration-driven virtual
suspension, not integrated raw gyro attitude. Test only in the desktop replay.

Camera suspension damping: roll/pitch now use 35% of the body displacement,
heave 45%, with a separate 0.32 s low-pass response. Vehicle suspension and yaw
are unchanged. Replay rewind resets the camera filter along with the body.

Camera suspension intermediate tuning: 60% roll/pitch, 65% heave, 0.22 s smoothing.

### Graphite paint and path cleanup

Removed decorative cyan center-path motion strokes; lane markings and the
predicted path remain. Body paint uses dark neutral graphite with restrained
clearcoat highlights and subtle derivative-filtered metallic grain. Blue EV
accents remain; ghost vehicles retain their separate muted appearance.
Verified by native desktop build and rendered manual-view inspection.

### Reference-inspired automotive finish

The supplied gray Niro photograph is a visual material reference, not a UV
texture or a calibrated reflectance measurement. Graphite substrate scattering
and a neutral Fresnel clearcoat now reflect a world-space analytic environment
(sky gradient, dark horizon, broad cloud and light sources). Reflections follow
view direction and curved panel normals; fine grain remains object anchored and
filtered at subpixel scale. This approximates the photo's dark-to-silver panel
contrast without baking its road/grass/sky into the vehicle. Original hybrid
mesh geometry and non-paint textures still limit photorealistic matching.

### Subtle camera-view paint reflections

Cooler graphite tint follows the second rear-quarter reference photograph.
Hybrid mode blends 18% of a soft nine-tap raw NV12 camera sample into the paint
environment. Reflection coordinates follow surface normal and view direction;
this is an artistic front-camera colour reflection, not full environment or
ray-traced reflection. Path annotations and UI are excluded. Strength follows
the existing camera crossfade, with zero contribution in 3D mode and on ghost
vehicles. No extra camera readback or framebuffer is required. Native build and
hybrid rendering checked on desktop; physical comma performance is untested.
