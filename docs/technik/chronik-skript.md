# Chronik-Skript und Datenformate (Meilensteine M1 bis M3)

Verbindliche Spezifikation für Parser (`core/src/script.cpp`), Lader (`core/src/loader.cpp`), Inhalte (`data/`) und Werkzeuge (`tools/`). Änderungen hier ziehen Änderungen in Code und Tests nach sich.

## 1. Syntax

### 1.1 Lexik

| Element | Regel |
|---|---|
| Kodierung | UTF-8; ein BOM am Dateianfang wird ignoriert |
| Leerraum | Leerzeichen, Tab, `\r`, `\n` trennen Tokens |
| Kommentar | `#` bis Zeilenende (außerhalb von Zeichenketten) |
| Klammern | `{` und `}` |
| Operatoren | `=`, `<`, `>`, `<=`, `>=`, `!=` |
| Zeichenkette | `"…"`; Escapes `\"` und `\\`; darf keinen Zeilenumbruch enthalten |
| Wort | Jede Folge von Zeichen ohne Leerraum und ohne `{ } = < > ! # "`. Umfasst Bezeichner (`berlin_coelln`, `BRA`), Zahlen (`-0.25`, `12000`), Daten (`1648.10.24`) und UTF-8-Text (`Cölln`) |

Zeilen- und Spaltennummern beginnen bei 1; die Spalte zählt Bytes.

### 1.2 Grammatik

```
datei      := anweisung*
block      := "{" anweisung* "}"
anweisung  := WORT OP wert              # key = value, key >= value …
            | WORT block                # key { … }      (wie key = { … })
            | WORT WORT block           # typ name { … }
            | wert                      # bare: Listeneintrag
            | block                     # anonymer Block
wert       := WORT | STRING | block
OP         := "=" | "<" | ">" | "<=" | ">=" | "!="
```

Entscheidung per Vorausschau: Nach einem `WORT` entscheidet das nächste Token: Operator → Zuweisung; `{` → Block; ein zweites `WORT` gefolgt von `{` → typisierter Block; sonst ist das erste `WORT` ein bare-Eintrag.

Ein `STRING` kann nur als Wert stehen (nicht als Schlüssel). `key = "Text"` setzt `quoted = true`.

### 1.3 Abbildung auf `ScriptNode`

| Quelltext | key | op | value | is_block |
|---|---|---|---|---|
| `name = "Berlin"` | `name` | `=` | `Berlin` (quoted) | nein |
| `jahr >= 1700` | `jahr` | `>=` | `1700` | nein |
| `lage = { 13.4 52.5 }` | `lage` | `=` | – | ja; Kinder sind zwei bare-Knoten |
| `regeln { … }` | `regeln` | `=` | – | ja |
| `ort berlin_coelln { … }` | `ort` | `=` | `berlin_coelln` | ja |
| `lutherisch` (in einem Block) | – | – | `lutherisch` | nein |

### 1.4 Fehler

Fehlermeldungen sind deutsch und haben die Form `datei.chron:Zeile:Spalte: Nachricht`. Beispiele: `nicht geschlossener Block (geöffnet in Zeile 3)`, `unerwartetes '}'`, `Wert nach '=' fehlt`, `nicht beendete Zeichenkette`. Der Parser setzt nach einem Fehler fort und meldet weitere Fehler.

### 1.5 Werte

| Typ | Format | Beispiel |
|---|---|---|
| Ganzzahl | optionales `-`, Ziffern | `12000` |
| Festkomma | optionales `-`, Ziffern, optional `.` und Nachkommaziffern | `0.10`, `-2.5` |
| Datum | `JJJJ.MM.TT`: genau vier, zwei und zwei Ziffern, gregorianisch, Jahr 0001–9999 | `1648.10.24`, `0800.12.25` |
| Wahrheitswert | `ja`/`nein` (auch `yes`/`no`, `wahr`/`falsch`) | `stadt = ja` |
| Farbe | Block mit drei Ganzzahlen 0–255 | `farbe = { 58 95 143 }` |
| Verweis | Bezeichner eines anderen Objekts | `besitzer = BRA` |

