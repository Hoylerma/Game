# Politik, Parteien und Wahlen (Meilenstein M8)

> **Status: verbindlich seit M8** – Entscheidungen 1–10 wie empfohlen freigegeben; Stand in
> Abschnitt 14 ([meilensteine.md](../meilensteine.md#m8--politik-parteien-und-wahlen-)). Die Werte in
> den Abschnitten 3 bis 9 sind die Spielwerte nach der Kalibrierung (Abschnitt 14.1) und dem Review
> in Schritt b3. Grundlage sind
> [09 Politik, Staat und Gesetze](../09_politik_staat_und_gesetze.md) (Abschnitte 2–6),
> [10 Parlamente, Wahlen, Koalitionen](../10_parlamente_wahlen_koalitionen.md) (Abschnitte 2, 3
> und 6), [11 Parteien-Datenbank](../11_parteien_datenbank.md) (Abschnitte 1 und 2),
> [07 Bevölkerung und Gesellschaft](../07_bevoelkerung_und_gesellschaft.md) und
> [20 Verantwortung und Erinnerungskultur](../20_verantwortung_und_erinnerungskultur.md).

Im Lesezeichen 1848 steht die Märzrevolution bevor. Wenige Wochen nach dem Start wählt der Deutsche
Bund die **Nationalversammlung** in der Frankfurter Paulskirche. M8 bringt dafür das politische
Grundgerüst in die Simulation:

- politische Strömungen und die Fraktionen der Paulskirche;
- Interessengruppen als gesellschaftliche Basis;
- Verfassung und Wahlrecht je Staat;
- Wahlkreise und eine Wahl, die aus den Bevölkerungsblöcken eine Sitzverteilung ergibt;
- in Godot den **Wahlabend** und einen ersten **Parteienmodus**.

Gesetzgebung, Regierungsbildung, Koalitionen und die Krise 1848/49 folgen mit M11 (Ereignisse und
Revolution) und M12 (KI). M8 legt die Daten und die Wahlmechanik an, auf die diese Meilensteine
bauen.

## 1. Umfang

Der Meilensteinplan verlangt:

- Strömungen und Parteien bzw. Fraktionen von 1848 (Paulskirche), Interessengruppen
- Verfassungen, Gesetze und Wahlrecht (Zensus- und Dreiklassenwahlrecht)
- Wahlkreise aus den Kreisen, Wahlen mit Ergebnis je Wahlkreis
- Wahlabend als eigener Bildschirm
- Parteienmodus (Spielen als Fraktion)
- Abnahme: Eine Wahl zur Nationalversammlung 1848 ergibt eine historisch plausible
  Sitzverteilung, die Abweichung ist dokumentiert; Wahlabend mit Hochrechnung und Ergebnis je
  Wahlkreis.

Vorschlag (Entscheidung 9): **zwei Teile** wie bei M7.

| Teil | Inhalt | Ergebnis |
|---|---|---|
| **M8a** | Strömungen, Fraktionen, Interessengruppen, Verfassung und Wahlrecht als Daten; Wahlkreise; Wahlsimulation im Kern; Kommandozeile `wahl` mit Wahllabor; Kalibrierung der Wahl 1848 | Sitzverteilung 1848 im Zielkorridor, in der CI |
| **M8b** | Brücke und Godot: Kartenmodus „Wahlen“, Wahlabend, Parteienmodus; Dreiklassenwahl Preußen 1849; Doku und Review | Wahlabend und Parteienmodus spielbar |

**Nicht in M8:**
- Gesetzgebungsverfahren und Gesetzesänderungen durch den Spieler (Abschnitt 6);
- Regierungsbildung, Koalitionen, Abstimmungen in der Paulskirche;
- Dynamik der Interessengruppen (Zustimmung, Aktionen);
- Persönlichkeiten mit Namen (Kapitel 13);
- Wahlen nach 1849 (die Wahltermine stehen in den Daten; regelmäßige Wahlen kommen mit den
  Verfassungen der Folgezeit).

## 2. Ausgangslage

**Vorhanden:**
- 97.257 Bevölkerungsblöcke mit Beruf (sieben, davon drei Schichten), Konfession (vier), Kultur
  (38), Altersaufbau (0–14, 15–29, 30–64, 65+), Alphabetisierung und Lebensstandard.
- Einkommen und Steuern je Beruf und Ort aus dem Wirtschaftsmodell.
- Staaten mit Regierungsform, Bundesgebiet je Ort, Verwaltung bis zum Kreis (Preußen, Österreich).
- 626 räumliche Kreise; datierte Felder je Staat in `staaten_historie.csv`.
- Zufallsströme 1–6, Zustandsverzeichnis mit Prüfsumme und Spielstand (Format 1).

**Es fehlt:** jedes politische Objekt, das Geschlecht der Blöcke (Abschnitt 7.1) und ein Termin,
an dem die Simulation etwas außerhalb des Tagesablaufs tut (der Wahltag).

**Quellen:** Die Websuche ist in dieser Umgebung erreichbar, liefert aber nur Auszüge. Direkte
Seitenabrufe (Wikipedia, Wikidata) sperrt die Netzwerkregel weiterhin. Gefundene Hinweise:
- die Fraktionen der Nationalversammlung mit ungefähren Stärken (englische Wikipedia „Factions in
  the Frankfurt Assembly“, ohne belastbare Tabelle);
- das Wahlrecht für „selbstständige“ Männer, laut Bundesarchiv und DHM 75–85 % der Männer;
- indirekte Wahl über Wahlmänner in den meisten Staaten.

Alle historischen Angaben bleiben daher `ungeprueft` wie in M7; die Fachprüfung schließt sich an
die von M7 an (Entscheidung 10).

## 3. Strömungen, Fraktionen, Interessengruppen

### 3.1 Strömungen (`common/stroemungen.chron`)

Fünf Strömungen nach Kapitel 11, Abschnitt 2.1. Jede hat einen Namen, eine Farbe und eine
Position auf der **Links-rechts-Achse** von −100 (Volkssouveränität, Republik) bis +100
(monarchisches Prinzip, Restauration). Die Achse ordnet auch die Stichwahl (Abschnitt 7.5) und die
Fraktionen (3.2).

| Strömung | Position (Spielwert) | Basis nach Kapitel 11 |
|---|---|---|
| `konservative` | +70 | Adel, Hof, orthodoxe Protestanten |
| `katholiken` (politischer Katholizismus) | +35 | katholisches Milieu |
| `liberale` (konstitutionelle Liberale) | +10 (Entwurf +15, kalibriert in a4) | Bildungs- und Wirtschaftsbürgertum |
| `demokraten` | −50 (Entwurf −45, kalibriert in a4) | Kleinbürgertum, Handwerker, Studenten |
| `arbeiter` (frühe Arbeiterbewegung) | −75 | Handwerksgesellen, Arbeiter |

Eine zweite Achse (großdeutsch – kleindeutsch) spaltet die Paulskirche erst ab Herbst 1848; sie
kommt mit M11.

### 3.2 Fraktionen der Nationalversammlung

Die Fraktionen bildeten sich nach der Wahl in Frankfurt, benannt nach ihren Versammlungslokalen.
Gewählt wurden Personen, keine Parteien. Im Modell bekommt jeder gewählte Abgeordnete eine Position
auf der Achse (Abschnitt 7.6) und kommt zu der Fraktion, in deren Bereich sie fällt. Ein Teil
bleibt fraktionslos.

| Fraktion | Richtung | Bereich (Spielwert) |
|---|---|---|
| Café Milani | konservative Rechte | +55 … +100 |
| Casino | rechtes Zentrum, konstitutionell-liberal | +20 … +55 |
| Landsberg | rechtes Zentrum (später aus dem Casino) | +12 … +20 |
| Augsburger Hof | gemäßigtes linkes Zentrum | +4 … +12 |
| Württemberger Hof | linkes Zentrum | −10 … +4 |
| Westendhall | gemäßigte Linke | −25 … −10 |
| Deutscher Hof | demokratische Linke | −50 … −25 |
| Donnersberg | äußerste Linke | −100 … −50 |

Die Fraktionen sind **Daten des Parlaments** in `common/parlamente.chron`, mit Name, Farbe und
Bereich. Sie gelten ab dem Stichtag `fraktionen_ab` (1. Oktober 1848), denn Landsberg, Augsburger
Hof und Westendhall entstanden erst im Sommer 1848. Im Parteienmodus spielt man eine Fraktion (Abschnitt 9).

### 3.3 Interessengruppen (`common/interessengruppen.chron`)

Elf Interessengruppen, die es 1848 gibt (Kapitel 09, Abschnitt 5.1): Adel und Ritterschaft,
katholische Kirche, evangelische Kirche, Handel und Finanzen, Zünfte und Mittelstand, Bauernschaft,
Beamtenschaft, Militär, Bildungsbürgertum, Arbeiterbewegung (ab 1848) und Nationalbewegung.

Jede Gruppe beschreibt zwei Dinge:
- **Mitglieder:** Anteile je Beruf, Konfession und Bildung, z. B. Bauernschaft = 90 % der Bauern
  und 30 % der Landarmen; katholische Kirche = 40 % der Katholiken. Ein Block kann mehreren Gruppen
  angehören.
- **Neigung:** Gewichte auf die Strömungen, z. B. Bildungsbürgertum: liberale 0,55, demokraten
  0,3, konservative 0,1, katholiken 0,05.

Daraus folgen je Staat:
- **Anteil** = Mitglieder ÷ Erwachsene;
- **Einfluss** = Einkommen der Mitglieder ÷ Einkommen aller.

Beides ist abgeleitet, also kein Zustand. Zustimmung, Forderungen und Aktionen der Gruppen kommen
später (Entscheidung 6). In M8 tragen die Gruppen vor allem die Wahlneigung (Abschnitt 7.3).

Umgesetzt (Schritt b3) als `interest_group_standing` im Kern, über die Orte des Staates:
- **Anteil:** Mitglieder ab 15 Jahren ÷ Einwohner ab 15;
- **Einfluss:** Einkommen der Mitglieder ÷ Einkommen aller Blöcke. Das Einkommen eines Blocks ist das
  Einkommen je Kopf seines Berufs im Ort × Größe, wie bei den Spenden (Abschnitt 9.1).

Die Staatsinfo zeigt beides im Fenster „Verfassung und Gruppen“, nur in Szenarien mit Parlament.

## 4. Verfassung und Wahlrecht je Staat

Neue datierte Felder je Staat in `staaten_historie.csv`, mit Quelle und Sicherheit:

| Feld | Werte | Beispiel |
|---|---|---|
| `verfassung` | `keine`, `staendisch`, `konstitutionell` | Baden, Bayern, Württemberg 1818/19 `konstitutionell`; Preußen 1848.12.05 (oktroyiert) |
| `wahlrecht` | `keines`, `staendisch`, `zensus`, `dreiklassen`, `allgemein_maenner` | Preußen 1849.05.30 `dreiklassen` |
| `wahlsystem` | `stichwahl`, `relativ`; dazu `wahl_indirekt` (ja/nein) und bei `zensus` die Schwelle `wahl_zensus` | Baden `stichwahl`, `wahl_indirekt = ja` |

Die Felder gelten **zum Startdatum** (wie alle Staatsfelder) und beschreiben den Landtag. Jede Wahl
trägt ihr eigenes Wahlrecht im Parlament (Abschnitt 5.1), so auch die Dreiklassenwahl in Preußen 1849.
Das Wahlrecht eines **Parlaments** kann das des Staates überstimmen: Für die Nationalversammlung
galt der Bundesbeschluss vom 7. April 1848. Danach durften alle volljährigen, „selbstständigen“
Männer wählen, ohne Zensus, Stand oder Konfession; die Auslegung lag bei den Staaten.

Die Optionen in Zahlen (Spielwerte, Abschnitt 7.1):

| Wahlrecht | Wer wählt | Gewicht |
|---|---|---|
| `allgemein_maenner` | Männer ab 25, „selbstständig“ nach Beruf (Landarme und Fabrikarbeiter zum Teil, je Staat) | gleich |
| `zensus` | Männer ab 25 mit Steuerleistung über einer Schwelle (Feld `wahl_zensus` je Staat) | gleich |
| `dreiklassen` | Männer ab 24 | drei Klassen nach Steuerleistung, jede wählt ein Drittel der Wahlmänner |
| `staendisch` | Vertreter der Stände | keine Volkswahl (kein Wahlkreis) |

Ein **Gesetzesmodell** mit 36 Gruppen (Kapitel 09, Abschnitt 6) baut M8 nicht. Die Felder sind die
erste Gruppe davon (Entscheidung 7).

## 5. Parlamente und Wahlkreise

### 5.1 Parlamente (`common/parlamente.chron`)

```text
parlament nationalversammlung {
    name          = "Deutsche Nationalversammlung"
    ebene         = bund                # bund oder Staatstag (z. B. PRU)
    sitze         = 649                 # Sollzahl
    seit          = 1848.03.01
    wahltag       = 1848.05.01
    sitz          = frankfurt_am_main
    wahlrecht     = allgemein_maenner
    wahlsystem    = stichwahl
    wahl_indirekt = ja
    fraktionen_ab = 1848.10.01
    boykott       = { tschechen }
    boykott_maerkte = { boehmen }
    fraktion casino { name = "Casino"  farbe = { … }  von = 20  bis = 55 }
    …
    quelle        = "…"
}
```

Vollständiges Schema: [chronik-skript.md](chronik-skript.md), Abschnitt 3.21.

**Vorschlag (Entscheidung 1):**
- **Deutsche Nationalversammlung** 1848: kalibriert, Abnahme.
- **Preußische Zweite Kammer** 17. Juli 1849 mit Dreiklassenwahlrecht: nur
  Plausibilitätsprüfung. Es zeigt die Wirkung des Wahlrechts auf dieselbe Bevölkerung, ohne den
  Boykott der Demokraten von 1849.

Weitere Wahlen (Preußische Nationalversammlung 1848, Landtage) sind dann reine Datenarbeit.

### 5.2 Sitze je Staat

Die Wahlordnung sah einen Abgeordneten je 50.000 Einwohner nach der Bundesmatrikel vor, also 649
Sitze. Gewählt und anwesend waren rund 585: In Böhmen und Mähren boykottierten die tschechischen
Wahlkreise die Wahl, andere wählten spät oder gar nicht.

**Vorschlag (Entscheidung 2):** 649 Sitze, verteilt auf die Staaten im Verhältnis zu ihrer
Bevölkerung im Bundesgebiet (Hare-Niemeyer, jeder Staat mindestens einen Sitz). Ein Wahlkreis je
Sitz. Die Matrikel von 1819 liegt nicht vor; die heutigen Daten (45 Mio. Einwohner, davon gut 40
im Bund) ergäben bei 50.000 je Sitz über 800.

### 5.3 Wahlkreise (Werkzeug `tools/geodaten/wahlkreise_1848.py`)

Wahlkreise werden **Inhalt** wie Kreise und Verwaltung: eine Tabelle `wahlkreise.csv` mit
Wahlkreis, Parlament, Staat, Name, Sitzen und Orten ([tabellen.md](tabellen.md)). Der Archivar macht
daraus `history/wahlkreise/<parlament>.chron` ([chronik-skript.md](chronik-skript.md), Abschnitt 3.22).

Bildung:
1. **Wahlgebiet:** Die Nationalversammlung wählt im Bundesgebiet und in den im April 1848
   zugelassenen Gebieten: Ost- und Westpreußen, Posen und Schleswig, im Werkzeug alle Orte Preußens
   und Dänemarks. Ein Landtag wählt in seinem Staat. Es zählen nur Orte mit Bevölkerung 1846.
2. **Sitze je Staat** nach Einwohnern 1846 (Hare-Niemeyer, mindestens ein Sitz); dann je Region des
   Staates: in Preußen die Regierungsbezirke, in Österreich die Kronländer, sonst die Landschaften
   der Karte. Eine Region ohne eigenen Sitz kommt zur nächsten Region desselben Staates.
3. **Großstädte:** Ein Ort mit mindestens dem 1,5-Fachen der Zielgröße (Einwohner je Sitz) ist ein
   eigener Wahlkreis mit mehreren Sitzen, z. B. Berlin und Wien mit je 6 Sitzen in der
   Nationalversammlung. Hamburg bekommt so alle drei Sitze; Geesthacht und Cuxhaven gehören dann
   zum nächsten Wahlkreis.
4. **Übrige Orte:** Bausteine sind die Kreise der Verwaltung (Preußen, Österreich), sonst die
   räumlichen Kreise aus M3; zu große Bausteine zerfallen in Orte. Die Bausteine werden entlang einer
   Hilbert-Kurve über ihre Lage geschnitten. Seit dem Review in b3 ist der Schnitt optimal
   (dynamische Programmierung): Er minimiert die quadrierte Abweichung der Abschnitte vom Mittel
   der Region plus eine Strafe für Lücken über 50 km zwischen Nachbarn der Kurve. Bausteine ab
   dem 0,75-Fachen der Zielgröße zerfallen in ihre Orte. Vorher schnitt das Werkzeug gierig
   entlang der Kurve; dabei entstanden Wahlkreise vom 0,2- bis 1,8-Fachen des Ziels.
5. **Name** nach dem einwohnerstärksten Ort, Kennung `nv_<staat>_<nr>` bzw. `pr_<nr>`.

Ergebnis:
- **Nationalversammlung:** 633 Wahlkreise mit 649 Sitzen. Einwohner je Sitz im Median rund 70.000
  (Liechtenstein 6.500 als eigener Staat, höchstens 120.000). Im Verhältnis zum Mittel der Region
  liegen alle Wahlkreise beim 2/3- bis 1,5-Fachen, bis auf sechs. Die Ausreißer sind Oldenburgs
  Exklaven: Die Fürstentümer Birkenfeld (Idar-Oberstein, 26.000) und Lübeck (Eutin, 19.000) sind
  eigene kleine Wahlkreise, und die beiden übrigen Oldenburger sind entsprechend groß. Dazu kommen
  Remscheid und Kaltennordheim mit dem 0,65-Fachen.
- **Preußische Zweite Kammer:** 337 Wahlkreise mit 350 Sitzen, rund 46.000 je Sitz (31.000 bis
  69.000, alle beim 0,69- bis 1,49-Fachen des Mittels der Region). Kein Wahlkreis reicht weiter
  als 68 km vom Mittelpunkt.
- **Weit gestreckt** sind in der Nationalversammlung nur Braunschweig mit Bodenwerder (116 km),
  Hamburg mit Cuxhaven (91 km) und Neumarkt in der Steiermark (88 km).
- **Zusammenhang:** Den prüft das Werkzeug nicht (es gibt keine Nachbarschaft der Orte). Ein
  Maß dafür ist die Ausdehnung, die der Bericht nennt.
- Die Tabelle ist reproduzierbar, die CI prüft sie mit `--pruefen`. Ausnahmen per Hand wären eine
  spätere Erweiterung des Werkzeugs.
- Für Preußen 1849 gelten eigene Wahlkreise. 1849 waren es Wahlbezirke mit zwei bis drei
  Abgeordneten; das Modell nimmt meist einen, Großstädte mehrere.

**Boykott (Entscheidung 3):** Wahlkreise mit Mehrheit einer Kultur aus `boykott` wählen nicht, ihre
Sitze bleiben leer. Mit `boykott_maerkte` gilt das nur in diesen Märkten. Für die Nationalversammlung
sind das die `tschechen` in Böhmen (Absage Palackýs). In Mähren wählten die meisten tschechischen
Wahlkreise, deshalb gilt der Boykott dort nicht. Polen und Dänen wählen im Modell. Historisch lagen die
polnischen Teile Posens außerhalb des Bundesgebiets; das Modell gibt Posen deshalb zu viele Sitze. Der
Boykott lässt 62 Sitze leer, also 587 besetzte, nahe den rund 585 der Literatur.

## 6. Gesetze und Verfassungen in M8

M8 baut **kein Gesetzgebungsverfahren**. Die Felder `verfassung`, `wahlrecht` und `wahlsystem`
gelten ab ihrem Datum (Abschnitt 4), und die Staatsinfo zeigt sie. Erlass durch den Fürsten,
Landtagsbeschluss und Zugeständnisse („Märzforderungen“) folgen mit den Ereignissen in M11. Dort
kann der Spieler das Wahlrecht ändern; M8 stellt nur sicher, dass die Wahl mit jeder der vier
Optionen rechnet (Tests mit der Fixture).

## 7. Die Wahl

Am Wahltag eines Parlaments (ein Datum, Entscheidung 4) rechnet der Kern die ganze Wahl in einem
Schritt, deterministisch, mit dem neuen Zufallsstrom `Wahlen = 7`. Rechnung je Wahlkreis über
seine Blöcke, in fester Reihenfolge; parallel je Wahlkreis wie die Orte, das Ergebnis hängt nicht
von der Zahl der Threads ab.

### 7.1 Wahlberechtigte

Die Blöcke haben kein Geschlecht. Männer zählen als die Hälfte der Erwachsenen (Spielwert 0,5).
Je Altersgruppe:
- ab 25: 30–64 und 65+ ganz, 15–29 zu einem Drittel (Spielwert);
- ab 24 (Dreiklassen): 15–29 zu 40 %.

„Selbstständig“ ist ein Anteil je Beruf (Regel `selbststaendig` in `berufe.chron`): Bauern,
Bürger, Handwerker und Adel 1, Landarme 0,6 (Gesinde und Tagelöhner zum Teil ausgeschlossen),
Fabrikarbeiter 0,7, Geistlichkeit 1. Ergebnis 1848: rund 80 % der Männer, nahe den 75–85 % der
Literatur.

Beim **Zensus** (Landtage) zählen die Steuern je Kopf und Jahr: Einkommen des Berufs im Ort × 12 ×
Kontribution des Staates. Privilegierte Berufe ohne Kontribution (Adel, Geistlichkeit) wählen als Stand
mit. Ohne Wahlrecht (`keines`, `staendisch`) gibt es keine Berechtigten; ständische Wahlen kommen später.

### 7.2 Wahlbeteiligung

```
beteiligung = basis (0,55) + 0,25 × alphabetisierung + 0,10 × stadt − 0,15 × (1 − lebensstandard/100)
```

Grenzen 0,15 bis 0,9; Spielwerte, kalibriert auf die Beteiligung der Literatur (in Württemberg
etwa drei Viertel, in Holstein unter der Hälfte). Der Wahlkampf ändert die Beteiligung nicht: Er
vervielfacht die Stimmen der eigenen Strömung im Wahlkreis (Abschnitt 9.1). Wähler und Beteiligung im
Wahllabor und in der Ortsinfo zeigen ihn deshalb nicht.

### 7.3 Neigung je Block

Die Neigung eines Blocks auf die fünf Strömungen ist eine gewichtete Summe, danach auf 1 normiert:

| Anteil | Herkunft | Beispiel |
|---|---|---|
| Interessengruppen | Mitgliedschaften des Blocks × Neigung der Gruppe (3.3) | Bauer, katholisch: Bauernschaft und katholische Kirche |
| Bildung | Alphabetisierung verschiebt von `konservative` zu `liberale` und `demokraten` | — |
| Not | Lebensstandard unter 40 stärkt `demokraten` und `arbeiter` (Pauperismus) | — |
| Stadt und Land | Stadt: `liberale` und `demokraten`, Land: `konservative` und `katholiken` | — |
| Region | Faktor je Staat oder Landschaft (Daten) | Baden und Pfalz `demokraten`, Rheinland `katholiken`, Ostelbien `konservative`, Sachsen `demokraten` |

Kleines Rauschen je Wahlkreis (±5 % je Strömung aus dem Wahlstrom) hält das Ergebnis unvorhersehbar,
im Wahllabor über die Seeds sichtbar.

### 7.4 Stimmen und Gewichte

Stimmen je Strömung = Σ Berechtigte × Beteiligung × Neigung, je Block.

Je nach Wahlrecht kommen Gewichte hinzu:
- **Dreiklassen:** Die Wähler eines Ortes werden nach Steuerleistung sortiert und in drei Klassen mit
  je einem Drittel der Steuern geteilt. Jede besetzte Klasse zählt gleich viel.
  - Der Ort steht für den Urwahlbezirk.
  - Die Steuerleistung ist das Einkommen je Kopf des Berufs im Ort; innerhalb eines Staates ordnet
    es wie die Steuern.
  - Ein Block, der über eine Klassengrenze reicht, zählt anteilig in beiden Klassen.
  - Die Stimmen eines Ortes ergeben weiter seine Wähler; nur ihr Gewicht verschiebt sich zu den
    Reichen.
- **Indirekte Wahl:** Die Wahlmänner verschieben zu den Honoratioren, also Faktor
  `wahlmaenner = { liberale = 1.1 katholiken = 1.1 }` (Spielwert). Die Literatur nennt die
  Überrepräsentation des Bildungsbürgertums; unter den rund 600 Abgeordneten war kein Arbeiter.

### 7.5 Mehrheit und Stichwahl

**Boykott:** Stellt eine Kultur aus `boykott` mehr als die Hälfte der Menschen eines Wahlkreises, wählt
er nicht. Die Berechtigten bleiben stehen, Wähler und Stimmen sind 0, der Sitz bleibt leer.

Bei `stichwahl` gewinnt eine Strömung mit absoluter Mehrheit. Sonst kommen die beiden
stärksten in die Stichwahl. Die Stimmen der übrigen gehen anteilig nach der Nähe an beide: an den
einen Finalisten der Abstand zum anderen geteilt durch beide Abstände. Die Liberalen (10) geben zwischen
Demokraten (−50) und Katholiken (35) also 60 ÷ 85, gut sieben Zehntel, an die Katholiken und den Rest an
die Demokraten.
Gleichstand entscheidet die Position (die gemäßigtere gewinnt), nicht der Zufall. Bei
`relativ` gewinnt die stärkste.

### 7.6 Abgeordnete und Fraktionen

Der Abgeordnete eines Wahlkreises bekommt die Position seiner Strömung, verschoben zur Mitte des
Wahlkreises (die mit den Stimmen gewichtete Position, Faktor 0,3), dazu Rauschen (±18). Daraus
folgt die Fraktion nach Abschnitt 3.2.

Ein Wahlkreis mit mehreren Sitzen (Berlin, Wien, Hamburg) gibt alle Sitze an die siegreiche Strömung.
Jeder Abgeordnete zieht aber eigene Position und Fraktion.

Ein Anteil bleibt fraktionslos (Spielwert 25 %, je Abgeordneter gezogen). Die Fraktionen entstehen
mit ihrem Datum: Die Wahl im Mai kennt nur ihre Positionen. Die Zuordnung gilt ab dem
**Fraktionsstichtag** des Parlaments (Oktober 1848 wie die Literaturwerte); die Godot-Anzeige
zeigt vorher die Strömungen.

### 7.7 Ergebnis und Zustand

Je Wahl und Wahlkreis bleiben gespeichert:
- Berechtigte, Wähler, Stimmen je Strömung;
- Sieger, Position und Fraktion des Abgeordneten;
- Meldereihenfolge (für den Wahlabend, Abschnitt 8).

Je Parlament kommen die Sitze je Fraktion und Strömung hinzu.

**Zufall:** Jeder Wahlkreis hat einen eigenen Generator aus Seed, Parlament und Wahlkreis (Strom
`Wahlen`). Er zieht in fester Folge: die Meldereihenfolge, das Rauschen je Strömung, dann je Sitz
Position und Fraktionslosigkeit. Deshalb rechnen die Wahlkreise parallel, und das Ergebnis hängt nicht
von der Zahl der Threads ab.

Das Ergebnis ist **Zustand**: Es steht im Zustandsverzeichnis (Prüfsumme, Spielstand, Textexport)
und in der Chronik („Wahltag – Deutsche Nationalversammlung: 587 von 649 Sitzen besetzt, 62 leer
(Boykott); stärkste Strömung: Konstitutionelle Liberale mit 293 Sitzen.“). Die Fraktionen gibt es am
Wahltag noch nicht, deshalb nennt die Chronik die Strömung. In M8 wirkt es auf nichts zurück: Wirtschaft und Bevölkerung rechnen wie bisher.

**1648 bleibt bitgleich** (Entscheidung 8): Der neue Zustand wird nur besucht, wenn die Welt
Parlamente hat. Das Szenario 1648 hat keine; die Prüfsumme nach 100 Jahren bleibt
`5db13a66d63e6079`. Der Spielstand bekommt **Format 2** ([speicherstaende.md](speicherstaende.md),
Abschnitt 6); einen Versionsschalter für ältere Spielstände gibt es noch nicht.

### 7.8 Zielwerte und Wahllabor

`uhrwerk-cli wahl --szenario voelkerfruehling [--parlament ID] [--seeds N] [--seed N] [--ziele DATEI]
[--wahlkreise]` rechnet je Seed vom Start bis zum Wahltag. Standard ist das erste Parlament des
Szenarios mit 16 Seeds. Die Ausgabe enthält:
- besetzte Sitze und die Beteiligung (Median der Wahlkreise);
- Sitze je Strömung und Fraktion mit Median sowie 5- und 95-%-Wert über die Seeds;
- die Blöcke der Wahlziele mit ihrem Anteil;
- je Staat Sitze, Berechtigte, Beteiligung und Sitze je Strömung;
- mit `--wahlkreise` das Ergebnis je Wahlkreis im ersten Lauf.

Die Zielwerte stehen in `common/wahlziele_voelkerfruehling.chron` (chronik-skript.md, Abschnitt 3.24).
Exitcode 1 bei Verstoß. Der Befehl ist ein Schritt im CI-Job „Balancing“ (16 Seeds); CTests rechnen den
Landtag der Testdaten mit drei und die Nationalversammlung mit zwei Seeds. Im Kern: `uhrwerk/election_lab.hpp`.

Ziele für die Nationalversammlung, Anteil an den besetzten Sitzen, Stand Oktober 1848
(ungeprüfte Richtwerte aus der Literaturkenntnis und der Websuche, Fachprüfung offen):

| Block | Fraktionen | Literatur (gerundet) | Zielkorridor |
|---|---|---|---|
| Rechte | Café Milani | rund 40 Abgeordnete | 4–10 % |
| Rechtes Zentrum | Casino, Landsberg | rund 160 | 22–34 % |
| Linkes Zentrum | Augsburger Hof, Württemberger Hof | rund 100–140 | 13–25 % |
| Linke | Westendhall, Deutscher Hof, Donnersberg | rund 140 | 15–27 % |
| Fraktionslos | — | ein Viertel bis ein Drittel | 20–35 % |
| Besetzte Sitze | — | rund 585 von 649 | 560–610 |
| Beteiligung | — | regional 40–75 % | Median 45–70 % |

Dazu kommt eine Plausibilitätsprüfung für Preußen 1849 (Dreiklassen): `konservative` und
`liberale` zusammen über 60 % der Sitze, `arbeiter` unter 3 %.

## 8. Wahlabend (Godot)

Der Kern rechnet am Wahltag das ganze Ergebnis. Der **Wahlabend ist eine Inszenierung** dieses
Ergebnisses (Entscheidung 4): Die Simulation hält an, und der Bildschirm lässt die Wahlkreise
nacheinander „eintreffen“, in der Meldereihenfolge. Sie ergibt sich aus dem Abstand vom Tagungsort
(`sitz` des Parlaments) zum ersten Ort des Wahlkreises plus 0 bis 150 km Zufall. Sie steht im Zustand,
damit jede Wiederholung gleich aussieht. Stil 1848 nach Kapitel 10,
Abschnitt 6: Telegramme und Extrablatt, Fraktur als Zierschrift, Antiqua für alle Zahlen.

| Bereich | Inhalt |
|---|---|
| Kopf | „Die Wahlen zur Deutschen Nationalversammlung“, Datum, „412 von 649 Wahlkreisen gemeldet“ |
| Hochrechnung | Balken je Strömung, ab dem Fraktionsstichtag je Fraktion: gemeldete Sitze, dazu Hochrechnung (die offenen Wahlkreise je Staat nach den Anteilen der gemeldeten desselben Staates) mit Fehlerband |
| Halbrund | Sitzordnung der Paulskirche, links nach rechts nach Position, gefüllt in der Meldereihenfolge |
| Telegramme | letzte Meldungen: „Wahlkreis Mannheim: Demokraten, 61 %“ |
| Karte | Kartenmodus „Wahlen“: gemeldete Wahlkreise in der Farbe der Siegerströmung, offene schraffiert |
| Steuerung | Tempo (×1, ×4, sofort), Ergebnis je Wahlkreis (Klick auf die Karte oder die Liste), Schließen |

Der Bildschirm öffnet sich am Wahltag von selbst und ist danach aus dem Spielmenü erreichbar
(„Letzte Wahl“).

**Umsetzung (Schritt b1):**
- **Panel:** Der Wahlabend (`game/scripts/ui/election_night.gd`) liegt als Panel über der linken
  Spalte, damit die Karte daneben sichtbar bleibt. Die Zeit läuft höchstens bis einschließlich zum
  Wahltag und hält dann an.
- **Meldungen:**
  - Tempo ×1 meldet 10 Wahlkreise je Sekunde, ×4 meldet 40, „Sofort“ alle.
  - Noch nicht gemeldete Wahlkreise sind auf der Karte schraffiert.
  - Ein Klick auf ein Telegramm wählt den Wahlkreis in der Ortsinfo.
- **Hochrechnung** (`game/scripts/core/election_tally.gd`):
  - Die offenen Sitze eines Staates gehen nach den Anteilen der gemeldeten Sitze desselben Staates,
    ohne Meldung dort nach allen gemeldeten.
  - Das Fehlerband ist ±1,96 σ, binomial je offenem Sitz.
  - Am Ende ist die Hochrechnung gleich dem Ergebnis.
- **Halbrund:** Es zeigt die besetzten Sitze von links nach rechts nach der Position der
  Abgeordneten. Leere Sitze (Boykott) nennt der Kopf.
- **Fraktionen:** Vor dem Fraktionsstichtag zeigen Balken und Halbrund die Strömungen, danach die
  Fraktionen. Der **Kartenmodus „Wahlen“** (Taste Ä, frei neben den Kameratasten) zeigt danach die Wahlkreise mit der Farbe
des Siegers und einer Helligkeit nach seinem Anteil. Die Ortsinfo nennt Wahlkreis, Sieger,
Stimmen je Strömung und die Beteiligung.

**Nach dem Review (Schritt b3):**
- Solange der Wahlabend läuft, zeigen Ortsinfo, Tooltip und Legende für noch nicht gemeldete
  Wahlkreise kein Ergebnis („noch nicht gemeldet“); die Sitze stehen erst am Ende in der Legende.
- Mit acht Fraktionen und den Fraktionslosen scrollen die Telegramme; die Tempozeile bleibt im Bild.
  Die stärkste Fraktion im Kopf ist nie „fraktionslos“.
- Telegramme und Balken nennen die Strömungen mit Kurznamen (`kurzname`), damit der Anteil sichtbar
  bleibt; die Telegramm-Knöpfe bleiben beim Melden dieselben (kein verlorener Klick).
- Ein langes Bild meldet nicht auf einen Schlag (höchstens 0,05 s je Bild zählen).
- „Letzte Wahl“ aus dem Spielmenü setzt beim Schließen das Tempo von vor dem Menü fort.
- Ein Screenshot über einen Wahltag öffnet den Wahlabend nur mit `--wahlabend`.

## 9. Parteienmodus

**Vorschlag (Entscheidung 5):** Statt eines Staates wählt man beim neuen Spiel eine
**Strömung**. Ab dem Fraktionsstichtag spielt man die Fraktion, in der die meisten eigenen
Abgeordneten sitzen.

Bis zur Wahl führt man **Wahlkampf**:
- **Mittel:** monatlich aus Spenden, nach dem Einfluss der Interessengruppen, die der Strömung
  zuneigen.
- **Befehl `Wahlkampf(strömung, wahlkreis, art)`**, je Art mit Kosten und Wirkung:
  - Versammlung: Mobilisierung +;
  - Flugblätter: Neigung + bei den Alphabetisierten;
  - Verein gründen: bleibende kleine Mobilisierung.
  - Die Wirkung bleibt bis zum Wahltag, mit abnehmendem Ertrag je Wahlkreis.
- **Anzeige:** Stimmungsberichte je Wahlkreis (Schätzung mit großem Fehler, wie Kapitel 10,
  Abschnitt 5 für die Zeit vor 1945).

Nach der Wahl zeigt die Fraktionsansicht Sitze und Abgeordnete. Abstimmungen in der Paulskirche
folgen mit M11. Die anderen Strömungen führen keinen Wahlkampf, bis M12 eine KI bringt (bekannte
Grenze).

Der Befehl ist ein gewöhnlicher Steuerbefehl der Simulation: im Befehlsprotokoll, wiederholbar,
mit Wirkung erst am nächsten Tag.

### 9.1 Umsetzung (Schritt b2)

**Kern** (`core/src/politics.cpp`):
- **Zustand:** je Wahlkreis und Strömung `wahlkampf` mit Mobilisierung, Flugblättern und Vereinen;
  je Strömung `parteikasse` und `spenden` (Zustandsverzeichnis, nur mit Parlamenten).
- **Spenden** am Monatsersten: Σ über alle Orte in einem Wahlkreis und alle Blöcke von
  Einkommen × Größe × Neigung zur Strömung (Abschnitt 7.3, mit Interessengruppen und Regionen),
  mal `spenden` aus `wahlregeln`. Bis zur Wahl am 1. Mai 1848 kommen zwei Zahlungen an: Liberale
  rund 7.800 Taler, Demokraten 7.500, Konservative 6.100, Katholiken 3.300, Arbeiterbewegung 230.
- **Befehl** `Wahlkampf` (Strömung, Wahlkreis, Art), erlaubt bis zum Wahltag des Parlaments und
  solange die Kasse die Kosten deckt. Jede Ausführung kostet `kosten` und hebt die Stufe um
  `wirkung × (1 − Stufe / hoechstens)`. Die Kosten sinken erst bei der Ausführung am nächsten Tag
  von der Kasse; bis dahin zählen eingereihte Befehle als verplant (`reserved_campaign_funds`,
  Review in b3), sodass die Brücke keinen Befehl annimmt, den die Kasse dann nicht deckt.
- **Wirkung** in der Wahl (Abschnitt 7.4):
  - Versammlung und Verein: Stimmen der Strömung im Wahlkreis × (1 + Mobilisierung + Verein);
  - Flugblätter: Neigung + Stufe × Alphabetisierung des Blocks, vor der Normierung.
- **Nach der Wahl** fallen Mobilisierung und Flugblätter auf null; Vereine bleiben.
- **Spielwerte** (`common/wahlen.chron`, Block `wahlkampf`, chronik-skript.md 3.23):
  | Art | Kosten | Wirkung | höchstens |
  |---|---|---|---|
  | Versammlung | 400 Taler | 0,05 | 0,25 |
  | Flugblätter | 250 Taler | 0,3 | 1 |
  | Verein gründen | 1.500 Taler | 0,03 | 0,06 |

  `spenden` ist 0,0003, `stimmungsfehler` 8 Punkte.
- **Wirkung gemessen:** Mannheim, Stand 30. April 1848, alle Mittel der Liberalen in diesem einen
  Wahlkreis (Wahlkreise nach dem Review in b3). Vorher Demokraten 50 %, Liberale 28 %.
  - 31 Flugblätter: Liberale 38 %, Demokraten 42 %;
  - 19 Versammlungen: Liberale 32 %, Demokraten 47 %.

  Wahlkampf kippt also knappe Wahlkreise, aber keine Hochburgen.

**Stimmungsbericht** (Brücke `get_campaign`): Schätzung der Anteile mit derselben Rechnung wie die
Wahl, aber ohne Zufall. Dazu kommt ein Anzeigefehler je Wahlkreis, Strömung und Monat, gleichverteilt
in ± `stimmungsfehler` Punkten. Danach wird wieder auf 100 % normiert. Der Fehler ist reine Anzeige
und berührt weder die Simulation noch einen Zufallsstrom.

**Welche Wahl:** Der Parteienmodus gilt immer der **nächsten ausstehenden Wahl** (frühester Wahltag
ohne Ergebnis). 1848 ist das bis zum 1. Mai die Nationalversammlung, danach die Preußische Zweite
Kammer vom 17. Juli 1849 (Review in b3; vorher blieb der Wahlkampf nach dem 1. Mai unerreichbar).
Die Brücke liefert dafür je Ort `campaign_district`.

**Godot:**
- Rolle im Spielmenü („Staat: …“ oder „Partei: …“, nur in Szenarien mit Parlament) oder
  Startoption `--partei=<strömung>`.
- Im Parteienmodus steht das Parteipanel an der Stelle des Staatspanels:
  - Mittel, verplante Mittel, Spenden;
  - die nächste Wahl mit Tag und der Zahl der Wahlkreise mit Wahlkampf;
  - nach einer Wahl deren Sitze, ab dem Fraktionsstichtag die Fraktion mit den meisten eigenen
    Abgeordneten und die Liste der Abgeordneten (beides zugleich, wenn nach einer Wahl die nächste
    aussteht);
  - „Staat spielen“ führt zurück.
- In der Ortsinfo rückt der Abschnitt „Wahl“ nach oben. Darunter steht der Abschnitt „Wahlkampf“
  zur nächsten Wahl: Parlament, Wahlkreis, Stimmungsbericht, Stand des eigenen Wahlkampfs und je
  ein Knopf je Art. Der Knopf ist gesperrt, wenn die Mittel nach den verplanten nicht reichen; er
  bleibt bei laufender Zeit derselbe, damit kein Klick verloren geht.
- Die Rolle steht im Spielstand (Metadaten `spielerstroemung`, speicherstaende.md).

## 10. Kern, Daten, Leistung

- **Lader:**
  - neue Dateien `common/stroemungen.chron`, `common/interessengruppen.chron`,
    `common/parlamente.chron` und `common/wahlen.chron`, Ordner `history/wahlkreise/`;
  - neue Felder in den Staaten (Abschnitt 4) und in den Berufen (`selbststaendig`);
  - alles im Inhaltsfingerabdruck (neuer Bereich `politik`);
  - späterer Inhalt mit `seit` wie in M7.
- **Archivar:** `wahlkreise.csv`; neue Felder in `staaten_historie.csv`; Prüfung, dass jeder Ort
  mit Daten im Bundesgebiet einen Wahlkreis hat.
- **Kern:**
  - `uhrwerk/politics.hpp`: `PoliticalCurrent`, `InterestGroup`, `Parliament`, `Faction`,
    `ElectoralDistrict`, `ElectionResult`;
  - die Wahl in `core/src/politics.cpp`, aufgerufen im Tagesablauf am Wahltag nach der Wirtschaft.
- **Leistung:**
  - Eine Wahl rechnet rund 100.000 Blöcke einmal: unter 0,1 s im Release-Build.
  - Der Wahlkampf ist ein Feld je Wahlkreis und Strömung.
  - Kein Einfluss auf das Spieljahr ohne Wahl.
- **Brücke:**
  - `get_parliaments`, `get_election(index)` (Wahlkreise, Ergebnis, Meldereihenfolge),
    `get_location_electoral_district_indices`, `get_currents`, `get_state_politics(state)`;
  - Kartenmodus 12 `MAP_WAHLEN`;
  - Parteienmodus `campaign`, `get_party`, `get_campaign` (Abschnitt 9.1).

## 11. Tests und Abnahme

| Kriterium | Prüfung |
|---|---|
| Eine Wahl zur Nationalversammlung 1848 ergibt eine historisch plausible Sitzverteilung (Abweichung dokumentiert) | `wahl --seeds 16` gegen `wahlziele_voelkerfruehling.chron` in der CI; Abweichungen je Block in dieser Datei, Abschnitt 14.1 |
| Wahlabend mit Hochrechnung und Ergebnis je Wahlkreis | Godot-Rauchtest (Wahlabend öffnet am Wahltag, alle Wahlkreise gemeldet, Hochrechnung am Ende = Ergebnis), Klicktest auf einen Wahlkreis, Screenshot in der CI |

Weitere Tests:
- **Einheitentests** mit der Fixture für jedes Wahlrecht und System:
  - Stichwahl;
  - Gleichstand;
  - Dreiklassen mit bekanntem Ergebnis;
  - Boykott;
  - Fraktionen ab dem Stichtag (`fraktionen_ab`);
  - Rundung der Sitzverteilung.
- **Determinismus:** gleiche Wahl mit 1 und 4 Threads.
- **Spielstände:** Speichern vor und nach dem Wahltag, Laden, gleiche Prüfsumme; Format 2 mit
  Formatwache.
- **Wiederholung:** mit Wahlkampfbefehlen.
- **1648:** Prüfsumme unverändert.

## 12. Offene Entscheidungen

1. **Welche Wahlen.** Empfehlung: **Nationalversammlung 1848** (kalibriert) und **Preußische
   Zweite Kammer Juli 1849** mit Dreiklassenwahlrecht (Plausibilität). Alternative: nur die
   Nationalversammlung; oder zusätzlich die Preußische Nationalversammlung 1848 und die Landtage.
2. **Sitze und Wahlkreise.** Empfehlung: **649 Sitze im Verhältnis zur Bevölkerung im
   Bundesgebiet**, Wahlkreise per Werkzeug aus den Kreisen (Abschnitt 5). Alternativen:
   - historische Sitze je Staat aus Literatur ohne Quellenzugang;
   - ein Sitz je 50.000 Einwohner der heutigen Daten (über 800 Sitze).
3. **Tschechischer Boykott.** Empfehlung: **ja**, Wahlkreise mit tschechischer Mehrheit bleiben leer
   (historisch der größte Teil der rund 64 fehlenden Sitze). Alternative: alle wählen.
4. **Wahlabend.** Empfehlung: **ein Wahltag**, das Ergebnis wird am Wahlabend als Folge von
   Meldungen inszeniert (Abschnitt 8). Alternative: Meldungen über Spieltage verteilt (die Wahlen
   zogen sich historisch von Mitte April bis in den Mai); das braucht Wahltage je Staat und eine
   Hochrechnung im Kern.
5. **Parteienmodus.** Empfehlung: **minimal**, also Strömung wählen und Wahlkampf mit Mitteln bis
   zur Wahl, danach Fraktionsansicht (Abschnitt 9). Alternative: auf M11 verschieben, wenn es
   Abstimmungen gibt; dann erfüllt M8 diesen Umfangspunkt nicht.
6. **Interessengruppen.** Empfehlung: **statisch**, also Mitglieder, Anteil und Einfluss als
   Grundlage der Wahlneigung, angezeigt in der Staatsinfo. Zustimmung und Aktionen kommen mit M11
   und M12. Alternative: Zustimmung schon in M8, ohne dass sie etwas bewirkt.
7. **Gesetze.** Empfehlung: **Verfassung, Wahlrecht und Wahlsystem als datierte Felder**, kein
   Gesetzgebungsverfahren (Abschnitt 6). Alternative: der Spieler ändert das Wahlrecht seines
   Staates per Erlass (ohne Folgen außer bei der nächsten Wahl).
8. **Prüfsumme 1648.** Empfehlung: **bitgleich**, weil der neue Zustand nur mit Parlamenten
   besucht wird; Spielstand Format 2. Alternative: neue Referenzprüfsumme.
9. **Teilung M8a/M8b.** Empfehlung: ja (Abschnitt 1).
10. **Quellen und Fachprüfung.** Empfehlung: Richtwerte aus der Literaturkenntnis, geprüft gegen die
    Auszüge der Websuche, alle `ungeprueft`; die Fachprüfung schließt sich an die von M7 an.
    Alternative: Zielwerte erst nach einer Prüfung mit Quellen festlegen (dann wartet die Abnahme).

## 13. Umsetzungsschritte

| Schritt | Inhalt | Prüfung |
|---|---|---|
| a1 | **Datenmodell:** Strömungen, Interessengruppen, Parlamente und Fraktionen, Felder `verfassung`, `wahlrecht`, `wahlsystem`, `selbststaendig`; Lader, Archivar, Fingerabdruck | Unit-Tests, `pruefen` für beide Szenarien |
| a2 | **Wahlkreise:** Werkzeug, `wahlkreise.csv`, Archivar, Lader; Stichprobe | Tests des Werkzeugs, Einwohnerspanne, Zusammenhang |
| a3 | **Wahlsimulation** im Kern, Zufallsstrom `Wahlen`, Zustand, Chronik, Spielstand Format 2 | Unit-Tests, Determinismus, 1648 bitgleich |
| a4 | **Wahllabor:** `uhrwerk-cli wahl`, Zielwerte, Kalibrierung 1848, CI | Zielwerte erfüllt |
| b1 | **Brücke und Godot:** Kartenmodus „Wahlen“, Wahlabend, Ortsinfo, Staatsinfo (Verfassung, Wahlrecht, Interessengruppen) | Godot-Tests, Screenshots |
| b2 | **Parteienmodus** (Wahlkampf, Mittel, Stimmungsberichte) und **Preußen 1849** (Dreiklassen) | Unit-Tests, Wiederholung mit Wahlkampfbefehlen, Plausibilität 1849 |
| b3 | **Doku und Review** (Abschluss M8) | CI grün |

## 14. Umsetzungsstand

| Schritt | Stand |
|---|---|
| a1 Datenmodell | `common/stroemungen.chron` (5 Strömungen), `common/interessengruppen.chron` (11 Gruppen, davon 10 im Lesezeichen 1848; die Arbeiterbewegung erst ab August 1848), `common/parlamente.chron` (Nationalversammlung 1848 mit 8 Fraktionen, Preußische Zweite Kammer 1849 mit 3), `selbststaendig` bei Landarmen (0,6) und Fabrikarbeitern (0,7); Verfassung und Wahlrecht der Landtage für die 38 Staaten von 1848 in `staaten_historie.csv` (aus der Regierungsform abgeleitet, Zensus in Baden, Württemberg und Hessen-Darmstadt; nach dem Review in b3 ständisch in den Niederlanden, Dänemark, Liechtenstein und Schaumburg-Lippe). Lader (chronik-skript.md 3.19–3.21), Archivar, Inhaltsfingerabdruck (Bereich `politik`), `info` und `pruefen`. 1648 ohne Parlament, Prüfsumme unverändert. |
| a2 Wahlkreise | `tools/geodaten/wahlkreise_1848.py` (mit Tests, CI prüft die Tabelle), `wahlkreise.csv` mit 633 Wahlkreisen der Nationalversammlung (649 Sitze) und 337 der Preußischen Zweiten Kammer (350 Sitze), Archivar (`history/wahlkreise/`), Lader mit Prüfungen (Sitze = Sollzahl, Orte des Staates, jeder Ort einmal), Fingerabdruck, `info`. Boykott um Polen und Dänen ergänzt. |
| a3 Wahl im Kern | `core/src/politics.cpp` (Abschnitte 7.1–7.7). Die Wahl läuft im Tagesablauf am Wahltag nach der Wirtschaft, mit Chronikeintrag. `common/wahlen.chron` (chronik-skript.md 3.23) enthält Wahlregeln und fünf Regionen als Spielwerte (nach der Kalibrierung in a4 vierzehn); Sitz der Parlamente ist Frankfurt bzw. Berlin. Ergebnis im Zustandsverzeichnis nur mit Parlamenten, Spielstand Format 2 (speicherstaende.md, Abschnitt 6). Tests: Neigung, Fraktionen, Stichwahl, Gleichstand, Boykott, Dreiklassen, Zensus, Threads, Spielstände, Formatwache. 1648 bitgleich. Erster Lauf 1848 ohne Kalibrierung: 504 von 649 Sitzen besetzt (145 leer, davon Tschechen 89, Polen 51, Dänen 5), die Liberalen gewinnen 369 Sitze, Konservative und Demokraten keinen. Die Kalibrierung folgt in a4. |
| a4 Wahllabor und Kalibrierung | `uhrwerk-cli wahl` (Abschnitt 7.8, Kern `uhrwerk/election_lab.hpp`), Wahlziele `common/wahlziele_voelkerfruehling.chron` (chronik-skript.md 3.24), CI-Schritt „Wahllabor 1848/49“, CTests `wahl_fixture` und `wahl_1848`. Ergebnis 1848 mit 16 Seeds: alle Ziele erfüllt (siehe unten). Preußen 1849: Konservative und Liberale 83 %, Arbeiterbewegung 0 %. |
| b1 Wahlabend und Kartenmodus | Brücke: `get_currents`, `get_parliaments`, `get_election`, `get_location_electoral_district_indices`, `get_shown_parliament`, Kartenmodus 12 `MAP_WAHLEN`, Wahlkreis und Ergebnis in `get_location` (`wahl`). Godot: Kartenmodus „Wahlen“ (Taste Ä) mit Wahlkreisgrenzen, Legende, Tooltip und Abschnitt „Wahl“ in der Ortsinfo; Wahlabend (Abschnitt 8) mit Spielmenü „Letzte Wahl“ und Startoption `--wahlabend`. Tests: Einheitentests (Hochrechnung, Halbrund, Telegramme), Rauchtest der Brücke 1848, Klicktest (Wahlabend am 1. Mai, schrittweise Meldungen, Hochrechnung am Ende = Ergebnis, Telegramm zeigt die Ortsinfo), Screenshot `einigkeit_wahlabend.png` in der CI. |
| b2 Parteienmodus | Kern: Spenden, Befehl `Wahlkampf` mit drei Arten, Wirkung in der Wahl, Ende mit der Wahl (Abschnitt 9.1); `common/wahlen.chron` mit Block `wahlkampf`; Befehle im Spielstand mit Ziel und Art (speicherstaende.md). Brücke: `campaign`, `get_party`, `get_campaign` (Stimmungsbericht), `set_player_current`, `get_player_current`, `player_current` in `get_save_info`. Godot: Rolle im Spielmenü, `--partei`, Parteipanel, Wahlkampf in der Ortsinfo. Tests: Kern (Spenden, Kosten, abnehmender Ertrag, Wirkung, Ende mit der Wahl, Wiederholung, Spielstand), Lader, Rauchtest der Brücke, Klicktest (Rolle, Versammlung in Mannheim, zurück zum Staat), Einheitentests (Parteipanel, `--partei`), Screenshot `einigkeit_partei.png` in der CI. `speichertest_1848` mischt Wahlkampfbefehle ein. 1648 bitgleich. Plausibilität Preußen 1849 (Dreiklassenwahl, `uhrwerk-cli wahl --parlament pr_abgeordnetenhaus --seeds 8`): Konservative 66, Katholiken 20, Liberale 226, Demokraten 38 von 350 Sitzen; Konservative und Liberale 83 %, Arbeiterbewegung 0 %, beide Ziele erfüllt. Grenze: Historisch boykottierten die Demokraten die Wahl vom Juli 1849; das Modell kennt keinen Boykott einer Strömung. |
| b3 Abschluss | Staatsinfo (Abschnitt 3.3 und 6): Knopf „Verfassung und Gruppen …“ im Staatspanel mit Verfassung, Wahlrecht und Wahlsystem des Landtags und Anteil und Einfluss jeder Interessengruppe im Staat (Kern `interest_group_standing`, Brücke `get_state_politics`, Startoption `--verfassung`). Review M8 in drei Teilen (Kern; Brücke und Godot; Werkzeuge, Daten und Doku), behoben: Wahlkreise optimal geschnitten (Abschnitt 5.3), Parteienmodus bis zur Wahl 1849 (9.1), verplante Mittel, Wahlkampf-Knöpfe und Telegramme bei laufender Zeit, Wahlabend mit Fraktionen im Bild und ohne Vorabergebnis, Parteienmodus nur mit Parlament, Lader- und Wahllaborprüfungen, Staaten NL, DEN, LIE, SLI ständisch, „Preußische Zweite Kammer“, Kurznamen der Strömungen, Doku nach Daten und Code. Ergebnis 1848 danach siehe 14.1; Preußen 1849 mit 8 Seeds: Konservative 70, Katholiken 18, Liberale 227, Demokraten 34; Konservative und Liberale 85 %. |

### 14.1 Kalibrierung der Nationalversammlung (Schritt a4)

Ergebnis von `uhrwerk-cli wahl --szenario voelkerfruehling --seeds 16`: Median über 16 Seeds, Anteil
an 587 besetzten Sitzen, in Klammern der 5- und 95-%-Wert. Stand nach dem Review in b3 mit den
gleichmäßigeren Wahlkreisen (Abschnitt 5.3); die Kalibrierung aus a4 blieb unverändert, alle Ziele
sind weiter erfüllt.

| Block | Fraktionen | Ergebnis | Ziel |
|---|---|---|---|
| Rechte | Café Milani (28) | 4,8 % (3,6–6,0) | 4–10 % |
| Rechtes Zentrum | Casino (117), Landsberg (63) | 31,5 % (27,8–34,9) | 22–34 % |
| Linkes Zentrum | Augsburger Hof (47), Württemberger Hof (67) | 19,6 % (17,4–22,8) | 13–25 % |
| Linke | Westendhall (23), Deutscher Hof (79), Donnersberg (13) | 19,1 % (17,7–21,5) | 15–27 % |
| Fraktionslos | — (144) | 24,5 % (22,0–28,1) | 20–35 % |
| Besetzte Sitze | — | 587 | 560–610 |
| Beteiligung | — | Median der Wahlkreise 54 % | 45–70 % |

Strömungen: Konservative 70, Katholiken 72, Liberale 293, Demokraten 150, Arbeiterbewegung 0.

Was die Kalibrierung geändert hat:
- **Stichwahl** anteilig nach der Nähe (Abschnitt 7.5). Vorher gingen alle Stimmen der übrigen an den
  näheren Finalisten, und die Demokraten verloren jede Stichwahl gegen die Mitte.
- **Boykott** nur der Tschechen in Böhmen (`boykott_maerkte`, Abschnitt 5.3).
- **Positionen:** Liberale +10 statt +15, damit ihre Abgeordneten sich wie historisch auf das rechte
  und linke Zentrum verteilen; Demokraten −50 statt −45 und `position_rauschen` 18 statt 15, damit
  der Donnersberg nicht leer bleibt.
- **Wahlregeln:**
  - Grundbeteiligung 0,55 statt 0,45, denn die Alphabetisierung der Blöcke liegt im Median bei 16 %;
  - Grundneigung der Nichtmitglieder demokratischer;
  - Wahlmänner nur noch ×1,1;
  - Städte stärker demokratisch.
- **Katholische Kirche:** Sie bindet 40 statt 60 % der Katholiken.
- **Regionen** nach der Wahlgeographie 1848 (`common/wahlen.chron`):
  - demokratisch: Baden und Pfalz, Württemberg, Sachsen, Thüringen, Niederschlesien, Rhein-Main,
    Franken, Wien, Ostpreußen und das Rheinland (dort auch katholisch);
  - konservativ: Brandenburg, Pommern, Westpreußen und Posen;
  - katholisch: Altbayern, Tirol und Vorarlberg;
  - Josephinismus (ganz Österreich): Liberale statt politischer Katholizismus.

Grenzen:
- Mit Mehrheitswahl je Wahlkreis gewinnen Hochburgen geschlossen: Baden 20 von 20 Sitzen an die
  Demokraten, Sachsen 26 von 26. Historisch saßen auch dort Liberale wie Bassermann und Mathy.
- Preußen stellt 67 Konservative, mehr als historisch; die Rechte im Ganzen liegt trotzdem im Ziel.
- Die Wahlkreise tragen den Namen ihres größten Ortes, und die Orte heißen wie heute (Wuppertal,
  Norderstedt, Villingen-Schwenningen). Historische Ortsnamen sind eine Aufgabe der Ortsdaten.
- Preußen 1849: Die Beteiligung liegt im Modell bei 56 %; historisch war sie wohl deutlich niedriger
  (auch wegen des Boykotts der Demokraten). Kein Ziel prüft sie.
- Die Arbeiterbewegung entsteht erst im August 1848 und gewinnt im Mai keinen Sitz.
- Alle Werte sind Spielwerte gegen ungeprüfte Richtwerte; die Fachprüfung mit Quellen steht aus.