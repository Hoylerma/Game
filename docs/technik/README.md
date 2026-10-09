# Technische Dokumentation

Dieser Ordner beschreibt die **Umsetzung** von EINIGKEIT. Das Design dahinter steht in
[02 Engine & Technik](../02_engine_und_technik.md); hier steht, was im Repository tatsächlich gebaut
ist (Stand: Meilenstein M1, technischer Durchstich).

| Dokument | Inhalt | verbindlich für |
|---|---|---|
| **README.md** (diese Datei) | Überblick: Schichten, Tick-Ablauf, Determinismus, Festkomma, Zufallsströme, Prüfungen | alle |
| [chronik-skript.md](chronik-skript.md) | Syntax des Chronik-Skripts, Datenordner, Dateischemata, Startzustand, Formeln der Simulation, Kartenformat | Parser, Lader, `data/`, Kartengenerator |
| [bridge-api.md](bridge-api.md) | GDScript-Schnittstelle der GDExtension (`UhrwerkSimulation`), Kartenmodi, Bibliotheksnamen | Brücke, Godot-Projekt |

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
│  common/ · history/staaten/ · history/orte/ · szenarien/ · karte/      │
└────────────────────────────────────────────────────────────────────────┘

  Weitere Nutzer des Kerns:   cli/   uhrwerk-cli (pruefen, info, lauf, determinismus)
                              tests/ Unit- und Integrationstests (doctest)
  Werkzeug für die Daten:     tools/kartengenerator/  erzeugt data/karte/provinzen.png
                                                      und definition.csv
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

### Weltmodell

