# Kartengenerator

Erzeugt aus den Spieldaten die drei Kartendateien, mit denen die Godot-Seite die Großkarte
darstellt und jedem Pixel einen Ort zuordnet. Format und Kodierung sind in
[docs/technik/chronik-skript.md](../../docs/technik/chronik-skript.md), Abschnitt 6 (Fassung M3),
festgelegt.

| Ausgabe (in `data/karte/`) | Inhalt |
|---|---|
| `provinzen.png` | RGB: Ort-ID (`R + G·256` = Kartenindex + 1) und Flächenart im Blaukanal |
| `fluesse.png` | Graustufen: Deckung durch Flüsse (0 … 255), kantengeglättet |
| `definition.csv` | Kartenindex, Ort, Name, Beschriftungspunkt, Pixelzahl, Fläche in km² |

Abhängigkeiten: **Python 3.10 oder neuer und numpy** (`python3 -m pip install numpy`). Die Ausgabe
ist deterministisch und plattformunabhängig (siehe „Determinismus“); geprüft mit Python 3.10.20 /
numpy 2.2.6 und Python 3.13.16 / numpy 2.5.3 (byte-gleiche Dateien).

| Datei | Inhalt |
|---|---|
| `kartengenerator.py` | Laden (karte.chron, GeoJSON, Orte, Staaten), Python-API, Kommandozeile |
| `karte_raster.py` | Verfahren: Rasterung, Rauschen, Zuordnung, Bereinigung, Beschriftungspunkt, Flächen, Flüsse |
| `karte_png.py` | PNG lesen und schreiben (zlib und numpy; Graustufen, RGB, RGBA) |
| `karte_vorschau.py` | politische Vorschau (nur zum Ansehen) |
| `chron.py` | Parser für das Chronik-Skript (Python-Fassung von `core/src/script.cpp`; auch von `tools/archivar` und `tools/geodaten` genutzt) |
| `test_kartengenerator.py`, `test_chron.py` | Tests (`unittest`) |
| `testdaten/` | kleine Testkarte mit Geometrie, 8 Orten und den erwarteten Ausgaben |

## Aufruf

```bash
# Karte neu erzeugen (schreibt data/karte/provinzen.png, fluesse.png, definition.csv)
python3 tools/kartengenerator/kartengenerator.py

# CI: sind die eingecheckten Dateien aktuell? (schreibt nichts)
python3 tools/kartengenerator/kartengenerator.py --pruefen

# Vorschau zum Ansehen (nicht einchecken): ganze Karte auf 2048 Pixel Breite verkleinert …
python3 tools/kartengenerator/kartengenerator.py --pruefen --vorschau /tmp/karte.png --vorschau-breite 2048
# … oder ein Ausschnitt (Grad) mit Ortsnamen, vergrößert
python3 tools/kartengenerator/kartengenerator.py --pruefen --vorschau /tmp/berlin.png \
    --vorschau-ausschnitt 12.5,52.0,14.5,53.0 --vorschau-breite 1600 --vorschau-namen
```

| Option | Bedeutung |
|---|---|
| `-h`, `--hilfe` | Hilfe anzeigen |
| `--daten DIR` | Datenordner, Standard: `<repo>/data` |
| `--ausgabe DIR` | Ausgabeordner, Standard: `<daten>/karte` |
| `--pruefen` | erzeugt die Karte im Speicher und vergleicht sie mit den Dateien im Ausgabeordner, ohne zu schreiben |
| `--vorschau DATEI` | schreibt zusätzlich eine politische Vorschau als PNG |
| `--vorschau-breite N` | Breite der Vorschau: kleiner als der Ausschnitt → Mittelwert über Blöcke, größer → Pixel vervielfacht |
| `--vorschau-ausschnitt L0,B0,L1,B1` | nur dieses Rechteck (Länge min, Breite min, Länge max, Breite max in Grad) |
| `--vorschau-namen` | Ortsnamen am Beschriftungspunkt eintragen (5×7-Pixelschrift) |
| `--zeiten` | Laufzeit der einzelnen Schritte ausgeben |

