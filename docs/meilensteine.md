# Meilensteine (Phasen 1 und 2)

Dieses Dokument zerlegt die ersten beiden Phasen der Roadmap aus
[24 Produktion, Roadmap, DLC](24_produktion_roadmap_dlc.md) (Abschnitt 3) in überprüfbare
Meilensteine. Die Roadmap bleibt maßgeblich für Phasen, Zeiträume und Budget. Hier steht, **in
welcher Reihenfolge** gebaut wird und **woran man erkennt, dass ein Meilenstein fertig ist**.

Ein Meilenstein ist fertig, wenn alle Abnahmekriterien erfüllt sind, die CI grün ist und der
Stand im [Änderungsprotokoll](../CHANGELOG.md) steht. Meilensteine werden der Reihe nach
gebaut; Abhängigkeiten stehen jeweils dabei.

## Überblick

| Nr. | Meilenstein | Phase | Baut auf | Stand |
|---|---|---|---|---|
| M1 | Technischer Durchstich | 1 – Vorproduktion | – | **fertig** |
| M2 | Datenpipeline „Archivar“ | 1 – Vorproduktion | M1 | **fertig** |
| M3 | Karte des Kernraums | 1 – Vorproduktion | M2 | **fertig** (eine Abnahme nicht messbar, siehe unten) |
| M4 | Bevölkerung und Lasttest | 1 – Vorproduktion | M3 | **fertig** |
| M5 | Wirtschaft und Staatshaushalt | 1 – Vorproduktion | M4 | offen |
| M6 | Speicherstände und Wiederholungen | 1 – Vorproduktion | M5 | offen |
| M7 | Lesezeichen 1848 | 2 – Vertical Slice | M6 | offen |
| M8 | Politik, Parteien und Wahlen | 2 – Vertical Slice | M7 | offen |
| M9 | Diplomatie und Zollverein | 2 – Vertical Slice | M7 | offen |
| M10 | Militär und Einigungskriege | 2 – Vertical Slice | M9 | offen |
| M11 | Ereignisse und Revolution 1848/49 | 2 – Vertical Slice | M8 | offen |
| M12 | Künstliche Intelligenz der Staaten | 2 – Vertical Slice | M8–M11 | offen |
| M13 | Spielbarer Vertical Slice 1848–1871 | 2 – Vertical Slice | M12 | offen |

Phase 1 endet mit M6 (Ziel der Roadmap: „Technikprototyp“). Phase 2 endet mit M13 (Ziel:
„Spielbare Epoche III 1848–1871“).

---

## Phase 1 – Vorproduktion

### M1 – Technischer Durchstich ✅

Der ganze Weg von den Daten bis zum Bild steht in kleinem Maßstab.

- Simulationskern „Uhrwerk“ (Festkomma, Zufallsströme, Kalender, Chronik-Skript, Lader,
  Tagestakt), Kommandozeile `uhrwerk-cli`, 87 Tests
- Testregion „Der Westfälische Frieden“ (1648): 16 Staaten, 60 Orte
- Kartengenerator, GDExtension-Brücke, Godot-Prototyp mit fünf Kartenmodi
- CI unter Linux und Windows; GCC, Clang und MSVC rechnen bitgleich

Bekannte Grenze: Staatskassen wachsen über Jahrzehnte stark, weil es außer der Hofhaltung keine
Ausgaben gibt (siehe [data/README.md](../data/README.md)). Wird in M5 gelöst.

### M2 – Datenpipeline „Archivar“ ✅

**Ziel:** Große Datenmengen (Tausende Orte, Hunderte Staaten) lassen sich aus Tabellen erzeugen,
prüfen und mit Quellen belegen, ohne jede `.chron`-Datei von Hand zu schreiben.

**Umfang**
- Werkzeug `tools/archivar/`: liest CSV-Tabellen (Orte, Staaten, Besitzwechsel mit Datum,
  Bevölkerung je Stichjahr) und schreibt Chronik-Skript
- Pflichtfeld Quelle je Datensatz; Kennzeichnung „Schätzung“ und „gesichert“
- Datierte Historie: Besitz, Herrscher und Zugehörigkeit mit Gültigkeitszeitraum, damit
  dieselben Daten mehrere Lesezeichen speisen
- Prüfungen: Verweise, Lücken in Zeiträumen, Ausreißer bei Bevölkerung und Fläche

