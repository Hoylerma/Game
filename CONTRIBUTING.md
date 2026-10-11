# Mitarbeit an EINIGKEIT

Dieses Dokument beschreibt, wie im Repository gearbeitet wird: Struktur, Branches, Commits, Code-Stil und Daten.

## Repository-Struktur

| Ordner | Inhalt | Abhängigkeiten |
|---|---|---|
| `core/` | Simulationskern **Uhrwerk** (C++20) – engine-unabhängig, deterministisch, headless | nur C++-Standardbibliothek |
| `cli/` | `uhrwerk-cli`: Kommandozeile für Testläufe, Determinismusprüfung, Berichte | `core` |
| `tests/` | Unit- und Integrationstests des Kerns | `core`, doctest (wird von CMake geladen) |
| `bridge/` | GDExtension-Brücke zwischen Uhrwerk und Godot | `core`, `third_party/godot-cpp` |
| `game/` | Godot-4-Projekt (Szenen, Skripte, Shader, Oberfläche) | `bridge` |
| `data/` | Spielinhalte: Tabellen (`tabellen/*.csv`), daraus erzeugtes **Chronik-Skript** (`*.chron`), Karte (`karte/`) | – |
| `tools/` | Hilfswerkzeuge: Archivar (Datenpipeline), Geodaten (Quellen, Geometrie, Orte und Kreise des Kernraums), Kartengenerator | Python 3.10+; Geodaten und Kartengenerator mit numpy |
| `docs/` | Game Design Document, technische Dokumentation in `docs/technik/` | – |
| `third_party/` | Fremdbibliotheken als Git-Submodule | – |

Grundregel aus dem Design ([02 Engine](docs/02_engine_und_technik.md)): **`core/` kennt Godot nicht.** Alles, was die Simulation betrifft, gehört in den Kern; die Godot-Seite liest nur und schickt Befehle.

## Bauen

```bash
# Repository mit Submodulen klonen
git clone --recurse-submodules https://github.com/Hoylerma/Game.git
# oder nachträglich:
git submodule update --init --recursive

# Kern, CLI und Tests
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

# Zusätzlich die GDExtension für Godot (kopiert die Bibliothek nach game/bin/)
cmake --preset bridge
cmake --build --preset bridge
```

Danach `game/project.godot` mit **Godot 4.4.1** öffnen.

Die Python-Werkzeuge laufen ab Python 3.10. Der Archivar braucht nur die Standardbibliothek,
Kartengenerator und Geodaten zusätzlich **numpy** (geprüft mit numpy 2.2.6 unter Python 3.10 und
2.5.3 unter Python 3.13, wie in der CI):

```bash
python3 -m pip install numpy
```

Nützliche Befehle:

```bash
./build/dev/cli/uhrwerk-cli lauf --jahre 50 --bericht      # 50 Jahre ab 1648 simulieren
./build/dev/cli/uhrwerk-cli determinismus --jahre 100      # zwei Läufe, Prüfsummen vergleichen
python3 tools/archivar/archivar.py                         # data/history/ aus data/tabellen/ erzeugen
python3 tools/kartengenerator/kartengenerator.py           # Karte neu erzeugen (provinzen.png, fluesse.png, definition.csv)

# Tests der Python-Werkzeuge (ohne Netz, wie in der CI)
python3 -m unittest discover -s tools/archivar -v
python3 -m unittest discover -s tools/kartengenerator -v
python3 -m unittest discover -s tools/geodaten -v

# Tests der Godot-Seite (nach dem Import: godot --headless --path game --import)
godot --headless --path game --script res://tests/unit_test.gd
godot --headless --path game --script res://tests/smoke_test.gd
godot --headless --path game --script res://tests/klick_test.gd
```

## Branches

- **`main`** ist der stabile Stand. Direkte Pushes auf `main` sind nicht vorgesehen; Änderungen kommen per Pull Request.
- Arbeitsbranches werden nach Art der Änderung benannt:

| Präfix | Verwendung | Beispiel |
|---|---|---|
| `feature/` | Neue Funktionen | `feature/wahlsimulation` |
| `fix/` | Fehlerbehebungen | `fix/kalender-schaltjahr` |
| `content/` | Spielinhalte, Daten, Ereignisse | `content/staaten-1648-sueden` |
| `docs/` | Design-Dokument und Dokumentation | `docs/epoche-weimar` |
| `chore/` | Build, CI, Werkzeuge | `chore/ci-windows` |

## Commits

