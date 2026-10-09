# Kartengenerator

Erzeugt aus den Spieldaten das **Ort-ID-Bild** `provinzen.png` und die Zuordnungstabelle
`definition.csv`, mit denen die Godot-Seite jedem Kartenpixel einen Ort zuordnet. Format und
Kodierung sind in [docs/technik/chronik-skript.md](../../docs/technik/chronik-skript.md),
Abschnitt 6, festgelegt.

Nur Python-Standardbibliothek, Python 3.10 oder neuer. Die Ausgabe ist deterministisch:
zweimal ausführen ergibt byteidentische Dateien. Das Verfahren ist so gebaut, dass jede
Plattform dieselben Pixel liefert (siehe „Determinismus“; geprüft mit Python 3.10 bis 3.13
unter Linux, auch mit verschiedenen zlib-Versionen).

| Datei | Inhalt |
|---|---|
| `kartengenerator.py` | Kommandozeile und Algorithmus |
| `chron.py` | Parser für das Chronik-Skript (Python-Fassung des Parsers aus `core/src/script.cpp`) |
| `test_kartengenerator.py` | Tests (`unittest`) |
| `testdaten/` | kleine Testregion mit 4 Orten und den erwarteten Ausgaben |

## Aufruf

```bash
# Karte neu erzeugen (schreibt data/karte/provinzen.png und data/karte/definition.csv)
python3 tools/kartengenerator/kartengenerator.py

# zusätzlich eine farbige politische Vorschau – nur zum Ansehen, nicht einchecken
python3 tools/kartengenerator/kartengenerator.py --vorschau /tmp/vorschau.png

# CI: sind die eingecheckten Dateien aktuell? (schreibt nichts)
python3 tools/kartengenerator/kartengenerator.py --pruefen
```

| Option | Bedeutung |
|---|---|
| `-h`, `--hilfe` | Hilfe anzeigen |
| `--daten DIR` | Datenordner, Standard: `<repo>/data` |
| `--ausgabe DIR` | Ausgabeordner, Standard: `<daten>/karte` |
| `--vorschau DATEI` | schreibt zusätzlich eine Vorschau (Besitzerfarben, Orts- und Staatsgrenzen, Meer blau, Lagepunkte mit Namen) |
| `--pruefen` | erzeugt die Karte im Speicher und vergleicht sie mit den Dateien im Ausgabeordner, ohne zu schreiben |

**Exitcodes:** `0` Erfolg bzw. Dateien aktuell · `1` `--pruefen`: Dateien fehlen oder sind
veraltet · `2` Fehler in den Daten, im Aufruf oder beim Schreiben.

Fehler und Warnungen stehen auf der Standardfehlerausgabe in der Form
`datei.chron:Zeile: Nachricht` (Syntaxfehler mit Spalte, Pfade relativ zum Datenordner). Dateien, deren Inhalt sich
nicht ändert, werden nicht neu geschrieben.

`--pruefen` vergleicht bei `provinzen.png` die **Pixel**, nicht die Bytes: Andere
zlib-Versionen dürfen anders komprimieren. `definition.csv` wird byteweise verglichen.

## Eingaben

- **`karte/karte.chron`** (Abschnitt 6.1): `bild_breite`, `bild_hoehe` (Ganzzahlen 1–16384),
  `laenge_min` < `laenge_max`, `breite_min` < `breite_max` (Grad), `max_radius_km` (> 0) sind
  Pflicht. `kueste` ist ein Linienzug `{ Länge Breite  Länge Breite … }` mit **streng
  steigenden** Längengraden und mindestens zwei Punkten; ohne `kueste` ist alles Land.
  Unbekannte Felder ergeben eine Warnung.
- **`history/orte/*.chron`** (Abschnitt 3.6): Gelesen werden `name`, `lage`, `flaeche` und
  (für die Vorschau) `besitzer`; die übrigen Felder prüft der Lader des Kerns
  (`uhrwerk-cli pruefen`). Die Dateien werden nach Dateiname (Bytefolge, wie im Lader)
  sortiert, die Einträge in Dateireihenfolge gezählt: Das ergibt den **Kartenindex**.
