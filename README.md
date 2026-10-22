# EINIGKEIT
### …und Recht und Freiheit · Deutschland von 1648 bis heute

> **Ein historisches Grand-Strategy-Spiel**, das die Karten-, Dynastie- und Kriegsstrategie von *Europa Universalis* mit der Gesellschafts-, Wirtschafts- und Politiksimulation von *Victoria 3* verbindet – konzentriert auf **ein** Land und **vier Jahrhunderte**: vom Flickenteppich aus über 300 Reichsständen nach dem Dreißigjährigen Krieg über Kaiserreich, Weimarer Republik, Diktatur, Weltkriege, Teilung und Wiedervereinigung bis zur Berliner Republik der Gegenwart.

**Dokumentstatus:** Game Design Document (GDD) · Version 0.1 · Konzeptphase
**Codename:** *Projekt Paulskirche*

---

## Auf einen Blick

| | |
|---|---|
| **Genre** | Historische Grand Strategy, Echtzeit mit Pause |
| **Vorbilder** | Europa Universalis IV/V, Victoria 3, Hearts of Iron IV, Crusader Kings III, *Through the Darkest of Times* |
| **Zeitraum** | 24. Oktober 1648 (Westfälischer Frieden) bis 1. Januar 2050 – historische Inhalte bis 2025, danach „Offene Zukunft“; Endlosmodus optional |
| **Startdaten** | 15 Lesezeichen von 1648 bis 2025 |
| **Karte** | ca. 4.500 „Orte“ im deutschen Kernraum, ca. 650 Provinzen im übrigen Europa, abstrakte Welt-Schauplätze (Kolonien, Seekrieg, Fernfronten) |
| **Spielbar** | 1648: über 300 Reichsstände (24 Fokus-Nationen mit eigenen Inhalten) · später Deutscher Bund, Kaiserreich, Weimarer Republik, BRD, DDR, alle Länder/Bundesländer |
| **Perspektiven** | Staatsmodus · Ländermodus (z. B. Bayern in der Bundesrepublik) · Parteienmodus · Opposition & Widerstand |
| **Engine** | Godot 4 (MIT-Lizenz) als Präsentationsschicht + eigener deterministischer C++20-Simulationskern „Uhrwerk“ |
| **Plattformen** | Windows, Linux (inkl. Steam Deck), macOS |
| **Multiplayer** | bis 16 Spieler – u. a. „Reichstag“ (300 Reichsstände), „16 Länder“, Parteienmodus, BRD gegen DDR |
| **Modding** | Vollständig: Skriptsprache „Chronik-Skript“, Karteneditor, Ereigniseditor, Steam Workshop |
| **Sprachen** | Deutsch (Leitsprache), Englisch; weitere geplant |
| **Spielzeit** | Große Kampagne 100–200 Stunden · einzelne Epochen 10–40 Stunden |
| **Altersfreigabe (Ziel)** | USK 12–16 (abhängig von der Bewertung der dokumentarischen NS-Inhalte), Schulfassung mit Begleitmaterial |

## Die drei Säulen

| **EINIGKEIT** | **RECHT** | **FREIHEIT** |
|---|---|---|
| Raum, Zusammenhalt, Nation: Territorien, Föderalismus, die deutsche Frage, Teilung und Einheit | Institutionen und Verfassung: Gesetze, Parlamente, Rechtsstaat, Legitimität | Gesellschaft und Teilhabe: Bevölkerung, Parteien, Wahlen, Grundrechte, Wohlstand |

Ergänzt durch zwei Querschnittsprinzipien: **Geschichte als lebendiges System** (historisch genau, aber offen) und **Verantwortung** (keine Verklärung, Opfer sichtbar, Konsequenzen spürbar).

---

## Inhaltsverzeichnis

### Grundlagen
| Nr. | Dokument | Inhalt |
|---|---|---|
| 00 | [Vision & Pitch](docs/00_vision_und_pitch.md) | Kernidee, EU×Victoria-Mischung, Designsäulen, Alleinstellungsmerkmale, Zielgruppe |
| 01 | [Name & Marke](docs/01_name_und_marke.md) | Titel, Alternativen, Logo, Key Art, Taglines, Markenrecht |
| 02 | [Engine & Technik](docs/02_engine_und_technik.md) | Engine-Vergleich, Architektur, Simulationskern, Kartenrenderer, Netzcode, Tools |
| 03 | [Kernspielablauf](docs/03_kernspielablauf.md) | Spielerrollen, Spielschleifen, Ressourcen, Beispielpartien, Wertung |
| 04 | [Zeitrahmen, Epochen & Startdaten](docs/04_zeitrahmen_epochen_startdaten.md) | 8 Epochen, 15 Lesezeichen, Mechanik-Phasen, Zeitgeist, Weltbühne |

