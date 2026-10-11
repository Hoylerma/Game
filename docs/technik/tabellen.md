# Tabellen der Datenpipeline (Meilensteine M2 und M3)

Orte und Staaten samt ihrer Geschichte werden in **Tabellen** gepflegt, nicht direkt im
Chronik-Skript. Das Werkzeug **Archivar** (`tools/archivar/archivar.py`) prüft die Tabellen und
erzeugt daraus `data/history/` (Format: [chronik-skript.md](chronik-skript.md), Abschnitte 3.5, 3.6
und 3.8). Die erzeugten Dateien werden eingecheckt; die CI prüft mit `--pruefen`, dass sie zu den
Tabellen passen.

```
data/tabellen/
├── orte.csv               Stammdaten der Orte
├── kreise.csv             Kreise (seit M3)
├── staaten.csv            Stammdaten der Staaten
├── besitz.csv             Besitzer je Ort mit Datum
├── bevoelkerung.csv       Bevölkerung, Konfession, Kultur und Verwüstung je Ort und Stichtag
├── staaten_historie.csv   datierte Werte der Staaten (Herrscher, Hauptstadt, Kasse …)
└── dateien.csv            Kopfkommentar je erzeugter Datei, Standardkultur je Landschaft
```

## Allgemeine Regeln

- **CSV mit Semikolon**, UTF-8 (BOM erlaubt), erste Zeile ist die Kopfzeile mit genau den unten
  genannten Spalten in dieser Reihenfolge. Werte mit Semikolon, Anführungszeichen oder
  Zeilenumbruch stehen in `"…"`, Anführungszeichen darin werden verdoppelt (`""`). So speichern
  LibreOffice Calc und Excel mit „Trennzeichen: Semikolon“. Mehrzeilig darf nur der `kommentar` in
  `dateien.csv` sein; Namen, Kurznamen, Herrscher und Quellen werden Zeichenketten im
  Chronik-Skript und dürfen keinen Zeilenumbruch (auch kein einzelnes CR) enthalten.
- Leerzeichen am Anfang und Ende eines Werts werden entfernt; leere Zeilen werden übergangen.
- **Zahlen mit Dezimalpunkt** (`0.25`, nicht `0,25`), **Daten gregorianisch** als `JJJJ.MM.TT`
  (genau vier, zwei und zwei Ziffern, Jahr ab 0001 – wie der Kern).
- Die **Reihenfolge der Zeilen** in `orte.csv` und `staaten.csv` bestimmt die Reihenfolge in den
  erzeugten Dateien und damit (zusammen mit dem Dateinamen) die Indizes im Spiel und die
  Kartenindizes. Verweise laufen trotzdem immer über Kennungen.
- In den Tabellen mit Geschichte (`besitz`, `bevoelkerung`, `staaten_historie`) trägt **jede
  Zeile** eine **Quelle** (Pflicht, nicht leer) und eine **Sicherheit**:

| Sicherheit | Bedeutung |
|---|---|
| `gesichert` | durch die genannte Quelle belegt und fachlich geprüft |
| `schaetzung` | begründete Schätzung (Größenordnung aus der Literatur), als solche kenntlich |
| `spielwert` | bewusst gesetzter Spielwert ohne historischen Anspruch (z. B. Staatskasse) |
| `ungeprueft` | recherchiert, aber noch nicht fachlich geprüft |

`python3 tools/archivar/archivar.py --bericht` zeigt, wie sich die Zeilen auf diese Stufen verteilen.

## `orte.csv`

