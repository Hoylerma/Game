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
| M9 | Diplomatie und Zollverein | 2 – Vertical Slice | M7 | **fertig** (Fachprüfung der Werte offen, siehe unten) |
| M10 | Militär und Einigungskriege | 2 – Vertical Slice | M9 | **fertig** (Fachprüfung der Werte offen, siehe unten) |
| M11 | Ereignisse und Revolution 1848/49 | 2 – Vertical Slice | M8 | **fertig** (Fachprüfung der Quellen und Werte offen, siehe unten) |
| M12 | Künstliche Intelligenz der Staaten | 2 – Vertical Slice | M8–M11 | **fertig** (Fachprüfung der Personen und Agenden offen, siehe unten) |
| M13 | Spielbarer Vertical Slice 1848–1871 | 2 – Vertical Slice | M12 | **technisch fertig** (Spieltest mit externen Testern offen; CI auf GitHub läuft zurzeit nicht, siehe unten) |
| M14 | Europa und Oberfläche (aus dem ersten Spieltest) | 3 – Early Access | M13 | Vertrag zur Freigabe: [technik/europa_und_oberflaeche.md](technik/europa_und_oberflaeche.md) |

Phase 1 endet mit M6 (Ziel der Roadmap: „Technikprototyp“). Phase 2 endet mit M13 (Ziel:
„Spielbare Epoche III 1848–1871“). Mit M13 ist die Phase 2 technisch abgeschlossen; offen sind die Arbeiten,
die nur Menschen leisten können: der Spieltest aus M13 und die Fachprüfung der Daten und Werte aus
M7–M13.

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

### M9 – Diplomatie und Zollverein ✅

**Ziel:** Diplomatie nach [14 Diplomatie](14_diplomatie.md) mit dem Zollverein als Kernsystem.
Vertrag: [technik/diplomatie.md](technik/diplomatie.md) (Entscheidungen freigegeben, Stand in
Abschnitt 11).

**Umfang**
- Beziehungen, Bündnisse, Garantien, Bundesversammlung des Deutschen Bundes
- Zollverein: Beitritt, Austritt, Zolleinnahmen, Wirkung auf die Märkte aus M5
- Diplomatische Spiele zur deutschen Frage (kleindeutsch, großdeutsch)

**Abnahme**
- Zollverein-Mitgliedschaften 1848 historisch korrekt; Beitritte verändern Handel und Kassen
  messbar

**Ergebnis**
- **Vertrag** in [technik/diplomatie.md](technik/diplomatie.md) mit dem Stand je Schritt. Umgesetzt in
  zwei Teilen: M9a (Zollunionen: Daten, Zollgebiete im Kern, Zolllabor, Godot) und M9b (Diplomatie:
  Beziehungen, Bundesversammlung und Spiele im Kern, Brücke und Godot, Review).
- **Zollunionen:** Zollverein (26 Staaten unter Preußen, Vereinstarif 10 %) und Steuerverein
  (3 Staaten unter Hannover, 3 %); die übrigen neun Staaten sind eigene Zollgebiete. Jeder Markt
  teilt sich nach der Bevölkerung in Zollgebiete; Zoll fällt nur auf Waren von außerhalb des
  eigenen Gebiets an, die Vereinskasse verteilt ihn nach der Bevölkerung. Beitritt, Austritt und
  Wechsel gelten zum 1. Januar; die Leitmacht entscheidet über Beitritte (der Zollverein lehnt
  Österreich ab), ohne sie löst sich ihre Union auf.
- **Zolllabor** (`uhrwerk-cli zoll`, in der CI): Zollanteil des Zollvereins 2,9 % der Einnahmen; der
  Beitritt Hannovers erhöht den Grenzhandel um 40 %, der Mecklenburg-Schwerins verdoppelt ihn.
- **Beziehungen** zwischen allen 38 Staaten (703 Paare) mit Zielwert aus Grundwert, Zollunion, Bund,
  Konfession und Verträgen; Gesandte (Preußen 4, Österreich 3, die übrigen 1), Bündnisse und
  Garantien nach Regel. 1848: Gegner sind Österreich–Preußen und Dänemark–Preußen; im Lager Preußens
  stehen 21 Staaten, im Lager Österreichs 5, unentschieden sind 10.
- **Bundesversammlung** in Frankfurt (Präsidium Österreich, Plenum 68 Stimmen, Engerer Rat 17 mit
  Kurien): Anträge auf Bundesreform, Zolleinigung, Bundesintervention und Präsidium, Abstimmung am
  nächsten Monatsersten nach den Beziehungen. Eine angenommene Zolleinigung ist verbindlich.
- **Diplomatische Spiele:** Forderungen (Führung im Bund, Beitritt zur Zollunion, Bündnisse lösen),
  Parteinahme der Bundesstaaten, drei Stufen zu 30 Tagen (Protest, Ultimatum, Mobilmachung),
  Manöver (nachgeben, Vermittlung, eskalieren) und Auflösung nach Stärke, ohne Krieg.