### Welt & Gesellschaft
| Nr. | Dokument | Inhalt |
|---|---|---|
| 05 | [Karte & Geographie](docs/05_karte_und_geographie.md) | Kartenebenen, Orte, Gelände, Flüsse, Rohstoffe, Städte, Verwaltung im Wandel, Kartenmodi |
| 06 | [Staaten & Nationen](docs/06_staaten_und_nationen.md) | Reichsstände 1648, 24 Fokus-Nationen, formbare Staaten, Bruchstellen, Personalunionen |
| 07 | [Bevölkerung & Gesellschaft](docs/07_bevoelkerung_und_gesellschaft.md) | Pop-Modell, Stände und Klassen, Konfessionen, Milieus, Demografie, Migration |
| 08 | [Wirtschaft & Finanzen](docs/08_wirtschaft_und_finanzen.md) | Güter, Gebäude, Märkte, Zollverein, Geld & Währung, Krisen, Planwirtschaft |

### Politik
| Nr. | Dokument | Inhalt |
|---|---|---|
| 09 | [Politik, Staat & Gesetze](docs/09_politik_staat_und_gesetze.md) | Regierungsformen, Stände, Interessengruppen, 36 Gesetzesgruppen, Verfassungskonvente, Diktaturen |
| 10 | [Parlamente, Wahlen & Koalitionen](docs/10_parlamente_wahlen_koalitionen.md) | Parlamente im Wandel, Wahlrecht, Wahlsimulation, Wahlabend, Koalitionsverhandlungen |
| 11 | [Parteien-Datenbank](docs/11_parteien_datenbank.md) | Alle relevanten Parteien und Fraktionen von 1848 bis heute |
| 12 | [Föderalismus & Bundesländer](docs/12_foederalismus_und_bundeslaender.md) | Ländermodus, Bundesrat, alle historischen Länder, 16 Bundesländer im Profil |
| 13 | [Persönlichkeiten](docs/13_persoenlichkeiten.md) | Charaktersystem, Herrscher, Kanzler, Präsidenten, Politiker, Militär, Widerstand, Wissenschaft |

### Macht & Krieg
| Nr. | Dokument | Inhalt |
|---|---|---|
| 14 | [Diplomatie](docs/14_diplomatie.md) | Reichspolitik, Bündnisse, Krisen, Friedenskonferenzen, Kalter Krieg, EU-Ebene |
| 15 | [Militär & Kriegsführung](docs/15_militaer_und_kriegsfuehrung.md) | Emergente Fronten, Mobilmachung, Logistik, See- und Luftkrieg, Abschreckung |
| 16 | [Die Weltkriege](docs/16_weltkriege.md) | Erster und Zweiter Weltkrieg im Detail, NS-Zeit-Design, Widerstand |
| 17 | [Teilung & Einheit](docs/17_teilung_und_einheit.md) | Besatzung, BRD und DDR, Systemwettbewerb, Friedliche Revolution, Wiedervereinigung |

### Inhalte, Ethik & Systeme
| Nr. | Dokument | Inhalt |
|---|---|---|
| 18 | [Ereignisse, Journale & Missionen](docs/18_ereignisse_journale_missionen.md) | Ereignissystem, Wendepunkte, Missionsbäume, Beispielskripte, Chronik |
| 19 | [Technologie, Wissenschaft & Kultur](docs/19_technologie_wissenschaft_kultur.md) | Innovationsbaum, Universitäten, Brain Drain, Kultur, Sport |
| 20 | [Verantwortung & Erinnerungskultur](docs/20_verantwortung_und_erinnerungskultur.md) | Leitlinien für NS-Zeit, Holocaust, Kolonialismus, DDR-Unrecht; Recht, USK, Beirat |
| 21 | [Künstliche Intelligenz](docs/21_kuenstliche_intelligenz.md) | KI-Architektur, Persönlichkeiten, Regime-KI, Weltbühne-KI, Tests |
| 22 | [UI/UX, Grafik & Audio](docs/22_ui_ux_grafik_audio.md) | Bildschirme, Kartenstile je Epoche, Wahlabend, Zeitung, Musik |
| 23 | [Multiplayer & Modding](docs/23_multiplayer_und_modding.md) | Spielmodi, Netzcode, Skriptsprache, Werkzeuge, Workshop-Regeln |
| 24 | [Produktion, Roadmap & DLC](docs/24_produktion_roadmap_dlc.md) | Team, Budget, Meilensteine, Förderung, DLC-Plan, Risiken |