**Exitcodes:** `0` Erfolg bzw. Dateien aktuell · `1` `--pruefen`: Dateien fehlen oder sind
veraltet · `2` Fehler in den Daten, im Aufruf oder beim Schreiben.

Fehler und Warnungen stehen auf der Standardfehlerausgabe in der Form
`datei.chron:Zeile: Nachricht` (Pfade relativ zum Datenordner). Dateien, deren Pixel bzw. Inhalt
sich nicht ändern, werden nicht neu geschrieben (auch wenn eine andere zlib-Version andere Bytes
erzeugen würde). `--pruefen` vergleicht bei den PNG-Dateien die **Pixel** und bei
`definition.csv` den Inhalt zeilenweise (Zeilenenden egal).

## Eingaben

- **`karte/karte.chron`** (Abschnitt 6.1): `bild_breite`, `bild_hoehe` (Ganzzahlen 1–4096),
  `laenge_min` < `laenge_max`, `breite_min` < `breite_max`, `bezugsbreite` (Grad) und
  `max_radius_km` (> 0) sind Pflicht. Das Seitenverhältnis muss auf 1 % zu
  `(laenge_max − laenge_min) × cos(bezugsbreite) / (breite_max − breite_min)` passen. Das alte Feld
  `kueste` (M1/M2) ergibt nur noch eine Warnung, unbekannte Felder ebenso.
- **`karte/geometrie/*.geojson`** (Abschnitt 6.2, erzeugt von [tools/geodaten](../geodaten/README.md)):
  `land`, `seen`, `kernraum` als `Polygon`/`MultiPolygon`, `fluesse` als
  `LineString`/`MultiLineString` mit Eigenschaft `rang` 1–6 (fehlt oder ungültig: Warnung, Rang 6).
  Alle vier Dateien sind Pflicht.
- **`history/orte/*.chron`** (Abschnitt 3.6): gelesen werden `name`, `lage` und für die Vorschau
  `kreis`, `besitzer` (Grundangabe, dann datierte Blöcke in zeitlicher Reihenfolge – die Vorschau
  zeigt den **jüngsten** Stand) und ob der Ort Simulationsdaten hat. Die Fläche `flaeche` geht
  **nicht** ein; sie wird aus der Karte berechnet. Dateien nach Dateiname (Bytefolge, wie im
  Lader), Einträge in Dateireihenfolge: Das ergibt den **Kartenindex**.
- **`history/staaten/*.chron`**: nur für `--vorschau` (`farbe`); Probleme sind dort nur Warnungen.

Fatal (Exitcode 2) sind Syntaxfehler, fehlende oder ungültige Pflichtfelder, doppelte oder
ungültige Ortskennungen, ein `;` oder Zeilenumbruch im Namen eines Ortes (CSV), ungültiges GeoJSON
und mehr als 65.535 Orte.

## Ausgaben

**`provinzen.png`** – RGB, 8 Bit je Kanal, `bild_breite × bild_hoehe`:

```
wert = R + G × 256      Kartenindex + 1, 0 = kein Ort
B    = Flächenart       0 = Kernraum-Land, 1 = Meer, 2 = Binnengewässer, 3 = Ausland
```

Bei `B ≠ 0` ist `wert = 0`. Kernraum-Land mit `wert = 0` gehört zu keinem Ort (Warnung).

**`fluesse.png`** – Graustufen (PNG-Farbtyp 0), gleiche Größe: `0` kein Fluss, `1 … 255` Deckung.

**`definition.csv`** – UTF-8, Semikolon, LF-Zeilenenden, mit Kopfzeile:

```
kartenindex;ort;name;x;y;pixel;flaeche_km2
0;hafen;Hafenstadt;71;70;3064;341
```

Ein Ort ohne Pixel (Warnung) hat `x = y = -1`, `pixel = 0`, `flaeche_km2 = 0`.

Alle PNG enthalten nur `IHDR`, `IDAT` und `IEND` (keine Zeitstempel, kein Farbprofil);
Zeilenfilter je Zeile *Sub* oder *Up*, zlib-Stufe 6.

## Verfahren

