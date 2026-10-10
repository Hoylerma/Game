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
| `test_geodaten.py`, `testdaten/` | Tests ohne Netz und ohne Zwischenspeicher |

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

python3 -m unittest discover -s tools/geodaten         # Tests (auch mit Python 3.10)
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
  vereinfachte Grenzlinie trennt z. B. Laufenburg (Baden) und Laufenburg (Aargau) nicht.
  Stand: 15.044 Kandidaten. Die Namen sind heutige Namen; historische deutsche Namen stehen
  unkenntlich unter den Alternativnamen (`Wrocław` → … `Breslau` …) und müssen bewusst gewählt
  und belegt werden.
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
  Belluno) zählen zu Norditalien und bleiben Ausland.
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