| Spalte | Inhalt |
|---|---|
| `id` | Kennung (a–z, 0–9, `_`), eindeutig |
| `datei` | Zieldatei ohne Endung in `history/orte/` (historische Landschaft, z. B. `brandenburg`) |
| `name` | Anzeigename |
| `gelaende` | Kennung aus `common/gelaende.chron` |
| `laenge`, `breite` | Lage des Hauptortes in Grad (WGS84) |
| `flaeche` | Fläche in km² (> 0) |
| `stadt` | `ja` oder `nein` |
| `kreis` | Kreiskennung aus `kreise.csv` (seit M3); leer = kein Kreis (Warnung) |
| `vorkommen` | seit M5: Vorkommen mit Leerzeichen getrennt, z. B. `salz` oder `salz weinlage`; Kennungen aus `vorkommen` in `common/betriebe.chron`; leer = keine |
| `hafen` | seit M5: `ja` (Hafen mit Zugang zum Weltmarkt), `nein` oder leer (= nein) |
| `quelle` | Quelle bzw. Begründung für den Ort (erscheint im Spiel) |

Die Spalten `vorkommen` und `hafen` werden von Hand gepflegt; `tools/geodaten/orte.py` übernimmt sie
beim Neuerzeugen der Tabelle (wie die Spalte `kultur` in `dateien.csv`).

## `kreise.csv` (seit M3)

| Spalte | Inhalt |
|---|---|
| `id` | Kennung (a–z, 0–9, `_`), eindeutig |
| `datei` | Zieldatei ohne Endung in `history/kreise/` |
| `name` | Anzeigename |
| `quelle` | Quelle bzw. Begründung (z. B. „räumlich gebildet aus 7 benachbarten Orten, M3“) |

Ein Kreis ohne Orte ist eine Warnung.

## `staaten.csv`

| Spalte | Inhalt |
|---|---|
| `tag` | Staatstag (2–4 Zeichen A–Z, 0–9), eindeutig |
| `datei` | Zieldatei ohne Endung in `history/staaten/` (üblich: `<TAG>_<name>`) |
| `name`, `kurzname` | Grundnamen; spätere Namen über `staaten_historie.csv` |
| `farbe` | drei Zahlen 0–255, durch Leerzeichen getrennt (`58 95 143`) |
| `quelle` | Quelle bzw. Begründung für den Staat (erscheint im Spiel) |

## `besitz.csv`

| Spalte | Inhalt |
|---|---|
| `ort` | Ortskennung |
| `ab` | Datum, ab dem der Besitzer gilt |
| `besitzer` | Staatstag |
| `sicherheit`, `quelle` | siehe oben |

Je Ort und Datum höchstens eine Zeile. Ein Besitzwechsel ist eine weitere Zeile mit späterem Datum.

## `bevoelkerung.csv`

| Spalte | Inhalt |
|---|---|
| `ort` | Ortskennung |
| `stichtag` | Datum der Angabe |
| `bevoelkerung` | Einwohner von Stadt und Umland (Ganzzahl ≥ 1) |
| `konfession` | Prozentanteile, z. B. `lutherisch=95 reformiert=5`; Summe 100, Mehrheit zuerst |
| `kultur` | seit M4: Prozentanteile wie bei `konfession`, z. B. `obersachsen=50 sorben=50`; leer = Standardkultur der Landschaft aus `dateien.csv` |
| `verwuestung` | Kriegsverwüstung 0 bis 1 |
| `sicherheit`, `quelle` | siehe oben |

Es gilt die jüngste Angabe mit Stichtag ≤ Szenariodatum; zwischen Stichtagen wird (noch) nicht
interpoliert.

**Orte ohne Daten (seit M3):** Ein Ort ohne Zeilen in `besitz.csv` und `bevoelkerung.csv` ist ein
Ort ohne Daten (Spezifikation Abschnitt 3.6): Er erscheint auf der Karte, nimmt aber nicht an der
Simulation teil. Zu jedem Szenariodatum hat ein Ort entweder einen Besitzer **und** eine
Bevölkerungsangabe oder keines von beiden.

## `staaten_historie.csv`

