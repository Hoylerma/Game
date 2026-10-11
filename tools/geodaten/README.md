# Geodaten

Werkzeuge für die Geometrie der Karte des Kernraums (Meilenstein M3): Rohdaten laden und pinnen,
Land, Seen und Flüsse aus Natural Earth zuschneiden, den von Hand gepflegten Umriss des Kernraums
prüfen, und Bausteine für den Ort-Generator (Ortskandidaten aus GeoNames, Höhen und Lagemerkmale).

Verträge: [chronik-skript.md, Abschnitt 6](../../docs/technik/chronik-skript.md) (Projektion,
Rasterregel, Dateien unter `data/karte/geometrie/`); Inhalt des Kernraums:
[Design-Dokument 05, Abschnitt 1](../../docs/05_karte_und_geographie.md).

| Datei | Zweck |
|---|---|
| `geodaten.py` | Befehle `holen`, `geometrie`, `pruefen`, `vorschau` |
| `quellen.json` | Manifest: URL, SHA-256, Lizenz und Quellenangabe jeder Rohdatei; Höhenkacheln mit Prüfwert je Kachel |
| `geo.py` | Gemeinsame Hilfen: Projektion nach 6.1, Punkt-in-Polygon und Rasterung nach 6.2, Zuschnitt, Vereinfachung, GeoJSON lesen/schreiben |
| `staedte.py` | Ortskandidaten aus GeoNames `cities1000` im Kernraum |
| `hoehen.py` | Höhen aus Terrarium-Kacheln, Höhenstatistik im Umkreis, Abstand zu Küste und Flüssen |
| `orte.py` | Orte und Kreise des Kernraums: `data/tabellen/orte.csv`, `kreise.csv`, `dateien.csv` (siehe [Orte des Kernraums](#orte-des-kernraums-ortepy)) |
| `landschaften.csv`, `landschaften_dateien.csv`, `exonyme.csv`, `ausschluesse.csv`, `einwohner.csv`, `gelaende_gebiete.csv` | Hilfstabellen von `orte.py` (von Hand gepflegt) |
| `test_geodaten.py`, `test_orte.py`, `testdaten/` | Tests ohne Netz und ohne Zwischenspeicher |

Erzeugt bzw. gepflegt in `data/karte/`:

| Datei | Herkunft |
|---|---|
| `karte.chron` | von Hand: Ausschnitt 1,8–23,5 °O × 45,1–56,2 °N, Bezugsbreite 51°, 4096 × 3329 Pixel (≈ 0,37 km je Pixel) |
| `geometrie/land.geojson` | `geodaten.py geometrie` aus Natural Earth `land` + `minor_islands` |
| `geometrie/seen.geojson` | `geodaten.py geometrie` aus Natural Earth `lakes` + `lakes_europe` |
| `geometrie/fluesse.geojson` | `geodaten.py geometrie` aus Natural Earth `rivers_lake_centerlines` + `rivers_europe` |
| `geometrie/kernraum.geojson` | von Hand (siehe unten) |

## Aufruf

Python ab 3.10 mit numpy. Rohdaten liegen nur im Zwischenspeicher `build/geodaten-cache/` (durch
`.gitignore` ausgeschlossen), nie im Repository.

```sh
python3 tools/geodaten/geodaten.py holen               # alle Quellen laden (≈ 50 MB) und SHA-256 prüfen
python3 tools/geodaten/geodaten.py holen geometrie     # nur eine Gruppe: geometrie, referenz, orte, hoehen
python3 tools/geodaten/geodaten.py geometrie           # land/seen/fluesse.geojson neu erzeugen (≈ 4 s)
python3 tools/geodaten/geodaten.py pruefen             # passen die Dateien zu den Quellen? Kernraum gültig?
python3 tools/geodaten/geodaten.py vorschau            # PNG zur Sichtprüfung (Temp-Ordner/einigkeit-geo/vorschau.png)
python3 tools/geodaten/geodaten.py vorschau --breite 1500 --max-rang 6 --ausschnitt 9 45.3 16.8 47.9

python3 tools/geodaten/staedte.py --min-einwohner 5000 --ausgabe /tmp/kandidaten.tsv
python3 tools/geodaten/hoehen.py 13.40 52.52 --radius 5

python3 tools/geodaten/orte.py                         # Orte und Kreise erzeugen (1–2 min), siehe unten
python3 tools/geodaten/orte.py --pruefen               # Exitcode 1, wenn die Tabellen veraltet sind

python3 -m unittest discover -s tools/geodaten         # 96 Tests ohne Netz (auch mit Python 3.10; läuft in der CI)
```

`holen` lädt nur, was fehlt oder einen falschen Prüfwert hat; eine Datei mit abweichendem Prüfwert
nach dem Laden ist ein Fehler (die Quelle hat sich geändert – bewusst prüfen und den Eintrag in
`quellen.json` erneuern). `holen --pinnen` trägt fehlende Prüfwerte ein und legt den Kachelbereich
neu fest, wenn sich der Kartenausschnitt geändert hat. `pruefen` braucht die Rohdaten und läuft
daher nur lokal, nicht in der CI; die CI-tauglichen Prüfungen der eingecheckten Dateien stehen in
den Tests. Exitcodes: 0 = in Ordnung, 1 = `pruefen` hat Abweichungen gefunden, 2 = Fehler.

`geometrie` ist deterministisch: gleiche Quellen und gleiches `karte.chron` ergeben byte-gleiche
Dateien (geprüft mit Python 3.10/numpy 2.2 und Python 3.13/numpy 2.5).

## Quellen und Lizenzen

Alle Dateien sind in `quellen.json` mit URL und SHA-256 gepinnt.

**Natural Earth** 1:10 Mio., Version 5.1.2 (GeoJSON-Fassung aus
`https://raw.githubusercontent.com/nvkelso/natural-earth-vector/v5.1.2/geojson/`): `ne_10m_land`,
`ne_10m_minor_islands`, `ne_10m_lakes`, `ne_10m_lakes_europe`, `ne_10m_rivers_lake_centerlines`,
`ne_10m_rivers_europe`; dazu `ne_10m_admin_0_boundary_lines_land` (heutige Staatsgrenzen, nur für
die Vorschau und zum Nachzeichnen des Kernraums). Lizenz: gemeinfrei (Public Domain).
Quellenangabe: „Made with Natural Earth. Free vector and raster map data @ naturalearthdata.com.“

**GeoNames** `cities1000` (alle Orte ab 1.000 Einwohnern, Stand März 2020), bezogen über das
npm-Paket `cities-with-1000@1.0.4` (`https://registry.npmjs.org/cities-with-1000/-/cities-with-1000-1.0.4.tgz`,
daraus `package/cities1000.txt`). Lizenz der Daten: Creative Commons Namensnennung 4.0
(CC BY 4.0, https://creativecommons.org/licenses/by/4.0/). Quellenangabe: „GeoNames
(https://www.geonames.org), Auszug cities1000, lizenziert unter CC BY 4.0.“ Die Datei
`testdaten/cities_test.txt` enthält gekürzte Zeilen daraus (gleiche Lizenz) und eine erfundene
Testzeile („Testdorf“).

**Terrain Tiles** (Terrarium-PNG, Web Mercator, Zoomstufe 8, 224 Kacheln für den Ausschnitt mit
0,2° Rand; `https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{z}/{x}/{y}.png`, AWS Open Data,
https://registry.opendata.aws/terrain-tiles/). Höhe = (R · 256 + G + B / 256) − 32768 m. Die
Quelldaten verlangen Namensnennung; nach
https://github.com/tilezen/joerd/blob/master/docs/attribution.md lautet die Quellenangabe:

> ArcticDEM terrain data DEM(s) were created from DigitalGlobe, Inc., imagery and funded under
> National Science Foundation awards 1043681, 1559691, and 1542736; Australia terrain data
> © Commonwealth of Australia (Geoscience Australia) 2017; Austria terrain data © offene Daten
> Österreichs – Digitales Geländemodell (DGM) Österreich; Canada terrain data contains information
> licensed under the Open Government Licence – Canada; Europe terrain data produced using
> Copernicus data and information funded by the European Union - EU-DEM layers; Global ETOPO1
> terrain data U.S. National Oceanic and Atmospheric Administration; Mexico terrain data source:
> INEGI, Continental relief, 2016; New Zealand terrain data Copyright 2011 Crown copyright (c) Land
> Information New Zealand and the New Zealand Government (All rights reserved); Norway terrain data
> © Kartverket; United Kingdom terrain data © Environment Agency copyright and/or database right
> 2015. All rights reserved; United States 3DEP (formerly NED) and global GMTED2010 and SRTM
> terrain data courtesy of the U.S. Geological Survey.

Für das Spiel gilt: Wer abgeleitete Daten (Orte mit Höhen, Gelände) ausliefert, nennt diese drei
Quellen in den Spielinformationen.

## Was `geometrie` tut

1. **Zuschnitt** auf den Ausschnitt aus `karte.chron` plus 0,25° Rand (Polygone nach
   Sutherland-Hodgman, Linien nach Liang-Barsky). Die Kanten am Zuschnittrahmen liegen außerhalb
   des Bildes.
2. **Doppel entfernen:** `minor_islands` wiederholt in Version 5.1.2 alle Inseln im Ausschnitt
   wörtlich aus `land`; Polygone mit gleichem äußeren Ring werden nur einmal übernommen (bei einer
   Gerade-ungerade-Auswertung über alle Polygone würden sich Doppel aufheben). `pruefen` meldet
   Kartenpixel, die in mehr als einem Polygon einer Datei liegen.
3. **Vereinfachung** nach Douglas-Peucker mit einer Toleranz von einem Kartenpixel (x-Achse mit
   cos(Bezugsbreite) gestreckt); Flächen unter 2 Pixeln² und Flussstücke unter 2 Pixeln Länge
   entfallen. Koordinaten mit 4 Nachkommastellen (≈ 11 m), Ringe nach RFC 7946 ausgerichtet
   (außen gegen den Uhrzeigersinn).
4. **Flüsse** nur auf Land und außerhalb der Seen (Rasterregel auf dem Kartenraster, verdichtet auf
   halbe Pixel): So enden Flüsse an Küsten und Seeufern, und Mittellinien durch Seen bleiben dort
   erhalten, wo Natural Earth den See nicht führt (sonst entstünden Lücken). Eigenschaft `rang`
   aus `scalerank`: ≤ 5 → 1 (Donau, Rhein mit Mündungsarmen, Elbe, Oder, Weichsel), 6–7 → 2 (Weser,
   Mosel, Memel, Drau, Mur), 8–9 → 3 (Main, Inn, Maas, Ems, Warthe, March, Moldau, Schelde),
   10 → 4 (Neckar, Saale, Spree, Isar, Saar, Netze), 11 → 5 (Salzach, Enns, Ruhr, Eger, Thaya,
   Lausitzer Neiße), 12 → 6 (Lippe, Unstrut, Fulda, Bober, Weiße Elster).
5. **Namen** deutsch: `name_de` der Quelle, sonst `name`, jeweils über kleine Tabellen in
   `geodaten.py` (`FLUSSNAMEN_DE`, `SEENAMEN_DE`) für übliche Exonyme (Weichsel, Memel, March,
   Glatzer Neiße, Drewenz, Küddow …) und unvollständige Seenamen („Schweriner“ → „Schweriner See“).
   Dieselbe Tabelle berichtigt einen Namensfehler der Quelle: Den Lauf Mže/Berounka (Mies/Beraun,
   Mähring bis Prag) führt Natural Earth als „Úhlava“, er heißt hier Beraun.
   Natural Earth führt nicht alle Flüsse (es fehlen z. B. Havel, Lahn, Lech und Pregel).

Ergebnis: zusammen etwa 0,26 MB (Land 66 Polygone, 88 Seen, 177 Flüsse).

Bekannte Grenzen der Quelle (1:10 Mio., Lagefehler an Küsten bis etwa 1 km): Watteninseln sind
schmal oder etwas versetzt, Mündungstrichter gelten als Meer (die Elbe etwa bis Hamburg), Haffe
und Bodden sind Meer, das Frische Haff ist als See geführt.

## Bausteine für den Ort-Generator

```python
import sys; sys.path.insert(0, "tools/geodaten")
import staedte, hoehen, geo

kandidaten = staedte.kandidaten()                 # list[Kandidat], nach Einwohnern absteigend
k = kandidaten[0]
k.stadt.name, k.stadt.alternativnamen, k.stadt.einwohner, k.stadt.laenge, k.stadt.breite
k.auf_land, k.wasserabstand_km                    # Küstenorte knapp im Wasser: auf_land = False

auskunft = hoehen.Lageauskunft.laden()            # ≈ 4 s: Höhenmosaik, Landmaske, Gewässer
m = auskunft.merkmale(13.40, 52.52, radius_km=5)  # Höhe, Statistik (nur Land), Küste, große Flüsse
m.hoehe, m.umkreis.mittel, m.umkreis.relief, m.kueste_km, m.fluss_km
```

- `staedte.kandidaten()` liest `cities1000.txt` und wählt Orte im Kernraum (exakte
  Punkt-in-Polygon-Prüfung). Orte im Meer oder in einem See bleiben bis 2 km vom Ufer Kandidaten
  (`auf_land = False`; z. B. Helgoland, Hel, Sonderburg, Lindau), weil die Küsten der Quelle grob
  sind. Orte aus Staaten, die ganz außerhalb liegen (CH, HR, HU, SK), entfallen immer – die
  vereinfachte Grenzlinie trennt z. B. Laufenburg (Baden) und Laufenburg (Aargau) nicht –, ebenso
  Orte der Provinz Trient (`AUSLAND_GEBIETE`; der Umriss ragt am Nonsberg einige Kilometer ins
  Trentino). Stand: 15.037 Kandidaten. Die Namen sind heutige Namen; historische deutsche Namen
  stehen unkenntlich unter den Alternativnamen (`Wrocław` → … `Breslau` …) und müssen bewusst
  gewählt und belegt werden.
- `hoehen.Hoehenmodell` dekodiert die PNG-Kacheln selbst (zlib und numpy, alle fünf PNG-Filter;
  gegen Pillow an allen 224 Kacheln geprüft) und interpoliert bilinear. Auflösung bei Zoom 8 etwa
  400 m: Gipfel erscheinen 2–8 % zu niedrig (Zugspitze 2762 statt 2962 m, Brocken 1116 statt
  1141 m); für die Einordnung von Gelände (Marsch, Mittelgebirge, Hochgebirge) genügt das.
  `statistik()` wertet die Pixel aus, deren Mitte im Umkreis liegt, optional mit Maske (Meerestiefen
  sonst im Mittel).
- `hoehen.Gewaesserabstand`: Küste = Rand von `land.geojson` ohne die Kanten am Zuschnittrahmen;
  Flüsse bis `max_rang` (Standard 3).
- `geo.py`: `Karte` (Projektion, `pruefe()` für die Regeln aus 6.1), `punkt_in_polygon`,
  `FlaechenIndex`, `LinienIndex`, `rastere_polygone`, `Rastermaske`, `abstand_km`.

## Der Kernraum (`kernraum.geojson`)

Ein Polygon mit 398 Punkten, gegen den Uhrzeigersinn, an den Küsten weit ins Meer gezogen
(maßgeblich ist der Schnitt mit `land.geojson`). Die Eigenschaft `abschnitte` nennt für jeden
Abschnitt den Index seines ersten Punktes und den Verlauf; beim Bearbeiten mitpflegen. Abschnitte
entlang heutiger Grenzen sind nach Natural Earth nachgezeichnet und an Grenzflüssen von Hand so
gelegt, dass die Orte auf der richtigen Seite liegen (Hochrhein, Untersee, Alter Rhein,
Burgenland); historische Grenzen sind aus historischem Wissen vereinfacht. Genauigkeit 1–10 km.
Kernraum-Land (ohne Seen) umfasst etwa 827.000 km².

Prüfen nach dem Bearbeiten: `geodaten.py pruefen` (Umlaufrichtung, Selbstüberschneidungen, alles im
Ausschnitt), die Tests (93 Prüforte in `geodaten.PRUEFORTE` mit erwarteter Lage) und die Vorschau
(`vorschau --ausschnitt …` mit den heutigen Grenzen in Grau).

### Entscheidungen

Grundsatz: Kernraum ist das Alte Reich nördlich der Alpen im Jahr 1648 mit Preußen, Posen und
Schleswig (Design-Dokument 05) und dazu schmale Säume der Nachbarn, in denen deutsche Geschichte
spielt (Grenzkriege, Abtretungen, Abstimmungsgebiete).

- **Belgien ganz.** Die Spanischen Niederlande gehörten 1648 als Burgundischer Reichskreis zum
  Reich (seit 1548 weitgehend selbständig), das Hochstift Lüttich zum Niederrheinisch-Westfälischen
  Kreis; ab 1714 österreichisch bis 1795. Belgien ist zudem Hauptschauplatz 1914 und 1940. Damit
  reicht Flandern bis an die Küste bei De Panne.
- **Niederlande nur als Saum:** Die Republik schied 1648 auch formell aus dem Reich aus. Im
  Kernraum liegen die Provinzen an der deutschen Grenze: Groningen, Drenthe, Overijssel östlich der
  IJssel, Gelderland mit Arnheim und Nimwegen, ganz Limburg (1839–1866 als Herzogtum Limburg im
  Deutschen Bund). Amsterdam, Rotterdam, Utrecht, Nordbrabant, Seeland mit Zeeuws-Vlaanderen und
  Friesland (auch Schiermonnikoog) bleiben Ausland.
- **Frankreich:** Lothringen ganz (Herzogtum mit Barrois sowie die Drei Bistümer Metz, Toul,
  Verdun; Westgrenze in den Argonnen, Südgrenze am Südrand der Vogesen) und das Elsass mit Belfort
  und Mömpelgard (bis 1793 württembergisch). Saum: Französisch-Flandern mit Dünkirchen und Lille
  (1648 spanisch), Douai, das Cambrésis (Hochstift Cambrai bis 1678 Reichsstand), der französische
  Hennegau (Valenciennes, Maubeuge), die Ardennen mit Charleville und Sedan sowie der Nordrand der
  Freigrafschaft (Lure, Héricourt; die Freigrafschaft war bis 1678 Teil des Burgundischen Kreises).
  Besançon, Reims und Arras bleiben Ausland.
- **Schweiz bleibt Ausland** (seit 1648 von der Reichsgerichtsbarkeit befreit), mit Basel und
  Schaffhausen (seit 1501 eidgenössisch). Grenze am Hochrhein, durch Unter- und Obersee und am
  Alpenrhein; Konstanz liegt im Kernraum. **Liechtenstein** und **Vorarlberg** gehören dazu.
- **Südtirol ja, Trentino nein.** Tirol gehört ganz zum Kernraum, im Süden bis zur Salurner Klause
  (Umriss der heutigen Provinz Bozen mit Deutschnonsberg und Gröden). Das italienischsprachige
  Welschtirol (Trentino, bis 1803 Hochstift Trient) sowie Buchenstein und Ampezzo (heute Provinz
  Belluno) zählen zu Norditalien und bleiben Ausland. Wo der Umriss am Nonsberg ins Trentino
  ragt, entfallen die Orte der Provinz Trient über ihren GeoNames-Code (`AUSLAND_GEBIETE`).
- **Kanaltal** (Tarvis, Malborghetto, Pontafel) gehört dazu: bis 1759 bambergisch, dann Kärnten bis
  1919.
- **Krain und Untersteiermark ja, Küstenland nein.** Krain (mit Laibach, Idria und Wippach) und die
  Untersteiermark bildeten mit Steiermark und Kärnten Innerösterreich und gehörten zum Deutschen
  Bund. Die Grenze im Süden folgt Kulpa, Uskokengebirge und Sotla (heutige Grenze zu Kroatien).
  Görz, Triest und Istrien (ebenfalls im Deutschen Bund) bleiben als Adria-Raum dem Europa-Layer
  überlassen – eine Abgrenzung nach Spielraum, keine historische Notwendigkeit; sie lässt sich
  ändern, indem der Abschnitt „Krain gegen Görz und das Küstenland“ nach Westen verschoben wird.
- **Ungarische Seite:** Das Burgenland (bis 1921 ungarisch, heute Österreich) gehört dazu, Ödenburg
  (Sopron, 1921 bei Ungarn geblieben) nicht. Das Übermurgebiet (bis 1919 ungarisch) und Pressburg
  bleiben Ausland; die Grenze zu Ungarn bzw. zur Slowakei folgt March, Weißen Karpaten und Beskiden.
- **Österreichisch-Schlesien** mit dem ganzen Teschener Land gehört dazu (Ostgrenze an der Biala
  gegen Galizien). **Polnischer Saum:** etwa 20–30 km jenseits der preußischen Grenze von 1815–1918
  (Westgalizien bis vor Krakau, Dombrowaer Becken, Tschenstochau, Wieluń, Kalisch, Konin, Kujawien
  mit Włocławek, Dobriner Land, Nordmasowien mit Mława, Suwałki). Die Grenze von 1815–1918 selbst
  ist nicht gezogen, nur der Außenrand des Saums.
- **Preußen:** West- und Ostpreußen mit dem Ermland (bis 1772 polnisch) sowie das **Memelland**
  (Grenze des Friedens von Melno 1422 bis 1920; vereinfacht nachgezeichnet, Unsicherheit 5–10 km:
  Heydekrug, Prökuls und Pogegen innen, Krottingen, Garsden, Schwekschne und
  Žemaičių Naumiestis außen). Litauen jenseits davon bleibt Ausland.
- **Schleswig ganz,** also bis zur Königsau (Kongeå), mit Alsen und Ærø (Ærø gehörte bis 1864 zum
  Herzogtum, Alsen und Nordschleswig bis 1920 zu Preußen). **Dänischer Saum** bis etwa 55,57 °N mit
  Kolding, Vejen, Esbjerg und Fanø. Fünen, Langeland, Lolland, Falster, Møn und Bornholm bleiben
  Ausland. **Helgoland** (1807/1814–1890 britisch) gehört dazu, ebenso Rügen, Fehmarn, Usedom,
  Wollin, die Ost- und Nordfriesischen Inseln.
- **Nicht abgebildet** sind Exklaven wie Büsingen (deutsch, in der Schweiz) und Baarle-Hertog
  (belgisch, in den Niederlanden). Zwischen Ortspaaren direkt an Grenzflüssen (Laufenburg,
  Diepoldsau) reicht die Genauigkeit von Natural Earth nicht immer; `staedte.kandidaten` filtert
  deshalb zusätzlich nach heutigem Staatscode.

### Umfang

Bei durchschnittlich 150 km² je Ort (Design-Dokument 05) ergäbe das Kernraum-Land von etwa
827.000 km² rund 5.500 Orte statt der geplanten 4.500; schon das Reichsgebiet von 1914 mit
Österreich und Böhmen-Mähren umfasst etwa 700.000 km². Der Ort-Generator sollte die Säume und dünn
besiedelte Gebiete (Hochgebirge, Moore) gröber aufteilen, oder die Zahl der Orte steigt auf etwa
5.000.

## Orte des Kernraums (`orte.py`)

`orte.py` erzeugt reproduzierbar die Orte und Kreise des ganzen Kernraums (Meilenstein M3) in
`data/tabellen/orte.csv`, `kreise.csv` und `dateien.csv`. Die 60 Orte der Testregion (M1, mit
Historie) bleiben unverändert – Kennung, Name, Datei, Lage, Gelände, Stadt, Quelle, Reihenfolge und
Historie; sie erhalten nur ihren Kreis und ihre Fläche aus der Karte. Alle neuen Orte sind *Orte ohne
Daten* (chronik-skript.md, Abschnitt 3.6): ohne Besitz und Bevölkerung, ohne Historie.
`besitz.csv`, `bevoelkerung.csv` und `staaten_historie.csv` bleiben unberührt.

### Aufruf und Reihenfolge

```sh
python3 tools/geodaten/orte.py                         # Tabellen schreiben (1–2 min; Rohdaten im Zwischenspeicher)
python3 tools/archivar/archivar.py                     # data/history/orte/*.chron, kreise/*.chron
python3 tools/kartengenerator/kartengenerator.py       # provinzen.png, fluesse.png, definition.csv
python3 tools/archivar/archivar.py --pruefen           # history/ aktuell?
python3 tools/geodaten/orte.py --pruefen               # Tabellen aktuell? (Exitcode 1 sonst)
ORTE_VOLLTEST=1 python3 -m unittest tools.geodaten.test_orte   # dasselbe als Test
```

`orte.py` rechnet die Karte selbst mit der Python-API des Kartengenerators (gleiche Eingaben, gleiche
Ladereihenfolge wie `kartengenerator.py`): daraus kommen die Nachbarschaften für die Kreise und die
Fläche aller Orte, die damit genau zu `definition.csv` passt. Der Lauf ist deterministisch: gleiche
Eingaben ergeben byte-gleiche Tabellen (geprüft mit Python 3.10/numpy 2.2 und 3.13/numpy 2.5).
`--bericht DATEI` schreibt die Kennzahlen zusätzlich in eine Datei.

### Hilfstabellen

| Datei | Inhalt |
|---|---|
| `landschaften.csv` | Staat und GeoNames-Verwaltungscodes (admin1–admin4) → Landschaft; 418 Regeln. Es gilt die passende Regel mit der tiefsten Ebene; leeres Feld = beliebig, „-“ = Feld muss leer sein (für fehlerhafte Codes in GeoNames). Ohne passende Regel erhält ein Kandidat die Landschaft des nächsten zugeordneten Kandidaten (steht im Bericht). |
| `landschaften_dateien.csv` | 77 Landschaften = Dateien unter `history/orte` und `history/kreise`: Name, Kurzname (Zusatz bei gleichnamigen Orten), Abstandsfaktor der Auswahl, Umfang für den Kopfkommentar (`dateien.csv`) |
| `exonyme.csv` | Deutsche Namen: `geonameid;name_de;geprueft;heute`. `geprueft = ja` nur, wenn `name_de` wörtlich in den GeoNames-Alternativnamen steht – allein oder mit Ortszusatz („Braunsberg  Ostpreußen“, „Landskron/Schönhengstgau“: Name, dann Leerzeichen oder Schrägstrich, dann ein groß geschriebener Zusatz). `orte.py` prüft das und die Spalte `heute` (GeoNames-Name, zur Lesbarkeit) bei jedem Lauf. Einträge mit `nein` (203) sind Namen ohne Beleg in GeoNames; sie wirken nicht. 978 Einträge mit `ja`, darunter für Orte in DE/AT auch die deutsche oder volle Namensform, wo der GeoNames-Hauptname abweicht (München statt Munich, Freiburg im Breisgau statt Freiburg, Meißen statt Meissen, Großarl statt Grossarl). |
| `ausschluesse.csv` | 51 Kandidaten, die entfallen, mit Begründung: moderne Gründungen (`neugruendung`, etwa Wolfsburg, Eisenhüttenstadt), Kunstnamen aus Gemeindereformen (`kunstname`, etwa Filderstadt, Rodgau), außerdem `ortsteil`, `doppelt`, `datenfehler` |
| `einwohner.csv` | Einwohnerzahlen für Verwaltungssitze, die GeoNames mit 0 Einwohnern führt (`geonameid;name;einwohner;beleg`; Osterode am Harz, Avesnes-sur-Helpe). Eingetragen ist eine gerundete Untergrenze der amtlichen Zahl; sie ersetzt die GeoNames-Zahl für Bedeutung, Stadt und Kreisname. `name` muss dem GeoNames-Namen gleichen. Ohne Eintrag zählt ein Ort mit 0 Einwohnern wie einer mit 300 und verliert gegen größere Dörfer der Umgebung. |
| `gelaende_gebiete.csv` | 14 Börden und 2 Heiden als Kreise (Mittelpunkt und Radius geschätzt) mit Beleg |

### Ablauf

1. **Kandidaten**: `staedte.kandidaten()` (im Kernraum, auf Land oder bis 2 km vor dem Ufer), nur
   GeoNames-Codes PPL, PPLA … PPLA4, PPLC (keine Ortsteile PPLX, keine Wüstungen), ohne
   `ausschluesse.csv`. Die GeoNames-Entsprechung eines M1-Ortes (gleicher Name, auch
   Alternativname oder Namensanfang, höchstens 5 km entfernt) entfällt als Kandidat. Für
   Verwaltungssitze ohne Einwohnerzahl in GeoNames gilt die Zahl aus `einwohner.csv`.
2. **Bedeutung** = log10(max(Einwohner, 300)) + Rangzuschlag + Bekanntheit. Rangzuschlag nach
   Verwaltungsrang: 1,0 Haupt- oder Landeshauptstadt, 0,5 Sitz auf Kreisebene (Kreis, Bezirk,
   Powiat, Okres, Arrondissement), 0,1 Gemeindesitz; die GeoNames-Codes bedeuten je Staat
   Verschiedenes (Tabelle `VERWALTUNGSRANG`). Bekanntheit = 0,1 × log2(1 + Zahl der
   Alternativnamen), höchstens 0,5 – sie hebt bekannte Orte über gleich große Vororte (Bad Ischl).
3. **Lage**: Kandidaten knapp im Wasser rücken auf das nächste Kernraum-Landpixel (höchstens 3 km,
   in der Quellenangabe vermerkt); ohne Land in 3 km entfallen sie (Reichenau im Bodensee).
   Länge und Breite mit 4 Nachkommastellen.
4. **Auswahl** (gewichtete Poisson-Scheibe): in absteigender Bedeutung wird ein Kandidat genommen,
   wenn kein gewählter Ort (die M1-Orte eingeschlossen) näher liegt als
   r = 7,95 km × f × Landschaftsfaktor. f fällt linear von 1,2 (Bedeutung ≤ 3,2, ein Dorf) auf
   0,55 (≥ 6,8, eine Großstadt); der Landschaftsfaktor ist 1,15 in den Säumen (Flandern, Brabant,
   Hennegau, Namur, Ardennen, Französisch-Flandern, Geldern, Overijssel, Groningen-Drenthe, Jütland,
   Kongresspolen, Galizien), sonst 1. Gegenüber Orten anderer Landschaften gilt 0,8 × r, damit
   Grenzorte (Asch neben Selb) nicht verdrängt werden. Danach erhält jede Landfläche ohne Ort
   (Insel) ihren wichtigsten Kandidaten. Zuletzt die Lückenfüllung: solange ein Landblock
   (4 × 4 Pixel) weiter als 15 km vom nächsten Ort entfernt ist, kommt der ihm nächste Kandidat
   hinzu, der mindestens 6 km Abstand zu allen Orten hat (gesucht wird höchstens 100 km weit;
   Land, das weiter als 40 km von jedem Ort liegt, gilt als unendlich weit entfernt).
5. **Name**: (a) Eintrag in `exonyme.csv` mit `geprueft = ja`; (b) in DE, AT, LI der GeoNames-Name
   ohne moderne Zusätze („Lutherstadt“, „Ostseebad“, „Hansestadt“, „… Stadt“, „… Markt“ usw.;
   „Bad“ bleibt, ebenso ein Zusatz, der zum Namen gehört: „Weil der Stadt“, „Stetten am Kalten
   Markt“);
   (c) in Südtirol der deutsche Teil des zweisprachigen Namens („Bolzano - Bozen“ → Bozen);
   (d) sonst der heutige Name. Deutsche Namen werden nie ohne GeoNames-Beleg vergeben.
   Führt GeoNames für einen von mehreren gleichnamigen Orten eine volle historische Namensform,
   steht diese in `exonyme.csv` („Freiburg im Breisgau“, „Hirschberg in Schlesien“, „Neustadt in
   Oberschlesien“, „Friedland in Böhmen“, „Naumburg an der Saale“). Die übrigen gleichnamigen Orte
   (auch gleich einem M1-Ort) erhalten „ (Kurzname der Landschaft)“, etwa „Mühlhausen (Böhmen)“
   (82 Orte); Namen mit Klammerzusatz bleiben. Namen aus den Umbenennungen der Jahre 1933–1945
   werden nicht übernommen, auch wenn GeoNames sie führt (Hindenburg für Zabrze, Heydebreck für
   Kędzierzyn, Breitenmarkt für Sieraków Śląski, Dietfurt für Żnin, Eichenbrück für Wągrowiec);
   ebenso keine Namen der Nachkriegszeit (Gottwald für Zlín).
6. **Kennung**: Kleinbuchstaben, ä → ae, ö → oe, ü → ue, ß → ss, Akzente weg, sonst „_“.
   Gleiche Kennungen erhalten alle die Landschaft als Zusatz, dann den nächsten benannten Fluss
   (bis 5 km), zuletzt eine Zahl; Kennungen der M1-Orte sind belegt.
7. **Gelände** aus dem Höhenmodell (Landpixel im Umkreis von 6 km: Mittel, 5- und 95-%-Perzentil,
   Relief = p95 − p5; Perzentile, weil einzelne Kacheln Ausreißer haben). Vorrang in dieser
   Reihenfolge:

   | Gelände | Regel |
   |---|---|
   | `kueste` | auf einer Insel bis 400 km² oder bis 6 km von der Küste und Mittel ≤ 30 m; Haffe (Binnengewässer ab 20 km², die ans Meer grenzen: Frisches Haff, Lebasee) zählen als Küste |
   | `hochgebirge` | p95 ≥ 1500 m oder Mittel ≥ 1200 m |
   | `mittelgebirge` | Relief ≥ 300 m, oder Mittel ≥ 450 m und Relief ≥ 150 m |
   | `boerde`, `heide` | in einem Gebiet aus `gelaende_gebiete.csv` und Relief < 150 m |
   | `flussaue` | bis 3 km von einem großen Fluss, Mittel höchstens 40 m über p5, Relief < 150 m |
   | `seenplatte` | Seenanteil ≥ 5 % im Umkreis von 10 km (ohne Haffe) und Mittel < 200 m |
   | `huegelland` | Relief ≥ 100 m, oder Mittel ≥ 200 m und Relief ≥ 50 m |
   | `flachland` | sonst (auch flache Hochebenen wie die Münchner Schotterebene) |

   `hochgebirge` ist in M3 zu `data/common/gelaende.chron` hinzugekommen. `bruch` und `wald`
   vergibt die Regel nicht (nur M1-Orte haben sie).
8. **Stadt** = `ja`, wenn der Ort heute Haupt-, Landeshaupt- oder Kreisstadt ist (Verwaltungsrang
   ≤ 2) oder heute mindestens 20.000 Einwohner hat. Eine Näherung, kein historisches Stadtrecht.
9. **Kreise**: Nachbarn sind Orte mit gemeinsamer Grenze auf der Karte. Ein abgetrennter Teil einer
   Landschaft mit weniger als 4 Orten (Exklave, etwa Jüterbog) bildet Kreise mit der Landschaft, mit
   der er die längste Grenze teilt; Inseln bleiben in ihrer Landschaft und werden über die
   kürzeste Luftlinie angebunden. Je Landschaft verschmilzt eine Ward-Agglomeration auf dem
   Nachbarschaftsgraphen benachbarte Gruppen (höchstens 12 Orte) bis zur Zielzahl
   round(n / 7,1), danach nur noch Gruppen unter 4 Orten; eine zu kleine Gruppe ohne freien
   Nachbarn wird mit der kleinsten Nachbargruppe zusammengelegt und neu zweigeteilt. Name und
   Kennung (`kr_<Hauptort>`) nach dem größten Ort (heutige Einwohnerzahl, für M1-Orte die ihrer
   GeoNames-Entsprechung); Quelle „räumlich gebildet (M3), keine historische Verwaltungseinheit“.
10. **Reihenfolge** in `orte.csv`: die M1-Orte unverändert vorn, dann die neuen Orte nach
    Landschaft, Kreis (je Landschaft von Nord nach Süd), Hauptort zuerst, dann nach Bedeutung;
    `kreise.csv` in derselben Kreisfolge. `dateien.csv` erhält Kopfkommentare für die neuen Dateien
    und einen M3-Absatz für die Dateien der M1-Orte (bei jedem Lauf ersetzt, auf 92 Zeichen je
    Zeile umgebrochen); die Zeilen der Staaten bleiben unverändert.

Quellenangabe eines neuen Ortes: `GeoNames (CC BY 4.0), geonameid N; <Herkunft des Namens>[; Zusatz
der Landschaft …][; Lage auf das nächste Landpixel gesetzt]; Gelände aus Höhenmodell; Fläche aus der
Karte`.

### Kennzahlen (Stand dieses Laufs)

- 14.404 Kandidaten (51 ausgeschlossen, 60 M1-Entsprechungen, 2 Einwohnerzahlen aus
  `einwohner.csv`); gewählt 4.482 neue Orte (Scheibe 4.412, Inseln 1, Füllung 69). Zusammen
  **4.542 Orte** in 77 Landschaften, **626 Kreise** (4–12 Orte, Median 7).
- Fläche: Median 169 km², kleinste 1 km² (Helgoland), größte 1.147 km² (Insterburg), Summe
  826.355 km².
- Gelände: Hügelland 1.620, Flachland 1.487, Mittelgebirge 736, Flussaue 243, Hochgebirge 195,
  Küste 131, Börde 66, Seenplatte 40, Heide 21, Bruch 2, Wald 1. Städte: 1.088.
- Namen der neuen Orte: 2.603 GeoNames-Namen in DE/AT/LI, 978 Namen aus `exonyme.csv` (948
  deutsche Namen außerhalb von DE/AT/LI, 30 abweichende Namensformen in DE/AT), 5 aus
  zweisprachigen Südtiroler Namen, 896 heutige Namen, für die `exonyme.csv` keinen belegten
  deutschen Namen enthält (am meisten in Posen 91, Lothringen 84, Kongresspolen 78,
  Westpreußen 74, Böhmen 59). 82 Orte tragen den Kurznamen ihrer Landschaft als Zusatz.
- Prüfungen: `archivar.py --pruefen` und `kartengenerator.py --pruefen` aktuell (Warnungen nur zu
  Helgoland, Wangerooge und Juist sowie zum Land ohne Ort); `uhrwerk-cli pruefen`: 0 Fehler,
  0 Warnungen, 4.542 Orte (60 mit Daten, 4.482 ohne Daten), 626 Kreise.

### Bekannte Grenzen

- GeoNames `cities1000` führt nur Orte mit heute mindestens 1.000 Einwohnern. In der Oblast
  Kaliningrad und im Memelland ist die Quelle dünn: Land liegt dort bis 27 km vom nächsten Ort
  entfernt (im übrigen Kernraum höchstens 25 km); deshalb `max_radius_km = 32` in `karte.chron`.
  Inseln ohne Kandidaten bleiben ohne Ort: 41 Teilflächen mit zusammen 2.139 Pixeln (Rømø 85 km²,
  Nordstrand 48, Poel 34, Amrum 29, Spiekeroog 19, Schilfgürtel des Neusiedler Sees 12, Memmert 8,
  Mellum 7, Wustrow 7, Trischen 6, Rottumeroog 5, Baltrum 4 …).
- Auswahl und Kreisnamen folgen der heutigen Bedeutung (Einwohner, Verwaltungssitze): Industrie-
  städte des 19./20. Jahrhunderts sind stärker vertreten, abgegangene Orte fehlen; Kreise heißen
  nach dem heute größten Ort (Kreis Freistadt statt Teschen, Kreis Ostrava).
- Lücken der Quelle wirken auf die Auswahl: Spremberg (Kreisstadt in der Lausitz) fehlt in
  `cities1000` ganz und kann ohne eine Ergänzungstabelle für Orte nicht aufgenommen werden.
  Schwetz (Świecie) unterliegt Kulm (Chełmno) in 7 km Abstand um 0,001 Bedeutungspunkte, weil
  GeoNames beide Kreisstädte ohne Verwaltungsrang (PPL) führt. Fehlende Einwohnerzahlen von
  Verwaltungssitzen ergänzt `einwohner.csv`; fehlende Ränge und Orte sind nicht korrigiert.
- Deutsche Namen nur mit GeoNames-Beleg: 896 Orte tragen ihren heutigen Namen, weil
  `exonyme.csv` für sie keinen belegten deutschen Namen enthält. Meist führt GeoNames keinen
  (etwa Lubawa/Löbau, Krynica Morska/Kahlberg, Ostrava/Mährisch Ostrau, Bielsko-Biala/Bielitz,
  Lille/Ryssel). Manche Alternativnamen in GeoNames sind bewusst nicht übernommen: Formen ohne
  Diakritika oder in fremder Umschrift (Horazdowitz, Prestitz, Cechtitz), Namen in anderen
  Sprachen (Edingen ist der niederländische Name von Enghien, Bröömt die elsässische Form von
  Brumath), Umbenennungen 1933–1945 (Deutscheneck, Niedersee, Andreashütte). Ergänzen nur mit
  Beleg in `exonyme.csv`. Manche Belege sind Abkürzungen oder
  Nebenformen und stehen so in GeoNames („Raudnitz a.d. Elbe“, „Flatow i. Westpr.“, „Roznau“ für
  Rožnau, „Sobeslau“ neben Sobieslau). Umbenennungen der Jahre 1933–1945 sind nicht aufgenommen;
  „Bad“ im Namen ist meist erst im 19./20. Jahrhundert verliehen und bleibt trotzdem stehen
  (92 Orte).
- Städte, die im 20. Jahrhundert aus mehreren Orten zusammengelegt wurden, tragen ihren heutigen
  Namen (Wuppertal, Kędzierzyn-Koźle, Bielsko-Biala, Castrop-Rauxel, Villingen-Schwenningen,
  Garmisch-Partenkirchen). Ihre historischen Teile führt `cities1000` nur als Ortsteile (PPLX, nicht
  aufgenommen: Cosel, Altona) oder gar nicht (Elberfeld, Barmen); sie sind keine eigenen Orte.
  Frýdek-Místek heißt nach dem belegten Namen seines älteren Teils Friedeck.
- Landschaftsgrenzen folgen heutigen Verwaltungsgrenzen (Kreise, Powiats, Okresy, Départements,
  Gemeinden) und nähern die historischen Grenzen nur an.
- Gelände ist eine Näherung aus Höhen im Raster von etwa 400 m mit festen Schwellen; Börden und
  Heiden nur in den Kreisen von `gelaende_gebiete.csv`.
- Helgoland (1 km²) und Wangerooge (6 km²) sind auf der Karte sehr klein; Archivar und
  Kartengenerator warnen darüber.

### Tests (`test_orte.py`)

Ohne Netz und ohne Zwischenspeicher, unter Python 3.10 und 3.13:

- Namen: Bereinigung moderner Zusätze (Zusätze, die zum Namen gehören, bleiben: „Weil der Stadt“,
  „Stetten am Kalten Markt“), Namensbeleg (auch mit Ortszusatz), Namenswahl, Prüfung von
  `exonyme.csv`, Zusatz bei gleichnamigen Orten; Kennungen (Umschrift, Zusatz Landschaft – Fluss –
  Zahl, Eindeutigkeit bei 300 Zufallsfällen).
- Regeln in `landschaften.csv` (tiefste Ebene, „-“ für leere Felder), Verwaltungsrang und
  Bedeutung, Einwohnerzahlen aus `einwohner.csv`, Abstandsfaktor, Punktgitter, Lückenfüllung auf
  Land weiter als 40 km von jedem Ort; Gelände-Regeln in Vorrangfolge, Gebiete, Haffe.
- Kreisbildung auf Gittern aller Größen bis 8 × 12 (4–12 Orte, zusammenhängend, im Mittel 5–9),
  Ketten, Teile ohne Landverbindung, Unabhängigkeit von der Reihenfolge der Eingaben (Determinismus),
  Exklaven; Kopfkommentare (`dateien.csv`: Umbruch, M3-Absatz wird ersetzt, nicht verdoppelt).
- Eingecheckte Hilfstabellen und erzeugte Tabellen: Umfang (4.200–4.800 Orte), eindeutige Kennungen,
  bekannte Geländetypen, Kreise mit Hauptort, M1-Orte vorn und als einzige mit Historie, Fläche wie
  in `definition.csv`, Stichprobe bekannter Städte (Berlin-Cölln, Hamburg, München, Wien, Prag,
  Breslau, Königsberg, Danzig, Straßburg, Köln, Frankfurt am Main, Nürnberg, Dresden, Leipzig,
  Brüssel, Memel, Glatz, Iglau, Diedenhofen, Liegnitz …).
- `ORTE_VOLLTEST=1`: vollständiger Lauf mit Byte-Vergleich (braucht den Zwischenspeicher).
