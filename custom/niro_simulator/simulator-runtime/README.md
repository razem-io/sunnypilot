# Original sunnypilot / comma Replay auf dem Mac

Separate Entwicklungsumgebung; kein Zugriff auf Fahrzeug-CAN oder das comma-Gerät.

- Quellstand: sunnypilot `737a6c4236e843034680c951005b38d15815363f`, passend zur verwendeten 0.10.1-Gerätebasis.
- `source/tools/replay` ist das originale Replay, kein HTML-Fahrzeugsimulator.
- `build-replay.sh` baut dessen C++-Quellen mit einer kleinen CMake-Rezeptur für macOS, ohne den gesamten Fahrassistenz-Stack oder die Fahrzeugmodelle zu bauen.
- Voraussetzungen: Homebrew capnp, zeromq, eigen, ffmpeg, curl, OpenSSL, CMake; libyuv wird aus den offiziellen Quellen gebaut.
- C++17 wie im originalen SConstruct; Assertions bleiben aktiv (die Bibliothek verwendet sie auch um Initialisierungsaufrufe herum).
- Replay publiziert gezielt Dashboard-/Kamera-Dienste über `--allow`; CAN/sendcan und fremde Ports bleiben unbenutzt.
- Grundlegende Änderung am Nachrichtenbackend: ZMQ-Publisher binden an 127.0.0.1 statt alle Netzwerkschnittstellen. `OPENPILOT_PREFIX` wird vom Mac-ZMQ-Backend nicht unterstützt; daher nur eine lokale Replay-Instanz gleichzeitig.
- Start: `./start-replay.sh`. Leert oder überschreibt keine Fahrzeugaufzeichnungen.
- Leertaste: Pause; s/S: ±10 Sekunden; m/M: ±60 Sekunden; e/d: nächster Assistenzbeginn/-ende; q: beenden.

Die Demo ist eine Toyota-Corolla-Aufzeichnung, kein Niro. Eine Niro-EV-Fahrt ist in der offiziellen Fahrzeug-Testliste referenziert; ihre Verfügbarkeit ist separat zu prüfen. Replay spielt vorhandene Ereignisse ab. MetaDrive für frei steuerbare Simulation ist noch nicht eingerichtet. Die native sunnypilot-Oberfläche kann jetzt separat über `native/start.sh` gestartet werden; siehe `native/README.md`. Der Qualcomm/Wayland-Treiberfehler bleibt gerätespezifisch.

Siehe FAHRZEUGDATEN.md für geprüfte Felder und Einschränkungen. Das Lesen von Live-Replay-Nachrichten braucht einen Subscriber im selben lokalen ZMQ-Backend. Alte Demo-Aufzeichnungen können aktuelle sunnypilot-Dienste wie MADS nicht liefern.

Verifiziert: Replay läuft mit fortschreitender Zeit und Kamera-Decodierung; `inspect-replay.py` hat gültige carState-Nachrichten mit wechselnder Geschwindigkeit und Lenkwinkel empfangen. Nachweis: `../deployment/replay-observed.jsonl`. Die Live-Daten stammen aus der Demo-Aufzeichnung.


## Verbundenes Dashboard

1. In einer eigenen cmux-Task-Workspace vom Projektverzeichnis aus starten: `simulator-runtime/.venv/bin/python simulator-runtime/dashboard_server.py`.
2. In einer zweiten Task-Workspace `simulator-runtime/start-replay.sh` starten. Der Leser darf vorher starten; dann empfängt er auch die selten gesendeten CarParams gleich am Anfang.
3. http://127.0.0.1:8774/ öffnen. Die Seite ist auf 2160 × 1080 angelegt und wird proportional verkleinert.

Der lokale Server liest original publizierte ZMQ-Nachrichten. Er sendet weder CAN noch Steuerbefehle. „Aufgezeichnete Fahrt“ bleibt echte Replay-Eingabe; „Gezielter Test“ ersetzt nur den Speicher-Eingang dieser Vorschau. Die Testfälle schreiben nichts zurück in Replay. Nach den Prüfungen wurde wieder auf Replay geschaltet.

Die originale Demo enthält keinen sunnypilot-MADS-Dienst. Die Freigabelogik bleibt deshalb im Fahrbetrieb gesperrt. Der ausdrücklich bezeichnete Inspektionsmodus lässt uns trotzdem die Modellanzeige ansehen. Zum Test der Umschaltung: Inspektionshäkchen ausschalten und im gezielten Test Assistenz, Rückwärts-Warnung, Manuell oder Datenausfall auswählen. Die native Oberfläche wird lokal durch einen beschrifteten Platzhalter repräsentiert; das ist kein Hardware-Integrationstest.

Alle CarState-Felder werden durchgereicht, einschließlich später hinzukommender Felder. Einheiten und Reaktionsgrenzen der Hauptanzeigen verwenden `telemetry.py`, die Anzeigeentscheidung verwendet `policy.py` direkt aus dem nativen Dashboard-Paket. Die 3D-Webdarstellung und die native Qt-/Wayland-Oberfläche sind verschiedene Renderer. Ein erfolgreicher Browsertest bestätigt daher nicht die Behebung des Qualcomm-Absturzes.

Prüfen:
- `.venv/bin/python -m unittest discover -s . -p test_dashboard.py`: 13 Tests, darunter 800 aufeinanderfolgende Zustandsauswertungen sowie vier unabhängige Türkontakte.
- `.venv/bin/python check_dashboard_http.py`: laufende HTTP-Anbindung, Umschaltfolgen, Werteumrechnung, Testdatenisolation und Eingabevalidierung; stellt abschließend Replay wieder her.
- Ergebnisse unter `../deployment/replay-dashboard-tests.log` und `../deployment/replay-dashboard-http.log`.

Türdecoder, Einzelheiten und Datenverfügbarkeit: `FAHRZEUGDATEN.md`. Native Dateien wurden für diese lokale Anbindung nicht verändert. Der frühere unbestätigte Geräte-Workaround bleibt separat und benötigt weiterhin einen Hardwaretest.


Native Alternative zur HTML-Vorschau: `native/start.sh` startet nun die originale Qt-Oberfläche mit dem Niro-Dashboard und einer aus Modelldaten gezeichneten Straßenansicht. Startparameter, Portanpassungen, lokale MADS-Testannahme und Grenzen stehen in `native/README.md`.


### Native lane-change replay loop (2026-10-20)

Run `./simulator-runtime/start-replay.sh --start 199 --loop-end 225` for the
left lane change starting around 204 s, completing around 210 s. The console
loops the selected interval at normal speed; space pauses and q exits.
Legacy lateralPlan lane-change metadata is migrated into modelV2; migrated
selfdriveState message lengths are measured in Cap'n Proto words correctly.

The native scene tracks low-confidence existing boundaries during a lane change
and unwraps the model's gradual lane-index reassignment once per maneuver.
The reference width comes from the observed lane; lateral visual speed is capped
at 0.9 m/s and the camera follows with damping. Model y (right) maps to negative
OpenGL z so left changes render left. Replay seeks reset the lateral origin.
`simulator-runtime/.venv/bin/python simulator-runtime/test_replay_lane_motion.py`
checks the cached full-rate segment: approximately 3.06 m left and one lane-index
reassignment. Visual checks show leftward ego displacement and camera recentering;
subjective smoothness remains to be assessed in the running loop.

Replay send failures now preserve services for transient EINTR/EAGAIN errors.
Previously any failed send permanently disabled that service and reported a
multiple-publisher error, leaving the dashboard at unknown assist status.
Other send failures still disable the service and now report the actual errno.
