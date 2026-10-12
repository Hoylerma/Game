# Technische Dokumentation

Dieser Ordner beschreibt die **Umsetzung** von EINIGKEIT. Das Design dahinter steht in
[02 Engine & Technik](../02_engine_und_technik.md); hier steht, was im Repository tatsächlich gebaut
ist (Stand: Meilenstein M3, Karte des Kernraums).

| Dokument | Inhalt | verbindlich für |
|---|---|---|
| **README.md** (diese Datei) | Überblick: Schichten, Tick-Ablauf, Determinismus, Festkomma, Zufallsströme, Prüfungen | alle |
| [chronik-skript.md](chronik-skript.md) | Syntax des Chronik-Skripts, Datenordner, Dateischemata (mit Orten ohne Daten und Kreisen), Startzustand, Formeln der Simulation, Kartenformat (Fassung M3) | Parser, Lader, `data/`, Archivar, Geodaten, Kartengenerator |
| [tabellen.md](tabellen.md) | Tabellen der Datenpipeline (`data/tabellen/*.csv`), Sicherheit der Angaben, Prüfungen des Archivars | Archivar, `data/` |
| [bridge-api.md](bridge-api.md) | GDScript-Schnittstelle der GDExtension (`UhrwerkSimulation`), Kartenmodi, Bibliotheksnamen | Brücke, Godot-Projekt |
| [bevoelkerung.md](bevoelkerung.md) | Bevölkerungsmodell seit M4 (Altersaufbau, Lebensstandard, Bildung, Kultur, Wanderung), Formeln, Mehrkernberechnung, Lasttest, Abnahme mit Messwerten | Kern, `data/`, Archivar |
| [wirtschaft.md](wirtschaft.md) | **M5 (verbindlich):** Güter, Betriebe, Märkte und Handel, Verbrauch nach Warenkorb, Steuerarten, Staatshaushalt mit Kredit und Bankrott, Balancing-Bericht, Abnahme | Kern, `data/`, Archivar |
| [speicherstaende.md](speicherstaende.md) | **Verbindlich seit M6:** Spielstände (Zustandsverzeichnis für Prüfsumme, Speichern, Laden und Textexport; Format mit zstd; Inhaltsfingerabdruck; Prüfungen beim Laden; Versionen), Befehlsprotokoll und Wiederholung, Kommandozeile, Spielmenü in Godot, Abnahme mit Messwerten | Kern, Kommandozeile, Brücke, Godot-Projekt |

Die öffentlichen Header in [`core/include/uhrwerk/`](../../core/include/uhrwerk/) sind die
verbindliche Programmierschnittstelle des Kerns. Jede Datei beginnt mit einem Kommentar, der ihre
Regeln erklärt.

## 1. Schichten

```
┌────────────────────────────────────────────────────────────────────────┐
│  PRÄSENTATION   game/            Godot 4.4 (GDScript, Shader, Szenen)  │
│  Karte, Kartenmodi, Leisten und Panels, Chronik, Screenshots           │
└───────────────▲───────────────────────────────────────┬────────────────┘
                │ liest: Dictionaries, Farben, float    │ Befehle: int
┌───────────────┴───────────────────────────────────────▼────────────────┐
│  BRÜCKE         bridge/          GDExtension „uhrwerk_bridge“ (C++)    │
│  Klasse UhrwerkSimulation: übersetzt Godot-Typen ↔ Kern-Typen          │
└───────────────▲───────────────────────────────────────┬────────────────┘
                │ const World&, LocationEconomy         │ Command
┌───────────────┴───────────────────────────────────────▼────────────────┐
│  UHRWERK        core/            Simulationskern (C++20, headless)     │
│  World · Simulation (Tick-Planer, Systeme) · Fixed · Pcg32 · Date      │
└───────────────▲────────────────────────────────────────────────────────┘
                │ load_world()
┌───────────────┴────────────────────────────────────────────────────────┐
│  DATEN          data/            Chronik-Skript (*.chron), Karte       │
│  common/ · szenarien/ · history/{staaten,kreise,orte}/ · karte/        │
└────────────────────────────────────────────────────────────────────────┘

  Weitere Nutzer des Kerns:   cli/   uhrwerk-cli (pruefen, info, lauf, determinismus, lasttest)
                              tests/ Unit- und Integrationstests (doctest)
  Werkzeuge für die Daten:    tools/archivar/         erzeugt data/history/ aus data/tabellen/
                              tools/geodaten/         Quellen, Geometrie (karte/geometrie/),
                                                      Orte und Kreise (tabellen/orte.csv, kreise.csv)
                              tools/kartengenerator/  erzeugt data/karte/provinzen.png,
                                                      fluesse.png und definition.csv
```

