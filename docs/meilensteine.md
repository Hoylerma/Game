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
| M5 | Wirtschaft und Staatshaushalt | 1 – Vorproduktion | M4 | **fertig** |
| M6 | Speicherstände und Wiederholungen | 1 – Vorproduktion | M5 | **fertig** |
| M7 | Lesezeichen 1848 | 2 – Vertical Slice | M6 | **fertig** (Fachprüfung der Daten offen, siehe unten) |
| M8 | Politik, Parteien und Wahlen | 2 – Vertical Slice | M7 | **fertig** (Fachprüfung der Werte offen, siehe unten) |
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

### M5 – Wirtschaft und Staatshaushalt ✅

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

**Ergebnis**
- **Modell und Formeln** in [technik/wirtschaft.md](technik/wirtschaft.md) (verbindlich):
  11 Güter, 6 Bedürfnisse mit Warenkorb je Schicht, 12 Betriebe als Arbeitsstätten der Berufe,
  ein Markt je Landschaft (77, davon 10 mit Orten mit Daten). Am Monatsersten bilden die Märkte
  Preise aus Angebot und Nachfrage und handeln mit Nachbarmärkten und über die Ostseehäfen mit dem
  Weltmarkt; am Rechentag erzeugt jeder Ort, zahlt Grundrente, Zehnt und Steuern, kauft nach
  Warenkorb ein und verschiebt seine Betriebe langsam zum Gewinn. Der Lebensstandard folgt dem
  Realeinkommen, fehlende Grundnahrung erhöht die Sterblichkeit, die Ernte schwankt je Markt und
  Jahr. Beim Start schwingt die Wirtschaft 60 Monate ein.
- **Staatshaushalt:** Kontribution, Akzise und Zoll statt eines Steuersatzes; Ausgaben für Hof,
  Verwaltung und Heer nach Einwohnern, Zinsen nach Verschuldung, Bauten aus dem Überschuss über
  der Rücklage (Landesausbau erhöht die Tragfähigkeit); Kredit, Tilgung, Staatsbankrott mit
  Schuldenschnitt. Die Staatsausgaben kaufen auf dem Markt der Hauptstadt ein.
