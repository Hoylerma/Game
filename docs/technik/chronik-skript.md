# Chronik-Skript und Datenformate (Meilensteine M1 bis M7)

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
│   ├── kulturen.chron      Kulturen bzw. Landsmannschaften (seit M4)
│   ├── berufe.chron        Berufsgruppen
│   ├── gueter.chron        Güter (seit M5)
│   ├── beduerfnisse.chron  Bedürfnisse mit Warenkorb je Schicht (seit M5)
│   ├── betriebe.chron      Betriebe: Arbeitsstätten der Berufe (seit M5)
│   ├── maerkte.chron       Märkte aus Landschaften (seit M5)
│   ├── stroemungen.chron   politische Strömungen (seit M8, optional)
│   ├── interessengruppen.chron  Interessengruppen (seit M8, optional)
│   ├── parlamente.chron    Parlamente mit Wahlrecht und Fraktionen (seit M8, optional)
│   └── wahlen.chron        Regeln der Wahlsimulation und Regionen (seit M8, optional)
├── history/                generiert aus tabellen/ (tools/archivar), nicht von Hand bearbeiten
│   ├── staaten/*.chron     Staaten (Reichsstände) mit datierter Historie
│   ├── kreise/*.chron      Kreise: Gruppen von 4–12 Orten (seit M3)
│   ├── verwaltung/*.chron  Verwaltungseinheiten der Staaten (seit M7, optional)
│   ├── orte/*.chron        Orte mit datierter Historie (Besitz, Bevölkerung …)
│   ├── eisenbahnen/*.chron Eisenbahnstrecken mit Abschnitten (seit M7, optional)
│   └── wahlkreise/*.chron  Wahlkreise je Parlament (seit M8, optional)
├── tabellen/*.csv          Quelle der Wahrheit für history/ (siehe tabellen.md)
├── szenarien/*.chron       Lesezeichen
└── karte/
    ├── karte.chron         Ausschnitt, Projektion und Bildgröße
    ├── geometrie/          GeoJSON: Land, Seen, Flüsse (aus Natural Earth), Kernraum (von Hand)
    ├── provinzen.png       generiert: Ort-ID-Bild mit Flächenart
    ├── fluesse.png         generiert: Flussebene
    └── definition.csv      generiert: Kartenindex → Ort, Beschriftungspunkt, Fläche
```

Ladereihenfolge: `regeln`, `gelaende`, `konfessionen`, `kulturen` (seit M4), `berufe`, `gueter`, `beduerfnisse`,
`betriebe`, `maerkte` (seit M5), dann alle Dateien der Ordner `szenarien` (seit M7 vor `common/`, weil das
Startdatum bestimmt, welche Güter, Berufe und Betriebe gelten), `stroemungen`, `interessengruppen`, `parlamente`,
`wahlen` (seit M8), `history/staaten`, `history/kreise` (seit M3), `history/verwaltung` (seit M7), `history/orte`, `history/eisenbahnen` (seit M7), `history/wahlkreise` (seit M8), jeweils **nach Dateiname sortiert**, Einträge in Dateireihenfolge. Daraus ergeben sich die Indizes in `World`. Die Szenarien kommen vor der Historie, weil das Startdatum des gewählten Szenarios bestimmt, welche datierten Blöcke gelten (Abschnitt 3.8).

Die Dateien in `history/` erzeugt das Werkzeug `tools/archivar` aus den Tabellen in `data/tabellen/` (Format: [tabellen.md](tabellen.md)). Änderungen gehören in die Tabellen; die CI prüft mit `archivar.py --pruefen`, dass `history/` aktuell ist.

## 3. Dateischemata

Pflichtfelder sind fett. Unbekannte Felder erzeugen eine **Warnung** (nicht fatal). Fehlende Pflichtfelder, ungültige Werte, doppelte Kennungen und unbekannte Verweise sind **fatal**.

### 3.1 `common/regeln.chron`

```text
regeln {
    verwuestung_abbau     = 0.03    # jährlicher relativer Abbau der Verwüstung
    verwuestung_ertrag    = 0.5     # Produktionsverlust je Verwüstungsgrad
    seuche_chance         = 0.01    # jährlich je Ort
    seuche_verlust_min    = 0.04
    seuche_verlust_max    = 0.12
    steuersatz_max        = 0.5

    # Bevölkerung (seit M4); Bedeutung und Startwerte: bevoelkerung.md, Abschnitt 4.9
    anteil_kinder             = 0.34    # Altersaufbau beim Start, 0..1
    anteil_jung               = 0.26    # Summe der drei Anteile höchstens 1 (Rest: Alte)
    anteil_erwerb             = 0.33
    fruchtbarkeit             = 0.093   # 0..1
    anteil_fruchtbar_erwerb   = 0.4     # 0..1
    fruchtbarkeit_landreserve = 0.3     # 0..10
    fruchtbarkeit_wohlstand   = 0.15    # 0..1
    sterblichkeit_kinder      = 0.045   # 0..1, ebenso _jung, _erwerb, _alt, _armut
    sterblichkeit_jung        = 0.010
    sterblichkeit_erwerb      = 0.018
    sterblichkeit_alt         = 0.11
    sterblichkeit_armut       = 0.3
    tragfaehigkeit_stadt      = 1.75    # 0..100
    lebensstandard_bezug      = 9       # größer als 0
    lebensstandard_anpassung  = 0.25    # 0..1
    bildung_stadt             = 1.25    # 0..10
    bildung_anpassung         = 0.03    # 0..1
    wanderung_ziele           = 4       # Ganzzahl 0..16
    wanderung_radius_km       = 60      # 0..2000
    wanderung_rate            = 0.3     # 0..10
    wanderung_max             = 0.12    # 0..1
    wanderung_landreserve     = 20      # 0..100
    wanderung_fremd           = 0.5     # 0..1
    auswanderung_schwelle     = 30      # 0..100
    auswanderung_rate         = 0.05    # 0..10
    block_mindestgroesse      = 1       # Personen, 0..1000000
    wanderung_mindestmenge    = 0.05    # Personen, 0..1000000

    # Wirtschaft und Staatshaushalt (seit M5); Bedeutung und Startwerte: wirtschaft.md, Abschnitt 4.9
    preis_min                 = 0.25    # größer als 0, höchstens 1
    preis_max                 = 4       # 1..100
    preis_anpassung           = 0.3     # 0..1
    handel_nachbarn           = 6       # Ganzzahl 0..16
    handel_radius_km          = 180     # 0..5000
    handel_rate               = 5       # 0..100
    handel_fracht             = 0.12    # 0..10, je 100 km
    seefracht                 = 0.10    # 0..10
    bahnfracht                = 0.04    # 0..10, je 100 km mit Bahnverbindung (seit M7)
    handelsspanne             = 0.05    # 0..0.5
    betrieb_anpassung         = 0.03    # 0..1
    betrieb_min_anteil        = 0.02    # 0..0.5
    ernte_schwankung_land     = 0.08    # 0..0.5
    ernte_schwankung_markt    = 0.10    # 0..0.5
    sterblichkeit_hunger      = 1.5     # 0..10
    einschwingen_monate       = 60      # Ganzzahl 0..600
    verwaltung_je_einwohner   = 0.3     # 0..1000 Reichstaler
    sold                      = 30      # 0..100000 Reichstaler
    reserve_jahre             = 0.5     # 0..100
    bau_anteil                = 0.05    # 0..1
    ausbau_max                = 0.3     # 0..10
    ausbau_halb               = 200     # größer als 0, Reichstaler je km²
    zins_basis                = 0.05    # 0..1, höchstens zins_max
    zins_risiko               = 0.02    # 0..1
    zins_max                  = 0.2     # 0..1
    bankrott_quote            = 4       # größer als 0, höchstens 1000
    bankrott_schnitt          = 0.5     # 0..1
    bankrott_aufschlag        = 0.04    # 0..1
    bankrott_jahre            = 20      # Ganzzahl 0..200
}
```

Alle Felder sind Pflicht. Abbildung auf `Rules` in derselben Reihenfolge (`devastation_decay` … `tax_rate_max`,
seit M4 weiter `share_children` … `migration_min_amount`, seit M5 weiter `price_min` … `bankruptcy_years`).
`steuersatz_max` gilt seit M5 für alle drei Steuerarten der Staaten (Abschnitt 3.5). Die drei Felder `wachstum_basis`, `wachstum_wiederaufbau`
und `wachstum_steuerdruck` aus M1 gibt es seit dem Bevölkerungsmodell von M4 nicht mehr; stehen sie noch in einer
Datei, meldet der Lader sie als unbekannte Felder (Warnung).

### 3.2 `common/gelaende.chron`

```text
gelaende flussaue {
    name           = "Flussaue"          # Pflicht
    farbe          = { 128 160 112 }     # Pflicht
    ertrag         = 1.15                # Pflicht, Faktor
    tragfaehigkeit = 45                  # Pflicht seit M4, Einwohner je km² (0..100000)
}
```

### 3.3 `common/konfessionen.chron`

```text
konfession lutherisch {
    name    = "Lutherisch"         # Pflicht
    farbe   = { 70 110 170 }       # Pflicht
    bildung = 1.2                  # Pflicht seit M4, Faktor auf die Alphabetisierung (0..10)
}
```

### 3.4 `common/berufe.chron`

```text
beruf bauern {
    name            = "Bauern"     # Pflicht
    schicht         = unten        # Pflicht: unten | mittel | oben
    steuerpflichtig = ja           # Pflicht
    anteil_stadt    = 0.10         # Pflicht: Anteil an der Bevölkerung städtischer Orte
    anteil_land     = 0.60         # Pflicht: Anteil an der Bevölkerung ländlicher Orte
    agrar           = ja           # Pflicht seit M4: Ertrag hängt an der Tragfähigkeit
    bildung         = 0.1          # Pflicht seit M4: Alphabetisierung, Grundwert 0..1
    in_stadt        = landarme     # optional seit M4: Beruf nach dem Zuzug in eine Stadt
    agrarabgabe     = 0.08         # optional seit M5: Anteil am Agrarertrag des Ortes (0..1)
    seit            = 1800.01.01   # optional seit M7: erst ab diesem Datum (Abschnitt 3.18)
    anteil_aus      = landarme     # optional seit M7: Anteile werden diesem Beruf abgezogen
    selbststaendig  = 0.6          # optional seit M8: Anteil „selbstständiger“ Männer (Wahlrecht 1848), 0..1, Standard 1
}
```

`anteil_aus` (seit M7): Der Beruf nimmt seine `anteil_stadt` und `anteil_land` dem genannten Beruf ab, so
bleibt die Summe 1, auch wenn es ihn erst ab einem Datum gibt (Fabrikarbeiter aus den Landarmen). Reichen
die Anteile des anderen Berufs nicht oder ist er unbekannt, ist das fatal.

`in_stadt` verweist auf einen Beruf derselben Datei (auch einen späteren); ein unbekannter Beruf ist fatal.

`agrarabgabe` (seit M5, Standard 0): Grundrente bzw. Zehnt – der Beruf erhält diesen Anteil am Erlös der
Agrarbetriebe seines Ortes ([wirtschaft.md](wirtschaft.md), Abschnitt 4.2). Die Summe über alle Berufe ist
höchstens 1 (sonst fatal). Das Feld `ertrag` (fester Ertrag je Kopf bis M4) gibt es seit M5 nicht mehr:
Die Berufe erzeugen in Betrieben (Abschnitt 3.13); steht es noch in einer Datei, ist es eine Warnung.

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
    kontribution   = 0.10                          # Pflicht seit M5 (vorher steuersatz), 0..steuersatz_max
    akzise         = 0.05                          # Pflicht seit M5, 0..steuersatz_max
    zoll           = 0.03                          # Pflicht seit M5, 0..steuersatz_max
    hofhaltung     = 30000                         # Pflicht, Ausgaben pro Jahr
    heer           = 10                            # Pflicht seit M5: Soldaten je 1.000 Einwohner, 0..1000
    schulden       = 0                             # optional seit M5: Reichstaler, Standard 0
    zollverein     = nein                          # optional seit M7: ja/nein, Standard nein, datierbar
    verfassung     = konstitutionell               # optional seit M8: keine | staendisch | konstitutionell
    wahlrecht      = zensus                        # optional seit M8: Landtag (Abschnitt 3.21), Standard keines
    wahlsystem     = stichwahl                     # optional seit M8: stichwahl | relativ
    wahl_indirekt  = ja                            # optional seit M8: Wahl über Wahlmänner, Standard nein
    wahl_zensus    = 1                             # Pflicht bei wahlrecht = zensus: Steuern je Kopf und Jahr
    quelle         = "…"                           # Pflicht (oder "Testdaten, geschätzt")
}
```

Die drei Steuerarten (seit M5, [wirtschaft.md](wirtschaft.md), Abschnitte 4.2 und 4.3): **Kontribution** auf
die Erzeugung steuerpflichtiger Berufe in Orten ohne Stadtrecht, **Akzise** auf den Verbrauch in Städten,
**Zoll** auf eingeführte Waren. Im Kern: `State::tax_rates` (Index `TaxKind`), `army_per_mille`, `debt`.
Das alte Feld `steuersatz` ist seit M5 unbekannt (Warnung) und `kontribution` fehlt dann (fatal).

Der Tag ist der Wert hinter `staat` (Großbuchstaben und Ziffern, 2–4 Zeichen). Die Hauptstadt wird nach dem Laden der Orte geprüft.

**Staatenbestand (seit M7).** Ein Staat **besteht** zu einem Szenario, wenn er zum Startdatum
mindestens einen Ort besitzt. Staaten ohne Ort fallen nach dem Laden der Orte aus der Welt (die
übrigen rücken in ihrer Reihenfolge nach); ihre Hauptstadt wird nicht geprüft. So enthalten die Daten
die Staaten aller Epochen, und jedes Szenario lädt nur die seiner Zeit. Der Spieler des gewählten
Szenarios muss bestehen (sonst fatal: `Spielerstaat 'BRA' von szenario … besitzt zum Startdatum keinen
Ort`); die Spieler der übrigen Szenarien müssen nur bekannte Staaten sein.

**Zollverein (seit M7).** `zollverein = ja` markiert die Mitgliedschaft im Deutschen Zollverein
(`State::customs_union`). In M7 ist das Feld Inhalt ohne Wirkung auf die Simulation; der Zoll
zwischen Mitgliedern entfällt mit M9.

**Verfassung und Wahlordnung (seit M8).** `verfassung` und die Wahlfelder beschreiben den Landtag zum
Startdatum (`State::constitution`, `State::law`); Werte wie in Abschnitt 3.21. In M8 sind sie Inhalt zur
Anzeige: Die Wahlen tragen ihr eigenes Wahlrecht im Parlament, und ein Gesetzgebungsverfahren, das die
Felder ändert, kommt mit M11 ([politik.md](politik.md), Abschnitte 4 und 6).

### 3.6 `history/orte/*.chron`

```text
ort berlin_coelln {
    name         = "Berlin-Cölln"                  # Pflicht
    gelaende     = flussaue                        # Pflicht
    lage         = { 13.40 52.52 }                 # Pflicht: Längengrad Ost, Breitengrad Nord (WGS84)
    flaeche      = 150                             # Pflicht, km²
    stadt        = ja                              # Pflicht
    kreis        = kr_berlin                       # optional (seit M3), Kreiskennung, nicht datierbar
    vorkommen    = { salz }                        # optional (seit M5), nicht datierbar
    hafen        = ja                              # optional (seit M5), Standard nein, nicht datierbar
    verwaltung   = pr_kr_teltow                    # optional (seit M7), Verwaltungseinheit, datierbar
    bund         = ja                              # optional (seit M7), im Deutschen Bund, datierbar
    quelle       = "Testdaten, geschätzt"          # Pflicht

    # Simulationsfelder: alle vier oder keines (siehe unten)
    besitzer     = BRA                             # Staatstag
    bevoelkerung = 12000                           # > 0
    konfession   = { lutherisch = 90 reformiert = 10 }   # Prozentwerte, Summe 100
    kultur       = { maerker = 100 }               # seit M4: Prozentwerte, Summe 100
    verwuestung  = 0.4                             # 0..1
}
```

Der Ort-Bezeichner ist der Wert hinter `ort` (Kleinbuchstaben, Ziffern, `_`).

**Markt, Vorkommen und Hafen (seit M5).** Der Markt eines Ortes ergibt sich aus seiner Datei (der
Landschaft) über `common/maerkte.chron` (Abschnitt 3.14); eine Datei, deren Landschaft zu keinem Markt
gehört, ist ein Fehler. `vorkommen` nennt Kennungen, die in `common/betriebe.chron` als `vorkommen` eines
Betriebs vorkommen (sonst fatal); höchstens 32 verschiedene. `hafen = ja` gibt dem Markt des Ortes Zugang
zum Weltmarkt. Im Kern: `Location::market`, `deposits` (Bitmaske über `World::deposits`), `is_port`.

**Verwaltung und Bundesgebiet (seit M7).** `verwaltung` nennt die Einheit der untersten Ebene
(Abschnitt 3.16), der der Ort zum Startdatum angehört; `verwaltung = keine` hebt eine frühere
Zuordnung in einem datierten Block auf. `bund = ja` heißt: Der Ort liegt im Gebiet des Deutschen
Bundes. Beide Felder sind datierbar und unabhängig von den Simulationsfeldern (auch Orte ohne Daten
dürfen sie tragen). Im Kern: `Location::admin` (`kNone` ohne Einheit) und `Location::federal`.

**Orte ohne Daten (seit M3).** Die fünf Simulationsfelder `besitzer`, `bevoelkerung`,
`konfession`, `kultur` (seit M4) und `verwuestung` (direkt oder aus datierten Blöcken, Abschnitt 3.8)
sind zum Startdatum entweder **alle** vorhanden oder **keines**. Fehlen alle, ist der Ort ein *Ort ohne
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
    spielbar = { BRA SAX }                 # optional (seit M7): hervorgehobene Staaten der Staatswahl
    waehrung = "Reichstaler"               # optional (seit M7): Name der Rechnungseinheit
    tragfaehigkeit_faktor = 1              # optional (seit M7): Agrartechnik, Faktor 0,1–10 auf die
                                           # Tragfähigkeit aller Gelände (Standard 1)
}
```

`tragfaehigkeit_faktor` (seit M7) gibt den Stand der Landwirtschaft zum Startdatum wieder: Kartoffel,
Klee- und Fruchtwechselwirtschaft und die Agrarreformen ernähren 1848 weit mehr Menschen je km² als 1648
(bevoelkerung.md, Abschnitt 4.1). Der Lader multipliziert `tragfaehigkeit` jedes Geländes damit; der
Wert steht so im Fingerabdruck (Bereich `gelaende`).

Spieler und spielbare Staaten müssen bekannte Staaten sein; im gewählten Szenario müssen sie zum
Startdatum bestehen (Abschnitt 3.5, sonst fatal). `spielbar` und `waehrung` betreffen nur die
Oberfläche; im Kern `Scenario::playable` und `Scenario::currency`. `list_scenarios` liest nur die
Szenarien, ohne die übrigen Daten zu laden (für die Wahl vor dem Spielstart).

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

### 3.10 `common/kulturen.chron` (seit M4)

```text
kultur maerker {
    name  = "Märker"             # Pflicht
    farbe = { 64 104 168 }       # Pflicht (Kartenmodus Kultur)
}
```

Kennung: Kleinbuchstaben, Ziffern, `_`, eindeutig; mindestens eine Kultur ist Pflicht. Eine Kultur
ist die Landsmannschaft bzw. Volksgruppe eines Bevölkerungsblocks ([bevoelkerung.md](bevoelkerung.md),
Abschnitt 2). Im Kern: `World::cultures` (`Culture { id, name, color }`) und `PopBlock::culture`. Die
Standardkultur jeder Landschaft und Abweichungen je Ort pflegt der Archivar
([tabellen.md](tabellen.md)).

### 3.11 `common/gueter.chron` (seit M5)

```text
gut getreide {
    name       = "Getreide"        # Pflicht
    farbe      = { 214 180 90 }    # Pflicht
    basispreis = 1                 # Pflicht: Reichstaler je Einheit, größer als 0
    weltmarkt  = 0.9               # optional: Preis am Weltmarkt als Faktor auf den Basispreis (> 0)
}
```

Mindestens ein Gut ist Pflicht. Im Kern: `World::goods` (`Good { id, name, color, base_price, world_factor }`;
`world_factor` 0 = nicht am Weltmarkt).

### 3.12 `common/beduerfnisse.chron` (seit M5)

```text
beduerfnis grundnahrung {
    name   = "Grundnahrung"                              # Pflicht
    hunger = ja                                          # optional, Standard nein
    unten  = { getreide = 4.2 vieh = 0.2 salz = 0.05 }   # Pflicht: Menge je Kopf und Jahr
    mittel = { getreide = 4.5 vieh = 0.5 salz = 0.08 }   # Pflicht
    oben   = { getreide = 5 vieh = 1 salz = 0.1 }        # Pflicht (ein leerer Block { } ist erlaubt)
}
```

Die Dateireihenfolge ist der Rang beim Einkauf ([wirtschaft.md](wirtschaft.md), Abschnitt 4.3). In
Mengenblöcken stehen nur bekannte Güter, jedes höchstens einmal, Mengen 0..1000000. Im Kern: `World::needs`
(`Need { id, name, hunger, basket[schicht] }`).

### 3.13 `common/betriebe.chron` (seit M5)

```text
betrieb weberei {
    name       = "Weberei"            # Pflicht
    beruf      = handwerker           # Pflicht
    erzeugt    = { tuch = 3.2 }       # Pflicht, nicht leer (außer bei handel = ja): Menge je Kopf und Jahr
    verbraucht = { faser = 2 }        # optional: Vorleistungen je Kopf und Jahr
    agrar      = nein                 # Pflicht
    ort        = alle                 # optional: alle (Standard) | stadt | land
    vorkommen  = salz                 # optional: nur in Orten mit diesem Vorkommen
    handel     = nein                 # optional: Einkommen aus der Handelsspanne; dann ohne erzeugt/verbraucht
    gewicht    = 1                    # Pflicht, größer als 0, höchstens 1000
    gelaende   = { wald = 4 }         # optional: Faktor 0..100 je Geländetyp auf das Gewicht (Standard 1)
}
```

Die Kennungen unter `vorkommen` bilden die Liste `World::deposits` (in Ladereihenfolge). Im Kern:
`World::businesses` (`BusinessType`). Bedeutung: [wirtschaft.md](wirtschaft.md), Abschnitt 2.3.

### 3.14 `common/maerkte.chron` (seit M5)

```text
markt sachsen {
    name         = "Kursachsen (Leipzig, Dresden)"   # Pflicht
    landschaften = { sachsen }                       # Pflicht, nicht leer: Dateien in history/orte ohne Endung
}
```

Jede Landschaft gehört höchstens einem Markt an (sonst fatal); eine Landschaft ohne Datei in `history/orte`
ist eine Warnung. Im Kern: `World::markets` (`Market { id, name, landscapes }`). Bedeutung:
[wirtschaft.md](wirtschaft.md), Abschnitt 2.4.

### 3.15 `common/zielwerte.chron` (seit M5)

Zielwerte des Balancing-Berichts. Die Datei gehört nicht zum Spielinhalt: Der Lader liest sie nicht,
nur `uhrwerk-cli bilanz` (und `uhrwerk-cli pruefen` prüft sie, wenn sie vorhanden ist). Bereiche stehen
als `{ von bis }` (von ≤ bis, beide Grenzen eingeschlossen), Anteile zwischen 0 und 1. Seit M7 kann ein
Szenario eigene Zielwerte haben: Gibt es `common/zielwerte_<szenario>.chron` (etwa
`zielwerte_voelkerfruehling.chron`), nehmen `bilanz` und `pruefen` für dieses Szenario diese Datei statt
`zielwerte.chron`; Güter in `ausnahme` prüft der Bericht gegen den Inhalt dieses Szenarios.

```text
vermoegen {                 # (Schatz − Schulden) ÷ Jahreseinnahmen, je Staat
    bereich = { -3 3 }      # Pflicht: erlaubter Bereich …
    anteil  = 0.95          # Pflicht: … in mindestens diesem Anteil der Staatsjahre
    ende    = { 0 1.5 }     # Pflicht: Median am Ende des Laufs
}
bankrotte {                 # je Staat und Jahrhundert
    median = 0              # Pflicht: höchstens
    p95    = 2              # Pflicht: höchstens (95-%-Wert)
}
steuerquote {
    bereich = { 0.6 1.4 }   # Pflicht: Steuerquote am Ende ÷ am Start (Median je Staat)
}
preise {                    # Preis ÷ Basispreis je Gut
    bereich = { 0.4 2.5 }   # Pflicht
    anteil  = 0.95          # Pflicht: Mindestanteil der Markt-Monate im Bereich
    ausnahme kolonialwaren { bereich = { 0.4 5 } }   # optional, mehrfach: eigener Bereich je Gut
}
bevoelkerung {
    jahr   = 1700           # Pflicht: Jahrestag des Starts in diesem Jahr …
    faktor = { 1.20 1.45 }  # Pflicht: … Bevölkerung gegen den Start (Median)
}
lebensstandard {            # Median je Schicht über alle Jahre und Läufe
    unten  = { 25 55 }      # Pflicht
    mittel = { 45 75 }      # Pflicht
    oben   = { 70 95 }      # Pflicht
}
```

Alle sechs Blöcke sind Pflicht; unbekannte Blöcke und Felder, Bereiche mit von > bis und unbekannte
Güter in `ausnahme` sind Fehler. Im Kern: `BalanceTargets` (`uhrwerk/balance.hpp`). Bedeutung:
[wirtschaft.md](wirtschaft.md), Abschnitt 7.

### 3.16 `history/verwaltung/*.chron` (seit M7)

```text
einheit pr_rb_potsdam {
    name   = "Regierungsbezirk Potsdam"   # Pflicht, Anzeigename
    art    = regierungsbezirk             # Pflicht, Bezeichner (provinz, kreis, kronland, …)
    ueber  = pr_prov_brandenburg          # optional, übergeordnete Einheit; fehlt bei der obersten
    quelle = "…"                          # Pflicht
}
```

Die politische Gliederung der Staaten ([lesezeichen_1848.md](lesezeichen_1848.md), Abschnitt 6),
unabhängig von den räumlichen Kreisen aus Abschnitt 3.9. Kennung: Kleinbuchstaben, Ziffern, `_`,
eindeutig; `keine` ist reserviert. `ueber` muss auf eine bekannte Einheit verweisen, der Baum darf
keinen Zyklus enthalten (beides fatal). Einheiten sind **nicht datiert** (datierte Einträge sind
fatal); welche Einheit gilt, steht datiert im Ort (`verwaltung`, Abschnitt 3.6). Der Ordner darf
fehlen. Im Kern: `World::admin_units` (`AdminUnit { id, name, kind, parent, source }`).

Verwaltung, Bundesgebiet und Zollverein sind in M7 **Inhalt** wie die Kreise: Sie stehen im
Fingerabdruck der Spieldaten (Bereiche `kreise`, `orte`, `staaten`), nicht in der Prüfsumme und nicht
im Spielstand. Sobald Gebietswechsel im Spiel die Zuordnung ändern, wandern `Location::admin` und
`Location::federal` in den Zustand.

### 3.17 `history/eisenbahnen/*.chron` (seit M7)

```text
strecke leipzig_dresden {
    name      = "Leipzig-Dresdner Eisenbahn"                              # Pflicht
    quelle    = "…"                                                       # Pflicht
    abschnitt = { von = leipzig bis = wurzen eroeffnet = 1837.07.31 }     # mindestens einer
    abschnitt = { von = wurzen bis = riesa eroeffnet = 1838.09.21 }
}
```

Eine Strecke besteht aus Abschnitten von Ort zu Ort (mit oder ohne Daten) mit dem Tag der
Eröffnung; `abschnitt` darf sich wiederholen, alle drei Felder sind Pflicht. Kennung wie bei Orten,
eindeutig. Fatal: unbekannter Ort, gleicher Anfang und gleiches Ende, ungültiges Datum, Strecke ohne
Abschnitt. Der Ordner darf fehlen. Im Kern: `World::railways`
(`Railway { id, name, sections, source }`, `RailSection { from, to, opened }`).

Strecken sind **Inhalt**: Ob ein Abschnitt befahren wird, folgt aus dem Datum. Sie stehen im
Fingerabdruck (Bereich `maerkte`), nicht in der Prüfsumme und nicht im Spielstand. Wirkung: Zwei
Nachbarmärkte handeln mit `bahnfracht` statt `handel_fracht`, sobald ein Bahnhof im einen und einer
im anderen über eröffnete Abschnitte verbunden sind ([wirtschaft.md](wirtschaft.md), Abschnitt 4.1).
Zwei Strecken, die nur im selben Markt enden, verbinden nichts; ein Abschnitt zwischen Märkten, die
keine Nachbarn sind, schafft keinen Handel, kann aber ein Netz schließen. Der Bau neuer Strecken im Spiel
kommt mit einem späteren Meilenstein.

### 3.18 Inhalt ab einem Datum: `seit` (seit M7)

`gut`, `beruf` und `betrieb` dürfen `seit = JJJJ.MM.TT` tragen. Liegt das Datum nach dem Startdatum des
gewählten Szenarios, wird die Definition gelesen und geprüft, gehört aber nicht zur Welt: Sie fehlt in
den Indizes, im Fingerabdruck und in der Prüfsumme. So bringt die frühe Industrie von 1848 (Kohle, Eisen,
Zeche, Hütte, Fabrik, Fabrikarbeiter) nichts in das Szenario 1648; dessen Ergebnisse bleiben bitgleich.
Spätere Definitionen dürfen aufeinander verweisen; ein Verweis aus einer geltenden Definition auf eine
spätere ist fatal („Gut 'kohle' gibt es erst seit 1800.01.01 …“). Auch spätere Definitionen brauchen eine
freie Kennung, und ihre Verweise (`in_stadt`, `anteil_aus`, Beruf und Güter eines Betriebs) müssen auf
geltende oder spätere Definitionen zeigen. `anteil_aus` zieht die angegebenen Anteile ab, unabhängig von
der Reihenfolge in der Datei; ein Kreis (A aus B, B aus A) ist fatal. Das Vorkommen eines späteren
Betriebs bleibt bekannt, damit Orte es tragen dürfen. Ohne gültiges Szenario (Prüfung aller Daten) gilt alles.
Fortschritt im Spiel – Inhalt, der während einer Partie hinzukommt – gibt es damit noch nicht: Eine Partie
ab 1648 erreicht die Industrie nicht.

### 3.19 `common/stroemungen.chron` (seit M8)

```text
stroemung liberale {
    name     = "Konstitutionelle Liberale"   # Pflicht
    kurzname = "Liberale"                    # optional: knappe Anzeigen (Telegramme, Balken); sonst name
    farbe    = { 206 160 40 }                # Pflicht
    position = 10                            # Pflicht: Links-rechts-Achse −100 … +100
}
```

Politische Strömungen ([politik.md](politik.md), Abschnitt 3.1). Die Position ordnet die Stichwahl und
die Fraktionen. Der Kurzname ist reine Anzeige und gehört nicht zum Inhaltsfingerabdruck. Die Datei darf fehlen (dann gibt es keine Politik); ist sie da, braucht sie mindestens
eine Strömung. Im Kern: `World::currents` (`PoliticalCurrent`).

### 3.20 `common/interessengruppen.chron` (seit M8)

```text
interessengruppe bildungsbuergertum {
    name         = "Bildungsbürgertum"                          # Pflicht
    farbe        = { 170 120 150 }                              # Pflicht
    berufe       = { buerger = 0.6 geistlichkeit = 0.4 }        # Anteil der Mitglieder je Beruf (0..1)
    konfessionen = { katholisch = 0.5 }                         # optional: Faktor je Konfession (0..1)
    nach_bildung = ja                                           # optional: zusätzlich × Alphabetisierung
    neigung      = { liberale = 0.55 demokraten = 0.45 }        # Pflicht: je Strömung, Summe 1
    seit         = 1700.01.01                                   # optional (Abschnitt 3.18)
    quelle       = "…"                                          # optional
}
```

Ein Block gehört der Gruppe mit dem Anteil `berufe[Beruf] × konfessionen[Konfession]`, mit
`nach_bildung` zusätzlich × seiner Alphabetisierung. Fehlt `berufe` oder `konfessionen`, zählt dieser
Faktor 1; mindestens eines von beiden ist Pflicht. Nicht genannte Berufe und Konfessionen zählen 0.
Fatal: unbekannte Berufe, Konfessionen oder Strömungen, Anteile außerhalb 0..1, eine Neigung mit einer
Summe außerhalb 1 ± 0,001, ein Verweis auf einen späteren Beruf aus einer geltenden Gruppe. Im Kern:
`World::interest_groups` (`InterestGroup`; Anteile als Vektoren über alle Berufe, Konfessionen und
Strömungen). Bedeutung: [politik.md](politik.md), Abschnitte 3.3 und 7.3.

### 3.21 `common/parlamente.chron` (seit M8)

```text
parlament nationalversammlung {
    name          = "Deutsche Nationalversammlung"   # Pflicht
    ebene         = bund                             # Pflicht: bund oder ein Staatstag
    sitze         = 649                              # Pflicht: Sollzahl, 1..10000
    seit          = 1848.03.01                       # optional (Abschnitt 3.18)
    wahltag       = 1848.05.01                       # Pflicht
    wahlrecht     = allgemein_maenner                # Pflicht: keines | staendisch | zensus | dreiklassen | allgemein_maenner
    wahlsystem    = stichwahl                        # optional: stichwahl (Standard) | relativ
    wahl_indirekt = ja                               # optional: Wahl über Wahlmänner, Standard nein
    wahl_zensus   = 1                                # Pflicht bei wahlrecht = zensus
    fraktionen_ab = 1848.10.01                       # optional: Stichtag der Fraktionen, Standard der Wahltag
    boykott       = { tschechen }                    # optional: Kulturen, deren Wahlkreise nicht wählen …
    boykott_maerkte = { boehmen }                    # optional seit M8 a4: … nur in diesen Märkten (Markt des ersten Ortes)
    sitz          = frankfurt_am_main                # optional seit M8 a3: Tagungsort (Meldereihenfolge am Wahlabend)
    quelle        = "…"                              # optional
    fraktion casino { name = "Casino" farbe = { 70 96 150 } von = 20 bis = 55 }   # mindestens eine
}
```

Ein Parlament mit seiner Wahl ([politik.md](politik.md), Abschnitt 5). `seit` ist hier das früheste
Startdatum eines Szenarios, in dem es vorkommt; so gibt es 1648 kein Parlament, und dessen Ergebnisse
bleiben bitgleich. Die Fraktionen teilen die Links-rechts-Achse **lückenlos** von −100 bis +100 auf (`von`
kleiner als `bis`; die Reihenfolge in der Datei ist frei, der Kern ordnet nach `von`). Die Ebene wird
nach dem Staatenbestand aufgelöst (Abschnitt 3.5): Ein Staat ohne Ort zum Startdatum ist fatal.
Weitere Fehler: unbekannte Kulturen im Boykott, `fraktionen_ab` vor dem Wahltag, `wahl_zensus` ohne
`wahlrecht = zensus`, ein unbekannter Ort als `sitz`, ein unbekannter Markt in `boykott_maerkte` oder
`boykott_maerkte` ohne `boykott`. Gibt es zum Startdatum ein Parlament, braucht die Welt
die Wahlregeln (Abschnitt 3.23). Die Datei darf fehlen. Im Kern: `World::parliaments` (`Parliament`,
`Faction`, `ElectoralLaw`).

### 3.22 `history/wahlkreise/*.chron` (seit M8)

```text
wahlkreis nv_bad_10 {
    name      = "Mannheim"                   # Pflicht
    parlament = nationalversammlung          # Pflicht, aus common/parlamente.chron
    staat     = BAD                          # Pflicht: alle Orte gehören zum Startdatum diesem Staat
    sitze     = 1                            # optional, Standard 1 (Großstädte mehrere)
    orte      = { mannheim schwetzingen }    # Pflicht, mindestens ein Ort
    quelle    = "…"                          # optional
}
```

Erzeugt vom Archivar aus `wahlkreise.csv` (eine Datei je Parlament). Wahlkreise eines späteren Parlaments
(Abschnitt 3.21, `seit`) gehören nicht zur Welt. Fatal: unbekanntes Parlament, unbekannter Ort, ein Ort
eines anderen Staates oder in zwei Wahlkreisen desselben Parlaments, ein Staat ohne Ort zum Startdatum,
ein Wahlkreis außerhalb des Staates eines Landtags, eine Summe der Sitze, die nicht der Sollzahl des
Parlaments entspricht. Der Ordner darf fehlen. Im Kern: `World::electoral_districts`
(`ElectoralDistrict`).

### 3.23 `common/wahlen.chron` (seit M8)

```text
wahlregeln {                                   # Pflicht, genau einmal
    maenneranteil       = 0.5                  # Männer unter den Erwachsenen (0..1)
    junge_im_wahlalter  = 0.33                 # Anteil der 15–29-Jährigen ab 25 (0..1)
    junge_ab_24         = 0.4                  # dasselbe ab 24 (Dreiklassen)
    beteiligung_basis   = 0.45                 # Beteiligung = basis + bildung × Alphabetisierung + stadt
    beteiligung_bildung = 0.25                 #   − not × (1 − Lebensstandard/100), begrenzt auf min..max
    beteiligung_stadt   = 0.1
    beteiligung_not     = 0.15
    beteiligung_min     = 0.15
    beteiligung_max     = 0.9
    not_unter           = 40                   # Lebensstandard (0..100), unter dem die Not wirkt
    neigung_basis       = { konservative = 0.35 liberale = 0.2 … }   # je Strömung 0..1, fehlende 0
    neigung_bildung     = { konservative = -0.15 liberale = 0.1 }    # je Strömung −1..1
    neigung_not         = { demokraten = 0.15 arbeiter = 0.1 }
    neigung_stadt       = { liberale = 0.06 demokraten = 0.06 }
    wahlmaenner         = { liberale = 1.15 }  # optional: Faktor bei indirekter Wahl (0..10), Standard 1
    rauschen            = 0.05                 # ± relativ je Wahlkreis und Strömung (0..1)
    zug_zur_mitte       = 0.3                  # Abgeordnete: Zug zur Mitte des Wahlkreises (0..1)
    position_rauschen   = 15                   # ± Punkte auf der Achse (0..100)
    fraktionslos        = 0.25                 # Anteil der Fraktionslosen (0..1)
    wahlkampf {                                # Parteienmodus (seit Schritt b2)
        spenden         = 0.0003               # Anteil am Monatseinkommen der Anhänger (0..1)
        stimmungsfehler = 8                    # Stimmungsbericht: ± Prozentpunkte (0..50)
        versammlung  { kosten = 400   wirkung = 0.05  hoechstens = 0.25 }   # Mobilisierung
        flugblaetter { kosten = 250   wirkung = 0.3   hoechstens = 1 }      # Neigung × Alphabetisierung
        verein       { kosten = 1500  wirkung = 0.03  hoechstens = 0.06 }   # bleibt nach der Wahl
    }
}

region suedwesten {                            # beliebig viele
    staaten = { BAD }                          # Staatstags zum Startdatum und/oder
    maerkte = { kurpfalz }                     # Märkte (common/maerkte.chron); mindestens eins von beiden
    neigung = { demokraten = 0.12 }            # Pflicht: Verschiebung je Strömung (−1..1)
}
```

Regeln der Wahlsimulation ([politik.md](politik.md), Abschnitt 7). Alle Felder von `wahlregeln` außer
`wahlmaenner` sind Pflicht, auch der Block `wahlkampf` mit allen Feldern (Abschnitt 9.1):
- `kosten` in der Währung des Szenarios (0..1.000.000);
- `wirkung` und `hoechstens` je 0..1. Jede Ausführung hebt die Stufe um
  `wirkung × (1 − Stufe / hoechstens)`. Die Neigungen verweisen auf `common/stroemungen.chron`. Ein Ort gehört zu
jeder Region, deren Staaten seinen Besitzer oder deren Märkte seinen Markt enthalten; die Verschiebungen
addieren sich. Staaten ohne Ort zum Startdatum werden übersprungen.

Fatal sind:
- unbekannte Strömungen, Staaten oder Märkte;
- Werte außerhalb der Bereiche;
- `beteiligung_min` über `beteiligung_max`;
- `wirkung` größer als `hoechstens` in einer Art des Wahlkampfs;
- ein fehlender oder doppelter Block `wahlregeln`.

Die Datei darf fehlen, solange es zum Startdatum kein Parlament gibt. Im Kern: `World::election_rules`
(`ElectionRules`) und `World::political_regions` (`PoliticalRegion`).

### 3.24 `common/wahlziele_<szenario>.chron` (seit M8)

Wahlziele des Wahllabors ([politik.md](politik.md), Abschnitt 7.8). Wie die Zielwerte (Abschnitt 3.15)
gehört die Datei nicht zum Spielinhalt. Nur `uhrwerk-cli wahl` liest sie; `uhrwerk-cli pruefen` prüft sie,
wenn sie vorhanden ist.

```text
wahl nationalversammlung {          # je Parlament des Szenarios höchstens ein Block
    besetzt     = { 560 610 }       # optional: besetzte Sitze, Median über die Seeds
    beteiligung = { 0.45 0.7 }      # optional: Median der Beteiligung der gewählten Wahlkreise
    block rechte {                  # beliebig viele
        fraktionen = { cafe_milani }        # Fraktionen des Parlaments, auch fraktionslos …
        anteil     = { 0.04 0.10 }          # Pflicht: Anteil an den besetzten Sitzen (0..1), Median
    }
    block buergerliche {
        stroemungen = { konservative liberale }   # … oder Strömungen (genau eines von beiden)
        anteil      = { 0.6 1 }
    }
}
```

Bereiche stehen als `{ von bis }` mit von ≤ bis. Fehler sind:
- unbekannte Parlamente, Fraktionen, Strömungen und Felder;
- ein Block mit beiden oder keiner Liste;
- Anteile außerhalb von 0 bis 1;
- zwei Blöcke `wahl` für dasselbe Parlament.

Im Kern: `ElectionTargets` (`uhrwerk/election_lab.hpp`).

## 4. Startzustand

Für jeden Ort entstehen Bevölkerungsblöcke **in dieser Reihenfolge**: für jeden Beruf (Ladereihenfolge), darin für jede Konfession des Ortes (Reihenfolge im `konfession`-Block) und darin für jede Kultur (Reihenfolge im `kultur`-Block, seit M4):

```
größe  = bevoelkerung × (stadt ? anteil_stadt : anteil_land) × prozent / 100 × kulturprozent / 100
kinder = größe × anteil_kinder,  jung = größe × anteil_jung,  erwerb = größe × anteil_erwerb
alt    = größe − kinder − jung − erwerb
```

Blöcke mit Anteil 0 oder Prozent 0 entfallen. Die Blöcke eines Ortes liegen zusammenhängend in `World::pops`.
Danach setzt `initialize_population` (seit M4, [bevoelkerung.md](bevoelkerung.md), Abschnitt 2.6) Lebensstandard
und Alphabetisierung jedes Blocks auf ihre Zielwerte und löst Blöcke unter `block_mindestgroesse` im ähnlichsten
Block ihres Ortes auf (Abschnitt 4.7 dort).

Das Weltdatum wird auf `szenario.datum` gesetzt; der Zustand von `state.last_month_*` ist 0; die Chronik ist leer.

## 5. Formeln der Simulation

Alle Rechnungen in `Fixed`. Reihenfolge der Operationen wie angegeben (wichtig für Bit-Gleichheit).
Seit M4 stehen die Formeln der Bevölkerung (Lebensstandard, Geburten, Sterbefälle, Altern, Bildung,
Wanderung, Neuordnung) in [bevoelkerung.md](bevoelkerung.md), Abschnitt 4; das feste Wachstum aus M1
(`wachstum_*`) gibt es nicht mehr. Seit M5 stehen Erzeugung, Märkte, Einkommen, Verbrauch und die
Steuern eines Ortes (Kontribution, Akzise, Zoll) in [wirtschaft.md](wirtschaft.md), Abschnitt 4; der
feste Ertrag je Beruf (`ertrag`) und der eine Steuersatz (`steuersatz`) entfallen.

**Wirtschaft eines Staates** (an seinem Rechentag):

```
einnahmen = Σ zuletzt festgestellte Steuern seiner Orte (Orte aufsteigend)
ausgaben  = hofhaltung / 12
schatz   += einnahmen − ausgaben
```

**Orte ohne Daten** (Abschnitt 3.6) haben keine Bevölkerungsblöcke: Sie tragen nichts zu
Bevölkerung und Wirtschaft bei, sind keine Wanderungsziele, und beim Jahreswechsel werden sie
**übersprungen** – sie ziehen keine Zufallszahlen, und ihre Verwüstung (0) bleibt unverändert. Ein
Ort, der Blöcke hat, deren Größe auf 0 gefallen ist, wird dagegen normal behandelt.

**Jahreswechsel** (wenn das aktuelle Datum vor dem Weiterschalten der 1. Januar ist), Orte in aufsteigender Reihenfolge:

1. Seuche: `rng_ereignisse.chance(seuche_chance)`; bei Treffer `verlust = rng_ereignisse.uniform(min, max)`, jede Altersgruppe jedes Blocks `gruppe −= gruppe × verlust` (Bilanz `seuchentote`), Chronikeintrag: `Seuche in {Ortsname}: {gerundeter Prozentwert} % der Bevölkerung sterben.`
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