**Abhängigkeiten zeigen nur nach unten.**

| Schicht | darf verwenden | darf nicht |
|---|---|---|
| `core/` | C++-Standardbibliothek | Godot, Betriebssystem-APIs (außer Dateizugriff und `UHRWERK_DATEN`), Gleitkomma in der Simulation |
| `bridge/` | `core`, `third_party/godot-cpp` | Simulationslogik; Zustand des Kerns direkt ändern |
| `game/` | nur die Klasse `UhrwerkSimulation` (über `ClassDB`), Kartendateien aus `data/karte/` | selbst rechnen, was die Simulation betrifft |
| `cli/`, `tests/` | `core` | – |

Die Godot-Seite **liest** und schickt **Befehle**. Werte in die Simulation gehen als Ganzzahlen
(z. B. Steuersatz in Prozent), Werte aus der Simulation dürfen `float` sein, weil sie nur angezeigt
werden. Damit gelangt nie eine Gleitkommazahl in den Zustand des Kerns.

### Module des Kerns

| Header | Inhalt |
|---|---|
| [`fixed.hpp`](../../core/include/uhrwerk/fixed.hpp) | Festkommazahl `Fixed` (siehe Abschnitt 4) |
| [`rng.hpp`](../../core/include/uhrwerk/rng.hpp) | Zufallsgenerator `Pcg32` und Strom-Kennungen `RngStream` (Abschnitt 5) |
| [`date.hpp`](../../core/include/uhrwerk/date.hpp) | Kalender (proleptisch gregorianisch), Datumsformate |
| [`script.hpp`](../../core/include/uhrwerk/script.hpp) | Parser des Chronik-Skripts → `ScriptNode`-Baum, Fehler mit Datei, Zeile, Spalte |
| [`loader.hpp`](../../core/include/uhrwerk/loader.hpp) | `load_world()`: Datenordner → `World`, fatale Fehler und Warnungen |
| [`world.hpp`](../../core/include/uhrwerk/world.hpp) | Weltmodell: flache Vektoren, Querverweise als Indizes |
| [`simulation.hpp`](../../core/include/uhrwerk/simulation.hpp) | `Simulation`: Tick-Planer, Systeme, Befehle, Prüfsumme |
| [`economy.hpp`](../../core/include/uhrwerk/economy.hpp) | Wirtschaft: Märkte, Handel, Ortsrechnung (Erzeugung, Einkommen, Verbrauch, Steuern), Staatshaushalt (Kredit, Bankrott, Landesausbau), Ernte, Einschwingen (seit M5) |
| [`balance.hpp`](../../core/include/uhrwerk/balance.hpp) | Balancing-Bericht: Zielwerte lesen, Läufe messen, Median und Quantile, Verstöße (seit M5; nicht Teil der Simulation) |
| [`population.hpp`](../../core/include/uhrwerk/population.hpp) | Bausteine des Bevölkerungsmodells: Kennzahlen je Ort, Bildungsziel, Zusammenlegen und Neuordnung der Blöcke, Startwerte (seit M4) |
| [`parallel.hpp`](../../core/include/uhrwerk/parallel.hpp) | Thread-Pool `ThreadPool` für die Mehrkernberechnung (seit M4) |
| [`lasttest.hpp`](../../core/include/uhrwerk/lasttest.hpp) | `make_load_test_world()`: Lasttest-Welt mit Daten für jeden Ort (seit M4) |