- **Balancing-Bericht** `uhrwerk-cli bilanz` mit Zielwerten in `data/common/zielwerte.chron`
  (Begründung in [data/README.md](../data/README.md#wirtschaftswerte)); CI-Job „Balancing“ mit
  16 Seeds über 100 Jahre.
- **Kalibrierung** mit den 60 Orten der Testregion: Berufsanteile der Städte, Warenkörbe,
  Betriebe (u. a. Saline), Bezugseinkommen, Fruchtbarkeit, Verwaltung je Einwohner und Hofhaltung
  je Staat. Bevölkerung 1648 → 1700 ×1,36, nach 100 Jahren ×1,86.
- **Brücke und Godot:** drei Steuerregler und Haushaltsfenster im Staatspanel, Abschnitt
  Wirtschaft in der Ortsinfo, Kartenmodi „Wirtschaft“ (Taste O) und „Märkte“ (Taste P, mit
  Marktgrenzen), jetzt zehn.
- **Referenz-Prüfsumme** nach 100 Jahren jetzt `5db13a66d63e6079` (Bevölkerung 713.456); der
  Lasttest (500.000 Blöcke, 3 Jahre, Seed 1648) endet mit `8602a1273080ee9e`.
- Tests: 136 doctest-Fälle im Kern (u. a. Ortsrechnung, Markt, Handel und Haushalt exakt gegen
  Handrechnung, Balancing-Bericht), CTests `bilanz_fixture` und `bilanz_verstoss`,
  Godot-Einheitentest 187 Prüfungen, Rauch- und Klicktest mit Wirtschaft; alle in der CI.
  ThreadSanitizer und AddressSanitizer mit UndefinedBehaviorSanitizer ohne Befund (lokal).

**Abnahme im Einzelnen**

| Kriterium | Stand |
|---|---|
| Kassen über 100 Jahre in plausibler Spanne, kein Staat wächst ins Unendliche | erfüllt: 32 Läufe ohne Staatsbankrott; Vermögen ((Schatz − Schulden) ÷ Jahreseinnahmen) am Ende im Median 0,85–1,12, tiefster Wert −1,77; alle Zielwerte erfüllt ([wirtschaft.md, Abschnitt 7](technik/wirtschaft.md#7-balancing-bericht)) |
| Balancing-Bericht über mehrere Seeds in der CI | erfüllt: Job „Balancing (Linux, Release)“, 16 Seeds × 100 Jahre in 4 s |
| Weiter aus M4: Lasttest höchstens 2 s, Prüfsummen bitgleich | erfüllt: 0,58 s je Spieljahr mit 4 Kernen (0,67 s auf dem Runner), etwa doppelt so viel wie in M4; Determinismus-Job (GCC, Clang, MSVC) grün |

**Bekannte Grenzen**
- **Nur 10 von 77 Märkten rechnen**, weil nur die 60 Orte der Testregion Daten haben; Handel gibt
  es nur zwischen diesen Märkten und über die Häfen der Testregion.
- **Staaten ohne Spieler ändern ihre Steuern nicht;** das Heer ist nur ein Kostenposten (bis M10
  bzw. M12). Die Kalibrierung gilt für Läufe ohne Eingriff.
- **Steuerquote sinkt mit der Verstädterung:** Die Kontribution trifft in Städten nur
  Agrareinkommen, und Bürger und Adel wachsen mit den Formeln aus M4 schneller als die Landarme
  (nach 100 Jahren ×3 bzw. ×4 gegen ×1,4; einen Abstieg in andere Berufe gibt es nicht). Die
  Zielspanne der Steuerquote beginnt deshalb bei 0,6 statt 0,7.
- **Kein Geld:** Preise in Reichstalern ohne Geldmenge, Inflation oder Münzverschlechterung;
  keine Staatsbetriebe, keine Zollgrenzen zwischen den Staaten (Zollverein mit M9).
- **Rechenzeit:** Die Ortsrechnung verdoppelt die Zeit je Spieljahr gegenüber M4, das Einschwingen
  der Lasttest-Welt dauert 1 s; Optimierungen sind notiert
  ([wirtschaft.md, Abschnitt 6](technik/wirtschaft.md#6-leistung)).
- **Startkassen aus M1:** Der Schatz zum Start ist noch als Anteil der damaligen Hofhaltung
  bemessen; mit der neuen Hofhaltung sind manche Staaten dadurch zu Beginn reicher als ihre
  Ausgaben nahelegen.
- Neu im Zustand und damit für die Speicherstände (M6) wichtig: Betriebsanteile, Angebot,
  Nachfrage, Einkommen und Bedarfsdeckung je Ort, Landesausbau, Marktzustand, Haushalt und
  Schulden der Staaten.

### M6 – Speicherstände und Wiederholungen ✅

**Ziel:** Partien speichern, laden und exakt nachspielen – Grundlage für Multiplayer und
Fehlersuche ([02 Engine und Technik](02_engine_und_technik.md), [23 Multiplayer und Modding](23_multiplayer_und_modding.md)).

**Umfang**
- Speicherformat mit Versionsnummer und Migration
- Befehlsprotokoll: Startzustand plus alle Befehle ergeben dieselbe Partie
- Godot: Speichern, Laden, Schnellspeichern

**Abnahme**
- Speichern → Laden → Weiterspielen ergibt dieselbe Prüfsumme wie ohne Unterbrechung
- Wiederholung aus Befehlsprotokoll ist bitgleich (CI-Test über 50 Jahre)

**Ergebnis**
- **Vertrag** in [technik/speicherstaende.md](technik/speicherstaende.md) (verbindlich).
- **Ein Zustandsverzeichnis** für Prüfsumme, Textexport, Schreiben und Lesen: Was gespeichert
  wird, steht in der Prüfsumme, und umgekehrt. Die Referenzprüfsummen blieben unverändert.
- **Spielstand v1:** Kopf, lesbare Metadaten, Nutzdaten mit zstd. Ein Spielstand der Spieldaten
  hat nach 100 Jahren 38 KB (3,8 MB roh) und lädt in 0,03 s (0,5 s mit den Spieldaten); die
  Lasttest-Welt mit 500.000 Blöcken hat 2,9 MB (41,9 MB roh), Speichern 0,4 s, Laden 0,5 s.
  Laden nur in Spieldaten mit passendem Inhaltsfingerabdruck (Meldung je Bereich), mit Prüfung
  aller Indizes und Größen und Gegenprobe der Prüfsumme; gleicher Zustand ergibt dieselben Bytes,
  unter GCC, Clang und MSVC derselbe Prüfwert der Nutzdaten.
- **Befehlsprotokoll und Wiederholung:** Jeder Spielstand enthält alle Befehle seit Partiebeginn
  und die Startprüfsumme; Protokolldateien tragen zusätzlich jährliche Kontrollpunkte, die
  Wiederholung meldet den ersten abweichenden.
- **Kommandozeile:** `lauf --speichern/--laden/--protokoll/--testbefehle`, `speichertest`,
  `wiederholung`, `spielstand [--text]`, `lasttest --speichern`.
- **Godot:** Spielmenü (Knopf „Menü“, F10) mit Speichern unter einem Namen und Laden aus der
  Liste, F5/F9, automatischer Spielstand am 1. Januar (drei bleiben).
- **Tests:** 146 Testfälle (u. a. 17 Speicherpunkte mit 1 und 4 Threads, beschädigte Dateien,
  Formatwache); CTests `speichertest_50_jahre`, `protokoll_50_jahre`, `wiederholung_50_jahre`,
  `wiederholung_spielstand`, `spielstand_laden`, `spielstand_fremder_inhalt`; Godot-Einheitentest
  217 Prüfungen, Rauch- und Klicktest mit Spielständen; alle in der CI. Ein Fuzz-Lauf mit 3.000
  verfälschten Nutzdaten unter AddressSanitizer und UndefinedBehaviorSanitizer ohne Befund (lokal).

**Abnahme im Einzelnen**

| Kriterium | Stand |
|---|---|
| Speichern → Laden → Weiterspielen ergibt dieselbe Prüfsumme wie ohne Unterbrechung | erfüllt: `speichertest_50_jahre` mit den Spieldaten speichert an 11 Tagen (5 mit wartenden Befehlen, einer mit Zugängen) in eine Datei, lädt mit frischen Spieldaten und 1 bzw. 3 Threads und trifft alle 50 Jahresprüfsummen und den Endstand; Unit-Test mit 17 Speicherpunkten, Godot-Rauch- und Klicktest über die Brücke |
| Wiederholung aus Befehlsprotokoll ist bitgleich (CI-Test über 50 Jahre) | erfüllt: `protokoll_50_jahre` und `wiederholung_50_jahre` (200 Testbefehle, 50 Kontrollpunkte und Endstand) sowie `wiederholung_spielstand` in den Jobs GCC, Clang und MSVC |
| Speicherformat mit Versionsnummer und Migration | Formatversion im Kopf, neuere werden abgelehnt, Formatwache im Test; Lesen älterer Versionen entsteht mit Version 2 |

**Bekannte Grenzen**
- **Spieldaten müssen passen:** Jede Änderung an Regeln, Berufen, Gütern oder Orten macht alte
  Spielstände unbrauchbar; das Laden über Kennungen kommt vor einer öffentlichen Version.
- **Keine Migration bisher:** Version 1 ist die erste; der Versionsschalter im Lesebesucher
  entsteht mit Version 2.
- **Wiederholung ab Szenariostart:** Partien aus der Lasttest-Welt lassen sich nicht wiederholen.
- **Oberfläche:** kein Überschreiben-Dialog; ein offenes Haushaltsfenster nimmt F10 entgegen, bis
  Escape es schließt; der automatische Spielstand lässt sich noch nicht abschalten.

---

## Phase 2 – Vertical Slice (Epoche III, 1848–1871)

### M7 – Lesezeichen 1848 ✅

**Ziel:** Das Lesezeichen „Völkerfrühling“ (01.03.1848) aus
[04 Zeitrahmen, Epochen, Startdaten](04_zeitrahmen_epochen_startdaten.md) mit vollständigen Daten.

**Umfang**
- Deutscher Bund mit allen Bundesstaaten; spielbar: Preußen, Österreich, Bayern, Sachsen
- Verwaltungsgliederung 1815–1866 (Provinzen, Regierungsbezirke, Kreise)
- Bevölkerung, Wirtschaft und Eisenbahnen um 1848 aus der Datenpipeline (M2)

**Abnahme**
- `uhrwerk-cli pruefen` ohne Fehler; jeder Datensatz mit Quelle
- Historische Stichprobenprüfung durch den Beirat (oder eine dokumentierte Fachprüfung)

**Ergebnis**
- **Vertrag** in [technik/lesezeichen_1848.md](technik/lesezeichen_1848.md) mit dem Stand je
  Schritt. Umgesetzt in zwei Teilen: M7a (Staaten, Orte, Verwaltung, Bevölkerung, Godot) und M7b
  (Eisenbahnen, Industrie, Haushalte, Kalibrierung).
- **Staaten:** 38 Staaten zum 1. März 1848, davon 31 neu und 7 aus 1648 fortgeführt. Alle haben
  Hauptstadt, Regierungsform, Herrscher, Konfession und Zollverein. Spielbar hervorgehoben sind
  Preußen, Österreich, Bayern, Sachsen und Baden; jeder andere Staat ist ebenfalls wählbar.
- **Orte:** 3.967 Orte haben Besitzer und Bundesgebiet, 575 Orte im Ausland bleiben ohne Daten
  (Entscheidung 4). Die Flächen liegen nahe der Literatur, etwa Preußen 275.000 km² und Österreich
  im Bund 185.000 km².
- **Verwaltung:** 458 Einheiten.
  - Preußen: 8 Provinzen, 25 Regierungsbezirke, 327 Kreise.
  - Österreich: 11 Kronländer, 51 Kreise bzw. Viertel.
  - Die Mittelstaaten haben nur die oberste Ebene.
  - Godot zeigt sie im Kartenmodus „Verwaltung“ (Taste Ü) und mit der Verwaltungskette in der
    Ortsinfo.
- **Bevölkerung 1846:** 45,0 Mio. Einwohner mit Konfession, Kultur und Berufen, verteilt aus
  Summen je Staat, Provinz und Kronland.
- **Eisenbahnen:** 42 Strecken mit 176 Abschnitten; zum Start sind 165 eröffnet (rund 4.700 km).
  Nachbarmärkte, deren Bahnhöfe im selben Netz liegen, handeln zur Bahnfracht (1848: 55 Marktpaare).
  Godot zeigt das Netz im Kartenmodus „Eisenbahnen“ (Taste +).
- **Industrie** ab 1800 (`seit`, im Szenario 1648 nicht vorhanden):
  - Güter Kohle und Eisen; Beruf Fabrikarbeiter (rund 1,4 Mio. Köpfe).
  - Betriebe Zeche, Hütte, Spinnerei, Maschinenbau, Brennerei und Salzwerk.
  - Vorkommen an 34 Kohle-, 19 Erz-, 30 Salz- und 43 Weinorten.
- **Haushalte** in Talern im Maßstab des Modells, nach den Verhältnissen um 1847: Hof 3–12 % der
  Einnahmen; Schulden 0–3 und Schatz 0,05–1 Jahreseinnahmen.
- **Kalibrierung:** `tragfaehigkeit_faktor = 2.5` (Agrartechnik 1848). Der Balancing-Bericht 1848
  erfüllt alle Zielwerte in `zielwerte_voelkerfruehling.chron` (8 Läufe über 30 Jahre, in der CI):
  - kein Staatsbankrott;
  - Vermögen am Ende im Median 1,05–1,26 Jahreseinnahmen;
  - Bevölkerung 1878 ×1,17;
  - Lebensstandard unten 44, mittel 54, oben 78.
- **Leistung:** 97.257 Blöcke. Laden im Release-Build 0,6 s, 10 Spieljahre 2,7 s (4 Kerne).
- **1648 unverändert:** Die Prüfsumme nach 100 Jahren bleibt `5db13a66d63e6079`.
- **Tests:**
  - 153 Testfälle.
  - CTests `daten_pruefen_1848`, `determinismus_1848` und `speichertest_1848`; Prüfsumme 1848 im
    Determinismus-Job.
  - Godot-Einheitentest mit 228 Prüfungen; Rauchtest mit neuem Spiel 1848, Staatswechsel,
    Eisenbahnen und Laden eines Spielstands von 1648; Screenshot 1848.
- **Review M7b:** Kern, Kommandozeile, Brücke, Godot und Archivar wurden geprüft. Behoben sind acht
  Befunde: zwei mittlere und sechs niedrige. Die mittleren:
  - Ein Markt verband als Drehkreuz Strecken, zwischen deren Bahnhöfen kein Abschnitt lag.
  - Die Eisenbahnebene zeigte 1648 das ganze spätere Netz gestrichelt.

**Abnahme im Einzelnen**

| Kriterium | Stand |
|---|---|
| `uhrwerk-cli pruefen` ohne Fehler | erfüllt: 0 Fehler, 0 Warnungen für beide Szenarien (CTests `daten_pruefen`, `daten_pruefen_1848`); der Archivar prüft alle Tabellen in der CI |
| Jeder Datensatz mit Quelle | erfüllt im Sinne der Pipeline: Jede Zeile hat `quelle` und `sicherheit`, der Archivar lehnt Zeilen ohne ab. Inhaltlich sind die Angaben aber `ungeprueft` (Besitz, Verwaltung, Staaten, Eisenbahnen), `schaetzung` (Bevölkerung) oder `spielwert` (Haushalte), weil die Quellen in der Umgebung der Umsetzung nicht erreichbar waren |
| Historische Stichprobenprüfung durch den Beirat (oder eine dokumentierte Fachprüfung) | **offen** (Entscheidung 7). Vorbereitet ist [pruefung/stichprobe_1848.md](pruefung/stichprobe_1848.md) mit 60 zufällig gezogenen Datensätzen (fester Seed) und Prüfvermerken: 46 plausibel, 6 Vereinfachungen, 2 zweifelhaft, 6 Fehler behoben. Dazu kommen Summen je Staat und Provinz. Die fachliche Prüfung gegen Quellen kann nur ein Mensch leisten |

**Bekannte Grenzen**
- **Fachprüfung offen:** Vor der Abnahme durch den Beirat sind alle historischen Angaben ungeprüft.
  Die Bevölkerung je Ort ist aus Summen verteilt; einzelne Orte können deutlich abweichen.
- **Ausland ohne Daten:** Lothringen und Elsass, Belgien und Französisch-Flandern, die Niederlande
  außer Limburg, Kongresspolen, Jütland und das Burgenland (Ungarn) haben keine Daten (575 Orte).
  Welschtirol und das Küstenland liegen außerhalb der Karte.
- **Verwaltung der Mittelstaaten** nur mit der obersten Ebene; Kreise gibt es in Preußen und
  Österreich. Die Orte kommen zum Kreis mit dem nächsten Sitz.
- **Haushalte im Maßstab des Modells:** Das Modell erwirtschaftet weniger als die historischen
  Staaten (Preußen rund 13,7 statt gut 60 Mio. Taler Steuern). Hof, Schatz und Schulden sind
  deshalb als Anteile der Einnahmen gesetzt, nicht als historische Beträge.
- **Eisenbahnen:** Es gibt nur die Strecken bis 1848 (einzelne Abschnitte bis Ende 1848), kein
  Netzausbau danach. Abstände rechnen in Luftlinie, die Bahn senkt nur die Fracht.
- **Industrie minimal** (Entscheidung 5): Die Einfuhr englischer Waren fehlt, und Kohle und Eisen
  haben nur regionale Märkte. Waren, Kohle und Eisen haben deshalb weitere Preisbereiche in den
  Zielwerten.
- **Tragfähigkeit für alle Gelände gleich gehoben:** Regionale Unterschiede der Agrarreformen fehlen;
  die Auswanderung ist nicht modelliert.
- **Oberfläche:** Es gibt keinen eigenen Startbildschirm; das Spiel startet mit 1648, das Lesezeichen
  1848 wählt man über „Neues Spiel“ oder `--szenario`.
- **Alte Spielstände:** Spielstände aus M6 lassen sich nicht laden. Die Bereiche `regeln` und
  `maerkte` des Inhaltsfingerabdrucks haben sich geändert (Regel `bahnfracht`, Strecken).

### M8 – Politik, Parteien und Wahlen ✅

**Ziel:** Politisches System nach [09 Politik, Staat und Gesetze](09_politik_staat_und_gesetze.md),
[10 Parlamente, Wahlen, Koalitionen](10_parlamente_wahlen_koalitionen.md) und
[11 Parteien-Datenbank](11_parteien_datenbank.md).
Vertrag: [technik/politik.md](technik/politik.md).

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

**Ergebnis**
- **Vertrag** in [technik/politik.md](technik/politik.md) mit dem Stand je Schritt. Umgesetzt in zwei
  Teilen: M8a (Daten, Wahlkreise, Wahl im Kern, Wahllabor) und M8b (Kartenmodus, Wahlabend,
  Parteienmodus, Preußen 1849, Staatsinfo, Review).
- **Strömungen und Gruppen:** 5 Strömungen auf einer Achse von −100 bis +100 (Konservative,
  politischer Katholizismus, konstitutionelle Liberale, Demokraten, frühe Arbeiterbewegung) und
  11 Interessengruppen (zur Wahl 1848 zehn; die Arbeiterbewegung ab August). Die Gruppen tragen
  die Wahlneigung der Blöcke; die Staatsinfo zeigt ihren Anteil und Einfluss im Staat.
- **Verfassung und Wahlrecht** der 38 Staaten als Felder zum Startdatum: Verfassung, Wahlrecht
  (ständisch, Zensus, Dreiklassen, allgemeines Männerwahlrecht) und Wahlsystem. Die Wahl rechnet
  mit jeder Option. Godot zeigt sie im Fenster „Verfassung und Gruppen“ des Staatspanels.
- **Parlamente:** Deutsche Nationalversammlung (Wahl am 1. Mai 1848, indirekt, 8 Fraktionen ab dem
  Stichtag) und Preußische Zweite Kammer (17. Juli 1849, Dreiklassenwahl, 3 Fraktionen).
- **Wahlkreise** aus den Kreisen der Verwaltung, per Werkzeug `wahlkreise_1848.py`: 633 Wahlkreise
  mit 649 Sitzen (rund 70.000 Einwohner je Sitz) und 337 mit 350 Sitzen in Preußen 1849. Der
  Schnitt ist optimal nach gleicher Einwohnerzahl und kurzen Wegen; bis auf Oldenburgs Exklaven
  und zwei kleine Fälle liegt jeder Wahlkreis beim 2/3- bis 1,5-Fachen des Mittels seiner Region.
- **Wahl im Kern:** Wahlberechtigte, Beteiligung, Neigung je Block, Wahlmänner, Boykott, Stichwahl
  und Abgeordnete mit Position und Fraktion. Sie ist deterministisch und hat einen eigenen
  Zufallsstrom.
- **Ergebnis 1848** (16 Seeds, Median, in der CI): 587 besetzte Sitze, Boykott der Tschechen in
  Böhmen. Rechte 4,8 %, rechtes Zentrum 31,5 %, linkes Zentrum 19,6 %, Linke 19,1 %, fraktionslos
  24,5 %; alle Zielkorridore erfüllt ([technik/politik.md](technik/politik.md#141-kalibrierung-der-nationalversammlung-schritt-a4)).
- **Preußen 1849** (Preußische Zweite Kammer, 8 Seeds): Konservative und Liberale 85 %,
  Arbeiterbewegung 0 %; beide Ziele erfüllt.
- **Wahlabend:** Er öffnet am Wahltag. Die Wahlkreise melden in der Meldereihenfolge; dazu
  Hochrechnung mit Fehlerband, Halbrund nach Strömung oder Fraktion und Telegramme, die den
  Wahlkreis in der Ortsinfo zeigen. Der Kartenmodus „Wahlen“ (Taste Ä) zeigt Wahlkreise und Sieger.
- **Parteienmodus:** Man spielt eine Strömung. Bis zur nächsten Wahl führt man mit Spenden
  Wahlkampf (Versammlung, Flugblätter, Verein) und sieht Stimmungsberichte mit großem Fehler; nach
  der Wahl folgen Sitze, Fraktion und Abgeordnete. Nach der Nationalversammlung geht der Wahlkampf
  zur Preußischen Zweiten Kammer weiter. Wahlkampf kippt knappe Wahlkreise, keine Hochburgen.
- **Wahllabor:** `uhrwerk-cli wahl` rechnet eine Wahl über viele Seeds und prüft die Wahlziele.
- **1648 unverändert:** Die Prüfsumme nach 100 Jahren bleibt `5db13a66d63e6079`, weil der neue
  Zustand nur mit Parlamenten besucht wird.
- **Tests:** 169 Testfälle (vorher 153); CTests `wahl_fixture` und `wahl_1848`; Godot-Einheitentest
  mit 260 Prüfungen (vorher 228); Rauchtest mit Wahl, Parteienmodus und Staatsinfo; Klicktest mit
  Wahlabend, Parteienmodus und Staatsinfo; Screenshots von Wahlabend und Parteienmodus in der CI.
- **Review M8** in drei Teilen (Kern; Brücke und Godot; Werkzeuge, Daten und Doku). Behoben sind
  die gefundenen Fehler, darunter vier mittlere:
  - Wahlkreise innerhalb einer Region waren sehr ungleich groß (0,2- bis 1,8-faches Mittel).
  - Nach dem 1. Mai 1848 war der Wahlkampf zur Wahl 1849 nicht erreichbar.
  - Bei laufender Zeit gingen Klicks auf die Wahlkampf-Knöpfe verloren.
  - Eingereihte Wahlkampfbefehle wurden nicht gegen die Mittel gerechnet; überzählige verfielen
    still.

**Abnahme im Einzelnen**

| Kriterium | Stand |
|---|---|
| Wahl zur Nationalversammlung 1848 mit historisch plausibler Sitzverteilung, Abweichung dokumentiert | erfüllt im Sinne der Zielwerte: Alle Korridore in `wahlziele_voelkerfruehling.chron` sind über 16 Seeds erfüllt (Schritt „Wahllabor 1848/49“ in der CI; der CTest `wahl_1848` rechnet zwei Seeds). Die Abweichungen stehen in [technik/politik.md](technik/politik.md), Abschnitt 14.1. Die Richtwerte sind ungeprüft (Entscheidung 10) |
| Wahlabend mit Hochrechnung und Ergebnis je Wahlkreis | erfüllt: Hochrechnung mit Fehlerband je Strömung bzw. Fraktion, am Ende gleich dem Ergebnis des Kerns (Klicktest); Ergebnis je Wahlkreis in Telegramm, Karte und Ortsinfo |

**Bekannte Grenzen**
- **Fachprüfung offen:** Strömungen, Gruppen, Neigungen und Zielkorridore sind Spielwerte nach
  Literaturkenntnis, ohne Prüfung gegen Quellen.
- **Mehrheitswahl verstärkt Hochburgen:** Baden geht mit 20 von 20 Sitzen an die Demokraten, Sachsen
  mit 26 von 26. Historisch saßen auch dort Liberale. Preußen stellt mehr Konservative als
  historisch.
- **Sitze nach der geschätzten Bevölkerung 1846 im Wahlgebiet**, nicht nach der Bundesmatrikel;
  Posen hat deshalb zu viele Sitze. Polen und Dänen boykottieren im Modell nicht.
- **Wahlkreisnamen** nach dem größten Ort mit heutigem Namen (Wuppertal, Norderstedt).
- **Ein Wahltag:** Historisch zogen sich die Wahlen von April bis Mai hin.
- **Preußen 1849:** Die Demokraten boykottierten die Wahl historisch; das Modell kennt keinen
  Boykott einer Strömung. Die Beteiligung liegt mit 56 % wohl zu hoch.
- **Parteienmodus minimal:** Die anderen Strömungen führen keinen Wahlkampf (KI mit M12).
  Abstimmungen, Regierungsbildung und Koalitionen kommen mit M11.
- **Kein Gesetzgebungsverfahren:** Verfassung und Wahlrecht gelten zum Startdatum; Änderungen
  kommen mit den Ereignissen in M11. Die Interessengruppen sind statisch.
- **Keine Wahlen nach 1849.**
- **Alte Spielstände:** Spielstände aus M7 lassen sich nicht laden (Format 2, Inhaltsbereich
  `politik`).

### M9 – Diplomatie und Zollverein

**Ziel:** Diplomatie nach [14 Diplomatie](14_diplomatie.md) mit dem Zollverein als Kernsystem.
Entwurf: [technik/diplomatie.md](technik/diplomatie.md).

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
