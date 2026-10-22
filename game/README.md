# EINIGKEIT – Godot-Projekt (Prototyp der Spieloberfläche, Meilensteine M1 bis M6)

Das Projekt zeigt die Simulation des Kerns **Uhrwerk** auf der Karte des **Kernraums**
(Meilenstein M3: rund 4.500 Orte in rund 620 Kreisen): Kartenansicht mit zehn Kartenmodi
(seit M4 auch Lebensstandard und Kultur, seit M5 Wirtschaftsleistung und Märkte), Zoomstufen mit
Staats-, Kreis- und Ortsnamen, obere Leiste, Staatspanel mit Reglern für Kontribution, Akzise und
Zoll und einem Haushaltsfenster (seit M5), Ortsinfo mit Wirtschaft (Markt, Einkommen und
Bedarfsdeckung je Beruf, Betriebe, Güter; seit M5) und Bevölkerung (Kulturen, Lebensstandard,
Alphabetisierung, Altersaufbau, Bilanz des Vorjahres), Chronik und Legende, seit M6 ein Spielmenü
mit Speichern und Laden, Schnellspeichern (F5/F9) und automatischem Spielstand am 1. Januar. Die Simulation selbst läuft in C++ und wird über die
GDExtension `uhrwerk_bridge` angesprochen (Schnittstelle:
[docs/technik/bridge-api.md](../docs/technik/bridge-api.md)). Die Godot-Seite **liest** nur
und schickt **Befehle**; sie rechnet nichts selbst.

## Voraussetzungen