| Spalte | Inhalt |
|---|---|
| `tag` | Staatstag |
| `ab` | Datum, ab dem der Wert gilt |
| `feld` | `name`, `kurzname`, `hauptstadt`, `regierungsform`, `herrscher`, `konfession`, `schatz`, `kontribution`, `akzise`, `zoll`, `hofhaltung`, `heer`, `schulden` (die letzten fünf und die drei Steuern seit M5; `steuersatz` heißt seit M5 `kontribution`) |
| `wert` | Wert im Format des Felds (Ortskennung, Konfessionskennung, Zahl, Text) |
| `sicherheit`, `quelle` | siehe oben |

Je Staat, Datum und Feld höchstens eine Zeile.

## `dateien.csv`

| Spalte | Inhalt |
|---|---|
| `ordner` | `orte`, `staaten` oder `kreise` |
| `datei` | Dateiname ohne Endung |
| `kultur` | seit M4, nur für `orte`: Standardkultur der Landschaft, z. B. `maerker=100` oder `obersachsen=70 sorben=30`; sonst leer |
| `kommentar` | mehrzeiliger Text; erscheint als Kopfkommentar der erzeugten Datei |

Die Spalte `kultur` wird von Hand gepflegt; `tools/geodaten/orte.py` übernimmt sie beim Neuerzeugen
der Tabelle.

## Prüfungen des Archivars

**Fehler** (Abbruch, Exitcode 2):

- Kopfzeile, Spaltenzahl, UTF-8
- Form von Kennungen, Tags, Dateinamen, Zahlen, Daten, Farben, `ja`/`nein`
- Zeilenumbruch in einem Wert, der als Zeichenkette ins Chronik-Skript geht (`name`, `kurzname`,
  `quelle` in `staaten.csv`, `kreise.csv`, `orte.csv`; `name`, `kurzname`, `herrscher` in
  `staaten_historie.csv`)
- Fläche nicht größer als 0, auch so klein, dass sie im Festkomma des Kerns 0 wird (unter
  etwa 0,0000076 km²)
- doppelte Orte, Staaten und doppelte Einträge je Datum
- unbekannte Verweise (Orte, Staaten, Gelände, Konfessionen, Kulturen, Felder)
- Wertebereiche (Verwüstung 0–1, Kontribution, Akzise und Zoll bis `steuersatz_max` aus
  `common/regeln.chron`, Heer 0–1000, Schulden ab 0, Konfessions- und Kultursumme 100)
- seit M5: unbekanntes oder doppeltes Vorkommen in `orte.csv`, `hafen` weder `ja`, `nein` noch leer
- seit M4: Bevölkerungsangabe ohne Kultur (Spalte `kultur` leer und keine Standardkultur der
  Landschaft in `dateien.csv`); Spalte `kultur` in `dateien.csv` bei `staaten` oder `kreise`
- fehlende Sicherheit oder Quelle
- **Lücken**: Zu jedem Datum eines Szenarios in `data/szenarien/` hat jeder Ort entweder Besitzer
  und Bevölkerungsangabe oder keines von beiden (Ort ohne Daten); jeder Staat braucht alle Pflichtfelder (`hauptstadt`, `regierungsform`,
  `herrscher`, `konfession`, `schatz`, `kontribution`, `akzise`, `zoll`, `hofhaltung`, `heer`), und die Hauptstadt muss zu
  diesem Datum dem Staat gehören (also ein Ort mit Daten sein).
- unbekannter Kreis in `orte.csv`

**Warnungen** (Ausreißer, vermutlich Tippfehler):

- Lage außerhalb der Karte (`karte/karte.chron`), Fläche unter 10 oder über 5.000 km²
- Bevölkerungsdichte über 150 Einwohner/km² (Land) bzw. 5.000 Einwohner/km² (Stadt)
- Änderung zwischen zwei Stichtagen über +5 % oder unter −10 % je Jahr
- Staat ohne Ort zu einem Szenariodatum, Kommentar für eine unbenutzte Datei