`World` hält alle Objekte in **flachen Vektoren** (`terrains`, `confessions`, `professions`,
`states`, `locations`, `pops`, `chronicle`). Querverweise sind **Indizes** (`Index` = `int32`,
`kNone` = −1), keine Zeiger. Die Reihenfolge jedes Vektors ergibt sich aus der Ladereihenfolge
([chronik-skript.md, Abschnitt 2](chronik-skript.md#2-datenordner)): Dateien nach Namen sortiert,
Einträge in Dateireihenfolge. Die Bevölkerungsblöcke eines Ortes liegen zusammenhängend
(`first_pop`, `pop_count`).

Der **Kartenindex** (Pixelwert in `provinzen.png`) ist vom **Simulationsindex** unabhängig. Die
Godot-Seite verbindet beide über die Ort-Kennung (`definition.csv` → `get_location_index`).

## 2. Tick-Ablauf

1 Tick = 1 Tag. `Simulation::step_day()` arbeitet jeden Tag dieselben Schritte in fester
Reihenfolge ab:

| Schritt | Was | Wann |
|---|---|---|
| 1 | **Befehle** der Warteschlange ausführen, in Eingangsreihenfolge; ungültige werden verworfen. Geänderte Steuersätze kommen einmal je Staat in die Chronik | jeden Tag |
| 2 | **Demografie** eines Ortes: Wachstum je Bevölkerungsblock | Ort *i* am Monatstag `(i % 28) + 1` |
| 3 | **Wirtschaft** eines Staates: Steuern aller Blöcke, Hofhaltung, Staatskasse | Staat *j* am Monatstag `(j % 28) + 1` |
| 4 | **Jahreswechsel**: Seuchen (Zufallsstrom `Ereignisse`), Abbau der Verwüstung, Orte aufsteigend | am 1. Januar |
| 5 | Datum um einen Tag weiterschalten | jeden Tag |

Die monatlichen Systeme sind **gestaffelt**: Jeder Ort und jeder Staat hat einen festen Rechentag
zwischen dem 1. und 28. Das verteilt die Last gleichmäßig über den Monat und funktioniert in
jedem Monat (auch im Februar). Die Formeln stehen in
[chronik-skript.md, Abschnitt 5](chronik-skript.md#5-formeln-der-simulation-m1).

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
Zustand: Datum, vergangene Tage, Seed, Staatskassen, Steuersätze, Verwüstung, Bevölkerung, die
Zustände aller Zufallsströme, die Chronik und die noch wartenden Befehle. Ganzzahlen gehen
byteweise in Little-Endian-Reihenfolge ein, daher ist die Prüfsumme plattformunabhängig.

**Geprüft wird das an drei Stellen:**

- Unit-Tests in [`tests/test_simulation.cpp`](../../tests/test_simulation.cpp): gleiche Läufe →
  gleiche Prüfsumme; jede Zustandsänderung → andere Prüfsumme.
- `uhrwerk-cli determinismus --jahre N`: zwei Läufe mit gleichem Seed (Vergleich nach jedem Jahr)
  und ein Lauf mit Seed + 1, der abweichen muss. Läuft als CTest `determinismus_50_jahre`.
- Die CI vergleicht die Prüfsumme nach 100 Jahren zwischen **GCC, Clang (Linux) und MSVC
  (Windows)**; alle drei müssen übereinstimmen.

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
`detail::div_u128_by_u64`), **ohne `__int128` und ohne Compiler-Builtins**. Damit verhält sich
der Code unter GCC, Clang und MSVC gleich. Die Tests vergleichen die Ergebnisse dort, wo der
Compiler `__int128` kennt, mit 200.000 Zufallsstichproben gegen echte 128-Bit-Arithmetik.

Zahlen aus den Daten (`0.10`, `-2.5`) liest `Fixed::parse` direkt aus dem Text, ohne Umweg über
`double`.

## 5. Zufallsströme

Der Generator ist **PCG32** (XSH-RR, 64 Bit Zustand, 32 Bit Ausgabe). Jedes System bekommt einen
eigenen Strom, abgeleitet aus dem Partie-Seed (`SimConfig::seed`, Standard 1648) und einer festen
Strom-Kennung:

| `RngStream` | Wert | Verwendung in M1 |
|---|---|---|
| `Demografie` | 1 | angelegt, noch nicht gezogen |
| `Wirtschaft` | 2 | angelegt, noch nicht gezogen |
| `Ereignisse` | 3 | Seuchen am Jahreswechsel (`chance`, `uniform`) |
| `Ki` | 4 | reserviert |

- Neue Inhalte in einem System verändern die Zufallsfolge der anderen Systeme **nicht**.
- Die Werte der Kennungen ändern sich **nie**; neue Ströme werden hinten angefügt. Sonst wären
  Spielstände und Referenz-Prüfsummen ungültig.
- `below(n)` ist frei von Modulo-Verzerrung, `unit()` liefert `Fixed` in [0, 1) aus den oberen
  16 Bit, `uniform(lo, hi)` und `chance(p)` bauen darauf auf – alles ohne Gleitkomma.
- Die Zustände aller Ströme gehören zur Prüfsumme.

## 6. Daten und Karte

- **Chronik-Skript** (`*.chron`): eine kleine, fehlertolerante Sprache für alle Inhalte. Der
  Parser meldet deutsche Fehler mit `datei:zeile:spalte` und setzt danach fort. Unbekannte Felder
  sind Warnungen, fehlende Pflichtfelder und unbekannte Verweise sind fatal.
  `uhrwerk-cli pruefen` zeigt alle Probleme eines Datenordners.
- **Karte:** `tools/kartengenerator/kartengenerator.py` erzeugt aus `data/karte/karte.chron` und
  den Lagen der Orte das Ort-ID-Bild `data/karte/provinzen.png` (Pixelwert `R + G × 256` =
  Kartenindex + 1) und `data/karte/definition.csv`. Beide Dateien sind eingecheckt; die CI prüft
  mit `--pruefen`, dass sie zum Datenstand passen.
- **Godot** lädt `provinzen.png` als Bild (für Klicks) und als Textur. Ein Shader holt je Pixel die
  Farbe des Ortes aus einer Nachschlagetextur, die `get_map_colors(modus)` füllt; Grenzen entstehen
  im Shader durch Nachbarvergleich.

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
| Kern (Linux, gcc/clang) | Build mit `-Werror`, alle Tests (doctest, `daten_pruefen`, `determinismus_50_jahre`), Prüfsumme nach 100 Jahren |
| Kern (Windows, MSVC) | dasselbe mit MSVC (`/W4 /WX`) – Portabilität |
| Determinismus (GCC, Clang, MSVC) | die drei Prüfsummen sind gleich |
| Formatierung | `clang-format-18 --dry-run --Werror` über `core/`, `cli/`, `tests/`, `bridge/` |
| Kartengenerator (Python 3.10, 3.13) | Tests des Generators; `data/karte/` ist aktuell |
| Godot | Brücke bauen, Projekt headless importieren, Rauchtest `game/tests/smoke_test.gd`, Screenshot der Hauptszene (Xvfb, OpenGL 3) als Artefakt |