1. **Projektion** (Abschnitt 6.1): `x = (Länge − laenge_min) / (laenge_max − laenge_min) × bild_breite`,
   `y = (breite_max − Breite) / (breite_max − breite_min) × bild_hoehe`. Pixel `(px, py)` deckt
   `[px, px+1) × [py, py+1)`; seine Mitte liegt bei `(px + 0,5, py + 0,5)`.
2. **Rasterung** der Polygone mit der Pixelmitte: Scanline-Füllung in numpy (Schnittpunkte jeder
   Pixelzeile mit allen Kanten, halboffene Kanten `[y0, y1)`, Füllung zwischen Paaren). Je Polygon
   Gerade-ungerade-Regel über seine Ringe (äußerer Ring minus Löcher), über alle Polygone die
   Vereinigung. Pixelmitten **genau auf dem Rand** – auf schrägen oder waagrechten Kanten und auf
   Ecken – zählen als außen. Kanten werden einheitlich ausgerichtet, damit zwei Polygone mit
   gemeinsamer Kante die Pixel lückenlos und ohne Überlappung aufteilen. Geprüft gegen eine exakte
   Referenz mit Brüchen (120 Zufallspolygone mit Ecken und Kanten auf Pixelmitten).
3. **Flächenarten:** *Meer* = nicht in `land`; *Binnengewässer* = in `land` und in `seen`;
   *Ausland* = übriges Land außerhalb von `kernraum`; *Kernraum-Land* = der Rest.
4. **Entfernung:** 111,32 km je Grad; waagrecht `Δλ × 111,32 × cos(Breite der Pixelzeile)`,
   senkrecht `Δφ × 111,32`. Der Kosinus ist eine eigene Taylorreihe (`cos_grad`; zwischen 40° und 60° höchstens 1,1e-16 von `math.cos` entfernt).
5. **Domain Warping:** Ein festes Value-Noise-Feld (ganzzahliger Hash der Gitterpunkte mit
   Startwert 1648, quintische Glättung, 5 Oktaven mit Wellenlängen 40/20/10/5/2,5 km und
   Amplituden 2,6/1,6/0,9/0,5/0,25 km, höchstens 5,85 km je Achse) verschiebt **Pixelmitten und
   Orte** mit demselben Feld. Das Feld wird für das ganze Bild separierbar berechnet (Gitterzeilen
   vorab, dann je Spalte interpoliert) und ist bitgleich zur Auswertung an einzelnen Punkten.
6. **Zuordnung:** Jedes Kernraum-Land-Pixel gehört dem Ort mit der kleinsten Entfernung zwischen
   den verschobenen Lagen, wenn sie höchstens `max_radius_km` beträgt; sonst keinem Ort. Ein Ort im
   Bild steht dabei auf der Mitte seines Lagepixels (Entfernung zu diesem Pixel genau 0).
   Gleichstand: kleinerer Kartenindex. Je Ort wird nur ein Rechteck geprüft, das alle erreichbaren
   Pixel enthält (Radius plus größte Verschiebung, ein Pixel Rand); der Test
   `test_rechteck_gleich_vollsuche` vergleicht mit der Prüfung aller Orte für alle Pixel.
7. **Lagepixel:** Liegt die `lage` eines Ortes auf Kernraum-Land, gehört dieses Pixel ihm (bei
   mehreren Orten im selben Pixel dem ersten, mit Warnung).
8. **Bereinigung:** Je Ort bleibt genau eine zusammenhängende Fläche (4er-Nachbarschaft): die mit
   dem Lagepixel, sonst die größte (Gleichstand: erste in Zeilenreihenfolge). Jede andere
   Teilfläche geht an den Ort, mit dessen Hauptfläche sie die längste Grenze (in Pixelkanten) teilt
   (Gleichstand: kleinerer Kartenindex). Grenzt sie an keine Hauptfläche, wartet sie eine Runde
   (ein Nachbar kann inzwischen angewachsen sein); bleibt sie übrig – etwa eine **Insel ohne
   eigenen Ort** –, gehört sie zu keinem Ort (Warnung). Zusammenhangskomponenten werden über Läufe
   gleicher Werte je Zeile und eine vektorisierte Union-Find-Suche bestimmt.
