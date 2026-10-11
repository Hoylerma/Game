# Tests der Godot-Seite

| Test | Braucht die GDExtension | Grafikausgabe | Ausgabe bei Erfolg |
|---|---|---|---|
| `unit_test.gd` – Hilfsklassen ohne Simulation | nein | nein | `UNIT OK (n Prüfungen)` |
| `smoke_test.gd` – Brücke und Kartendaten | ja | nein | `SMOKE OK` |
| `klick_test.gd` – Klickerkennung für jeden Ort | ja | nein | `KLICK OK n/n` |
| `leistung_test.gd` – Bildzeiten beim Schwenken und Zoomen | ja | ja (z. B. xvfb) | Tabelle und `LEISTUNG OK` |

`godot` steht für Godot 4.4.1, z. B. `tools/godot/Godot_v4.4.1-stable_linux.x86_64`. Alle Tests
laufen im `--script`-Modus und brauchen einen vorherigen Import, damit Godot die Klassennamen
(`class_name`) kennt:

```bash
godot --headless --path game --import
```

Datenordner ist wie im Spiel `data/` im Repository oder `$UHRWERK_DATEN`. Ein Skriptfehler
bricht im `--script`-Modus nur die betroffene Funktion ab; die Ausgabe sollte deshalb auch auf
`SCRIPT ERROR` geprüft werden.

## Einheitentests (`unit_test.gd`)

Prüft ohne Simulation: deutsche Zahlenformate (`GermanFormat`), Kommandozeilenoptionen
(`LaunchOptions`, auch `--zoom` und `--kartenmodus=5`), das Lesen von `karte.chron` und die
Projektion (`MapConfig`, das M1-Feld `kueste` wird ignoriert), die Pfadauflösung
(`UhrwerkPaths`), Legenden (`LegendData`: Verlaufsschätzung, Eintrag „Keine Daten“ mit
Schraffur), die Kartenmodi samt Kreise-Modus und die Eingabeaktionen aus `project.godot`, die
Detailstufen (`MapDetail`: je Ortsgröße genau eine Namen-Ebene, Grenzen nach Zoom; bei leerer
Ebene der Rückfall auf die nächste, `MapLabels.shown_tier`) und `LocationCatalog` (auch die
Zählung der Kreise mit Orten für die Legende).

Dazu legt der Test unter `user://unit_test_karte` eine kleine Testkarte an (8 × 6 Pixel mit
drei Orten, Meer, See und Ausland), lädt sie mit `MapData` (Kartenindex und Flächenart je
Pixel, Beschriftungspunkt, Pixelzahl, Fläche, Nachschlagetextur 256 Spalten, Hinweis bei
fehlender `fluesse.png`, Ablehnung einer `definition.csv` vor M3), prüft daran
`MapLabelLayout` (Bündelung zu Gebieten, pixelgewichteter Schwerpunkt, Ausweichen auf den
nächsten Ort, Staats- und Kreisnamen) und löscht den Ordner wieder.

```bash
godot --headless --path game --script res://tests/unit_test.gd
```

Am Ende steht `UNIT OK` (Exitcode 0) oder `UNIT FEHLGESCHLAGEN` mit allen Fehlern (Exitcode 1).

## Rauchtest (`smoke_test.gd`)

Prüft die GDExtension und die Kartendaten ohne Szene und ohne Grafikausgabe:

1. `ClassDB.class_exists("UhrwerkSimulation")` und alle Methoden der Schnittstelle
   ([bridge-api.md](../../docs/technik/bridge-api.md)) vorhanden
2. vor dem Laden leere Abfragen (auch Indexarrays und Kreise); ein falscher Datenordner
   liefert eine Fehlermeldung
3. Daten laden; Anzahl Staaten und Orte, Spielerstaat, Bevölkerung und Fläche > 0; alle
   Felder von `get_state` und `get_location` (seit M3 mit `has_data`, `kreis_index`,
   `kreis_name`); `get_location_index` passt zu jedem Ort
4. ungültige Indizes und Befehle werden abgewiesen (leere Rückgabe bzw. `false`, auch
   `get_kreis` und `get_kreis_index`)
5. `get_map_colors` liefert für alle sechs Kartenmodi eine Farbe je Ort
6. Orte ohne Daten und Kreise: `get_location_owner_indices` und `get_location_kreis_indices`
   passen zu `get_location`; Orte ohne Daten haben keinen Besitzer und keine Bevölkerung;
   `get_kreis` hat alle Felder, `get_kreis_index` passt, `location_count` stimmt; Alpha der
   Kartenfarben ist 0 genau für Orte ohne Daten in den Modi 0, 1, 2 und 4, sonst 1