- **`history/staaten/*.chron`**: nur für `--vorschau` (`farbe`); Probleme sind dort nur Warnungen.

Fatal sind Syntaxfehler, fehlende oder ungültige Pflichtfelder, doppelte oder ungültige
Ortskennungen und ein `;` im Namen eines Ortes (Trennzeichen der CSV).

## Ausgaben

**`provinzen.png`** – RGB, 8 Bit je Kanal, `bild_breite × bild_hoehe` Pixel. Jeder Pixel kodiert

```
wert = R + G × 256 = Kartenindex + 1      (0 = kein Ort: Meer, Ausland, außerhalb)
B    = 0
```

Das PNG enthält nur die Blöcke `IHDR`, `IDAT` und `IEND` (keine Zeitstempel, kein
Farbprofil). Zeilenfilter: erste Zeile *Sub*, danach *Up*; zlib-Stufe 9.

**`definition.csv`** – UTF-8, Semikolon, LF-Zeilenenden, mit Kopfzeile:

```
kartenindex;ort;name
0;dessau;Dessau
```

## Algorithmus

1. **Projektion:** equirektangular. Pixel `(x, y)` hat seinen Mittelpunkt bei
   `Länge = laenge_min + (x + 0,5) · Δλ` und `Breite = breite_max − (y + 0,5) · Δφ`
   (Zeile 0 ist der Nordrand).
2. **Land und Meer:** Die Küstenbreite wird für jede Pixelspalte linear zwischen den Punkten
   von `kueste` interpoliert. Pixel nördlich davon sind Meer, Pixel auf oder südlich der Linie
   und Spalten außerhalb des Längenbereichs des Linienzugs sind Land.
3. **Entfernung:** Kugelerde mit 6371 km Radius, `dx = Δλ · 111,19 km · cos(Breite der Pixelzeile)`,
   `dy = Δφ · 111,19 km`, Entfernung `√(dx² + dy²)`.
4. **Domain Warping:** Ein festes Value-Noise-Feld (ganzzahliger Hash der Gitterpunkte mit
   Startwert 1648, quintische Glättung, 5 Oktaven mit Wellenlängen 48/24/12/6/3 km und
   Amplituden 6/3,5/2/1,1/0,6 km) verschiebt jeden Punkt um höchstens 13,2 km je Achse.
   Verschoben werden **Pixel und Orte** mit demselben Feld; die Zuordnung ist damit ein
   Voronoi-Diagramm im verschobenen Raum. Dadurch werden die Grenzen organisch, und das Pixel
   der Lage eines Ortes gehört immer zu ihm selbst (Entfernung 0).
5. **Zuordnung:** Jedes Landpixel erhält den nächstgelegenen Ort unter allen Orten, deren
   Entfernung (zwischen den verschobenen Lagen) höchstens `max_radius_km` beträgt; sonst 0.
   Bei gleicher Entfernung gewinnt der kleinere Kartenindex. Der Rand zu leerem Land ist
   dadurch ebenfalls unregelmäßig.
6. **Beschleunigung:** Für jede Kachel von 32 × 32 Pixeln werden vorab die Kandidaten bestimmt:
   Orte, deren kleinstmögliche Entfernung zur (um die größte Verschiebung erweiterten) Kachel
   nicht größer ist als die größtmögliche Entfernung des besten Ortes, der die ganze Kachel
   erreicht. Das Ergebnis ist identisch mit der Prüfung aller Orte (Test
   `test_kacheln_gleich_vollsuche`; in der Testregion 0 abweichende Pixel), aber gut viermal
   schneller (Zuordnung etwa 1,8 s statt 8,4 s). Das Rauschen wird zeilenweise über vorab
   berechnete Gitterzeilen ausgewertet (bitgleich zur punktweisen Auswertung).