| Was | Woher |
|---|---|
| Godot **4.4.1** (Standard-Build, kein .NET) | [godotengine.org](https://godotengine.org/download/archive/4.4.1-stable/); im Repository optional unter `tools/godot/` (nicht versioniert) |
| GDExtension `game/bin/libuhrwerk_bridge.so` (Linux), `uhrwerk_bridge.dll` (Windows), `libuhrwerk_bridge.dylib` (macOS) | `cmake --preset bridge && cmake --build --preset bridge` im Repository-Hauptordner |
| Spieldaten | `data/` im Repository (oder Umgebungsvariable `UHRWERK_DATEN`) |
| Karte `data/karte/provinzen.png`, `fluesse.png` und `definition.csv` (Fassung M3) | `python3 tools/kartengenerator/kartengenerator.py` |

Ohne Erweiterung startet das Projekt trotzdem fehlerfrei und zeigt einen deutschen Hinweis,
wie sie gebaut wird. Fehlt die Karte oder hat `definition.csv` noch das Format vor M3,
erscheint ebenfalls ein Hinweis mit dem Befehl des Kartengenerators; Leisten und Panels
funktionieren dann weiter. Fehlt nur `fluesse.png`, zeigt die Karte keine Flüsse.

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
| `--kartenmodus=<0..13>` | 0 Politisch, 1 Bevölkerungsdichte, 2 Konfession, 3 Gelände, 4 Verwüstung, 5 Kreise, 6 Lebensstandard, 7 Kultur (6 und 7 seit M4), 8 Wirtschaft, 9 Märkte (seit M5), 10 Verwaltung, 11 Eisenbahnen (beide seit M7), 12 Wahlen (seit M8), 13 Zollgebiete (seit M9) |
| `--ort=<ort_id>` | Ort auswählen, Ortsinfo geöffnet (z. B. `berlin_coelln`) |
| `--zoom=<faktor>` | Vergrößerung gegenüber der Übersicht (1 = ganze Karte), zentriert auf `--ort`, sonst auf die Kartenmitte (bis zum Faktor 1 bleibt die Bildmitte der Übersicht); gilt auch ohne `--screenshot` |
| `--haushalt` | Haushaltsfenster des Spielerstaates vor dem Screenshot öffnen (seit M5) |
| `--verfassung` | Fenster „Verfassung und Gruppen“ des Spielerstaates vor dem Screenshot öffnen (seit M8; nur in Szenarien mit Parlament, 1848) |
| `--zollunion` | Fenster „Zollunion“ des Spielerstaates vor dem Screenshot öffnen (seit M9; nur in Szenarien mit Zollunion, 1848) |
| `--spielmenue` | Spielmenü vor dem Screenshot öffnen (seit M6) |
| `--szenario=<id>` | Szenario beim Start, z. B. `voelkerfruehling` (seit M7; Standard `westfaelischer_frieden`); gilt auch ohne `--screenshot` |
| `--staat=<TAG>` | Spielerstaat beim Start, z. B. `BAV` (seit M7; Standard: Spieler des Szenarios) |
| `--wahlabend` | Wahlabend der letzten Wahl mit dem ganzen Ergebnis vor dem Screenshot öffnen (seit M8; dafür `--tage` über den Wahltag, 1848 z. B. `--tage=62`) |
| `--partei=<strömung>` | Parteienmodus mit dieser Strömung beim Start, z. B. `liberale` (seit M8; nur in Szenarien mit Parlament, 1848) |

Auf der Kernraum-Karte bei 1600 × 900 Pixeln reicht die ferne Stufe (Staatsnamen) bis etwa
`--zoom=2,7`, die mittlere (Kreisnamen) bis etwa `--zoom=6,3`, darüber folgt die nahe
(Ortsnamen); z. B. `--zoom=1`, `--zoom=4` und `--zoom=10`. Die Schwellen stehen unter
[Detailstufen](#detailstufen). Auf der Kommandozeile ist der Dezimalpunkt zu verwenden
(`--zoom=2.5`).

```bash
xvfb-run -a -s "-screen 0 1600x900x24" \
  godot --path game --rendering-driver opengl3 --audio-driver Dummy \
  -- --screenshot=/tmp/einigkeit.png --tage=365 --kartenmodus=5 --ort=nuernberg --zoom=4
```

Relative Pfade beziehen sich auf das Startverzeichnis (`$PWD`). Exitcode 0, wenn das Bild
gespeichert wurde und Simulation und Karte geladen waren; sonst 1 (das Bild des
Fehlerzustands wird trotzdem gespeichert, sofern eine Grafikausgabe vorhanden ist). Im
Modus `--headless` gibt es kein Bild; ohne Bildschirm hilft `xvfb-run`.

## Bedienung

| Eingabe | Wirkung |
|---|---|
| Linksklick auf die Karte | Ort auswählen, Ortsinfo öffnen (Klick ins Leere, ins Meer oder Ausland schließt sie) |
| Esc | Auswahl aufheben |
| Rechte oder mittlere Maustaste ziehen | Karte verschieben |
| Mausrad | Zoomen (zum Mauszeiger hin, von knapp unter der Übersicht, bis ein Kartenpixel 6 Bildschirmpixel groß ist) |
| W A S D, Pfeiltasten | Karte verschieben |
| Pos1 | Übersicht über die ganze Karte |
| H | Zur Hauptstadt des Spielerstaates (mittlere Zoomstufe) |
| Leertaste | Pause / weiter |
| 1 – 5 | Geschwindigkeit: 2, 5, 15, 40 oder 120 Tage je Sekunde |
| Q W E R T Z U I O P Ü + Ä Ö | Kartenmodus Politisch, Bevölkerungsdichte, Konfession, Gelände, Verwüstung, Kreise, Lebensstandard, Kultur, Wirtschaft, Märkte, Verwaltung und Eisenbahnen (beide seit M7; „+“ ist die Taste rechts neben Ü, englisch „]“), Wahlen (seit M8; „Ä“ rechts neben Ö, englisch „'“) und Zollgebiete (seit M9; „Ö“, englisch „;“) (die Leiste zeigt Kurznamen, die Taste steht im Tooltip). Im Modus Eisenbahnen zeichnet die Karte die Strecken: eröffnete durchgezogen, die der nächsten fünf Jahre gestrichelt |
| Wahlabend | seit M8: öffnet am Wahltag von selbst, die Zeit hält an. Die Wahlkreise treffen in der Meldereihenfolge ein (Tempo ×1, ×4 oder sofort); Halbrund, Balken mit Hochrechnung und Fehlerband, Telegramme (Klick zeigt den Wahlkreis). Danach im Spielmenü „Letzte Wahl“ |
| Steuerregler | Kontribution, Akzise und Zoll des Spielerstaates (wirksam ab dem nächsten simulierten Tag); seit M9 ist der Zollregler eines Mitglieds einer Zollunion gesperrt und zeigt den Tarif der Union |
| „Haushalt im Einzelnen …“ | Haushaltsfenster: Einnahmen je Steuerart, Ausgaben je Posten (Hof, Verwaltung, Heer, Zinsen, Bauten), Schatz, Schulden, Zins, Staatsbankrotte |
| „Verfassung und Gruppen …“ | seit M8, 1848: Verfassung, Wahlrecht und Wahlsystem des Landtags; Interessengruppen des Staates mit Anteil an den Einwohnern ab 15 und Einfluss (Anteil am Einkommen) |
| „Zollunion …“ | seit M9, 1848: Zollgebiet, Tarif und Anteil an der Vereinskasse; je Union Leitmacht, Mitglieder, Tarif und Kasse mit „Beitreten …“ bzw. „Austreten …“, nach einer Rückfrage mit Zeitpunkt (nächster 1. Januar); „Antrag zurückziehen …“. Lehnt die Leitmacht ab (Österreich beim Zollverein), ist der Knopf gesperrt ([docs/technik/diplomatie.md](../docs/technik/diplomatie.md), Abschnitt 3.5) |
| Klick auf einen Chronikeintrag | Ort des Ereignisses auswählen und anfahren |
| F10, Knopf „Menü“ | Spielmenü: Neues Spiel mit Wahl des Lesezeichens (1648 oder 1848, seit M7), Speichern unter einem Namen, Laden aus der Liste, Schnellspeichern und -laden; die Zeit hält an, Esc oder F10 schließt es (seit M6) |
| „Diesen Staat spielen“ (Ortsinfo) | Den Besitzer des Ortes als Spielerstaat übernehmen (seit M7); nach einem neuen Spiel nennt ein Hinweis die hervorgehobenen Staaten des Lesezeichens |
| Rolle (Spielmenü) | seit M8, nur in Szenarien mit Parlament (1848): „Staat: …“ oder „Partei: …“. Im **Parteienmodus** steht links das Parteipanel (Mittel, verplante Mittel, Spenden; die nächste Wahl mit Wahlkampf; nach einer Wahl deren Sitze, Fraktion und Abgeordnete) statt des Staatspanels; „Staat spielen“ führt zurück. Nach der Nationalversammlung (1. Mai 1848) gilt der Wahlkampf der Preußischen Zweiten Kammer (17. Juli 1849) |
| Wahlkampf (Ortsinfo) | seit M8, im Parteienmodus für die nächste Wahl: Parlament und Wahlkreis, Stimmungsbericht (Schätzung mit ± 8 Punkten Fehler) und die Knöpfe Versammlung, Flugblätter und Verein gründen; Wirkung ab dem nächsten Tag, gesperrt, wenn die Mittel nach den schon angesetzten Aktionen nicht reichen ([docs/technik/politik.md](../docs/technik/politik.md), Abschnitt 9) |
| F5 | Schnellspeichern (`schnellspeicher.einigkeit`) |
| F9 | Schnellladen |

**Doppelbelegung von W:** W gehört sowohl zur Kamerasteuerung (WASD) als auch zum
Kartenmodus „Bevölkerungsdichte“. Kurz getippt (unter 0,18 s) schaltet W den Kartenmodus um,
gehalten bewegt es die Karte nach oben. Die Pfeiltaste nach oben bewegt immer sofort.

Die Tastenbelegung steht als Aktionen in `project.godot` (`kamera_*`, `kartenmodus_*`,
`tempo_*`, `auswahl_aufheben`, `spielmenue`, `schnellspeichern`, `schnellladen`) und lässt sich im Editor unter *Projekteinstellungen →
Eingabebelegung* ändern. Es werden physische Tasten verwendet, die Belegung gilt also auf
jeder Tastatur an derselben Stelle: „Z“ ist die Taste rechts neben T (auf englischen
Tastaturen beschriftet mit „Y“), „Ü“ die rechts neben P (englisch „[“).

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
│   │   ├── location_catalog.gd    LocationCatalog: unveränderliche Ortsangaben (einmal gelesen)
│   │   ├── map_data.gd            MapData: provinzen.png, fluesse.png, definition.csv
│   │   ├── map_config.gd          MapConfig: karte.chron (Ausschnitt, Projektion)
│   │   ├── map_modes.gd           MapModes: Kartenmodi, Namen, Tasten
│   │   ├── german_format.gd       GermanFormat: Tausenderpunkt, Dezimalkomma, Einheiten
│   │   ├── save_slots.gd          SaveSlots: Ordner, Dateinamen und Liste der Spielstände, Autosave (seit M6)
│   │   ├── election_tally.gd      ElectionTally: Meldereihenfolge, Hochrechnung, Halbrund (seit M8)
│   │   └── launch_options.gd      LaunchOptions: --screenshot, --tage, --kartenmodus, --ort, --zoom
│   ├── map/
│   │   ├── map_view.gd            MapView: Darstellung, Nachschlagetexturen, Auswahl, Rahmen
│   │   ├── map_camera.gd          MapCamera: Ziehen, Zoomen, Übersicht
│   │   ├── map_detail.gd          MapDetail: Detailstufen nach Zoom (Grenzen, Namen)
│   │   ├── map_labels.gd          MapLabels: Namen im Bildschirmraum mit Rastern
│   │   ├── map_label_layout.gd    MapLabelLayout: Namenspunkte von Staaten und Kreisen
│   │   └── railway_layer.gd       RailwayLayer: Eisenbahnstrecken über der Karte (seit M7)
│   └── ui/                    Skripte der Panels, Legendenberechnung, Farbpalette; Wahlabend (election_night.gd) und Parteipanel (party_panel.gd, beide seit M8)
├── shaders/map.gdshader       Kartenshader (canvas_item)
├── theme/einigkeit_theme.tres Theme der Oberfläche (Papier und Tinte)
└── tests/                     Einheiten-, Rauch-, Klick- und Leistungstest, siehe tests/README.md
```

### Spielstände (seit M6)

Spielstände liegen in `user://spielstaende/` (unter Linux
`~/.local/share/godot/app_userdata/EINIGKEIT/spielstaende/`), Format und Prüfungen in
[docs/technik/speicherstaende.md](../docs/technik/speicherstaende.md). Das **Spielmenü**
(`scripts/ui/game_menu.gd`, im Code aufgebaut) öffnet sich über den Knopf „Menü“ rechts in der
oberen Leiste oder F10 und hält die Zeit an: Name des Spielstands (vorbelegt mit Staat und Datum),
Liste der Spielstände (zuletzt geänderte zuerst, mit Datum, Staat und Änderungszeit; Dateien, die
nicht zu den Spieldaten passen oder keine Spielstände sind, erscheinen ausgegraut mit dem Grund
als Tooltip; Spielstände des anderen Lesezeichens sind ladbar und wechseln es), Neues Spiel
(„Starten“ verlangt einen zweiten Klick, weil die laufende Partie endet), Laden, Schnellspeichern, Schnellladen, Weiterspielen; seit M8 „Letzte Wahl“ (Wahlabend mit dem ganzen Ergebnis) und die Rolle (Staat oder Partei, steht im Spielstand). Die Namen `schnellspeicher`
und `autosave_…` sind reserviert. F5 und F9 wirken auch ohne
Menü. Am **1. Januar** speichert das Spiel automatisch; `autosave_1` ist der neueste, die drei
letzten bleiben. Nach dem Laden steht die Zeit, die Auswahl ist aufgehoben, und Grenzen, Namen,
Panels, Legende und Chronik zeigen den geladenen Stand. Rückmeldungen („Schnellgespeichert“,
„Geladen: …“) erscheinen drei Sekunden lang unter dem Datum.

### Simulation

`SimulationClient` spricht die Klasse `UhrwerkSimulation` nur über `ClassDB` an. Ein direkter
Verweis auf den Klassennamen wäre ohne gebaute Erweiterung ein Parse-Fehler; so startet das
Projekt in jedem Fall. Beim Start prüft die Hülle, ob alle Methoden der Schnittstelle vorhanden
sind (seit M3 auch `get_location_owner_indices`, `get_location_kreis_indices`, `get_kreis*`; seit M5 `set_tax_percent`, `get_market*`, `get_location_market_indices`; seit M6 `save_game`, `load_game`, `get_save_info`, `get_save_format_version`),
und meldet eine veraltete Erweiterung verständlich.

Die Zeit läuft in `main.gd`: Je Bild werden die fälligen ganzen Tage mit `advance_days`
simuliert (höchstens 30 je Bild und seit M6 höchstens bis zum nächsten 1. Januar, damit der
automatische Spielstand den Jahreswechsel trifft), danach liest die Oberfläche Datum, Staat, Ort, Kartenfarben und
neue Chronikeinträge. Die Steuerregler schicken `set_tax_percent` mit der Steuerart (seit M5;
vorher `set_tax_rate_percent` für den einen Steuersatz); der Kern führt den Befehl am nächsten Tag
aus, bis dahin zeigt das Panel „gilt ab dem nächsten Tag“.

Im Kartenmodus **Märkte** zeichnet die Karte statt der Kreis- die Marktgrenzen (dieselbe Ebene des
Shaders, gefüttert mit `get_location_market_indices`); die Farben der aktiven Märkte folgen
nacheinander der Palette, damit sich Nachbarn unterscheiden. Die Modusleiste hat seit M5 keinen
Titel mehr und schmalere Knöpfe, damit zehn Modi bei 1.600 Pixeln Breite zwischen die Spalten
passen.

Abfragen über alle Orte sind gebündelt, damit nichts je Bild 4.500-mal die Brücke ruft:
`LocationCatalog` liest Name, Stadt, Gelände, Kreis und „hat Daten“ einmal nach dem Laden
(diese Angaben ändern sich während einer Partie nicht); Besitzer und Kreise kommen als zwei
Indexarrays (`get_location_owner_indices`, `get_location_kreis_indices`), die Farben als ein
Array je Kartenmodus (`get_map_colors`). Die Legende wird monatlich nur in den Modi neu
aufgebaut, deren Werte sich ändern (Politisch, Dichte, Konfession, Verwüstung).

### Karte

**Daten** ([chronik-skript.md, Abschnitt 6](../docs/technik/chronik-skript.md)):

- `provinzen.png` (4096 × 3329 Pixel) wird mit `Image.load_from_file` geladen. Das `Image`
  bleibt für die Klick-Erkennung im Speicher, eine `ImageTexture` dient der Darstellung.
  `R + G × 256` ist Kartenindex + 1 (0 = kein Ort), der Blaukanal die Flächenart:
  0 Kernraum-Land, 1 Meer, 2 Binnengewässer, 3 Ausland.
- `fluesse.png` (Graustufen, Deckung durch Flüsse) wird mit Mipmaps hochgeladen; beim
  Herauszoomen mittelt die Grafikkarte die Deckung, statt zu flimmern.
- `definition.csv` (`kartenindex;ort;name;x;y;pixel;flaeche_km2`) ordnet Kartenindizes
  Ort-Kennungen zu und liefert je Ort den Beschriftungspunkt (ein Pixel, das garantiert zum
  Ort gehört), die Pixelzahl und die Fläche. `get_location_index` verbindet Karten- und
  Simulationsindex; beide sind unabhängig. Eine Datei im Format vor M3
  (`kartenindex;ort;name`) wird mit einem Hinweis abgelehnt.

**Nachschlagetexturen.** Der Shader holt je Pixel Farbe, Besitzer und Kreis über den
Kartenindex aus zwei Texturen mit 256 Spalten (4.542 Orte → 256 × 18 Texel); eine einzeilige
Textur wäre breiter als die sichere Texturgröße im Kompatibilitätsrenderer.
`color_lut` enthält die Farbe aus `get_map_colors` (Alpha 1 = Farbe, Alpha 0 = keine Daten,
Alpha 0,5 = Ort der Karte, den die Simulation nicht kennt). `info_lut` enthält Besitzer + 1
(R + G × 256) und Kreis + 1 (B + A × 256). Beide werden nur neu geschrieben, wenn sich die
Werte geändert haben (Farben: über `PackedColorArray.to_byte_array` und eine native
Umwandlung, rund 1 ms).

**Shader** (`shaders/map.gdshader`):

- Meer, Binnengewässer und Ausland haben eigene, gedämpfte Farben; Orte die Farbe ihres
  Kartenmodus. Orte ohne Daten (Alpha 0 in den Modi Politisch, Dichte, Konfession,
  Verwüstung) und Kernraum-Land ohne Ort tragen eine diagonale Schraffur „keine Daten“. Sie
  ist an der Karte verankert; ihr Abstand bleibt auf dem Bildschirm etwa gleich, weil die
  Periode in Zweierpotenzen springt und dazwischen überblendet wird.
- Flüsse aus `fluesse.png` über allem außer dem Meer.
- Grenzen entstehen durch Nachbarvergleich im Bildschirmraum (Abstand ein Bildschirmpixel),
  damit sie bei jedem Zoom gleich dick sind: Staatsgrenzen (auch Kernraum ↔ Ausland) immer und
  kräftig, Kreisgrenzen ab mittlerem Zoom, Ortsgrenzen erst nah (Deckkraft aus `MapDetail`,
  im Kartenmodus „Kreise“ sind Kreisgrenzen immer deutlich). Küste und Ufer zeichnen beide
  Seiten der Grenze Land ↔ Wasser.
- Besitzer und Kreis der Nachbarn liest der Shader nur, wenn deren Kartenindex abweicht; im
  Inneren eines Ortes entfällt die Grenzsuche ganz.
- Ausgewählter Ort: Umriss in Siegelrot; überfahrener Ort: aufgehellt. Papierton und eine
  feine Körnung (Rauschtextur mit festem Startwert) geben der Karte die Anmutung eines
  gedruckten Atlas; der Rahmen mit Gradeinteilung folgt den Grenzen aus `karte.chron`.

<a id="detailstufen"></a>**Detailstufen.** Maß ist die Bildschirmgröße eines typischen
Ortes, s = Zoom × √(Median der Pixel je Ort); auf der Kernraum-Karte (Median 1.212 Pixel)
ist ein typischer Ort etwa 35 Kartenpixel breit, in der Übersicht (Zoom ≈ 0,18 bei
1600 × 900) also etwa 6 Bildschirmpixel. So gelten die Stufen für jede Kartenauflösung gleich (`MapDetail`):

| Stufe | Ortsgröße s | Grenzen | Namen |
|---|---|---|---|
| fern | unter 17 px | Staaten, Küste | Staatsnamen |
| mittel | 17 bis 40 px | dazu Kreise (ab s ≈ 10 eingeblendet) | Kreisnamen |
| nah | ab 40 px | dazu Orte (ab s ≈ 28 eingeblendet) | Ortsnamen |

**Namen** (`MapLabels`, `MapLabelLayout`) werden im Bildschirmraum gezeichnet und bleiben
dadurch bei jedem Zoom scharf. Je Zoom ist genau eine Ebene sichtbar (hartes Umschalten an den
Schwellen; eine Überblendung zeigte beide Ebenen halb durchsichtig übereinander). Ist die Ebene
der Zoomstufe leer – ohne Kreisdaten etwa die Kreisebene –, erscheint die nächste nicht leere,
die genauere zuerst (mittlere Stufe ohne Kreise: Ortsnamen).

- *Ortsnamen* stehen am Beschriftungspunkt aus `definition.csv`. Rangfolge: Hauptstädte,
  Hauptorte der Kreise (ein Kreis heißt nach seinem größten Ort), übrige Städte, übrige Orte;
  bei Gleichstand der flächengrößere. Ist der Platz belegt, weicht ein Ortsname eine Zeile
  nach oben oder unten aus. Der Name des ausgewählten Ortes wird immer zuerst gesetzt, im
  selben Stil wie ohne Auswahl (eine ausgewählte Hauptstadt bleibt fett).
- *Kreisnamen* (kursiv) stehen im Schwerpunkt der Beschriftungspunkte der Orte des Kreises,
  gewichtet mit deren Pixelzahl; liegt dieser Punkt nicht im Kreis, rückt er auf den
  nächstgelegenen Ort des Kreises. Ein Kreisname bleibt auch dann stehen, wenn sein
  gleichnamiger Hauptort ausgewählt ist und an derselben Stelle steht.
- *Staatsnamen* (gesperrte Großbuchstaben, Größe nach Fläche) stehen ebenso im
  pixelgewichteten Schwerpunkt. Die Orte eines Staates werden zu Gebieten gebündelt
  (Beschriftungspunkte höchstens 2,5 Ortsdurchmesser auseinander); beschriftet werden das
  größte Gebiet und jedes weitere mit mindestens 35 % davon und mindestens acht typischen
  Orten (Exklaven). Ist schon das größte Gebiet kleiner, steht ein Name im Schwerpunkt aller
  Orte des Staates.
- Jede Ebene liegt in einem Weltraster (256 Kartenpixel je Zelle): Je Bild werden nur die
  Namen der sichtbaren Zellen geprüft, nicht alle 4.500 Orte. Überlappungen verhindert ein
  Belegungsraster im Bildschirmraum (8 Pixel je Zelle); Panels und Leisten sind darin vorab
  belegt, damit keine Namen unter halbtransparenten Panels durchscheinen. Gezeichnet wird nur
  nach einer Kamerabewegung, einer neuen Auswahl oder wenn ein Panel Größe, Lage oder
  Sichtbarkeit ändert (`MapLabels.set_covering_controls`, etwa die Legende beim Moduswechsel);
  Textbreiten werden beim ersten Gebrauch gemessen und gemerkt.

**Auswahl.** `MapView.location_at_screen(punkt)` rechnet einen Bildschirmpunkt über die
Kamera in Kartenpixel um und liest den Kartenindex aus dem Bild; Klick und Hervorhebung nutzen
diese eine Funktion. Der Klicktest prüft sie für jeden Ort bei drei Zoomstufen
(`tests/klick_test.gd`).

**Kamera.** Das Spiel startet mit der Übersicht über die ganze Karte im freien Bereich
zwischen den Panels (Pos1 kehrt dorthin zurück, H springt zur Hauptstadt). Zoomgrenzen: 0,8 ×
Übersicht bis Zoom 6 (ein typischer Ort ist dann etwa 210 Pixel breit). Das Mausrad zoomt in
Stufen von 1,2 mit Glättung zum Mauszeiger hin.

**Ortsinfo und Tooltip.** Für Orte ohne Daten zeigt die Ortsinfo „Keine Daten“, Art, Kreis,
Gelände und Fläche und einen Hinweis, dass der Ort nicht an der Simulation teilnimmt;
Bevölkerung, Wirtschaft, Verwüstung und Konfessionen sind ausgeblendet. Der Tooltip nennt
Besitzer und Kreis (bzw. „Keine Daten · Kreis … · Fläche“), im Kartenmodus „Kreise“ nur den
Kreis.

### Legenden

Die Legenden werden aus den Farben der Simulation (`get_map_colors`) abgeleitet, damit sie
immer zur Karte passen. Für Dichte und Verwüstung schätzt `LegendData` den Verlauf aus den
Orten mit Daten: Passt eine lineare Abbildung Wert → Farbe, zeigt die Legende die exakten
Endfarben, sonst die tatsächlich vorkommenden Farben. In den datenabhängigen Modi steht
zusätzlich ein Feld „Keine Daten“ mit der Schraffur. Der Modus „Kreise“ zeigt die Palette der
Brücke (12 Farben, `palette[kreisindex % 12]`, ohne Bedeutung) und die Zahl der Kreise mit
Orten und der Orte in Kreisen (ein leerer Kreis ist in den Daten erlaubt, erscheint aber nicht
auf der Karte). Die Farben wiederholen sich: Innerhalb einer Landschaftsdatei haben benachbarte
Kreise fast immer verschiedene Farben, an den Grenzen zweier Landschaften nicht immer (mit den
Daten des Kernraums 61 von 1.747 benachbarten Kreispaaren, 60 davon an Landschaftsgrenzen,
z. B. Karlsruhe und Mannheim). Die Kreisgrenzen sind in
diesem Modus deshalb immer deutlich gezeichnet.

### Leistung

Gemessen mit `tests/leistung_test.gd` (Fenster 1600 × 900, echte Daten mit 4.541 Orten)
unter xvfb mit dem **Software-Renderer llvmpipe** auf 4 Prozessorkernen, also ohne
Grafikkarte; Mittelwerte je Bild:

| Abschnitt | ms je Bild | Bilder/s |
|---|---|---|
| Übersicht, ruhend / beim Schwenken | 31 / 31 | 32 / 33 |
| Schwenken, mittlere Zoomstufe | 61 | 16 |
| Schwenken, nahe Zoomstufe | 61 | 16 |
| Zoomfahrt mit dem Mausrad | 54 | 18 |
| Simulation 120 Tage/s, mittlere Zoomstufe | 60 | 17 |

Der Kartenshader macht mit llvmpipe rund 40 ms aus (1,44 Millionen Bildpunkte, je Pixel
fünf Zugriffe auf das ID-Bild, an Grenzen bis zu fünf auf `info_lut`), die halbtransparenten
Panels mit Schatten rund 14 ms, die Namen 1–9 ms (GDScript). Eine Grafikkarte erledigt den
Shader in einem Bruchteil einer Millisekunde; das Ziel „60 Bilder/s auf einem
Mittelklasse-Rechner“ (Abnahme M3) ließ sich hier mangels Grafikkarte nicht messen.

Beseitigte Engpässe: Der Kartenrahmen bestand aus sieben gefüllten Rechtecken, die größer
als die Karte waren und beim Hineinzoomen den ganzen Bildschirm mehrfach überdeckten (jetzt
Ringe; Grundlast 38 → 5,5 ms). Besitzer und Kreis der Nachbarn liest der Shader nur noch an
Grenzen, die Körnung kommt aus einem bilinearen statt zwei trilinearen Zugriffen
(Shader 71 → 45 ms). Mittlere Zoomstufe insgesamt: 113 → 61 ms je Bild.

## Bekannte Grenzen des Prototyps

- Wappen sind schlichte Schilde in der Staatsfarbe.
- Für Orte ohne Daten kennt das Spiel keine Einwohnerzahl; die Rangfolge der Ortsnamen
  stützt sich deshalb auf Stadt, Kreishauptort und Fläche. Unter zwei benachbarten Städten
  gewinnt daher nicht immer die bekanntere (die ausgewählte steht immer).
- Staatsnamen eines zersplitterten Staates stehen im Schwerpunkt aller seiner Orte, auf dem
  nächstgelegenen eigenen Ort; bei weit verstreuten Orten (Testregion 1648) ist das nur eine
  grobe Lage.
- Die Panels der Oberfläche (Theme aus M1) haben weiche Schatten; mit Software-Rendering
  kosten sie spürbar Zeit.
- Mehr als 65.534 Kartenindizes passen nicht in R + G (Vertrag der Karte).