### Weltmodell

`World` hält alle Objekte in **flachen Vektoren** (`terrains`, `confessions`, `cultures`,
`professions`, `states`, `districts`, `locations`, `pops`, `chronicle`). Querverweise sind **Indizes**
(`Index` = `int32`, `kNone` = −1), keine Zeiger. Die Reihenfolge jedes Vektors ergibt sich aus der Ladereihenfolge
([chronik-skript.md, Abschnitt 2](chronik-skript.md#2-datenordner)): Dateien nach Namen sortiert,
Einträge in Dateireihenfolge. Die Bevölkerungsblöcke eines Ortes liegen zusammenhängend
(`first_pop`, `pop_count`). Seit M4 trägt ein Block Beruf, Konfession, Kultur, vier Altersgruppen,
Lebensstandard und Alphabetisierung (64 Byte); dazu kommen je Ort Zugänge (`arrivals`),
Momentaufnahmen, Jahresbilanzen und Ortssteuern
([bevoelkerung.md, Abschnitt 2](bevoelkerung.md#2-datenmodell)).

**Orte ohne Daten** (seit M3, [chronik-skript.md, Abschnitt 3.6](chronik-skript.md#36-historyortechron)):
Ein Ort, der zum Startdatum keines der vier Simulationsfelder (`besitzer`, `bevoelkerung`,
`konfession`, `verwuestung`) hat, steht nur auf der Karte: Besitzer `kNone`, keine
Bevölkerungsblöcke (`Location::has_data()` ist `false`), Verwüstung 0. Hat ein Ort nur einen Teil
der Felder, ist das ein Ladefehler; Hauptstädte müssen Orte mit Daten sein. So lässt sich die
Karte des ganzen Kernraums (4.542 Orte) laden, obwohl erst 60 Orte historische Daten haben.

**Kreise** (seit M3, [Abschnitt 3.9](chronik-skript.md#39-historykreisechron-seit-m3)):
`World::districts` (`District { id, name, source }`) aus `history/kreise/`, Verweis
`Location::district` (`kNone` ohne Kreis). Ein Kreis bündelt 4–12 benachbarte Orte; in M3 dient er
nur der Kartendarstellung (Grenzen, Namen, Kartenmodus „Kreise“), ist nicht datiert und ändert
sich während einer Partie nicht.

Der **Kartenindex** (Pixelwert in `provinzen.png`) ist vom **Simulationsindex** unabhängig. Die
Godot-Seite verbindet beide über die Ort-Kennung (`definition.csv` → `get_location_index`).

## 2. Tick-Ablauf

1 Tick = 1 Tag. `Simulation::step_day()` arbeitet jeden Tag dieselben Schritte in fester
Reihenfolge ab:

| Schritt | Was | Wann |
|---|---|---|
| 1 | **Befehle** der Warteschlange ausführen, in Eingangsreihenfolge; ungültige werden verworfen. Geänderte Steuersätze kommen einmal je Staat in die Chronik | jeden Tag |
| 2 | **Jahresabschluss**: Bilanz des Jahres wird Bilanz des Vorjahres (seit M4); **Ernte**: Erntefaktor je Markt (Zufallsstrom `Wirtschaft`, seit M5) | am 1. Januar |
| 3 | **Neuordnung** der Blöcke: Zugänge werden Blöcke, zu kleine Blöcke aufgelöst, nur wenn nötig (seit M4); dann die **Märkte**: Summen aus den Orten, Handel, Preise (seit M5) | am Monatsersten |
| 4 | **Wirtschaft und Bevölkerung** eines Ortes: Erzeugung, Einkommen, Verbrauch, Steuern (seit M5), dann Lebensstandard, Geburten, Sterbefälle, Altern, Bildung, Wanderung – **parallel** über die Orte des Tages (seit M4) | Ort *i* am Monatstag `(i % 28) + 1` |
| 5 | **Zusammenführen**: Momentaufnahmen veröffentlichen, Wanderung je Zielort buchen (parallel über Zielorte) | nach Schritt 4 |
| 6 | **Haushalt** eines Staates: Summe der Ortssteuern je Art, Hof, Verwaltung, Heer, Zinsen, Bauten (Landesausbau), Kredit und Tilgung, Staatsbankrott | Staat *j* am Monatstag `(j % 28) + 1` |
| 7 | **Jahreswechsel**: Seuchen (Zufallsstrom `Ereignisse`), Abbau der Verwüstung, Orte aufsteigend | am 1. Januar |
| 8 | Datum um einen Tag weiterschalten | jeden Tag |

Die monatlichen Systeme sind **gestaffelt**: Jeder Ort und jeder Staat hat einen festen Rechentag
zwischen dem 1. und 28. Das verteilt die Last gleichmäßig über den Monat und funktioniert in
jedem Monat (auch im Februar). Weil der Rechentag am Index hängt, verschieben neu eingefügte Orte
die Rechentage der folgenden Orte (siehe Prüfsumme in Abschnitt 3).

**Orte ohne Daten** haben keine Bevölkerungsblöcke und tragen deshalb nichts zu Bevölkerung und
Wirtschaft bei, sind keine Wanderungsziele, und beim Jahreswechsel werden sie übersprungen und
ziehen keine Zufallszahlen. Damit ändern angehängte Orte ohne Daten die Ergebnisse der übrigen Orte
nicht (Unit-Test).

Die Formeln stehen in [bevoelkerung.md, Abschnitt 4](bevoelkerung.md#4-formeln) (Bevölkerung,
Ortssteuern) und [chronik-skript.md, Abschnitt 5](chronik-skript.md#5-formeln-der-simulation)
(Staatshaushalt, Jahreswechsel).

**Mehrkernberechnung (seit M4):** Die Ortsrechnungen eines Tages laufen in Paketen zu 8 Orten auf
allen Kernen (`SimConfig::threads`, 0 = alle). Jede schreibt nur Daten ihres Ortes und liest andere
Orte nur über deren veröffentlichte Momentaufnahme; das Buchen der Wanderung läuft je Zielort in
fester Reihenfolge. Das Ergebnis ist daher bei jeder Thread-Zahl bitgleich
([bevoelkerung.md, Abschnitt 5](bevoelkerung.md#5-mehrkernberechnung)).

Ein Befehl, der über `submit()` (bzw. `set_tax_rate_percent` der Brücke) eingereiht wird, wirkt
**ab dem nächsten simulierten Tag**. Die Oberfläche zeigt bis dahin „gilt ab dem nächsten Tag“.

In Godot ruft `main.gd` je Bild `advance_days(n)` mit den fälligen ganzen Tagen auf (höchstens 30
je Bild) und liest danach Datum, Staaten, Orte, Kartenfarben und neue Chronikeinträge.

## 3. Determinismus

Gleicher Datenstand + gleicher Seed + gleiche Befehle ⇒ **bitgleicher Zustand** – auf jeder
Plattform, mit jedem Compiler, in jeder Optimierungsstufe. Das ist die Voraussetzung für
Lockstep-Multiplayer, Replays und reproduzierbare Tests.

**Regeln für den Kern** (auch in [CONTRIBUTING.md](../../CONTRIBUTING.md#code-stil)):

1. **Keine Gleitkommazahlen** in der Simulation. Alle Größen sind `Fixed`; `to_double()` ist nur
   für Anzeige und Werkzeuge da.
2. **Keine unkontrollierten Zufallsquellen:** kein `std::rand`, kein `std::random_device`, keine
   `<random>`-Verteilungen (deren Ergebnisse je Standardbibliothek verschieden sind). Zufall nur
   über `Pcg32`-Ströme der Simulation.
3. **Keine Zeitstempel** und keine Abhängigkeit von Uhrzeit, Umgebung oder Speicheradressen.
4. **Feste Reihenfolge:** Iteration nur über geordnete Container (Vektoren nach Index); keine
   Iteration über `unordered_*`, deren Reihenfolge sich auf den Zustand auswirkt. Summen werden in
   der vorgeschriebenen Reihenfolge gebildet (Orte aufsteigend, Blöcke aufsteigend).
5. **Reihenfolge der Rechenoperationen** wie in der Spezifikation: Festkomma rundet bei jeder
   Operation, `a * b / c` ist nicht dasselbe wie `a * (b / c)`.
6. **Zustand ändert sich nur durch `step_day()`** und eingereihte Befehle. Abfragen wie
   `location_economy()` rechnen, ohne etwas zu ändern.

**Prüfsumme:** `Simulation::checksum()` bildet FNV-1a (64 Bit) über den gesamten veränderlichen
Zustand: Datum, vergangene Tage, Seed, Staatskassen, Steuersätze, Verwüstung, Bevölkerung (seit
M4 mit Altersgruppen, Kultur, Lebensstandard, Alphabetisierung und den Zugängen), Momentaufnahmen,
Bilanzen und Ortssteuern, die Zustände aller Zufallsströme, die Chronik und die noch wartenden
Befehle. Die Zahl der Threads gehört nicht dazu. Ganzzahlen gehen
byteweise in Little-Endian-Reihenfolge ein, daher ist die Prüfsumme plattformunabhängig.

Unveränderlicher Inhalt – Namen, Lage, Gelände, Fläche und **bewusst auch die Kreise** – gehört
nicht zur Prüfsumme: Neue oder anders gebildete Kreise in den Daten ändern die Referenzprüfsumme
nicht. Orte ohne Daten gehen über Besitzer (`kNone`) und Blockzahl (0) ein. Kommen Orte hinzu,
ändert sich die Referenz trotzdem, weil sich die Zahl der Orte und die Rechentage nachfolgender
Orte verschieben. Referenz nach 100 Jahren mit `data/` (Seed 1648, jede Thread-Zahl): seit M5
`5db13a66d63e6079` (Wirtschaft mit Märkten und Staatshaushalt), in M4 `ed6b14a02cfbfa5d` (Bevölkerungsmodell), in M3 `40de8c42e8f7a0ff` (erster M3-Stand mit 4.541 Orten
`5548a5fa80f75c0d`), vorher `8d44cf792e143bb0` (Begründung in
[data/README.md](../../data/README.md#prüfung)).

**Geprüft wird das an drei Stellen:**

- Unit-Tests in [`tests/test_simulation.cpp`](../../tests/test_simulation.cpp): gleiche Läufe →
  gleiche Prüfsumme; jede Zustandsänderung → andere Prüfsumme.
- `uhrwerk-cli determinismus --jahre N`: zwei Läufe mit gleichem Seed (Vergleich nach jedem Jahr,
  seit M4 der zweite mit einem Thread) und ein Lauf mit Seed + 1, der abweichen muss. Läuft als
  CTest `determinismus_50_jahre`.
- Seit M4: Unit-Test mit 1, 2, 3, 4 und 8 Threads und `uhrwerk-cli lasttest --vergleich T`
  (CTest `lasttest_threads`) – die Zahl der Threads ändert die Prüfsumme nicht.
- Die CI vergleicht die Prüfsumme nach 100 Jahren und die des Lasttests (50.000 Blöcke, 2 Jahre)
  zwischen **GCC, Clang (Linux) und MSVC (Windows)**; alle drei müssen übereinstimmen.

## 4. Festkomma (`Fixed`)

| Eigenschaft | Wert |
|---|---|
| Format | Q47.16 in einem `int64_t`: 16 Nachkommabits |
| Auflösung | 1/65536 ≈ 0,0000153 |
| Wertebereich | etwa ±1,4 · 10¹⁴ |
| Multiplikation | exaktes 128-Bit-Zwischenprodukt, dann **abrunden** (Richtung −∞) |
| Division | exakter Quotient, **abschneiden** (Richtung 0); Divisor 0 ergibt 0 |
| `from_ratio`, `parse` | **kaufmännisch** (Hälfte weg von null) |
| `round()` | kaufmännisch auf eine ganze Zahl, `trunc()` schneidet ab |
| Text | `to_string(n)` deterministisch mit Punkt, z. B. `"12.50"` |

Die 128-Bit-Zwischenwerte rechnet `fixed.hpp` mit zwei 64-Bit-Hälften (`detail::mul_u64`,
`detail::div_u128_by_u64`), **ohne `__int128` und ohne Compiler-Builtins**. Seit M4 gibt es
bitgleiche Schnellwege: Multiplikation mit einem 64-Bit-Produkt, wenn beide Beträge unter 2³²
liegen; Division nativ in 64 Bit, wenn der Zähler hineinpasst, sonst nach Knuth (Algorithmus D,
32-Bit-Ziffern); nur wenn der Quotient nicht in 64 Bit passt, die bitweise Schulmethode. Damit verhält sich
der Code unter GCC, Clang und MSVC gleich. Die Tests vergleichen die Ergebnisse dort, wo der
Compiler `__int128` kennt, mit 200.000 Zufallsstichproben gegen echte 128-Bit-Arithmetik.

Zahlen aus den Daten (`0.10`, `-2.5`) liest `Fixed::parse` direkt aus dem Text, ohne Umweg über
`double`.

## 5. Zufallsströme

Der Generator ist **PCG32** (XSH-RR, 64 Bit Zustand, 32 Bit Ausgabe). Jedes System bekommt einen
eigenen Strom, abgeleitet aus dem Partie-Seed (`SimConfig::seed`, Standard 1648) und einer festen
Strom-Kennung:

| `RngStream` | Wert | Verwendung |
|---|---|---|
| `Demografie` | 1 | angelegt, noch nicht gezogen |
| `Wirtschaft` | 2 | Erntefaktor je Markt und Jahr (seit M5) |
| `Ereignisse` | 3 | Seuchen am Jahreswechsel (`chance`, `uniform`) |
| `Ki` | 4 | reserviert |
| `Lasttest` | 5 | Aufbau der Lasttest-Welt (seit M4), nicht in der Simulation |
| `Testbefehle` | 6 | Testbefehle der Kommandozeile (`--testbefehle`, seit M6), nicht in der Simulation |

- Neue Inhalte in einem System verändern die Zufallsfolge der anderen Systeme **nicht**.
- Die Werte der Kennungen ändern sich **nie**; neue Ströme werden hinten angefügt. Sonst wären
  Spielstände und Referenz-Prüfsummen ungültig.
- `below(n)` ist frei von Modulo-Verzerrung, `unit()` liefert `Fixed` in [0, 1) aus den oberen
  16 Bit, `uniform(lo, hi)` und `chance(p)` bauen darauf auf – alles ohne Gleitkomma.
- Die Zustände aller Ströme gehören zur Prüfsumme und zum Spielstand (seit M6).

## 6. Daten und Karte

- **Chronik-Skript** (`*.chron`): eine kleine, fehlertolerante Sprache für alle Inhalte. Der
  Parser meldet deutsche Fehler mit `datei:zeile:spalte` und setzt danach fort. Unbekannte Felder
  sind Warnungen, fehlende Pflichtfelder und unbekannte Verweise sind fatal.
  `uhrwerk-cli pruefen` zeigt alle Probleme eines Datenordners.
- **Datierte Historie und Tabellen (M2):** Orte und Staaten tragen datierte Blöcke
  `JJJJ.MM.TT = { … }`; der Lader wendet alle bis zum Startdatum des Szenarios an
  ([chronik-skript.md, Abschnitt 3.8](chronik-skript.md#38-datierte-blöcke-seit-m2)). So speisen
  dieselben Daten mehrere Lesezeichen. Gepflegt werden sie in `data/tabellen/*.csv`;
  `tools/archivar/archivar.py` prüft Verweise, Lücken zu den Szenariodaten und Ausreißer und
  erzeugt `data/history/` ([tabellen.md](tabellen.md)). Die CI prüft mit `--pruefen`, dass beides
  zusammenpasst.
- **Geodaten (seit M3):** `tools/geodaten/geodaten.py` lädt die offenen Quellen – Natural Earth
  1:10 Mio. (gemeinfrei), GeoNames `cities1000` (CC BY 4.0) und AWS Terrain Tiles (Höhen) – nur in
  den Zwischenspeicher `build/geodaten-cache/` und prüft sie gegen die SHA-256-Werte in
  `tools/geodaten/quellen.json`. Daraus entstehen `data/karte/geometrie/land.geojson`,
  `seen.geojson` und `fluesse.geojson` (zugeschnitten, vereinfacht, deterministisch);
  `kernraum.geojson` ist von Hand gezeichnet. Quellenangaben:
  [Anhang C](../anhang/c_quellen_und_daten.md#5-verwendete-geodaten-stand-m3).
- **Orte und Kreise des Kernraums (seit M3):** `tools/geodaten/orte.py` wählt aus GeoNames die
  Orte des Kernraums, vergibt Namen (deutsche Namen nur mit Beleg), Kennungen, Gelände (aus dem
  Höhenmodell) und Kreise (räumlich gebildet, 4–12 benachbarte Orte) und schreibt
  `data/tabellen/orte.csv` und `kreise.csv`; die 60 Orte der Testregion bleiben dabei unverändert
  vorn. Die Fläche jedes Ortes übernimmt `orte.py` aus der Karte. Alle neuen Orte sind Orte ohne
  Daten. Reihenfolge der Erzeugung: `geodaten.py geometrie` → `orte.py` → `archivar.py` →
  `kartengenerator.py` ([tools/geodaten/README.md](../../tools/geodaten/README.md#orte-des-kernraums-ortepy)).
- **Karte v2 (seit M3,** [chronik-skript.md, Abschnitt 6](chronik-skript.md#6-karte-fassung-m3)**):**
  `tools/kartengenerator/kartengenerator.py` (Python mit numpy) erzeugt aus `karte.chron`, der
  Geometrie und den Lagen der Orte drei Dateien:
  - `provinzen.png` (4096 × 3329 Pixel, Plattkarte mit Bezugsbreite 51 °N, etwa 0,37 km je
    Pixel): `R + G × 256` = Kartenindex + 1, der Blaukanal die Flächenart (0 Kernraum-Land,
    1 Meer, 2 Binnengewässer, 3 Ausland). Land und Meer sind damit ins Bild eingebacken; die
    Küstenlinie aus M1 entfällt.
  - `fluesse.png`: Deckung durch Flüsse in Graustufen (nur Darstellung).
  - `definition.csv`: Kartenindex, Ort, Name, Beschriftungspunkt, Pixelzahl, Fläche in km².

  Jedes Kernraum-Land-Pixel gehört dem nächsten Ort (mit deterministischem Rauschen für natürliche
  Grenzen, höchstens `max_radius_km`), jeder Ort hat genau eine zusammenhängende Fläche. Im
  Pixelpfad stehen nur Grundrechenarten, daher sind die Bilder auf allen Plattformen pixelgleich.
  Alle drei Dateien sind eingecheckt; die CI prüft mit `--pruefen`, dass sie zum Datenstand
  passen. Die Fläche der Orte geht nicht in die Zuordnung ein, sondern wird aus der Karte
  berechnet. Mit den Daten von M3 sind 99,96 % des Kernraum-Landes Orten zugeordnet; 41 kleine
  Teilflächen (meist Inseln ohne GeoNames-Ort) gehören zu keinem Ort.
- **Godot** lädt `provinzen.png` als Bild (für Klicks) und als Textur. Der Shader holt je Pixel
  Farbe, Besitzer und Kreis des Ortes aus zwei Nachschlagetexturen mit 256 Spalten (eine Zeile
  wäre für 4.542 Orte zu breit), die `get_map_colors(modus)`, `get_location_owner_indices()` und
  `get_location_kreis_indices()` füllen. Meer, Binnengewässer und Ausland haben eigene Farben,
  Orte ohne Daten (Alpha 0 in den datenabhängigen Kartenmodi) eine Schraffur „Keine Daten“.
  Staats-, Kreis- und Ortsgrenzen entstehen im Shader durch Nachbarvergleich und werden je nach
  Zoomstufe eingeblendet; die Namen (Staaten fern, Kreise mittel, Orte nah) zeichnet GDScript im
  Bildschirmraum. Einzelheiten und Leistungsmessung: [game/README.md](../../game/README.md#karte).

## 7. Bauen und Prüfen

| Preset | Zweck | Ausgabe |
|---|---|---|
| `dev` | Debug, Warnungen als Fehler; Kern, CLI, Tests | `build/dev/` |
| `release` | optimierter Build von Kern, CLI und Tests | `build/release/` |
| `bridge` | GDExtension samt godot-cpp (`RelWithDebInfo`, `template_debug`), ohne Tests | `build/bridge/`, Bibliothek in `game/bin/` |

Befehle und Beispiele: [README.md, Abschnitt „Entwicklung“](../../README.md#entwicklung).

Die CI ([`.github/workflows/ci.yml`](../../.github/workflows/ci.yml)) prüft jeden Push und
jeden Pull Request:

| Job | Prüft |
|---|---|
| Kern (Linux, gcc/clang) | Build mit `-Werror`, alle Tests (doctest, `daten_pruefen`, `determinismus_50_jahre`, `lasttest_threads`), Prüfsummen nach 100 Jahren und des Lasttests |
| Kern (Windows, MSVC) | dasselbe mit MSVC (`/W4 /WX`) – Portabilität |
| Determinismus (GCC, Clang, MSVC) | die Prüfsummen (100 Jahre und Lasttest) sind auf allen drei gleich |
| Lasttest (Linux, Release) | 500.000 Blöcke, 3 Spieljahre, Median höchstens 3 s (Puffer für schwankende Runner), gleiche Prüfsummen mit einem Thread; Messwerte in der Zusammenfassung des Laufs ([bevoelkerung.md, Abschnitt 7](bevoelkerung.md#7-lasttest)) |
| Formatierung | `clang-format-18 --dry-run --Werror` über `core/`, `cli/`, `tests/`, `bridge/` |
| Python-Werkzeuge (Python 3.10 mit numpy 2.2.6, Python 3.13 mit numpy 2.5.3) | Tests von Archivar, Kartengenerator und Geodaten (ohne Netz); `data/history/` und `data/karte/` sind aktuell (`--pruefen`) |
| Godot | Brücke bauen, Projekt headless importieren, Einheitentest `unit_test.gd`, Rauchtest `smoke_test.gd`, Klicktest `klick_test.gd` (jeder Ort in drei Zoomstufen), alle Protokolle ohne Skriptfehler; zwei Screenshots der Hauptszene (Xvfb, OpenGL 3) als Artefakt |

Nur lokal laufen die Prüfungen, die die Rohdaten im Zwischenspeicher `build/geodaten-cache/`
brauchen: `python3 tools/geodaten/geodaten.py pruefen` (Geometrie passt zu den Quellen) und
`python3 tools/geodaten/orte.py --pruefen` (Orte und Kreise sind aktuell). Ebenfalls nur lokal
und nur mit Grafikausgabe: der Leistungstest `game/tests/leistung_test.gd`.