9. **Beschriftungspunkt:** das Pixel mit dem größten Abstand zum Rand der Fläche, bei Gleichstand
   das erste in Zeilenreihenfolge. Abstand: **Chamfer 3-4** (3 je Schritt waagrecht/senkrecht, 4
   diagonal) zum nächsten Pixel, das nicht zum Ort gehört; der Bildrand zählt als fremd. Exakte
   Zwei-Durchgänge-Transformation, je Zeile vektorisiert (geprüft gegen eine Brute-Force-Referenz).
10. **Fläche:** Summe der Pixelflächen mit `cos(Breite)` der Zeilenmitte. Summiert wird in
    Festkomma (km² × 2²⁴, ganzzahlig und damit exakt in jeder Reihenfolge), dann kaufmännisch auf
    ganze km² gerundet.
11. **Flüsse:** Je Strecke der Abstand jeder Pixelmitte zur Strecke (runde Enden); Deckung
    `clamp(Breite/2 + 0,5 − Abstand, 0, 1)` auf 1/255 gerundet, ohne Wurzel über Schwellen auf dem
    quadrierten Abstand. Linienbreite in Pixeln nach `rang`: 3,0 / 2,5 / 2,0 / 1,6 / 1,3 / 1,0.
    Überlappungen: Maximum.
12. **Warnungen:** Lage außerhalb des Ausschnitts oder nicht auf Kernraum-Land (im Meer, in einem
    Binnengewässer, im Ausland), zwei Orte im selben Lagepixel, Ort ohne Pixel, Ort mit weniger als
    10 % des Medians aller Orte, Kernraum-Land ohne Ort (Summe und die 10 größten Teilflächen mit
    Pixel und Lage).

Alle Parameter (Rauschen, Flussbreiten, Warnschwellen) sind Konstanten am Anfang von
`karte_raster.py` bzw. `kartengenerator.py`. Eine Änderung ändert die Karte: danach die Karte und
die erwarteten Ausgaben der Testdaten neu erzeugen.

**Determinismus:** Im Pixelpfad stehen nur elementweise Grundrechenarten auf IEEE-754-Zahlen
(numpy rundet jede Operation exakt, ohne FMA-Zusammenfassung), Vergleiche, Abrunden und
Ganzzahloperationen. Keine Winkelfunktionen aus numpy oder `math`, keine Wurzel, keine
Gleitkomma-Summen mit plattformabhängiger Reihenfolge, kein Zufall ohne festen Startwert, keine
Zeitstempel. Sortierungen mit festgelegter Gleichstandsregel.

## Laufzeit und Speicher

Gemessen mit Python 3.13 / numpy 2.5 auf 4 Kernen (numpy rechnet hier einkernig); die Spannen
stammen aus mehreren Läufen bei unterschiedlicher Auslastung des Rechners (mit Python 3.10 /
numpy 2.2 im selben Bereich):

| Datensatz | Wandzeit | Spitzenspeicher |
|---|---|---|
| volle Karte 4096 × 3329 mit den Daten von M3 (4.541 Orte), `--pruefen` | 13–21 s | ≈ 610 MB |
| davon: Geometrie / Zuordnung / Bereinigung / Beschriftungspunkte | ≈ 5–10 / 2–4 / 2,7–5 / 1,7–3,7 s | |
| volle Karte 4096 × 3329 (echte Geometrie), 4.500 synthetische Orte | ≈ 10 s | ≈ 620 MB |
| davon: Geometrie rastern + Rauschen + Flüsse | ≈ 3 s | |
| Zuordnung / Bereinigung / Beschriftungspunkte | ≈ 1,5 / 1,7 / 2,3 s | |
| Testdaten (204 × 200, 8 Orte) | < 0,1 s | |

Die Vorschau der vollen Karte kostet zusätzlich etwa 3–5 s.

## Python-API (ohne Dateien)

Für Werkzeuge, die Orte planen und die Karte dafür wiederholt berechnen (z. B. die Orte-Erzeugung):