## 2. Datenordner

```
data/
├── common/
│   ├── regeln.chron        globale Regeln und Balancing-Werte
│   ├── gelaende.chron      Geländetypen
│   ├── konfessionen.chron  Konfessionen
│   └── berufe.chron        Berufsgruppen
├── history/                generiert aus tabellen/ (tools/archivar), nicht von Hand bearbeiten
│   ├── staaten/*.chron     Staaten (Reichsstände) mit datierter Historie
│   ├── kreise/*.chron      Kreise: Gruppen von 4–12 Orten (seit M3)
│   └── orte/*.chron        Orte mit datierter Historie (Besitz, Bevölkerung …)
├── tabellen/*.csv          Quelle der Wahrheit für history/ (siehe tabellen.md)
├── szenarien/*.chron       Lesezeichen
└── karte/
    ├── karte.chron         Ausschnitt, Projektion und Bildgröße
    ├── geometrie/          GeoJSON: Land, Seen, Flüsse (aus Natural Earth), Kernraum (von Hand)
    ├── provinzen.png       generiert: Ort-ID-Bild mit Flächenart
    ├── fluesse.png         generiert: Flussebene
    └── definition.csv      generiert: Kartenindex → Ort, Beschriftungspunkt, Fläche
```

Ladereihenfolge: `regeln`, `gelaende`, `konfessionen`, `berufe`, dann alle Dateien der Ordner `szenarien`, `history/staaten`, `history/kreise` (seit M3), `history/orte`, jeweils **nach Dateiname sortiert**, Einträge in Dateireihenfolge. Daraus ergeben sich die Indizes in `World`. Die Szenarien kommen vor der Historie, weil das Startdatum des gewählten Szenarios bestimmt, welche datierten Blöcke gelten (Abschnitt 3.8).

Die Dateien in `history/` erzeugt das Werkzeug `tools/archivar` aus den Tabellen in `data/tabellen/` (Format: [tabellen.md](tabellen.md)). Änderungen gehören in die Tabellen; die CI prüft mit `archivar.py --pruefen`, dass `history/` aktuell ist.

## 3. Dateischemata

Pflichtfelder sind fett. Unbekannte Felder erzeugen eine **Warnung** (nicht fatal). Fehlende Pflichtfelder, ungültige Werte, doppelte Kennungen und unbekannte Verweise sind **fatal**.

### 3.1 `common/regeln.chron`

```text
regeln {
    wachstum_basis        = 0.004   # jährliches Grundwachstum
    wachstum_wiederaufbau = 0.025   # zusätzlich je Verwüstungsgrad (Wiederaufbau, Peuplierung)
    wachstum_steuerdruck  = 0.02    # Abzug je Steuersatz
    verwuestung_abbau     = 0.03    # jährlicher relativer Abbau der Verwüstung
    verwuestung_ertrag    = 0.5     # Produktionsverlust je Verwüstungsgrad
    seuche_chance         = 0.01    # jährlich je Ort
    seuche_verlust_min    = 0.04
    seuche_verlust_max    = 0.12
    steuersatz_max        = 0.5
}
```

Alle Felder sind Pflicht. Abbildung auf `Rules` in derselben Reihenfolge (`growth_base` … `tax_rate_max`).

### 3.2 `common/gelaende.chron`

```text
gelaende flussaue {
    name     = "Flussaue"          # Pflicht
    farbe    = { 128 160 112 }     # Pflicht
    wachstum = 1.05                # Pflicht, Faktor
    ertrag   = 1.15                # Pflicht, Faktor
}
```

### 3.3 `common/konfessionen.chron`

```text
konfession lutherisch {
    name  = "Lutherisch"           # Pflicht
    farbe = { 70 110 170 }         # Pflicht
}
```

### 3.4 `common/berufe.chron`

```text
beruf bauern {
    name            = "Bauern"     # Pflicht
    schicht         = unten        # Pflicht: unten | mittel | oben
    ertrag          = 6            # Pflicht: Reichstaler pro Kopf und Jahr
    steuerpflichtig = ja           # Pflicht
    anteil_stadt    = 0.10         # Pflicht: Anteil an der Bevölkerung städtischer Orte
    anteil_land     = 0.60         # Pflicht: Anteil an der Bevölkerung ländlicher Orte
}
```

