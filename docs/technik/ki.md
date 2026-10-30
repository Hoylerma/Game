# Künstliche Intelligenz der Staaten (Meilenstein M12)

> **Status: Entwurf** – Entscheidungen 1–14 in Abschnitt 11 stehen zur Freigabe; Umsetzung erst
> danach. Grundlage sind [21 Künstliche Intelligenz](../21_kuenstliche_intelligenz.md) (Abschnitte
> 1–5, 8–11), [13 Persönlichkeiten](../13_persoenlichkeiten.md) (Abschnitte 1 und 3) und der
> Meilensteinplan ([meilensteine.md](../meilensteine.md#m12--künstliche-intelligenz-der-staaten)).
> Haushalt steht in [wirtschaft.md](wirtschaft.md), Wahlen in [politik.md](politik.md), Diplomatie und
> Bundesversammlung in [diplomatie.md](diplomatie.md), Krieg und Generalstab in
> [militaer.md](militaer.md), Ereignisse und Journale in [ereignisse.md](ereignisse.md), die Syntax in
> [chronik-skript.md](chronik-skript.md).

Bis M11 handeln die Staaten ohne Spieler nur nach festen Regeln: Sie behalten ihre Steuersätze,
nehmen Bündnisse ab einer Beziehung von 100 an, eskalieren Krisen nach Stärke, schließen Frieden,
sobald die Kriegspunkte reichen, und wählen die Optionen der Ereignisse nach Gewicht. Von sich aus
fordern sie nichts, entsenden keine Gesandten und erklären keine Kriege. Kriege entstehen nur aus
Lesezeichen, Krisen, Bundesbeschlüssen und Ereignissen.

M12 gibt jedem Staat ohne Spieler einen eigenen Antrieb:
- eine **Persönlichkeit** des Staates und seiner leitenden Personen (Herrscher, leitender Minister),
  die zur Laufzeit wechseln können (Thronwechsel 1848, 1858/61; Bismarck 1862);
- eine **historische Agenda** mit gewichteten Zielen und Zeitfenstern (Preußen: Führung im Norden,
  Österreich: Führung im Bund, Frankreich: Kompensation am Rhein, Dänemark: Schleswig);
- **Domänen-Planer** für Haushalt, Diplomatie, Krieg und Politik, die monatlich Optionen nach
  Nutzen bewerten und dieselben **Befehle** geben wie der Spieler;
- **Erklärungen**: Jede bedeutende Entscheidung trägt ihre drei wichtigsten Gründe;
- ein **KI-Labor**, das 100 Partien 1848–1871 ohne Spieler rechnet und die Ausgänge der deutschen
  Frage zählt.

Der spielbare Vertical Slice 1848–1871 (M13) baut darauf auf.

## 1. Umfang

Der Meilensteinplan verlangt:

- Entscheidungen zu Haushalt, Steuern, Diplomatie, Krieg und Politik;
- Persönlichkeiten der Herrscher und Minister aus Kapitel 13;
- KI rechnet deterministisch im Kern (eigener Zufallsstrom);
- Abnahme: 100 KI-Partien 1848–1871 ohne Spieler, keine Abstürze, Ergebnisse plausibel verteilt
  (Bericht mit Häufigkeiten: Einigung ja/nein, Form der Einigung).

Vorschlag (Entscheidung 1): **zwei Teile** wie bei M7 bis M11.

| Teil | Inhalt | Ergebnis |
|---|---|---|
| **M12a** | Persönlichkeiten, Staatspersönlichkeiten und Agenden im Chronik-Skript, Amtswechsel zur Laufzeit, Spielerstaat im Kern, Lagebild, Nutzenbewertung mit Gründen, Planer für Haushalt, Diplomatie, Krieg und Politik, KI-Labor auf der Test-Fixture | KI im Kern, prüfbar ohne Oberfläche |
| **M12b** | Daten 1848–1871 (Personen, Staatspersönlichkeiten, Agenden), Kalibrierung mit 100 Partien und Neuberechnung aller Labore, Personen und „Warum?“ in Godot, Doku und Review | Abnahme: 100 Partien, Bericht mit Häufigkeiten |

**Nicht in M12:**
- **Schwierigkeitsgrade** (Kapitel 21, Abschnitt 8): M12 baut eine Stufe („Geselle“). Lehrling bis
  Kanzler sind Parameter derselben Planer und kommen mit M13.
- **Offener Modus** (Kapitel 21, Abschnitt 4): M12 rechnet den **historischen Modus** (Agenden
  stark gewichtet, mit Zeitfenstern). Der offene Modus ist ein Faktor in den Regeln (Abschnitt 3.3)
  und wird mit M13 wählbar.
- **Informationslücken und Spionage** (Abschnitt 2 dort): Das Lagebild ist vollständig, wie die
  Anzeige des Spielers (Entscheidung 6).
- **HTN-Pläne und Einflusskarten** für Feldzüge: Die Heere führt weiter der Generalstab aus M10; die
  KI entscheidet über Mobilmachung, Krieg, Ziele und Frieden, nicht über einzelne Märsche.
- **Gesetzgebung, Ernennungen, Koalitionen, Länderkammern, Opposition** (Abschnitt 5 dort): Gesetze
  ändern sich weiter nur durch Ereignisse; es gibt keine Koalitionen und keinen Bundesrat mit
  Landespolitik vor 1867.
- **Weltbühne** mit rund 25 Außenmächten (Abschnitt 7 dort): Außer Frankreich und Dänemark gibt es
  keine Außenmächte; Russland, Großbritannien und Italien bleiben Text in Ereignissen.
- **Persönlichkeiten mit Laufbahn**, Geburt, Tod und Beziehungen (Kapitel 13, Abschnitte 1.3–1.5):
  Personen haben feste Amtszeiten oder Bedingungen; sie altern nicht und sterben nicht.
- **Regime-KI** (Abschnitt 6 dort), Bauprojekte und Produktionsmethoden (keine Daten dafür).

## 2. Ausgangslage

**Vorhanden:**
- **Befehle** für alles, was ein Staat tun kann: Steuersatz, Zollunion, Gesandter, Vertrag,
  Antrag, Forderung, Manöver, Marsch, Mobilmachung, Kriegserklärung, Kriegsziel, Friedensangebot,
  Generalstab, Ereigniswahl, Wahlkampf (`CommandType` in `simulation.hpp`). Jeder Befehl prüft seine
  Bedingungen selbst und wirkt am nächsten Tag.
- **Regeln für Staaten ohne Spieler:** Haushalt mit festen Sätzen, Kredit und Bankrott
  (wirtschaft.md 4.6); Annahme von Bündnis ab Beziehung 100 und Garantie ab 50 (diplomatie.md 4.3);
  Stimmen in der Bundesversammlung nach Beziehung (4.4); Parteinahme, Eskalation und Auflösung der
  Krisen nach Stärke (4.5); Mobilmachung, Märsche und Angriffe durch den **Generalstab** (militaer.md
  5); Frieden nach Kriegspunkten (4.5 dort); Ereignisoptionen nach Gewicht (ereignisse.md 3.5).
- **Ereignisspieler** (`World::event_player`, Befehl `Ereignisspieler`): Der Kern weiß, welcher Staat
  vom Menschen gespielt wird, aber nur für Ereignisse. Godot schickt ihn beim Start, nach dem Laden
  und beim Staatswechsel, im Parteienmodus keinen.
- **Zufallsstrom `Ki`** (Nummer 4) ist seit M1 reserviert und unbenutzt.
- **Herrscher** als Text je Staat (`herrscher` in `staaten_historie.csv`, 70 Zeilen); datierte
  Staatsfelder gelten nur zum Startdatum (lesezeichen_1848.md), Thronwechsel nach dem Start greifen
  nicht.
- **Ereignisse und Journale** (M11): 109 Ereignisse mit Merkmalen (`bismarck`, `gastein`,
  `sueden_fuer_anschluss` …), das Journal „Die deutsche Frage“ mit fünf Ausgängen und dem
  Ereignislabor, das Ausgänge über Seeds zählt.
- **Messung:** Ein Lauf 1848–1871 ohne Spieler braucht 29 s (Release, 4 Threads, ohne KI); die
  Labore rechnen ihre Seeds nacheinander.
- Ohne Spieler endet die deutsche Frage heute über 48 Seeds zu 35 % kleindeutsch, 23 % in zwei
  Bünden, 42 % im alten Bund (ereignisse.md 13, b3) – allein aus den Ereignissen.

**Es fehlt:** Personen mit Eigenschaften und Amtszeit; Ziele der Staaten; jede Entscheidung, die
ein Staat ohne Spieler von sich aus trifft; Gründe für Entscheidungen; ein Bericht über viele
Partien mit allen Systemen.

**Quellen:** wie in M7 bis M11 ohne Seitenzugriff. Personen und Amtszeiten 1848–1871 sind gut
bekannt (Nipperdey, Huber Bd. 2 und 3, Gall *Bismarck*, Clark *Preußen*, Doering-Manteuffel *Die
deutsche Frage und das europäische Staatensystem*). Amtsdaten sind `ungeprueft`, Eigenschaften,
Persönlichkeiten und Agenden **Spielwerte** (`spielwert`) mit kurzer Begründung (Entscheidung 13).

## 3. Persönlichkeiten und Agenden (M12a, Daten in M12b)

### 3.1 Neigungen

Die KI rechnet nicht mit Eigenschaften selbst, sondern mit **sechs Neigungen** je Staat (−2 … +2,
Festkomma), die aus Staatspersönlichkeit und Personen entstehen:

| Neigung | Hoch heißt | wirkt auf |
|---|---|---|
| `risiko` | nimmt schlechtere Kräfteverhältnisse in Kauf | Forderungen, Eskalation, Kriegserklärung |
| `expansion` | will Gebiet und Führung | Kriegsziele, Forderungen, Agenda „Führung“ |
| `legitimitaet` | hält am Bund, an Verträgen und Fürstenrecht fest | Bundestreue, Interventionen, Annahme von Anträgen |
| `reform` | gibt nach innen nach | Ereignisoptionen, Steuern bei Unruhe |
| `sparsamkeit` | hält den Haushalt ausgeglichen | Steuern, Mobilmachung, Kredit |
| `vermittlung` | sucht Kompromiss | Manöver „Vermittlung“, Friedensangebote |

### 3.2 Staatspersönlichkeit

Jeder Staat mit Daten hat eine Staatspersönlichkeit aus Kapitel 21, Abschnitt 3.1; für 1848–1871
genügen sechs:

| Persönlichkeit | risiko | expansion | legitimitaet | reform | sparsamkeit | vermittlung |
|---|---|---|---|---|---|---|
| Machtpolitiker | +1 | +2 | −1 | 0 | 0 | −1 |
| Bewahrer | −1 | 0 | +2 | −1 | 0 | 0 |
| Reformer | 0 | 0 | 0 | +2 | 0 | +1 |
| Händler | −2 | −1 | 0 | +1 | +2 | +1 |
| Hegemon | +1 | +1 | 0 | 0 | −1 | −1 |
| Gleichgewichtswahrer | 0 | −1 | +1 | 0 | 0 | +2 |

Vorschlag für die Daten (M12b, Spielwerte): Preußen Machtpolitiker, Österreich Bewahrer, Bayern
und Württemberg Bewahrer, Baden Reformer, Sachsen Bewahrer, Hannover Bewahrer, Hansestädte Händler,
Frankreich Hegemon, Dänemark Bewahrer; kleine Staaten ohne Angabe gelten als Bewahrer.

### 3.3 Personen

Neue Datei `common/persoenlichkeiten.chron` (Format künftig in chronik-skript.md, Abschnitt 3.37):

```
person otto_von_bismarck {
    name    = "Otto von Bismarck"
    staat   = PRU
    rolle   = minister            # herrscher oder minister (leitender Minister)
    von     = 1862.09.23
    bis     = 1890.03.20
    bedingung { merkmal = bismarck }      # optional, Vokabular der Ereignisse (ereignisse.md 3.2)
    werte   = { staatskunst = 8  diplomatie = 10  kriegskunst = 4  wirkung = 7 }
    eigenschaften = { machtpolitiker taktiker }
    quelle  = "Gall, Bismarck (1980)"
    sicherheit = ungeprueft
}
```

- **Im Amt** ist eine Person, wenn das Datum in `von … bis` liegt und ihre Bedingung gilt; gibt es
  mehrere, gilt die zuletzt beginnende. Der Kern prüft am Monatsersten; ein Wechsel schreibt die
  Chronik („Preußen: Otto von Bismarck wird Ministerpräsident“) und setzt den Text `herrscher` des
  Staates. Ohne Person im Amt gilt der Herrscher aus `staaten_historie.csv` ohne Eigenschaften.
- **Eigenschaften** stammen aus einer festen Liste von 16 aus Kapitel 13, Abschnitt 1.2, je mit
  Beitrag zu den Neigungen (Abschnitt 3.1): Machtpolitiker (+1 risiko, +1 expansion), Zauderer
  (−2 risiko), Pragmatiker (+1 vermittlung, −1 legitimitaet), Ideologe (−1 vermittlung), Bewahrer
  (+1 legitimitaet, −1 reform), Reformer (+1 reform), Fromm (+1 legitimitaet), Sparsam
  (+1 sparsamkeit), Prunkliebend (−1 sparsamkeit), Soldatenkönig (+1 risiko, −1 sparsamkeit),
  Ehrlicher Makler (+2 vermittlung), Taktiker (Kriegserklärung erst bei günstigem Bündnisfall),
  Draufgänger (+2 risiko), Vorsichtig (−1 risiko), Verwalter (+1 sparsamkeit, +1 reform),
  Krisenmanager (Unruhe zählt halb bei Steuern).
- **Werte** (1–10) wirken klein: Diplomatie ±10 % auf Annahme und Beziehungswachstum, Staatskunst
  ±10 % auf die Verwaltungskosten, Kriegskunst der Herrscher nur als Text (Befehlshaber hat M10).
- **Gewicht** der Personen nach Verfassung: absolut und ständisch Herrscher 0,6, Minister 0,4;
  konstitutionell 0,4 und 0,6. Neigung = Staatspersönlichkeit + Σ Gewicht × Eigenschaften, begrenzt
  auf −2 … +2.
- **Umfang 1848–1871** (Vorschlag): Herrscher und leitende Minister von Preußen, Österreich, Bayern,
  Sachsen, Hannover, Württemberg, Baden, Hessen-Darmstadt, Kurhessen, Frankreich, Dänemark – rund
  60 Personen. Ministerwechsel der Revolution hängen an Merkmalen der Ereignisse (Märzministerium
  Camphausen nur mit `maerzministerium`, Brandenburg/Manteuffel nach `belagerungszustand`,
  Schwarzenberg 1848–1852).

### 3.4 Agenden

Neue Datei `common/agenden.chron` (chronik-skript.md, künftig Abschnitt 3.38): je Staat gewichtete Ziele mit Zeitfenster und
Bedingung. Ein Ziel nennt **eine Art**, die ein Planer umsetzen kann:

| Art | Bedeutung | Planer |
|---|---|---|
| `fuehrung_bund` | Führung im Bund (Forderung, Anträge Vorsitz und Reform) | Diplomatie |
| `zollunion` | eine Zollunion halten, ausweiten oder ihr beitreten | Diplomatie |
| `buendnis` | Bündnis mit einem Staat | Diplomatie |
| `gebiet` | eine Verwaltungseinheit erwerben (Kriegsziel Gebiet) | Krieg |
| `annexion` | einen Staat einverleiben | Krieg |
| `schwaechen` | einen Rivalen schwächen (Forderungen, Kriegsziel Entschädigung) | Diplomatie, Krieg |
| `status_quo` | Bund und Grenzen erhalten (Intervention, Parteinahme gegen Revisionisten) | Diplomatie |
| `ruhe` | Unruhe senken (Ereignisoptionen, Steuern) | Politik, Haushalt |

```
agenda PRU {
    ziel { art = fuehrung_bund                gewicht = 60  ab = 1849.04.01 }
    ziel { art = zollunion  union = zollverein gewicht = 50 }
    ziel { art = gebiet  gebiet = dk_holstein gewicht = 40  ab = 1863.11.15  bis = 1866.12.31 }
    ziel { art = schwaechen  gegner = AUT      gewicht = 50  ab = 1862.09.23  bedingung { merkmal = bismarck } }
    ziel { art = annexion  gegner = HAN        gewicht = 20  ab = 1866.01.01  bedingung { krieg_mit = AUT } }
}
```

Ein Ziel ist **aktiv**, wenn sein Zeitfenster und seine Bedingung gelten. Der **historische Modus**
nimmt die Gewichte wie angegeben und Ziele nur in ihrem Fenster; der offene Modus (ab M13) halbiert
die Gewichte und öffnet die Fenster (Regel `agenda_offen`). Vorschlag für 1848–1871 rund zehn Staaten
mit je drei bis sechs Zielen; Staaten ohne Agenda verfolgen nur `status_quo` und `ruhe`.

## 4. Spielerstaat und Takt (M12a)

### 4.1 Spielerstaat im Kern

Der Befehl `Ereignisspieler` wird zum **Befehl `Spielerstaat`** (Entscheidung 3): Für diesen Staat
warten die Ereignisse wie bisher, und die KI gibt **keine Befehle** für ihn. Der Generalstab führt
nur Heere, die der Spieler ihm übergeben hat (wie M10). Im Parteienmodus und in den Laboren gibt es
keinen Spielerstaat: Die KI führt alle Staaten. Der Name im Zustand und im Spielstand bleibt
`event_player` (Formatwache), die Brücke behält `set_event_player` als Alias.

### 4.2 Takt

| Ebene | Takt | Inhalt |
|---|---|---|
| **Strategie** | 1. Januar, nach einem Frieden, nach einem Amtswechsel | aktive Ziele mit Gewicht × Neigung, ein **Hauptziel** und ein **Rivale** je Staat |
| **Planer** | Monatserster, nach Haushalt, Beziehungen und Bundesversammlung | Haushalt, Diplomatie, Krieg, Politik je Staat in fester Reihenfolge (Staatsindex) |
| **Reaktion** | täglich | offene Ereignisse des Staates, Manöver in eigenen Krisen, Annahme von Angeboten, Friedensangebote im Krieg |

Die Planer lesen den Zustand des Monatsersten (ein **Lagebild** je Staat, Abschnitt 4.3) und geben
Befehle in die Warteschlange; sie wirken wie Spielerbefehle **am nächsten Tag** und laufen durch
dieselben Prüfungen. Weil sie aus dem Zustand folgen, stehen KI-Befehle **nicht im
Befehlsprotokoll** (eine Wiederholung rechnet sie neu; Entscheidung 4). Die Planer rechnen die
Staaten nacheinander; jeder Staat liest nur das Lagebild, nicht die Befehle der anderen desselben
Tages. Das hält das Ergebnis unabhängig von der Zahl der Threads.

### 4.3 Lagebild

Je Staat am Monatsersten, nicht im Spielstand (aus dem Zustand berechenbar):

- **Stärke** = Soldaten unter Waffen + mobilisierbare Reserve (Friedensstärke × Faktor der
  Mobilmachung − unter Waffen); **Bündnisstärke** = Stärke + Stärke der Verbündeten und Garanten.
- **Bedrohung** durch einen Staat = Stärke des anderen ÷ eigene Stärke × Feindschaft (Beziehung
  unter 0: 1 + |Beziehung|/100, sonst 1 − Beziehung/400) × Nähe (gemeinsame Grenze 1, im Bund 0,6,
  sonst 0,3). Der Staat mit der größten Bedrohung ist der **Rivale**, sofern die Agenda keinen nennt.
- **Finanzen**: Saldo der letzten drei Monate, Schuldenquote (Schulden ÷ Jahreseinnahmen), Zins.
- **Innere Lage**: Unruhe, Zielwert der Unruhe, laufender Aufstand, Zustimmung von Militär und Adel.
- **Bund und Lager**: Lager im Bund, Zollunion, offene Anträge, laufende Spiele und Kriege mit
  Kriegspunkten und Müdigkeit.

## 5. Nutzenbewertung und Gründe (M12a)

Jede Option eines Planers erhält einen **Nutzen** als Summe benannter Faktoren:

```
Nutzen(option) = Σ faktor_i        (Festkomma, je Faktor ein Name: „Bedrohung durch Österreich +35“)
```

- Ein Planer wählt die Option mit dem größten Nutzen, wenn er die **Schwelle** der Domäne
  übersteigt (Regeln in `common/ki.chron`); „nichts tun“ ist immer eine Option mit Nutzen 0.
- **Rauschen**: Jeder Nutzen erhält einen Zuschlag aus dem Strom `Ki` (gleichverteilt ±
  `ki_rauschen`, Standard 10), damit Partien mit verschiedenem Seed verschieden verlaufen.
- **Gründe**: die drei Faktoren mit dem größten Betrag. Bedeutende Entscheidungen (Kriegserklärung,
  Forderung, Bündnis, Friedensangebot, Antrag, Beitritt, Steuererhöhung um mehr als 2 Punkte)
  schreiben die Chronik mit Gründen und das **KI-Protokoll** des Staates (letzte 20 Entscheidungen
  mit Datum, Art, Ziel und Gründen; im Spielstand).
- **Abkühlung**: Nach einer Forderung, Kriegserklärung oder einem abgelehnten Angebot wartet der
  Planer für dieselbe Art und denselben Gegner `ki_abkuehlung` Monate (Standard 12).

## 6. Domänen-Planer (M12a)

### 6.1 Haushalt und Steuern

Ersetzt die festen Sätze der Staaten ohne Spieler (wirtschaft.md 4.6).

- **Erhöhen** (Kontribution oder Akzise, die mit der kleineren Wirkung auf die Unruhe): Saldo der
  letzten drei Monate negativ und Schuldenquote über `ki_schulden_grenze` (Standard 3,0); Faktoren:
  Defizit, Schuldenquote, Sparsamkeit; dagegen Unruhe und Reform.
- **Senken**: Überschuss über 15 % der Einnahmen und Schuldenquote unter 1,0, oder Unruhe über 50
  (ohne Krieg); Faktoren: Überschuss, Unruhe, Reform.
- Schritt höchstens 2 Prozentpunkte je Monat, nie über den Höchstsatz, Zoll nicht als Mitglied einer
  Zollunion (wie beim Spieler).
- Im Krieg und bei Mobilmachung zählt das Defizit halb; Kredit und Bankrott bleiben Regeln des
  Kerns.

### 6.2 Diplomatie

| Handlung | Nutzen (Hauptfaktoren) | ersetzt |
|---|---|---|
| **Gesandte** entsenden und abberufen | Wichtigkeit des Partners: Agenda, Rivale, Verbündete, Stärke; freie Gesandte zuerst zu den wichtigsten | – (Staaten ohne Spieler hatten keine) |
| **Bündnis anbieten** | Beziehung ≥ 50, gemeinsamer Rivale, Agenda `buendnis`, Stärke des Partners; dagegen legitimitaet bei Lagerwechsel | – |
| **Bündnis annehmen** | dieselben Faktoren aus Sicht des Ziels | Regel „ab Beziehung 100“ |
| **Garantie** geben | Großmacht für kleinen Nachbarn im eigenen Lager, status_quo | – (Annahme: Regel „ab 50“ bleibt Grundlage) |
| **Forderung** stellen (Führung, Zollunion, Bündnis lösen) | Agenda, Bündnisstärke ÷ Gegner ≥ 1,0 − 0,15 × risiko, Rivale, expansion; dagegen laufender Krieg, Abkühlung | – |
| **Manöver** in einer Krise | nachgeben unter ⅔ der Stärke, vermitteln mit vermittlung, eskalieren ab gleicher Stärke + risiko | Eskalationsregel der Hauptparteien (diplomatie.md 4.5) |
| **Antrag** in der Bundesversammlung | Agenda `fuehrung_bund`/`zollunion`, Prognose „angenommen“, kein offener Antrag | – |
| **Zollunion** beitreten oder austreten | Handel mit der Union, Lager, Agenda, Leitmacht ist Rivale (dagegen) | – |

Die **Parteinahme** in Krisen und die **Stimmen** in der Bundesversammlung bleiben Regeln
(diplomatie.md 4.4/4.5), erhalten aber den Faktor legitimitaet (ein Bewahrer stimmt für Anträge der
Präsidialmacht eher mit Ja).

### 6.3 Krieg

- **Kriegserklärung** nur aus einem aktiven Ziel der Art `gebiet`, `annexion` oder `schwaechen`
  (Entscheidung 8). Nutzen: Gewicht des Ziels, Bündnisstärke der eigenen Seite ÷ der Gegenseite
  (aus `get_declaration_sides`, also mit Bündnisfall) gegen die Schwelle 1,3 − 0,15 × risiko, Rivale,
  expansion; dagegen Schuldenquote, Unruhe über 40, laufender Krieg, Kriegsmüdigkeit des letzten
  Krieges, legitimitaet gegen Bundesstaaten (Bundesrecht). Taktiker erklären nur, wenn kein Dritter
  dem Gegner beisteht. Kriegsziel ist das Ziel der Agenda.
- **Mobilmachung**: bei Krise Stufe 3 (wie der Generalstab), zusätzlich wenn ein Nachbar mobil macht
  und die Bedrohung über 1,5 liegt; **Abrüstung** 60 Tage nach Frieden und Krise.
- **Weitere Kriegsziele** im Krieg, wenn die Kriegspunkte die Kosten der eigenen Ziele um 50 %
  übersteigen und die Agenda ein weiteres Ziel gegen die Gegenseite hat.
- **Frieden**: Angebot und Annahme nach Nutzen: Kriegspunkte gegen Kosten, Müdigkeit beider Seiten,
  Unruhe, Schulden, vermittlung. Die Regel „Frieden am Monatsersten, wenn eine Seite ihre Ziele
  deckt“ bleibt als Rückfall (militaer.md 4.5).
- Die **Heere** führt der Generalstab (militaer.md 5); die KI übergibt ihm alle Heere ihrer
  Staaten.

### 6.4 Politik

- **Ereignisoptionen**: Optionen dürfen einen Block `ki { risiko = 1  reform = -1 … }` tragen; ihr
  Gewicht wird mit `1 + 0,25 × Σ Neigung × Faktor` (mindestens 0,1) multipliziert. Ohne Block bleibt
  das Gewicht aus M11 (Entscheidung 9). Die Korridore aus M11 werden mit KI neu geprüft (M12b).
- **Wahlkampf der Strömungen** ohne Spieler (politik.md 9): monatlich bis zur Wahl; Mittel in die
  Wahlkreise mit der knappsten Lage nach dem Stimmungsbericht, den auch der Spieler sieht (mit
  Fehler); Rücklage 20 %.
- **Unruhe**: Ist die Unruhe über 60 und kein Ereignis offen, senkt der Haushaltsplaner Steuern (6.1);
  weitere Mittel gegen Unruhe gibt es ohne Gesetzgebung nicht.

## 7. KI-Labor (M12a, Kalibrierung in M12b)

`uhrwerk-cli ki --szenario voelkerfruehling --seeds 100 --bis 1871.07.01 [--ziele datei]
[--parallel N] [--ohne-ki]` rechnet Partien ohne Spieler und berichtet:

- je Lauf: Ausgang der deutschen Frage mit Datum, Kriege (wer gegen wen, Sieger, Dauer),
  untergegangene Staaten, Bankrotte, höchste Unruhe, Zahl der KI-Entscheidungen je Art, Laufzeit
  und Anteil der KI daran;
- über alle Läufe: **Einigung ja/nein** (ja: kleindeutsch, großdeutsch oder zwei Bünde) und die
  **Form der Einigung** mit Anteilen, Kriege je Paar, Bündnisse, Forderungen, Anträge, Abstürze
  (jeder Lauf in eigenem Prozessteil; ein Fehler zählt, der Bericht läuft weiter);
- Korridore aus `common/kiziele_<szenario>.chron`; Exitcode 1 bei Verstößen.

`--parallel N` rechnet N Seeds gleichzeitig (je Lauf ein Thread, Entscheidung 11); 100 Läufe sollen
auf 4 Kernen in höchstens 20 Minuten fertig sein. `--ohne-ki` rechnet wie M11 zum Vergleich.

**Korridore** (Vorschlag, Entscheidung 10; 100 Läufe 1.3.1848–1.7.1871):

| Messgröße | Korridor |
|---|---|
| Einigung (kleindeutsch, großdeutsch oder zwei Bünde) | 55 … 90 % |
| davon kleindeutsch | 30 … 70 % aller Läufe |
| großdeutsch | 0 … 10 % |
| zwei Bünde | 10 … 40 % |
| alter Bund | 10 … 40 % |
| ohne Bund und Reich | 0 … 10 % |
| kein Ausgang in mehr als | 80 % der Läufe |
| ein Krieg Preußen gegen Österreich | 40 … 90 % |
| Bankrott Preußens oder Österreichs | 0 … 20 % |
| Abstürze | 0 |

Zum Vergleich Kapitel 21, Abschnitt 10: „ein deutscher Bundesstaat bis 1880“ 70–90 %, davon
kleindeutsch 55–75 % (historischer Modus). Die Korridore bis 1871 liegen darunter, weil die Partie
neun Jahre früher endet und der Süden 1871 noch draußen sein darf.

**CI:** je Push 16 Seeds gegen die Korridore (wie die übrigen Labore); die 100 Partien als eigener
Workflow `ki-partien.yml` (manuell und wöchentlich), Bericht als Artefakt.

## 8. Zustand, Kern und Spielstand

- Neue Dateien `core/src/ai.cpp` (Planer, Nutzen, Protokoll), `core/src/ai_loader.cpp` (Personen,
  Agenden, Regeln), Header `uhrwerk/ai.hpp`, Labor `core/src/ai_lab.cpp`.
- **Zustand** (nur in Welten mit KI-Daten): Personen im Amt je Staat, Hauptziel und Rivale,
  Abkühlungen (Art, Gegner, bis), KI-Protokoll je Staat. Spielstand **Format 6**, Inhaltsbereich
  `ki` (Personen, Agenden, Regeln, Eigenschaften).
- **1648 bitgleich** (Entscheidung 14): ohne KI-Daten keine Planer und kein Zugriff auf den Strom
  `Ki`; die Prüfsumme nach 100 Jahren bleibt `5db13a66d63e6079`.
- **Leistung:** Planer höchstens 5 ms je Staat und Monat, Strategie 50 ms je Staat und Jahr (Kapitel
  21, Abschnitt 11); gemessen im KI-Labor.

## 9. Brücke und Godot (M12b)

- **Brücke:** Personen je Staat (`get_state_persons`: Name, Rolle, seit, Werte, Eigenschaften,
  Neigungen), KI-Protokoll (`get_ai_decisions(state, limit)` mit Gründen), Absichten
  (`get_intentions(state)`: Hauptziel, Rivale, aktive Ziele mit Gewicht), Spielerstaat
  (`set_player_state` schickt den Befehl `Spielerstaat`).
- **Godot:**
  - **Staatsinfo:** Herrscher und leitender Minister mit Eigenschaften und den sechs Neigungen.
  - **Diplomatiefenster:** für den gewählten Staat „Absichten“ (Hauptziel, Rivale) – nur mit eigenem
    Gesandten dort (Kapitel 21, Abschnitt 9: Absichten mit ausreichendem Lagebild) – und die letzten
    Entscheidungen mit „Warum?“-Tooltip (drei Gründe).
  - **Chronik:** KI-Entscheidungen mit Gründen; Kriegserklärungen und Forderungen an den Spieler
    zusätzlich als Hinweis.
  - **Krisenleiste und Kriegsfenster:** Begründung der Gegenseite im Tooltip.

## 10. Tests und Abnahme

| Kriterium | Prüfung |
|---|---|
| 100 KI-Partien 1848–1871 ohne Spieler, keine Abstürze | `uhrwerk-cli ki --seeds 100` (Workflow `ki-partien.yml`, lokal vor dem Abschluss); Abstürze 0 |
| Ergebnisse plausibel verteilt, Bericht mit Häufigkeiten | Bericht „Einigung ja/nein“ und „Form der Einigung“; Korridore aus Abschnitt 7 erfüllt; kein Ausgang in allen Läufen |

Weitere Tests:
- **Einheitentests:** Lader (Personen, Agenden, Fehler und Verweise), Amtswechsel nach Datum und
  Bedingung, Neigungen aus Persönlichkeit und Eigenschaften, Nutzen mit Gründen, jeder Planer auf der
  Fixture (Steuer erhöhen bei Defizit, Bündnis bei gemeinsamem Rivalen, Forderung erst bei
  Übermacht, Kriegserklärung nur im Zeitfenster, Frieden bei Erschöpfung), Spielerstaat ohne
  KI-Befehle, Abkühlung.
- **Determinismus und Spielstände:** gleiche Prüfsumme mit 1 und 4 Threads und mit `--parallel`;
  Speichern mitten in einer KI-Krise und nach einem Amtswechsel; Wiederholung mit Spielerbefehlen
  ergibt dieselben KI-Befehle; Formatwache.
- **1648 bitgleich**; 1848 rechnet mit KI anders: Bilanz, Wahl, Zoll, Krieg und Ereignisse werden
  mit KI neu gerechnet und ihre Korridore geprüft (M12b, Entscheidung 12).
- **Godot:** Rauchtest der neuen Methoden, Klicktest (Staatsinfo mit Personen, Absichten mit
  Gesandtem, „Warum?“ an einer Forderung der KI), Screenshot der Staatsinfo.

## 11. Entscheidungen (zur Freigabe)

1. **Teilung M12a/M12b.** Empfehlung: **ja** (Abschnitt 1). Alternative: Daten und System zusammen,
   Preußen und Österreich zuerst (schneller sichtbar, schwerer prüfbar).
2. **Architektur.** Empfehlung: **Utility-Planer** je Domäne mit benannten Faktoren, monatlich, dazu
   eine jährliche Strategieebene (Abschnitte 4–6); die Ausführung im Krieg bleibt beim Generalstab.
   Alternativen:
   - HTN-Pläne und Einflusskarten nach Kapitel 21 schon jetzt (viel Aufwand, für 38 Staaten und drei
     Kriege kaum Gewinn);
   - weiter feste Regeln, nur erweitert (nicht erklärbar, keine Persönlichkeit).
3. **Spielerstaat.** Empfehlung: Der Befehl `Ereignisspieler` wird zum **Spielerstaat**, den die KI
   auslässt (Abschnitt 4.1). Alternative: ein zweiter Befehl nur für die KI (zwei Zustände, die
   auseinanderlaufen können).
4. **KI-Befehle.** Empfehlung: **dieselben Befehle und Prüfungen** wie für den Spieler, wirksam am
   nächsten Tag, **nicht im Protokoll** (aus dem Zustand ableitbar). Alternativen:
   - direkte Eingriffe in den Zustand (schneller, aber Sonderwege und Schummelgefahr);
   - KI-Befehle im Protokoll (größere Spielstände, Wiederholung prüft nichts Neues).
5. **Persönlichkeiten.** Empfehlung: **Staatspersönlichkeit + Herrscher + leitender Minister** mit 16
   Eigenschaften, verdichtet zu **sechs Neigungen** (Abschnitt 3); Amtszeiten nach Datum und
   Bedingung, rund 60 Personen. Alternativen:
   - nur Staatspersönlichkeit (kein Thronwechsel, Bismarck wirkt nicht);
   - das volle System aus Kapitel 13 (Laufbahn, Tod, Beziehungen, 90 Eigenschaften).
6. **Lagebild.** Empfehlung: **vollständige Information** wie in der Anzeige des Spielers.
   Alternative: Schätzung fremder Stärken (±20 % ohne Gesandten); sinnvoll erst mit Spionage.
7. **Modus.** Empfehlung: **historischer Modus** mit Zeitfenstern in M12, offener Modus als Regel
   vorbereitet und mit M13 wählbar. Alternative: nur offener Modus (freier, aber die Korridore der
   Einigung wären kaum zu halten).
8. **Kriege der KI.** Empfehlung: **nur aus aktiven Agendazielen** im Zeitfenster und mit Übermacht;
   Ereignisse bleiben daneben (M11, Entscheidung 10). Alternativen:
   - jede Bedrohung kann zum Krieg führen (offener, aber Kriege zur Unzeit, etwa Bayern gegen Baden);
   - Kriege weiter nur aus Ereignissen (dann bliebe die KI im Krieg stumm).
9. **Ereignisoptionen.** Empfehlung: Gewichte aus M11 **mal Neigung** über einen optionalen Block
   `ki { … }` (Abschnitt 6.4). Alternative: KI wählt Optionen allein nach Nutzen (dann wären die
   Gewichte und Korridore aus M11 hinfällig).
10. **Korridore.** Empfehlung: die Werte in Abschnitt 7 (Einigung 55–90 %, kleindeutsch 30–70 %).
    Alternative: die Werte aus Kapitel 21 bis 1880 (70–90 %, davon kleindeutsch 55–75 %) schon für
    1871 (verlangt, dass der Süden fast immer beitritt).
11. **Laufzeit des Labors.** Empfehlung: **Seeds parallel** (`--parallel`), 16 Seeds je Push, 100 in
    einem eigenen Workflow. Alternative: 100 Seeds je Push (rund 50 Minuten mehr CI).
12. **Andere Labore.** Empfehlung: Die KI rechnet in **allen** Läufen ohne Spieler ab 1848; Bilanz,
    Wahl, Zoll, Krieg und Ereignisse werden in M12b mit KI neu kalibriert, `--ohne-ki` vergleicht.
    Alternative: KI nur im KI-Labor (die übrigen Korridore bleiben unberührt, prüfen aber nicht mehr
    das Spiel, das man spielt).
13. **Quellen.** Empfehlung: Amtsdaten `ungeprueft` aus der Literatur, Eigenschaften, Agenden und
    Persönlichkeiten `spielwert` mit Begründung; neutrale, funktionsbezogene Eigenschaften
    (Kapitel 13, Abschnitt 1.7 sinngemäß), keine herabsetzenden. Alternative: Eigenschaften erst
    nach Fachprüfung (dann wartet die Abnahme).
14. **Prüfsumme 1648.** Empfehlung: **bitgleich** (KI nur in Welten mit KI-Daten); Spielstand
    Format 6. Alternative: neue Referenzprüfsumme.

## 12. Umsetzungsschritte

| Schritt | Inhalt | Prüfung |
|---|---|---|
| a1 | **Datenmodell:** Personen (`common/persoenlichkeiten.chron`), Eigenschaften und Neigungen, Staatspersönlichkeit, Agenden (`common/agenden.chron`), Regeln (`common/ki.chron`); Amtswechsel zur Laufzeit; Befehl `Spielerstaat`; Zustand, Inhaltsbereich `ki`, Format 6 | Unit-Tests, `pruefen`, 1648 bitgleich |
| a2 | **Rahmen und Haushalt:** Takt, Lagebild, Nutzen mit Gründen und Rauschen, KI-Protokoll, Abkühlung; Planer Haushalt und Mobilmachung | Unit-Tests auf der Fixture |
| a3 | **Diplomatie und Politik:** Gesandte, Verträge (Angebot und Annahme), Forderungen, Manöver, Anträge, Zollunion; Ereignisoptionen mit Neigung, Wahlkampf der Strömungen | Unit-Tests |
| a4 | **Krieg:** Kriegserklärung aus Agenden, weitere Ziele, Frieden nach Nutzen, Abrüstung | Unit-Tests, Kriegslabor auf der Fixture |
| a5 | **KI-Labor** `uhrwerk-cli ki` mit Korridoren, `--parallel` und `--ohne-ki` auf der Test-Fixture; CTests | Labor auf der Fixture |
| b1 | **Daten 1848–1871:** rund 60 Personen, Staatspersönlichkeiten, Agenden von etwa zehn Staaten, Blöcke `ki` an den großen Ereignissen | `pruefen`, Archivar |
| b2 | **Kalibrierung:** 100 Partien, Korridore aus Abschnitt 7; Bilanz, Wahl, Zoll, Krieg und Ereignisse mit KI neu; Workflow `ki-partien.yml` | Korridore erfüllt, CI |
| b3 | **Brücke und Godot:** Personen in der Staatsinfo, Absichten und „Warum?“ im Diplomatiefenster, Chronik, Spielerstaat | Godot-Tests, Screenshots |
| b4 | **Doku und Review** (Abschluss M12) | CI grün |

## 13. Umsetzungsstand

| Schritt | Stand |
|---|---|
| – | Entwurf; Entscheidungen 1–14 stehen zur Freigabe |
