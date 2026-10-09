# Änderungsprotokoll

Alle nennenswerten Änderungen an EINIGKEIT werden in dieser Datei festgehalten.

Das Format basiert auf [Keep a Changelog](https://keepachangelog.com/de/1.1.0/),
die Versionsnummern folgen [Semantic Versioning](https://semver.org/lang/de/).
Vor Version 1.0 kann sich jede Schnittstelle ändern.

## [Unveröffentlicht]

Meilenstein **M1 – technischer Durchstich**: Daten → Simulationskern → GDExtension → Godot, mit
einer Testregion um 1648. Geplant als Version 0.1.0 (so steht es bereits in `CMakeLists.txt` und
`game/project.godot`).

### Hinzugefügt

- **Git-Grundkonfiguration:** `.gitignore`, `.gitattributes` (LF-Zeilenenden, Git LFS für große
  Quell-Assets), `.editorconfig`, `.clang-format`, Beitragsregeln in `CONTRIBUTING.md` (Struktur,
  Branches, Conventional Commits mit Bereich, Code-Stil), Issue-Vorlagen „Fehler“, „Historischer
  Fehler“ und „Vorschlag“, Vorlage für Pull Requests. godot-cpp 4.4.1 als Submodul unter
  `third_party/godot-cpp`.
- **Technische Spezifikation:** [`docs/technik/chronik-skript.md`](docs/technik/chronik-skript.md)
  (Syntax, Dateischemata, Startzustand, Formeln, Kartenformat) und
  [`docs/technik/bridge-api.md`](docs/technik/bridge-api.md) (GDScript-Schnittstelle).
- **Simulationskern „Uhrwerk“** (`core/`, C++20, ohne Abhängigkeiten außer der
  Standardbibliothek):
  - Festkommazahl `Fixed` (Q47.16) mit festgelegten Rundungsregeln, 128-Bit-Zwischenwerte ohne
    Compiler-Erweiterungen.
  - Zufallsgenerator PCG32 mit getrennten Strömen je System, abgeleitet aus dem Partie-Seed.
  - Kalender (gregorianisch) mit deutschen Datumsformaten.
  - Parser für das Chronik-Skript mit deutschen Fehlermeldungen (`datei:zeile:spalte`) und
    Wiederaufsetzen nach Fehlern.
  - Lader für Regeln, Gelände, Konfessionen, Berufe, Staaten, Orte und Szenarien mit
    Unterscheidung zwischen fatalen Fehlern und Warnungen.
  - Simulation mit Tagestick: Befehle (Steuersatz), gestaffelte monatliche Demografie und
    Wirtschaft, Seuchen und Abbau der Kriegsverwüstung zum Jahreswechsel, Chronik.
  - Prüfsumme (FNV-1a, 64 Bit) über den gesamten veränderlichen Zustand.
  - CMake-Presets `dev` (Debug, Warnungen als Fehler), `release` und `bridge`.
- **Kommandozeile `uhrwerk-cli`** mit den Befehlen `pruefen`, `info`, `lauf` (`--bericht`,
  `--chronik`) und `determinismus`; Optionen `--daten`, `--szenario`, `--jahre`, `--tage`,
  `--seed`; Exitcodes 0 (in Ordnung), 1 (Daten- oder Determinismusfehler), 2 (Bedienfehler).
- **Tests:** 87 Testfälle mit doctest 2.4.11 (Festkomma, Zufall, Kalender, Parser, Weltmodell,
  Lader, Simulation) und eigenen Testdaten in `tests/daten/`; dazu die CTest-Läufe
  `daten_pruefen` und `determinismus_50_jahre` mit den echten Spieldaten.
- **Testregion „Der Westfälische Frieden“ (24.10.1648):** 16 Staaten, 60 Orte in Mittel- und
  Nordostdeutschland, 10 Geländetypen, 4 Konfessionen, 6 Berufsgruppen, Spieler-Vorauswahl
  Brandenburg (siehe [`data/README.md`](data/README.md)).
- **Kartengenerator** (`tools/kartengenerator/`, nur Python-Standardbibliothek): erzeugt das
  Ort-ID-Bild `data/karte/provinzen.png` und `data/karte/definition.csv` deterministisch aus den
  Lagen der Orte; Vorschaubild mit `--vorschau`, Aktualitätsprüfung mit `--pruefen`; eigene Tests.
- **GDExtension-Brücke** (`bridge/`): Klasse `UhrwerkSimulation` für GDScript nach
  `bridge-api.md`; die Bibliothek wird direkt nach `game/bin/` gebaut.
- **Godot-Prototyp** (`game/`, Godot 4.4.1): Karte der Testregion mit fünf Kartenmodi
  (politisch, Bevölkerungsdichte, Konfession, Gelände, Verwüstung), obere Leiste mit Datum und
  Geschwindigkeit, Staatspanel mit Steuersatz-Regler, Ortsinfo, Chronik und Legende;
  Screenshots von der Kommandozeile; Rauchtest `game/tests/smoke_test.gd`. Ohne gebaute
  Erweiterung startet das Projekt mit einem Hinweis statt mit Fehlern.
- **CI** (GitHub Actions, `.github/workflows/ci.yml`): Kern mit GCC und Clang unter Linux und mit
  MSVC unter Windows, plattformübergreifender Vergleich der Prüfsummen, Formatprüfung mit
  clang-format 18, Tests und Aktualitätsprüfung des Kartengenerators, Godot-Job mit Brücke,
  Rauchtest und Screenshot der Hauptszene als Artefakt.
- **Dokumentation:** Abschnitt „Entwicklung“ in der `README.md`, technischer Überblick in
  [`docs/technik/README.md`](docs/technik/README.md), dieses Änderungsprotokoll.

## [0.0.1] – 2026-10-09

### Hinzugefügt

- **Game Design Document** (`README.md`, `docs/00`–`docs/24`, Anhänge A–C): Vision und Marke,
  Engine-Empfehlung (Godot 4 mit eigenem C++20-Kern „Uhrwerk“), Kernspielablauf, Epochen und
  Lesezeichen, Karte, Staaten, Bevölkerung, Wirtschaft, Politik, Parlamente und Wahlen,
  Parteien-Datenbank, Föderalismus, Persönlichkeiten, Diplomatie, Militär, Weltkriege, Teilung
  und Einheit, Ereignisse, Technologie und Kultur, Verantwortung und Erinnerungskultur, KI,
  Oberfläche und Audio, Multiplayer und Modding, Produktion und Roadmap; Zeitleiste, Glossar,
  Quellen.

[Unveröffentlicht]: https://github.com/Hoylerma/Game/compare/143c84ff750e89f80fae0e8c5bb4672ea5724cc8...HEAD
[0.0.1]: https://github.com/Hoylerma/Game/tree/143c84ff750e89f80fae0e8c5bb4672ea5724cc8
