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