Die Summe aller `anteil_stadt` und die Summe aller `anteil_land` muss jeweils 1 ergeben (Toleranz ±0,001), sonst fatal.

### 3.5 `history/staaten/*.chron`

```text
staat BRA {
    name           = "Kurfürstentum Brandenburg"   # Pflicht
    kurzname       = "Brandenburg"                 # Pflicht
    farbe          = { 58 95 143 }                 # Pflicht
    hauptstadt     = berlin_coelln                 # Pflicht, Verweis auf einen Ort dieses Staates
    regierungsform = absolute_monarchie            # Pflicht (Bezeichner)
    herrscher      = "Friedrich Wilhelm"           # Pflicht
    konfession     = reformiert                    # Pflicht, Konfession des Herrscherhauses
    schatz         = 20000                         # Pflicht, Reichstaler
    steuersatz     = 0.10                          # Pflicht, 0..steuersatz_max
    hofhaltung     = 30000                         # Pflicht, Ausgaben pro Jahr
    quelle         = "…"                           # Pflicht (oder "Testdaten, geschätzt")
}
```

Der Tag ist der Wert hinter `staat` (Großbuchstaben und Ziffern, 2–4 Zeichen). Die Hauptstadt wird nach dem Laden der Orte geprüft.

### 3.6 `history/orte/*.chron`

```text
ort berlin_coelln {
    name         = "Berlin-Cölln"                  # Pflicht
    gelaende     = flussaue                        # Pflicht
    lage         = { 13.40 52.52 }                 # Pflicht: Längengrad Ost, Breitengrad Nord (WGS84)
    flaeche      = 150                             # Pflicht, km²
    stadt        = ja                              # Pflicht
    kreis        = kr_berlin                       # optional (seit M3), Kreiskennung, nicht datierbar
    quelle       = "Testdaten, geschätzt"          # Pflicht

    # Simulationsfelder: alle vier oder keines (siehe unten)
    besitzer     = BRA                             # Staatstag
    bevoelkerung = 12000                           # > 0
    konfession   = { lutherisch = 90 reformiert = 10 }   # Prozentwerte, Summe 100
    verwuestung  = 0.4                             # 0..1
}
```

Der Ort-Bezeichner ist der Wert hinter `ort` (Kleinbuchstaben, Ziffern, `_`).

**Orte ohne Daten (seit M3).** Die vier Simulationsfelder `besitzer`, `bevoelkerung`,
`konfession` und `verwuestung` (direkt oder aus datierten Blöcken, Abschnitt 3.8) sind zum
Startdatum entweder **alle** vorhanden oder **keines**. Fehlen alle, ist der Ort ein *Ort ohne
Daten*: Besitzer `kNone`, keine Bevölkerungsblöcke, Verwüstung 0. Er erscheint auf der Karte,
nimmt aber an der Simulation nicht teil. Fehlen nur einige, ist das ein Fehler
(`Ort potsdam hat zum Startdatum 'besitzer', aber nicht 'bevoelkerung'`). So lässt sich die Karte
des ganzen Kernraums laden, auch wenn erst ein Teil der Orte historische Daten hat.

### 3.7 `szenarien/*.chron`

```text
szenario westfaelischer_frieden {
    name    = "Der Westfälische Frieden"   # Pflicht
    datum   = 1648.10.24                   # Pflicht
    spieler = BRA                          # Pflicht, Staatstag (Vorauswahl)
}
```

### 3.8 Datierte Blöcke (seit M2)

In `staat`- und `ort`-Definitionen darf ein Block stehen, dessen Schlüssel ein Datum `JJJJ.MM.TT` ist. Er enthält Felder desselben Schemas, die **ab diesem Datum** gelten:

