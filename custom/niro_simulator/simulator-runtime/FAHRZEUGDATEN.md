# Fahrzeugdaten für das Niro-Dashboard

Geprüft am Quellstand des verwendeten sunnypilot-Releases. `CarState` vereinheitlicht Feldnamen und Einheiten; es garantiert nicht, dass jedes Auto jedes Feld befüllt. Ein Defaultwert von null ist kein Nachweis für einen Sensorwert.

| Daten | Gemeinsames Interface | Niro-/Hyundai-Auswertung im Quellcode | Dashboard-Status |
|---|---|---|---|
| Geschwindigkeit, Stillstand | `vEgo`, `vEgoCluster`, `standstill` | vorhanden | Geschwindigkeit am Auto bestätigt |
| Vier Raddrehzahlen | `wheelSpeeds` | vorhanden | in der lokalen Datentabelle |
| Lenkradwinkel, Winkelgeschwindigkeit | `steeringAngleDeg`, `steeringRateDeg` | vorhanden | Winkel und 3D-Vorderräder am Auto bestätigt |
| Lenkmoment / Fahrer lenkt mit | `steeringTorque`, `steeringPressed` | vorhanden | in der lokalen Datentabelle |
| Gang P/R/N/D | `gearShifter` | EV-Gangsignal vorhanden | Live-Ganganzeige und gezielte Testfälle |
| Blinker links / rechts | `leftBlinker`, `rightBlinker` | vorhanden | Blinker am 3D-Modell und Statusanzeige |
| Mindestens eine Tür offen | `doorOpen` | vier Türkontakte werden zusammengefasst | Warnhinweis integriert; keine individuelle Tür aus diesem Feld ableitbar |
| Fahrergurt offen | `seatbeltUnlatched` | vorhanden | Statusanzeige integriert |
| Bremse betätigt | `brakePressed` | vorhanden; kann bei HEV/EV auch manuelle Rekuperation umfassen | in der lokalen Datentabelle |
| Bremspedalstärke | `brake` | in diesem Parser ausdrücklich auf 0 gesetzt | nicht als echten Messwert verwenden |
| Gaspedal betätigt | `gasPressed` | vorhanden | in der lokalen Datentabelle |
| Parkbremse / Auto Hold | `parkingBrake`, `brakeHoldActive` | vorhanden | in der lokalen Datentabelle |
| Tempomat / Solltempo | `cruiseState` | vorhanden, konfigurationsabhängig | von openpilot-Aktivität unterscheiden |
| Totwinkel links/rechts | `leftBlindspot`, `rightBlindspot` | nur bei aktiviertem BSM | Ausstattung und Live-Daten prüfen |
| Serien-Notbremsung / Kollisionswarnung | `stockAeb`, `stockFcw` | konfigurationsabhängig | Warnungen müssen Vorrang behalten |
| CAN-Gültigkeit / Ausfall | `canValid`, `canTimeout` | vorhanden | ungültige Daten nicht darstellen |
| Ladestand / Laden | `fuelGauge`, `charging` | Interface vorhanden, im geprüften Niro-Pfad nicht befüllt | zusätzliche Zuordnung erforderlich |
| Reifendruck, Zell-/Motortemperatur, Hochvolt-Leistung | kein entsprechendes allgemeines CarState-Feld in diesem Stand | zusätzliche Auswertung nötig | Demo-Werte sind keine Live-Daten |

## Assistenz und Umschaltung

Die tatsächliche Assistenzaktivität stammt zusätzlich aus `carControl`, `selfdriveState` und sunnypilots `selfdriveStateSP` (MADS). Tempomat-Schalter oder Gang allein sagen nicht, ob gelenkt wird. Unser Dashboard gibt die normale Oberfläche bei aktiver Assistenz, Warnmeldungen oder fehlenden frischen Pflichtdaten frei.

## Offizielle Testwerkzeuge