**Abnahme**
- Die Testregion aus M1 wird vollständig aus Tabellen erzeugt und ist byteidentisch mit dem
  eingecheckten Stand
- `--pruefen` in der CI (wie beim Kartengenerator)
- Dokumentation des Tabellenformats in `docs/technik/`

**Ergebnis**
- Kern: datierte Blöcke `JJJJ.MM.TT = { … }` in Orten und Staaten (Spezifikation Abschnitt 3.8)
- Tabellen `data/tabellen/*.csv` mit Quelle und Sicherheit je Zeile
  ([tabellen.md](technik/tabellen.md)); Werkzeug `tools/archivar/` mit 25 Tests
- Die Testregion wird vollständig aus den Tabellen erzeugt; `archivar.py --pruefen` bestätigt
  byteidentische Dateien, die Prüfsumme nach 100 Jahren ist gegenüber M1 unverändert
- CI-Job „Archivar und Kartengenerator“ (Python 3.10 und 3.13)
- Ende-zu-Ende geprüft: ein Besitzwechsel in `besitz.csv` und ein zweites Szenario ergeben mit
  denselben Daten zwei verschiedene Startzustände

### M3 – Karte des Kernraums ✅

**Ziel:** Die volle Karte nach [05 Karte und Geographie](05_karte_und_geographie.md).

**Umfang**
- Kernraum: Altes Reich nördlich der Alpen mit Preußen, Posen, Schleswig und angrenzenden
  Grenzregionen, **etwa 4.500 Orte** und **etwa 620 Kreise**
- Kartengenerator mit echten Polygonen für Küsten, Inseln (Rügen, Fehmarn), Haffe und Seen
  statt eines einzelnen Küstenlinienzugs; Flüsse als eigene Ebene
- Land und Meer werden ins ID-Bild eingebacken, damit der Shader keine Punktgrenze mehr hat
  (heute höchstens 126 Küstenpunkte)
- Godot: Zoomstufen mit Ortsnamen je Ebene, flüssige Darstellung bei voller Karte

**Abnahme**
- Jeder Ort des Kernraums hat eine zusammenhängende Fläche; Generator unter einer Minute
- Kartenansicht hält 60 Bilder pro Sekunde auf einem Mittelklasse-Rechner
- Klickerkennung für alle Orte richtig (automatischer Test)

**Ergebnis**
- **Kernraum** als von Hand gezeichneter Umriss (`data/karte/geometrie/kernraum.geojson`,
  398 Punkte; jede Grenzentscheidung ist in [tools/geodaten/README.md](../tools/geodaten/README.md)
  begründet) mit **4.542 Orten in 626 Kreisen** (4–12 Orte je Kreis, Median 7). Die Orte erzeugt
  `tools/geodaten/orte.py` reproduzierbar aus GeoNames `cities1000`; Gelände kommt aus einem
  Höhenmodell (AWS Terrain Tiles), die Fläche aus der Karte (1–1.147 km², Median 169 km²).
- **Orte ohne Daten** in Kern, Brücke und Archivar: Die 60 Orte der Testregion behalten ihre
  historischen Daten; die übrigen 4.482 stehen auf der Karte, nehmen aber an der Simulation nicht
  teil. **Kreise** als neue Ebene (`history/kreise/`, Kartenmodus „Kreise“).
- **Geometrie** aus Natural Earth 1:10 Mio. (Land mit Inseln wie Rügen, Fehmarn und den
  Friesischen Inseln, 88 Seen, 177 Flüsse in sechs Rängen), zugeschnitten mit
  `tools/geodaten/geodaten.py`. Meer, Binnengewässer und Ausland sind als Flächenart in das
  ID-Bild eingebacken; die Grenze von 126 Küstenpunkten aus M1 entfällt. Flüsse sind eine eigene
  Ebene (`fluesse.png`).
- **Kartengenerator v2** (Python mit numpy): 4096 × 3329 Pixel (etwa 0,37 km je Pixel),
  pixelgleich unter Python 3.10 und 3.13; Kernraum-Land zu 99,96 % Orten zugeordnet.
- **Godot:** Großkarte mit drei Zoomstufen (Staatsnamen fern, Kreisnamen mittel, Ortsnamen nah),
  abgestuften Grenzen, Schraffur für Orte ohne Daten und sechs Kartenmodi.
