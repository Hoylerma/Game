# Lesezeichen 1848 „Völkerfrühling“ (Meilenstein M7)

> **Status: in Umsetzung** – Entscheidungen 1–7 wie empfohlen freigegeben; M7a Schritte a1–a5
> umgesetzt, Stand in Abschnitt 13
> ([meilensteine.md](../meilensteine.md#m7--lesezeichen-1848)). Grundlage sind
> [04 Zeitrahmen, Epochen, Startdaten](../04_zeitrahmen_epochen_startdaten.md) (Lesezeichen 6),
> [05 Karte und Geographie](../05_karte_und_geographie.md) (Abschnitte 2 und 8),
> [06 Staaten und Nationen](../06_staaten_und_nationen.md),
> [07 Bevölkerung und Gesellschaft](../07_bevoelkerung_und_gesellschaft.md),
> [08 Wirtschaft und Finanzen](../08_wirtschaft_und_finanzen.md) und
> [20 Verantwortung und Erinnerungskultur](../20_verantwortung_und_erinnerungskultur.md).

Das zweite Lesezeichen beginnt am **1. März 1848**: Deutscher Bund, Vormärz, Zollverein, die ersten
Eisenbahnen, Pauperismus – wenige Tage vor der Märzrevolution. M7 liefert die **Daten** dafür
(Staaten, Grenzen, Verwaltung, Bevölkerung, Wirtschaft, Eisenbahnen) und das Wenige an Modell, das
sie braucht. Politik, Diplomatie, Militär und Ereignisse folgen in M8–M11.

## 1. Umfang

Der Meilensteinplan verlangt:
- Deutscher Bund mit allen Bundesstaaten; spielbar: Preußen, Österreich, Bayern, Sachsen
- Verwaltungsgliederung 1815–1866 (Provinzen, Regierungsbezirke, Kreise)
- Bevölkerung, Wirtschaft und Eisenbahnen um 1848 aus der Datenpipeline (M2)
- Abnahme: `uhrwerk-cli pruefen` ohne Fehler, jeder Datensatz mit Quelle; historische
  Stichprobenprüfung durch den Beirat oder eine dokumentierte Fachprüfung

Gegenüber M5 und M6 ist M7 vor allem **Datenarbeit**: Heute haben 60 Orte Daten (Testregion 1648),
für 1848 brauchen etwa 3.800 Orte Besitzer und Bevölkerung. Vorschlag (Entscheidung 6): **zwei
Teile**.

| Teil | Inhalt | Ergebnis |
|---|---|---|
| **M7a** | Staaten 1848, Zuordnung aller Orte zu Staaten, Verwaltungsgliederung, Bevölkerung mit Konfession, Sprache und Berufen; Szenario `voelkerfruehling`; Staatswahl in Godot | spielbares Lesezeichen mit dem Wirtschaftsmodell aus M5 |
| **M7b** | Eisenbahnen 1848 als Modell und Daten, frühe Industrie (Kohle, Eisen, Fabriken), Haushalte der Staaten in Talern, Kalibrierung und Balancing-Bericht für 1848 | Wirtschaft um 1848 im Gleichgewicht |

## 2. Ausgangslage

**Vorhanden:** datierte Historie in den Tabellen (`besitz.csv`, `bevoelkerung.csv`,
`staaten_historie.csv` mit Stichtag, Quelle und Sicherheit je Zeile), Szenarien mit eigenem Datum,
4.542 Orte des Kernraums mit Lage, Fläche, Gelände und Landschaft; 626 räumlich gebildete Kreise
(keine historischen Einheiten); 77 Märkte; Bevölkerungs- und Wirtschaftsmodell; Spielstände.
Der Kern lädt ein Szenario mit dem jüngsten Stand je Feld zum Szenariodatum und braucht für 1848
keinen Umbau.

**Es fehlt:** Staaten nach 1806, Besitz und Bevölkerung von 1848, eine Verwaltungsebene über den
Orten, Eisenbahnen, Industrie, die Wahl von Szenario und Staat beim Start.

**Netzzugang dieser Arbeitsumgebung:** Erreichbar sind nur GitHub, PyPI und npm. Die Quellen, die
für Grenzen und Volkszählungen in Frage kommen, sperrt die Netzwerkregel der Umgebung (Proxy 403):
`de.wikipedia.org`, `www.wikidata.org`, `query.wikidata.org`, `planet.openhistoricalmap.org`
(OpenHistoricalMap; das Verzeichnis auf S3 ist lesbar, die Dateien nicht), `gov.genealogy.net`,
`www.hgis-germany.de`. Siehe Entscheidung 1.

## 3. Quellen

| Bedarf | Quelle (Lizenz) | Nutzung |
|---|---|---|
| Staaten, Herrscher, Regierungsform, Hauptstädte 1848 | Standardliteratur zum Deutschen Bund; Wikidata (CC0) zur Prüfung | Tabellen `staaten*.csv` |
| Zugehörigkeit der Orte | heutige Verwaltungseinheiten aus GeoNames (CC BY 4.0, schon im Zwischenspeicher von M3) plus Zuordnungstabelle; OpenHistoricalMap (CC0) zur Prüfung der Grenzen, soweit erfasst | Abschnitt 5 |
| Verwaltungseinheiten 1848 | Wikidata (Kreise und Regierungsbezirke mit Anfangs- und Enddatum, Kreisstadt); Literatur zur Verwaltungsgeschichte | Abschnitt 6 |
| Bevölkerung um 1846/1849 | Zählungen des Zollvereins (Dezember 1846, 1849), Preußen 1846/1849, Österreich 1846; Sammlung: A. Kraus (Bearb.), *Quellen zur Bevölkerungsstatistik Deutschlands 1815–1875* (Boppard 1980), Kreisebene | Abschnitt 7 |
| Konfession, Sprache | dieselben Zählungen; für Sprachgebiete (Polnisch, Sorbisch, Tschechisch, Dänisch, Litauisch, Slowenisch) Sprachkarten und Literatur | Anteile je Ort |
| Eisenbahnen | Eröffnungsdaten der Strecken bis 1848 (Literatur zur Eisenbahngeschichte; Wikidata) | Abschnitt 8 |

Alle Angaben tragen wie bisher **Quelle** und **Sicherheit** (`gesichert`, `schaetzung`, `spielwert`,
`ungeprueft`, [tabellen.md](tabellen.md#allgemeine-regeln)). Ohne Zugriff auf die Quellen
(Entscheidung 1) bleibt eine Zahl aus der Literaturkenntnis `ungeprueft` bzw. `schaetzung`, und die
Quelle nennt, wogegen sie zu prüfen ist – sie behauptet keine Einsicht, die nicht stattfand.

## 4. Staaten 1848

**Bundesstaaten** (38 Einträge; Holstein mit Lauenburg und Luxemburg mit Limburg je als einer gezählt, die Zählweise der Bundesglieder schwankt in der Literatur): Österreich, Preußen, Bayern, Sachsen, Hannover, Württemberg, Baden,
Kurhessen, Großherzogtum Hessen, Holstein und Lauenburg (König von Dänemark als Herzog),
Luxemburg und Limburg (König der Niederlande als Großherzog bzw. Herzog), Braunschweig,
Mecklenburg-Schwerin, Mecklenburg-Strelitz, Nassau, Sachsen-Weimar-Eisenach, Sachsen-Coburg und
Gotha, Sachsen-Meiningen, Sachsen-Altenburg, Oldenburg, Anhalt-Dessau, Anhalt-Köthen (seit 1847
von Dessau verwaltet), Anhalt-Bernburg, Schwarzburg-Sondershausen, Schwarzburg-Rudolstadt,
Hohenzollern-Hechingen, Hohenzollern-Sigmaringen, Liechtenstein, Waldeck, Reuß ältere Linie, Reuß
jüngere Linie, Schaumburg-Lippe, Lippe, Hessen-Homburg und die Freien Städte Frankfurt, Lübeck,
Bremen und Hamburg.

**Außerhalb des Bundes, aber im Kernraum:** die Provinzen Preußen und Posen (Preußen), Schleswig
(Dänemark), Krakau und die übrigen nicht zum Bund gehörenden Teile Österreichs im Kartenausschnitt,
dazu die Nachbarn Frankreich (mit Elsass und Lothringen), Belgien, Niederlande, Schweiz,
Kongresspolen (Russland), Ungarn, Lombardei-Venetien. Siehe Entscheidung 4.

**Je Staat:** Name, Kurzname, Farbe, Hauptstadt, Regierungsform, Herrscher, Konfession des
Herrscherhauses; Kasse, Steuersätze, Heer, Schulden und Hofhaltung als `spielwert` (Kalibrierung in
M7b). **Mitgliedschaft im Deutschen Bund und im Zollverein**, damit M8/M9 darauf aufbauen: der
Zollverein als datiertes Staatsfeld `zollverein`, der Bund über das Ortsfeld `bund` (Bundesgebiet;
ein Staat ist Mitglied, wenn er mindestens einen Ort im Bundesgebiet besitzt – so sind Preußen und
Österreich Mitglieder, ohne dass Posen oder Ungarn zum Bund gehören). Die Staatskennungen aus 1648 bleiben, wo der Staat
fortbesteht (z. B. `SAX`, `BAV`); neue Staaten (z. B. `PRU` für das Königreich Preußen statt `BRA`)
bekommen neue Kennungen, alte enden am Datum ihres Untergangs.

**Währung:** Der Kern rechnet in einer Einheit. Für 1848 wird sie als **Taler** (preußischer Taler)
angezeigt; Gulden-Staaten werden umgerechnet (1 Taler = 1¾ süddeutsche Gulden, rund 1,43 Gulden
Konventionsmünze nach 14-Taler- gegen 20-Gulden-Fuß). Den Namen der Einheit („Reichstaler“ 1648,
„Taler“ 1848) legt das Szenario fest.

## 5. Zugehörigkeit der Orte

Ohne Grenzgeometrie von 1848 (Entscheidung 1) entsteht die Zuordnung in zwei Stufen:

1. **Regel über heutige Verwaltungseinheiten:** Jeder Ort hat aus GeoNames die heutigen Einheiten
   (Land/Bundesland, Bezirk bzw. Regierungsbezirk, Kreis). Eine neue Tabelle
   `data/tabellen/territorien_1848.csv` ordnet solche Einheiten einem Staat von 1848 zu, z. B.
   „heutiger Landkreis Ansbach → Bayern“, „heutige Woiwodschaft Großpolen → Preußen“.
2. **Ausnahmen je Ort** für alles, was quer zu den heutigen Grenzen liegt: thüringische und
   anhaltische Kleinstaaten mit Exklaven, Hohenzollern, Lippe, Waldeck, Coburg, die Grenze zwischen
   Preußen und Hannover, Oldenburgs Fürstentümer Lübeck und Birkenfeld, Hessen-Homburgs Oberamt
   Meisenheim, die preußischen Exklaven.

`orte.py` bzw. ein neues Werkzeug `tools/geodaten/zuordnung.py` erzeugt daraus die Zeilen von
`besitz.csv` mit Stichtag 1848.03.01. Prüfungen: jeder Ort im Bund hat genau einen Besitzer; die
Fläche je Staat liegt in der Spanne der Literaturangaben; Hauptstädte gehören ihrem Staat;
Kartenbild zur Sichtprüfung (`--vorschau`).

## 6. Verwaltungsgliederung

Die 626 Kreise aus M3 sind eine **feste, räumliche Ebene** ([05](../05_karte_und_geographie.md),
Abschnitt 2.1) und bleiben. Neu ist eine **datierte politische Ebene** darüber
(Abschnitt 2.2), unabhängig von den räumlichen Kreisen:

```
verwaltung.csv   id;datei;name;art;ueber;staat;sicherheit;quelle
                 pr_rb_potsdam;preussen;Regierungsbezirk Potsdam;regierungsbezirk;pr_prov_brandenburg;PRU;gesichert;…
besitz.csv       neue Spalten verwaltung (Einheit der untersten Ebene ab dem Datum) und bund (ja/leer)
```

- `art`: `provinz`, `regierungsbezirk`, `kreis`, `kronland`, `landdrostei`, … – frei, je Staat
  verschieden; `ueber` verweist auf die nächsthöhere Einheit, die oberste hängt am Staat.
- Kern: `World::admin_units` (Inhalt), `Location::admin` und `Location::federal`. **Umgesetzt als
  Inhalt** wie die Kreise: Fingerabdruck der Spieldaten statt Prüfsumme und Spielstand, damit sich
  Referenzprüfsumme und Speicherformat in M7 nicht ändern. Die Einheiten sind (noch) nicht datiert;
  datiert ist die Zuordnung im Ort. In den Zustand wandern die Felder, sobald Gebietswechsel im Spiel
  die Zuordnung ändern (Formatversion 2). Format: [chronik-skript.md](chronik-skript.md), Abschnitte
  3.6 und 3.16; Tabelle: [tabellen.md](tabellen.md). Brücke und
  Godot: Kartenmodus „Verwaltung“ mit Grenzen der gewählten Ebene, Ortsinfo mit der Kette
  „Kreis Teltow · Regierungsbezirk Potsdam · Provinz Brandenburg“.
- **Tiefe (Entscheidung 2):** Preußen mit Provinzen (8), Regierungsbezirken (25) und Kreisen (rund
  330); Österreich mit Kronländern und Kreisen (Böhmen 16, Mähren 6, Schlesien 2 usw.); Bayern,
  Sachsen, Hannover, Württemberg, Baden, Kurhessen und Großherzogtum Hessen mit ihrer obersten
  Ebene (Kreise, Kreisdirektionen, Landdrosteien, Provinzen); die übrigen Staaten ohne Unterteilung.
  Zuordnung der Orte zu Kreisen wie in Abschnitt 5 (heutige Einheiten plus Ausnahmen; bei Kreisen
  hilfsweise der nächste Kreissitz).

## 7. Bevölkerung

- **Stichtag:** 3. Dezember 1846 (Zählung des Zollvereins; Österreich 1846). Die Zahlen gelten zum
  Szenariodatum 1. März 1848 (es gilt die jüngste Angabe vor dem Datum, wie in M2).
- **Verfahren:** Summen je Staat und je Verwaltungseinheit aus den Zählungen (`ungeprueft` ohne
  Quellenzugriff); Verteilung auf die Orte nach heutiger Einwohnerzahl der Gemeinden im Ort
  (GeoNames) mit Abschlag für das Städtewachstum seit 1848, Städte über 10.000 Einwohner einzeln
  aus der Literatur. Verteilte Werte sind `schaetzung`; die Prüfung vergleicht die Summen je
  Einheit.
- **Konfession** je Staat bzw. Einheit (katholisch, lutherisch, reformiert, uniert – Preußen seit
  1817 –, jüdisch); **Sprache/Kultur** über die vorhandenen Kulturen plus neue (polnisch, kaschubisch,
  masurisch, litauisch, tschechisch, slowenisch, dänisch, friesisch). Juden erscheinen als
  Konfession, nicht als eigene Kultur.
- **Berufe** wie in M4/M5 mit Anteilen für 1848 (mehr Handwerker und Unterschichten, Pauperismus);
  Fabrikarbeiter kommen mit M7b.
- Umfang: rund 3.800 Orte im Bund und in den preußischen Ostprovinzen und Schleswig; zusammen etwa
  45 Millionen Einwohner (Größenordnung, zu prüfen).
- **Umgesetzt** (`tools/geodaten/bevoelkerung_1846.py`): Statt der heutigen Gemeindezahlen allein
  verteilt das Werkzeug die Summe je Raum (Provinz, Kronland oder Staat) nach Fläche, ländlichem
  Dichtefaktor je Landschaft, Gelände und – für Städte ohne feste Zahl – einem gedämpften
  Stadtgewicht aus der heutigen Einwohnerzahl; 167 Städte haben feste Werte (auch die damals
  kleinen Industriestädte). Die neuen Kulturen waren schon seit M4 vorhanden. Berufe wie in M5.

## 8. Eisenbahnen und Industrie (M7b)

- **Eisenbahnen:** Tabelle `eisenbahnen.csv` (Strecke, Abschnitte von Ort zu Ort, Eröffnung,
  Quelle). Bis März 1848 rund 4.000–5.000 km im Bund (Leipzig–Dresden, Berlin–Potsdam, Köln–Minden,
  Berlin–Hamburg, Berlin–Breslau und weiter nach Oberschlesien, Nordbahn Wien–Olmütz, Badische
  Hauptbahn, Ludwig-Süd-Nord-Bahn u. a.). Modell: Märkte, die über eine Strecke verbunden sind,
  handeln mit niedrigerer Fracht (neuer Regelwert `fracht_bahn`); Godot zeigt die Strecken als
  Linien (eigene Ebene des Kartenshaders).
- **Industrie:** neue Güter `kohle` und `eisen`, Betriebe `zeche`, `huette`, `fabrik` (Textil und
  Maschinen zusammen), Beruf `fabrikarbeiter`; Vorkommen `kohle` und `erz` an den Orten der Reviere
  (Ruhr, Saar, Aachen, Oberschlesien, Zwickau, Ostrau, Kladno, Siegerland).
- **Haushalte** in Talern für die Staaten von 1848 (Steuern, Hof, Verwaltung, Heer, Schulden) und
  **Zielwerte 1848** für den Balancing-Bericht (eigene Datei je Szenario).

## 9. Szenario, Kommandozeile, Godot

- `data/szenarien/voelkerfruehling.chron`: Datum 1848.03.01, Spielerstaat Preußen, Währungsname,
  spielbare Staaten. `uhrwerk-cli … --szenario voelkerfruehling` funktioniert schon heute.
- **Godot:** Startbildschirm mit Wahl des Lesezeichens (1648 oder 1848) und des Staates (spielbare
  Staaten des Szenarios hervorgehoben, alle anderen wählbar); Spielstände merken sich das Szenario
  ohnehin. Kartenmodi „Verwaltung“ (M7a) und „Eisenbahnen“ (M7b).
- **Leistung:** Etwa 3.800 Orte mit Daten statt 60 (rund 60.000 Blöcke) liegen weit unter dem
  Lasttest mit 500.000 Blöcken; das Einschwingen beim Laden wird länger (Messung in M7a).
  Gemessen: 3.967 Orte, 82.491 Blöcke; Laden im Release-Build unter 1 s (Debug 12 s), 10 Spieljahre
  im Debug-Build mit 4 Threads 20 s.

## 10. Tests und Abnahme

| Kriterium | Prüfung |
|---|---|
| `uhrwerk-cli pruefen` ohne Fehler; jeder Datensatz mit Quelle | `pruefen` für beide Szenarien in der CI; der Archivar lehnt Zeilen ohne Quelle oder Sicherheit ab; `archivar.py --bericht` zeigt die Verteilung der Sicherheit |
| Historische Stichprobenprüfung (Beirat oder dokumentierte Fachprüfung) | `docs/pruefung/stichprobe_1848.md`: 60 zufällig (fester Seed) gezogene Datensätze aus Besitz, Bevölkerung, Verwaltung und Staaten mit Prüfvermerk; dazu Summenprüfungen (Einwohner und Fläche je Staat und Einheit gegen Literaturwerte). Die fachliche Prüfung selbst kann nur ein Mensch leisten (Entscheidung 7) |

Weitere Tests: das Szenario 1648 bleibt unverändert (Referenzprüfsumme `5db13a66d63e6079`);
Determinismus, Spielstände und Wiederholung auch für 1848 (CTests); Godot-Rauchtest lädt beide
Szenarien; Klicktest mit der Staatswahl.

## 11. Offene Entscheidungen

1. **Netzzugang für Quellen.** Empfehlung: **in den Umgebungseinstellungen freigeben**
   (`de.wikipedia.org`, `www.wikidata.org`, `query.wikidata.org`, `planet.openhistoricalmap.org`;
   Netzwerkzugriff → erlaubte Domains). Dann prüfe ich Zahlen und Zuordnungen gegen Wikidata,
   Wikipedia und OpenHistoricalMap und kann viele Zeilen als belegt kennzeichnen. Alternative: ohne
   Netz aus der Literaturkenntnis – dann sind fast alle Zeilen `ungeprueft` oder `schaetzung`, und
   die Stichprobenprüfung muss vollständig ein Mensch übernehmen.
2. **Tiefe der Verwaltung.** Empfehlung: wie in Abschnitt 6 (Preußen drei Ebenen, Österreich zwei,
   mittlere Staaten eine). Alternative: für alle Staaten bis zur Kreisebene (Bayern rund 250
   Landgerichte, Württemberg 64 Oberämter, Baden rund 80 Bezirksämter) – etwa doppelter Datenaufwand.
3. **Spielbare Staaten.** Empfehlung: Preußen, Österreich, Bayern, Sachsen (Meilensteinplan) **und
   Baden** (Kapitel 04; Zentrum der Revolution). Die übrigen sind ebenfalls wählbar, nur nicht
   hervorgehoben.
4. **Ausland.** Empfehlung: Orte außerhalb von Bund, Preußen, Österreich und Schleswig (Elsass,
   Lothringen, Belgien, Niederlande ohne Limburg, Schweiz, Kongresspolen, Ungarn,
   Lombardei-Venetien) bleiben 1848 **Orte ohne Daten**. Alternative: mit Besitzer und
   Bevölkerung, rund 700 Orte mehr.
5. **Industrie in M7b.** Empfehlung: minimal wie in Abschnitt 8 (zwei Güter, drei Betriebe, ein
   Beruf). Alternative: ganz in einen späteren Meilenstein verschieben (Kapitel 08 sieht
   Industrialisierung ohnehin als eigenes Großthema).
6. **Teilung in M7a und M7b.** Empfehlung: ja; M7a ist für sich spielbar und abnehmbar.
7. **Fachprüfung.** Empfehlung: Ich lege die Stichprobe mit Prüfvermerken und Summenprüfungen an;
   die fachliche Abnahme übernimmst du oder ein Beirat. Bis dahin gilt M7 mit „Fachprüfung offen“
   als technisch fertig.

## 12. Umsetzungsschritte

| Schritt | Inhalt | Prüfung |
|---|---|---|
| a1 | **Staaten 1848** in den Tabellen, Szenario `voelkerfruehling`, Felder `bund` und `zollverein`, Währungsname | `pruefen` für beide Szenarien |
| a2 | **Zuordnung der Orte** (`territorien_1848.csv`, Ausnahmen, Werkzeug, Vorschau) | Fläche je Staat, Hauptstädte, Sichtprüfung |
| a3 | **Verwaltung** (Tabelle, Archivar, Lader, Kern, Fingerabdruck) | Unit-Tests, `pruefen` |
| a4 | **Bevölkerung 1846** mit Konfession, Kultur, Berufen | Summen je Staat und Einheit, Lauf über 50 Jahre ohne Ausreißer |
| a5 | **Brücke und Godot:** Staatswahl, Kartenmodus „Verwaltung“, Ortsinfo | Godot-Tests, Screenshots |
| a6 | **Doku, Stichprobe, Review** (Abschluss M7a) | CI grün, `stichprobe_1848.md` |
| b1 | **Eisenbahnen** (Daten, Modell, Kartenebene) | Unit-Tests, Sichtprüfung |
| b2 | **Industrie und Haushalte 1848** | `wirtschaft --szenario voelkerfruehling` |
| b3 | **Kalibrierung**, Zielwerte 1848, Balancing-Job | Bericht in der CI |
| b4 | **Doku und Review** (Abschluss M7) | CI grün |

## 13. Umsetzungsstand M7a

| Schritt | Stand |
|---|---|
| a1 Staaten | 38 Staaten zum 1. März 1848 (31 neu, 7 aus 1648 fortgeführt mit datiertem Namen); Hauptstadt, Regierungsform, Herrscher (Wechsel 1848 datiert), Konfession, Zollverein. Staaten ohne Ort zum Startdatum bestehen nicht (Lader und Archivar). Spielwerte aus der Bevölkerung (Hofhaltung 0,25, Schatz 0,3 Taler je Einwohner), Kalibrierung in b3. |
| a2 Zuordnung | `tools/geodaten/zuordnung.py` mit 476 Regeln: 3.967 Orte mit Besitzer, 575 Orte ohne Daten. Flächen nahe der Literatur (Preußen 275.000 km², Österreich im Bund 185.000, Bayern 77.600, Hannover 37.400, Württemberg 19.700, Sachsen 15.100, Baden 15.000). |
| a3 Verwaltung | Modell, Lader, Archivar, Fingerabdruck und Brücke fertig. Daten (458 Einheiten): Preußen mit 8 Provinzen, 25 Regierungsbezirken und 327 Kreisen (Stadt- und Landkreis mit gleichem Sitz zusammengefasst); Österreich mit 11 Kronländern und 51 Kreisen bzw. Vierteln (ohne Welschtirol und Küstenland, außerhalb der Karte); die oberste Ebene der Mittelstaaten; die drei Herzogtümer unter Dänemark. Die Orte kommen zum Kreis mit dem nächsten Sitz innerhalb ihrer Provinz bzw. ihres Kronlands (`verwaltung = naechste:<einheit>`, Spalte `sitz`); 23 Sitze ohne eigene Ortsstelle stehen für den nächsten Ort. Im Grenzraum von Brandenburg, Pommern, Posen und Schlesien legen Ortsregeln die Provinz fest. In Ober- und Niederösterreich gelten seit der Stichprobe Regeln je heutigem Bezirk, weil der nächste Sitz an Donau und Wienerwald versagt. |
| a4 Bevölkerung | `tools/geodaten/bevoelkerung_1846.py`: 45,0 Millionen Einwohner, Summen je Provinz, Kronland oder Staat. Ein Lauf über 10 Jahre verliert ohne Kalibrierung 1 % (Geburten 36,5 ‰, Sterbefälle 34,9 ‰); Zielwerte 1848 folgen in b3. |
| a5 Godot | Spielmenü „Neues Spiel“ mit Wahl des Lesezeichens, Knopf „Diesen Staat spielen“ in der Ortsinfo (jeder Staat wählbar, die spielbaren nennt ein Hinweis), Kartenmodus „Verwaltung“ (Taste Ü), Ortsinfo mit Verwaltungskette und Bundesgebiet, Staatsinfo mit Bund und Zollverein, Startoptionen `--szenario` und `--staat`. Statt eines eigenen Startbildschirms startet das Spiel wie bisher mit 1648. |
| a6 Doku, Stichprobe | Doku der Werkzeuge und Tabellen fertig. Stichprobe in [stichprobe_1848.md](../pruefung/stichprobe_1848.md): 60 Datensätze, davon 46 plausibel, 6 Vereinfachungen, 6 Fehler (behoben, darunter die Konfession in sieben Landschaften und die Viertel in Ober- und Niederösterreich) und 2 zweifelhaft; Flächen der großen Staaten und aller preußischen Provinzen innerhalb ±5 % der Richtwerte. Die Fachprüfung mit Quellen steht aus (Entscheidung 7). Review offen. |

**Quellen:** Die Domains der Quellen (Wikipedia, Wikidata, GenWiki/GOV, OpenHistoricalMap) waren in
der Umgebung der Umsetzung trotz Freigabe nicht erreichbar. Alle historischen Angaben stammen daher
aus dem Wissensstand ohne Quellenzugang und tragen `ungeprueft` (Staaten, Regeln, Summen, Städte)
bzw. `schaetzung` (verteilte Einwohner); die Prüfung gegen Quellen steht vor der Abnahme an.
