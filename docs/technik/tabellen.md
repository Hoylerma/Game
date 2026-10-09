# Tabellen der Datenpipeline (Meilenstein M2)

Orte und Staaten samt ihrer Geschichte werden in **Tabellen** gepflegt, nicht direkt im
Chronik-Skript. Das Werkzeug **Archivar** (`tools/archivar/archivar.py`) prüft die Tabellen und
erzeugt daraus `data/history/` (Format: [chronik-skript.md](chronik-skript.md), Abschnitte 3.5, 3.6
und 3.8). Die erzeugten Dateien werden eingecheckt; die CI prüft mit `--pruefen`, dass sie zu den
Tabellen passen.

```
data/tabellen/
├── orte.csv               Stammdaten der Orte
├── staaten.csv            Stammdaten der Staaten
├── besitz.csv             Besitzer je Ort mit Datum
├── bevoelkerung.csv       Bevölkerung, Konfession und Verwüstung je Ort und Stichtag
├── staaten_historie.csv   datierte Werte der Staaten (Herrscher, Hauptstadt, Kasse …)
└── dateien.csv            Kopfkommentar je erzeugter Datei
```

## Allgemeine Regeln

- **CSV mit Semikolon**, UTF-8 (BOM erlaubt), erste Zeile ist die Kopfzeile mit genau den unten
  genannten Spalten in dieser Reihenfolge. Werte mit Semikolon, Anführungszeichen oder
  Zeilenumbruch stehen in `"…"`, Anführungszeichen darin werden verdoppelt (`""`). So speichern
  LibreOffice Calc und Excel mit „Trennzeichen: Semikolon“.
- Leerzeichen am Anfang und Ende eines Werts werden entfernt; leere Zeilen werden übergangen.
- **Zahlen mit Dezimalpunkt** (`0.25`, nicht `0,25`), **Daten gregorianisch** als `JJJJ.MM.TT`.
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
| `quelle` | Quelle bzw. Begründung für den Ort (erscheint im Spiel) |

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
| `verwuestung` | Kriegsverwüstung 0 bis 1 |
| `sicherheit`, `quelle` | siehe oben |

Es gilt die jüngste Angabe mit Stichtag ≤ Szenariodatum; zwischen Stichtagen wird (noch) nicht
interpoliert.

## `staaten_historie.csv`

| Spalte | Inhalt |
|---|---|
| `tag` | Staatstag |
| `ab` | Datum, ab dem der Wert gilt |
| `feld` | `name`, `kurzname`, `hauptstadt`, `regierungsform`, `herrscher`, `konfession`, `schatz`, `steuersatz`, `hofhaltung` |
| `wert` | Wert im Format des Felds (Ortskennung, Konfessionskennung, Zahl, Text) |
| `sicherheit`, `quelle` | siehe oben |

Je Staat, Datum und Feld höchstens eine Zeile.

## `dateien.csv`

| Spalte | Inhalt |
|---|---|
| `ordner` | `orte` oder `staaten` |
| `datei` | Dateiname ohne Endung |
| `kommentar` | mehrzeiliger Text; erscheint als Kopfkommentar der erzeugten Datei |

## Prüfungen des Archivars

**Fehler** (Abbruch, Exitcode 2):

- Kopfzeile, Spaltenzahl, UTF-8
- Form von Kennungen, Tags, Dateinamen, Zahlen, Daten, Farben, `ja`/`nein`
- doppelte Orte, Staaten und doppelte Einträge je Datum
- unbekannte Verweise (Orte, Staaten, Gelände, Konfessionen, Felder)
- Wertebereiche (Verwüstung 0–1, Steuersatz bis `steuersatz_max` aus `common/regeln.chron`,
  Konfessionssumme 100)
- fehlende Sicherheit oder Quelle
- **Lücken**: Zu jedem Datum eines Szenarios in `data/szenarien/` braucht jeder Ort einen Besitzer
  und eine Bevölkerungsangabe, jeder Staat alle Pflichtfelder (`hauptstadt`, `regierungsform`,
  `herrscher`, `konfession`, `schatz`, `steuersatz`, `hofhaltung`), und die Hauptstadt muss zu
  diesem Datum dem Staat gehören.

**Warnungen** (Ausreißer, vermutlich Tippfehler):

- Lage außerhalb der Karte (`karte/karte.chron`), Fläche unter 10 oder über 5.000 km²
- Bevölkerungsdichte über 150 Einwohner/km² (Land) bzw. 5.000 Einwohner/km² (Stadt)
- Änderung zwischen zwei Stichtagen über +5 % oder unter −10 % je Jahr
- Staat ohne Ort zu einem Szenariodatum, Kommentar für eine unbenutzte Datei