7. Steuerbefehl: `set_tax_rate_percent` wird angenommen und ist nach einem Tag wirksam
8. 365 Tage simulieren; Datum (Kalenderrechnung im Test), `get_days_elapsed`, Datumstext
9. Einnahmen, Ausgaben und Produktion > 0
10. Chronik: Größe, Einträge mit allen Feldern, leere Abfrage hinter dem Ende
11. Prüfsumme 16-stellig hexadezimal; ein zweiter Lauf mit denselben Befehlen ergibt dieselbe
12. Karte (Fassung M3): `provinzen.png`, `fluesse.png` (Graustufen, gleiche Größe) und
    `definition.csv` (neue Kopfzeile) vorhanden; jeder Eintrag der CSV über
    `get_location_index` auflösbar, jeder Ort der Simulation in der CSV, keine Lücken;
    Bildgröße wie in `karte.chron`; Blaukanal ist eine Flächenart 0..3, bei B ≠ 0 kein Ort;
    alle Kartenindizes bekannt; jeder Ort mit mindestens einem Pixel, Pixelzahl je Ort wie in
    der CSV, jeder Beschriftungspunkt im eigenen Ort

Am Ende steht `SMOKE OK` (Exitcode 0) oder `SMOKE FEHLGESCHLAGEN` mit allen Fehlern (Exitcode 1).

```bash
# aus dem Repository-Hauptordner, nach dem Import
godot --headless --path game --script res://tests/smoke_test.gd

# anderer Datenordner (z. B. mit frisch erzeugter Karte)
UHRWERK_DATEN=/tmp/daten godot --headless --path game --script res://tests/smoke_test.gd

# ohne Kartenprüfung
godot --headless --path game --script res://tests/smoke_test.gd -- --ohne-karte
```

## Klicktest (`klick_test.gd`)

Abnahme M3 „Klickerkennung für alle Orte richtig“. Der Test startet die Hauptszene mit den
echten Daten und prüft:

1. **Jeden Ort der `definition.csv` bei drei Zoomstufen** (fern = Übersicht, mittel, nah;
   `MapDetail.LEVEL_SIZES`): Der Beschriftungspunkt des Ortes (ein Pixel, das laut Generator
   sicher zum Ort gehört) wird mit der Zeichentransformation der Karte – der
   Canvas-Transformation, mit der Godot das Kartenbild tatsächlich zeichnet – in
   Bildschirmkoordinaten umgerechnet und über die Auswahlfunktion der Karte
   (`MapView.location_at_screen`, dieselbe wie bei Klick und Hervorhebung) aufgelöst. Die
   gefundene Ortskennung muss die des Ortes sein. Die Kamera fährt die Karte dazu in
   Kacheln ab (Übersicht 1, mittel 6–12, nah 30–48 Kacheln), damit die Punkte an
   verschiedenen Bildschirmstellen liegen.
2. **Stichproben über echte Eingabeereignisse:** je Zoomstufe rund 40 Orte. Die Kamera legt
   den Beschriftungspunkt auf eine zufällige (fest geseedete) Stelle, die nicht unter einem
   Panel liegt; dann geht ein Linksklick über `Input.parse_input_event` durch die ganze
   Eingabekette (Fenster → Viewport → GUI → Karte). Danach muss die Hauptszene genau diesen
   Ort ausgewählt haben. Ein Klick ins Meer muss die Auswahl aufheben.
3. **Namen und Panels** (zuerst, solange die Ortsinfo noch nie angezeigt wurde;
   `MapLabels.record_drawn` hält die gezeichneten Namen fest): Nach einem Wechsel vom Modus
   „Kreise“ zu „Politisch“ (die Legende ändert ihre Größe) und nach der ersten Auswahl per
   Linksklick (die Ortsinfo erscheint) liegt ohne Kamerabewegung kein Name unter einem Panel;
   in der mittleren Stufe bleibt der Kreisname Wilhelmshaven stehen, wenn der gleichnamige Ort
   ausgewählt ist; die ausgewählte Hauptstadt Berlin-Cölln steht im Hauptstadt-Stil.

```bash
godot --headless --path game --script res://tests/klick_test.gd

# mit echtem Fenster 1600 × 900 (headless ist das Fenster 64 × 64, der Viewport 1600 × 1600)
xvfb-run -a -s "-screen 0 1600x900x24" godot --path game --rendering-driver opengl3 \
  --audio-driver Dummy --script res://tests/klick_test.gd
```