- **Godot:** Kartenmodi „Zollgebiete“ (Taste Ö) und „Beziehungen“ (Taste #), Fenster „Zollunion“
  und „Diplomatie“ (Staaten, Bundesversammlung mit Halbrund, Deutsche Frage), Krisenleiste mit
  Manövern, Haushaltsposten „Gesandte“.
- **Spielstand Format 3** mit dem Inhaltsbereich `diplomatie`; der neue Zustand wird nur mit
  Zollunionen bzw. dem Deutschen Bund besucht.
- **1648 unverändert:** Die Prüfsumme nach 100 Jahren bleibt `5db13a66d63e6079`.
- **Tests:** 189 Testfälle (vorher 169); CTests `zollunionen_1848` und `zoll_1848`; Godot-Einheitentest
  mit 277 Prüfungen (vorher 260); Rauchtest mit Zollunionen und Diplomatie; Klicktest mit Beitritt,
  Gesandtem, Forderung, Krisenleiste und Antrag; Screenshots der Zollgebiete, Beziehungen und der
  Bundesversammlung in der CI.
- **Review M9** über Kern, Brücke und Godot, Daten und Doku. Behoben sind:
  - Ein erzwungener Beitritt (Zolleinigung, verlorene Krise) ließ sich bis zum 1. Januar
    zurückziehen.
  - Eine Abstimmung oder Krise am 1. Januar kündigte den Beitritt zum selben Tag an; er galt aber
    erst ein Jahr später.
  - Die Liste der Beziehungen war im Tooltip teuer.
  - Kleinigkeiten im Fenster „Diplomatie“; der Knopf des Kartenmodus heißt jetzt „Beziehung“.

**Abnahme im Einzelnen**

| Kriterium | Stand |
|---|---|
| Zollverein-Mitgliedschaften 1848 historisch korrekt | erfüllt: CTest `zollunionen_1848` prüft die Mitglieder gegen die Liste in [technik/diplomatie.md](technik/diplomatie.md), Abschnitt 2; `uhrwerk-cli info` nennt Unionen und Mitglieder |
| Beitritte verändern Handel und Kassen messbar | erfüllt: Das Zolllabor misst Grenzhandel, Handel, Zoll und Preise mit und ohne Beitritt über 8 Seeds gegen Zielwerte (Schritt „Zolllabor 1848“ in der CI, CTest `zoll_1848`) |

**Bekannte Grenzen**
- **Fachprüfung offen:** Grundbeziehungen, Stimmen, Schwellen und Tarife sind Spielwerte nach
  Literaturkenntnis, ohne Prüfung gegen Quellen.
- **Zollanteil zu klein:** Der Zoll bringt dem Zollverein knapp 3 % der Einnahmen, historisch eher
  10–20 %; dafür fehlt dem Modell Einfuhr von Kolonial- und englischen Fabrikwaren.
- **Keine KI:** Die anderen Staaten handeln nicht von sich aus (M12); sie stimmen und nehmen Partei
  nach Regel.
- **Bundesreform und Präsidium scheitern nach Regel immer** (Einstimmigkeit, wie historisch).
- **Krisen ohne Krieg** und ohne Außenmächte; die Führung im Bund wirkt nur auf die Anzeige „Die
  deutsche Frage“. Krieg und Bündnisfall kommen mit M10.
- **Die Bundesversammlung tagt durchgehend**; ihre Suspendierung 1848–1850 kommt mit M11.
- **Alte Spielstände:** Spielstände aus M8 laden nicht (Format 3), Spielstände 1848 aus den ersten
  M9-Schritten ebenso nicht (Inhaltsbereich `diplomatie`).

### M10 – Militär und Einigungskriege ✅

**Ziel:** Kriegführung nach [15 Militär und Kriegsführung](15_militaer_und_kriegsfuehrung.md).
Vertrag: [technik/militaer.md](technik/militaer.md) (Entscheidungen freigegeben, Stand in
Abschnitt 12).

**Umfang**
- Heere, Aushebung, Versorgung, Bewegung auf der Karte, Schlachten, Fronten
- Eisenbahnen als Aufmarschwege
- Kriegsziele, Friedensverhandlungen, Gebietsabtretungen
- Ausgangslagen der Kriege 1864, 1866 und 1870/71

**Abnahme**
- Die drei Kriege lassen sich durchspielen; ein ungestörter Lauf ohne Spieler führt zu
  plausiblen, nicht festgelegten Ergebnissen
- Heereskosten belasten den Haushalt aus M5

**Ergebnis**
- **Vertrag** in [technik/militaer.md](technik/militaer.md) mit dem Stand je Schritt. Umgesetzt in
  zwei Teilen: M10a (Krieg im Kern auf den Daten von 1848: Nachbarschaft, Heere, Bewegung, Schlacht,
  Besetzung, Frieden, Generalstab, Kriegslabor) und M10b (Daten 1849–1870, drei Kriegslesezeichen,
  Kalibrierung, Brücke und Godot, Review).
- **Netz und Heere:** 13.069 Paare benachbarter Orte mit Grenzlänge und gekreuztem Fluss, 13 Fähren;
  Garnisonen an den Korpsstandorten (1848 zusammen 382.528 Soldaten), Feldarmeen erst mit der
  Mobilmachung (Preußen ×2,5 in 28 Tagen), Abrüstung in 30 Tagen. Märsche über Land und Bahn auf dem
  schnellsten Weg (20 km bzw. 300 km je Tag, Einladen nach Kapazität, Warten auf belegten Strecken),
  Versorgung über Bahnhöfe der eigenen Seite, Zermürbung im Winter.
- **Krieg:** Kriegserklärung mit Bündnisfall, Krise → Krieg, Bundesexekution; tägliche Schlachten
  mit eigenem Zufallsstrom (Frontbreite, Gelände, Festung, Fluss, Geschütze, Moral), Rückzug,
  Gefangene, Verwundete, Gefallene in der Bevölkerung; Besetzung, Belagerung und Einschließung;
  Kriegsziele (Gebiet, Annexion, Führung im Bund, Zollunion, Entschädigung) mit Kosten in
  Kriegspunkten, Kriegsmüdigkeit, Friedensangebote und Frieden nach Regel, Besitzwechsel zur
  Laufzeit, Untergang von Staaten und Auflösung des Bundes. Ein Generalstab führt die Heere nach
  Regeln; der Spieler kann ihm Heere übergeben.
- **Daten 1849–1870:** Historie mit 38 datierten Änderungen (u. a. Annexionen 1866, Norddeutscher
  Bund, Zollverein 1854 und 1868), Frankreich mit 217 Orten und Hinterland, 62 weitere Bahnstrecken
  (185 Abschnitte), Heerwesen, Garnisonen und Festungen bis 1870.
- **Drei Kriegslesezeichen** „Düppel“ (1.2.1864), „Königgrätz“ (16.6.1866) und „Sedan“ (19.7.1870), die
  im Krieg beginnen (Aufmarsch mit Befehlshabern, Kriege mit Zielen beider Seiten, Bündnisse).
- **Kriegslabor** (`uhrwerk-cli krieg`, in der CI): Düppel 16 von 16 Läufen für Preußen (Median 90
  Tage), Königgrätz 11 von 16 (107 Tage), Sedan 14 von 16 für die deutsche Seite (197 Tage); in
  keinem Lesezeichen derselbe Ausgang in allen Läufen außer Düppel (dort historisch zwingend,
  Abweichung in Abschnitt 6.3 des Vertrags).
- **Godot:** Heere als Marken mit Stärke, Auswahl per Klick, Marschbefehl per Rechtsklick mit Weg und
  Ankunft, Schlachten und Belagerungen auf der Karte; Kartenmodus „Krieg“ (Taste Y) mit
  schraffierten besetzten Orten und Fronten; Heeresfenster, Kriegsfenster mit Friedensangeboten samt
  Vorab-Prüfung und weiteren Zielen, „Krieg erklären …“ im Fenster „Diplomatie“, Mobilmachung im
  Staatspanel, Schlachtberichte; die drei Lesezeichen im Menü „Neues Spiel“.
- **Spielstand Format 4** mit dem Inhaltsbereich `militaer`; der Zustand des Militärs wird nur in
  Welten mit Militärdaten besucht.
- **1648 unverändert:** Die Prüfsumme nach 100 Jahren bleibt `5db13a66d63e6079`.
- **Tests:** 216 Testfälle (vorher 189); neue CTests `krieg_daten`, `pruefen_krieg_fixture`,
  `krieg_fixture`, `krieg_1848`, je Lesezeichen `daten_pruefen_<lesezeichen>` und
  `krieg_<lesezeichen>` sowie `speichertest_koeniggraetz`; Godot-Einheitentest mit 302 Prüfungen
  (vorher 277); Rauchtest Königgrätz; Klicktest mit Auswahl, Marsch, Generalstab, Kriegsfenster,
  Schlachtbericht, Kriegserklärung, dem Durchspielen aller drei Lesezeichen bis zum Frieden und dem
  Untergang des Spielerstaates; Screenshots des Kartenmodus „Krieg“ und des Kriegsfensters in der CI.
- **Review M10** über Kern, Brücke und Godot, Daten und Doku. Behoben sind:
  - Das Hinterland zahlte seine vollen Einnahmen in die Kasse, ohne die Ausgaben seiner Bevölkerung:
    Frankreich tilgte mitten im Krieg 1870 seine ganzen Schulden, und Dänemark verfehlte seit b1 den
    Balancing-Bericht 1848 (Vermögen). Jetzt ist sein Überschuss ein Posten im Haushalt
    im Verhältnis, das der Kartenteil des Staates im Frieden übrig behält (Frankreich 690.000 statt
    2,6 Mio. Taler je Monat, Dänemark 40.000 statt 130.000).
  - Im Parteienmodus ließ sich ein Heer des Staates per Rechtsklick in Marsch setzen.
  - Verliert der Spielerstaat alle Orte, hält die Zeit an und ein Hinweis nennt den Weg zu einem
    anderen Staat.
  - Grenzen und Staatsnamen folgten einem Frieden erst am nächsten Monatsersten, die Legende im
    Kartenmodus „Krieg“ einem neuen Krieg ebenso.
  - Das Kriegsfenster rechnete die Zielauswahl jede Sekunde neu (rund 18 ms) und schloss dabei eine
    offene Auswahl.

**Abnahme im Einzelnen**

| Kriterium | Stand |
|---|---|
| Die drei Kriege lassen sich durchspielen | erfüllt: Der Klicktest spielt Düppel, Königgrätz und Sedan als Preußen mit Marschbefehl per Rechtsklick und einem angenommenen Friedensangebot bis zum Frieden; Königgrätz zusätzlich mit Generalstab, Schlachtbericht und Kriegserklärung; der Rauchtest prüft die Brücke |
| Ein ungestörter Lauf ohne Spieler führt zu plausiblen, nicht festgelegten Ergebnissen | erfüllt: Das Kriegslabor rechnet je Lesezeichen 16 Seeds gegen die Korridore der Kriegsziele (CI-Schritt „Kriegslabor Lesezeichen“); Königgrätz und Sedan enden nicht in allen Läufen gleich, Düppel mit Absicht immer für Preußen |
| Heereskosten belasten den Haushalt aus M5 | erfüllt: Der Einheitentest prüft den Posten „Heer“ mit der Mobilmachung (×2,4 bis ×2,5); das Kriegslabor berichtet die Schulden der Hauptparteien vor und nach dem Krieg |

**Bekannte Grenzen**
- **Fachprüfung offen:** Stärken, Daten des Aufmarschs und Hinterlands sind `ungeprueft`, Kampfwerte
  und Regeln Spielwerte nach Literaturkenntnis, ohne Prüfung gegen Quellen.
- **Bevölkerung der Kriegslesezeichen von 1846** ohne Hochrechnung auf 1864–1870 (Entscheidung 14).
- **Keine KI:** Außer dem Generalstab, der Heere führt, handeln die Staaten nicht von sich aus (M12):
  Sie erklären keine Kriege und bieten keinen Frieden an; Frieden schließen sie nach Regel.
- **Haushalt im Krieg:** Die Staaten wirtschaften schon im Frieden mit Überschuss (keine
  Ausgabenpolitik ohne KI); Preußen tilgt 1870 trotz Mobilmachung weiter Schulden, Frankreich
  verschuldet sich.
- **Keine Arbeitskräfte:** Reservisten fehlen der Wirtschaft nicht (Entscheidung 7).
- **Keine Flotte, keine Außenmächte** außer Frankreich (Italien, Russland, Großbritannien fehlen).
- **Kriegsfenster** ohne eigene Höhenanpassung: Mit sehr vielen Kriegen kann es unter die Leiste der
  Kartenmodi reichen.
- **Alte Spielstände:** Spielstände aus M9 laden nicht (Format 4), Spielstände 1848 und der Kriegslesezeichen
  aus früheren M10-Schritten ebenso nicht (Inhaltsbereich `militaer`).

### M11 – Ereignisse und Revolution 1848/49 ✅

**Ziel:** Ereignisse, Journale und Missionen nach
[18 Ereignisse, Journale, Missionen](18_ereignisse_journale_missionen.md).
Vertrag: [technik/ereignisse.md](technik/ereignisse.md) (Entscheidungen freigegeben, Stand in
Abschnitt 13).

**Umfang**
- Ereignissystem im Chronik-Skript (Bedingungen, Auswahlmöglichkeiten, Folgen)
- Märzrevolution, Paulskirche und Verfassungskonvent, Reichsverfassungskampagne, Olmütz
- Journal „Deutsche Frage“ mit offenen Ausgängen
- Leitlinien aus [20 Verantwortung und Erinnerungskultur](20_verantwortung_und_erinnerungskultur.md) beachten

**Abnahme**
- Mindestens 80 Ereignisse für 1848–1871 mit Quellenangaben
- Die Revolution kann scheitern oder gelingen; beide Wege sind spielbar

**Ergebnis**
- **Vertrag** in [technik/ereignisse.md](technik/ereignisse.md) mit dem Stand je Schritt. Umgesetzt in
  zwei Teilen: M11a (System im Kern auf einer Test-Fixture: Gesetze, Ereignisse, Zustimmung, Unruhe,
  Aufstände, Journale, Abstimmungen, Ereignislabor) und M11b (Inhalt 1848–1871, Kalibrierung, Brücke
  und Godot, Review).
- **Ereignisse im Chronik-Skript:** Bedingungen, Optionen mit Gewichten und Zuschlägen, Wirkungen aus
  einem festen Vokabular (Gesetze, Verfassung, Wahlrecht, Zustimmung, Unruhe, Beziehungen, Kasse,
  Folgeereignisse, Befristungen, Aufstände, Anträge im Bund, Kriegserklärung mit Zielen, Merkmale);
  tägliche Auslösung mit Wartezeit, Termin und Wiederholung. Ohne Spieler wählt der Kern nach Gewicht
  aus dem Zufallsstrom `Ereignisse`; Ereignisse des Spielerstaates warten bis zur Wahl (Befehle
  `Ereigniswahl` und `Ereignisspieler`, Frist 30 Tage).
- **Gesetze, Zustimmung, Unruhe:** drei Gesetzesgruppen (Presse, Vereine, Grundlasten) mit Startwerten
  je Staat, Wünsche der Interessengruppen, monatliche Annäherung von Zustimmung und Unruhe an ihre
  Zielwerte (Not, Teuerung, Verfassung, Zensur, Ansteckung), Steuerverweigerung und Auswanderung,
  Aufstände mit wöchentlichem Kräftevergleich, Intervention und drei Ausgängen.
- **Journale** „Revolution 1848/49“ (7 Stufen, 5 Ausgänge) und „Die deutsche Frage“ (bis 1871, 5
  Ausgänge) als Zustandsautomat mit Zielen; acht Abstimmungen der Paulskirche nach Fraktionen mit den
  Sitzen der Wahl aus M8; die Bundesversammlung ruht ab Juli 1848.
- **Inhalt:** 109 Ereignisse (83 historisch, 15 dynamisch, 11 atmosphärisch), alle mit Quelle: 65
  für 1848–1851, 33 für 1852–1871, elf wiederholbare aus dem Zustand (Missernte, Cholera,
  Auswanderung …); Gesetze 1848 bis 1854 in `staaten_historie.csv`.
- **Ereignislabor** (`uhrwerk-cli ereignisse`, in der CI): Ohne Spieler endet die Revolution über 64
  Läufe zu 68,8 % in der Gegenrevolution, zu 21,9 % im Reich der Paulskirche und zu 9,4 % in der
  Union; mit der Strategie `krone` erreicht Preußen das Reich, mit `reaktion` die Gegenrevolution
  (jeweils alle Läufe). Die deutsche Frage endet über 48 Läufe zu 35 % kleindeutsch, zu 23 % in zwei
  Bünden und zu 42 % im alten Bund.
- **Godot:** Ereignisfenster mit Text, Quelle, Optionen samt Folgen und historischem Kompass, das die
  Zeit anhält; Journalfenster (Taste J) mit Zielen, Weg, Ausgängen und den bisherigen Ereignissen;
  Gesetze, Zustimmung, Unruhe und Aufstand in der Staatsinfo; Kartenmodus „Unruhe“ (Taste X);
  Wendepunkte in der Chronik; die ruhende Bundesversammlung im Diplomatiefenster.
- **Spielstand Format 5** mit dem Inhaltsbereich `ereignisse` (Stand 6); Zustand der Ereignisse nur in
  Welten mit Ereignissen.
- **1648 unverändert:** Die Prüfsumme nach 100 Jahren bleibt `5db13a66d63e6079`.
- **Tests:** 239 Testfälle (vorher 216); neue CTests `ereignis_daten`, `pruefen_ereignis_fixture`,
  `ereignisse_fixture` mit den Strategien `krone` und `reaktion`, `ereignisse_1848` und
  `ereignisse_quellen`; Godot-Einheitentest mit 317 Prüfungen (vorher 302); Rauchtest der Ereignisse
  1848; Klicktest mit Ereignisfenster, Journalen, Kartenmodus „Unruhe“, ruhender Bundesversammlung und
  den beiden Wegen der Abnahme; Screenshots des Ereignis- und des Journalfensters in der CI.
- **Review M11** über Kern, Brücke und Godot, Daten und Doku. Behoben ist, dass die Brücke die
  Aufstandsart „Barrikadenkämpfe“ und die Ereignisart „atmosphärisch“ als Latin-1 statt UTF-8 an
  Godot gab („BarrikadenkÃ¤mpfe“); der Klicktest prüft das jetzt.

**Abnahme im Einzelnen**

| Kriterium | Stand |
|---|---|
| Mindestens 80 Ereignisse für 1848–1871 mit Quellenangaben | erfüllt: 109 Ereignisse, alle mit `quelle` (`uhrwerk-cli info`: „109 mit Quelle“; CTest `ereignisse_quellen` verlangt mindestens 80); die Quellen sind Literaturangaben ohne Seitenprüfung |
| Die Revolution kann scheitern oder gelingen; beide Wege sind spielbar | erfüllt: Ohne Spieler treten Gegenrevolution, Reich und Union auf, keiner in allen Läufen (CI-Schritt „Ereignislabor 1848“); die Strategien `krone` und `reaktion` erreichen ihren Ausgang in allen Läufen. Der Klicktest nimmt als Preußen über das Ereignisfenster die Krone an und erreicht am 5. April 1849 das Reich der Paulskirche; als Baden beantwortet er die Märzforderungen mit dem Militär und erlebt Barrikadenkämpfe |

**Bekannte Grenzen**
- **Fachprüfung offen:** Ereignisse, Daten und Quellen sind `ungeprueft` (Literatur ohne Seitenzugriff),
  Gewichte und Wirkungen Spielwerte aus der Kalibrierung.
- **Keine KI:** Die Staaten wählen ihre Optionen nach Gewicht, nicht nach Lage und Zielen (M12).
- **Parteienmodus ohne eigene Ereignisse:** Entscheidung 9 sieht Ereignisse der gespielten Strömung
  vor; der Kern kennt sie noch nicht, im Parteienmodus wählt der Staat nach Gewicht.
- **Deutsche Frage ohne Gebietsänderung aus 1848:** Besitz, Bundesgebiet und Bundesversammlung einer
  Partie ab 1848 bleiben die des Starts (militaer.md 6.1); ein Reich entsteht als Merkmal und über
  Kriege der Ereignisse.
- **Anzeige:** Zustimmung als Zahl statt Balken, Aufstände nur im Kartenmodus „Unruhe“ (kein Symbol
  in der Hauptstadt).
- **Alte Spielstände:** Spielstände aus M10 laden nicht (Format 5), Spielstände 1848 aus früheren
  M11-Schritten ebenso nicht (Inhaltsbereich `ereignisse`).

### M12 – Künstliche Intelligenz der Staaten ✅

**Ziel:** Computerstaaten nach [21 Künstliche Intelligenz](21_kuenstliche_intelligenz.md).
Vertrag: [technik/ki.md](technik/ki.md) (Entscheidungen freigegeben, Stand in Abschnitt 13).

**Umfang**
- Entscheidungen zu Haushalt, Steuern, Diplomatie, Krieg und Politik
- Persönlichkeiten der Herrscher und Minister aus [13 Persönlichkeiten](13_persoenlichkeiten.md)
- KI rechnet deterministisch im Kern (eigener Zufallsstrom)

**Abnahme**
- 100 KI-Partien 1848–1871 ohne Spieler: keine Abstürze, Ergebnisse plausibel verteilt
  (Bericht mit Häufigkeiten: Einigung ja/nein, Form der Einigung)

**Ergebnis**
- **Vertrag** in [technik/ki.md](technik/ki.md) mit dem Stand je Schritt. Umgesetzt in zwei Teilen:
  M12a (System im Kern auf einer Test-Fixture: Personen, Agenden, Planer, KI-Labor) und M12b (Daten
  1848–1871, Kalibrierung mit 100 Partien, Brücke und Godot, Review).
- **Persönlichkeiten:** sechs Neigungen (Risiko, Expansion, Legitimität, Reform, Sparsamkeit,
  Vermittlung) aus sechs Staatspersönlichkeiten und 16 Eigenschaften nach Kapitel 13; Herrscher und
  leitender Minister je Staat mit Amtszeit, Bedingung und Werten, Amtswechsel am Monatsersten mit
  Chronik (Märzministerien, Metternichs Entlassung, Abdankung Ludwigs I., Bismarck 1862).
- **Agenden:** Ziele mit Gewicht, Zeitfenster und Bedingung (Zollunion, Führung im Bund, Status quo,
  Ruhe, Bündnis, Gebiet, Annexion, Schwächen); Hauptziel und Rivale je Staat jeden Monatsersten.
- **Planer** am Monatsersten nach den Ereignissen, für jeden Staat außer dem Spielerstaat: Haushalt
  (Kontribution und Akzise nach Defizit, Schuldenquote, Überschuss und Unruhe), Mobilmachung und
  Abrüstung, Gesandte, Bündnisse gegen einen gemeinsamen Gegner mit Annahme nach Nutzen, Garantien,
  Forderungen, Manöver in Krisen, Anträge im Bund, Zollunionen, Kriegserklärung aus der Agenda bei
  Übermacht, weitere Kriegsziele, Frieden nach Nutzen, Wahlkampf der Strömungen und Neigungen an
  Ereignisoptionen. Jede Option ist eine Summe benannter Faktoren mit Rauschen aus dem Strom `Ki`; die
  KI gibt dieselben Befehle wie der Spieler (am nächsten Tag wirksam, nicht im Befehlsprotokoll) und
  schreibt die drei wichtigsten Gründe ins KI-Protokoll.
- **Daten 1848–1871:** 71 Personen (25 Herrscher, 46 Minister) von elf Staaten, Agenden von 14 Staaten
  mit 44 Zielen, Blöcke `ki` an 35 Ereignisoptionen.
- **KI-Labor** (`uhrwerk-cli ki`) mit Korridoren je Szenario, Läufen parallel (`--parallel`, gleiche
  Prüfsummen) und zum Vergleich ohne KI (`--ohne-ki`); in der CI mit 16 Partien, im Workflow
  `ki-partien.yml` mit 100 (manuell und wöchentlich).
- **Godot:** Herrscher, Minister und Neigungen in der Staatsinfo; Absichten (nur mit eigenem
  Gesandten) und die letzten Entscheidungen mit „Warum?“ im Diplomatiefenster; Hinweise bei
  Kriegserklärungen und Forderungen an den Spieler; Gründe in Chronik, Krisenleiste und Kriegsfenster.
- **Befehle** `Spielerstaat` (vorher `Ereignisspieler`) und `Spielerströmung`; **Spielstand Format 6**
  mit dem Inhaltsbereich `ki`; Zustand der KI nur in Welten mit KI-Daten.
- **1648 unverändert:** Die Prüfsumme nach 100 Jahren bleibt `5db13a66d63e6079`. 1848 rechnet mit KI
  anders; alle Labore 1848 (Revolution, Strategien, deutsche Frage, vier Kriegslabore, Zoll, Wahl,
  Bilanz) wurden mit KI neu gerechnet und halten ihre Korridore.
- **Tests:** 253 Testfälle (vorher 239); neue CTests `ki_daten`, `pruefen_ki_fixture`, `ki_fixture`,
  `ki_fixture_ohne_ki` und `ki_1848`; CI-Schritt „KI-Labor 1848–1871“; Rauch- und Klicktest mit
  Personen, Absichten und „Warum?“ 1848.
- **Review M12** über Kern, Brücke und Godot, Daten und Doku: Einheitentests und das
  KI-Labor 1848/49 unter ASan/UBSan, das KI-Labor parallel unter TSan, ohne Befund; die Spielstände mit KI prüft der Speichertest 1848.
  Behoben ist eine falsche Angabe in der Brücken-Doku (Rückgabe von `get_intentions` ohne KI).

**Abnahme im Einzelnen**

| Kriterium | Stand |
|---|---|
| 100 KI-Partien 1848–1871 ohne Spieler, keine Abstürze | erfüllt: `uhrwerk-cli ki --szenario voelkerfruehling --seeds 100` mit 0 Abstürzen und ohne Staatsbankrott, zweimal gerechnet bitgleich (Workflow `ki-partien.yml`); 24,8 Minuten auf vier Kernen, davon die KI 3,6 % (0,2 ms je Staat und Monat, Ziel 5 ms) |
| Ergebnisse plausibel verteilt, Bericht mit Häufigkeiten (Einigung ja/nein, Form der Einigung) | erfüllt: Einigung 67 % (ohne KI 56 % über 16 Partien), kleindeutsch 44 %, zwei Bünde 23 %, alter Bund 33 %, großdeutsch und ohne Bund 0 %; kein Ausgang in allen Läufen; Krieg Preußen–Österreich in 70 % der Partien. Alle Korridore aus `common/kiziele_voelkerfruehling.chron` erfüllt |

**Bekannte Grenzen**
- **Fachprüfung offen:** Personen, Eigenschaften, Agenden und Gewichte sind Spielwerte aus der
  Kalibrierung (`sicherheit = spielwert`), Amtszeiten nach Literatur ohne Seitenprüfung.
- **Laufzeit der 100 Partien:** 24,8 statt 20 Minuten auf vier Kernen; die KI braucht 3,6 %, der Rest
  ist die Simulation selbst.
- **Eine Schwierigkeitsstufe, historischer Modus:** Schwierigkeitsgrade und der offene Modus
  (Regel `agenda_offen`) werden mit M13 wählbar.
- **Vollständiges Lagebild:** Die KI kennt alle Stärken (keine Informationslücken und Spionage); nur
  die Anzeige der Absichten verlangt einen Gesandten.
- **Keine Feldzugsplanung:** Märsche führt weiter der Generalstab aus M10; die KI entscheidet über
  Mobilmachung, Krieg, Ziele und Frieden.
- **Keine Gesetzgebung und Ernennungen durch die KI:** Gesetze ändern sich nur durch Ereignisse;
  Personen altern und sterben nicht, ihre Amtszeiten stehen fest oder folgen Bedingungen.
- **Außenmächte:** Außer Frankreich und Dänemark gibt es keine; Russland, Großbritannien und Italien
  bleiben Text in Ereignissen.
- **Alte Spielstände:** Spielstände aus M11 laden nicht (Format 6).

### M13 – Spielbarer Vertical Slice 1848–1871

**Ziel:** Eine Epoche, die sich wie das fertige Spiel anfühlt – Grundlage für Förderung,
Publisher und Early Access.
Vertrag: [technik/vertical_slice.md](technik/vertical_slice.md) (Entscheidungen freigegeben, Stand in
Abschnitt 13).

**Umfang**
- Oberfläche nach [22 UI/UX, Grafik, Audio](22_ui_ux_grafik_audio.md): alle Fenster der Epoche,
  Einführung bzw. Tutorial, Musik und Geräusche in Grundfassung
- Lokalisierung Deutsch (Englisch vorbereitet)
- Installierbare Versionen für Windows, Linux und macOS

**Abnahme**
- Eine Partie 1848–1871 lässt sich vollständig ohne Fehler spielen
- Spieltest mit externen Testern; Rückmeldungen dokumentiert
- Leistungsziele aus M3 und M4 gelten auch mit allen Systemen

**Ergebnis**
- **Vertrag** in [technik/vertical_slice.md](technik/vertical_slice.md) mit dem Stand je Schritt. Umgesetzt
  in zwei Teilen: M13a (die Partie mit Anfang und Ende, Hauptbildschirm, Leistung) und M13b
  (Präsentation, Onboarding, Lokalisierung, Auslieferung, Spieltest, Review).
- **Start und Einstellungen:** Startbildschirm mit Hauptmenü, neues Spiel in drei Schritten (Lesezeichen
  1848 und drei Kriegslesezeichen, 1648 unter „Weitere“; Preußen, Österreich, Bayern und Sachsen
  empfohlen; Schwierigkeit Lehrling, Geselle, Meister als Parameter der KI-Planer; Geschichtsverlauf
  historisch oder offen). **Spielstand Format 7** mit Spieleinstellungen, Bünden, Wahlterminen,
  Jahresreihen und früheren Amtszeiten.
- **Die Partie:** Bünde entstehen zur Laufzeit aus Vorlagen (Norddeutscher Bund, Reich der Paulskirche,
  großdeutsches Reich, Deutsches Reich 1871) mit Bundesgebiet, Bundespflicht und Kartenmodus „Bund“;
  Wahlen gehen bis 1871 weiter (Preußisches Abgeordnetenhaus zu den historischen Terminen, Reichstag
  eines gegründeten Bundes); das **Vermächtnis** (Einigkeit, Recht, Freiheit, Wohlstand, Geltung) wird
  aus dem Zustand gemessen; am 1. Juli 1871 fällt das **Urteil der Geschichte** mit Wendepunkten und
  Chronik als HTML, danach Weiterspielen oder Hauptmenü.
- **Hauptbildschirm nach Kapitel 22:** obere Leiste, Bereichsleiste, Staatsübersicht, rechte Übersicht,
  Ticker mit den Kartenmodi; Fenster Kabinett und Hof, Gesetze, Parlament, Interessengruppen,
  Gesellschaft, Wirtschaft, Statistisches Jahrbuch, Chronik, Lexikon; verschachtelte Tooltips,
  Pausenregeln, Einstellungen mit Barrierefreiheit und Tastenbelegung.
- **Präsentation:** Epochenrahmen mit eigenen Ornamenten, vier eingebettete Schriften (OFL),
  Vignetten, die **Zeitung** zu wichtigen Nachrichten; **Musik** aus sieben gemeinfreien Stücken als
  eigener Notentext mit eigenem Synthesizer, sechs Bedienklänge, Stimmung nach der Lage des Staates.
- **Onboarding:** Geschichtsstunde „März 1848 in Berlin“ in vierzehn Schritten, Hinweise beim ersten
  Öffnen von 19 Fenstern, „Was ist passiert?“ nach dem Laden.
- **Lokalisierung:** gettext mit Deutsch als Quellsprache (1.304 Texte), Englisch für Startbildschirm,
  Spielmenü und Einstellungen, Zahlen und Daten je Sprache, Pseudo-Locale-Test.
- **Auslieferung:** Version 0.13.0; Pakete für Linux (tar.gz), Windows (Installer und ZIP) und macOS
  (DMG, universal, ad hoc signiert) mit `DRITTANBIETER.txt`, Workflow `pakete.yml` mit Paketrauchtest;
  Kern-Job unter macOS, Determinismus über vier Plattformen; Rückmeldung im Spiel mit Bildrate.
- **Spieltest vorbereitet:** Leitfaden, Fragebogen, Auswertung und Ergebnisvorlage in
  [pruefung/spieltest_m13.md](pruefung/spieltest_m13.md).
- **Leistung:** Profil 1848 und Optimierung (128-Bit-Festkomma, Hauptstadt zuerst, wiederverwendete
  Speicher); Spieljahr 1848 im Median 1,2 s auf vier Kernen (Ziel 2 s).
- **1648 unverändert:** Die Prüfsumme nach 100 Jahren bleibt `5db13a66d63e6079`.
- **Tests:** 267 Testfälle (vorher 253), 46 CTests (neu u. a. `leistung_1848`, `zeitung_1848`,
  `ki_1848_lehrling`, `ki_1848_meister_offen`); Godot: Einheitentest
  (517 Prüfungen), Rauchtest, Klicktest mit Startbildschirm, allen Fenstern, Zeitung, Musik und
  Geschichtsstunde, **Durchspieltest** über vier Staaten bis 1871, **Sprachtest** (Pseudo und Englisch);
  Python-Tests für Klang, Sprache, Paket und Spieltest.
- **Review M13** über Kern, Brücke und Godot, Daten, Werkzeuge und Doku (Schritt b7, Abschnitt 13 im
  Vertrag): die ganze CTest-Suite (45 Tests ohne den Zeittest, darunter alle Labore 1848, die
  Speichertests 1848 und 1866 und die KI-Labore je Stufe) unter ASan/UBSan ohne Befund; Durchspieltest über
  vier Staaten, Leistung 1848, Lasttest und Bildzeiten neu gemessen. Ergänzt ist die Beschreibung des
  Rauchtests (Bereiche, Vermächtnis, Rückmeldung) in `game/tests/README.md`.

**Abnahme im Einzelnen**

| Kriterium | Stand |
|---|---|
| Eine Partie 1848–1871 lässt sich vollständig ohne Fehler spielen | erfüllt: Durchspieltest über die Oberfläche als Preußen, Österreich, Bayern und Sachsen bis zum Urteil am 1. Juli 1871 (je rund 62 s, 24–42 beantwortete Ereignisse, 11–13 Wahlabende, 276 geöffnete Fenster, Speichern und Laden alle fünf Jahre mit gleicher Prüfsumme), ohne Skriptfehler; Linux-Paket mit Paketrauchtest beider Szenarien gleich `uhrwerk-cli`. Die Partie „von Hand in jeder Paketfassung“ steht mit dem Spieltest aus |
| Spieltest mit externen Testern; Rückmeldungen dokumentiert | **offen** (Entscheidung 18): vorbereitet sind Leitfaden, Fragebogen, Rückmeldefunktion mit Messwerten, Auswertung und Ergebnisvorlage ([pruefung/spieltest_m13_ergebnisse.md](pruefung/spieltest_m13_ergebnisse.md)); durchführen müssen ihn mindestens fünf Menschen ohne Projektwissen |
| Leistungsziele aus M3 und M4 gelten auch mit allen Systemen | erfüllt für den Kern: Spieljahr 1848 im Median 1,2 s auf vier Kernen (Grenze 2 s, `uhrwerk-cli leistung`), Lasttest mit 500.000 Blöcken im Median 0,63 s je Spieljahr (Grenze 3 s, gleiche Prüfsummen mit einem Thread); Bildzeiten mit llvmpipe als Wache (`leistung_test.gd`): 1 bis 10 % über dem Stand aus M10 (Median 32–62 ms je Bild, Grenze der Wache 10 %). **Offen:** 60 Bilder/s auf einem Mittelklasse-Rechner mit Grafikkarte, gemessen im Spieltest (die Rückmeldung misst sie) |

**Bekannte Grenzen**
- **Spieltest offen:** Ohne ihn ist der Meilenstein nur technisch fertig; Befunde aus dem Test kommen als
  eigener Schritt nach.
- **CI auf GitHub:** Seit dem 22. Oktober 2026 (M9) bekommen die Läufe keinen Runner mehr und brechen
  nach wenigen Sekunden ab: Das Freikontingent an Actions-Minuten des privaten Repositorys ist
  aufgebraucht (nicht der Code). Alle Prüfungen von M9 bis M13 liefen lokal unter Linux; der Kern unter
  MSVC und Apple Clang und die Determinismusprüfung über vier Plattformen sind bisher **nicht gelaufen**.
  Seit dem 18.11.2026 ist das Repository öffentlich und die CI sparsamer (schneller Umfang je Pull
  Request, voller Umfang zweimal im Monat und von Hand, siehe `ci.yml`). Am selben Tag lief
  `pakete.yml` zum ersten Mal: Pakete für Linux, Windows und macOS gebaut, der Paketrauchtest
  (beide Szenarien, 30 Tage, Prüfsumme gleich `uhrwerk-cli`) auf allen drei Systemen grün.
- **Bildrate auf echter Hardware** nicht gemessen (die Arbeitsumgebung hat keine Grafikkarte).
- **Englisch** nur für Startbildschirm, Spielmenü, Einstellungen, Zahlen, Daten und Gründe der KI (179
  von 1.304 Texten); Ereignisse, Journale, Lexikon und Chronik sind deutsch.
- **Präsentation in Grundfassung:** keine Porträts und Ereignisbilder (Entscheidung 12), sieben
  Musikstücke aus dem eigenen Synthesizer statt Aufnahmen.
- **Auslieferung ohne Signatur:** Windows nicht signiert, macOS ad hoc signiert und nicht notarisiert;
  keine automatischen Updates.
- **Fachprüfung offen:** Vermächtnisformeln, Vorlagen der Bünde, Wahltermine und Zeitungstexte sind
  Spielwerte bzw. ungeprüft wie die Daten aus M7–M12.
- **Alte Spielstände:** Spielstände aus M12 laden nicht (Format 7).
