# Änderungsprotokoll

Alle nennenswerten Änderungen an EINIGKEIT werden in dieser Datei festgehalten.

Das Format basiert auf [Keep a Changelog](https://keepachangelog.com/de/1.1.0/),
die Versionsnummern folgen [Semantic Versioning](https://semver.org/lang/de/).
Vor Version 1.0 kann sich jede Schnittstelle ändern.

## [Unveröffentlicht]

### Meilenstein M9 – Diplomatie und Zollverein

Vertrag: [technik/diplomatie.md](docs/technik/diplomatie.md), Entscheidungen 1–12 wie empfohlen
freigegeben; Stand je Schritt in Abschnitt 11 dort.

- **Zollunionen als Daten (Schritt a1):** `common/zollunionen.chron` mit Zollverein (Leitung
  Preußen) und Steuerverein (Leitung Hannover), Tarif je 3 %
  ([chronik-skript.md](docs/technik/chronik-skript.md) Abschnitt 3.25). Das Staatsfeld
  `zollverein = ja/nein` heißt jetzt `zollunion = <kennung>|keine`; 1848 gehören 26 Staaten zum
  Zollverein, Hannover, Oldenburg und Schaumburg-Lippe zum Steuerverein, neun Staaten sind je ein
  eigenes Zollgebiet. Lader und Archivar prüfen die Verweise, `uhrwerk-cli info` zeigt Unionen und
  Mitglieder, neuer CTest `zollunionen_1848`. Inhaltsbereich `diplomatie`, Spielstand **Format 3**
  (Spielstände aus M8 laden nicht mehr). Brücke: `get_state` liefert `customs_union` und
  `customs_union_name` statt `zollverein`. Noch ohne Wirkung auf Handel und Haushalt; 1648 bitgleich.
- **Zollgebiete im Kern (Schritt a2):** Gibt es Zollunionen, teilt sich jeder Markt nach der
  Bevölkerung in Zollgebiete. Der Zoll wirkt im Handel wie zusätzliche Fracht und trifft beim Kauf
  nur die Einfuhr von außerhalb des eigenen Gebiets; die Mitglieder einer Union teilen ihren Zoll
  über die Vereinskasse nach der Bevölkerung. Neuer Befehl `Zollunion` (Beitritt mit Zustimmung der
  Leitmacht, Austritt, Wechsel, wirksam zum 1. Januar, mit Chronik); Mitglieder können ihren
  Zollsatz nicht selbst ändern; der Zollverein lehnt Österreich ab (`ausgeschlossen`). 1848 sinkt
  der Zoll von 5,7 % auf 1,1 % der Staatseinnahmen, der Handel steigt um 9 %; Bilanz und Wahlen 1848
  erfüllen weiter alle Zielwerte. 1648 bitgleich.
- **Zolllabor und Kalibrierung (Schritt a3):** `uhrwerk-cli zoll` vergleicht je Seed Läufe ohne und
  mit dem Wechsel eines Staates in eine Zollunion (Grenzhandel, Handel, Zoll, Preise im letzten Jahr)
  und prüft den Zollanteil der Unionen gegen `common/zollziele_voelkerfruehling.chron`; neuer
  CI-Schritt „Zolllabor 1848“. Der Vereinstarif des Zollvereins steigt auf 10 % (Zollanteil 2,9 % statt
  1 %); der Beitritt Hannovers erhöht den Grenzhandel um 40 % und senkt seinen Zoll auf zwei Drittel,
  der Mecklenburg-Schwerins verdoppelt ihn. Tritt eine Leitmacht aus, löst sich ihre Union auf
  (Hannover und der Steuerverein). Bilanz und Wahlen 1848 erfüllen alle Zielwerte.
- **Zollunionen in Godot (Schritt a4):**
  - Kartenmodus „Zollgebiete“ (13, Taste Ö): die Unionen in ihrer Farbe (`farbe` in
    `zollunionen.chron`), Staaten ohne Union blass; Legende mit Mitgliederzahl, Tooltip mit Tarif.
  - Staatspanel: Der Zollregler eines Mitglieds ist gesperrt und zeigt den Tarif der Union; im
    Haushaltsfenster steht der Zoll als Anteil an der Vereinskasse. Neuer Knopf „Zollunion …“ mit
    Fenster: Zollgebiet, Tarif, Anteil an der Kasse, je Union Leitmacht, Mitglieder, Tarif und Kasse;
    „Beitreten …“, „Austreten …“ und „Antrag zurückziehen …“ mit Rückfrage und Zeitpunkt (nächster
    1. Januar, Auflösung der eigenen Union beim Austritt einer Leitmacht). Ein offener Antrag steht
    im Hinweis des Panels.
  - Ortsinfo: Zollgebiet des Ortes mit Tarif. Startoption `--zollunion`.
  - Brücke: `get_customs_unions`, `get_customs_change` (Vorab-Prüfung ohne Wirkung, Kern
    `evaluate_customs_change`), `request_customs_union`, neue Felder in `get_state` und
    `get_location`, `MAP_ZOLLGEBIETE`; `set_tax_percent` lehnt den Zoll eines Mitglieds ab.
  - Tests: Rauchtest mit Unionen, Vorab-Prüfung und Antrag Hannovers; Klicktest mit Beitritt und
    Rücknahme über das Fenster; CI-Screenshot `einigkeit_zoll.png`. 1648 bitgleich.
- **Diplomatie im Kern (Schritt b1):** In Szenarien mit Deutschem Bund hat jedes Staatenpaar eine
  Beziehung von −200 bis +200, die sich monatlich auf ihren Zielwert zu bewegt: Grundwert aus der neuen
  Tabelle `beziehungen.csv` (24 Paare 1848, z. B. Österreich–Preußen −40), dazu gemeinsame Zollunion,
  Bund, Konfession, Bündnis und Garantie (`common/diplomatie.chron`). Neue Befehle `Gesandter`
  (verbessert die Beziehung, kostet 0,5 % der Monatseinnahmen; Preußen 4, Österreich 3, sonst 1
  Gesandter) und `Vertrag` (Bündnis ab Beziehung 100, Garantie ab 50, kein Bündnis mit dem Verbündeten
  eines Gegners; Kündigung). `uhrwerk-cli info` zeigt Gegner, Gesandte und die Lager zwischen Preußen
  und Österreich. Spielstände 1848 aus M9a laden nicht mehr (Inhalt `diplomatie`); 1648 bitgleich.
- **Bundesversammlung und diplomatische Spiele im Kern (Schritt b2):**
  - Neue Daten `common/bundesversammlung.chron`: Präsidium Österreich, Plenum mit 68 Stimmen, Engerer
    Rat mit 17 Stimmen (Kurien der kleinen Staaten). Befehl `Bundesantrag` für Bundesreform,
    Zolleinigung, Bundesintervention und Präsidium; Abstimmung am nächsten Monatsersten nach den
    Beziehungen zum Antragsteller und zum stärksten Gegner, mit Chronik und Wirkung (Zolleinigung:
    alle Bundesstaaten treten der Union bei).
  - Diplomatische Spiele: Befehl `Forderung` (Führung im Bund, Beitritt zur Zollunion, Bündnis lösen)
    mit Parteinahme der Bundesstaaten, drei Stufen (Protest, Ultimatum, Mobilmachung) zu 30 Tagen,
    Befehl `Manoever` (nachgeben, Vermittlung, eskalieren) und Auflösung nach Stärke; Mobilmachung
    kostet das 1,5-Fache des Heeres.
  - `uhrwerk-cli info` zeigt die Bundesversammlung. Spielstände 1848 aus Schritt b1 laden nicht mehr;
    1648 bitgleich.
- **Diplomatie in Godot (Schritt b3):**
  - Knopf „Diplomatie …“ im Staatspanel (1848) mit einem Fenster in drei Reitern: **Staaten** nach
    Beziehung mit Verträgen, Gesandten und Lager, dazu für den gewählten Staat Gesandte, Bündnis,
    Garantie, Forderungen (mit Rückfrage und den Seiten der Krise) und Bundesintervention;
    **Bundesversammlung** mit Halbrund der Plenarstimmen nach Lagern, offenem Antrag, Anträgen mit
    Prognose und den letzten Abstimmungen; **Deutsche Frage** mit Lagern, Zollverein, Reformen,
    Krisen und Verträgen.
  - Krisenleiste über der Karte mit Stufe, Seiten und den Manövern Nachgeben, Vermittlung anbieten
    und Eskalieren.
  - Kartenmodus „Beziehungen“ (Taste #, fünfzehnter Modus): grün freundlich, rot feindlich zum
    Spielerstaat. Haushaltsposten „Gesandte“, Startoption `--diplomatie[=staaten|bund|frage]`.
  - Brücke: Abfragen und Befehle der Diplomatie (bridge-api.md, Abschnitt Diplomatie).
- **Review und Abschluss M9 (Schritt b4):**
  - Ein Beitritt nach einer angenommenen Zolleinigung des Bundes oder einer verlorenen Krise
    „Beitritt zur Zollunion“ ist jetzt verbindlich. Vorher konnte der betroffene Staat ihn bis zum
    1. Januar zurückziehen. Das Fenster „Zollunion“ zeigt „verbindlich“ und sperrt die Rücknahme
    (Brücke: `customs_request.binding`).
  - Eine Abstimmung oder Krise am 1. Januar kündigte den Beitritt zum selben Tag an, obwohl dessen
    Jahreswechsel schon gelaufen war. Jetzt gilt er zum folgenden 1. Januar, wie er tatsächlich wirkt.
  - Ein Mitglied einer Kurie ohne eigene Plenarstimme zählte im Engeren Rat immer als Nein (1848 ohne
    Folgen).
  - Die Liste der Beziehungen (Fenster, Tooltip im Kartenmodus „Beziehungen“) rechnet die Zielwerte
    in einem Durchgang statt je Staat neu.
  - Godot: Der Knopf des Kartenmodus heißt „Beziehung“ statt „Diplomatie“ (nicht mit dem Fenster zu
    verwechseln); das Fenster „Diplomatie“ blendet Staaten eines anderen Spielstands aus.
  - Doku zum Abschluss: diplomatie.md verbindlich mit Grenzen, Meilensteinplan mit Ergebnis und
    Abnahme. Beide Abnahmekriterien sind erfüllt; 1648 bleibt bitgleich.

### Meilenstein M8 – Politik, Parteien und Wahlen

Im Lesezeichen 1848 wählt der Deutsche Bund am 1. Mai die **Nationalversammlung**: 633 Wahlkreise,
649 Sitze, davon 587 besetzt; die Tschechen in Böhmen boykottieren. Die Sitzverteilung trifft alle
Zielkorridore der Fraktionen, die Abweichungen sind dokumentiert. Der **Wahlabend** inszeniert das
Ergebnis mit Meldungen, Hochrechnung und Halbrund. Im **Parteienmodus** spielt man eine Strömung und
führt bis zur Wahl Wahlkampf. Die Preußische Zweite Kammer von 1849 (Dreiklassenwahlrecht) ist
plausibel. Das Szenario 1648 bleibt bitgleich (`5db13a66d63e6079`).

Alle Werte sind Spielwerte gegen ungeprüfte Richtwerte; die Fachprüfung mit Quellen schließt sich an
die von M7 an. Ergebnis und Grenzen:
[docs/meilensteine.md](docs/meilensteine.md#m8--politik-parteien-und-wahlen-).

- **Vertrag:** [technik/politik.md](docs/technik/politik.md) (neu) mit Umsetzungsstand je Schritt;
  [chronik-skript.md](docs/technik/chronik-skript.md) Abschnitte 3.19–3.24 (Strömungen,
  Interessengruppen, Parlamente, Wahlkreise, Wahlregeln, Wahlziele).
- **Daten (M8a):**
  - 5 Strömungen von den Konservativen bis zur frühen Arbeiterbewegung, 11 Interessengruppen,
    2 Parlamente: Nationalversammlung 1848 mit 8 Fraktionen, Preußische Zweite Kammer 1849 mit 3.
  - Verfassung, Wahlrecht und Wahlsystem der 38 Staaten in `staaten_historie.csv`.
  - `wahlkreise.csv`: 633 Wahlkreise der Nationalversammlung (649 Sitze) und 337 des Preußischen
    Zweiten Kammer (350 Sitze), erzeugt mit `tools/geodaten/wahlkreise_1848.py`.
  - `common/wahlen.chron` mit Wahlregeln, Wahlkampf und 14 Regionen der Wahlgeographie;
    Wahlziele `common/wahlziele_voelkerfruehling.chron`.
- **Kern:**
  - Wahl am Wahltag im Tagesablauf: Wahlberechtigte nach Wahlrecht (allgemein, Zensus, Dreiklassen),
    Beteiligung, Neigung je Block aus Interessengruppen, Bildung, Not, Stadt und Region, Wahlmänner,
    Boykott, Stichwahl, Abgeordnete mit Position und Fraktion ab dem Stichtag; Zufallsstrom `Wahlen`.
  - Parteienmodus: Spenden am Monatsersten, Befehl `Wahlkampf` (Versammlung, Flugblätter, Verein)
    mit abnehmendem Ertrag; Schätzung je Wahlkreis für den Stimmungsbericht.
  - Spielstand Format 2: Wahlen und Wahlkampf im Zustandsverzeichnis (nur mit Parlamenten), Befehle
    mit Ziel und Aktion, Metadaten `spielerstroemung`, Inhaltsbereich `politik`.
- **CLI:** `uhrwerk-cli wahl` (Wahllabor über viele Seeds, mit Wahlzielen, `--parlament`,
  `--wahlkreise`); `info` mit Parlamenten und Wahlkreisen; Testbefehle mit Wahlkampf.
- **Brücke und Godot:**
  - Kartenmodus „Wahlen“ (Taste Ä) mit Wahlkreisgrenzen, Legende und Abschnitt „Wahl“ in der Ortsinfo.
  - Wahlabend am Wahltag: Meldungen in Meldereihenfolge (×1, ×4, sofort), Hochrechnung mit
    Fehlerband, Halbrund nach Strömung oder Fraktion, Telegramme; „Letzte Wahl“ im Spielmenü.
  - Parteienmodus: Rolle im Spielmenü, Parteipanel, Wahlkampf und Stimmungsbericht in der Ortsinfo.
  - Staatsinfo „Verfassung und Gruppen …“: Verfassung, Wahlrecht und Wahlsystem des Landtags, Anteil
    und Einfluss der Interessengruppen (`get_state_politics`, Kern `interest_group_standing`).
  - Startoptionen `--wahlabend`, `--partei` und `--verfassung`.
  - Einheitentest mit **260 Prüfungen** (vorher 228); Kern mit **169 Testfällen** (vorher 153).
- **CI:** CTests `wahl_fixture` und `wahl_1848`, Schritt „Wahllabor 1848/49“, Prüfung der Tabelle
  der Wahlkreise, Screenshots von Wahlabend und Parteienmodus.
- **Review M8** (Kern; Brücke und Godot; Werkzeuge, Daten und Doku):
  - Wahlkreise optimal geschnitten (gleiche Einwohnerzahl, kurze Wege): bis auf Oldenburgs Exklaven
    beim 2/3- bis 1,5-Fachen des Regionsmittels statt 0,2 bis 1,8; größte Ausdehnung 116 statt 232
    km. Alle Wahlziele bleiben erfüllt.
  - Parteienmodus: Wahlkampf zur nächsten ausstehenden Wahl, nach dem 1. Mai 1848 also zur
    Preußischen Zweiten Kammer 1849; eingereihte Befehle zählen als verplant; Knöpfe und Telegramme
    bleiben bei laufender Zeit bestehen; nur in Szenarien mit Parlament.
  - Wahlabend: passt mit acht Fraktionen ins Bild, die stärkste Fraktion ist nie „fraktionslos“,
    Ortsinfo und Legende zeigen vor der Meldung kein Ergebnis; „Letzte Wahl“ setzt das Tempo fort.
  - Lader: Wahltag vor dem Start und leere `boykott_maerkte` sind Fehler; Wahlziele prüfen
    `beteiligung` und doppelte Einträge.
  - Daten: Niederlande, Dänemark, Liechtenstein und Schaumburg-Lippe ständisch; „Preußische Zweite
    Kammer“ statt des erst 1855 eingeführten „Abgeordnetenhauses“; Kurznamen der Strömungen.
  - Doku nach Daten und Code; Archivar lehnt `sitze` mit Nicht-Ziffern ab, statt abzubrechen.

### Meilenstein M7 – Lesezeichen 1848

Das zweite Lesezeichen **„Völkerfrühling“ (1. März 1848)** ist spielbar. Es umfasst den Deutschen
Bund mit 38 Staaten, 3.967 Orte mit Besitzer, Verwaltung und Bevölkerung 1846 (45,0 Mio.
Einwohner), 42 Eisenbahnstrecken, frühe Industrie und Haushalte in Talern. Der Balancing-Bericht
1848 erfüllt alle Zielwerte. Das Szenario 1648 bleibt bitgleich (`5db13a66d63e6079`).

Technisch ist M7 fertig, **die Fachprüfung der Daten mit Quellen steht noch aus** (Entscheidung 7).
Die Quellen waren in der Umgebung nicht erreichbar, deshalb tragen alle historischen Angaben
`ungeprueft` oder `schaetzung`. Ergebnis und Grenzen:
[docs/meilensteine.md](docs/meilensteine.md#m7--lesezeichen-1848-).

- **Vertrag:** [technik/lesezeichen_1848.md](docs/technik/lesezeichen_1848.md) (neu) mit
  Umsetzungsstand je Schritt; [chronik-skript.md](docs/technik/chronik-skript.md) Abschnitte
  3.16–3.18 (Verwaltung, Eisenbahnen, `seit`) sowie Ergänzungen in 3.4, 3.7 und 3.15.
- **Daten (M7a):**
  - 31 neue Staaten und 7 aus 1648 fortgeführte, mit Hauptstadt, Regierungsform, Herrscher,
    Konfession und Zollverein.
  - Zuordnung der Orte über `territorien_1848.csv` (476 Regeln), mit Bundesgebiet.
  - 458 Verwaltungseinheiten: Preußen mit 8 Provinzen, 25 Regierungsbezirken und 327 Kreisen;
    Österreich mit 11 Kronländern und 51 Kreisen; die oberste Ebene der Mittelstaaten.
  - Bevölkerung 1846 mit Konfession, Kultur und Berufen aus Summen je Staat, Provinz und Kronland
    (`einwohner_1846.csv`, `staedte_1846.csv`, `merkmale_1846.csv`).
- **Daten (M7b):**
  - `eisenbahnen.csv`: 176 Abschnitte, zum Start 165 eröffnet (rund 4.700 km).
  - Ab 1800 Kohle und Eisen; Betriebe Zeche, Hütte, Spinnerei, Maschinenbau, Brennerei und
    Salzwerk; Beruf Fabrikarbeiter.
  - Vorkommen an 34 Kohle-, 19 Erz-, 30 Salz- und 43 Weinorten.
  - Hof, Schatz und Schulden 1848 im Maßstab des Modells.
  - Zielwerte `zielwerte_voelkerfruehling.chron`.
- **Werkzeuge:**
  - `tools/geodaten/zuordnung.py`, `bevoelkerung_1846.py` und `stichprobe_1848.py`.
  - Archivar mit `verwaltung.csv` und `eisenbahnen.csv` (Bahnhöfe über den nächsten passenden Ort
    oder eine Kennung) und mit `spielbar`; Staaten ohne Ort zum Startdatum bestehen nicht.
  - `--bericht` mit Verwaltungseinheiten, Strecken und Szenarien.
- **Kern:**
  - Verwaltung (Einheiten, Kette, Sitz), Bundesgebiet und Zollverein je Ort und Staat;
    spielbare Staaten, Währung und Szenarioliste.
  - Eisenbahnen mit Regel `bahnfracht`: Nachbarmärkte im selben Netz handeln ab dem Tag der
    Verbindung zur Bahnfracht.
  - `seit` für Güter, Berufe und Betriebe: Späterer Inhalt fehlt in Welt, Fingerabdruck und
    Prüfsumme.
  - `anteil_aus` für Berufe.
  - Szenariofeld `tragfaehigkeit_faktor` (Agrartechnik; 1848: 2,5).
  - Szenarien werden vor `common/` gelesen.
  - **153 Testfälle** (vorher 146).
- **CLI:**
  - `--szenario voelkerfruehling` in allen Befehlen.
  - `info` mit Eisenbahnen (Strecken, Abschnitte, Verbindungen mit Bahnfracht).
  - `bilanz` und `pruefen` mit Zielwerten je Szenario (`zielwerte_<szenario>.chron`).
- **Brücke und Godot:**
  - „Neues Spiel“ mit Wahl des Lesezeichens; „Diesen Staat spielen“ in der Ortsinfo.
  - Kartenmodi „Verwaltung“ (Taste Ü) und „Eisenbahnen“ (Taste +, Strecken über der Karte).
  - Ortsinfo mit Verwaltungskette, Bundesgebiet und Bahnhof; Staatsinfo mit Bund und Zollverein.
  - Startoptionen `--szenario` und `--staat`.
  - Beim Szenariowechsel werden Ortskatalog, Farben und Ladeliste neu aufgebaut.
  - `unit_test.gd` mit **228 Prüfungen** (vorher 217); Rauchtest mit den Eisenbahnen 1648 und 1848.
- **CI:**
  - CTests `daten_pruefen_1848`, `determinismus_1848` und `speichertest_1848`.
  - Der Determinismus-Job vergleicht auch die Prüfsumme 1848.
  - Der Balancing-Job hat einen zweiten Bericht für 1848 (8 Seeds, 30 Jahre).
  - Screenshot 1848.
- **Review M7b:**
  - Das Bahnnetz verbindet Orte, nicht Märkte. Zwei Strecken, die nur im selben Markt enden, gelten
    nicht mehr als verbunden; 1848 sind 55 Paare von Nachbarmärkten verbunden.
  - Die Landfracht wird wieder in der dokumentierten Reihenfolge gerundet.
  - `anteil_aus` hängt nicht mehr von der Reihenfolge in der Datei ab; Kreise sind ein Fehler.
  - Spätere Definitionen werden vollständig geprüft (Kennungen, Verweise). `bahnfracht` darf nicht
    über `handel_fracht` liegen.
  - Karte: „Markt mit Bahnfracht“ folgt dem Kern (`get_rail_access`). Gestrichelt werden nur
    Abschnitte der nächsten fünf Jahre, 1648 also keine; die Linien werden täglich aktualisiert.
  - Der Archivar meldet doppelte Abschnitte auch über Strecken hinweg und gleiche Streckennamen.
  - Spielstände aus M6 sind mit M7 nicht mehr ladbar, weil sich der Inhaltsfingerabdruck geändert hat.
- **Stichprobe:** [pruefung/stichprobe_1848.md](docs/pruefung/stichprobe_1848.md) mit 60
  Datensätzen. Davon sind 46 plausibel, 6 Vereinfachungen und 2 zweifelhaft; 6 Fehler sind behoben.
  Die Flächen der großen Staaten liegen innerhalb ±5 % der Richtwerte.

### Meilenstein M6 – Speicherstände und Wiederholungen

Partien lassen sich speichern, laden und **bitgleich fortsetzen**, und aus Startzustand und
Befehlsprotokoll lässt sich jede Partie **exakt wiederholen** – geprüft über 50 Jahre mit 200
Testbefehlen in allen drei Compiler-Jobs. Damit ist die Vorproduktion (Phase 1) abgeschlossen.
Ergebnis und bekannte Grenzen:
[docs/meilensteine.md](docs/meilensteine.md#m6--speicherstände-und-wiederholungen-).

- **Vertrag:** [technik/speicherstaende.md](docs/technik/speicherstaende.md) (neu, verbindlich):
  Zustandsverzeichnis, Inhaltsfingerabdruck, Prüfungen beim Laden, Dateiformat v1,
  Befehlsprotokoll und Wiederholung, Versionen, Kommandozeile, Godot, Abnahme mit Messwerten.
  [bridge-api.md](docs/technik/bridge-api.md) ergänzt.
- **Kern:**
  - Zustandsverzeichnis (`core/src/state_access.hpp`): eine Liste aller veränderlichen Felder für
    Prüfsumme, Textexport (`Simulation::state_text`), Schreiben und Lesen; Referenzprüfsummen
    unverändert.
  - Spielstände (`uhrwerk/savegame.hpp`): 48-Byte-Kopf, Metadaten als Text, Nutzdaten mit zstd
    1.5.6 (per `FetchContent`, statisch); Inhaltsfingerabdruck je Bereich; Laden in frisch
    geladene Spieldaten mit Prüfung aller Indizes und Größen und Gegenprobe der Prüfsumme;
    gleicher Zustand ergibt dieselben Bytes; atomares Schreiben über eine Nebendatei.
  - Befehlsprotokoll (`Simulation::command_log`, beim Einreihen, mit wartenden Befehlen) und
    Startprüfsumme; Wiederholung mit jährlichen Kontrollpunkten, die den ersten abweichenden Tag
    meldet; Protokolldatei `.protokoll`.
  - Neuer Zufallsstrom `Testbefehle = 6` (nur Kommandozeile).
  - **146 Testfälle** (vorher 136), u. a. Weiterspielen an 17 Speicherpunkten mit 1 und 4 Threads,
    beschädigte und fremde Dateien, Formatwache, Wiederholung mit abweichendem Befehl.
- **CLI:** `lauf --speichern/--laden/--protokoll/--testbefehle`, `speichertest`, `wiederholung`,
  `spielstand [--text]`, `lasttest --speichern`.
- **Brücke und Godot:** `save_game`, `load_game`, `get_save_info`, `get_save_format_version`;
  Spielmenü (Knopf „Menü“, F10) mit Speichern unter einem Namen und Laden aus der Liste,
  Schnellspeichern (F5) und -laden (F9), automatischer Spielstand am 1. Januar (drei bleiben),
  Startoption `--spielmenue`. `unit_test.gd` mit **217 Prüfungen** (vorher 187); Rauch- und
  Klicktest mit Spielständen.
- **CI:** CTests `speichertest_50_jahre`, `protokoll_50_jahre`, `wiederholung_50_jahre`,
  `wiederholung_spielstand`, `spielstand_laden`, `spielstand_fremder_inhalt`; der
  Determinismus-Job vergleicht zusätzlich den Prüfwert eines Spielstands nach 100 Jahren
  (`17078dcdeecde792`); Lasttest-Job mit Spielstand; vierter Screenshot (Spielmenü).
- **Behoben:** MSVC brach mit `/WX` an unerreichbarem Code im Zustandsverzeichnis ab (C4702); das
  Datum der Metadaten wird ohne `sscanf` gelesen.
- **Review M6:** Kurzname der Staaten und Ortsname gehören zum Inhaltsfingerabdruck (sie stehen in
  der Chronik und damit in der Prüfsumme); Feldzahl statt Größe von `Rules` als Erinnerung an den
  Fingerabdruck; Laden prüft ungerade Inkremente der Zufallsströme; Wiederholung lehnt fremde
  Formatversionen ab; reservierte Namen im Spielmenü; am Jahreswechsel gehen keine Tage mehr
  verloren; je eine Umsetzung von FNV-1a, ISO-Datum und Hex-Darstellung.

### Meilenstein M5 – Wirtschaft und Staatshaushalt

Einkommen, Preise und Steuern entstehen jetzt aus einem Wirtschaftskreislauf mit Gütern, Betrieben
und regionalen Märkten, und die Staaten führen einen Haushalt mit Hof, Verwaltung, Heer, Zinsen und
Bauten, mit Kredit und Staatsbankrott. **Über 100 Jahre und 32 Seeds bleibt jeder Staat ohne
Bankrott bei etwa einer Jahreseinnahme Vermögen.** Ergebnis und bekannte Grenzen:
[docs/meilensteine.md](docs/meilensteine.md#m5--wirtschaft-und-staatshaushalt-).

- **Vertrag:** [technik/wirtschaft.md](docs/technik/wirtschaft.md) (neu, verbindlich): Güter,
  Bedürfnisse, Betriebe, Märkte und Handel, Ortsrechnung, Staatshaushalt, Ernte, Einschwingen,
  Balancing-Bericht, Abnahme mit Messwerten. [chronik-skript.md](docs/technik/chronik-skript.md)
  Abschnitte 3.11–3.15, [bridge-api.md](docs/technik/bridge-api.md),
  [tabellen.md](docs/technik/tabellen.md) ergänzt.
- **Kern:**
  - Wirtschaft (`uhrwerk/economy.hpp`): Märkte am Monatsersten mit Preisen aus Angebot und
    Nachfrage, Handel mit Nachbarmärkten und über Häfen mit dem Weltmarkt; Ortsrechnung am
    Rechentag (parallel) mit Erzeugung, Grundrente und Zehnt, Kontribution, Verbrauch nach
    Warenkorb, Akzise und Zoll, Bedarfsdeckung und Betriebsanteilen; Erntefaktor je Markt und
    Jahr (Strom Wirtschaft); Einschwingen über 60 Monate beim Laden. Hunger erhöht die
    Sterblichkeit, der Lebensstandard folgt dem Realeinkommen.
  - Staatshaushalt am Rechentag mit Kredit, Tilgung, Zins nach Verschuldung, Staatsbankrott
    (Chronik), Bauten und Landesausbau; Staatsnachfrage auf dem Markt der Hauptstadt.
    Steuerbefehl mit Steuerart (`Command::kind`, ungültige Arten werden verworfen).
  - Balancing-Bericht (`uhrwerk/balance.hpp`): Zielwerte lesen, Läufe messen, Median und
    Quantile in Festkomma, Verstöße.
  - **136 Testfälle** (vorher 123), u. a. Ortsrechnung, Markt, Handel und Haushalt exakt gegen
    Handrechnung, Bericht und Zielwerte.
- **CLI:** `uhrwerk-cli wirtschaft` (Güter, Berufe, Haushalt je Staat, `--maerkte`, `--jahre`),
  `uhrwerk-cli bilanz` (`--seeds`, `--jahre`, `--ziele`; Exitcode 1 bei Verstoß); `pruefen` prüft
  die Zielwerte mit; `lauf` zeigt Lebensstandard und Alphabetisierung, `--bericht` die Schulden.
- **Daten:** `common/gueter.chron` (11), `beduerfnisse.chron` (6), `betriebe.chron` (12),
  `maerkte.chron` (77), `zielwerte.chron`; Agrarabgaben der Berufe; Kontribution, Akzise, Zoll,
  Heer und Schulden je Staat; Vorkommen und Häfen je Ort; Hofhaltung neu bemessen; Regelwerte
  für Preise, Handel, Ernte, Hunger, Haushalt und Kredit. Das Feld `ertrag` der Berufe entfällt.
- **Archivar:** neue Spalten und Felder mit Prüfungen; `orte.py` behält die Spalten `vorkommen`
  und `hafen`.
- **Brücke und Godot:** `set_tax_percent` mit Steuerart, Haushalt in `get_state`, Wirtschaft in
  `get_location`, `get_market*`, Kartenmodi 8 „Wirtschaft“ (Taste O) und 9 „Märkte“ (Taste P) mit
  Marktgrenzen; drei Steuerregler und Haushaltsfenster im Staatspanel, Abschnitt Wirtschaft in der
  Ortsinfo, Startoption `--haushalt`. `unit_test.gd` mit **187 Prüfungen** (vorher 179).
- **CI:** neuer Job „Balancing (Linux, Release)“ (16 Seeds, 100 Jahre); CTests `bilanz_fixture`
  und `bilanz_verstoss`; dritter Screenshot (Märkte mit Haushaltsfenster).
- **Referenz-Prüfsumme** nach 100 Jahren jetzt `5db13a66d63e6079` (vorher `ed6b14a02cfbfa5d`),
  Bevölkerung 713.456; Lasttest-Referenz `8602a1273080ee9e` (vorher `a5a4c0d80a2f23ea`).

### Meilenstein M4 – Bevölkerung und Lasttest

Die Bevölkerung entsteht jetzt aus Geburten, Sterbefällen, Altern und Wanderung statt aus einem
festen Wachstum, und der Kern rechnet **500.000 Bevölkerungsblöcke in 0,33–0,40 s je Spieljahr**
(4 Kerne, Release) – bitgleich bei jeder Zahl von Threads und unter GCC, Clang und MSVC. Ergebnis
und bekannte Grenzen: [docs/meilensteine.md](docs/meilensteine.md#m4--bevölkerung-und-lasttest-).

- **Vertrag:** [technik/bevoelkerung.md](docs/technik/bevoelkerung.md) (neu, verbindlich):
  Datenmodell, Ablauf eines Tages, alle Formeln in Rechenreihenfolge, Regelwerte und
  Kalibrierung, Mehrkernberechnung, Lasttest, Abnahme mit Messwerten.
  [chronik-skript.md](docs/technik/chronik-skript.md) Abschnitte 3.1–3.6 und 3.10 (Kulturen,
  neue Felder), [bridge-api.md](docs/technik/bridge-api.md), [tabellen.md](docs/technik/tabellen.md)
  ergänzt.
- **Kern:**
  - Bevölkerungsblock mit Kultur, vier Altersgruppen, Lebensstandard und Alphabetisierung
    (64 Byte); `World::cultures`, Zugänge je Ort, Momentaufnahmen, Jahresbilanzen und
    Ortssteuern als neuer Zustand (alle in der Prüfsumme).
  - Bevölkerungsmodell (`uhrwerk/population.hpp`): Tragfähigkeit und Landreserve, Lebensstandard
    aus dem Netto-Einkommen, Geburten und Sterbefälle nach Lebensstandard, Altern, Bildung,
    Wanderung der Jungen (bis zu 4 Nachbarorte im Umkreis von 60 km und die nächste Stadt),
    Auswanderung, monatliche Neuordnung der Blöcke. Die Felder `wachstum_*` entfallen.
  - Mehrkernberechnung mit eigenem Thread-Pool (`uhrwerk/parallel.hpp`): Orte eines Tages und das
    Buchen der Wanderung je Zielort laufen parallel; feste Aufteilung und feste Reihenfolge beim
    Zusammenführen. `SimConfig::threads`.
  - Bitgleiche Schnellwege in `Fixed` (Knuth-Division, 64-Bit-Multiplikation), Rechentag-Listen,
    Steuern je Ort: 18 statt 450 ns je Block und Monat mit dem Modell aus M1.
  - Lasttest-Welt `make_load_test_world` (`uhrwerk/lasttest.hpp`, Zufallsstrom `Lasttest = 5`).
  - **123 Testfälle** (vorher 101), u. a. Formeln gegen Nachrechnung, exakte Buchführung,
    gleiche Prüfsummen mit 1–8 Threads, Kalibrierung mit den echten Daten.
- **CLI:** `uhrwerk-cli lasttest` (`--bloecke`, `--jahre`, `--threads`, `--vergleich`,
  `--grenze`, `--seed`); `--threads` für `lauf` und `determinismus`; `lauf` zeigt die Bevölkerung
  und die Jahresbilanz in ‰; `determinismus` rechnet den zweiten Lauf mit einem Thread.
- **Daten:** 38 Kulturen in `common/kulturen.chron`; Spalte `kultur` in `dateien.csv`
  (Standard je Landschaft) und `bevoelkerung.csv` (Anteile je Ort); Tragfähigkeit je Geländetyp,
  Bildungsfaktor je Konfession, Agrar-, Bildungs- und Stadtberuf je Beruf; kalibrierte
  Regelwerte (1648 → 1700 ×1,29, → 1816 ×2,33).
- **Archivar:** Kulturen und Kulturanteile mit Prüfungen; **35 Tests** (vorher 33). `orte.py`
  erhält die Spalte `kultur`.
- **Brücke und Godot:** `get_location` mit Kultur, Altersgruppen, Lebensstandard,
  Alphabetisierung und Bilanz des Vorjahres; `get_state` mit Lebensstandard und Bilanz;
  Kartenmodi 6 „Lebensstandard“ (Taste U) und 7 „Kultur“ (Taste I); Ortspanel mit Kulturen,
  Gesellschaft, Altersaufbau und Bilanz; Kartenleiste mit Kurznamen. `unit_test.gd` mit
  **179 Prüfungen** (vorher 170).
- **CI:** Determinismus vergleicht zusätzlich die Prüfsumme des Lasttests (50.000 Blöcke,
  2 Jahre); neuer Job „Lasttest (Linux, Release)“ mit 500.000 Blöcken, Grenze 3 s und Vergleich
  mit einem Thread; CTest `lasttest_threads`.
- **Referenz-Prüfsumme** nach 100 Jahren jetzt `ed6b14a02cfbfa5d` (vorher `40de8c42e8f7a0ff`),
  Bevölkerung 642.668; Lasttest-Referenz `a5a4c0d80a2f23ea`.

### Meilenstein M3 – Karte des Kernraums

Die Karte zeigt den ganzen Kernraum mit **4.542 Orten in 626 Kreisen**. Historische Daten haben
weiterhin nur die 60 Orte der Testregion; die übrigen 4.482 sind *Orte ohne Daten*. Ergebnis und
bekannte Grenzen: [docs/meilensteine.md](docs/meilensteine.md#m3--karte-des-kernraums-).

- **Verträge:** [chronik-skript.md](docs/technik/chronik-skript.md) Abschnitt 6 in Fassung M3
  (Plattkarte mit Bezugsbreite, GeoJSON-Geometrie für Land, Seen, Flüsse und Kernraum,
  Flächenart im Blaukanal von `provinzen.png`, Flussebene `fluesse.png`, `definition.csv` mit
  Beschriftungspunkt, Pixelzahl und Fläche), Abschnitt 3.6 *Orte ohne Daten* und 3.9 *Kreise*;
  [bridge-api.md](docs/technik/bridge-api.md) und [tabellen.md](docs/technik/tabellen.md) ergänzt.
- **Kern:** Orte ohne Daten – die vier Simulationsfelder (`besitzer`, `bevoelkerung`,
  `konfession`, `verwuestung`) sind zum Startdatum alle vorhanden oder keines; ohne sie hat ein Ort
  keinen Besitzer und keine Bevölkerungsblöcke und wird beim Jahreswechsel übersprungen (zieht
  keine Zufallszahlen). Hauptstädte müssen Orte mit Daten sein. Kreise aus `history/kreise/`
  (`World::districts`, `Location::district`, Feld `kreis` im Ort, nicht datierbar). `uhrwerk-cli`
  zeigt Orte mit und ohne Daten, Kreise und die Ladezeit. **101 Testfälle** (vorher 91).
- **Brücke:** `has_data`, `kreis_index` und `kreis_name` in `get_location`; gepackte Indexarrays
  `get_location_owner_indices` und `get_location_kreis_indices`; `get_kreis_count`,
  `get_kreis_index`, `get_kreis`; Kartenmodus 5 „Kreise“ (Palette mit 12 Farben); Alpha 0 für
  Orte ohne Daten in den Modi Politisch, Dichte, Konfession und Verwüstung.
- **Archivar:** Spalte `kreis` in `orte.csv`, neue Tabelle `kreise.csv` → `history/kreise/`;
  Orte ohne Zeilen in `besitz.csv` und `bevoelkerung.csv` sind Orte ohne Daten; Bericht mit Orten
  mit und ohne Daten und Kreisen. **33 Tests** (vorher 25).
- **Geodaten** (`tools/geodaten/`, neu, Python mit numpy):
  - `geodaten.py holen` lädt die offenen Quellen nur in den Zwischenspeicher
    `build/geodaten-cache/` und prüft sie gegen die SHA-256-Werte in `quellen.json`: Natural Earth
    5.1.2 (1:10 Mio., gemeinfrei), GeoNames `cities1000` über das npm-Paket
    `cities-with-1000@1.0.4` (CC BY 4.0), 224 Höhenkacheln der AWS Terrain Tiles (Zoomstufe 8).
    Quellenangaben: [Anhang C](docs/anhang/c_quellen_und_daten.md#5-verwendete-geodaten-stand-m3).
  - `geodaten.py geometrie` schneidet Land, Seen und Flüsse auf den Kartenausschnitt zu
    (`data/karte/geometrie/`, zusammen 0,26 MB; byte-gleich unter Python 3.10 und 3.13);
    `pruefen` vergleicht mit den Quellen, `vorschau` zeichnet eine Prüfkarte.
  - `data/karte/geometrie/kernraum.geojson` von Hand: 398 Punkte, jede Grenzentscheidung
    begründet, 93 Prüforte mit erwarteter Lage.
  - `orte.py` erzeugt die Orte und Kreise des Kernraums: aus 14.404 Kandidaten 4.482 neue Orte
    (gewichtete Poisson-Scheibe nach Bedeutung, Inseln, Lückenfüllung), 626 Kreise mit 4–12 Orten
    (Median 7) in 77 Landschaften; deutsche Namen nur mit Beleg in den GeoNames-Alternativnamen
    (978 belegte Einträge in `exonyme.csv`), Gelände aus dem Höhenmodell, Fläche aus der Karte.
  - **96 Tests** ohne Netz und ohne Zwischenspeicher (ein Volltest mit `ORTE_VOLLTEST=1`).
- **Kartengenerator v2** (jetzt mit numpy): Rasterung der Polygone nach der Pixelmitte,
  Flächenarten Kernraum-Land, Meer, Binnengewässer und Ausland, Zuordnung zum nächsten Ort mit
  deterministischem Rauschen (nur Grundrechenarten), genau eine zusammenhängende Fläche je Ort,
  Flussebene, Beschriftungspunkt, Fläche in Festkomma, Vorschau mit Ausschnitt und Namen. Die
  volle Karte (4096 × 3329 Pixel, 4.542 Orte) entsteht in 13–21 s; Kernraum-Land ist zu
  99,96 % Orten zugeordnet. Ausgabe byte-gleich unter Python 3.10 (numpy 2.2.6) und 3.13 (numpy
  2.5.3). **88 Tests.**
- **Daten:** 4.542 Orte (60 mit, 4.482 ohne Daten) in 77 Orts- und 77 Kreisdateien; neuer
  Geländetyp `hochgebirge` (jetzt 11); `karte.chron` in Fassung M3 (1,8–23,5 °O × 45,1–56,2 °N,
  4096 × 3329 Pixel, etwa 0,37 km je Pixel, `max_radius_km = 32`). `flaeche` aller Orte kommt aus
  der Karte (1–1.147 km², Median 169 km²); die 60 Orte der Testregion sind sonst unverändert.
- **Godot:** Großkarte mit Meer, Seen, Ausland und Flüssen, Schraffur „Keine Daten“, Staats-,
  Kreis- und Ortsgrenzen nach Zoomstufe; Namen nach Zoomstufe (Staaten fern, Kreise mittel, Orte
  nah) ohne Überlappung; Kartenmodus „Kreise“ (Taste Z); Ortsinfo und Tooltip für Orte ohne
  Daten; Startoption `--zoom`. Tests: `unit_test.gd` (170 Prüfungen), `smoke_test.gd` mit den
  Kartendaten v2, neu `klick_test.gd` (jeder der 4.542 Orte in drei Zoomstufen richtig erkannt,
  dazu rund 120 Klicks über echte Eingabeereignisse und Prüfungen der Namen an den Panels) und
  `leistung_test.gd`.
- **CI:** Job „Python-Werkzeuge“ mit numpy (2.2.6 unter Python 3.10, 2.5.3 unter 3.13) und den
  Tests der Geodaten; Godot-Job zusätzlich mit Einheiten- und Klicktest, alle Protokolle werden
  auf Skriptfehler geprüft; zweiter Screenshot im Kartenmodus „Kreise“ (mittlere Zoomstufe).
- **Referenz-Prüfsumme** nach 100 Jahren jetzt `40de8c42e8f7a0ff` (vorher `8d44cf792e143bb0`):
  Der Rechentag eines Ortes hängt von seinem Index ab (Index mod 28 + 1), und durch die
  eingefügten Orte ohne Daten haben die meisten der 60 Orte mit Daten einen anderen Rechentag.
  Bevölkerung nach 100 Jahren 627.680 statt 627.721. GCC, Clang und ASan liefern dieselbe
  Prüfsumme. Kreise gehören bewusst nicht zur Prüfsumme (unveränderlicher Inhalt).
- **Korrekturen nach dem Review von M3:**
  - Kern: Daten nur noch streng als `JJJJ.MM.TT` (genau 4, 2 und 2 Ziffern), wie im Archivar und
    in der Spezifikation; `1701.1.18` ist jetzt ein Ladefehler.
  - Archivar: Zeilenumbruch (LF oder einzelnes CR) in Namen, Kurznamen, Herrschern und Quellen
    ist ein Fehler (der Kern lehnt ihn in Zeichenketten ab); Jahr `0000` ist ungültig; eine
    Fläche, die im Festkomma des Kerns 0 wird (unter etwa 0,0000076 km²), ist ein Fehler statt
    einer Warnung.
  - Geodaten: neue Hilfstabelle `einwohner.csv` für Verwaltungssitze, die GeoNames mit 0
    Einwohnern führt (Osterode am Harz und Avesnes-sur-Helpe werden nicht mehr von Gittelde
    bzw. Avesnelles verdrängt); Orte im Trentino entfallen (`AUSLAND_GEBIETE`, statt
    Castelfondo jetzt Laurein in Deutschnonsberg); „ Markt“ und „ Stadt“ bleiben, wenn sie zum
    Namen gehören (Stetten am Kalten Markt); 26 neue belegte Namen in `exonyme.csv` (Meißen,
    Großarl, Straßwalchen, Großpetersdorf, Laßnitzhöhe sowie deutsche Namen wie Horitz,
    Eibenschitz, Mariensee, Laurein); die Lückenfüllung bricht auf Land fern von jedem Ort
    nicht mehr mit `OverflowError` ab; der Fluss Mies/Beraun heißt nicht mehr „Angel“.
  - Godot: Namen werden neu gesetzt, wenn ein Panel Größe, Lage oder Sichtbarkeit ändert
    (keine Namen mehr unter Legende und Ortsinfo); der Kreisname bleibt, wenn sein
    gleichnamiger Hauptort ausgewählt ist; eine ausgewählte Hauptstadt behält ihren Stil; die
    Legende „Kreise“ zählt nur Kreise mit Orten; ohne Kreisdaten zeigt die mittlere Stufe
    Ortsnamen.
  - Die Referenz-Prüfsumme des ersten M3-Stands (`5548a5fa80f75c0d`, 4.541 Orte) ändert sich
    dadurch auf `40de8c42e8f7a0ff`: Osterode am Harz in `goettingen` verschiebt die Indizes der
    38 Orte mit Daten in den Dateien `lausitz` bis `thueringen`.

### Meilenstein M2 – Datenpipeline „Archivar“

- **Datierte Blöcke** im Chronik-Skript: `JJJJ.MM.TT = { … }` in `staat` und `ort` gilt ab seinem
  Datum; der Lader liest die Szenarien vor der Historie und wendet alle Blöcke bis zum Startdatum
  an (Spezifikation Abschnitt 3.8, 4 neue Testfälle).
- **Tabellen** `data/tabellen/*.csv` als Quelle der Wahrheit für Orte und Staaten: Stammdaten,
  Besitz, Bevölkerung je Stichtag, Staatshistorie, Kopfkommentare. Jede Zeile mit Geschichte trägt
  Quelle und Sicherheit (`gesichert`, `schaetzung`, `spielwert`, `ungeprueft`). Format:
  [`docs/technik/tabellen.md`](docs/technik/tabellen.md).
- **Archivar** (`tools/archivar/`): prüft Verweise, Lücken zu allen Szenariodaten und Ausreißer,
  erzeugt `data/history/` deterministisch, `--pruefen` für die CI, `--bericht` für die Übersicht;
  25 Tests mit eigener Fixture.
- Die Testregion 1648 ist verlustfrei in die Tabellen umgezogen (Prüfsumme nach 100 Jahren
  unverändert); die Kopfkommentare der Dateien sind erhalten.
- Kartengenerator: Die Vorschau liest den Besitzer aus datierten Blöcken.
- CI: Job „Archivar und Kartengenerator“ mit Tests und Aktualitätsprüfung.

### Meilenstein M1 – technischer Durchstich

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
  [`docs/technik/README.md`](docs/technik/README.md), dieses Änderungsprotokoll,
  Meilensteinplan [`docs/meilensteine.md`](docs/meilensteine.md) (M1–M13).

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
