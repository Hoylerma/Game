# Chronik-Skript und Datenformate (Meilenstein M1)

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
| Datum | `JJJJ.MM.TT` | `1648.10.24` |
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
├── history/
│   ├── staaten/*.chron     Staaten (Reichsstände) zum Szenariostart
│   └── orte/*.chron        Orte mit Bevölkerung zum Szenariostart
├── szenarien/*.chron       Lesezeichen
└── karte/
    ├── karte.chron         Projektion und Bildgröße für den Kartengenerator
    ├── provinzen.png       generiert: Ort-ID-Bild
    └── definition.csv      generiert: Zuordnung Kartenindex → Ort
```

Ladereihenfolge: `regeln`, `gelaende`, `konfessionen`, `berufe`, dann alle Dateien der Ordner `history/staaten`, `history/orte`, `szenarien`, jeweils **nach Dateiname sortiert**, Einträge in Dateireihenfolge. Daraus ergeben sich die Indizes in `World`.

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
    besitzer     = BRA                             # Pflicht, Staatstag
    gelaende     = flussaue                        # Pflicht
    lage         = { 13.40 52.52 }                 # Pflicht: Längengrad Ost, Breitengrad Nord (WGS84)
    flaeche      = 150                             # Pflicht, km²
    bevoelkerung = 12000                           # Pflicht, > 0
    stadt        = ja                              # Pflicht
    konfession   = { lutherisch = 90 reformiert = 10 }   # Pflicht, Prozentwerte, Summe 100
    verwuestung  = 0.4                             # Pflicht, 0..1
    quelle       = "Testdaten, geschätzt"          # Pflicht
}
```

Der Ort-Bezeichner ist der Wert hinter `ort` (Kleinbuchstaben, Ziffern, `_`).

### 3.7 `szenarien/*.chron`

```text
szenario westfaelischer_frieden {
    name    = "Der Westfälische Frieden"   # Pflicht
    datum   = 1648.10.24                   # Pflicht
    spieler = BRA                          # Pflicht, Staatstag (Vorauswahl)
}
```

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

**Jahreswechsel** (wenn das aktuelle Datum vor dem Weiterschalten der 1. Januar ist), Orte in aufsteigender Reihenfolge:

1. Seuche: `rng_ereignisse.chance(seuche_chance)`; bei Treffer `verlust = rng_ereignisse.uniform(min, max)`, jeder Block `größe −= größe × verlust`, Chronikeintrag: `Seuche in {Ortsname}: {gerundeter Prozentwert} % der Bevölkerung sterben.`
2. Verwüstung: `verwüstung −= verwüstung × verwuestung_abbau`.

## 6. Karte

### 6.1 `karte/karte.chron`

```text
karte {
    bild_breite = 1024
    bild_hoehe  = 1152
    laenge_min  = 9.5      # Grad Ost, linker Bildrand
    laenge_max  = 16.5
    breite_min  = 50.0     # Grad Nord, unterer Bildrand
    breite_max  = 54.8
    max_radius_km = 42     # weiter entfernte Punkte gehören zu keinem Ort
    kueste = { 9.5 54.6  10.9 54.0  … }   # Ostseeküste als Linienzug (Länge Breite …, höchstens 126 Punkte); nördlich davon Meer
}
```

### 6.2 `karte/provinzen.png` (generiert)

RGB, 8 Bit je Kanal, Größe `bild_breite × bild_hoehe`. Jeder Pixel kodiert den **Kartenindex + 1**:

```
wert = R + G × 256        # 0 = kein Ort (Meer, Ausland, außerhalb)
kartenindex = wert − 1
B = 0
```

### 6.3 `karte/definition.csv` (generiert)

Semikolon-getrennt, UTF-8, mit Kopfzeile:

```
kartenindex;ort;name
0;berlin_coelln;Berlin-Cölln
```

Der Kartenindex ist unabhängig vom Simulationsindex. Die Godot-Seite ordnet beide über die Ort-Kennung zu.
