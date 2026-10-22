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
Zählung der Kreise mit Orten für die Legende). Seit M8 auch die Rechnungen des Wahlabends
(`ElectionTally`: Meldereihenfolge, Hochrechnung, Halbrund, Telegramme), `--wahlabend`,
`--partei`, `--verfassung` und das Parteipanel (Angaben vor und nach der Wahl); seit M9 der
Kartenmodus „Zollgebiete“ mit Taste Ö, `--zollunion` und `LegendData.states_text`, der Kartenmodus
„Beziehungen“ mit Taste #, `--diplomatie[=staaten|bund|frage]` und die Farben der Beziehungen
(`LegendData.relation_color`).

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
   `kreis_name`, seit M4 mit Kultur, Altersgruppen, Lebensstandard, Alphabetisierung und
   Vorjahresbilanz, seit M5 mit Haushalt je Posten und Wirtschaft des Ortes);
   `get_location_index` passt zu jedem Ort
4. ungültige Indizes und Befehle werden abgewiesen (leere Rückgabe bzw. `false`, auch
   `get_kreis`, `get_kreis_index`, `get_market` und `set_tax_percent` mit ungültiger Steuerart)
5. `get_map_colors` liefert für alle Kartenmodi (seit M9 fünfzehn) eine Farbe je Ort
6. Orte ohne Daten und Kreise: `get_location_owner_indices` und `get_location_kreis_indices`
   passen zu `get_location`; Orte ohne Daten haben keinen Besitzer und keine Bevölkerung;
   `get_kreis` hat alle Felder, `get_kreis_index` passt, `location_count` stimmt; Alpha der
   Kartenfarben ist 0 genau für Orte ohne Daten in den Modi 0, 1, 2, 4, 6, 7, 8 und 10 bis 14, im Modus 9
   (Märkte) nur für Orte ohne Daten in Märkten ohne Orte mit Daten, sonst 1
7. Wirtschaft (seit M5): Märkte mit allen Feldern und Preisen, Marktindex je Ort, jeder Ort mit
   Daten in einem aktiven Markt; Steuern je Art ergeben die Einnahmen, die Posten die Ausgaben;
   die Hauptstadt des Spielerstaates hat Markt, Berufe mit Einkommen, Betriebe und Güter
8. Steuerbefehle: `set_tax_rate_percent` (Kontribution) und `set_tax_percent` (Akzise) werden
   angenommen und sind nach einem Tag wirksam; die Chronik meldet die neue Akzise
9. 365 Tage simulieren; Datum (Kalenderrechnung im Test), `get_days_elapsed`, Datumstext
10. Einnahmen, Ausgaben und Produktion > 0
11. Chronik: Größe, Einträge mit allen Feldern, leere Abfrage hinter dem Ende
12. Prüfsumme 16-stellig hexadezimal; ein zweiter Lauf mit denselben Befehlen ergibt dieselbe
13. Spielstände (seit M6): `save_game` schreibt, `get_save_info` nennt Datum, Spielerstaat, Tage,
    Prüfsumme und Formatversion und meldet `compatible`; nach 30 weiteren Tagen stellt
    `load_game` die gespeicherte Prüfsumme wieder her, und 30 Tage danach stimmt sie mit dem
    Lauf ohne Unterbrechung überein; eine fehlende, eine fremde und eine abgeschnittene Datei
    werden mit Meldung abgelehnt, die laufende Simulation bleibt dabei unverändert
14. Karte (Fassung M3): `provinzen.png`, `fluesse.png` (Graustufen, gleiche Größe) und
    `definition.csv` (neue Kopfzeile) vorhanden; jeder Eintrag der CSV über
    `get_location_index` auflösbar, jeder Ort der Simulation in der CSV, keine Lücken;
    Bildgröße wie in `karte.chron`; Blaukanal ist eine Flächenart 0..3, bei B ≠ 0 kein Ort;
    alle Kartenindizes bekannt; jeder Ort mit mindestens einem Pixel, Pixelzahl je Ort wie in
    der CSV, jeder Beschriftungspunkt im eigenen Ort
