# Tabellen der Datenpipeline (Meilensteine M2 bis M7)

Orte und Staaten samt ihrer Geschichte werden in **Tabellen** gepflegt, nicht direkt im
Chronik-Skript. Das Werkzeug **Archivar** (`tools/archivar/archivar.py`) prüft die Tabellen und
erzeugt daraus `data/history/` (Format: [chronik-skript.md](chronik-skript.md), Abschnitte 3.5, 3.6,
3.8, 3.9 und 3.16). Die erzeugten Dateien werden eingecheckt; die CI prüft mit `--pruefen`, dass sie zu den
Tabellen passen.

```
data/tabellen/
├── orte.csv               Stammdaten der Orte
├── kreise.csv             Kreise (seit M3)
├── verwaltung.csv         Verwaltungseinheiten der Staaten (seit M7)
├── staaten.csv            Stammdaten der Staaten
├── besitz.csv             Besitzer, Verwaltung und Bundesgebiet je Ort mit Datum
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

## `verwaltung.csv` (seit M7)

| Spalte | Inhalt |
|---|---|
| `id` | Kennung (a–z, 0–9, `_`), eindeutig, nicht `keine`; üblich mit Staatskürzel, z. B. `pr_rb_potsdam` |
| `datei` | Zieldatei ohne Endung in `history/verwaltung/` |
| `name` | Anzeigename, z. B. „Regierungsbezirk Potsdam“ |
| `art` | Bezeichner der Ebene, z. B. `provinz`, `regierungsbezirk`, `kreis`, `kronland` |
| `ueber` | übergeordnete Einheit; leer bei der obersten |
| `staat` | Staatstag, zu dem die Einheit gehört (nur zur Prüfung, geht nicht ins Chronik-Skript) |
| `sitz` | Ort des Verwaltungssitzes (Kreisstadt), leer erlaubt; nutzt `tools/geodaten/zuordnung.py` für die Zuordnung nach dem nächsten Sitz, geht nicht ins Chronik-Skript |
| `sicherheit`, `quelle` | siehe oben |

Eine Einheit liegt im selben Staat wie die übergeordnete; der Baum hat keinen Zyklus. Die
Einheiten sind nicht datiert; welche gilt, steht datiert in `besitz.csv`.

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
| `verwaltung` | seit M7: Verwaltungseinheit der untersten Ebene aus `verwaltung.csv`, die zum Besitzer gehört; leer = keine |
| `bund` | seit M7: `ja`, wenn der Ort im Gebiet des Deutschen Bundes liegt; leer oder `nein` = nicht |
| `sicherheit`, `quelle` | siehe oben |

Je Ort und Datum höchstens eine Zeile. Ein Besitzwechsel ist eine weitere Zeile mit späterem Datum.
Jede Zeile beschreibt den Ort vollständig ab ihrem Datum: Eine spätere Zeile ohne `verwaltung` hebt
die frühere Einheit auf (der Archivar schreibt dann `verwaltung = keine`). Ins Chronik-Skript gehen
`verwaltung` und `bund` nur, wenn sie sich gegenüber der vorigen Zeile ändern.

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
| `feld` | `name`, `kurzname`, `hauptstadt`, `regierungsform`, `herrscher`, `konfession`, `schatz`, `kontribution`, `akzise`, `zoll`, `hofhaltung`, `heer`, `schulden` (die letzten fünf und die drei Steuern seit M5; `steuersatz` heißt seit M5 `kontribution`), `zollverein` (seit M7, `ja`/`nein`) |
| `wert` | Wert im Format des Felds (Ortskennung, Konfessionskennung, Zahl, Text) |
| `sicherheit`, `quelle` | siehe oben |

Je Staat, Datum und Feld höchstens eine Zeile.

## `dateien.csv`

| Spalte | Inhalt |
|---|---|
| `ordner` | `orte`, `staaten`, `kreise` oder `verwaltung` (seit M7) |
| `datei` | Dateiname ohne Endung |
| `kultur` | seit M4, nur für `orte`: Standardkultur der Landschaft, z. B. `maerker=100` oder `obersachsen=70 sorben=30`; sonst leer |
| `kommentar` | mehrzeiliger Text; erscheint als Kopfkommentar der erzeugten Datei |

Die Spalte `kultur` wird von Hand gepflegt; `tools/geodaten/orte.py` übernimmt sie beim Neuerzeugen
der Tabelle.

## Eingaben für das Lesezeichen 1848 (seit M7)

Diese Tabellen liest nicht der Archivar, sondern die Werkzeuge `tools/geodaten/zuordnung.py` und
`bevoelkerung_1846.py` ([README](../../tools/geodaten/README.md#lesezeichen-1848-zuordnungpy-bevoelkerung_1846py)).
Sie erzeugen daraus die Zeilen von `besitz.csv` zum 1848.03.01 und von `bevoelkerung.csv` zum
1846.12.03; diese Zeilen nie von Hand ändern, sondern die Eingaben.

| Tabelle | Spalten | Inhalt |
|---|---|---|
| `territorien_1848.csv` | `gebiet;staat;bund;verwaltung;sicherheit;quelle` | Regeln für Besitzer, Bundesgebiet und Verwaltung. `gebiet`: `ort:<kennung>`, heutiger Kreis `DE.15.00.16066`, Bezirk `DE.02.094`, `landschaft:<datei>`, Bundesland `DE.02`, Staat `DE`. Leere Felder setzen nichts; `staat = -` macht einen Ort ohne Daten, `verwaltung = -` hebt die Einheit auf. |
| `einwohner_1846.csv` | `raum;einwohner;sicherheit;quelle` | Einwohnersumme je Raum: Verwaltungseinheit (`pr_rheinprovinz`) oder Staat (`BAV`). Ein Ort gehört zum ersten Raum seiner Verwaltungskette, sonst zu seinem Staat. |
| `staedte_1846.csv` | `ort;einwohner;sicherheit;quelle` | feste Einwohnerzahl der Stadt; die Umgebung des Ortes kommt dazu |
| `merkmale_1846.csv` | `gebiet;konfession;kultur;dichte;sicherheit;quelle` | Konfession und Kultur (Anteile wie in `bevoelkerung.csv`) und ländlicher Dichtefaktor; `gebiet` wie oben, dazu `verwaltung:<id>` und `staat:<TAG>`. Rang: Ort > Kreis > Bezirk > Landschaft > Verwaltung > Staat > Bundesland > Staat heute. Ohne Kultur gilt die Standardkultur der Landschaft. |

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
  und Bevölkerungsangabe oder keines von beiden (Ort ohne Daten); jeder Staat, der zu diesem Datum
  mindestens einen Ort besitzt (seit M7; die übrigen bestehen in diesem Szenario nicht), braucht alle
  Pflichtfelder (`hauptstadt`, `regierungsform`, `herrscher`, `konfession`, `schatz`, `kontribution`,
  `akzise`, `zoll`, `hofhaltung`, `heer`), und die Hauptstadt muss zu diesem Datum dem Staat gehören
  (also ein Ort mit Daten sein).
- seit M7: Spielerstaat eines Szenarios ohne Ort zu dessen Datum
- unbekannter Kreis in `orte.csv`
- seit M7: unbekannte Verwaltungseinheit (in `besitz.csv` oder als `ueber`), Einheit eines anderen
  Staates als der Besitzer bzw. die übergeordnete Einheit, Zyklus über `ueber`, `bund` und
  `zollverein` weder `ja`, `nein` noch leer

**Warnungen** (Ausreißer, vermutlich Tippfehler):

- Lage außerhalb der Karte (`karte/karte.chron`), Fläche unter 10 oder über 5.000 km²
- Bevölkerungsdichte über 150 Einwohner/km² (Land) bzw. 5.000 Einwohner/km² (Stadt)
- Änderung zwischen zwei Stichtagen über +5 % oder unter −10 % je Jahr
- Staat, der in keinem Szenario einen Ort besitzt; Kommentar für eine unbenutzte Datei