- **Replay:** originale aufgezeichnete Nachrichten mit Zeitverlauf, Pause, Suche und Wiederholung. Keine frei steuerbare Fahrzeugphysik.
- **MetaDrive-Bridge:** echte Fahrzeugsimulation mit openpilot; separates Setup und höherer Grafik-/Modellbedarf. Unterstützte Eingaben sind nicht automatisch alle CarState-Felder.
- **Öffentliche Demo:** im Replay über `--demo` verfügbar; kein persönlicher Account erforderlich.
- **Niro-EV-Testfahrt:** `50c6c9b85fd1ff03/2020-10-26--17-56-06`, referenziert in `opendbc/car/tests/routes.py`. Referenz vorhanden. Direkter Zugriff über die Route-API am 20.10.2026: HTTP 403; keine Niro-Fahrt geladen. Andere Testdatenquellen und enthaltene Manöver bleiben zu prüfen.

Die Routenliste und Demo-Referenzen liegen im Repository; die großen Aufzeichnungen werden separat geladen. Ältere Aufzeichnungen enthalten nicht zwingend aktuelle sunnypilot-/MADS-Nachrichten. Fehlende Dienste müssen in einer isolierten Testumgebung explizit ergänzt werden, nicht als reale Messwerte ausgegeben.

Quellen: `source/opendbc_repo/opendbc/car/car.capnp`, `source/opendbc_repo/opendbc/car/hyundai/carstate.py`, `source/tools/replay/README.md`, `source/tools/sim/README.md`. Quellcommit: `737a6c4236e843034680c951005b38d15815363f`.

## Laufende lokale Anbindung

Vorschau: http://127.0.0.1:8774/ . Alle empfangenen CarState-Felder werden unverändert durchgereicht und in der durchsuchbaren Tabelle angezeigt. Die große Darstellung nutzt Geschwindigkeit, Lenkradwinkel, daraus angenäherten Radeinschlag, Gang, Blinker, Bremse/Gas, Türstatus, Gurt und Totwinkel. Die Radeinschläge sind keine einzeln gemessenen Radwinkel und enthalten keine Ackermann-Geometrie. Übersetzung: CarParams, ansonsten eine frische gültige liveParameters-Schätzung; Herkunft ist sichtbar.

Frische CarState-Nachricht und canValid sind für Live-Anzeigen erforderlich. Die Rohdatentabelle kennzeichnet alte Nachrichten. Ein Feld in der Tabelle bedeutet weiterhin nicht, dass ein Fahrzeug dessen Sensor unterstützt. Insbesondere Batterie, Temperaturen und Reifendruck werden nicht aus Defaultwerten erfunden.

## Vier einzelne Niro-Türkontakte

Für den ersten Niro EV, nicht CAN-FD, im Hyundai-Generic-DBC:

| Tür | CAN-Nachricht | Signal | Bitposition / Länge, Little Endian |
|---|---|---|---|
| Fahrer | CGW1, 1345 / 0x541 | CF_Gway_DrvDrSw | 8 / 2 |
| Beifahrer | CGW1, 1345 / 0x541 | CF_Gway_AstDrSw | 35 / 1 |
| Hinten links | CGW2, 1363 / 0x553 | CF_Gway_RLDrSw | 24 / 1 |
| Hinten rechts | CGW2, 1363 / 0x553 | CF_Gway_RRDrSw | 23 / 1 |

`niro_doors.py` ist ein separat getesteter, rein lesender Decoder. Er verlangt eine bestätigte Niro-Zuordnung und frische 8-Byte-Nachrichten vom richtigen Bus. Fehlende Kontakte bleiben unbekannt; Fahrerwerte 2/3 werden nicht als eindeutiger Türzustand ausgegeben. Der Serienparser verwendet hier `any(...)` und fasst damit die Kontakte zusammen.

Der Decoder hängt noch nicht am Fahrzeug-CAN. Das Toyota-Replay wird bewusst nicht mit Niro-Bitpositionen interpretiert. In der Vorschau wird deshalb nur der tatsächlich vorhandene Sammelkontakt gezeigt. Für aufklappende einzelne 3D-Türen müssen zusätzlich die Türmeshes sauber getrennt und Scharnierpunkte gesetzt werden.