15. Lesezeichen 1848 (seit M7): Szenarien ohne Laden, Verwaltung, Bundesgebiet, Eisenbahnen,
    Staatswechsel. Seit M8 die Wahlen: zwei Parlamente, fünf Strömungen, Wahlkreis je Ort, die
    Wahl am 1. Mai mit 633 Wahlkreisen und vollständiger Meldereihenfolge, Ortsinfo Mannheim,
    Legende mit Boykott. Dazu der Parteienmodus: Mittel nach der Spende des Monatsersten,
    Stimmungsbericht mit Summe 100 %, eine Versammlung kostet am nächsten Tag genau ihre Kosten,
    ungültige Wahlkampfbefehle werden abgelehnt, eingereihte zählen als verplant (der dritte Verein
    wird abgelehnt), die Rolle steht im Spielstand; nach der Wahl passen die Sitze der Partei zum
    Ergebnis, der Wahlkampf zur Nationalversammlung ist beendet, und Berlin hat einen offenen
    Wahlkreis der Preußischen Zweiten Kammer. Dazu „Verfassung und Gruppen“ für Baden; 1648 hat
    weder diese Angaben noch einen Parteienmodus. Seit M9 die Zollunionen: Zollverein mit
    26 Staaten unter Preußen und Tarif 10 %, Steuerverein mit 3; Preußens Zollregler gesperrt
    (`set_tax_percent` lehnt ab) mit Anteil an der Vereinskasse; Vorab-Prüfung (Hannover zum
    1. Januar 1849, der Steuerverein löst sich auf; Österreich abgelehnt); Kartenmodus
    „Zollgebiete“ und Zollgebiet in der Ortsinfo; Hannovers Antrag steht am nächsten Tag im
    Staat und in der Chronik. 1648 gibt es keine Union und keinen Beitritt. Seit M9, Schritt b3
    die Diplomatie: Preußen mit Gesandten, Österreich als Gegner (−30), Bayern im Lager
    Österreichs; Vorab-Prüfung der Handlungen (Gesandter und Forderung möglich, Bündnis abgelehnt);
    Bundesversammlung mit Präsidium Österreich, 68 Plenarstimmen und 17 im Engeren Rat; Prognose
    einer Bundesreform; Gesandter, Antrag und Forderung werden angenommen, ungültige abgewiesen;
    am nächsten Tag laufen die Krise „Führung im Bund“ (Stufe 1) und der Antrag, der Gesandte ist
    in München; nach dem Nachgeben Preußens und 40 Tagen ist die Krise beendet und die Reform
    abgestimmt („Die deutsche Frage“); im Kartenmodus „Beziehungen“ hat der Spielerstaat seine
    Farbe. 1648 gibt es weder Beziehungen noch Bundesversammlung noch Krisen

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
4. **Wirtschaft** (seit M5): Das Staatspanel hat drei Steuerregler; das Haushaltsfenster öffnet
   sich und nennt alle Steuerarten und Posten; ein bewegter Akzise-Regler meldet „gilt ab dem
   nächsten Tag“; die Ortsinfo der Hauptstadt zeigt Markt, Einkommen und Preise; der Wechsel in
   den Kartenmodus „Märkte“ läuft ohne Fehler.
5. **Spielstände** (seit M6, in einem eigenen Ordner `user://klick_test_spielstaende`, der danach
   gelöscht wird): F10 öffnet das Spielmenü und hält die Zeit an; Speichern im Menü legt die Datei
   an, die Liste zeigt sie, die obere Leiste meldet es; Escape schließt das Menü und setzt die
   Geschwindigkeit fort; F5 legt den Schnellspeicher an, F9 stellt seine Prüfsumme wieder her; eine
   fremde Datei steht ausgegraut in der Liste und lässt sich nicht laden, der eigene Spielstand
   schon; mit Stufe 5 entsteht am nächsten 1. Januar `autosave_1`.