```python
import sys; sys.path.insert(0, "tools/kartengenerator")
import kartengenerator as kg

# Alles aus dem Datenordner laden und rechnen (schreibt nichts):
ergebnis = kg.erzeuge_karte(Path("data"))

# Oder: Geometrie einmal rastern (≈ 3,5 s, ≈ 320 MB) und mit eigenen Orten mehrfach rechnen
# (volle Karte, 4.500 Orte: ≈ 5 s je Aufruf, Spitzenspeicher ≈ 650 MB):
grundlage = kg.lade_grundlage(Path("data"), fluesse=False)
orte = [kg.Ort("berlin_coelln", "Berlin-Cölln", 13.40, 52.52), ...]   # Reihenfolge = Kartenindex
ergebnis = kg.erzeuge(grundlage, orte)

for f in ergebnis.flaechen:          # je Ort: kartenindex, ort, name, x, y, pixel, flaeche_km2, laenge, breite
    print(f.ort, f.flaeche_km2, f.laenge, f.breite)    # laenge/breite: Mitte des Beschriftungspixels
ergebnis.warnungen                   # Liste deutscher Meldungen (wie auf der Kommandozeile)
ergebnis.ohne_ort                    # Kernraum-Land ohne Ort: Teilflaeche(pixel, flaeche_km2, x, y, laenge, breite)
ergebnis.nachbarn()                  # {(a, b): Grenzlänge in Pixelkanten} für benachbarte Orte a < b
ergebnis.ort_bei(13.4, 52.5)         # Kartenindex an dieser Stelle oder None
ergebnis.werte, ergebnis.art         # numpy-Felder: Kartenindex + 1 je Pixel, Flächenart je Pixel
ergebnis.provinzen()                 # provinzen.png als (H, W, 3)-Feld
kg.schreibe_ausgabe(Path("/tmp/karte"), ergebnis)   # nur wenn gewünscht (braucht fluesse=True)
```

`kg.lade_orte_daten(daten)` liest die Orte wie die Kommandozeile; Fehler kommen als
`kg.DatenFehler` (Liste `meldungen`).

## Vorschau

Staatsfarben je Ort (benachbarte Orte leicht unterschiedlich hell), Orte ohne Daten hell
schraffiert, Kernraum-Land ohne Ort rosa, Ausland gedämpft, Meer und Seen blau, Flüsse; Grenzen
abgestuft: Ortsgrenzen fein (abgedunkelte Füllfarbe), Kreisgrenzen mittel, Staatsgrenzen kräftig
(2 Pixel, auch zu Land ohne Ort), Küste blau, Kernraumgrenze braun; Lagepunkte schwarz. Die
Verkleinerung mittelt Blöcke, damit auch feine Grenzen sichtbar bleiben.

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
# zusätzlich gegen eine zweite Python-Version (sonst wird dieser Test übersprungen):
KARTENGENERATOR_PYTHON_VERGLEICH=/pfad/zu/python3.10 python3 -m unittest discover -s tools/kartengenerator
```

Geprüft werden der Parser, das Laden (Reihenfolge, Fehlermeldungen mit Zeile, karte.chron,
GeoJSON), die Rasterung (exakte Referenz, Löcher, MultiPolygon, gemeinsame Kanten, Flächenarten
der Testdaten), das Rauschen (Raster gleich Punktauswertung, beschränkt), die Zuordnung (Rechteck
gleich Vollsuche, ohne Rauschen exakt nächster Ort, Gleichstand, Radius, Warnungen), die
Bereinigung, Chamfer-Abstand und Beschriftungspunkt, Flächen, Flüsse, PNG (alle fünf Zeilenfilter,
Fehler), Vorschau, Kommandozeile mit `--pruefen` und der Determinismus (zweimal gleich; andere
Python-Version: Python sucht `python3.10` … `python3.14` mit numpy im Suchpfad oder nimmt
`KARTENGENERATOR_PYTHON_VERGLEICH`).

`testdaten/karte/provinzen.png`, `fluesse.png` und `definition.csv` sind die erwarteten Ausgaben
für die Testdaten (Festland mit Haff, Insel und Inselchen, See mit Seeinsel, Ausland mit Grenzsee,
drei Flüsse, acht Orte, einer ohne Daten). Nach einer **gewollten** Änderung am Verfahren neu
erzeugen:

```bash
python3 tools/kartengenerator/kartengenerator.py --daten tools/kartengenerator/testdaten
```
