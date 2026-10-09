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
| M2 | Datenpipeline „Archivar“ | 1 – Vorproduktion | M1 | offen |
| M3 | Karte des Kernraums | 1 – Vorproduktion | M2 | offen |
| M4 | Bevölkerung und Lasttest | 1 – Vorproduktion | M3 | offen |
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

### M2 – Datenpipeline „Archivar“

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

### M3 – Karte des Kernraums

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

### M4 – Bevölkerung und Lasttest

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
  Mittelklasse-Rechner (Zielwert, wird mit M4 festgelegt)
- Prüfsummen bleiben auf GCC, Clang und MSVC bitgleich, auch bei unterschiedlicher Kernanzahl

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
