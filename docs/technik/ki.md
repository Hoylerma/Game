# Künstliche Intelligenz der Staaten (Meilenstein M12)

> **Status: verbindlich** – Entscheidungen 1–14 wie empfohlen freigegeben (Abschnitt 11); Umsetzung
> läuft, Stand in Abschnitt 13. Grundlage sind [21 Künstliche Intelligenz](../21_kuenstliche_intelligenz.md) (Abschnitte
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

Neue Datei `common/persoenlichkeiten.chron` (Format in chronik-skript.md, Abschnitt 3.38):

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

Neue Datei `common/agenden.chron` (chronik-skript.md, Abschnitt 3.39): je Staat gewichtete Ziele mit Zeitfenster und
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

Präzisiert in Schritt a2: Die Strategie (Hauptziel und Rivale) wird **jeden Monatsersten** vor den
Planern neu bestimmt; das kostet wenig und folgt Frieden und Amtswechsel ohne eigenen Auslöser. Am
Monatsersten läuft die KI nach den Ereignissen (Schritt 6f des Tages): erst die Ämter, dann je
bestehendem Staat außer dem Spielerstaat in der Reihenfolge der Staaten Strategie, Haushalt und
Mobilmachung.

### 4.3 Lagebild

Je Staat am Monatsersten, nicht im Spielstand (aus dem Zustand berechenbar):

- **Stärke** = Soldaten unter Waffen + mobilisierbare Reserve (Friedensstärke × Faktor der
  Mobilmachung − unter Waffen); **Bündnisstärke** = Stärke + Stärke der Verbündeten und Garanten.
- **Bedrohung** durch einen Staat = Stärke des anderen ÷ eigene Stärke × Feindschaft (Beziehung
  unter 0: 1 + |Beziehung|/100, sonst 1 − Beziehung/400) × Nähe (gemeinsame Grenze 1, im Bund 0,6,
  sonst 0,3). Der Staat mit der größten Bedrohung ist der **Rivale**, sofern die Agenda keinen nennt.
- **Finanzen**: Saldo des Vormonats (Einnahmen − Ausgaben; präzisiert in a2 statt drei Monate, die
  Abkühlung der Steuern glättet), Schuldenquote (Schulden ÷ Jahreseinnahmen).
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
  Planer für dieselbe Art und denselben Gegner `abkuehlung` Monate (Standard 12); nach einer
  Steueränderung `abkuehlung_steuern` Monate (Standard 3, beide Richtungen), nach einer Mobilmachung
  ebenso lange.

Präzisiert in Schritt a2: Schwelle `schwelle` (Standard 10) für alle Planer; das Rauschen wird nur für
Optionen gezogen, deren Bedingung gilt, in fester Reihenfolge. Die Faktoren tragen einen Staat, wo es
einen gibt („Bedrohung durch Österreich +12“). Das KI-Protokoll hält je Staat die letzten `protokoll`
Entscheidungen (Standard 20) mit Datum, Art, Ziel, neuem Wert, Nutzen und drei Gründen; in a2 schreibt
nur die Mobilmachung zusätzlich die Chronik (über den Befehl).

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

Präzisiert in Schritt a2 (Spielwerte in `common/ki.chron`):

| Option | Bedingung | Faktoren |
|---|---|---|
| erhöhen | Saldo < 0 und Schuldenquote > `schulden_grenze` | Defizit = min(50, 100 × Defizit ÷ Einnahmen, im Krieg oder mobil halb); Schuldenquote = min(30, 10 × (Quote − Grenze)); Sparsamkeit = 10 × Neigung; Unruhe = −½ × Unruhe; Reform = −5 × Neigung |
| senken | Überschuss > `ueberschuss_ab` (15 %) der Einnahmen und Quote < 1, oder Unruhe > `unruhe_steuern_ab` (50) ohne Krieg | Überschuss = min(50, 100 × (Anteil − 15 %)); Unruhe = ½ × (Unruhe − 50); Sparsamkeit = −10 × Neigung; Reform = 5 × Neigung |

Ein Überschuss senkt die Steuern nur, wenn der Schatz mindestens `ruecklage_ab` Jahreseinnahmen hält
(Standard 1): Ohne diese Bedingung senkten die kleinen Staaten 1848 ihre Kontribution bis auf die
Hälfte, der Zollanteil des Zollvereins verdoppelte sich bis 1853 und die Steuerquote fiel in 30 Jahren
auf 0,4 des Starts (Bilanzziel 0,6 … 1,4).

Erhöht wird die niedrigere von Kontribution und Akzise (bei Gleichstand die Akzise) um einen Punkt,
bei einem Defizit-Faktor ab 30 um zwei; gesenkt die höhere (bei Gleichstand die Kontribution) um einen
Punkt. Gewinnt die größere Option mit Rauschen über der Schwelle. Der Zoll bleibt unberührt.

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

Präzisiert in Schritt a3 (je Staat und Monatserster höchstens eine Handlung jeder Art):

| Handlung | Bedingung | Faktoren |
|---|---|---|
| Gesandter | freier Gesandter, Ziel nimmt ihn an; höchstens einer je Monat | Agenda 30 (Ziel nennt den Staat), Rivale 20, Verbündeter oder Garant 15, Nachbar 10, Stärke 10 × Anteil an der stärksten Macht; Sparsamkeit −5 × Neigung |
| Bündnis anbieten | weniger als `buendnisse` (3) Bündnisse; gemeinsamer Rivale (beide Strategien nennen denselben Staat, seit b2 nur mit Beziehung des Anbietenden zu ihm unter `rival_below`, also ein Gegner) oder Agenda `buendnis`; das Ziel würde annehmen | Agenda ½ × Gewicht, Rivale 20, Stärke min(20, 10 × Verhältnis), Beziehung ÷ 20, Bündnisse −5 je eigenes |
| Bündnis annehmen (ohne Rauschen) | Regel gegen Bündnisse mit Rivalen bleibt; beide haben weniger als `buendnisse` Bündnisse (seit a5) | (Beziehung − 50) ÷ 5, gemeinsamer Rivale +20, Anbieter ist der eigene Rivale −30; ab der Schwelle. Bei Beziehung 100 ohne Rivalen wie die Regel aus M9 |
| Garantie | Nachbar mit höchstens einem Drittel der eigenen Stärke, Garantie nach Regel möglich | Nachbar 5, Beziehung ÷ 10, Legitimität 5 × Neigung, Agenda `status_quo` ¼ × Gewicht |
| Forderung | kein eigenes Spiel, kein Krieg; Agenda `fuehrung_bund` (gegen den Rivalen, sonst die Präsidialmacht), `zollunion` (Leitmacht gegen benachbarte Bundesstaaten außerhalb, seit b1; seit b2 nur gegen Mitglieder einer anderen Union, also Zollverein gegen Steuerverein) oder `schwaechen` (Führung im Bund gegen den Gegner); Bündnisstärke ≥ 1 − 0,15 × Risiko der des Gegners; seit b1 höchstens eine Forderung je `abkuehlung` (zusätzlich zur Abkühlung je Gegner) | Agenda ½ × Gewicht, Stärke 20 × Überschuss über die Grenze, Expansion und Risiko je 5 × Neigung |
| Manöver | Hauptpartei eines Spiels; höchstens eines je Monat | nachgeben unter `spiel_nachgeben` − 0,1 × Risiko, eskalieren ab 1 − 0,15 × Risiko, sonst vermitteln mit Neigung Vermittlung über 0, sonst entscheidet die Regel der Stufe |
| Antrag | Bundesversammlung tagt; Agenda `fuehrung_bund` (Präsidium, als Präsidialmacht die Reform) oder `zollunion` (Zolleinigung der eigenen Union; seit b2 stellt die KI diesen Antrag nicht mehr: angenommen führte er 1851 alle Bundesstaaten samt Österreich in den Zollverein, was Preußen historisch gerade verhinderte); Antrag gültig und mit Mehrheit nach der Vorschau | Agenda ½ × Gewicht, Legitimität 3 × Neigung |
| Zollunion | Agenda `zollunion` nennt die Union, Beitritt möglich | Agenda ½ × Gewicht, Beziehung zur Leitmacht ÷ 10, Leitmacht ist Rivale −30 |

Forderungen, Anträge und Beitritte folgen allein der Agenda; ohne Agenden (bis b1) handeln die Staaten
diplomatisch nur mit Gesandten, Bündnissen gegen gemeinsame Rivalen, Garantien und Manövern.

**Bündnisgrenze bei der Annahme** (Schritt a5, nach dem ersten KI-Labor 1848): Die Grenze `buendnisse`
galt zunächst nur für den Anbietenden. Weil alle Angebote eines Monatsersten am nächsten Tag nacheinander
wirksam werden, nahm ein Staat beliebig viele an: Am 2. März 1848 entstanden 36 Bündnisse, Ende 1849
waren es 91 (fast fünf je Staat). Seitdem lehnt ein Staat ohne Spieler ab, wenn er oder der Anbietende die
Grenze schon erreicht hat: 11 Bündnisse am 2. März 1848, 55 Ende 1849 – nahe der Obergrenze von drei je
Staat. Wie viele Bündnisse plausibel sind (`buendnisse`, Schwelle, Faktor Rivale), klärt die Kalibrierung
in b2.

### 6.3 Krieg

- **Kriegserklärung** nur aus einem aktiven Ziel der Art `gebiet`, `annexion` oder `schwaechen`
  (Entscheidung 8). Nutzen: Gewicht des Ziels, Bündnisstärke der eigenen Seite ÷ der Gegenseite
  (aus `get_declaration_sides`, also mit Bündnisfall) gegen die Schwelle 1,3 − 0,15 × risiko, Rivale,
  expansion; dagegen Schuldenquote, Unruhe über 40, laufender Krieg, Kriegsmüdigkeit des letzten
  Krieges, legitimitaet gegen Bundesstaaten (Bundesrecht). Taktiker erklären nur bei günstigem
  Bündnisfall: die Gegenseite hat höchstens so viele Staaten wie die eigene (seit b2; vorher nur gegen einen
  Gegner ohne Beistand). Kriegsziel ist das Ziel der Agenda.
- **Mobilmachung**: bei Krise Stufe 3 (wie der Generalstab), zusätzlich wenn ein Nachbar mobil macht
  und die Bedrohung über 1,5 liegt; **Abrüstung** 60 Tage nach Frieden und Krise.
  Präzisiert in a2: Mobil gegen einen Nachbarn nur bei Beziehung unter 0 (Faktor Bedrohung =
  10 + 20 × (Bedrohung − `bedrohung_mobil`), Risiko + 5 × Neigung, Sparsamkeit − 5 × Neigung).
  Abrüstung nach `abruestung_monate` (2) Monatsersten in Folge, an denen der Staat mobil ist, ohne
  Krieg, ohne eigene Krise auf Stufe 3, ohne Hilfe gegen einen Aufstand und ohne Mobilmachung einer
  Krise (Faktor Frieden = 10 × Monate, Sparsamkeit + 5, Risiko − 5 je Neigungspunkt).
- **Weitere Kriegsziele** im Krieg, wenn die Kriegspunkte die Kosten der eigenen Ziele um 50 %
  übersteigen und die Agenda ein weiteres Ziel gegen die Gegenseite hat.
- **Frieden**: Angebot und Annahme nach Nutzen: Kriegspunkte gegen Kosten, Müdigkeit beider Seiten,
  Unruhe, Schulden, vermittlung. Die Regel „Frieden am Monatsersten, wenn eine Seite ihre Ziele
  deckt“ bleibt als Rückfall (militaer.md 4.5).
- Die **Heere** führt der Generalstab (militaer.md 5); die KI übergibt ihm alle Heere ihrer
  Staaten.

Präzisiert in Schritt a4:

| Handlung | Bedingung | Faktoren |
|---|---|---|
| Kriegserklärung | nicht im Krieg; aktives Agendaziel `gebiet` (Gegner: Besitzer der meisten Orte der Einheit), `annexion` oder `schwaechen`; Erklärung möglich; Seiten nach dem Bündnisfall mit Stärke ≥ `kriegs_staerke` (1,3) − 0,15 × Risiko; Taktiker nur, wenn die Gegenseite nach dem Bündnisfall höchstens so viele Staaten hat wie die eigene (seit b2; vorher nur gegen einen Gegner ohne Beistand, sodass Bismarck 1866 nie erklärte); Abkühlung je Gegner | Agenda = Gewicht, Stärke min(40, 20 × Überschuss), Expansion und Risiko je 5 × Neigung, Schuldenquote −10 je Punkt über der Grenze, Unruhe −½ je Punkt über 40, gegen einen Bundesstaat Legitimität −10 × Neigung; **doppelte Schwelle** |
| Kriegsziel der Erklärung | | `gebiet`: die Einheit; `annexion`: der Staat; `schwaechen`: Führung im Bund unter Bundesstaaten, sonst Entschädigung von sechs Monatseinnahmen |
| weiteres Kriegsziel | Hauptpartei; Kriegspunkte ≥ 1,5 × Kosten der eigenen Ziele; Agendaziel `gebiet` oder `annexion` gültig in diesem Krieg; eines je Monat | Agenda = Gewicht, Kriegspunkte − Kosten, Expansion 5 × Neigung |
| Friedensangebot | Hauptpartei; die Gegenseite nähme nach der Vorab-Prüfung an; eines je Monat | eigene Ziele, wenn die Punkte die Kosten decken (10 + Punkte − Kosten); Kapitulation, wenn die Gegenseite ihre Ziele deckt und die eigene Müdigkeit 80 − 10 × Risiko erreicht; weißer Frieden ab Müdigkeit 50 − 10 × Risiko unter 25 Punkten, wenn die Gegenseite höchstens 10 Punkte mehr hat und die eigene Seite nicht um `kriegs_staerke` überlegen ist (Müdigkeit − 40, Vermittlung 5 × Neigung) |

Die **Annahme** eines Friedens bleibt die Regel aus militaer.md 4.5 (Kriegspunkte, Müdigkeit), ebenso
der Frieden nach Regel am Monatsersten; die KI bietet nur an, was angenommen würde. Einzige Ausnahme
(nach dem Kriegslabor): Einen **weißen Frieden** lehnt eine Gegenseite ohne Spieler zusätzlich ab, wenn
ihr Gegner kriegsmüder ist als sie oder ihre Seite um `kriegs_staerke` überlegen ist
(`ai_accepts_white_peace`); den Krieg beendet dann die Regel am Monatsersten, spätestens bei
Kriegsmüdigkeit 100. Ohne diese Ausnahme bot Frankreich im Lesezeichen Sedan nach einigen Monaten
weißen Frieden an, den das erschöpfte, aber doppelt so starke Preußen annahm: Der Verteidiger siegte
nur in 4 von 16 Läufen statt in 13, mit der Ausnahme in 14. Eine
Kriegserklärung schreibt neben der Chronik des Krieges ihre Gründe („Preußen – Gründe: Agenda
Österreich +50, Stärke gegen Österreich +12, Risiko +8“).

**Keine Verträge im Krieg** (Schritt a3, nach dem Kriegslabor): Bündnisse und Garantien bietet die KI
nicht an, solange einer der beiden Staaten im Krieg steht. Ein Bündnis mitten im Krieg machte den
Partner zur eigenen Seite (Durchmarsch und Versorgung); im Lesezeichen Königgrätz siegte Preußen dann
nur noch in 3 von 16 Läufen statt in 11.

### 6.4 Politik

- **Ereignisoptionen**: Optionen dürfen einen Block `ki { risiko = 1  reform = -1 … }` tragen; ihr
  Gewicht wird mit `1 + 0,25 × Σ Neigung × Faktor` (mindestens 0,1) multipliziert. Ohne Block bleibt
  das Gewicht aus M11 (Entscheidung 9). Die Korridore aus M11 werden mit KI neu geprüft (M12b).
  Kalibriert in b2: Bezüge je ±0,5 (Faktor also 1 + 0,125 × Σ Neigung), Vermittlung nur an außenpolitischen
  Optionen. Mit ±1 und Vermittlung an inneren Ereignissen verschob die KI die Revolution zu stark: Die vielen
  Bewahrer unter den Kleinstaaten gewährten die Märzforderungen seltener (58 % statt mindestens 61 %),
  Preußen (Machtpolitiker, Vermittlung −1) kämpfte öfter an den Barrikaden, und das Reich der Paulskirche
  kam in keinem von 16 Läufen zustande; „standhalten“ in Olmütz bekam mit Expansion den Faktor 1,55 und
  führte in 24 von 100 Läufen zum Krieg von 1850.
- **Wahlkampf der Strömungen** ohne Spieler (politik.md 9): monatlich bis zur Wahl; Mittel in die
  Wahlkreise mit der knappsten Lage nach dem Stimmungsbericht, den auch der Spieler sieht (mit
  Fehler); Rücklage 20 %.
  Präzisiert in a3: in den `wahlkampf_monate` (4) Monaten vor einer Wahl, je Strömung außer der des
  Spielers (neuer Befehl `Spielerströmung`, den die Oberfläche im Parteienmodus schickt). Je Monat
  Versammlungen in den Wahlkreisen, in denen die Strömung erste oder zweite ist, geordnet nach dem
  Abstand zwischen ihr und der stärksten anderen (mit Rauschen ±8 Punkte), so viele, wie
  (Mittel × (1 − Rücklage)) ÷ verbleibende Monate bezahlt. Der Wahlkampf steht nicht im KI-Protokoll
  (eine Strömung ist kein Staat).
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

Präzisiert in Schritt a5 (`core/src/ai_lab.cpp`, Zieldatei: chronik-skript.md Abschnitt 3.40):

- **Korridore** als Blöcke `einigung` (Ausgänge, die als Einigung zählen), `ausgang`, `krieg TAG { gegen
  = TAG }` (ein Krieg der beiden als Hauptparteien, gleich wer erklärt), `bankrott { staaten = { … } }` und
  `labor.hoechstens` (kein Ausgang in mehr Läufen; „kein Ausgang in mehr als 80 %“); Abstürze sind immer
  ein Verstoß. Die Zieldatei für 1848 entsteht mit der Kalibrierung in b2, bis dahin rechnet das Labor auf
  der Test-Fixture (`tests/daten_ki`: Brandenburg mit einer Agenda gegen Anhalt).
- **Je Lauf:** Ausgang mit Datum, Kriege mit Angreifer, Verteidiger, Beginn, Ende und Gewinner (auch
  laufende), untergegangene Staaten, Bankrotte je Staat, höchste Unruhe eines Staates an einem
  Monatsersten, Entscheidungen der KI je Art (aus dem KI-Protokoll des Monatsersten), Bündnisse am Ende,
  Laufzeit und Prüfsumme. Über alle Läufe zusätzlich Kriege je Paar und die Entscheidungen je Art.
- **Laufzeit der KI:** Die Simulation misst die Rechenzeit von Schritt 6f (`Simulation::ai_seconds`, kein
  Zustand); der Bericht nennt ihren Anteil und die Zeit je Staat und Monatserstem (Ziel: höchstens 5 ms).
  Gemessen 1848–1850 (Release, zwei Läufe gleichzeitig): 1,5 ms je Staat und Monat, 36 % der Laufzeit;
  ein Lauf bis 1871 dauert damit etwa 40 s, 100 Läufe auf vier Kernen etwa 17 Minuten. Seit b2 berechnet
  der Planer die Stärken aller Staaten einmal je Monatserstem (sie ändern sich während der Planung nicht,
  Befehle wirken am nächsten Tag) statt bei jedem Vergleich; danach 0,4 ms je Staat und Monat und rund 5 %
  der Laufzeit, bitgleich. **100 Partien 1848–1871** (b2): 24,8 Minuten auf vier Kernen, KI 3,6 % (0,2 ms je
  Staat und Monat); das Ziel von 20 Minuten verfehlt die Simulation selbst, nicht die KI.
- **`--parallel N`:** N Läufe gleichzeitig, je Lauf ein Thread (mit mehr als einem gleichzeitigen Lauf
  rechnet jede Simulation einthreadig); die Ergebnisse stehen in der Reihenfolge der Seeds, die Prüfsummen
  sind dieselben wie nacheinander. Eine Ausnahme in einem Lauf zählt als Absturz, die übrigen rechnen
  weiter.
- **Andere Labore** (Entscheidung 12): Alle Labore ohne Spieler rechnen ab 1848 mit der KI, mit einer
  Ausnahme seit b2: Das **Zolllabor** vergleicht dieselbe Partie mit und ohne einen Wechsel der Zollunion
  und rechnet deshalb ohne KI – sonst wechselten die Staaten im Vergleichslauf selbst (Preußen setzte den
  Beitritt Hannovers durch), und der Vergleich mäße nichts mehr.
- **`--ohne-ki`:** dieselbe Welt mit abgeschalteter KI (`AiRules::active` aus): keine Planer, keine Annahme
  nach Nutzen, Ereignisgewichte ohne Neigung – also wie M11.

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

Präzisiert in Schritt b3:

- **Spielerstaat:** `set_player_state` bleibt eine reine Anzeigeauswahl (welchen Staat die Oberfläche
  zeigt, seit M5). Den Befehl `Spielerstaat` schickt weiter `set_event_player` (M11): Godot ruft ihn
  beim Start, nach dem Laden und beim Staatswechsel, im Parteienmodus mit −1 (die KI führt dann alle
  Staaten). Ein zweiter Weg zum selben Befehl wäre nur eine weitere Fehlerquelle.
- **Personen** (`get_state_persons`): Herrscher und leitender Minister mit `title` (Amtstitel, sonst
  „Herrscher“ bzw. „leitender Minister“), `since_text`, `values` (Staatskunst, Diplomatie,
  Kriegskunst, Wirkung), `traits` (Namen), `inclinations` (Summe ihrer Eigenschaften) und `weight`
  (Anteil an den Neigungen des Staates: Herrscher `gewicht_herrscher_absolut` bzw.
  `gewicht_herrscher_konstitutionell`, Minister der Rest). Die **Staatsinfo** zeigt beide in der
  Faktentabelle (Eigenschaften und Werte im Tooltip) und darunter die Neigungen des Staates mit der
  Staatspersönlichkeit, z. B. „Neigungen (Machtpolitiker): Risiko −0,2 · Expansion +2,0 · …“; sie
  liest höchstens einmal je Sekunde neu.
- **Absichten** (`get_intentions`): `personality`, `inclinations` mit `inclination_keys` und
  `inclination_names`, `rival_index`, `rival_name`, `goal_text` (Hauptziel als Satz) und `goals`
  (aktive Ziele mit Gewicht, `main` beim Hauptziel); `player` meldet den Spielerstaat, für den die KI
  keine Absichten hat. Ohne eigenen Gesandten zeigt das Diplomatiefenster „Absichten: unbekannt“ mit
  dem Hinweis auf den Gesandten; die **Entscheidungen** (die letzten drei, neueste zuerst) sind
  öffentlich, weil sie Befehle sind, deren Folgen jeder sieht.
- **Chronik:** Auch Forderungen schreiben ihre Gründe in die Chronik (wie Kriegserklärungen seit a4);
  Kriegserklärungen und Forderungen an den Spielerstaat erscheinen zusätzlich als Hinweis in der
  Kopfleiste mit den Gründen der KI.
- **Krisenleiste und Kriegsfenster:** Der Tooltip des Titels nennt die Gründe der fordernden bzw.
  erklärenden Hauptpartei, gesucht in ihren letzten 20 Entscheidungen.

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

## 11. Entscheidungen (wie empfohlen freigegeben)

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
| – | Entscheidungen 1–14 wie empfohlen freigegeben; Umsetzung beginnt mit a1 |
| b3 | **erledigt:** Brücke `get_state_persons`, `get_intentions` und `get_ai_decisions` (bridge-api.md, Abschnitt KI der Staaten); Staatsinfo mit Herrscher, Minister und Neigungen; Diplomatiefenster mit Absichten (nur mit Gesandtem) und den letzten drei Entscheidungen mit „Warum?“; Gründe der Forderungen auch in der Chronik; Hinweis bei Kriegserklärungen und Forderungen an den Spieler; Gründe im Tooltip von Krisenleiste und Kriegsfenster. Der Spielerstaat bleibt bei `set_event_player` (Abschnitt 9). Godot-Tests: Rauchtest (Personen, Absichten, Entscheidungen 1848), Klicktest (Absichten, „Warum?“, Staatsinfo), Screenshots der Staatsinfo und des Diplomatiefensters; 1648 bitgleich |
| b2 | **erledigt:** Korridore `common/kiziele_voelkerfruehling.chron` (Abschnitt 7) und für 16 Partien `kiziele_voelkerfruehling_16.chron`; CI-Schritt „KI-Labor 1848–1871“ (16 Partien), Workflow `ki-partien.yml` (100 Partien, manuell und wöchentlich), CTest `ki_1848`. **100 Partien:** Einigung 67 %, kleindeutsch 44 %, zwei Bünde 23 %, Bund 33 %, großdeutsch und ohne Bund 0 %, Krieg Preußen–Österreich 70 %, keine Bankrotte, keine Abstürze; zweimal gerechnet bitgleich. Laufzeit 24,8 Minuten auf vier Kernen (Ziel 20 verfehlt: die KI braucht nur noch 3,6 % – 0,2 ms je Staat und Monat –, der Rest ist die Simulation). Kalibrierung: Stärken einmal je Monatserstem (vorher 45 % der Rechenzeit); Bündnis gegen einen gemeinsamen Rivalen nur bei Beziehung unter `rival_below` (Bündnisse am Ende 19 statt 55); Taktiker erklären bei gleich vielen Staaten je Seite (Bismarck 1866; vorher erklärte die KI nie); `ki`-Blöcke mit ±0,5 und Vermittlung nur außenpolitisch (6.4); keine Anträge auf Zolleinigung, Zollforderungen nur gegen Mitglieder einer anderen Union, Hannover im Steuerverein bis zum Septembervertrag 1851; Zolllabor ohne KI. Alle Labore 1848 mit KI in ihren Korridoren (Revolution, Strategien, deutsche Frage bis 1871, vier Kriegslabore, Zoll, Wahl, Bilanz) |
| b1 | **erledigt:** 71 Personen (25 Herrscher, 46 leitende Minister) von elf Staaten in `common/persoenlichkeiten.chron`, Wechsel der Revolution an Ereignissen (Märzministerien mit `maerzministerium`, Metternich bis zu seiner Entlassung, Maximilian II. nach der Abdankung, Bismarck mit `bismarck`); Staatspersönlichkeiten und Agenden von 14 Staaten mit 44 Zielen in `common/agenden.chron`; Blöcke `ki` an 35 Optionen von 16 Staatsereignissen (Welt-Ereignisse wie Krimkrieg und Fürstentag haben keinen Staat, der Faktor gilt dort nicht). Nach der ersten Chronik und den Godot-Tests: höchstens eine Forderung je `abkuehlung`, Zollforderungen nur an Bundesstaaten, Zolleinigung beantragt nur die Leitmacht (6.2); Zoll- und Führungsziele erst nach der Revolution (ab Juli 1849, Österreich ab März 1849). KI-Labor 1848–1871 mit 16 Seeds und den Korridoren aus Abschnitt 7 (Probe): Einigung 75 % (ohne KI 56 %), kleindeutsch 31 %, zwei Bünde 44 % (Probe bis 40 %), Bund 25 %, Krieg Preußen–Österreich 63 %, keine Abstürze; die KI erklärt noch keinen Krieg (Übermacht nach dem Bündnisfall nie erreicht), die Bündnisse sind gesättigt (55 am Ende), KI 29 % der Laufzeit. Kalibrierung mit b2 |
| a5 | **erledigt:** KI-Labor `uhrwerk-cli ki` (`core/src/ai_lab.cpp`) mit Korridoren aus `common/kiziele_<szenario>.chron` (chronik-skript.md 3.40: Einigung, Ausgänge, höchster Anteil eines Ausgangs, Kriege je Paar, Bankrotte, Abstürze), Bericht je Lauf und über alle Läufe, Laufzeit und Anteil der KI (`Simulation::ai_seconds`), `--parallel` (gleiche Prüfsummen) und `--ohne-ki`. CTests `ki_fixture` und `ki_fixture_ohne_ki` auf der Fixture `tests/daten_ki`; Tests in `test_ai.cpp`. Bündnisgrenze auch bei der Annahme (6.2); alle Labore 1848 danach neu gerechnet und in ihren Korridoren (deutsche Frage bis 1871: 25 % kleindeutsch, 12,5 % zwei Bünde, 62,5 % Bund; Sedan 87,5 %, Königgrätz 68,8 %). Die Zieldatei 1848 und der Workflow `ki-partien.yml` folgen mit b2 |
| a4 | **erledigt** (Labore mit KI: Revolution, Strategien, deutsche Frage bis 1871, die vier Kriegslabore, Zoll, Wahl und Bilanz in ihren Korridoren; weißer Frieden nach dem Sedan-Labor eingeschränkt, siehe 6.3): Kriegserklärung nur aus aktiven Agendazielen bei Übermacht der Seiten (Bündnisfall), mit doppelter Schwelle, Gründen in der Chronik und Abkühlung je Gegner; Kriegsziel aus der Agenda; weitere Kriegsziele bei Punkten über 1,5 × Kosten; Friedensangebote (eigene Ziele, weißer Frieden, Kapitulation) nur, wenn die Gegenseite annähme; keine Bündnisse und Garantien im Krieg. Regel `kriegs_staerke`; Inhaltsbereich ki Stand 4. Tests in `test_ai.cpp` |
| a3 | **erledigt:** Planer Diplomatie (Gesandte, Bündnis gegen gemeinsame Rivalen mit Annahme nach Nutzen statt „ab Beziehung 100“, Garantie für kleine Nachbarn, Forderung, Manöver, Antrag und Zollunion aus der Agenda) und Politik (Block `ki { … }` an Ereignisoptionen, Wahlkampf der Strömungen ohne Spieler); Befehl `Spielerströmung` (Brücke: `set_player_current`), im Zustand mit der KI; Inhaltsbereich ki Stand 3, ereignisse Stand 7. Tests in `test_ai.cpp` |
| a2 | **erledigt** (Labore 1848 mit KI: Revolution, Strategien, deutsche Frage bis 1871 mit 31 % kleindeutsch, 19 % zwei Bünde, 50 % Bund, Kriege, Wahl und Bilanz in ihren Korridoren; im Zolllabor die Untergrenze des Grenzhandels Mecklenburgs auf ×1,2, siehe `zollziele_voelkerfruehling.chron`): Takt am Monatsersten nach den Ereignissen (Ämter, Strategie mit Hauptziel und Rivale, Planer), Lagebild (Stärke, Bedrohung mit Feindschaft und Nähe), Nutzen als Summe benannter Faktoren mit Rauschen aus dem Strom `Ki`, Schwelle, KI-Protokoll je Staat mit drei Gründen (`ai_reason_text`, `ai_decision_text`), Abkühlung; Planer Haushalt (Kontribution und Akzise nach Defizit, Schuldenquote, Überschuss und Unruhe) und Mobilmachung (gegen bedrohliche mobile Nachbarn, Abrüstung nach zwei ruhigen Monatsersten). KI-Befehle gehen ohne Protokoll in die Warteschlange und wirken am nächsten Tag; der Spielerstaat bleibt unberührt. Zustand: Strategie, Protokoll, Abkühlungen; Inhaltsbereich Stand 2. Tests in `test_ai.cpp`; 1648 bitgleich |
| a1 | **erledigt:** Regeln, sechs Staatspersönlichkeiten und 16 Eigenschaften in `common/ki.chron` (chronik-skript.md 3.37), Personen mit Amt, Amtszeit, Bedingung, Werten und Eigenschaften (`common/persoenlichkeiten.chron`, 3.38), Agenden mit Staatspersönlichkeit und Zielen (`common/agenden.chron`, 3.39) – geladen nur mit Ereignissen und ab `seit`; Neigungen (`state_inclinations`) und aktive Ziele (`agenda_goal_active`); Ämter je Staat und Rolle, beim Start ohne und am Monatsersten mit Eintrag in der Chronik; Befehl `Spielerstaat` (vorher `Ereignisspieler`); Strom `Ki` im Zustand; Inhaltsbereich `ki`, Spielstand Format 6; `uhrwerk-cli info` mit Zeile „KI“. Personen und Agenden 1848–1871 folgen mit b1. Tests in `test_ai.cpp`; 1648 bitgleich |
