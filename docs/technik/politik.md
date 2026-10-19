# Politik, Parteien und Wahlen (Meilenstein M8)

> **Status: in Umsetzung** – Entscheidungen 1–10 wie empfohlen freigegeben; Stand in Abschnitt 14
> ([meilensteine.md](../meilensteine.md#m8--politik-parteien-und-wahlen)). Grundlage sind
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
| `liberale` (konstitutionelle Liberale) | +15 | Bildungs- und Wirtschaftsbürgertum |
| `demokraten` | −45 | Kleinbürgertum, Handwerker, Studenten |
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

Die Fraktionen sind **Daten des Parlaments**: `history/parlamente/nationalversammlung.chron` mit
Name, Farbe, Bereich und `seit` (Landsberg, Augsburger Hof und Westendhall entstanden erst im
Sommer 1848). Im Parteienmodus spielt man eine Fraktion (Abschnitt 9).

### 3.3 Interessengruppen (`common/interessengruppen.chron`)

Elf Interessengruppen, die es 1848 gibt (Kapitel 09, Abschnitt 5.1): Adel und Ritterschaft,
katholische Kirche, evangelische Kirche, Handel und Finanzen, Zünfte und Mittelstand, Bauernschaft,
Beamtenschaft, Militär, Bildungsbürgertum, Arbeiterbewegung (ab 1848) und Nationalbewegung.

Jede Gruppe beschreibt zwei Dinge:
- **Mitglieder:** Anteile je Beruf, Konfession und Bildung, z. B. Bauernschaft = 90 % der Bauern;
  katholische Kirche = 60 % der Katholiken. Ein Block kann mehreren Gruppen angehören.
- **Neigung:** Gewichte auf die Strömungen, z. B. Bildungsbürgertum: liberale 0,5, demokraten
  0,3, konservative 0,1, katholiken 0,1.

Daraus folgen je Staat:
- **Anteil** = Mitglieder ÷ Erwachsene;
- **Einfluss** = Einkommen der Mitglieder ÷ Einkommen aller.

Beides ist abgeleitet, also kein Zustand. Zustimmung, Forderungen und Aktionen der Gruppen kommen
später (Entscheidung 6). In M8 tragen die Gruppen vor allem die Wahlneigung (Abschnitt 7.3).

## 4. Verfassung und Wahlrecht je Staat

Neue datierte Felder je Staat in `staaten_historie.csv`, mit Quelle und Sicherheit:

| Feld | Werte | Beispiel |
|---|---|---|
| `verfassung` | `keine`, `staendisch`, `konstitutionell` | Baden, Bayern, Württemberg 1818/19 `konstitutionell`; Preußen 1848.12.05 (oktroyiert) |
| `wahlrecht` | `keines`, `staendisch`, `zensus`, `dreiklassen`, `allgemein_maenner` | Preußen 1849.05.30 `dreiklassen` |
| `wahlsystem` | `mehrheit_stichwahl`, `mehrheit_relativ`; dazu `indirekt` (ja/nein) | — |

Die Felder gelten **zum Startdatum** (wie alle Staatsfelder) und beschreiben den Landtag. Jede Wahl
trägt ihr eigenes Wahlrecht im Parlament (Abschnitt 5.1), so auch die Dreiklassenwahl in Preußen 1849.
Das Wahlrecht eines **Parlaments** kann das des Staates überstimmen: Für die Nationalversammlung
galt der Bundesbeschluss vom 7. April 1848. Danach durften alle volljährigen, „selbstständigen“
Männer wählen, ohne Zensus, Stand oder Konfession; die Auslegung lag bei den Staaten.

Die Optionen in Zahlen (Spielwerte, Abschnitt 7.1):

| Wahlrecht | Wer wählt | Gewicht |
|---|---|---|
| `allgemein_maenner` | Männer ab 25, „selbstständig“ nach Beruf (Landarme und Fabrikarbeiter zum Teil, je Staat) | gleich |
| `zensus` | Männer ab 25 mit Steuerleistung über einer Schwelle (Feld `zensus_schwelle` je Staat) | gleich |
| `dreiklassen` | Männer ab 24 | drei Klassen nach Steuerleistung, jede wählt ein Drittel der Wahlmänner |
| `staendisch` | Vertreter der Stände | keine Volkswahl (kein Wahlkreis) |

Ein **Gesetzesmodell** mit 36 Gruppen (Kapitel 09, Abschnitt 6) baut M8 nicht. Die Felder sind die
erste Gruppe davon (Entscheidung 7).

## 5. Parlamente und Wahlkreise

### 5.1 Parlamente (`history/parlamente/*.chron`)

```text
parlament nationalversammlung {
    name      = "Deutsche Nationalversammlung"
    ebene     = bund                # bund oder Staatstag (z. B. PRU)
    sitze     = 649                 # Sollzahl
    wahl      = { tag = 1848.05.01  wahlrecht = allgemein_maenner  indirekt = ja }
    fraktion casino { name = "Casino"  farbe = { … }  von = 20  bis = 55 }
    …
    quelle    = "…"
}
```

**Vorschlag (Entscheidung 1):**
- **Deutsche Nationalversammlung** 1848: kalibriert, Abnahme.
- **Preußisches Abgeordnetenhaus** 17. Juli 1849 mit Dreiklassenwahlrecht: nur
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
   Hilbert-Kurve über ihre Lage in Gruppen gleicher Einwohnerzahl geschnitten.
5. **Name** nach dem einwohnerstärksten Ort, Kennung `nv_<staat>_<nr>` bzw. `pr_<nr>`.

Ergebnis:
- **Nationalversammlung:** 633 Wahlkreise mit 649 Sitzen. Einwohner je Sitz im Median rund 70.000
  (Liechtenstein 6.500 als eigener Staat, höchstens 125.000).
- **Preußisches Abgeordnetenhaus:** 337 Wahlkreise mit 350 Sitzen, rund 46.000 je Sitz. Kein
  Wahlkreis reicht weiter als 66 km vom Mittelpunkt.
- **Weit gestreckt** sind nur Kleinstaaten mit Exklaven: Oldenburg mit Eutin (Cloppenburg 232 km)
  und Braunschweig (Bodenwerder 119 km). Ihre Exklaven sind für einen eigenen Sitz zu klein.
- **Zusammenhang:** Den prüft das Werkzeug nicht (es gibt keine Nachbarschaft der Orte). Ein
  Maß dafür ist die Ausdehnung, die der Bericht nennt.
- Die Tabelle ist reproduzierbar, die CI prüft sie mit `--pruefen`. Ausnahmen per Hand wären eine
  spätere Erweiterung des Werkzeugs.
- Für Preußen 1849 gelten eigene Wahlkreise. 1849 waren es Wahlbezirke mit zwei bis drei
  Abgeordneten; das Modell nimmt meist einen, Großstädte mehrere.

**Boykott (Entscheidung 3):** Wahlkreise mit Mehrheit einer Kultur aus `boykott` wählen nicht, ihre
Sitze bleiben leer. Für die Nationalversammlung sind das `tschechen` (Palacký), `polen` (Posen) und
`daenen` (Nordschleswig).

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

### 7.2 Wahlbeteiligung

```
beteiligung = basis (0,45) + 0,25 × alphabetisierung + 0,10 × stadt − 0,15 × (1 − lebensstandard/100)
              + mobilisierung der Strömungen im Wahlkreis (Wahlkampf, Abschnitt 9)
```

Grenzen 0,15 bis 0,9; Spielwerte, kalibriert auf die Beteiligung der Literatur (in Württemberg
etwa drei Viertel, in Holstein unter der Hälfte).

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
- **Dreiklassen:** Die Wähler eines Wahlkreises werden nach Steuerleistung (Steuern je Kopf ihres
  Berufs im Ort) sortiert und in drei Klassen mit je einem Drittel der Steuern geteilt. Jede Klasse
  zählt ein Drittel.
- **Indirekte Wahl:** Die Wahlmänner verschieben zu den Honoratioren, also Faktor
  `wahlmaenner_liberal` (Spielwert 1,15) auf `liberale` und `katholiken`. Die Literatur nennt die
  Überrepräsentation des Bildungsbürgertums; unter den rund 600 Abgeordneten war kein Arbeiter.

### 7.5 Mehrheit und Stichwahl

Bei `mehrheit_stichwahl` gewinnt eine Strömung mit absoluter Mehrheit. Sonst kommen die beiden
stärksten in die Stichwahl, und die Stimmen der übrigen gehen anteilig an die näher stehende.
Gleichstand entscheidet die Position (die gemäßigtere gewinnt), nicht der Zufall. Bei
`mehrheit_relativ` gewinnt die stärkste.

### 7.6 Abgeordnete und Fraktionen

Der Abgeordnete eines Wahlkreises bekommt die Position seiner Strömung, verschoben zur Mitte des
Wahlkreises (die mit den Stimmen gewichtete Position, Faktor 0,3), dazu Rauschen (±15). Daraus
folgt die Fraktion nach Abschnitt 3.2.

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

Das Ergebnis ist **Zustand**: Es steht im Zustandsverzeichnis (Prüfsumme, Spielstand, Textexport)
und in der Chronik („Wahl zur Deutschen Nationalversammlung: 585 Abgeordnete, Casino stärkste
Fraktion“). In M8 wirkt es auf nichts zurück: Wirtschaft und Bevölkerung rechnen wie bisher.

**1648 bleibt bitgleich** (Entscheidung 8): Der neue Zustand wird nur besucht, wenn die Welt
Parlamente hat. Das Szenario 1648 hat keine; die Prüfsumme nach 100 Jahren bleibt
`5db13a66d63e6079`. Der Spielstand bekommt **Format 2** mit dem Versionsschalter aus
[speicherstaende.md](speicherstaende.md), Abschnitt 6.

### 7.8 Zielwerte und Wahllabor

`uhrwerk-cli wahl [--szenario voelkerfruehling] [--parlament nationalversammlung] [--seeds N]
[--wahlkreise]` rechnet bis zum Wahltag und gibt aus:
- Sitze je Strömung und Fraktion mit Median sowie 5- und 95-%-Wert über die Seeds;
- Beteiligung und Berechtigte je Staat;
- mit `--wahlkreise` das Ergebnis je Wahlkreis.

Die Zielwerte stehen in `common/wahlziele_voelkerfruehling.chron` (Format wie `zielwerte*.chron`).
Exitcode 1 bei Verstoß; ein Schritt im CI-Job „Balancing“.

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
nacheinander „eintreffen“, in der Meldereihenfolge (Abstand des Wahlkreises von Frankfurt plus
Zufall, im Zustand gespeichert, damit jede Wiederholung gleich aussieht). Stil 1848 nach Kapitel 10,
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
(„Letzte Wahl“). Der **Kartenmodus „Wahlen“** (Taste Ä, frei neben den Kameratasten) zeigt danach die Wahlkreise mit der Farbe
des Siegers und einer Helligkeit nach seinem Anteil. Die Ortsinfo nennt Wahlkreis, Sieger,
Stimmen je Strömung und die Beteiligung.

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

## 10. Kern, Daten, Leistung

- **Lader:**
  - neue Dateien `common/stroemungen.chron` und `common/interessengruppen.chron`, Ordner
    `history/parlamente/` und `history/wahlkreise/`;
  - neue Felder in den Staaten (Abschnitt 4) und in den Berufen (`selbststaendig`);
  - alles im Inhaltsfingerabdruck (neuer Bereich `politik`);
  - späterer Inhalt mit `seit` wie in M7.
- **Archivar:** `wahlkreise.csv`; neue Felder in `staaten_historie.csv`; Prüfung, dass jeder Ort
  mit Daten im Bundesgebiet einen Wahlkreis hat.
- **Kern:**
  - `uhrwerk/politics.hpp`: `Current`, `InterestGroup`, `Parliament`, `Faction`,
    `ElectoralDistrict`, `ElectionResult`;
  - die Wahl in `core/src/politics.cpp`, aufgerufen im Tagesablauf am Wahltag nach der Wirtschaft.
- **Leistung:**
  - Eine Wahl rechnet rund 100.000 Blöcke einmal: unter 0,1 s im Release-Build.
  - Der Wahlkampf ist ein Feld je Wahlkreis und Strömung.
  - Kein Einfluss auf das Spieljahr ohne Wahl.
- **Brücke:**
  - `get_parliaments`, `get_election(parliament)` (Wahlkreise, Ergebnis, Meldereihenfolge),
    `get_location_district_indices`, `get_currents`, `get_interest_groups(state)`;
  - Kartenmodus 12 `MAP_WAHLEN`;
  - Befehl `campaign(...)`.

## 11. Tests und Abnahme

| Kriterium | Prüfung |
|---|---|
| Eine Wahl zur Nationalversammlung 1848 ergibt eine historisch plausible Sitzverteilung (Abweichung dokumentiert) | `wahl --seeds 16` gegen `wahlziele_voelkerfruehling.chron` in der CI; Abweichungen je Block in dieser Datei, Abschnitt 13 |
| Wahlabend mit Hochrechnung und Ergebnis je Wahlkreis | Godot-Rauchtest (Wahlabend öffnet am Wahltag, alle Wahlkreise gemeldet, Hochrechnung am Ende = Ergebnis), Klicktest auf einen Wahlkreis, Screenshot in der CI |

Weitere Tests:
- **Einheitentests** mit der Fixture für jedes Wahlrecht und System:
  - Stichwahl;
  - Gleichstand;
  - Dreiklassen mit bekanntem Ergebnis;
  - Boykott;
  - Fraktionen mit `seit`;
  - Rundung der Sitzverteilung.
- **Determinismus:** gleiche Wahl mit 1 und 4 Threads.
- **Spielstände:** Speichern vor und nach dem Wahltag, Laden, gleiche Prüfsumme; Format 2 mit
  Formatwache.
- **Wiederholung:** mit Wahlkampfbefehlen.
- **1648:** Prüfsumme unverändert.

## 12. Offene Entscheidungen

1. **Welche Wahlen.** Empfehlung: **Nationalversammlung 1848** (kalibriert) und **Preußisches
   Abgeordnetenhaus Juli 1849** mit Dreiklassenwahlrecht (Plausibilität). Alternative: nur die
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
| a1 Datenmodell | `common/stroemungen.chron` (5 Strömungen), `common/interessengruppen.chron` (11 Gruppen, davon 10 im Lesezeichen 1848; die Arbeiterbewegung erst ab August 1848), `common/parlamente.chron` (Nationalversammlung 1848 mit 8 Fraktionen, Preußisches Abgeordnetenhaus 1849 mit 3), `selbststaendig` bei Landarmen (0,6) und Fabrikarbeitern (0,7); Verfassung und Wahlrecht der Landtage für die 38 Staaten von 1848 in `staaten_historie.csv` (aus der Regierungsform abgeleitet, Zensus in Baden, Württemberg, Hessen-Darmstadt und den Niederlanden). Lader (chronik-skript.md 3.19–3.21), Archivar, Inhaltsfingerabdruck (Bereich `politik`), `info` und `pruefen`. 1648 ohne Parlament, Prüfsumme unverändert. |
| a2 Wahlkreise | `tools/geodaten/wahlkreise_1848.py` (mit Tests, CI prüft die Tabelle), `wahlkreise.csv` mit 633 Wahlkreisen der Nationalversammlung (649 Sitze) und 337 des Preußischen Abgeordnetenhauses (350 Sitze), Archivar (`history/wahlkreise/`), Lader mit Prüfungen (Sitze = Sollzahl, Orte des Staates, jeder Ort einmal), Fingerabdruck, `info`. Boykott um Polen und Dänen ergänzt. |
