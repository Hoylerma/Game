# Paket – Auslieferung von EINIGKEIT

Seit M13b5 ([vertical_slice.md](../../docs/technik/vertical_slice.md), Abschnitt 7). Baut aus dem
Godot-Projekt spielbare Pakete für Linux, Windows und macOS und prüft sie.

| Datei | Zweck |
|---|---|
| `paket.py` | `bauen` (Export, Daten, Texte, Archiv), `rauchtest` (Paket gegen `uhrwerk-cli`), `drittanbieter` |
| `lizenzen.gd` | gibt die Lizenzangaben der Godot-Engine aus (für `DRITTANBIETER.txt`) |
| `einigkeit.iss` | Installer für Windows (Inno Setup 6) |
| `test_paket.py` | Tests ohne Godot: Version, Exportvorlagen, Texte, Auswertung des Rauchtests |

## Pakete

| System | Ergebnis in `build/pakete/` | Inhalt |
|---|---|---|
| Linux x86_64 | `einigkeit-<version>-linux-x86_64.tar.gz` | `einigkeit.x86_64`, `einigkeit.pck`, `libuhrwerk_bridge.so`, `data/`, Texte |
| Windows x86_64 | `…-windows-x86_64.zip`, `…-windows-x86_64-setup.exe` | `einigkeit.exe`, `einigkeit.console.exe`, `.pck`, `uhrwerk_bridge.dll`, `data/`, Texte |
| macOS (universal) | `einigkeit-<version>-macos.dmg` | `EINIGKEIT.app` (Daten in `Contents/Resources/data`), Texte |

Die Exportvorlagen stehen in `game/export_presets.cfg` (Release, `.pck` neben dem Programm; `tests/` und
die Notentexte bleiben draußen). Die Version kommt aus `game/project.godot` (`config/version`) und muss zu
`CMakeLists.txt` und den Vorlagen für Windows und macOS passen (`test_paket.py` prüft das).

Die Spieldaten liegen als Ordner `data/` neben dem Programm; `UhrwerkPaths` findet sie dort in jeder
exportierten Fassung (auf macOS in der App). Die macOS-App wird nach dem Einfügen der Daten **ad hoc**
signiert (sonst startet sie auf Apple Silicon nicht), aber nicht notarisiert: Beim ersten Start mit
Rechtsklick → Öffnen.

## Bauen

Voraussetzungen: Godot 4.4.1 (Editor) mit den Exportvorlagen 4.4.1, Python 3.10+, die Brücke als
Release und die gerenderte Musik.

```bash
cmake --preset bridge-release && cmake --build --preset bridge-release   # macOS: -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
python3 tools/klang/klang.py
python3 tools/paket/paket.py bauen --system linux --godot tools/godot/Godot_v4.4.1-stable_linux.x86_64
```

`bridge-release` schreibt dieselbe Datei in `game/bin/` wie das Preset `bridge`; für Editor und Tests
danach wieder `cmake --build --preset bridge`. Den Windows-Installer baut Inno Setup aus dem
Paketordner:

```bat
iscc /DVersion=0.13.0 /DQuelle=build\pakete\einigkeit-0.13.0-windows-x86_64 /DAusgabe=build\pakete tools\paket\einigkeit.iss
```

DMG und Signatur entstehen nur auf macOS (`hdiutil`, `codesign`); auf anderen Systemen bleibt der
Ordner mit der App.

## Paketrauchtest

```bash
python3 tools/paket/paket.py rauchtest --cli build/release/cli/uhrwerk-cli \
    --programm build/pakete/einigkeit-0.13.0-linux-x86_64/einigkeit.x86_64
```

Startet das gepackte Programm ohne Fenster (`--headless -- --rauchtest --tage=30 --szenario=…`) für den
Westfälischen Frieden und den Völkerfrühling und vergleicht die Prüfsumme mit `uhrwerk-cli lauf` aus
demselben Stand. Er schlägt auch fehl, wenn das Programm seine Daten nicht aus dem Paket liest. Unter
Windows nimmt er `einigkeit.console.exe` (nur sie schreibt auf die Konsole), auf macOS das Programm in
der App. Ergebnis: `PAKETRAUCHTEST OK`, Exitcode 0; sonst 1.

## Workflow `pakete.yml`

Läuft von Hand (Actions → Pakete → Run workflow) und bei Tags `v*` (der Tag muss zur Version passen).
Je System: Brücke als Release und `uhrwerk-cli` bauen, Godot und Exportvorlagen laden (SHA-512 aus
`SHA512-SUMS.txt` des Releases) und cachen, Musik rendern, packen, Paketrauchtest, unter Windows der
Installer. Die Pakete hängen 30 Tage als Artefakte am Lauf.

## DRITTANBIETER.txt

`paket.py` erzeugt sie bei jedem Bau (`paket.py drittanbieter --godot …` gibt sie nur aus): die Lizenz
der Godot-Engine mit allen ihren Bestandteilen (von Godot selbst ausgegeben), godot-cpp (MIT), zstd
(BSD, aus dem Build-Ordner), die vier Schriften (SIL Open Font License, aus `game/assets/schriften`)
und die Herkunft der Musik. EINIGKEIT selbst hat vorerst keine eigene Lizenz.

## Rückmeldung

Im Spielmenü legt „Bericht anlegen“ unter **Rückmeldung** ein ZIP im Benutzerordner an
(`rueckmeldungen/rueckmeldung_<Datum>_<Zeit>.zip`): `info.txt` (Version, System, Grafik, Sprache,
Szenario, Spieldatum, Prüfsumme), `notiz.txt` (das Textfeld), `spielstand.einigkeit`, `godot.log` und
`einstellungen.cfg`. Es wird nichts versandt; Testerinnen und Tester schicken die Datei selbst.

```bash
python3 -m unittest discover -s tools/paket -v
```