Commit-Nachrichten folgen [Conventional Commits](https://www.conventionalcommits.org/) mit einem **Bereich**:

```
<typ>(<bereich>): <kurze Beschreibung im Imperativ>

<optional: was und warum, umgebrochen bei 72 Zeichen>
```

| Typ | Bedeutung |
|---|---|
| `feat` | Neue Funktion |
| `fix` | Fehlerbehebung |
| `content` | Spielinhalte und Daten |
| `docs` | Dokumentation |
| `test` | Tests |
| `refactor` | Umbau ohne Verhaltensänderung |
| `perf` | Leistung |
| `chore` | Build, CI, Werkzeuge, Abhängigkeiten |

Bereiche: `core`, `cli`, `bridge`, `game`, `data`, `tools`, `docs`, `ci`, `repo`.

Beispiele: `feat(core): Demografiesystem mit gestaffeltem Monatspuls` · `content(data): Kursachsen 1648 ergänzt` · `fix(bridge): Farbtabelle bei leerer Karte`

Ein Commit sollte **eine** logische Änderung enthalten und für sich bauen und testen.

## Pull Requests

- Kleine, fokussierte PRs; die Vorlage füllt sich fast von selbst.
- Die CI muss grün sein: Build mit GCC, Clang und MSVC, alle Tests, Determinismusprüfung, Formatprüfung, Tests und Aktualitätsprüfung der Python-Werkzeuge (Python 3.10 und 3.13), Godot-Einheiten-, Rauch- und Klicktest.
- **Determinismus ist Pflicht:** Keine Gleitkommazahlen und keine unkontrollierten Zufallsquellen im Kern (siehe unten).

## Code-Stil

- **C++:** C++20, Formatierung mit `clang-format` (Konfiguration im Repository). Prüfen: `clang-format-18 --dry-run --Werror $(git ls-files '*.cpp' '*.hpp' '*.h')` (Version 18 wie in der CI)
- **Bezeichner** im Code auf Englisch (Typen, Funktionen), **Spielbegriffe** dürfen deutsch sein (`Steuersatz`, `Ort`), wo sie Fachbegriffe des Designs sind. Kommentare und Fehlermeldungen für Spieler auf Deutsch.
- **GDScript:** Godot-Stil (Tabs, `snake_case`), statische Typen verwenden.
- **Python (`tools/`):** ab Python 3.10 lauffähig; Bezeichner und Meldungen deutsch wie im
  vorhandenen Code. Außer der Standardbibliothek nur numpy (Kartengenerator, Geodaten). Erzeugte
  Dateien müssen deterministisch sein: gleiche Eingaben ergeben unter Python 3.10 und 3.13
  byte- bzw. pixelgleiche Ausgaben (keine Zeitstempel, feste Reihenfolgen, im Pixelpfad des
  Kartengenerators nur Grundrechenarten). Jedes Werkzeug hat `unittest`-Tests ohne Netz.
- **Im Kern verboten:** `float`/`double` in der Simulation (nur für Anzeige), `std::rand`, `std::random_device`, Zeitstempel, Iteration über ungeordnete Container mit Auswirkung auf den Zustand.

## Daten und historische Inhalte

- Inhalte liegen in `data/` im Chronik-Skript. **Orte und Staaten samt Geschichte** werden in `data/tabellen/*.csv` gepflegt; `python3 tools/archivar/archivar.py` erzeugt daraus `data/history/` (nicht von Hand bearbeiten). Format: [docs/technik/tabellen.md](docs/technik/tabellen.md).
- Jede Zeile mit Geschichte hat eine **Quelle** und eine **Sicherheit** (`gesichert`, `schaetzung`, `spielwert`, `ungeprueft`). Nur fachlich geprüfte Angaben sind `gesichert`.
- Nach Änderungen an Orten auch die Karte neu erzeugen (`python3 tools/kartengenerator/kartengenerator.py`); die CI prüft beides mit `--pruefen`.
- **Orte und Kreise des Kernraums** (seit M3) erzeugt `python3 tools/geodaten/orte.py` in `data/tabellen/orte.csv` und `kreise.csv`; die 60 Orte der Testregion behalten dabei ihre Daten. Gesteuert wird die Erzeugung über die Hilfstabellen in `tools/geodaten/` (`landschaften.csv`, `exonyme.csv`, `ausschluesse.csv`, `gelaende_gebiete.csv`); danach Archivar und Kartengenerator laufen lassen und `orte.py --pruefen` (Reihenfolge und Regeln: [tools/geodaten/README.md](tools/geodaten/README.md#orte-des-kernraums-ortepy)).
- **Ortsnamen:** Ein deutscher Name, der vom GeoNames-Hauptnamen abweicht, kommt nur mit Beleg in `tools/geodaten/exonyme.csv` (`geprueft = ja` nur, wenn der Name wörtlich in den GeoNames-Alternativnamen steht); Namen aus den Umbenennungen 1933–1945 werden nicht übernommen. Historische Namensformen, die GeoNames nicht belegt, kann `orte.py` noch nicht übernehmen (Einträge mit `geprueft = nein` wirken nicht); dafür braucht es eine andere zitierfähige Quelle und eine Erweiterung des Werkzeugs.
- **Geodaten:** Rohdaten (Natural Earth, GeoNames, Höhenkacheln) gehören **nie** ins Repository. `python3 tools/geodaten/geodaten.py holen` lädt sie nach `build/geodaten-cache/` (durch `.gitignore` ausgeschlossen); jede Datei ist mit URL, SHA-256, Lizenz und Quellenangabe in `tools/geodaten/quellen.json` gepinnt. Eine neue Quelle braucht eine Lizenz, die die Weitergabe abgeleiteter Daten erlaubt, und einen Eintrag in [Anhang C](docs/anhang/c_quellen_und_daten.md#5-verwendete-geodaten-stand-m3). Eingecheckt werden nur abgeleitete, kleine Dateien (`data/karte/geometrie/*.geojson`, Tabellen, Karte).
- Für sensible Themen gelten die Leitlinien in [docs/20_verantwortung_und_erinnerungskultur.md](docs/20_verantwortung_und_erinnerungskultur.md).
- Historische Fehler bitte über die Issue-Vorlage „Historischer Fehler“ melden.

## Große Dateien (Git LFS)

Quell-Assets wie `*.psd`, `*.kra`, `*.blend`, Audio (`*.wav`, `*.ogg`, …) und Schriften laufen über **Git LFS** (siehe `.gitattributes`). Vor dem ersten Commit solcher Dateien einmalig `git lfs install` ausführen. Kleine PNG-Dateien (z. B. Karten-ID-Bilder) liegen direkt im Repository.