```text
ort potsdam {
    name     = "Potsdam"
    gelaende = sandboden
    lage     = { 13.06 52.40 }
    flaeche  = 400
    stadt    = nein
    quelle   = "…"

    1648.10.24 = {
        besitzer     = BRA
        bevoelkerung = 1500
        konfession   = { lutherisch = 100 }
        verwuestung  = 0.6
    }
    1701.01.18 = { besitzer = PRE }
}
```

Regeln:

- Der Lader wendet alle Blöcke mit Datum **≤ Startdatum** des gewählten Szenarios in **zeitlicher** Reihenfolge an (nicht in Dateireihenfolge). Ein Feld aus einem Block ersetzt dasselbe Feld aus der Grundangabe oder aus einem früheren Block; Felder ohne Datum gelten von Anfang an.
- Pflichtfelder müssen **zum Startdatum** vorhanden sein, gleich ob aus der Grundangabe oder einem Block.
- Nicht datierbar: `lage` und `flaeche` (Ort), `farbe` (Staat).
- Fehler (für alle Blöcke, auch künftige): ungültiges Datum, Wert statt Block, dasselbe Datum zweimal, ein Feld zweimal im selben Block, nicht datierbares Feld. Werte und Verweise (z. B. `besitzer`) prüft der Lader nur für das, was zum Startdatum gilt; die vollständige Prüfung aller Zeiträume übernimmt `archivar.py`.
- Ohne gültiges Szenario wendet der Lader alle Blöcke an, damit `uhrwerk-cli pruefen` trotzdem alle Felder sieht.

Offen für M7: Staaten, die zum Startdatum noch nicht oder nicht mehr bestehen (heute wird jeder Staat geladen, auch ohne Orte).

### 3.9 `history/kreise/*.chron` (seit M3)

```text
kreis kr_berlin {
    name   = "Berlin"            # Pflicht, Anzeigename (meist der Hauptort)
    quelle = "…"                 # Pflicht
}
```

Ein Kreis bündelt 4–12 benachbarte Orte (Design-Dokument 05, Abschnitt 2) und dient in M3 der
Kartendarstellung (Grenzen, Beschriftung nach Zoomstufe); später ist er Grundlage für Verwaltung und
Wahlkreise. Kennung: Kleinbuchstaben, Ziffern, `_`, eindeutig. Kreise sind in M3 **nicht
datiert**. `ort.kreis` muss auf einen bekannten Kreis verweisen (sonst Fehler); ein Kreis ohne Orte
ist eine Warnung. Im Kern: `World::districts` (`District { id, name, source }`) und
`Location::district` (`kNone`, wenn der Ort keinen Kreis hat). Der Ordner `history/kreise` darf
fehlen oder leer sein (ältere Datenstände, Testdaten); dann hat kein Ort einen Kreis.

## 4. Startzustand

Für jeden Ort entstehen Bevölkerungsblöcke **in dieser Reihenfolge**: für jeden Beruf (Ladereihenfolge) und darin für jede Konfession des Ortes (Reihenfolge im `konfession`-Block):

```
größe = bevoelkerung × (stadt ? anteil_stadt : anteil_land) × prozent / 100
```

Blöcke mit Anteil 0 oder Prozent 0 entfallen. Die Blöcke eines Ortes liegen zusammenhängend in `World::pops`.

Das Weltdatum wird auf `szenario.datum` gesetzt; der Zustand von `state.last_month_*` ist 0; die Chronik ist leer.

## 5. Formeln der Simulation (M1)

Alle Rechnungen in `Fixed`. Reihenfolge der Operationen wie angegeben (wichtig für Bit-Gleichheit).

**Produktion eines Bevölkerungsblocks pro Monat**

```
p = größe × beruf.ertrag × gelände.ertrag × (1 − verwüstung × regeln.verwuestung_ertrag) / 12
```

**Steuer eines Blocks pro Monat:** `p × staat.steuersatz`, nur wenn `beruf.steuerpflichtig`.

**Wirtschaft eines Staates** (an seinem Rechentag):

```
einnahmen = Σ Steuern aller Blöcke aller Orte des Staates (Orte aufsteigend, Blöcke aufsteigend)
ausgaben  = hofhaltung / 12
schatz   += einnahmen − ausgaben
```

