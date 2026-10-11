# Bevölkerung (Meilenstein M4) – Entwurf

> **Status: Entwurf zur Abstimmung.** Dieses Dokument beschreibt, wie M4 „Bevölkerung und
> Lasttest“ ([meilensteine.md](../meilensteine.md#m4--bevölkerung-und-lasttest)) gebaut werden
> soll: Datenmodell, Ablauf, Formeln, Mehrkernberechnung, Lasttest und Abnahme. Nach der Freigabe
> wird es verbindlich wie [chronik-skript.md](chronik-skript.md); Abweichungen bei der Umsetzung
> werden hier nachgetragen. Abschnitt 12 sammelt die Entscheidungen, die vor dem Start bestätigt
> werden sollen.

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
| **Zugänge** (`Arrival`) | Zugezogene, für die es im Ort noch keinen Block gibt | werden bei der Neuordnung zu Blöcken |

Diese Daten gehören zum Zustand (Prüfsumme, später Spielstand) und liegen im `World`.
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
| 5 | **Zusammenführen**: Momentaufnahmen veröffentlichen, Wanderung buchen | nach Schritt 4 | – |
| 6 | Wirtschaft der Staaten mit heutigem Rechentag | Staat *j* am Tag `(j % 28) + 1` | – |
| 7 | Jahreswechsel: Seuchen, Abbau der Verwüstung | 1. Januar | – |
| 8 | Datum weiterschalten | jeden Tag | – |

Der Rechentag der Orte bleibt wie in M1. Statt täglich alle 4.542 Orte auf ihren Rechentag zu
prüfen, hält die Simulation je Rechentag die Liste der Orte **mit Daten** (aufsteigend).

## 4. Formeln

Alle Rechnungen in `Fixed`, in der angegebenen Reihenfolge. Jahreswerte aus `regeln.chron`
werden beim Laden einmal in Monatswerte umgerechnet: `x_monat = x / 12`. In der Schleife über die
Blöcke steht keine Division mehr, außer bei der Wanderung (Abschnitt 4.5).

### 4.1 Vorberechnung je Ort (am Rechentag, vor den Blöcken)

```
n          = Σ größe aller Blöcke des Ortes
trag       = fläche × gelände.tragfaehigkeit × (stadt ? tragfaehigkeit_stadt : 1)
reserve    = max(0, 1 − n / trag)                      Landreserve 0..1
zustand    = 1 − verwüstung × verwuestung_ertrag
je Beruf b:
  agrar_b  = b.agrar ? min(1, trag / n) : 1             abnehmender Ertrag bei Übervölkerung
  ertrag_b = b.ertrag × gelände.ertrag × zustand × agrar_b      (Taler pro Kopf und Jahr)
  netto_b  = b.steuerpflichtig ? ertrag_b × (1 − staat.steuersatz) : ertrag_b
  ziel_b   = 100 × netto_b / (netto_b + lebensstandard_bezug)   Ziel-Lebensstandard 0..100
```

Die **Landreserve** ersetzt das „Wiederaufbauwachstum“ aus M1: Ein verwüsteter, entvölkerter Ort
hat viel freies Land, also mehr und frühere Heiraten und Zuzug von Bauern. Die **Tragfähigkeit**
bremst das Wachstum, wenn der Boden ausgeschöpft ist: Der Ertrag je Kopf der Agrarberufe sinkt,
damit Lebensstandard und Geburten, und die Abwanderung steigt. Spätere Meilensteine heben die
Tragfähigkeit durch Agrartechnik (Kartoffel, Agrarreformen).

### 4.2 Lebensstandard (je Block)

```
L ← L + (ziel_b − L) × lebensstandard_anpassung
```

Mit dem Bezugseinkommen 12 Taler ergibt das 1648 ohne Verwüstung und Steuern etwa: Landarme 33,
Bauern 50, Handwerker 62, Geistlichkeit 71, Bürger 77, Adel 87.

### 4.3 Geburten, Sterbefälle, Altern (je Block)

Alle Größen werden aus dem Stand **vor** dem Schritt berechnet (k, j, e, a = Kinder, Jung,
Erwerbsalter, Alt):

```
s     = clamp(1 + sterblichkeit_armut × (50 − L) / 50, 0.5, 2)      Sterblichkeit nach Lebensstandard
f     = fruchtbarkeit_monat × (1 + fruchtbarkeit_landreserve × reserve)
                            × (1 + fruchtbarkeit_wohlstand × (L − 50) / 50)
geb   = (j + e × anteil_fruchtbar_erwerb) × f
tk    = k × sterblichkeit_kinder_monat × s     (ebenso tj, te, ta)
ak    = k × 1/180      aj = j × 1/180      ae = e × 1/420               Altern (15, 15, 35 Jahre)
k ← k + geb − tk − ak
j ← j + ak − tj − aj
e ← e + aj − te − ae
a ← a + ae − ta
```

`(50 − L) / 50` und `(L − 50) / 50` werden als Multiplikation mit vorab berechneten Regelwerten
geschrieben. Keine Altersgruppe wird negativ: Alle Abgänge einer Altersgruppe zusammen (auch mit
Wanderung, Abschnitt 4.5) liegen selbst bei `s = 2` unter 2 % je Monat, und die Multiplikation
rundet ab. Bilanz: `geburten += geb`, `sterbefaelle += tk+tj+te+ta`.

### 4.4 Bildung (je Block)

```
ziel  = min(1, beruf.bildung × konfession.bildung × (stadt ? bildung_stadt : 1) × (50 + L) / 100)
A    ← A + (ziel − A) × bildung_anpassung_monat
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
beruf am Ziel   b' = (ziel.stadt und b.in_stadt gesetzt) ? b.in_stadt : b
anziehung(o, b) = ziel_b(o) + (b.agrar ? wanderung_landreserve × reserve(o) : 0)
für jedes Ziel z:
  diff_z  = max(0, anziehung(z, b') − anziehung(hier, b))     z aus der Momentaufnahme, hier aktuell
  strom_z = j × wanderung_rate_monat × diff_z / 100 × (besitzer verschieden ? wanderung_fremd : 1)
summe = Σ strom_z;  falls summe > j × wanderung_max_monat: jeden strom_z mit (j × wanderung_max_monat) / summe skalieren
Ströme unter wanderung_mindestmenge entfallen (die Menschen bleiben)
auswanderung = j × auswanderung_rate_monat × max(0, auswanderung_schwelle − L) / 100
j ← j − Σ strom_z − auswanderung
```

Jeder Strom kommt als Eintrag in den **Ausgang** des Ortes: Zielort, Beruf am Ziel, Konfession,
Kultur, Menge, Lebensstandard und Alphabetisierung des Herkunftsblocks. Auswanderung verlässt die
Simulation (Bilanz `auswanderung`). Damit sind beide Richtungen abgedeckt: vom übervölkerten Land
in die Stadt, aber auch aus Städten und dichten Orten in entvölkerte Landstriche mit freien Höfen.
`wanderung_fremd` bildet ab, dass Wegzug über Landesgrenzen schwerer war (Leibeigenschaft,
Abzugsgeld).

### 4.6 Produktion und Steuern

Am Rechentag des Ortes, mit den Werten aus 4.1:

```
produktion_monat = Σ größe × ertrag_b / 12      (Blöcke aufsteigend)
steuern_monat    = Σ größe × ertrag_b / 12 × staat.steuersatz   (nur steuerpflichtige Berufe)
```

Die Wirtschaft eines Staates summiert an **seinem** Rechentag die zuletzt berechneten
`steuern_monat` seiner Orte (aufsteigend), statt alle Blöcke neu zu rechnen. Hofhaltung und Kasse
wie in M1. Geändert gegenüber M1: der Agrarfaktor `agrar_b` und der Zeitpunkt (Steuern werden am
Rechentag des Ortes festgestellt). `location_economy()` rechnet für die Anzeige weiter frisch.

### 4.7 Zusammenführen und Neuordnung

**Zusammenführen** (Schritt 5, nacheinander): für jeden Ort des Tages aufsteigend

1. Momentaufnahme veröffentlichen (Abschnitt 5.1).
2. Ausgang in Reihenfolge buchen: Gibt es im Zielort einen Block mit gleichem Schlüssel, kommen
   die Menschen in dessen Altersgruppe Jung; Lebensstandard und Alphabetisierung werden nach
   Personen gewichtet gemittelt. Sonst: Menge ≥ `block_mindestgroesse` → Zugang (gleicher Schlüssel
   im selben Ort wird zusammengefasst); kleiner → in den ähnlichsten Block. Bilanz `zuzug`/`wegzug`.

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
| `fruchtbarkeit` | 0,095 | Geburten je Person im fruchtbaren Alter und Jahr |
| `anteil_fruchtbar_erwerb` | 0,4 | Anteil der 30–64-Jährigen im fruchtbaren Alter |
| `fruchtbarkeit_landreserve` | 0,3 | Zuschlag bei voller Landreserve |
| `fruchtbarkeit_wohlstand` | 0,15 | Zu- oder Abschlag je 50 Punkte Lebensstandard |
| `sterblichkeit_kinder` / `_jung` / `_erwerb` / `_alt` | 0,045 / 0,010 / 0,018 / 0,11 | jährliche Sterblichkeit je Altersgruppe |
| `sterblichkeit_armut` | 0,3 | Zu- oder Abschlag je 50 Punkte Lebensstandard |
| `tragfaehigkeit_stadt` | 3 | Faktor der Tragfähigkeit für Städte (Markt, Handel, Gewerbe) |
| `lebensstandard_bezug` | 12 | Netto-Einkommen (Taler pro Kopf und Jahr) für Lebensstandard 50 |
| `lebensstandard_anpassung` | 0,25 | Anteil der Lücke zum Ziel je Monat |
| `bildung_stadt` / `bildung_anpassung` | 1,5 / 0,03 | Stadtfaktor; Anteil der Lücke je Jahr |
| `wanderung_ziele` / `wanderung_radius_km` | 4 / 60 | Ziele je Ort |
| `wanderung_rate` / `wanderung_max` | 0,3 / 0,12 | Anteil der Jungen je Jahr bei 100 Punkten Unterschied; Obergrenze je Jahr |
| `wanderung_landreserve` / `wanderung_fremd` | 20 / 0,5 | Punkte für freies Land; Faktor über Landesgrenzen |
| `auswanderung_schwelle` / `auswanderung_rate` | 30 / 0,05 | Lebensstandard, unter dem ausgewandert wird; Anteil je Jahr und 100 Punkte |
| `block_mindestgroesse` / `wanderung_mindestmenge` | 1 / 0,05 | Personen |

Die **Tragfähigkeit** je Gelände liegt beim 1,8-Fachen der Dichte von 1648 auf dem Land
(gemessen an den 60 Orten mit Daten: 10–33 Einwohner je km²), z. B. Börde 60, Hügelland 50,
Flussaue 45, Flachland 30, Heide, Bruch und Seenplatte 20.

**Kalibrierung.** Eine Überschlagsrechnung (Gleitkomma, ein Ort mit mittleren Werten, Verwüstung
0,5) ergibt mit diesen Werten: Geburtenrate 38–41 ‰, Sterberate etwa 34 ‰ (typisch für die Frühe
Neuzeit), Bevölkerung ×1,34 bis
1700 (Ziel aus Kapitel 07: 11 → 14,5 Mio., ×1,32) und ×2,0 bis 1816 (Ziel ×2,3; die Lücke
schließt später die steigende Tragfähigkeit). Der Altersaufbau bleibt stabil (Kinder 33–36 %,
Alte 7–8 %). Die endgültigen Werte legt die Umsetzung mit dem echten Kern fest; ein Test prüft
den Bereich (Abschnitt 8).

## 5. Mehrkernberechnung

### 5.1 Grundsatz

Das Ergebnis darf nicht von der Zahl der Threads abhängen. Das gilt, wenn jede parallele Aufgabe

1. nur **eigene** Daten schreibt: die Blöcke ihres Ortes, ihren Ausgang, ihre Bilanz, ihre
   Steuern und ihre **neue** Momentaufnahme (zweiter Puffer), und
2. alles andere nur **liest**, und zwar Daten, die während der parallelen Phase niemand
   schreibt: Regeln, Staaten, unveränderlichen Inhalt und die **veröffentlichten**
   Momentaufnahmen anderer Orte.

Dann ist jede Aufgabe eine reine Funktion ihrer Eingaben, und es ist gleich, welcher Thread sie
wann rechnet. Alles, was Orte verbindet – Wanderung buchen, Momentaufnahmen veröffentlichen,
Staatseinnahmen summieren, Neuordnung, Seuchen –, läuft danach **nacheinander in fester
Reihenfolge** (Orte aufsteigend, Einträge in Reihenfolge).

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
  (Testdaten und eine kleine Lasttest-Welt).
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

## 7. Lasttest

**Lasttest-Welt.** `make_load_test_world(base, blocks, seed)` in `core/` (`uhrwerk/lasttest.hpp`)
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
| Kern-Jobs (Debug) | CTest `lasttest_threads`: 20.000 Blöcke, 1 Jahr, 1 gegen 3 Threads |
| Kern (Linux, Release) | `lasttest` mit 500.000 Blöcken, 3 Jahren, `--grenze 3` (Puffer für schwankende Runner); Zeit steht im Protokoll |
| Determinismus | Prüfsumme des Lasttests (100.000 Blöcke, 2 Jahre) unter GCC, Clang und MSVC gleich |

## 8. Abnahme

| Kriterium | Prüfung |
|---|---|
| **500.000 Blöcke, Median ≤ 2,0 s je Spieljahr** auf 4 Kernen | `uhrwerk-cli lasttest` auf dem GitHub-Runner `ubuntu-latest` (4 vCPU). Der Referenz-PC aus Kapitel 02 (6 Kerne/12 Threads) ist schneller; was der Runner schafft, schafft er auch. Messwert und Rechner kommen in [meilensteine.md](../meilensteine.md) |
| Prüfsummen bitgleich unter GCC, Clang, MSVC | CI-Job Determinismus (100 Jahre Testdaten und Lasttest) |
| Prüfsummen gleich bei 1, 2, 3, 4, 8 Threads | Unit-Test und `--vergleich` |
| Exakte Buchführung | Test: Bevölkerungsänderung = Geburten − Sterbefälle − Seuchentote − Auswanderung (bitgenau in `Fixed`); Σ Zuzug = Σ Wegzug |
| Kalibrierung | Test mit den Daten der Testregion: Bevölkerung 1648 → 1700 wächst um den Faktor 1,20–1,45; Kinderanteil 30–40 %, kein Block negativ |
| Speicher | 500.000 Blöcke unter 200 MB Arbeitsspeicher (Lasttest gibt den Spitzenwert aus, soweit das Betriebssystem ihn liefert) |

## 9. Prüfsumme

Neu in der Prüfsumme: alle Felder der Blöcke (statt `size`), die Momentaufnahmen, die Bilanzen,
die Steuern je Ort und die Zugänge. Nicht dazu gehören die Thread-Zahl und abgeleitete Hilfsdaten
(Wanderungsziele, Listen). Die Referenzprüfsumme nach 100 Jahren ändert sich mit Schritt 3 der
Umsetzung; der neue Wert wird in [README.md](README.md) und [data/README.md](../../data/README.md)
nachgetragen, ebenso eine Referenz für den Lasttest.

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

## 12. Entscheidungen zur Bestätigung

1. **Altersgruppen im Block** statt als Blockmerkmal (Abschnitt 2.3).
2. **Sechs Berufe** der Epoche bleiben; die 20 Berufsgruppen kommen mit den Betrieben in M5.
3. **Lebensstandard aus dem Einkommen** (Ertrag, Verwüstung, Steuern, Übervölkerung), bis M5
   Märkte und Güter bringt.
4. **Kultur** je Landschaft mit Abweichungen je Ort, unabhängig von der Konfession verteilt.
5. **Abnahmegrenze 2,0 s** je Spieljahr auf 4 Kernen; nach der Schätzung wäre auch 1 s möglich.
   Vorschlag: 2,0 s bleibt die Grenze, der tatsächliche Wert wird dokumentiert.
6. **Keine neue Abhängigkeit** für Threads (eigener kleiner Pool statt Taskflow).