- Tests: 101 doctest-Fälle im Kern, Archivar 33, Kartengenerator 88, Geodaten 96, Godot-
  Einheitentest 170 Prüfungen, Rauchtest und Klicktest; alle in der CI.

**Abnahme im Einzelnen**

| Kriterium | Stand |
|---|---|
| Jeder Ort hat eine zusammenhängende Fläche | erfüllt: 4.542 von 4.542 Orten haben genau eine Fläche (4er-Nachbarschaft), vom Generator erzwungen und unabhängig nachgezählt |
| Generator unter einer Minute | erfüllt: 13–21 s für die volle Karte (Python 3.10 und 3.13 mit numpy, 4 Kerne); `orte.py` braucht für Orte und Kreise zusätzlich 70–120 s |
| 60 Bilder/s auf einem Mittelklasse-Rechner | **nicht nachgewiesen.** Gemessen werden konnte nur mit Software-Rendering (llvmpipe auf 4 Prozessorkernen, ohne Grafikkarte): 16–33 Bilder/s, in der Übersicht 27–33, beim Schwenken und Zoomen in mittlerer und naher Stufe 16–18. Fast die ganze Zeit geht dort in das Rastern des Kartenshaders (etwa 40 ms je Bild). Die Messung auf einem Rechner mit Grafikkarte steht aus (`game/tests/leistung_test.gd`, siehe [game/README.md](../game/README.md#leistung)). |
| Klickerkennung für alle Orte richtig (automatischer Test) | erfüllt: `game/tests/klick_test.gd` erkennt 4.542 von 4.542 Orten in allen drei Zoomstufen, dazu rund 120 Klicks über echte Eingabeereignisse; läuft in der CI |

**Bekannte Grenzen**
- **Neue Orte ohne historische Daten:** 4.482 der 4.542 Orte haben keinen Besitzer, keine
  Bevölkerung und keine Konfession. Die Karte des Kernraums steht, die Daten für 1648 gibt es
  nur für die Testregion. Daten für 1848 sind Teil von M7; für den übrigen Kernraum um 1648 ist
  noch kein Meilenstein festgelegt.
- **Ortsnamen sind oft heutige Formen** und noch fachlich zu prüfen: 92 Orte mit dem Präfix
  „Bad “ (der Titel wurde meist erst im 19. oder 20. Jahrhundert verliehen), 101 Namen mit
  Klammerzusatz (82 Unterscheidungen gleichnamiger Orte wie „Mühlhausen (Böhmen)“, 9 heutige
  Zusätze wie „Schwedt (Oder)“ oder „Kempten (Allgäu)“, 10 Namen der Testregion aus M1 wie
  „Frankfurt (Oder)“), 3 mit „in Bayern“, Namen aus Zusammenlegungen des 20. Jahrhunderts
  (Wuppertal, Garmisch-Partenkirchen, Castrop-Rauxel, Villingen-Schwenningen). 896 Orte
  außerhalb von Deutschland, Österreich und Liechtenstein tragen den heutigen Namen, weil
  `exonyme.csv` für sie keinen belegten deutschen Namen enthält (etwa Lubawa für Löbau; meist
  führt GeoNames keinen, manche Alternativnamen sind bewusst nicht übernommen, siehe
  [tools/geodaten/README.md](../tools/geodaten/README.md#bekannte-grenzen)). Bei den Kreisnamen sind es 13 mit „Bad “
  und 27 mit Klammer. Deutsche Namen werden nur mit Beleg ergänzt (`tools/geodaten/exonyme.csv`).
- **Auswahl nach heutiger Bedeutung:** GeoNames führt Orte mit heute mindestens 1.000
  Einwohnern (Stand 2020). Industriestädte des 19. und 20. Jahrhunderts sind daher stärker
  vertreten, abgegangene Orte fehlen. Die Kreise sind räumlich gebildet, keine historischen
  Verwaltungseinheiten, und heißen nach dem heute größten Ort.
- **Kreisfarben:** Die Palette mit 12 Farben trennt benachbarte Kreise nicht überall: 61 von
  1.747 benachbarten Kreispaaren haben dieselbe Farbe, 60 davon an der Grenze zweier
  Landschaftsdateien (z. B. Karlsruhe und Mannheim). Im Kartenmodus „Kreise“
  sind die Kreisgrenzen deshalb immer gezeichnet.
- 41 kleine Teilflächen des Kernraum-Landes (zusammen etwa 274 km², meist Inseln ohne
  GeoNames-Ort wie Rømø, Nordstrand, Poel und Amrum) gehören zu keinem Ort.
- Die Referenz-Prüfsumme nach 100 Jahren hat sich geändert (`40de8c42e8f7a0ff` statt
  `8d44cf792e143bb0`): Der Rechentag eines Ortes hängt von seinem Index ab, und die meisten der
  60 Orte mit Daten haben durch die eingefügten Orte einen anderen. Die Ergebnisse weichen nur
  wenig ab (Bevölkerung nach 100 Jahren 627.680 statt 627.721). Der erste M3-Stand mit 4.541
  Orten ergab `5548a5fa80f75c0d`; die Korrekturen nach dem Review (Osterode am Harz neu in
  `goettingen`) verschoben die Indizes von 38 Orten mit Daten um eins.
- Für M4 vorgemerkt: `step_day` prüft jeden Tag den Rechentag aller 4.542 Orte; 100 Jahre
  dauern im Release-Build 0,52 s statt 0,225 s mit 60 Orten. *In M4 erledigt* (Rechentag-Listen,
  Schnellwege in `Fixed`): 0,008 s mit dem Modell aus M1.

### M4 – Bevölkerung und Lasttest ✅

**Ziel:** Die Bevölkerungssimulation aus [07 Bevölkerung und Gesellschaft](07_bevoelkerung_und_gesellschaft.md)
im vollen Maßstab, mit dem Lasttest aus der Roadmap.

**Umfang**
- Bevölkerungsblöcke mit Beruf, Schicht, Konfession, Kultur, Bildung und Lebensstandard
- Wanderung zwischen Orten (Stadt–Land, Auswanderung), Geburten und Sterblichkeit nach Lage
- Mehrkernberechnung im Kern, ohne den Determinismus zu verlieren (feste Aufteilung,
  feste Reihenfolge beim Zusammenführen)
- Messwerkzeug `uhrwerk-cli lasttest`

**Abnahme**
- **500.000 Bevölkerungsblöcke**; ein Spieljahr rechnet in höchstens 2 Sekunden auf einem
  Mittelklasse-Rechner (festgelegt: Median aus 3 Spieljahren auf 4 Kernen, Release-Build)
- Prüfsummen bleiben auf GCC, Clang und MSVC bitgleich, auch bei unterschiedlicher Kernanzahl

**Ergebnis**
- **Modell und Formeln** in [technik/bevoelkerung.md](technik/bevoelkerung.md) (verbindlich):
  Jeder Block trägt Beruf (Schicht über den Beruf), Konfession, Kultur, einen **Altersaufbau** aus
  vier Gruppen (Kinder, Junge, Erwerbsfähige, Alte), Lebensstandard 0–100 und Alphabetisierung
  0–1 (64 Byte). Am Rechentag eines Ortes: Tragfähigkeit und Landreserve aus Fläche und Gelände,
  Lebensstandard aus dem Netto-Einkommen (Ertrag, Verwüstung, Steuern, Übervölkerung), Geburten
  und Sterbefälle nach Lebensstandard, Altern, Bildung nach Beruf, Konfession, Stadt und
  Wohlstand, **Wanderung** der Jungen zu bis zu vier Nachbarorten im Umkreis von 60 km und in die
  nächste Stadt (Bauern werden dort Landarme), **Auswanderung** bei Not. Jahresbilanz je Ort
  (Geburten, Sterbefälle, Seuchentote, Zuzug, Wegzug, Auswanderung), bitgenau verbucht.
- **Mehrkernberechnung:** eigener Thread-Pool ohne neue Abhängigkeit. Die Orte eines Tages
  rechnen parallel; jede Aufgabe schreibt nur ihren Ort und liest andere Orte nur über deren
  veröffentlichte Momentaufnahme. Wanderung wird je Zielort in fester Reihenfolge gebucht, ebenfalls
  parallel. Das Ergebnis hängt nicht von der Zahl der Threads ab.
- **Leistung:** bitgleiche Schnellwege für Division und Multiplikation in `Fixed`
  (Knuth-Division statt 128-Bit-Schleife), Rechentag-Listen, Steuern je Ort statt Neuberechnung
  aller Blöcke. Mit dem Modell aus M1 sank die Zeit je Block und Monat von 450 ns auf 18 ns.
- **Daten:** 38 Kulturen (`common/kulturen.chron`), Standardkultur je Landschaft und Abweichungen
  je Ort in den Tabellen; Tragfähigkeit je Geländetyp, Bildungsfaktoren je Konfession und Beruf;
  die Regelwerte für Demografie, Wanderung und Bildung sind mit den 60 Orten der Testregion
  kalibriert: **1648 → 1700 ×1,29** (Ziel etwa ×1,32), 1648 → 1816 ×2,33; Geburten 37,8 ‰,
  Sterbefälle 31,7 ‰ um 1700, Alphabetisierung 31 % (1648) → 38 % (1700) → 53 % (1816).
- **Lasttest:** `uhrwerk-cli lasttest` baut aus der Karte eine Welt, in der jeder der 4.542 Orte
  Daten hat (`make_load_test_world`, eigener Zufallsstrom), misst Zeit je Spieljahr, Median und
  Speicher und vergleicht auf Wunsch mit einer anderen Thread-Zahl (`--vergleich`) und einer
  Grenze (`--grenze`). CI-Job „Lasttest“ (Release, 500.000 Blöcke).
- **Brücke und Godot:** Ortspanel mit Kulturen, Gesellschaft (Lebensstandard, Alphabetisierung),
  Altersaufbau und Bilanz des Vorjahres; Staatspanel mit Bevölkerungsentwicklung; neue
  Kartenmodi „Lebensstandard“ (Taste U) und „Kultur“ (Taste I), jetzt acht.
- **Referenz-Prüfsumme** nach 100 Jahren jetzt `ed6b14a02cfbfa5d` (Bevölkerung 642.668); der
  Lasttest (500.000 Blöcke, 3 Jahre, Seed 1648) endet mit `a5a4c0d80a2f23ea`.
- Tests: 123 doctest-Fälle im Kern (u. a. Formeln gegen Nachrechnung, exakte Buchführung, 1–8
  Threads, Kalibrierung mit den echten Daten), CTest `lasttest_threads`, Archivar 35,
  Kartengenerator 88, Geodaten 96, Godot-Einheitentest 179 Prüfungen, Rauch- und Klicktest; alle
  in der CI. ThreadSanitizer ohne Befund (lokal).

**Abnahme im Einzelnen**

| Kriterium | Stand |
|---|---|
| 500.000 Blöcke, ein Spieljahr in höchstens 2 s | erfüllt: Median **0,33–0,40 s** mit 4 Kernen (Xeon 2,8 GHz, Release, 499.992 Blöcke), 0,64 s mit 2, 0,88 s mit einem Kern; auf dem GitHub-Runner mit nur 2 vCPU 0,52 s. Spitze 42–44 MB Arbeitsspeicher. Messwerte in [bevoelkerung.md, Abschnitt 6.3](technik/bevoelkerung.md#63-schätzung) |
| Prüfsummen bitgleich auf GCC, Clang und MSVC | erfüllt: CI-Job Determinismus vergleicht die Prüfsumme nach 100 Jahren und die des Lasttests |
| … auch bei unterschiedlicher Kernanzahl | erfüllt: Unit-Test mit 1, 2, 3, 4 und 8 Threads, `lasttest --vergleich 1` in CTest und CI, `determinismus` mit einem Thread im zweiten Lauf |

**Bekannte Grenzen**
- **Nur 60 Orte mit echten Daten.** Die Bevölkerung des übrigen Kernraums ist nicht erfasst; den
  vollen Maßstab prüft der Lasttest mit einer **synthetischen Welt** (Bevölkerung aus der
  Tragfähigkeit, gleich große Blöcke, zufällige Kulturen). Ihre Zahlen sagen etwas über Rechenzeit
  und Speicher, nicht über die Geschichte.
- **Messrechner:** Der Wert für 4 Kerne stammt aus der Entwicklungsumgebung (Cloud-Rechner mit
  4 Kernen); der GitHub-Runner hat nur 2 vCPU. Eine Messung auf dem Referenz-PC aus Kapitel 02
  steht aus; er hat mehr Kerne und sollte schneller sein.
- **Keine Krisen außer Seuchen:** Missernten, Hungerkrisen und Kriegsfolgen für die Bevölkerung
  kommen mit den Ereignissen (M11) und dem Militär (M10). Die Kurven sind daher glatter als in
  der Geschichte.
- **Lebensstandard nur aus dem Einkommen:** Bedürfnisse, Güter und Preise fehlen bis M5; ebenso
  die 20 Berufsgruppen (bis dahin die 6 Berufe der Epoche).
- **Bildung ohne Politik:** Die Alphabetisierung folgt Beruf, Konfession, Stadt und
  Lebensstandard; Schulgesetze und Bildungsstufen kommen mit M8.
- **Kulturen vereinfacht:** Jüdische Gemeinden haben die Kultur ihres Ortes (die Konfession
  `juedisch` unterscheidet sie); eine eigene Kultur und Angleichung über die Zeit fehlen.
- **Auswanderung verlässt die Welt:** Ziele außerhalb des Kernraums (Amerika, Russland, Ungarn)
  sind nicht modelliert; Binnenwanderung bewegt nur die Gruppe der Jungen.
- Die Kalibrierung liegt 1700 mit ×1,29 etwas unter dem Ziel (×1,32) und 1816 mit ×2,33 etwas
  darüber (×2,27 nach Kapitel 07); sie beruht nur auf den 60 Orten der Testregion.
- Neu im Zustand und damit für die Speicherstände (M6) wichtig: Zugänge je Ort, Momentaufnahmen,
  Jahresbilanzen und Ortssteuern.

### M5 – Wirtschaft und Staatshaushalt

**Ziel:** Ein Wirtschaftssystem nach [08 Wirtschaft und Finanzen](08_wirtschaft_und_finanzen.md),
das über Jahrzehnte im Gleichgewicht bleibt.

**Umfang**
- Güter, Betriebe und regionale Märkte mit Preisen; Handel zwischen Märkten
- Staatshaushalt mit Ausgaben, die mit dem Staat wachsen: Verwaltung je Einwohner, Heer,
  Bauten, Schuldendienst; Kredite und Staatsbankrott
- Steuerarten statt eines einzigen Steuersatzes (Kontribution, Akzise, Zölle)
- Oberfläche: Haushaltsfenster, Kartenmodus Wirtschaft

**Abnahme**
- Über 100 Spieljahre ohne Spielereingriff bleiben Kassen in einer plausiblen Spanne
  (Zielwerte im Datenordner dokumentiert); kein Staat wächst ins Unendliche
- Balancing-Bericht über mehrere Seeds in der CI

### M6 – Speicherstände und Wiederholungen

**Ziel:** Partien speichern, laden und exakt nachspielen – Grundlage für Multiplayer und
Fehlersuche ([02 Engine und Technik](02_engine_und_technik.md), [23 Multiplayer und Modding](23_multiplayer_und_modding.md)).

**Umfang**
- Speicherformat mit Versionsnummer und Migration
- Befehlsprotokoll: Startzustand plus alle Befehle ergeben dieselbe Partie
- Godot: Speichern, Laden, Schnellspeichern

**Abnahme**
- Speichern → Laden → Weiterspielen ergibt dieselbe Prüfsumme wie ohne Unterbrechung
- Wiederholung aus Befehlsprotokoll ist bitgleich (CI-Test über 50 Jahre)

---

## Phase 2 – Vertical Slice (Epoche III, 1848–1871)

### M7 – Lesezeichen 1848

**Ziel:** Das Lesezeichen „Völkerfrühling“ (01.03.1848) aus
[04 Zeitrahmen, Epochen, Startdaten](04_zeitrahmen_epochen_startdaten.md) mit vollständigen Daten.

**Umfang**
- Deutscher Bund mit allen Bundesstaaten; spielbar: Preußen, Österreich, Bayern, Sachsen
- Verwaltungsgliederung 1815–1866 (Provinzen, Regierungsbezirke, Kreise)
- Bevölkerung, Wirtschaft und Eisenbahnen um 1848 aus der Datenpipeline (M2)

**Abnahme**
- `uhrwerk-cli pruefen` ohne Fehler; jeder Datensatz mit Quelle
- Historische Stichprobenprüfung durch den Beirat (oder eine dokumentierte Fachprüfung)

### M8 – Politik, Parteien und Wahlen

**Ziel:** Politisches System nach [09 Politik, Staat und Gesetze](09_politik_staat_und_gesetze.md),
[10 Parlamente, Wahlen, Koalitionen](10_parlamente_wahlen_koalitionen.md) und
[11 Parteien-Datenbank](11_parteien_datenbank.md).

**Umfang**
- Strömungen und Parteien bzw. Fraktionen von 1848 (Paulskirche), Interessengruppen
- Verfassungen, Gesetze und Wahlrecht (Zensus- und Dreiklassenwahlrecht)
- Wahlkreise aus den Kreisen, Wahlen mit Ergebnis je Wahlkreis
- **Wahlabend** als eigener Bildschirm
- Parteienmodus (Spielen als Fraktion)

**Abnahme**
- Eine Wahl zur Nationalversammlung 1848 ergibt eine historisch plausible Sitzverteilung
  (Abweichung dokumentiert)
- Wahlabend mit Hochrechnung und Ergebnis je Wahlkreis

### M9 – Diplomatie und Zollverein

**Ziel:** Diplomatie nach [14 Diplomatie](14_diplomatie.md) mit dem Zollverein als Kernsystem.

**Umfang**
- Beziehungen, Bündnisse, Garantien, Bundesversammlung des Deutschen Bundes
- Zollverein: Beitritt, Austritt, Zolleinnahmen, Wirkung auf die Märkte aus M5
- Diplomatische Spiele zur deutschen Frage (kleindeutsch, großdeutsch)

**Abnahme**
- Zollverein-Mitgliedschaften 1848 historisch korrekt; Beitritte verändern Handel und Kassen
  messbar

### M10 – Militär und Einigungskriege

**Ziel:** Kriegführung nach [15 Militär und Kriegsführung](15_militaer_und_kriegsfuehrung.md).

**Umfang**
- Heere, Aushebung, Versorgung, Bewegung auf der Karte, Schlachten, Fronten
- Eisenbahnen als Aufmarschwege
- Kriegsziele, Friedensverhandlungen, Gebietsabtretungen
- Ausgangslagen der Kriege 1864, 1866 und 1870/71

**Abnahme**
- Die drei Kriege lassen sich durchspielen; ein ungestörter Lauf ohne Spieler führt zu
  plausiblen, nicht festgelegten Ergebnissen
- Heereskosten belasten den Haushalt aus M5

### M11 – Ereignisse und Revolution 1848/49

**Ziel:** Ereignisse, Journale und Missionen nach
[18 Ereignisse, Journale, Missionen](18_ereignisse_journale_missionen.md).

**Umfang**
- Ereignissystem im Chronik-Skript (Bedingungen, Auswahlmöglichkeiten, Folgen)
- Märzrevolution, Paulskirche und Verfassungskonvent, Reichsverfassungskampagne, Olmütz
- Journal „Deutsche Frage“ mit offenen Ausgängen
- Leitlinien aus [20 Verantwortung und Erinnerungskultur](20_verantwortung_und_erinnerungskultur.md) beachten

**Abnahme**
- Mindestens 80 Ereignisse für 1848–1871 mit Quellenangaben
- Die Revolution kann scheitern oder gelingen; beide Wege sind spielbar

### M12 – Künstliche Intelligenz der Staaten

**Ziel:** Computerstaaten nach [21 Künstliche Intelligenz](21_kuenstliche_intelligenz.md).

**Umfang**
- Entscheidungen zu Haushalt, Steuern, Diplomatie, Krieg und Politik
- Persönlichkeiten der Herrscher und Minister aus [13 Persönlichkeiten](13_persoenlichkeiten.md)
- KI rechnet deterministisch im Kern (eigener Zufallsstrom)

**Abnahme**
- 100 KI-Partien 1848–1871 ohne Spieler: keine Abstürze, Ergebnisse plausibel verteilt
  (Bericht mit Häufigkeiten: Einigung ja/nein, Form der Einigung)

### M13 – Spielbarer Vertical Slice 1848–1871

**Ziel:** Eine Epoche, die sich wie das fertige Spiel anfühlt – Grundlage für Förderung,
Publisher und Early Access.

**Umfang**
- Oberfläche nach [22 UI/UX, Grafik, Audio](22_ui_ux_grafik_audio.md): alle Fenster der Epoche,
  Einführung bzw. Tutorial, Musik und Geräusche in Grundfassung
- Lokalisierung Deutsch (Englisch vorbereitet)
- Installierbare Versionen für Windows, Linux und macOS

**Abnahme**
- Eine Partie 1848–1871 lässt sich vollständig ohne Fehler spielen
- Spieltest mit externen Testern; Rückmeldungen dokumentiert
- Leistungsziele aus M3 und M4 gelten auch mit allen Systemen