**Demografie eines Ortes** (an seinem Rechentag):

```
rate  = (wachstum_basis + wachstum_wiederaufbau × verwüstung) × gelände.wachstum
        − wachstum_steuerdruck × besitzer.steuersatz
größe += größe × rate / 12        (für jeden Block; nie unter 0)
```

**Orte ohne Daten** (Abschnitt 3.6) haben keine Bevölkerungsblöcke: Sie tragen nichts zu Demografie
und Wirtschaft bei, und beim Jahreswechsel werden sie **übersprungen** – sie ziehen keine
Zufallszahlen, und ihre Verwüstung (0) bleibt unverändert. Ein Ort, der Blöcke hat, deren Größe
auf 0 gefallen ist, wird dagegen normal behandelt.

**Jahreswechsel** (wenn das aktuelle Datum vor dem Weiterschalten der 1. Januar ist), Orte in aufsteigender Reihenfolge:

1. Seuche: `rng_ereignisse.chance(seuche_chance)`; bei Treffer `verlust = rng_ereignisse.uniform(min, max)`, jeder Block `größe −= größe × verlust`, Chronikeintrag: `Seuche in {Ortsname}: {gerundeter Prozentwert} % der Bevölkerung sterben.`
2. Verwüstung: `verwüstung −= verwüstung × verwuestung_abbau`.

## 6. Karte (Fassung M3)

Die Karte zeigt den **Kernraum** (Design-Dokument 05: Altes Reich nördlich der Alpen mit Österreich,
Böhmen, Mähren, Schlesien, Lothringen, Elsass, Luxemburg; Preußen mit Ost- und Westpreußen, Posen,
Schleswig; angrenzende Grenzregionen) mit etwa 4.500 Orten. Land außerhalb des Kernraums ist
*Ausland* und hat keine Orte.

### 6.1 `karte/karte.chron`

```text
karte {
    bild_breite   = 4096
    bild_hoehe    = 3597
    laenge_min    = 4.5      # Grad Ost, linker Bildrand
    laenge_max    = 23.5
    breite_min    = 45.5     # Grad Nord, unterer Bildrand
    breite_max    = 56.0
    bezugsbreite  = 51.0     # Grad Nord, siehe Projektion
    max_radius_km = 25       # weiter entfernte Pixel gehören zu keinem Ort
}
```

Alle Felder sind Pflicht; `bild_breite` und `bild_hoehe` höchstens 4096 (Texturgrenze).

**Projektion** (verbindlich für Generator und Godot): Plattkarte mit Bezugsbreite.

```
x = (länge − laenge_min) / (laenge_max − laenge_min) × bild_breite
y = (breite_max − breite) / (breite_max − breite_min) × bild_hoehe
```

Pixel `(px, py)` deckt `[px, px+1) × [py, py+1)` ab; seine Mitte liegt bei `(px + 0,5, py + 0,5)`.
Damit Pixel annähernd quadratisch sind, muss gelten
`bild_breite / bild_hoehe = (laenge_max − laenge_min) × cos(bezugsbreite) / (breite_max − breite_min)`
mit höchstens 1 % Abweichung. Entfernungen und Flächen in km rechnet der Generator mit
111,32 km je Grad und `cos(Breite)` für Längengrade.

### 6.2 `karte/geometrie/*.geojson`

GeoJSON (RFC 7946, WGS84 Länge/Breite), je Datei eine `FeatureCollection`:

| Datei | Inhalt | Herkunft |
|---|---|---|
| `land.geojson` | Festland und Inseln (`Polygon`/`MultiPolygon`) | Natural Earth 10m `land` + `minor_islands`, auf den Kartenausschnitt zugeschnitten, erzeugt von `tools/geodaten` |
| `seen.geojson` | Binnengewässer (`Polygon`/`MultiPolygon`) | Natural Earth 10m `lakes` + `lakes_europe`, erzeugt |
| `fluesse.geojson` | Flüsse (`LineString`/`MultiLineString`), Eigenschaft `rang` (1 = größter Fluss … 6) | Natural Earth 10m `rivers_lake_centerlines` + `rivers_europe`, erzeugt |
| `kernraum.geojson` | Umriss des Kernraums (`Polygon`/`MultiPolygon`) | von Hand gepflegt |

