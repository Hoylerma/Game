# EINIGKEIT – Godot-Projekt (Prototyp der Spieloberfläche, Meilenstein M1)

Das Projekt zeigt die Simulation des Kerns **Uhrwerk** auf einer Karte der Testregion 1648:
Kartenansicht mit fünf Kartenmodi, obere Leiste, Staatspanel mit Steuersatz, Ortsinfo,
Chronik und Legende. Die Simulation selbst läuft in C++ und wird über die GDExtension
`uhrwerk_bridge` angesprochen (Schnittstelle: [docs/technik/bridge-api.md](../docs/technik/bridge-api.md)).
Die Godot-Seite **liest** nur und schickt **Befehle**; sie rechnet nichts selbst.

## Voraussetzungen

| Was | Woher |
|---|---|
| Godot **4.4.1** (Standard-Build, kein .NET) | [godotengine.org](https://godotengine.org/download/archive/4.4.1-stable/); im Repository optional unter `tools/godot/` (nicht versioniert) |
| GDExtension `game/bin/libuhrwerk_bridge.so` (Linux), `uhrwerk_bridge.dll` (Windows), `libuhrwerk_bridge.dylib` (macOS) | `cmake --preset bridge && cmake --build --preset bridge` im Repository-Hauptordner |
| Spieldaten | `data/` im Repository (oder Umgebungsvariable `UHRWERK_DATEN`) |
| Karte `data/karte/provinzen.png` und `definition.csv` | `python3 tools/kartengenerator/kartengenerator.py` |

Ohne Erweiterung startet das Projekt trotzdem fehlerfrei und zeigt einen deutschen Hinweis,
wie sie gebaut wird. Fehlt die Karte, erscheint ebenfalls ein Hinweis mit dem Befehl des
Kartengenerators; Leisten und Panels funktionieren dann weiter.

## Starten

```bash
# einmalig bzw. nach neuen Dateien: Projekt importieren (legt game/.godot/ an)
godot --headless --path game --import

# Spiel starten
godot --path game

# im Editor öffnen
godot --editor --path game
```

`godot` steht für die Godot-4.4.1-Programmdatei, z. B. `tools/godot/Godot_v4.4.1-stable_linux.x86_64`.
Der Import ist nötig, weil Godot erst dabei die globalen Klassennamen (`class_name`) registriert
und das Programmsymbol importiert; der Editor erledigt das beim Öffnen von selbst.

### Datenordner

Standard ist `../data` relativ zum Projektordner (also `data/` im Repository). Die
Umgebungsvariable `UHRWERK_DATEN` überschreibt ihn; relative Angaben beziehen sich auf den
Projektordner `game/`:

```bash
UHRWERK_DATEN=/tmp/meine-daten godot --path game
```

### Screenshots von der Kommandozeile

Optionen stehen nach `--` (Benutzerargumente):

| Option | Bedeutung |
|---|---|
| `--screenshot=<pfad>` | Nach dem Laden simulieren, etwa 20 Bilder warten, den Viewport als PNG speichern und beenden |
| `--tage=<n>` | Simulierte Tage vor dem Screenshot (Standard 60) |
| `--kartenmodus=<0..4>` | 0 Politisch, 1 Bevölkerungsdichte, 2 Konfession, 3 Gelände, 4 Verwüstung |
| `--ort=<ort_id>` | Ort auswählen, Ortsinfo geöffnet (z. B. `berlin_coelln`) |

```bash
xvfb-run -a -s "-screen 0 1600x900x24" \
  godot --path game --rendering-driver opengl3 --audio-driver Dummy \
  -- --screenshot=/tmp/einigkeit.png --tage=365 --kartenmodus=1 --ort=berlin_coelln
```

Relative Pfade beziehen sich auf das Startverzeichnis (`$PWD`). Exitcode 0, wenn das Bild
gespeichert wurde und Simulation und Karte geladen waren; sonst 1 (das Bild des
Fehlerzustands wird trotzdem gespeichert, sofern eine Grafikausgabe vorhanden ist). Im
Modus `--headless` gibt es kein Bild; ohne Bildschirm hilft `xvfb-run`.

## Bedienung

| Eingabe | Wirkung |
|---|---|
| Linksklick auf die Karte | Ort auswählen, Ortsinfo öffnen (Klick ins Leere schließt sie) |
| Esc | Auswahl aufheben |
| Rechte oder mittlere Maustaste ziehen | Karte verschieben |
| Mausrad | Zoomen (zum Mauszeiger hin, mit Grenzen) |
| W A S D, Pfeiltasten | Karte verschieben |
| Leertaste | Pause / weiter |
| 1 – 5 | Geschwindigkeit: 2, 5, 15, 40 oder 120 Tage je Sekunde |
| Q W E R T | Kartenmodus Politisch, Bevölkerungsdichte, Konfession, Gelände, Verwüstung |
| Steuersatz-Regler | Steuersatz des Spielerstaates (wirksam ab dem nächsten simulierten Tag) |
| Klick auf einen Chronikeintrag | Ort des Ereignisses auswählen und anfahren |

**Doppelbelegung von W:** W gehört sowohl zur Kamerasteuerung (WASD) als auch zum
Kartenmodus „Bevölkerungsdichte“. Kurz getippt (unter 0,18 s) schaltet W den Kartenmodus um,
gehalten bewegt es die Karte nach oben. Die Pfeiltaste nach oben bewegt immer sofort.

Die Tastenbelegung steht als Aktionen in `project.godot` (`kamera_*`, `kartenmodus_*`,
`tempo_*`, `auswahl_aufheben`) und lässt sich im Editor unter *Projekteinstellungen →
Eingabebelegung* ändern. Es werden physische Tasten verwendet, die Belegung gilt also auch auf
QWERTZ-Tastaturen an derselben Stelle.

## Aufbau

```
game/
├── project.godot              Name, Hauptszene, Fenster 1600×900 (canvas_items/expand),
│                              Renderer gl_compatibility, Eingabebelegung, Theme
├── icon.svg
├── bin/uhrwerk.gdextension    Beschreibung der GDExtension (Bibliotheken je Plattform)
├── scenes/
│   ├── main.tscn              Hauptszene: Karte + Oberfläche (CanvasLayer)
│   ├── map/map_view.tscn      Kartenbild (Sprite2D mit Shader) und Kamera
│   └── ui/                    top_bar, state_panel, chronicle_panel, location_panel,
│                              legend_panel, map_mode_bar, map_tooltip, notice_layer
├── scripts/
│   ├── main.gd                Ablaufsteuerung: Laden, Zeit, Eingaben, Screenshot
│   ├── core/                  Hilfsklassen ohne Szenenbaum (auch im --script-Modus nutzbar)
│   │   ├── uhrwerk_paths.gd       UhrwerkPaths: Datenordner (UHRWERK_DATEN oder res://../data)
│   │   ├── simulation_client.gd   SimulationClient: typisierte Hülle um UhrwerkSimulation
│   │   ├── map_data.gd            MapData: provinzen.png + definition.csv, Kartenindex ↔ Ort
│   │   ├── map_config.gd          MapConfig: karte.chron (Projektion, Küste)
│   │   ├── map_modes.gd           MapModes: Kartenmodi, Namen, Tasten
│   │   ├── german_format.gd       GermanFormat: Tausenderpunkt, Dezimalkomma, Einheiten
│   │   └── launch_options.gd      LaunchOptions: --screenshot, --tage, --kartenmodus, --ort
│   ├── map/                   map_view.gd (Darstellung, Auswahl, Rahmen), map_camera.gd,
│   │                          map_labels.gd (Ortsnamen)
│   └── ui/                    Skripte der Panels, Legendenberechnung, Farbpalette
├── shaders/map.gdshader       Kartenshader (canvas_item)
├── theme/einigkeit_theme.tres Theme der Oberfläche (Papier und Tinte)
└── tests/                     Einheiten- und Rauchtest, siehe tests/README.md
```

### Simulation

`SimulationClient` spricht die Klasse `UhrwerkSimulation` nur über `ClassDB` an. Ein direkter
Verweis auf den Klassennamen wäre ohne gebaute Erweiterung ein Parse-Fehler; so startet das
Projekt in jedem Fall. Beim Start prüft die Hülle, ob alle Methoden der Schnittstelle vorhanden
sind, und meldet eine veraltete Erweiterung verständlich.

Die Zeit läuft in `main.gd`: Je Bild werden die fälligen ganzen Tage mit `advance_days`
simuliert (höchstens 30 je Bild), danach liest die Oberfläche Datum, Staat, Ort, Kartenfarben und
neue Chronikeinträge. Der Steuersatz-Regler schickt `set_tax_rate_percent`; der Kern führt den
Befehl am nächsten Tag aus, bis dahin zeigt das Panel „gilt ab dem nächsten Tag“.

### Karte

- `provinzen.png` wird mit `Image.load_from_file` geladen. Das `Image` bleibt für die
  Klick-Erkennung im Speicher, eine `ImageTexture` (Filter NEAREST) dient der Darstellung.
- `definition.csv` ordnet Kartenindizes Ort-Kennungen zu; `get_location_index` liefert daraus
  den Simulationsindex. Beide Indizes sind unabhängig.
- Der Shader dekodiert je Pixel den Kartenindex aus `R + G × 256` und holt die Farbe aus einer
  1 Pixel hohen Nachschlagetextur (Breite = Anzahl Kartenindizes, `texelFetch`). Eine zweite
  Nachschlagetextur enthält den Besitzer je Ort für die Staatsgrenzen.
- Grenzen entstehen durch Nachbarvergleich im Bildschirmraum, damit sie bei jedem Zoom gleich
  dick sind: Ortsgrenzen dünn und hell, Staatsgrenzen kräftig, Küste und Rand zu Gebieten ohne
  Ort am stärksten. Ausgewählter Ort: Umriss in Siegelrot; überfahrener Ort: aufgehellt.
- Gebiete ohne Ort: Mit `karte.chron` erkennt der Shader das Meer nördlich der Küstenlinie
  (gleiche Regel wie der Kartengenerator) und zeigt unerfasstes Land schraffiert; ohne die Datei
  gilt jede Fläche ohne Ort als Meer. Die Meerfarbe ist eine Uniform (`sea_color`).
- Papierton und eine feine Körnung geben der Karte die Anmutung eines gedruckten Atlas; der
  Rahmen mit Gradeinteilung folgt den Grenzen aus `karte.chron`.
- Ortsnamen (`MapLabels`) werden im Bildschirmraum gezeichnet und bleiben dadurch bei jedem Zoom
  scharf. Sie stehen im Schwerpunkt der Ortsfläche; Hauptstädte erscheinen immer, Städte ab
  mittlerem Zoom, alle übrigen Orte erst beim Hineinzoomen. Ein Name, der einen bereits gesetzten
  überdecken würde, entfällt.

### Legenden

Die Legenden werden aus den Farben der Simulation (`get_map_colors`) abgeleitet, damit sie
immer zur Karte passen. Für Dichte und Verwüstung schätzt `LegendData` den Verlauf aus den
Orten: Passt eine lineare Abbildung Wert → Farbe, zeigt die Legende die exakten Endfarben,
sonst die tatsächlich vorkommenden Farben.

## Bekannte Grenzen des Prototyps

- Wappen sind schlichte Schilde in der Staatsfarbe.
- Die Nachschlagetexturen sind einzeilig; mehr als 16.384 Kartenindizes (übliche
  Texturgrenze) brauchen später eine zweizeilige Anordnung.
- Ortsnamen stehen im Schwerpunkt der Fläche, nicht an der Lage des Hauptortes (die Brücke
  liefert keine Koordinaten).