7. **Bereinigung:** Zerfällt ein Ort in mehrere Teilflächen (4er-Nachbarschaft), bleibt die
   Fläche mit dem Lagepixel (sonst die größte). Jede andere Teilfläche geht an den Nachbarn mit
   der längsten gemeinsamen Grenze; Meer zählt dabei nicht, leeres Land schon (dann wird die
   Teilfläche leer). Kleinste Teilflächen zuerst, höchstens 10 Runden.
8. **Warnungen:** Ort ohne Pixel, Ort mit weniger als 10 % des Medians aller Orte, Lage im Meer
   oder außerhalb des Kartenausschnitts, Lagepixel gehört zu einem anderen Ort, Ort besteht nach
   der Bereinigung noch aus mehreren Teilflächen.

**Gewichtung nach Fläche** (`FLAECHEN_GEWICHT` in `kartengenerator.py`, Entfernung geteilt durch
`√(flaeche / mittlere Fläche)`): vorhanden, aber **abgeschaltet**. In der Testregion hängt die
Zellgröße fast nur vom Abstand der Orte ab; volle Gewichtung hebt die Korrelation zwischen
Pixelzahl und `flaeche` nur von 0,46 auf 0,58 und verschiebt Grenzen um wenige Kilometer.

**Determinismus:** Gerechnet wird nur mit Grundrechenarten auf IEEE-754-Zahlen (in Python auf
allen Plattformen exakt gleich gerundet). Der Kosinus ist eine eigene Taylorreihe statt
`math.cos`, dessen letzte Stelle je C-Bibliothek abweichen kann. Kein Zufall ohne festen
Startwert, keine Zeitstempel, keine Iteration über ungeordnete Mengen mit Einfluss auf das
Ergebnis.

**Laufzeit** (Testregion, 1024 × 1152 Pixel, 60 Orte, Python 3.13): insgesamt etwa 2,5–3 s,
davon Zuordnung 1,8 s, Bereinigung und Statistik 0,5 s, PNG 0,4 s; die Vorschau kostet
zusätzlich 0,6–1 s.

Alle Parameter des Verfahrens (Rauschen, Kachelgröße, Warnschwelle) sind Konstanten am Anfang
von `kartengenerator.py`. Eine Änderung ändert die Karte: danach die Karte und die erwarteten
Ausgaben der Testdaten neu erzeugen (siehe unten).

## Parser (`chron.py`)

Vollständige Grammatik nach Abschnitt 1 mit denselben Knotenformen, Fehlermeldungen und
demselben Wiederaufsetzen nach Fehlern wie der Kern; iterativ, daher auch bei 256
Verschachtelungsebenen ohne Rekursionsgrenze.

```python
import chron

ergebnis = chron.parse_script_file("data/history/orte/brandenburg.chron")
for fehler in ergebnis.errors:            # ParseError, str(): "datei:zeile:spalte: Nachricht"
    print(fehler)
for ort in ergebnis.root.find_all("ort"):  # ScriptNode: key, op, value, is_block, quoted, children, line, column
    print(ort.value, ort.get_string("name"), ort.find("lage").bare_values())
```

Werte: `parse_zahl` (Festkomma als `float`), `parse_ganzzahl`, `parse_bool` (`ja`/`yes`/`wahr`,
`nein`/`no`/`falsch`); dazu `ScriptNode.get_zahl`, `get_int`, `get_bool`.

## Tests

```bash
python3 -m unittest discover -s tools/kartengenerator -v
```

Geprüft werden der Parser (alle Formen und Fehler), das Laden (Reihenfolge, Fehlermeldungen
mit Zeile), Küsten- und Landmaske, Rauschen, Zuordnung (Kacheln gegen Vollsuche, Eigenschaften,
Warnungen, Gewichtung), Bereinigung, PNG-Kodierung und -Dekodierung (alle fünf Zeilenfilter),
`definition.csv`, Determinismus und die Kommandozeile einschließlich `--pruefen`.

`testdaten/karte/provinzen.png` und `definition.csv` sind die erwarteten Ausgaben für die
Testdaten. Nach einer **gewollten** Änderung am Verfahren neu erzeugen:

```bash
python3 tools/kartengenerator/kartengenerator.py --daten tools/kartengenerator/testdaten
```