Ausgabe mit den echten Daten (4.542 Orte) unter xvfb:

```
  Namen und Panels: 4 Prüfungen
  fern (Zoom 0.182, 1 Kacheln): 4542/4542 Orte richtig
  mittel (Zoom 0.747, 12 Kacheln): 4542/4542 Orte richtig
  nah (Zoom 1.838, 48 Kacheln): 4542/4542 Orte richtig
  Eingabeereignisse: 122/122 Klicks richtig (1 Stichproben unter Panels übersprungen)
  Klick ins Meer hebt die Auswahl auf
KLICK OK 4542/4542
```

`n` in `KLICK OK n/n` zählt die Orte mit Fläche, die bei allen drei Zoomstufen richtig
erkannt wurden. Bei Fehlern steht `KLICK FEHLGESCHLAGEN` mit den ersten 20 Abweichungen
(Exitcode 1). Laufzeit headless etwa 4 s, unter xvfb etwa 25 s.

## Leistungstest (`leistung_test.gd`)

Startet die Hauptszene mit den echten Daten, schaltet V-Sync und die Bildratenbremse ab und
bewegt die Karte über echte Eingabeereignisse (Ziehen mit der rechten Maustaste, Mausrad).
Je Abschnitt (Ruhe in der Übersicht, Schwenken fern/mittel/nah, Zoomfahrt, laufende
Simulation mit 120 Tagen je Sekunde, Kartenmodus Kreise) misst er die Abstände zwischen zwei
Bildern und gibt Mittel, Median, 95-%-Wert, Höchstwert und Bilder je Sekunde aus, dazu die
Zeit für die Namen (GDScript) und die Renderzeit des Viewports laut `RenderingServer`.
Grenzwerte prüft der Test nicht, weil die Zahlen stark vom Rechner abhängen.

```bash
xvfb-run -a -s "-screen 0 1600x900x24" godot --path game --rendering-driver opengl3 \
  --audio-driver Dummy --script res://tests/leistung_test.gd -- --bilder=150
```

Ergebnis unter xvfb mit dem Software-Renderer llvmpipe (4 Kerne, keine Grafikkarte):

```
Abschnitt                               Mittel  Median    95 %     Max Bilder/s    Namen   Render Namen#
Ruhe, Übersicht                          31.34   30.93   37.02   39.33    31.9     0.00    12.15      5
Schwenken, Übersicht                     30.63   29.84   35.61   43.70    32.6     0.98    12.06      5
Schwenken, mittel                        61.29   61.22   69.19  117.08    16.3     4.82    16.95     76
Schwenken, nah                           60.64   60.40   70.05   74.64    16.5     8.37    20.29    152
Zoomfahrt (Mausrad)                      54.31   55.86   71.52   80.20    18.4     3.78    14.96    172
Simulation 120 Tage/s, mittel            60.08   59.21   68.62   72.98    16.6     0.00    18.38    122
Kartenmodus Kreise, Schwenken mittel     61.73   59.07   72.40  276.30    16.2     4.89    18.61     83
```

Die Zeit geht fast ganz in das Rastern im Software-Renderer (Kartenshader etwa 40 ms,
Panels etwa 14 ms); Einordnung und beseitigte Engpässe stehen in
[game/README.md](../README.md#leistung). Der einzelne Höchstwert von 276 ms im letzten
Abschnitt ist ein Ausreißer; ein Moduswechsel selbst kostet 4–12 ms.

## Start ohne Erweiterung

Das Projekt muss auch ohne `game/bin/libuhrwerk_bridge.so` ohne Skriptfehler starten und einen
Hinweis zeigen:

```bash
godot --headless --path game --quit-after 30 2>&1 | grep -E "SCRIPT ERROR|Parse Error" && echo "Fehler!"
```

Godot selbst meldet dabei, dass die Bibliothek der Erweiterung fehlt
(`ERROR: GDExtension dynamic library not found`); das ist erwartet.

## Screenshot

```bash
xvfb-run -a -s "-screen 0 1600x900x24" \
  godot --path game --rendering-driver opengl3 --audio-driver Dummy \
  -- --screenshot=/tmp/einigkeit.png --tage=60 --kartenmodus=0 --ort=berlin_coelln --zoom=4
```

`--audio-driver Dummy` vermeidet Warnungen auf Rechnern ohne Soundkarte. Mit dem
Software-Renderer (llvmpipe) meldet Godot außerdem, dass sich V-Sync nicht einstellen lässt;
auch das ist erwartet.
