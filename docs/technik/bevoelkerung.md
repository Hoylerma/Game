# Bevölkerung (Meilenstein M4)

> **Status: verbindlich** für Kern (`core/src/population.cpp`, `core/src/simulation.cpp`), Daten und
> Tests, wie [chronik-skript.md](chronik-skript.md). Das Dokument entstand als Entwurf für M4
> „Bevölkerung und Lasttest“ ([meilensteine.md](../meilensteine.md#m4--bevölkerung-und-lasttest-))
> und wurde vor der Umsetzung freigegeben; Abweichungen der Umsetzung vom Entwurf sind eingearbeitet
> (u. a. Formeln in Abschnitt 4 in der exakten Rechenreihenfolge, paralleles Buchen der Wanderung,
> kalibrierte Werte in Abschnitt 4.9). Abschnitt 12 hält die bestätigten Entscheidungen fest.

Das Design dahinter steht in [07 Bevölkerung und Gesellschaft](../07_bevoelkerung_und_gesellschaft.md)
und [02 Engine und Technik](../02_engine_und_technik.md), Abschnitte 5.2–5.5.

## 1. Ziel und Abgrenzung

M4 ersetzt das Wachstum aus M1 (ein fester Prozentsatz je Ort) durch eine Bevölkerung, die aus
Geburten, Sterbefällen, Altern und Wanderung entsteht, und macht den Kern schnell genug für
500.000 Bevölkerungsblöcke.

| Dimension aus Kapitel 07 | in M4 | später |
|---|---|---|
| Ort | ✔ wie bisher | – |
| Beruf und Schicht | ✔ die 6 Berufe der Epoche 1648–1806, Schicht am Beruf | 20 Berufsgruppen mit Betrieben (M5) |
| Konfession | ✔ wie bisher | Konversion, Konfessionspolitik (M8/M11) |
| Kultur (Landsmannschaft) | ✔ neu, wird vererbt und wandert mit | Angleichung, Nationalbewusstsein |
| Altersgruppe | ✔ als **Altersaufbau im Block** (Abschnitt 2.3) | Wehrpflicht, Renten |
| Bildung | ✔ Alphabetisierung als Anteil 0–1 | vier Bildungsstufen, Schulpflicht als Gesetz (M8) |
| Wohlstand | ✔ Lebensstandard 0–100 aus dem Einkommen | Bedürfnisse und Güter mit Märkten (M5) |
| Milieu, Sprache, Bewusstsein, Unzufriedenheit, Frauenanteil | – | M8 (Politik), M11 (Ereignisse) |

Nicht in M4: Bedürfnisse und Güter (dafür braucht es Märkte, M5), Hungerkrisen und andere
Ereignisse (M11), Peuplierung als Staatsaktion (M8), Teilen großer Blöcke (nicht nötig, solange
Blöcke nur über ihre Merkmale unterschieden werden).

## 2. Datenmodell

### 2.1 Bevölkerungsblock

```cpp
struct PopBlock {            // 64 Byte, eine Cache-Zeile
    Index location;          // Ort
    Index profession;        // Beruf (Schicht über den Beruf)
    Index confession;        // Konfession
    Index culture;           // Kultur (neu)
    Fixed children;          // 0–14 Jahre (Personen)
    Fixed young;             // 15–29
    Fixed working;           // 30–64
    Fixed old;               // 65+
    Fixed living_standard;   // Lebensstandard 0..100
    Fixed literacy;          // Alphabetisierung 0..1
    Fixed size() const;      // children + young + working + old
};
```

Das Feld `size` aus M1 entfällt; die Größe ist die Summe der Altersgruppen.

**Speicheranordnung: Blöcke als Strukturen (AoS), nicht Struktur aus Feldern (SoA).** Kapitel 02
nennt SoA für Massendaten. Für M4 ist AoS besser: Die Demografie liest und schreibt in jedem
Schritt **alle** Felder eines Blocks, die Blöcke eines Ortes liegen zusammenhängend, und ein
Block füllt genau eine Cache-Zeile. SoA lohnt sich erst für Durchläufe, die wenige Felder
vieler Blöcke lesen (etwa Summen für Karten); dafür gibt es die Ortsstatistik (Abschnitt 2.4).
Die Festkomma-Multiplikation lässt sich ohnehin nicht sinnvoll vektorisieren.

Speicherbedarf bei 500.000 Blöcken: 32 MB für die Blöcke, dazu etwa 3 MB für Orte und Statistik.

### 2.2 Schlüssel und Reihenfolge

Ein Block ist durch **(Ort, Beruf, Konfession, Kultur)** bestimmt; in einem Ort gibt es jeden
Schlüssel höchstens einmal. Die Blöcke eines Ortes liegen weiter zusammenhängend
(`first_pop`, `pop_count`). Innerhalb des Ortes gilt die Reihenfolge der Entstehung: erst die
Startblöcke (Abschnitt 2.6), dann neue Blöcke in der Reihenfolge, in der die Neuordnung sie
anlegt (Abschnitt 4.7). Die Reihenfolge ist damit Teil des Zustands und deterministisch.

### 2.3 Altersaufbau im Block statt Altersgruppe als Blockmerkmal

Kapitel 02 nennt die Altersgruppe als Blockmerkmal. Für M4 schlage ich vor, die vier
Altersgruppen **innerhalb** jedes Blocks zu führen:

- Geburten und Altern bleiben im Block. Als Merkmal müsste jede Geburt und jeder Übergang in
  eine andere Altersgruppe Menschen zwischen Blöcken verschieben – viermal so viele Blöcke und
  viel mehr Zusammenführungen.
- Die Altersstruktur eines Ortes ergibt sich durch Summieren; Wehrpflichtige, Arbeitskräfte und
  Rentenlast (spätere Meilensteine) lassen sich genauso ablesen.
- Nachteil: Menschen eines Blocks unterscheiden sich nur in der Zahl, nicht in Bildung oder
  Lebensstandard nach Alter. Für M4 ohne Folgen.

### 2.4 Zustand je Ort (neu)

| Daten | Inhalt | Zweck |
|---|---|---|
| **Momentaufnahme** (`LocationSnapshot`) | Bevölkerung, Landreserve, Ziel-Lebensstandard je Beruf | wird von **anderen** Orten gelesen (Wanderung); Abschnitt 5 |
| **Bilanz** laufendes Jahr und Vorjahr | Geburten, Sterbefälle, Seuchentote, Zuzug, Wegzug, Auswanderung | Anzeige, Statistik, exakte Buchführung (Test) |
| **Steuern des Monats** | Produktion und Steuern beim letzten Rechentag | Staatseinnahmen (Abschnitt 4.6) |
| **Zugänge** (`arrivals[ort]`) | Zugezogene, für die es im Ort noch keinen Block gibt (je Ort eine Liste, Felder wie ein Block) | werden bei der Neuordnung zu Blöcken |

Diese Daten gehören zum Zustand (Prüfsumme, später Spielstand) und liegen im `World`
(`snapshots`, `snapshot_targets`, `balance_current`, `balance_previous`, `economy`, `arrivals`). Der
Lader lässt sie leer; die Simulation füllt sie beim Start aus dem Anfangszustand.
Abgeleitete Hilfsdaten, die sich aus dem unveränderlichen Inhalt ergeben – Orte je Rechentag,
Orte je Staat, Wanderungsziele –, hält die `Simulation`.

### 2.5 Neue und geänderte Daten

| Datei | Änderung |
|---|---|
| `common/kulturen.chron` (neu) | `kultur <id> { name farbe }`; etwa 30 Landsmannschaften und Minderheiten aus Kapitel 07, Abschnitt 4 |
| `common/regeln.chron` | neue Pflichtfelder für Demografie, Lebensstandard, Bildung und Wanderung (Abschnitt 4.9); `wachstum_basis`, `wachstum_wiederaufbau` und `wachstum_steuerdruck` entfallen |
| `common/gelaende.chron` | `tragfaehigkeit` (Einwohner je km² unter der Landwirtschaft der Zeit) ersetzt `wachstum` |
| `common/berufe.chron` | `agrar = ja/nein` (Ertrag hängt am Boden), `bildung` (Alphabetisierung, Ausgangswert), optional `in_stadt = <beruf>` (Beruf nach dem Zuzug in eine Stadt, z. B. Bauern → Landarme) |
| `common/konfessionen.chron` | `bildung` (Faktor auf die Alphabetisierung, z. B. 1,3 für lutherisch) |
| `history/orte/*.chron` | fünftes Simulationsfeld `kultur = { maerker = 100 }` (Prozente, Summe 100). Die Regel „alle oder keines“ gilt dann für fünf Felder |
| `tabellen/dateien.csv` | Spalte `kultur`: Standardkultur einer Landschaft (Datei in `history/orte/`) |
| `tabellen/bevoelkerung.csv` | Spalte `kultur` (optional): Abweichung je Ort, Format wie `konfession` |

Der Archivar schreibt `kultur` in jeden datierten Block eines Ortes mit Daten: aus der Spalte in
`bevoelkerung.csv`, sonst die Standardkultur der Landschaft. Die Kultur wird **unabhängig von der
Konfession** verteilt, so wie M1 die Berufe verteilt. Bekannte Vereinfachung: Jüdische Gemeinden
erhalten die Kultur des Ortes; eine eigene Zuordnung kommt mit der Sprachdimension.

### 2.6 Startzustand

Für jeden Ort entstehen Blöcke für jeden Beruf, darin jede Konfession, darin jede Kultur
(jeweils in Lade- bzw. Blockreihenfolge):

```
größe = bevoelkerung × anteil(beruf) × prozent(konfession) / 100 × prozent(kultur) / 100
kinder = größe × anteil_kinder, jung = größe × anteil_jung, erwerb = größe × anteil_erwerb,
alt = größe − kinder − jung − erwerb                        (Rest, damit die Summe stimmt)
lebensstandard = Ziel-Lebensstandard des Berufs im Ort (Abschnitt 4.2)
alphabetisierung = Zielwert (Abschnitt 4.4)
```

Blöcke mit Größe unter `block_mindestgroesse` werden beim Start wie bei der Neuordnung aufgelöst
(Abschnitt 4.7); die Menschen gehen in den ähnlichsten Block über.

## 3. Ablauf eines Tages

| Schritt | Was | Wann | parallel |
|---|---|---|---|
| 1 | Befehle | jeden Tag | – |
| 2 | **Jahresabschluss**: Bilanz des Jahres wird Bilanz des Vorjahres | 1. Januar | – |
| 3 | **Neuordnung**: Zugänge werden Blöcke, zu kleine Blöcke aufgelöst (nur wenn nötig) | Monatserster | – |
| 4 | **Bevölkerung** der Orte mit heutigem Rechentag: Lebensstandard, Geburten, Sterbefälle, Altern, Bildung, Wanderung, Steuern, Momentaufnahme | Ort *i* am Tag `(i % 28) + 1` | ✔ |
| 5 | **Zusammenführen**: Momentaufnahmen veröffentlichen (nacheinander), Wanderung je Zielort buchen | nach Schritt 4 | ✔ (Buchen) |
| 6 | Wirtschaft der Staaten mit heutigem Rechentag | Staat *j* am Tag `(j % 28) + 1` | – |
| 7 | Jahreswechsel: Seuchen, Abbau der Verwüstung | 1. Januar | – |
| 8 | Datum weiterschalten | jeden Tag | – |

Der Rechentag der Orte bleibt wie in M1. Statt täglich alle 4.542 Orte auf ihren Rechentag zu
prüfen, hält die Simulation je Rechentag die Liste der Orte **mit Daten** (aufsteigend).

## 4. Formeln

Alle Rechnungen in `Fixed`, **genau in der angegebenen Reihenfolge** (Festkomma rundet bei jeder
Operation: `a × b / c` ist nicht `a × (b / c)`). Regelwerte sind Jahreswerte aus `regeln.chron`; die
Umrechnung auf einen Monat steht in den Formeln. Konstanten: `1/180` und `1/420` als
`Fixed::from_ratio`, `bildung_anpassung / 12` einmal beim Start der Simulation berechnet.

### 4.1 Vorberechnung je Ort (am Rechentag, vor den Blöcken)

```
n          = Σ größe aller Blöcke des Ortes
trag       = fläche × gelände.tragfaehigkeit × (stadt ? tragfaehigkeit_stadt : 1)
reserve    = max(0, 1 − n / trag)                      Landreserve 0..1
zustand    = 1 − verwüstung × verwuestung_ertrag
agrar      = min(1, trag / n)                           abnehmender Ertrag der Agrarbetriebe bei Übervölkerung
je Beruf b (seit M5 aus der Wirtschaft des Ortes, wirtschaft.md Abschnitte 4.2–4.4):
  real_b   = Netto-Einkommen je Kopf und Jahr ÷ Preisniveau des Warenkorbs
  ziel_b   = 100 × real_b / (real_b + lebensstandard_bezug)     Ziel-Lebensstandard 0..100
  hunger_b = 1 + sterblichkeit_hunger × (1 − Deckung der Grundnahrung)²
```

Bis M4 kam das Einkommen aus einem festen Ertrag je Beruf (`ertrag × gelände.ertrag × zustand ×
agrar`, abzüglich `steuersatz`). Seit M5 entsteht es auf den Märkten; die Formel des Lebensstandards
bleibt, nur das Einkommen wird durch das Preisniveau geteilt.

Die **Landreserve** ersetzt das „Wiederaufbauwachstum“ aus M1: Ein verwüsteter, entvölkerter Ort
hat viel freies Land, also mehr und frühere Heiraten und Zuzug von Bauern. Die **Tragfähigkeit**
bremst das Wachstum, wenn der Boden ausgeschöpft ist: Der Ertrag je Kopf der Agrarberufe sinkt,
damit Lebensstandard und Geburten, und die Abwanderung steigt. Spätere Meilensteine heben die
Tragfähigkeit durch Agrartechnik (Kartoffel, Agrarreformen).

### 4.2 Lebensstandard (je Block)

```
L ← L + (ziel_b − L) × lebensstandard_anpassung
```

Mit dem Bezugseinkommen 9 Taler (seit M5, vorher 12) ergibt das nach dem Einschwingen 1648 im Mittel:
Landarme 29, Handwerker 38, Bauern 44, Geistlichkeit 58, Bürger 61, Adel 83; bis 1700 steigen die
Werte mit dem Abbau der Verwüstung (Landarme 37, Bauern und Handwerker 50, Adel 80).

### 4.3 Geburten, Sterbefälle, Altern (je Block)

Alle Größen werden aus dem Stand **vor** dem Schritt berechnet (k, j, e, a = Kinder, Jung,
Erwerbsalter, Alt), mit dem Lebensstandard L nach 4.2:

```
d      = (L − 50) / 50                                          −1 … +1
s      = clamp(1 − sterblichkeit_armut × d, 1/2, 2)               Sterblichkeit nach Lebensstandard
s_m    = s × hunger_b / 12                                       (hunger_b seit M5, Abschnitt 4.1)
w      = max(0, 1 + fruchtbarkeit_wohlstand × d)
f_ort  = fruchtbarkeit × (1 + fruchtbarkeit_landreserve × reserve)          einmal je Ort
f_m    = f_ort × w / 12
geb    = (j + e × anteil_fruchtbar_erwerb) × f_m
tk     = k × sterblichkeit_kinder × s_m     (ebenso tj, te, ta mit _jung, _erwerb, _alt)
ak     = k × 1/180      aj = j × 1/180      ae = e × 1/420                  Altern (15, 15, 35 Jahre)
k ← k + geb − tk − ak
j ← j + ak − tj − aj
e ← e + aj − te − ae
a ← a + ae − ta
```

Keine Altersgruppe wird negativ: Alle Abgänge einer Altersgruppe zusammen (auch mit Wanderung,
Abschnitt 4.5) liegen selbst bei den Höchstwerten der Regeln unter 100 % je Monat, und
Multiplikation und Division runden ab bzw. schneiden ab. Bilanz: `geburten += geb`,
`sterbefaelle += tk + tj + te + ta`.

### 4.4 Bildung (je Block)

```
ziel  = clamp(beruf.bildung × konfession.bildung × (stadt ? bildung_stadt : 1) × (50 + L) / 100, 0, 1)
A    ← A + (ziel − A) × (bildung_anpassung / 12)
```

Ohne Schulgesetze (M8) ändert sich die Alphabetisierung in M4 über den Lebensstandard (wer mehr
hat, schickt Kinder in die Schule), langsam über die Generationen und durch Wanderung.

### 4.5 Wanderung (je Block, nach 4.3)

**Wanderungsziele** werden beim Start der Simulation einmal je Ort berechnet: die
`wanderung_ziele` (4) nächstgelegenen Orte mit Daten im Umkreis von `wanderung_radius_km`, für
ländliche Orte zusätzlich die nächste Stadt (auch außerhalb des Umkreises). Abstand in Festkomma
als `(Δlänge × cos 51°)² + Δbreite²` ohne Wurzel; bei Gleichstand der kleinere Index.

Nur die Altersgruppe **Jung** (15–29) wandert – Gesinde, Gesellen, junge Paare.

```
j               = Jung nach 4.3
beruf am Ziel   b' = (ziel.stadt und b.in_stadt gesetzt) ? b.in_stadt : b
anziehung(o, b) = ziel_b(o) + (b.agrar ? wanderung_landreserve × reserve(o) : 0)
für jedes Ziel z (nach Abstand aufsteigend):
  diff_z  = anziehung(z, b') − anziehung(hier, b)        z aus der Momentaufnahme, hier aktuell
  strom_z = diff_z > 0 ? j × wanderung_rate × diff_z / 1200 : 0
  strom_z = strom_z × wanderung_fremd,  wenn z einem anderen Staat gehört
summe = Σ strom_z
grenze = j × wanderung_max / 12
falls summe > grenze: strom_z = strom_z × grenze / summe
Ströme unter wanderung_mindestmenge entfallen (die Menschen bleiben)
auswanderung = L < auswanderung_schwelle ? j × auswanderung_rate × (auswanderung_schwelle − L) / 1200 : 0
j ← j − Σ strom_z − auswanderung
```

`/ 1200` rechnet eine Jahresrate je 100 Punkte in eine Monatsrate je Punkt um.

Jeder Strom kommt als Eintrag in den **Ausgang** des Ortes: Zielort, Beruf am Ziel, Konfession,
Kultur, Menge, Lebensstandard und Alphabetisierung des Herkunftsblocks. Auswanderung verlässt die
Simulation (Bilanz `auswanderung`). Damit sind beide Richtungen abgedeckt: vom übervölkerten Land
in die Stadt, aber auch aus Städten und dichten Orten in entvölkerte Landstriche mit freien Höfen.
`wanderung_fremd` bildet ab, dass Wegzug über Landesgrenzen schwerer war (Leibeigenschaft,
Abzugsgeld).

### 4.6 Produktion und Steuern

Seit M5 in [wirtschaft.md](wirtschaft.md), Abschnitte 4.2–4.6: Erzeugung in Betrieben, Märkte,
Steuern je Art (Kontribution, Akzise, Zoll), festgestellt am Rechentag des Ortes; der Staat summiert an
seinem Rechentag die zuletzt festgestellten Steuern seiner Orte. `location_economy()` gibt die Werte des
letzten Rechentags zurück.

### 4.7 Zusammenführen und Neuordnung

**Zusammenführen** (Schritt 5):

1. Nacheinander für jeden Ort des Tages aufsteigend: Momentaufnahme veröffentlichen
   (Abschnitt 5.1) und die Einträge seines Ausgangs dem jeweiligen Zielort zuordnen. Jeder
   Zielort hat damit eine Eingangsliste in fester Reihenfolge: Herkunftsorte aufsteigend, darin
   die Einträge in ihrer Reihenfolge.
2. Je Zielort die Eingangsliste in Reihenfolge buchen: Gibt es im Zielort einen Block mit gleichem
   Schlüssel, kommen die Menschen in dessen Altersgruppe Jung; Lebensstandard und
   Alphabetisierung werden nach Personen gewichtet gemittelt. Sonst: Menge unter
   `block_mindestgroesse` → in den ähnlichsten Block des Zielortes; sonst ein Zugang (gleicher
   Schlüssel im selben Ort wird zusammengefasst). Bilanz `zuzug` beim Ziel, `wegzug` beim
   Herkunftsort. Verschiedene Zielorte sind voneinander unabhängig und werden parallel gebucht.

**Neuordnung** (Schritt 3, am Monatsersten, nur wenn es Zugänge oder zu kleine Blöcke gibt), Orte
aufsteigend:

1. Blöcke unter `block_mindestgroesse` (auch leere) gehen im ähnlichsten verbleibenden Block auf;
   der größte Block eines Ortes bleibt immer.
2. Zugänge werden in ihrer Reihenfolge als neue Blöcke angehängt.
3. Die Blockliste wird neu aufgebaut, `first_pop` und `pop_count` werden neu gesetzt.

**Ähnlichster Block:** gleicher Beruf und gleiche Konfession → gleicher Beruf und gleiche Kultur
→ gleicher Beruf → gleiche Konfession → größter Block; bei Gleichstand der kleinere Index.

Zugänge zählen sofort zur Bevölkerung des Ortes (`location_population`), nehmen aber erst nach
der Neuordnung, also höchstens einen Monat später, an Geburten und Sterbefällen teil. Aufwand der
Neuordnung: höchstens 12 Mal im Jahr ein Durchlauf über alle Blöcke.

### 4.8 Jahreswechsel

Wie in M1 (Seuche mit dem Strom `Ereignisse`, Abbau der Verwüstung, Orte ohne Daten
übersprungen). Die Seuche trifft alle Altersgruppen gleich (`× (1 − verlust)`), Bilanz
`seuchentote`.

### 4.9 Regelwerte (Startwerte) und Kalibrierung

| Feld | Startwert | Bedeutung |
|---|---|---|
| `anteil_kinder` / `anteil_jung` / `anteil_erwerb` | 0,34 / 0,26 / 0,33 | Altersaufbau beim Start (Rest: Alte) |
| `fruchtbarkeit` | 0,093 | Geburten je Person im fruchtbaren Alter und Jahr (kalibriert; M4: 0,088, Entwurf: 0,095) |
| `anteil_fruchtbar_erwerb` | 0,4 | Anteil der 30–64-Jährigen im fruchtbaren Alter |
| `fruchtbarkeit_landreserve` | 0,3 | Zuschlag bei voller Landreserve |
| `fruchtbarkeit_wohlstand` | 0,15 | Zu- oder Abschlag je 50 Punkte Lebensstandard |
| `sterblichkeit_kinder` / `_jung` / `_erwerb` / `_alt` | 0,045 / 0,010 / 0,018 / 0,11 | jährliche Sterblichkeit je Altersgruppe |
| `sterblichkeit_armut` | 0,3 | Zu- oder Abschlag je 50 Punkte Lebensstandard |
| `tragfaehigkeit_stadt` | 1,75 | Faktor der Tragfähigkeit für Städte mit Amt (kalibriert, Entwurf: 3) |
| `lebensstandard_bezug` | 9 | Realeinkommen (Taler pro Kopf und Jahr) für Lebensstandard 50 (M4: 12, Netto-Einkommen) |
| `lebensstandard_anpassung` | 0,25 | Anteil der Lücke zum Ziel je Monat |
| `bildung_stadt` / `bildung_anpassung` | 1,25 / 0,03 | Stadtfaktor; Anteil der Lücke je Jahr |
| `wanderung_ziele` / `wanderung_radius_km` | 4 / 60 | Ziele je Ort |
| `wanderung_rate` / `wanderung_max` | 0,3 / 0,12 | Anteil der Jungen je Jahr bei 100 Punkten Unterschied; Obergrenze je Jahr |
| `wanderung_landreserve` / `wanderung_fremd` | 20 / 0,5 | Punkte für freies Land; Faktor über Landesgrenzen |
| `auswanderung_schwelle` / `auswanderung_rate` | 30 / 0,05 | Lebensstandard, unter dem ausgewandert wird; Anteil je Jahr und 100 Punkte |
| `block_mindestgroesse` / `wanderung_mindestmenge` | 1 / 0,05 | Personen |

Die **Tragfähigkeit** je Gelände liegt beim 1,8-Fachen der Dichte von 1648 auf dem Land
(gemessen an den 60 Orten mit Daten: 10–33 Einwohner je km²), z. B. Börde 60, Hügelland 50,
Flussaue 45, Flachland 30, Heide, Bruch und Seenplatte 20.

**Kalibrierung.** Die Werte sind mit dem Kern und den echten Daten (60 Orte der Testregion,
Seed 1648) abgestimmt; ein Test prüft den Bereich (Abschnitt 8). Zielwerte aus Kapitel 07,
Abschnitt 7.3: 1648 → 1700 rund ×1,32 (11 → 14,5 Mio.), 1700 → 1816 rund ×1,72 (→ 25 Mio.).

Stand M5 (Wirtschaft aus [wirtschaft.md](wirtschaft.md), Abschnitt 4.9; die Werte aus M4 stehen in
der Zeile darunter):

| Jahr | Bevölkerung | Faktor seit 1648 | Geburten | Sterbefälle | Seuchen | Wanderung | Alphabetisierung |
|---|---|---|---|---|---|---|---|
| 1648 | 384.000 | ×1,00 | – | – | – | – | 21 % (M4: 31 %) |
| 1700 | 511.000 | ×1,33 (M4: ×1,29) | 39,1 ‰ | 32,5 ‰ | Einzeljahr | 2,4 ‰ | 25 % (M4: 38 %) |
| 1750 | 664.000 | ×1,73 (M4: ×1,68) | 37,7 ‰ | 32,5 ‰ | Einzeljahr | 2,5 ‰ | 28 % (M4: 44 %) |
| 1816 | 858.000 | ×2,23 (M4: ×2,33) | 36,8 ‰ | 32,5 ‰ | Einzeljahr | 2,1 ‰ | 32 % (M4: 53 %) |

Die Alphabetisierung liegt seit M5 niedriger, weil die Orte mit Stadtrecht (Stadt mit Amt) jetzt mehr
Bauern und Landarme und weniger Bürger und Handwerker haben (berufe.chron, wirtschaft.md
Abschnitt 4.9). Der Altersaufbau bleibt stabil, der mittlere Lebensstandard steigt mit dem Abbau der
Kriegsverwüstung von 39 auf 48. Die Abstimmung (Parametervergleich
Fruchtbarkeit 0,082–0,088 × Stadtfaktor 1,5–2 × Armut 0,3–0,6 × Wohlstand 0,15–0,3) zeigte: Die
Wachstumsrate hängt vor allem an Fruchtbarkeit und Stadtfaktor; ein höheres Gewicht des
Lebensstandards beschleunigt das Wachstum, weil der Lebensstandard mit dem Wiederaufbau steigt.
Hungerkrisen und Kriege (M11) fehlen noch; sie bremsen das Wachstum später zusätzlich.

## 5. Mehrkernberechnung

### 5.1 Grundsatz

Das Ergebnis darf nicht von der Zahl der Threads abhängen. Das gilt, wenn jede parallele Aufgabe

1. nur **eigene** Daten schreibt: die Blöcke ihres Ortes, ihren Ausgang, ihre Bilanz, ihre
   Steuern und ihre **neue** Momentaufnahme (zweiter Puffer), und
2. alles andere nur **liest**, und zwar Daten, die während der parallelen Phase niemand
   schreibt: Regeln, Staaten, unveränderlichen Inhalt und die **veröffentlichten**
   Momentaufnahmen anderer Orte.

Dann ist jede Aufgabe eine reine Funktion ihrer Eingaben, und es ist gleich, welcher Thread sie
wann rechnet. Alles, was Orte verbindet – Momentaufnahmen veröffentlichen, Ströme den Zielorten
zuordnen, Staatseinnahmen summieren, Neuordnung, Seuchen –, läuft danach **nacheinander in fester
Reihenfolge** (Orte aufsteigend, Einträge in Reihenfolge).

Das **Buchen** der Wanderung folgt derselben Regel aus Sicht der Zielorte: Jede Aufgabe bucht die
Eingangsliste genau eines Zielortes in fester Reihenfolge und schreibt nur dessen Blöcke,
Zugänge und Bilanz. Gemessen ist das nötig: Mit 370.000 Blöcken entstehen rund 2,9 Mio. Ströme
im Jahr; nacheinander gebucht kosteten sie 44 % der Rechenzeit.

Die veröffentlichte Momentaufnahme eines Ortes ist die von seinem letzten Rechentag. Welcher Stand
das ist, hängt nur vom Kalender ab, nicht von den Threads.

Nebenbei: Die Addition von `Fixed` ist exakte Ganzzahladdition und damit unabhängig von der
Reihenfolge. Nur Multiplikation und Division runden. Summen wären also selbst parallel gebildet
bitgleich; der Kern bildet sie trotzdem in fester Reihenfolge, damit die Regel einfach bleibt.

### 5.2 Aufteilung

- Die Orte des Tages (etwa 1/28 aller Orte mit Daten) werden in **Pakete fester Größe** (8 Orte)
  geteilt. Die Threads holen sich Pakete über einen atomaren Zähler. Welcher Thread welches Paket
  rechnet, ist zufällig – das ist erlaubt, weil nach 5.1 das Ergebnis davon nicht abhängt.
- Der Haupt-Thread rechnet mit. Am Ende der Phase warten alle (Barriere), dann folgt Schritt 5.
- Mit einem Thread (oder unter 2 Paketen) läuft alles ohne Thread-Wechsel im Haupt-Thread.
- Das Buchen der Wanderung teilt die Zielorte in Pakete zu 16, aber erst ab 2.048 Strömen am Tag;
  darunter kostet das Wecken der Threads mehr, als es bringt (gemessen mit den 60 Orten der
  Testregion: sonst dreifache Laufzeit mit vier Threads). Paketgrößen und Schwelle beeinflussen
  nur die Laufzeit, nie das Ergebnis.

### 5.3 Thread-Pool

Eigener kleiner Pool in `core/` (`uhrwerk/parallel.hpp`) mit `std::thread`, `std::mutex`,
`std::condition_variable` und `std::atomic` – keine neue Abhängigkeit (Taskflow aus Kapitel 02
lohnt sich erst mit einem Job-Graphen über mehrere Systeme). `SimConfig::threads`: 0 = alle
Kerne (`std::thread::hardware_concurrency`), sonst die angegebene Zahl. Die `Simulation` besitzt
den Pool; die Brücke übernimmt die Voreinstellung 0. Die Zahl der Threads ist **kein** Teil des
Zustands und nicht in der Prüfsumme.

### 5.4 Zufall im Parallelteil

M4 braucht im Parallelteil keinen Zufall: Geburten, Sterbefälle und Wanderung sind Erwartungswerte
in Festkomma. Zufall bleibt bei den Seuchen am Jahreswechsel (nacheinander, Strom `Ereignisse`).
Regel für später: Braucht eine parallele Aufgabe Zufall, bekommt sie einen eigenen Strom aus
(Seed, Strom-Kennung, Ortsindex, Tagesnummer), nie einen gemeinsamen.

### 5.5 Prüfung

- Unit-Test: dieselbe Partie mit 1, 2, 3, 4 und 8 Threads → gleiche Prüfsumme nach jedem Jahr
  (600 Orte aus vervielfachten Testdaten, damit mehrere Pakete je Tag entstehen).
- `uhrwerk-cli determinismus` rechnet Lauf A mit allen Kernen und Lauf B mit einem Thread.
- `uhrwerk-cli lasttest --vergleich 1`: Lauf mit vielen Threads und mit einem Thread vergleichen.
- CI: Prüfsummen des Lasttests unter GCC, Clang und MSVC vergleichen (wie heute die 100 Jahre).
- Lokal einmal mit ThreadSanitizer bauen und den Lasttest laufen lassen (Datenrennen).

## 6. Leistung

### 6.1 Ausgangslage

Gemessen in M3 (Release, ein Thread): Mit Daten für alle 4.500 Orte (etwa 54.000 Blöcke) dauert
ein Spieljahr 0,29 s, das sind etwa 450 ns je Block und Monat. Hochgerechnet auf 500.000 Blöcke:
2,7 s – über dem Ziel, und das mit dem einfachen Modell aus M1. Die Zeit geht fast ganz in die
Festkomma-Division (Schleife über 128 Bit) bei `größe × rate / 12` und in der Wirtschaft, die
jeden Monat alle Blöcke neu rechnet.

### 6.2 Maßnahmen

1. **Schnellwege in `Fixed`, bitgleich:** Division ohne 128-Bit-Schleife, wenn der Zähler nach dem
   Schieben in 64 Bit passt (|a| < 2⁴⁷, im Spiel praktisch immer); Multiplikation mit einem
   einzigen 64-Bit-Produkt, wenn beide Beträge unter 2³² liegen. Das Ergebnis ist in beiden Fällen
   mathematisch dasselbe; Tests vergleichen Schnell- und Langweg mit Zufallsstichproben und
   Grenzfällen. **Prüfstein: Die Referenzprüfsumme `40de8c42e8f7a0ff` bleibt nach diesem Schritt
   unverändert.**
2. **Keine Division in der Blockschleife:** Monatswerte und Faktoren werden beim Laden bzw. einmal
   je Ort und Beruf berechnet (Abschnitt 4.1).
3. **Rechentag-Listen und Orte je Staat** statt täglicher Suche über alle Orte und
   `state_locations()` mit neuem Vektor bei jedem Aufruf.
4. **Steuern je Ort** statt Neuberechnung aller Blöcke in der Wirtschaft (Abschnitt 4.6).
5. **Mehrere Kerne** (Abschnitt 5).

### 6.3 Schätzung

Je Block und Monat etwa 30 Festkomma-Operationen, mit Schnellwegen etwa 150 ns. Bei 500.000
Blöcken sind das 6 Mio. Blockschritte im Jahr: etwa 0,9 s mit einem Thread, 0,3 s mit vier.
Dazu kommen die Neuordnung (höchstens 12 Durchläufe, etwa 30 ms) und das Zusammenführen. Das Ziel
von 2 s ist damit erreichbar, auch mit einem Thread knapp. Die Schätzung prüft Schritt 1 der
Umsetzung mit einer Messung.

**Messung nach Schritt 1** (Release, ein Thread, Modell aus M1, Blöcke der 60 Orte mit Daten auf
alle 4.542 Orte kopiert): 307.000 Blöcke rechnen 0,05–0,07 s je Spieljahr, also etwa 18 ns je
Block und Monat statt 450 ns. 100 Jahre mit den echten Daten: 0,008 s statt 0,60 s. Die
Referenzprüfsumme `40de8c42e8f7a0ff` ist unverändert.

**Messung am Ende von M4** (Release, volles Modell, Lasttest-Welt mit 499.992 Blöcken in 4.542
Orten, Median aus 3 Spieljahren):

| Rechner | Threads | Median je Spieljahr | Arbeitsspeicher (Spitze) |
|---|---|---|---|
| Entwicklungsumgebung: Xeon 2,8 GHz, 4 Kerne | 4 | 0,33–0,40 s | 42 MB |
| ebenso | 2 | 0,64 s | |
| ebenso | 1 | 0,88 s | |
| GitHub-Runner `ubuntu-latest` (2 vCPU) | 2 | 0,52 s | 44 MB |

Das entspricht etwa 150 ns je Block und Monat mit einem Thread – wie geschätzt. Mit vier Threads
ist der Lauf 2,2- bis 2,7-mal so schnell (die Werte schwanken zwischen Läufen um etwa 20 %); den Rest begrenzen die Teile, die nacheinander laufen
(Neuordnung, Veröffentlichen der Momentaufnahmen, Sammeln der Wanderung) und das Buchen der rund
2,8 Mio. Wanderungsströme je Jahr. 100 Jahre mit den 60 Orten der Testregion dauern 0,12 s.

## 7. Lasttest

**Lasttest-Welt.** `make_load_test_world(base, options)` in `core/` (`uhrwerk/lasttest.hpp`,
`LoadTestOptions { blocks, seed }`)
erzeugt aus der geladenen Karte (4.542 Orte, Staaten, Gelände, Flächen) eine Welt, in der **jeder**
Ort Daten hat. Eigener Zufallsstrom `Lasttest = 5`, damit ist die Welt bei gleichem Seed immer
gleich.

- Besitzer: der Besitzer des nächsten Ortes mit echten Daten.
- Bevölkerung: Tragfähigkeit × gleichverteilt 0,3–0,9; Verwüstung 0–0,6.
- Blöcke je Ort ≈ Zielzahl / Orte (≈ 110 bei 500.000): alle Berufe × alle Konfessionen × 4–5
  zufällig gewählte Kulturen, Anteile gleichmäßig, damit keine Blöcke sofort aufgelöst werden.
- Die Wanderung zwischen Orten mit verschiedenen Kulturen erzeugt laufend neue Blöcke; die
  Neuordnung wird also mitgemessen.

**Befehl:**

```
uhrwerk-cli lasttest [--bloecke N] [--jahre J] [--threads T] [--vergleich T2] [--grenze S] [--seed S]
```

Standard: 500.000 Blöcke, 3 Jahre, alle Kerne. Ausgabe: Aufbauzeit, Zeit je Spieljahr (365 Tage),
Median, Blöcke am Ende, Prüfsumme je Jahr. `--vergleich T2` wiederholt den Lauf mit T2 Threads und
meldet Exitcode 1 bei abweichender Prüfsumme. `--grenze S` meldet Exitcode 1, wenn der Median über
S Sekunden liegt.

**CI:**

| Wo | Was |
|---|---|
| Kern-Jobs (Debug) | CTest `lasttest_threads`: 20.000 Blöcke, 1 Jahr, 3 Threads gegen 1 Thread |
| Lasttest (Linux, Release) | `lasttest` mit 500.000 Blöcken, 3 Jahren, `--vergleich 1 --grenze 3` (Puffer für schwankende Runner); Ausgabe in der Zusammenfassung des Laufs |
| Determinismus | Prüfsumme des Lasttests (50.000 Blöcke, 2 Jahre, Debug) unter GCC, Clang und MSVC gleich |

Referenz: Standardlauf (500.000 Blöcke, 3 Jahre, Seed 1648) endet mit der Prüfsumme
`a5a4c0d80a2f23ea` (M4), seit M5 `3d0d1ce2215dba7c`, bei jeder Zahl von Threads.

## 8. Abnahme

| Kriterium | Prüfung | Stand (Ende M4) |
|---|---|---|
| **500.000 Blöcke, Median ≤ 2,0 s je Spieljahr** auf 4 Kernen | `uhrwerk-cli lasttest` in der CI (Job „Lasttest“, Grenze 3 s als Puffer) und in der Entwicklungsumgebung. Der Referenz-PC aus Kapitel 02 (6 Kerne/12 Threads) ist schneller als beide | **erfüllt:** 0,33–0,40 s mit 4 Kernen, 0,88 s mit einem; auf dem GitHub-Runner (nur 2 vCPU) 0,52 s (Abschnitt 6.3) |
| Prüfsummen bitgleich unter GCC, Clang, MSVC | CI-Job Determinismus (100 Jahre Testdaten und Lasttest) | **erfüllt** |
| Prüfsummen gleich bei 1, 2, 3, 4, 8 Threads | Unit-Test, `--vergleich` (CTest und CI-Job Lasttest), `determinismus` (zweiter Lauf mit einem Thread); ThreadSanitizer ohne Befund | **erfüllt** |
| Exakte Buchführung | Test: Bevölkerungsänderung = Geburten − Sterbefälle − Seuchentote − Auswanderung (bitgenau in `Fixed`); Σ Zuzug = Σ Wegzug | **erfüllt** |
| Kalibrierung | Test mit den Daten der Testregion: Bevölkerung 1648 → 1700 wächst um den Faktor 1,20–1,45; Kinderanteil 30–40 %, kein Block negativ | **erfüllt:** ×1,29 (Abschnitt 4.9) |
| Speicher | 500.000 Blöcke unter 200 MB Arbeitsspeicher (Lasttest gibt den Spitzenwert aus, soweit das Betriebssystem ihn liefert) | **erfüllt:** 42–44 MB (Linux) |

## 9. Prüfsumme

Neu in der Prüfsumme: alle Felder der Blöcke (statt `size`), die Momentaufnahmen, die Bilanzen,
die Steuern je Ort und die Zugänge. Nicht dazu gehören die Thread-Zahl und abgeleitete Hilfsdaten
(Wanderungsziele, Listen). Die Referenzprüfsumme nach 100 Jahren ändert sich mit Schritt 3 der
Umsetzung; der neue Wert wird in [README.md](README.md) und [data/README.md](../../data/README.md)
nachgetragen, ebenso eine Referenz für den Lasttest.

Referenz nach 100 Jahren mit `data/` (Seed 1648, jede Thread-Zahl): `ed6b14a02cfbfa5d` (seit M4,
Schritt 3; vorher `40de8c42e8f7a0ff`). Seit M5 gehört die Wirtschaft dazu; die aktuelle Referenz steht in
[README.md, Abschnitt 3](README.md#3-determinismus).

## 10. Brücke und Oberfläche

Klein gehalten, damit man die neue Bevölkerung im Spiel sieht:

- `get_location_info`: Altersgruppen, Lebensstandard und Alphabetisierung (nach Personen
  gemittelt), Mehrheitskultur und Kulturanteile, Bilanz des Vorjahres.
- `get_state_info`: Bevölkerungsbilanz des Vorjahres.
- Zwei neue Kartenmodi: `MAP_LEBENSSTANDARD = 6` (Verlauf) und `MAP_KULTUR = 7` (Farbe der
  Mehrheitskultur).
- Ortspanel und Legende in Godot; Einheitentest für die neuen Modi.

## 11. Umsetzungsschritte

Nacheinander, jeder Schritt mit Build unter GCC und Clang, Tests und clang-format, dann Commit:

| Schritt | Inhalt | Prüfstein |
|---|---|---|
| 1 | **Leistung ohne Modelländerung:** Schnellwege in `Fixed`, Rechentag-Listen, Orte je Staat, Thread-Pool (noch ungenutzt), Messung | Referenzprüfsumme unverändert; Messung bestätigt oder korrigiert Abschnitt 6.3 |
| 2 | **Datenmodell:** Kulturen, neuer Block, Lader, Daten (`regeln`, `gelaende`, `berufe`, `konfessionen`, `kulturen`, `kultur` in den Orten), Archivar mit Tests, chronik-skript.md | `uhrwerk-cli pruefen` ohne Fehler, Archivar `--pruefen` |
| 3 | **Bevölkerungsmodell:** Formeln aus Abschnitt 4, Parallelphase, Zusammenführen, Neuordnung, Bilanzen, Prüfsumme, Kalibrierung | Tests aus Abschnitt 8 außer Lasttest; neue Referenzprüfsumme |
| 4 | **Lasttest:** Lasttest-Welt, `uhrwerk-cli lasttest`, CTest, CI-Jobs | Abnahme 500.000 Blöcke ≤ 2 s; Threads und Compiler bitgleich |
| 5 | **Brücke und Godot:** Abschnitt 10 | Godot-Tests in der CI grün |
| 6 | **Doku und Review:** technik/README, chronik-skript.md, data/README, meilensteine.md; abschließendes Review | CI grün |

## 12. Entscheidungen (bestätigt vor der Umsetzung)

1. **Altersgruppen im Block** statt als Blockmerkmal (Abschnitt 2.3).
2. **Sechs Berufe** der Epoche bleiben; die 20 Berufsgruppen kommen mit den Betrieben in M5.
3. **Lebensstandard aus dem Einkommen** (Ertrag, Verwüstung, Steuern, Übervölkerung), bis M5
   Märkte und Güter bringt.
4. **Kultur** je Landschaft mit Abweichungen je Ort, unabhängig von der Konfession verteilt.
5. **Abnahmegrenze 2,0 s** je Spieljahr auf 4 Kernen; nach der Schätzung wäre auch 1 s möglich.
   Vorschlag: 2,0 s bleibt die Grenze, der tatsächliche Wert wird dokumentiert.
6. **Keine neue Abhängigkeit** für Threads (eigener kleiner Pool statt Taskflow).