### Anhänge
| | Dokument | Inhalt |
|---|---|---|
| A | [Zeitleiste 1648–2025](docs/anhang/a_zeitleiste.md) | Ereignisgerüst für Inhalte und KI |
| B | [Glossar](docs/anhang/b_glossar.md) | Begriffe von „Ablösung“ bis „Zweitstimmendeckung“ |
| C | [Quellen & Daten](docs/anhang/c_quellen_und_daten.md) | Fachliteratur, Datenquellen, Lizenzen |

---

## Leseempfehlung

- **In 20 Minuten verstehen, worum es geht:** README → [00 Vision](docs/00_vision_und_pitch.md) → [03 Kernspielablauf](docs/03_kernspielablauf.md)
- **Was macht das Spiel einzigartig?** [12 Ländermodus](docs/12_foederalismus_und_bundeslaender.md), [10 Wahlen](docs/10_parlamente_wahlen_koalitionen.md), [15 Emergente Fronten](docs/15_militaer_und_kriegsfuehrung.md), [17 Teilung & Einheit](docs/17_teilung_und_einheit.md)
- **Für Programmierung:** [02 Engine](docs/02_engine_und_technik.md), [21 KI](docs/21_kuenstliche_intelligenz.md), [23 Modding](docs/23_multiplayer_und_modding.md) – und für den Code: [Entwicklung](#entwicklung), [Technische Dokumentation](docs/technik/README.md)
- **Für Historiker und Content-Team:** [11 Parteien](docs/11_parteien_datenbank.md), [13 Persönlichkeiten](docs/13_persoenlichkeiten.md), [20 Verantwortung](docs/20_verantwortung_und_erinnerungskultur.md), [Anhang A](docs/anhang/a_zeitleiste.md)

## Hinweise zu diesem Dokument

- Alle Zahlenwerte für Spielmechaniken (Modifikatoren, Schwellen, Kosten) sind **Startwerte für das Balancing**, keine endgültigen Festlegungen.
- Historische Zahlen sind gerundet und vor der Umsetzung durch das Historiker-Team gegen die in [Anhang C](docs/anhang/c_quellen_und_daten.md) genannten Quellen zu prüfen.
- Die Datenbasis für die Gegenwart ist auf den Stand Mai 2025 (Startdatum des letzten Lesezeichens) eingefroren und wird per Update fortgeschrieben.
- „Erstmal nur Deutschland“: Die Architektur ist von Anfang an darauf ausgelegt, später weitere Länder als Nationen-Pakete zu ergänzen (siehe [24 Roadmap](docs/24_produktion_roadmap_dlc.md)).

---

## Entwicklung

[![Prüfungen](https://github.com/Hoylerma/Game/actions/workflows/ci.yml/badge.svg)](https://github.com/Hoylerma/Game/actions/workflows/ci.yml)

### Projektstand

**Meilensteine M1 (technischer Durchstich), M2 (Datenpipeline), M3 (Karte des Kernraums), M4
(Bevölkerung und Lasttest), M5 (Wirtschaft und Staatshaushalt) und M6 (Speicherstände und
Wiederholungen) sind abgeschlossen – damit ist die Vorproduktion (Phase 1) fertig. In der Phase 2
sind M7 (Lesezeichen 1848) und M8 (Politik, Parteien und Wahlen) technisch fertig; die Fachprüfung
der Daten und Werte steht aus**, siehe [Meilensteinplan](docs/meilensteine.md). Der ganze Weg von den Daten bis
zum Bild steht: Tabellen → Archivar → Chronik-Skript → Simulationskern „Uhrwerk“ → GDExtension →
Godot-Oberfläche. Die Karte zeigt seit M3 den **ganzen Kernraum mit 4.542 Orten in 626 Kreisen**;
historische Daten (Besitzer, Bevölkerung, Konfession, Verwüstung) haben um 1648 nur die
**60 Orte der Testregion**. Die übrigen 4.482 Orte sind dort *Orte ohne Daten*: Sie stehen auf
der Karte, nehmen an der Simulation aber nicht teil. Seit M7 gibt es das zweite Lesezeichen,
**„Völkerfrühling“ (1. März 1848)**. Es hat 3.967 Orte mit Daten in 38 Staaten des Deutschen
Bundes, die Verwaltung bis zu den preußischen und österreichischen Kreisen, die Bevölkerung 1846
(45 Mio.), die Eisenbahnen bis 1848 und die frühe Industrie. Seit M4 entsteht die Bevölkerung aus
Geburten, Sterbefällen, Altern und Wanderung (mit Kultur, Lebensstandard und Alphabetisierung), und
der Kern rechnet im Lasttest 500.000 Bevölkerungsblöcke in 0,6 s je Spieljahr auf mehreren Kernen –
mit gleicher Prüfsumme bei jeder Zahl von Threads. Seit M5 entstehen Einkommen, Preise und Steuern
aus Gütern, Betrieben und regionalen Märkten, und die Staaten führen einen Haushalt mit Kredit und
Staatsbankrott; ein Balancing-Bericht prüft in der CI, dass die Kassen über 100 Jahre plausibel
bleiben. Seit M6 lassen sich Partien speichern, laden und bitgleich fortsetzen (zstd-komprimiert,
38 KB nach 100 Jahren), und aus Startzustand und Befehlsprotokoll lässt sich jede Partie exakt
wiederholen. Seit M8 wählt der Deutsche Bund 1848 die Nationalversammlung: 633 Wahlkreise, fünf
Strömungen und zehn Interessengruppen (die Arbeiterbewegung folgt im August), Stichwahl und
Fraktionen der Paulskirche; die Preußische Zweite Kammer wählt 1849 nach dem Dreiklassenwahlrecht.
Der Wahlabend inszeniert das Ergebnis mit Hochrechnung, und im Parteienmodus führt man als Strömung
Wahlkampf. Das ganze Spiel aus dem Design-Dokument ist das noch nicht.

| Baustein | Stand |
|---|---|
| Simulationskern „Uhrwerk“ (`core/`) | Festkomma, Zufallsströme, Kalender, Parser und Lader für das Chronik-Skript mit datierter Historie, Tagestick, Seuchen und Kriegsverwüstung, Steuerbefehl, Chronik, Prüfsumme; seit M3 Orte ohne Daten und Kreise; seit M4 Bevölkerungsmodell (Altersaufbau, Kultur, Lebensstandard, Bildung, Wanderung), Mehrkernberechnung und Lasttest-Welt; seit M5 Wirtschaft (Güter, Betriebe, Märkte, Handel, Verbrauch) und Staatshaushalt (drei Steuerarten, Kredit, Bankrott, Landesausbau), Balancing-Bericht; seit M6 Zustandsverzeichnis, Spielstände (zstd), Inhaltsfingerabdruck, Befehlsprotokoll und Wiederholung, Textexport des Zustands; seit M7 Verwaltungsgliederung, Bundesgebiet und Zollverein, spielbare Staaten, Eisenbahnen mit Bahnfracht, Inhalt ab einem Datum (`seit`) und Tragfähigkeit je Szenario; seit M8 Strömungen, Interessengruppen, Verfassung und Wahlrecht, Parlamente und Wahlkreise, Wahl mit Stichwahl, Boykott und Fraktionen, Wahlkampf und Spenden, Spielstand Format 2; seit M9 Zollgebiete mit Zoll im Handel, Vereinskasse und Beitritt, Beziehungen zwischen den Staaten mit Gesandten, Bündnissen und Garantien, Spielstand Format 3 |
| Kommandozeile `uhrwerk-cli` (`cli/`) | Daten prüfen, Überblick (Orte mit und ohne Daten, Kreise), Simulationsläufe mit Bericht, Bevölkerungsbilanz und Chronik, Determinismusprüfung, Lasttest, Wirtschaft im Überblick, Balancing-Bericht über viele Seeds; seit M6 Speichern, Laden, Protokoll, Wiederholung, Speichertest und Spielstandinfo mit Textexport; seit M7 alle Befehle auch für 1848 (`--szenario voelkerfruehling`), Zielwerte je Szenario; seit M8 Wahllabor (`wahl`) mit Wahlzielen; seit M9 Zolllabor (`zoll`) mit Zollzielen |
| Tests (`tests/`) | 169 Testfälle (doctest) und Integrationsläufe mit den echten Daten, Lasttest mit 3 Threads gegen 1 Thread, Balancing-Bericht auf der Test-Fixture, Speichertest und Wiederholung über 50 Jahre, Datenprüfung, Determinismus und Speichertest für 1848, Wahllabor auf der Fixture und für 1848 |
| Spieldaten (`data/`) | Lesezeichen „Der Westfälische Frieden“ (24.10.1648): 16 Staaten, 4.542 Orte in 626 Kreisen, davon 60 mit historischen Daten (Mittel- und Nordostdeutschland); seit M5 Güter, Bedürfnisse, Betriebe, 77 Märkte und Zielwerte des Balancing-Berichts; seit M7 Lesezeichen „Völkerfrühling“ (1.3.1848): 38 Staaten, 3.967 Orte mit Daten, 458 Verwaltungseinheiten, 42 Eisenbahnstrecken, frühe Industrie ab 1800 und eigene Zielwerte; seit M8 Strömungen, Interessengruppen, Verfassung und Wahlrecht der Staaten, Nationalversammlung 1848 und Preußische Zweite Kammer 1849 mit 633 bzw. 337 Wahlkreisen, Wahlregeln und Wahlziele; gepflegt in Tabellen mit Quelle und Sicherheit je Angabe |
| Archivar (`tools/archivar/`) | erzeugt `data/history/` (Staaten, Kreise, Orte) aus den Tabellen und prüft Verweise, Lücken und Ausreißer; seit M7 auch Verwaltung und Eisenbahnen, seit M8 Wahlkreise; 44 Tests |
| Geodaten (`tools/geodaten/`) | lädt und pinnt die offenen Quellen (Natural Earth, GeoNames, Terrain Tiles), schneidet Land, Seen und Flüsse zu und erzeugt mit `orte.py` die Orte und Kreise des Kernraums; seit M7 Zuordnung der Orte 1848 (`zuordnung.py`), Bevölkerung 1846 und Stichprobe; seit M8 Wahlkreise 1848/49 (`wahlkreise_1848.py`); 123 Tests |
| Kartengenerator (`tools/kartengenerator/`) | erzeugt Ort-ID-Bild mit Flächenart (4096 × 3329 Pixel), Flussebene und Zuordnungstabelle aus Geometrie und Orten (Python mit numpy, 13–21 s); 88 Tests |
| Brücke (`bridge/`) | GDExtension mit der Klasse `UhrwerkSimulation` |
| Godot-Prototyp (`game/`) | Großkarte des Kernraums mit vierzehn Kartenmodi (u. a. Lebensstandard, Kultur, Wirtschaft, Märkte, Verwaltung, Eisenbahnen, Wahlen und Zollgebiete), Zoomstufen mit Staats-, Kreis- und Ortsnamen, Staatspanel mit drei Steuerreglern und Haushaltsfenster, Ortsinfo mit Wirtschaft, Kulturen, Altersaufbau und Bevölkerungsbilanz, Chronik, Legende, Spielmenü mit Speichern und Laden, F5/F9, Autosave; neues Spiel mit Wahl des Lesezeichens (1648 oder 1848) und Staatswahl; seit M8 Wahlabend, Parteienmodus und „Verfassung und Gruppen“ in der Staatsinfo; seit M9 Fenster „Zollunion“ mit Beitritt und Austritt; Einheitentest (267 Prüfungen), Rauchtest, Klicktest (4.542 von 4.542 Orten in drei Zoomstufen, Namen und Panels, Wirtschaft, Spielstände, Wahlabend, Parteienmodus, Zollunion) |
| CI (`.github/workflows/`) | Linux (GCC, Clang), Windows (MSVC), Prüfsummenvergleich (100 Jahre, Lasttest und Prüfwert eines Spielstands), Lasttest mit 500.000 Blöcken (Release), Balancing-Bericht (16 Seeds, 100 Jahre; 1848 mit 8 Seeds, 30 Jahre), Wahllabor 1848/49, Formatierung, Python-Werkzeuge (Python 3.10 und 3.13 mit numpy), Godot mit Einheiten-, Rauch- und Klicktest und Screenshots |

Noch **nicht** umgesetzt ist alles Weitere aus dem Design-Dokument – historische Daten für den
übrigen Kernraum um 1648, Gesetzgebung und Regierungsbildung, Diplomatie, Krieg, Ereignisse, KI,
Multiplayer und Modding-Werkzeuge. Was sich ändert, steht im [Änderungsprotokoll](CHANGELOG.md).

### Repository-Struktur

| Ordner | Inhalt | Abhängigkeiten |
|---|---|---|
| [`core/`](core/) | Simulationskern **Uhrwerk** (C++20) – engine-unabhängig, deterministisch, headless | nur C++-Standardbibliothek |
| [`cli/`](cli/) | `uhrwerk-cli`: Kommandozeile für Testläufe, Determinismusprüfung, Berichte | `core` |
| [`tests/`](tests/) | Unit- und Integrationstests des Kerns | `core`, doctest (wird von CMake geladen) |
| [`bridge/`](bridge/) | GDExtension-Brücke zwischen Uhrwerk und Godot | `core`, `third_party/godot-cpp` |
| [`game/`](game/) | Godot-4-Projekt (Szenen, Skripte, Shader, Oberfläche) | `bridge` |
| [`data/`](data/) | Spielinhalte: Tabellen (`tabellen/*.csv`), daraus erzeugtes **Chronik-Skript** (`*.chron`), Karte (`karte/`) | – |
| [`tools/`](tools/) | Hilfswerkzeuge: [Archivar](tools/archivar/README.md) (Datenpipeline), [Geodaten](tools/geodaten/README.md) (Quellen, Geometrie, Orte des Kernraums), [Kartengenerator](tools/kartengenerator/README.md) | Python 3.10+; Geodaten und Kartengenerator mit numpy |
| [`docs/`](docs/) | Game Design Document, technische Dokumentation in [`docs/technik/`](docs/technik/) | – |
| `third_party/` | Fremdbibliotheken als Git-Submodule (godot-cpp 4.4.1) | – |

Grundregel: **`core/` kennt Godot nicht.** Die Godot-Seite liest nur und schickt Befehle.

### Voraussetzungen

| Werkzeug | Version | wofür |
|---|---|---|
| C++-Compiler mit C++20 | GCC 13 oder Clang 18 (lokal geprüft), MSVC 2022 (Kern, CLI und Tests in der CI; Brücke unter Windows noch ungeprüft) | Kern, CLI, Tests, Brücke |
| CMake | 3.25 oder neuer (die Presets haben Version 6) | Build |
| Ninja | aktuelle Version (geprüft mit 1.11) | Generator aller Presets |
| Python | 3.10 oder neuer | Archivar, Geodaten, Kartengenerator; godot-cpp erzeugt damit seine Bindings |
| numpy | 2.x (geprüft mit 2.2.6 unter Python 3.10 und 2.5.3 unter Python 3.13) | Kartengenerator und Geodaten: `python3 -m pip install numpy` |
| Godot | **4.4.1** (Standard-Build, kein .NET) | Spiel und Editor; [Download](https://godotengine.org/download/archive/4.4.1-stable/), optional abgelegt unter `tools/godot/` (nicht versioniert) |
| clang-format | 18 | Formatierung des C++-Codes |

Unter Windows die Befehle in der **Developer PowerShell for VS 2022** ausführen (dort sind `cl`
und Ninja im Pfad) und beim ersten Konfigurieren `-DCMAKE_CXX_COMPILER=cl` angeben.

### Schnellstart

**1. Klonen** (mit Submodulen, sonst fehlt godot-cpp für die Brücke):

```bash
git clone --recurse-submodules https://github.com/Hoylerma/Game.git
cd Game
# bei einem bestehenden Klon ohne Submodule:
git submodule update --init --recursive
```

**2. Kern, Kommandozeile und Tests bauen und testen** (Preset `dev`: Debug, Warnungen als Fehler):

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Weitere Presets: `release` (optimiert, Ausgabe in `build/release/`) und `bridge` (siehe Schritt 5).

**3. Kommandozeile** – sucht den Datenordner selbst (`UHRWERK_DATEN` oder `data/` ab dem
Arbeitsverzeichnis), alternativ `--daten DIR`:

```bash
./build/dev/cli/uhrwerk-cli pruefen                            # Spieldaten laden, Fehler und Warnungen zeigen
./build/dev/cli/uhrwerk-cli info                               # Szenario, Staaten und Orte im Überblick
./build/dev/cli/uhrwerk-cli lauf --jahre 50 --bericht --chronik  # 50 Jahre ab 1648 simulieren
./build/dev/cli/uhrwerk-cli lauf --jahre 10 --seed 7           # anderer Seed
./build/dev/cli/uhrwerk-cli determinismus --jahre 100          # zwei Läufe vergleichen (Prüfsumme je Jahr)
./build/dev/cli/uhrwerk-cli wirtschaft --jahre 20 --maerkte    # Güter, Berufe, Haushalt, Preise je Markt
./build/dev/cli/uhrwerk-cli lauf --jahre 50 --testbefehle 200 --speichern partie.einigkeit --protokoll partie.protokoll
./build/dev/cli/uhrwerk-cli lauf --laden partie.einigkeit --jahre 10   # Spielstand laden und weiterrechnen
./build/dev/cli/uhrwerk-cli wiederholung partie.protokoll      # Partie vom Start bitgleich nachrechnen
./build/dev/cli/uhrwerk-cli spielstand partie.einigkeit --text # Zustand als Text (für diff)
./build/dev/cli/uhrwerk-cli speichertest --jahre 50 --testbefehle 200   # Abnahme: Speichern ändert nichts
./build/dev/cli/uhrwerk-cli --hilfe                            # alle Befehle und Optionen

# Release-Build: cmake --preset release && cmake --build --preset release
./build/release/cli/uhrwerk-cli lasttest --jahre 3 --vergleich 1 --grenze 2   # 500.000 Bevölkerungsblöcke
./build/release/cli/uhrwerk-cli bilanz --seeds 16 --jahre 100               # Balancing-Bericht gegen die Zielwerte
./build/release/cli/uhrwerk-cli wahl --szenario voelkerfruehling --seeds 16  # Wahllabor: Nationalversammlung 1848
./build/release/cli/uhrwerk-cli zoll --szenario voelkerfruehling            # Zolllabor: Beitritte zum Zollverein
```

Exitcodes: `0` in Ordnung, `1` Daten- oder Determinismusfehler oder verfehlte Zielwerte (`bilanz`, `wahl`, `zoll`),
`2` Bedienfehler.

**4. Daten bearbeiten** – Orte und Staaten stehen in `data/tabellen/*.csv` (Format:
[docs/technik/tabellen.md](docs/technik/tabellen.md)); der Archivar erzeugt daraus `data/history/`:

```bash
python3 tools/archivar/archivar.py              # prüfen und data/history/ neu erzeugen
python3 tools/archivar/archivar.py --pruefen    # ist data/history/ aktuell? (wie in der CI)
python3 tools/archivar/archivar.py --bericht    # Umfang und Sicherheit der Angaben
python3 -m unittest discover -s tools/archivar -v
```

**Karte erzeugen** – nach Änderungen an Orten, an `data/karte/karte.chron` oder an der Geometrie
in `data/karte/geometrie/`. Der Kartengenerator braucht numpy; die erzeugten Dateien
`data/karte/provinzen.png`, `fluesse.png` und `definition.csv` werden eingecheckt:

```bash
python3 -m pip install numpy                                             # einmalig
python3 tools/kartengenerator/kartengenerator.py                         # Karte neu erzeugen (13–21 s)
python3 tools/kartengenerator/kartengenerator.py --pruefen               # ist die Karte aktuell? (wie in der CI)
python3 tools/kartengenerator/kartengenerator.py --pruefen --vorschau /tmp/karte.png --vorschau-breite 2048
                                                                         # Vorschau zum Ansehen (nicht einchecken)
python3 -m unittest discover -s tools/kartengenerator -v                # Tests des Generators
```

**Geodaten und Orte des Kernraums** – `tools/geodaten/` lädt die offenen Quellen (Natural Earth,
GeoNames, Terrain Tiles; etwa 50 MB) nur in den Zwischenspeicher `build/geodaten-cache/`, nie ins
Repository; jede Datei ist mit SHA-256 in `tools/geodaten/quellen.json` gepinnt. Die Tests
brauchen weder Netz noch Zwischenspeicher:

```bash
python3 tools/geodaten/geodaten.py holen          # Quellen laden bzw. Prüfwerte vergleichen
python3 tools/geodaten/geodaten.py pruefen        # passen data/karte/geometrie/*.geojson zu den Quellen?
python3 tools/geodaten/orte.py --pruefen          # sind orte.csv und kreise.csv aktuell? (1–2 min)
python3 -m unittest discover -s tools/geodaten -v # Tests ohne Netz (wie in der CI)
```

Neu erzeugt wird in dieser Reihenfolge: `geodaten.py geometrie` (Land, Seen, Flüsse) →
`orte.py` (`data/tabellen/orte.csv`, `kreise.csv`) → `archivar.py` → `kartengenerator.py`.
Einzelheiten, Quellenangaben und Namensregeln: [tools/geodaten/README.md](tools/geodaten/README.md).

**5. Brücke bauen** – baut godot-cpp (beim ersten Mal je nach Rechner 5–15 Minuten) und legt die Bibliothek
direkt im Godot-Projekt ab (`game/bin/libuhrwerk_bridge.so`, `uhrwerk_bridge.dll` bzw.
`libuhrwerk_bridge.dylib`):

```bash
cmake --preset bridge
cmake --build --preset bridge
```

**6. Godot öffnen** – `godot` steht für Godot 4.4.1, z. B.
`tools/godot/Godot_v4.4.1-stable_linux.x86_64`:

```bash
godot --headless --path game --import     # einmalig: Projekt importieren (legt game/.godot/ an)
godot --path game                         # Spiel starten
godot --editor --path game                # im Editor öffnen

# Tests der Godot-Seite (wie in der CI): Hilfsklassen, Brücke und Kartendaten, Klickerkennung
godot --headless --path game --script res://tests/unit_test.gd     # „UNIT OK (260 Prüfungen)“
godot --headless --path game --script res://tests/smoke_test.gd    # „SMOKE OK“
godot --headless --path game --script res://tests/klick_test.gd    # „KLICK OK 4542/4542“

# Screenshot ohne Bildschirm (Software-Rendering über Xvfb)
xvfb-run -a -s "-screen 0 1600x900x24" \
  godot --path game --rendering-driver opengl3 --audio-driver Dummy \
  -- --screenshot=/tmp/einigkeit.png --tage=60 --kartenmodus=0 --ort=berlin_coelln
```

Ohne gebaute Brücke oder ohne Karte startet das Projekt trotzdem und zeigt einen Hinweis, wie
sie erzeugt werden. Bedienung, Optionen (z. B. `--zoom=4` für die mittlere Zoomstufe) und
Leistungstest: [`game/README.md`](game/README.md), [`game/tests/README.md`](game/tests/README.md).

**Formatierung prüfen** (wie in der CI):

```bash
clang-format-18 --dry-run --Werror $(git ls-files 'core/*.[ch]pp' 'cli/*.[ch]pp' 'tests/*.[ch]pp' 'bridge/*.[ch]pp' 'bridge/*.h')
clang-format-18 -i <datei>                # Datei formatieren
```

### Weiterlesen

| Dokument | Inhalt |
|---|---|
| [CONTRIBUTING.md](CONTRIBUTING.md) | Branches, Commit-Nachrichten, Pull Requests, Code-Stil, Regeln für Daten |
| [docs/technik/](docs/technik/README.md) | Architektur, Tick-Ablauf, Determinismus, Festkomma, Zufallsströme; Spezifikationen [Chronik-Skript](docs/technik/chronik-skript.md), [Tabellen](docs/technik/tabellen.md) und [Brücken-API](docs/technik/bridge-api.md) |
| [data/README.md](data/README.md) | Inhalt und Aufbau der Spieldaten: Kernraum, Orte ohne Daten, Testregion 1648 |
| [game/README.md](game/README.md) | Godot-Projekt: Starten, Bedienung, Aufbau, Leistung |
| [game/tests/README.md](game/tests/README.md) | Tests der Godot-Seite: Einheiten-, Rauch-, Klick- und Leistungstest |
| [tools/archivar/README.md](tools/archivar/README.md) | Archivar: Aufruf, Arbeitsweise, Orte ohne Daten und Kreise |
| [tools/geodaten/README.md](tools/geodaten/README.md) | Geodaten: Quellen und Lizenzen, Zuschnitt, Kernraum, Orte und Kreise des Kernraums |
| [tools/kartengenerator/README.md](tools/kartengenerator/README.md) | Kartengenerator: Aufruf, Verfahren, Determinismus |
| [docs/meilensteine.md](docs/meilensteine.md) | Meilensteine M1–M13 für Vorproduktion und Vertical Slice |
| [CHANGELOG.md](CHANGELOG.md) | Änderungsprotokoll |