Rasterregel: Ein Pixel gehört zu einem Polygon, wenn seine **Mitte** innerhalb liegt
(äußerer Ring minus Löcher; Punkte genau auf dem Rand zählen als außen). Daraus:
*Meer* = nicht in `land`; *Binnengewässer* = in `land` und in `seen`;
*Ausland* = Land, nicht Binnengewässer, nicht in `kernraum`; *Kernraum-Land* = der Rest.

### 6.3 `karte/provinzen.png` (generiert)

RGB, 8 Bit je Kanal, Größe `bild_breite × bild_hoehe`:

```
wert = R + G × 256        # Kartenindex + 1; 0 = kein Ort
kartenindex = wert − 1
B = Flächenart            # 0 = Kernraum-Land, 1 = Meer, 2 = Binnengewässer, 3 = Ausland
```

Regeln: Bei `B ≠ 0` ist `wert = 0`. Jedes Kernraum-Land-Pixel gehört dem nächstgelegenen Ort
(Entfernung in km, mit deterministischem Rauschen für natürliche Grenzen), höchstens
`max_radius_km` entfernt; sonst `wert = 0` mit Warnung. Jeder Ort hat **genau eine
zusammenhängende Fläche** (4er-Nachbarschaft) mit mindestens einem Pixel; liegt seine `lage` auf
Kernraum-Land, gehört dieses Pixel ihm. Abgetrennte Teilflächen eines Ortes gehen an den Nachbarort
mit der längsten gemeinsamen Grenze; grenzen sie an keinen Ort (z. B. eine Insel ohne eigenen Ort),
gehören sie zu keinem Ort (`wert = 0` mit Warnung). Die Fläche (`flaeche`) der Orte geht **nicht**
in die Zuordnung ein (sie wird aus der Karte berechnet, Abschnitt 6.5).

### 6.4 `karte/fluesse.png` (generiert)

Graustufen, 8 Bit (PNG-Farbtyp 0), gleiche Größe: `0` = kein Fluss, `1…255` = Deckung des Pixels
durch Flüsse (Breite nach `rang`, kantengeglättet). Nur Darstellung; keine Wirkung auf die
Simulation.

### 6.5 `karte/definition.csv` (generiert)

Semikolon-getrennt, UTF-8, mit Kopfzeile:

```
kartenindex;ort;name;x;y;pixel;flaeche_km2
0;berlin_coelln;Berlin-Cölln;2118;1043;1734;212
```

- `x`, `y`: Beschriftungspunkt in Pixeln (Ganzzahlen); das Pixel gehört garantiert zum Ort
  (Pixel mit größtem Abstand zum Rand der Fläche, bei Gleichstand das erste in Zeilenreihenfolge).
- `pixel`: Anzahl der Pixel des Ortes; `flaeche_km2`: Fläche in km², auf ganze km² gerundet.
- Ein Ort ohne Pixel (Warnung des Generators) hat `x = y = -1`, `pixel = 0` und `flaeche_km2 = 0`.

Der Kartenindex folgt der Ladereihenfolge der Orte (Dateien sortiert, Einträge in
Dateireihenfolge). Die Godot-Seite ordnet Kartenindex und Simulationsindex trotzdem immer über die
Ort-Kennung zu.

### 6.6 Erzeugung und Prüfung

`python3 tools/kartengenerator/kartengenerator.py` erzeugt `provinzen.png`, `fluesse.png` und
`definition.csv` deterministisch (gleiche Eingaben → pixelgleiche Bilder auf allen Plattformen;
`--pruefen` vergleicht Pixel, nicht PNG-Bytes). Abhängigkeiten: Python ab 3.10 und **numpy**;
im Pixelpfad nur Grundrechenarten, Vergleiche und Rundung (keine Winkelfunktionen aus numpy oder
`math`, deren Ergebnis plattformabhängig sein kann). Ziel: volle Karte in unter einer Minute auf
vier Kernen.