6. **Neues Spiel 1848** (seit M7): Start über das Spielmenü, Kartenmodi Verwaltung und
   Eisenbahnen, Staatswechsel über die Ortsinfo, danach Laden eines Spielstands von 1648.
7. **Staatsinfo und Parteienmodus** (seit M8): „Verfassung und Gruppen …“ öffnet das Fenster mit
   Verfassung und Interessengruppen; 1648 bietet das Spielmenü keine Rolle an. Die Rolle „Partei:
   Konstitutionelle Liberale“ im Spielmenü ersetzt das Staatspanel durch das Parteipanel. Die Ortsinfo von Mannheim zeigt den Abschnitt
   „Wahl“ vor den Eckdaten, ohne „Diesen Staat spielen“. Der Knopf „Versammlung“ meldet sich in
   der oberen Leiste, am nächsten Tag sind die Mittel kleiner und das Panel nennt einen Wahlkreis
   mit Wahlkampf; der Knopf ist nach dem Tageswechsel derselbe. „Staat spielen“ führt zurück.
8. **Wahlen 1848** (seit M8):
   - Taste Ä schaltet in den Kartenmodus „Wahlen“.
   - Mit Stufe 5 hält die Zeit am 2. Mai an, und der Wahlabend öffnet sich mit 0 von 633
     gemeldeten Wahlkreisen.
   - Mit Tempo ×4 kommen die Meldungen schrittweise; „Sofort“ meldet alle, und die
     Hochrechnung gleicht dann dem Ergebnis des Kerns.
   - Die Ortsinfo von Mannheim verrät das Ergebnis nicht vor seiner Meldung.
   - Ein Telegramm zeigt Mannheim mit Ergebnis in der Ortsinfo.
   - „Schließen“ setzt die Geschwindigkeit fort; „Letzte Wahl“ im Spielmenü öffnet das ganze
     Ergebnis.
   - Ab dem Fraktionsstichtag (Oktober 1848) passt der Wahlabend mit acht Fraktionen ins Bild, und
     die stärkste Fraktion ist nicht „fraktionslos“.
9. **Zollunion** (seit M9): Taste Ö schaltet in den Kartenmodus „Zollgebiete“, die Legende nennt
   den Zollverein mit 26 Staaten; der Zollregler Bayerns ist gesperrt und steht auf 10 %; die
   Ortsinfo von Hamburg nennt das Zollgebiet. Als Hannover öffnet „Zollunion …“ das Fenster mit
   „Beitreten …“ und „Austreten …“; der Beitritt zum Zollverein fragt mit Zeitpunkt und Auflösung
   des Steuervereins nach, „Antrag stellen“ sperrt den Knopf („beantragt“), am nächsten Tag
   stehen der Antrag im Fenster und im Hinweis des Staatspanels und der Eintrag in der Chronik;
   „Antrag zurückziehen …“ nimmt ihn mit Rückfrage zurück.
10. **Diplomatie** (seit M9, Schritt b3): Taste # schaltet in den Kartenmodus „Beziehungen“ mit
    Legende. Als Preußen öffnet „Diplomatie …“ das Fenster; ein Gesandter nach Bayern steht am
    nächsten Tag in der Liste und im Haushalt (Posten „Gesandte“). Die Forderung „Führung im Bund“
    an Österreich fragt mit den Seiten der Krise nach; am nächsten Tag zeigt die Krisenleiste die
    Krise über dem Fenster (ohne Überlappung, das Fenster bleibt über der Leiste der Kartenmodi),
    „Nachgeben“ beendet sie. Im Reiter „Bundesversammlung“ wird eine Bundesreform beantragt, die am
    nächsten Tag als offener Antrag erscheint; Esc schließt das Fenster.

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
  Wirtschaft: 5 Prüfungen
  Spielstände: Menü, Speichern, Escape, F5, F9, Liste, automatischer Spielstand
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
