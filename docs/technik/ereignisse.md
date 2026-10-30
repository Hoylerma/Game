# Ereignisse und Revolution 1848/49 (Meilenstein M11)

> **Status: verbindlich** – Entscheidungen 1–13 wie empfohlen freigegeben (Abschnitt 11); Umsetzung
> läuft, Stand in Abschnitt 13. Grundlage sind
> [18 Ereignisse, Journale, Missionen](../18_ereignisse_journale_missionen.md) (Abschnitte 1–4, 6–9),
> [09 Politik, Staat und Gesetze](../09_politik_staat_und_gesetze.md) (Abschnitte 3, 5, 6 und 13),
> [20 Verantwortung und Erinnerungskultur](../20_verantwortung_und_erinnerungskultur.md) und der
> Meilensteinplan ([meilensteine.md](../meilensteine.md#m11--ereignisse-und-revolution-184849)).
> Politik und Wahlen stehen in [politik.md](politik.md), Diplomatie und Bundesversammlung in
> [diplomatie.md](diplomatie.md), Krieg und Frieden in [militaer.md](militaer.md), die Syntax in
> [chronik-skript.md](chronik-skript.md).

Im März 1848 gehen in fast allen deutschen Staaten die Regierungen vor den Forderungen von
Versammlungen, Petitionen und Barrikaden zurück: Märzministerien, Pressefreiheit, Bauernbefreiung,
ein Parlament für ganz Deutschland. Ein gutes Jahr später ist das meiste davon zurückgenommen. Der
König von Preußen lehnt die Kaiserkrone der Paulskirche ab, die Aufstände für die Reichsverfassung
werden niedergeschlagen, und 1851 tagt wieder die Bundesversammlung. Ob es so kommen musste, ist
die Frage, die das Lesezeichen 1848 dem Spieler stellt.

M11 bringt dafür in die Simulation:
- ein **Ereignissystem** im Chronik-Skript: Auslöser, Optionen, Folgen, für den Spielerstaat als
  Entscheidung und für alle anderen Staaten nach Regel;
- die Größen, auf die Ereignisse wirken: **Gesetze** (Presse, Vereine, Grundlasten), die
  **Zustimmung** der Interessengruppen und die **Unruhe** je Staat;
- **Journale** als Zustandsautomaten: die **Revolution 1848/49** mit offenem Ausgang und die
  **deutsche Frage** bis 1871;
- **mindestens 80 Ereignisse** 1848–1871 mit Quellenangaben.

Die KI der Staaten (M12) und der durchgehend spielbare Zeitraum 1848–1871 (M13) bauen darauf auf.

## 1. Umfang

Der Meilensteinplan verlangt:

- ein Ereignissystem im Chronik-Skript (Bedingungen, Auswahlmöglichkeiten, Folgen);
- Märzrevolution, Paulskirche und Verfassungskonvent, Reichsverfassungskampagne, Olmütz;
- das Journal „Deutsche Frage“ mit offenen Ausgängen;
- die Leitlinien aus Kapitel 20 beachten;
- Abnahme: mindestens 80 Ereignisse für 1848–1871 mit Quellenangaben; die Revolution kann scheitern
  oder gelingen, beide Wege sind spielbar.

Vorschlag (Entscheidung 1): **zwei Teile** wie bei M7 bis M10.

| Teil | Inhalt | Ergebnis |
|---|---|---|
| **M11a** | Ereignisse im Chronik-Skript (Lader, Bedingungen, Wirkungen, Auslösung, Wahl nach Regel, Befehl für den Spieler), Gesetze, Zustimmung, Unruhe, Aufstände, Journale als Zustandsautomaten; Ereignislabor auf der Test-Fixture | Ereignisse im Kern, prüfbar ohne Oberfläche |
| **M11b** | Inhalt: Revolution 1848/49 und deutsche Frage als Journale, die Ereignisse 1848–1871, Kalibrierung mit dem Ereignislabor; Ereignis- und Journalfenster in Godot; Doku und Review | Abnahme: 80 Ereignisse, die Revolution kann scheitern oder gelingen |

**Nicht in M11:**
- **Gesetzgebungsverfahren** mit Lesungen und Mehrheiten (Kapitel 09, Abschnitt 6.2). Gesetze
  ändern sich in M11 durch Ereignisse und Entscheidungen; ein Landtag stimmt nicht ab.
- **Persönlichkeiten** mit Laufbahn und Eigenschaften (Kapitel 13). Ereignisse nennen historische
  Personen im Text (Friedrich Wilhelm IV., Heinrich von Gagern, Robert Blum), ohne dass sie Werte
  hätten.
- **Missionsbäume** und **Wendepunkte** als eigener Bildschirm (Kapitel 18, Abschnitte 5 und 6):
  Die großen Entscheidungen von 1848/49 (Märzforderungen, Kaiserkrone, Olmütz) sind Ereignisse mit
  eigener Kennzeichnung; der Wendepunkt-Bildschirm kommt mit M13.
- **Lexikon** (Kapitel 18, Abschnitt 9.2). Jedes Ereignis trägt eine Quelle; Lexikonartikel folgen.
- **Bilder** zu den Ereignissen; M11 zeigt Text (Bildrechte und Auswahl gehören zu M13).
- **Lokalisierung:** Texte stehen deutsch in den Daten; Schlüssel für Englisch folgen mit M13.
- Ereignisse vor 1848 (1648, Testregion) und nach 1871.
- **Eigener Antrieb der Staaten** über die Ereignisse hinaus (M12): Staaten ohne Spieler wählen
  Optionen nach Gewichten, sie verfolgen keine Ziele.

## 2. Ausgangslage

**Vorhanden:**
- **Lesezeichen „Völkerfrühling“** ab dem 1. März 1848: 38 Staaten, 3.967 Orte mit Daten,
  Bevölkerung 1846. Verfassungen zum Start: 18 Staaten `konstitutionell` (Baden, Bayern, Württemberg,
  Hannover, Sachsen, Kurhessen, Hessen-Darmstadt, Nassau, thüringische und kleinere Staaten; davon
  drei mit Zensuswahlrecht), 13 `staendisch` (darunter Preußen mit dem Vereinigten Landtag, Österreich,
  die Hansestädte, Mecklenburg, Dänemark für Holstein und Lauenburg), 7 ohne Verfassung (u. a.
  Oldenburg, Hohenzollern, Reuß).
- **Politik (M8):** fünf Strömungen (Konservative, Katholiken, Liberale, Demokraten, Arbeiter), elf
  Interessengruppen mit Mitgliedern, Anteil, Einfluss und Neigung, aber **ohne Zustimmung**
  (Entscheidung 6 in M8: „Zustimmung und Aktionen kommen mit M11 und M12“). Verfassung, Wahlrecht
  und Wahlsystem sind datierte Felder ohne Gesetzgebung („Zugeständnisse folgen mit den Ereignissen
  in M11“, politik.md Abschnitt 6). Die **Wahl zur Nationalversammlung** am 1. Mai 1848 mit 633
  Wahlkreisen, 649 Sitzen und acht Fraktionen der Paulskirche (Café Milani bis Donnersberg); die
  Preußische Zweite Kammer am 17. Juli 1849 mit Dreiklassenwahlrecht. **Keine Abstimmungen** in den
  Parlamenten. Parteienmodus mit Wahlkampf.
- **Diplomatie (M9):** Bundesversammlung in Frankfurt mit Anträgen und Abstimmungen (sie **tagt
  durchgehend**, die Suspendierung 1848–1851 ist als Grenze offen), Beziehungen, Bündnisse,
  diplomatische Spiele, Reiter „Deutsche Frage“ mit Lagern, Zollverein, Reformen und Krisen
  **ohne Wertung**.
- **Militär (M10):** Heere, Mobilmachung, Krieg mit Bündnisfall, Kriegsziele, Frieden mit
  Besitzwechsel, Generalstab; Kriege ohne Spieler nur aus Lesezeichen, Krisen und
  Bundesbeschlüssen. Drei Kriegslesezeichen 1864, 1866, 1870; die Historie 1849–1870 mit dem Ende
  des Bundes und dem Norddeutschen Bund (Bundesrat) **gilt nur beim Laden** eines Lesezeichens.
- **Wirtschaft (M5):** Lebensstandard je Block (1848 im Median unten 44,5, mittel 53,2, oben 79,0
  über 30 Jahre), Preise je Markt, Auswanderung im Bevölkerungsmodell (M4).
- **Zufallsstrom `Ereignisse`** (seit M1, Seuchen der Testregion 1648); Chronik mit Ort und Staat.
- **Datierte Änderungen** greifen zur Laufzeit nur für Inhalt mit `seit`/`bis` (Bahnstrecken,
  Arbeiterbewegung ab August 1848, Bundesversammlung bis 1866); Staatsfelder gelten zum Startdatum.

**Es fehlt:** Ereignisse und Entscheidungen; Gesetze außer Verfassung und Wahlrecht; Zustimmung der
Gruppen; eine Größe für Unruhe und revolutionären Druck; Aufstände; Abstimmungen in der Paulskirche;
die Zentralgewalt 1848/49 und das Ruhen der Bundesversammlung; ein Ausgang der deutschen Frage;
Texte mit Quellen.

**Quellen:** wie in M7 bis M10 ohne Seitenzugriff. Daten und Verlauf 1848–1871 sind gut bekannt
(Überblicke: Siemann, *Die deutsche Revolution von 1848/49*; Nipperdey, *Deutsche Geschichte
1800–1866*; Huber, *Deutsche Verfassungsgeschichte* Bd. 2 und 3; Mommsen, *1848 – Die ungewollte
Revolution*; Hachtmann, *Berlin 1848*; Wehler, *Deutsche Gesellschaftsgeschichte* Bd. 2 und 3).
Jedes historische Ereignis nennt sein Datum und eine Quelle; Wirkungen und Gewichte sind Spielwerte
(`spielwert`), Daten und Zahlen `ungeprueft` (Entscheidung 12).

## 3. Ereignisse im Chronik-Skript (M11a)

### 3.1 Aufbau

Ereignisse stehen in `common/ereignisse/*.chron`, eine Datei je Thema (`maerz_1848.chron`,
`paulskirche.chron`, `reaktion.chron` …). Die Syntax bleibt die des Chronik-Skripts
(chronik-skript.md, Abschnitt 1): keine neue Sprache, sondern ein **festes Vokabular** von
Bedingungen und Wirkungen als Blöcke (Entscheidung 2).

```text
ereignis maerz.forderungen {
    titel  = "Die Märzforderungen"
    text {                            # Zeilen eines Absatzes, mit Leerzeichen verbunden (3.8)
        "In Mannheim hat eine Volksversammlung Pressefreiheit, Schwurgerichte, Volksbewaffnung"
        "und ein deutsches Parlament gefordert. Petitionen mit Tausenden Unterschriften liegen"
        "vor dem Schloss."
    }
    datum  = 1848.02.27               # historisches Datum (Anzeige, Kompass)
    quelle = "Siemann 1985, S. 66 f."
    art    = historisch               # historisch | dynamisch | atmosphaerisch
    wendepunkt = ja                   # große Entscheidung (Kennzeichnung im Fenster)

    wer = staat                       # staat: je Staat geprüft | welt: einmal für alle
    bedingung {
        datum >= 1848.03.01
        datum <  1848.06.01
        gesetz_presse = vorzensur
        unruhe >= 30
        nicht { geschehen = maerz.forderungen }  # je Staat einmal
    }
    wartezeit = 10                    # mittlere Tage bis zum Eintritt, sobald die Bedingung gilt

    option gewaehren {
        text = "Gewähren: ein Märzministerium berufen"
        historisch = ja
        gewicht = 70                  # Wahl ohne Spieler (3.5)
        wirkung {
            gesetz_presse = pressefreiheit
            gesetz_vereine = vereinsfreiheit
            zustimmung_bildungsbuergertum = 3
            zustimmung_adel = -2
            unruhe = -20
            merke = maerzministerium   # Flag am Staat
        }
    }
    option hinhalten {
        text = "Hinhalten: eine Kommission einsetzen"
        gewicht = 20
        wirkung { unruhe = 10  folge { ereignis = maerz.petitionen  tage = 14 } }
    }
    option militaer {
        text = "Das Militär einsetzen"
        gewicht = 10
        bedingung { zustimmung_militaer >= 0 }
        wirkung { zustimmung_militaer = 2  zustimmung_bildungsbuergertum = -4  aufstand = barrikaden }   # ab Schritt a3 (4.4)
    }
}
```

Jede Option zeigt im Fenster ihre **Sofortfolgen** (aus den Wirkungen erzeugt: „Pressefreiheit,
Bildungsbürgertum +3, Adel −2, Unruhe −20“) und, falls sie Folgeereignisse auslöst, den Hinweis
„Diese Entscheidung wird Folgen haben“ (Kapitel 18, Gestaltungsregel 2).

### 3.2 Bedingungen

Eine Bedingung ist ein Block von Prüfungen, die alle gelten müssen; `oder { … }` und `nicht { … }`
verknüpfen. Bedingungen lesen nur, sie ändern nichts.

Die Kennung einer Gruppe, eines Gesetzes, Gutes oder Staates hängt mit `_` am Schlüssel
(`zustimmung_militaer >= 0`), weil `WORT WORT = wert` keine Form des Chronik-Skripts ist
(chronik-skript.md 1.2). Vokabular, Wertebereiche und Prüfungen des Laders: chronik-skript.md 3.34.

| Prüfung | Bedeutung |
|---|---|
| `datum >= / < JJJJ.MM.TT` | Tagesdatum |
| `staat = TAG`, `staat_in = { … }` | nur für diese Staaten (bei `wer = staat`) |
| `geschehen = <ereignis>`, `geschehen { ereignis = <id> option = <name> }` | das Ereignis ist für diesen Staat (bzw. die Welt) eingetreten, mit der gewählten Option |
| `merkmal = <name>`, `welt_merkmal = <name>` | Flag am Staat bzw. in der Welt (3.7) |
| `journal_<id> = <stufe>` | Stufe eines Journals (Abschnitt 5, ab Schritt a4) |
| `gesetz_<gruppe> = <option>` | Gesetz des Staates (4.1); auch `verfassung`, `wahlrecht` |
| `zustimmung_<gruppe> >= n` | Zustimmung einer Interessengruppe (4.2) |
| `anteil_<gruppe> >= x`, `einfluss_<gruppe> >= x` | Anteil und Einfluss (M8) |
| `unruhe >= n`, `nachbar_unruhe >= n` | Unruhe des Staates bzw. eines Nachbarstaates (4.3) |
| `lebensstandard_unten <= n` | Lebensstandard der unteren Schicht |
| `preis_<gut> >= x` | Preisfaktor im Markt der Hauptstadt |
| `im_krieg = ja`, `krieg_mit = TAG`, `mobil = ja` | Krieg und Mobilmachung (M10) |
| `krieg_gewonnen = TAG`, `krieg_verloren = TAG` | Ausgang eines beendeten Krieges (seit b3) |
| `bund = ja`, `zollunion = <id>`, `beziehung_<TAG> >= n` | Bund, Zollunion, Beziehung (M7, M9) |
| `sitze { parlament = <id> stroemung = <id> mindestens = n }` | Sitze nach der letzten Wahl (M8) |
| `soldaten >= n`, `schatz >= n`, `schulden_quote >= x` | Heer und Haushalt |
| `spieler = ja` | der Staat ist der Spielerstaat (nur für Texte und Kompass, nicht für Auslöser) |

### 3.3 Wirkungen

Wirkungen ändern den Zustand am Tag der Wahl; alle sind deterministisch, Zufall nur über
`zufall { … }` aus dem Strom `Ereignisse`.

| Wirkung | Bedeutung |
|---|---|
| `gesetz_<gruppe> = <option>`, `verfassung = …`, `wahlrecht = …` | Gesetz bzw. Verfassungsfeld ändern (4.1) |
| `zustimmung_<gruppe> = ±n` | Zustimmung einer Gruppe ändern (Ergebnis −10 … +10) |
| `unruhe = ±n` | Unruhe des Staates ändern |
| `merke = <name>`, `vergiss = <name>`, `welt_merke = <name>` | Flag setzen bzw. löschen |
| `befristet { … tage = n }` (ab Schritt a2) | Wirkung auf Zeit (3.7): Steuerertrag, Moral, Unruhe je Monat |
| `schatz = ±n`, `monatseinnahmen = ±x` | Geld (auch als Vielfaches der Monatseinnahmen) |
| `beziehung_<TAG> = ±n` | Beziehung zu einem Staat (M9) |
| `auswanderung = x` | Anteil der unteren Schicht wandert aus (über das Bevölkerungsmodell, M4) |
| `verwuestung = x`, `verwuestung { ort = <id> wert = x }` | Verwüstung (M1) |
| `aufstand = <art>` | Aufstand beginnen (4.4, ab Schritt a3) |
| `mobilmachung = ja`/`nein`; `krieg = TAG` (seit a4, ohne Kriegsziel), `krieg { gegner = TAG … }` (seit b3, mit Kriegszielen) | Militär (M10), mit Bündnisfall; den Frieden schließt der Krieg selbst (M10) |
| `antrag { art = … ziel = … }`, `bundesversammlung = ruht`/`tagt` (seit a4) | Bundesversammlung (M9) und ihr Ruhen 1848–1851 |
| `journal_<id> = <stufe>` (seit a4) | Journal weiterschalten (Abschnitt 5) |
| `abstimmung = <id>` (seit a4) | Abstimmung in einem Parlament auslösen (5.3) |
| `folge { ereignis = <id> tage = n }`, `folge { ereignis = <id> staat = TAG }` | Folgeereignis, auch für einen anderen Staat |
| `chronik = "…"` | eigener Chronikeintrag (sonst schreibt die gewählte Option ihren Text) |

### 3.4 Auslösung

- **Täglich** prüft der Kern nach Wirtschaft, Politik und Militär die Ereignisse, deren
  Datumsfenster offen ist, in der Reihenfolge ihrer Kennung, je Staat in der Reihenfolge der
  Staaten (Determinismus).
- Gilt die Bedingung, tritt das Ereignis mit der Wahrscheinlichkeit `1 / wartezeit` je Tag ein
  (mittlere Wartezeit wie `mean_time_to_happen`); `wartezeit = 0` heißt sofort. Gezogen wird aus dem
  Strom `Ereignisse`, je Ereignis und Staat eine Ziehung, nur wenn die Bedingung gilt.
- Ein Ereignis tritt je Staat (bzw. für die Welt) **einmal** ein, außer es trägt
  `wiederholbar = ja` mit `abstand = n` Tagen (dynamische Ereignisse).
- **Feste Termine** (`termin = 1848.05.18`) treten an genau diesem Tag ein, wenn die Bedingung gilt
  (z. B. Eröffnung der Paulskirche); sonst verfallen sie.
- Folgeereignisse (`folge`) treten nach ihrer Frist ein, wenn ihre Bedingung dann gilt, auch wenn das
  Ereignis schon eingetreten ist; `nur_folge = ja` heißt, dass ein Ereignis nur so eintritt. Eine Folge
  ohne Frist (`tage = 0`) tritt am selben Tag ein.
- **Umsetzung (seit a2, `core/src/events.cpp`):** Reihenfolge im Tag nach dem Militär: befristete
  Wirkungen (Ablauf, am Monatsersten Unruhe und Beziehung), fällige Fristen des Spielers, fällige
  Folgeereignisse, Auslösung, dann die Folgen ohne Frist aus der Auslösung. Prüfungen des Datums auf
  oberster Ebene der Bedingung schließen das Fenster für alle Staaten zugleich; Staaten ohne Orte mit
  Bevölkerung nehmen nicht teil. Die Ziehung je Ereignis und Staat (`1 / wartezeit`) kommt nur, wenn
  die Bedingung gilt; am Termin und ohne Wartezeit gibt es keine.

### 3.5 Wahl ohne Spieler

Staaten ohne Spieler wählen eine Option **nach Gewicht** (`gewicht`), gezogen aus dem Strom
`Ereignisse`; nicht verfügbare Optionen (eigene `bedingung`) zählen nicht. Gewichte dürfen vom
Zustand abhängen (Entscheidung 4):

```text
gewicht = 40
gewicht_wenn { unruhe >= 60  plus = 30 }       # unter Druck eher nachgeben
gewicht_wenn { zustimmung_militaer >= 5  plus = -20 }
```

Die historische Option hat in der Regel das größte Gewicht, aber nicht alle (Abnahme: „nicht
festgelegt“). Welt-Ereignisse (`wer = welt`) ohne Staat wählen ebenso.

### 3.6 Wahl des Spielers

- Ein Ereignis des Spielerstaates (bzw. im Parteienmodus seiner Strömung, Entscheidung 9) **wartet**
  auf die Wahl: Der Kern legt es in die Liste offener Ereignisse; Godot hält die Zeit an und öffnet
  das Fenster (Abschnitt 9).
- Der Spieler wählt per **Befehl `Ereigniswahl`** (Wert = Ereignis, Ziel = Option). Wirksam ab dem
  nächsten Tag wie alle Befehle, im Protokoll für die Wiederholung (M6). Welcher Staat wartet, setzt
  der **Befehl `Ereignisspieler`** (Staat oder keiner; `World::event_player`, Zustand): Die Oberfläche
  schickt ihn beim Start einer Partie, die Kommandozeile und die Labore spielen ohne ihn. Eine Option,
  deren Bedingung nicht gilt, lässt sich nicht wählen; Folgeereignisse des Spielerstaates warten
  ebenfalls.
- Ohne Wahl gilt nach `frist` Tagen (Standard 30) die Option mit dem größten Gewicht, damit Läufe
  ohne Oberfläche (Kommandozeile, Labore) weiterlaufen. Godot lässt die Frist nicht verstreichen,
  weil die Zeit steht.
- Der **historische Kompass** markiert die Option mit `historisch = ja` (abschaltbar in Godot).

### 3.7 Flags und befristete Wirkungen

- **Flags** (`merke`) sind Namen am Staat oder in der Welt; sie halten fest, was geschah
  (`maerzministerium`, `kaiserkrone_abgelehnt`), und tragen Bedingungen späterer Ereignisse.
- **Befristete Wirkungen** sind Modifikatoren mit Ablaufdatum: Steuerertrag ±x, Moral der Heere ±x,
  Unruhe je Monat ±n, Beziehung zu allen Bundesstaaten ±n. Sie stehen mit Text in der Staatsinfo
  („Belagerungszustand in Berlin, bis 28. Juli 1849“). Seit a2 (`befristet { … }`, chronik-skript.md
  3.34): Der Steuerertrag wirkt im Haushalt auf alle drei Steuerarten, die Moral als Faktor auf die
  Kampfkraft der Heere des Staates, Unruhe und Beziehung am Monatsersten; die Wirkung endet am Tag
  `bis` (`World::timed_effects`, Zustand).
- **Chronik:** Eine gewählte Option schreibt „Staat: Titel – Optionstext“, außer sie hat eine eigene
  Wirkung `chronik = "…"`.

### 3.8 Lader und Prüfungen

- Jede Kennung einmal; jedes Ereignis mit Titel, Text, `quelle` (Pflicht für `art = historisch`),
  mindestens einer Option; jede Option mit Text und Gewicht ≥ 0, höchstens eine `historisch = ja`.
- Verweise auf Ereignisse, Gruppen, Gesetze, Gesetzesoptionen, Strömungen, Parlamente, Staaten, Orte,
  Journale und Stufen werden beim Laden geprüft (Meldungen wie im Lader üblich).
- Folgeketten ohne Bedingung dürfen keinen Kreis bilden (`pruefen` meldet ihn).
- `uhrwerk-cli pruefen` prüft die Ereignisse, `uhrwerk-cli info` zählt sie je Art und Jahrzehnt.
- **Lange Texte:** `text` ist eine Zeichenkette oder ein Block aus Zeichenketten, die mit Leerzeichen
  verbunden werden; eine leere Zeichenkette `""` beginnt einen neuen Absatz. Die Syntax bleibt
  unverändert (Zeichenketten ohne Zeilenumbruch, chronik-skript.md 1.1).

## 4. Gesetze, Zustimmung, Unruhe (M11a)

### 4.1 Gesetze

M11 führt drei Gesetzesgruppen aus Kapitel 09 (Abschnitt 6.1) ein, die 1848 im Mittelpunkt stehen,
neben den vorhandenen Feldern `verfassung`, `wahlrecht` und `wahlsystem` (Entscheidung 3):

| Gruppe | Optionen | 1848 zum Start (Vorschlag) | Wirkung im Modell |
|---|---|---|---|
| `presse` (12) | `vorzensur`, `pressegesetz`, `pressefreiheit` | Vorzensur in den Bundesstaaten (Karlsbader Beschlüsse 1819); wo sie Anfang März 1848 schon fiel, setzt das erste Ereignis die Pressefreiheit | Unruhe (Zensur +), Zustimmung Bildungsbürgertum und Nationalbewegung |
| `vereine` (13) | `verbot`, `genehmigung`, `vereinsfreiheit` | Verbot politischer Vereine (Bundesbeschluss 1832) | Wahlkampf der Strömungen (M8, Verein gründen nur mit Vereinsfreiheit), Unruhe |
| `grundlasten` (22) | `grundherrschaft`, `abloesung`, `frei` | Ablösung begonnen in Preußen (seit 1807/1821), Sachsen (1832), Hannover (1833), Baden und Württemberg; Grundherrschaft in Österreich (bis 7.9.1848), Bayern (bis 1848) und Mecklenburg | Lebensstandard der Bauern (Abgaben), Zustimmung Bauernschaft und Adel, Agrarunruhen |

Die Werte zum Start stehen datiert in `staaten_historie.csv` (Archivar, Quelle und Sicherheit je
Angabe). Die Verfassungsfelder werden zur Laufzeit änderbar: Ein Ereignis setzt z. B.
`verfassung = konstitutionell` (Preußen 5.12.1848) oder `wahlrecht = dreiklassen` (30.5.1849); die
nächste Wahl rechnet damit (M8).

### 4.2 Zustimmung der Interessengruppen

Je Staat und Gruppe eine **Zustimmung** von −10 bis +10 (Kapitel 09, Abschnitt 5.2), Zustand,
Start 0. Sie entsteht aus:
- **Ereignissen** (`zustimmung <gruppe> = ±n`);
- **Gesetzen**: jede Gruppe hat in `interessengruppen.chron` Wünsche je Gesetzesgruppe (z. B.
  Bildungsbürgertum: Pressefreiheit +2, Vorzensur −2; Adel: Grundherrschaft +2, frei −3). Die
  Zustimmung nähert sich je Monat um ein Fünftel dem **Zielwert** aus den Gesetzen; Ereignisse
  verschieben sie zusätzlich.

Die Zustimmung wirkt auf die Unruhe (4.3), auf Aufstände (4.4, Militär und Beamtenschaft) und auf
die Gewichte der Optionen. Auf die Wahlen wirkt sie in M11 nicht (Kalibrierung M8 bleibt).

### 4.3 Unruhe

Je Staat eine **Unruhe** von 0 bis 100: der revolutionäre Druck, Zustand (Entscheidung 5). Sie
nähert sich je Monat um ein Viertel einem **Zielwert** und springt durch Ereignisse.

```
Ziel = 20 × Not + 15 × Teuerung + Verfassungsmangel + Zensur + Ansteckung − Zustimmung
Not              = max(0, 50 − Lebensstandard unten) ÷ 25       (0 … 2)
Teuerung         = max(0, Getreidepreis − 1,2) ÷ 0,5               (0 … 2, Markt der Hauptstadt)
Verfassungsmangel = keine 15, staendisch 10, konstitutionell 0
Zensur           = Vorzensur 10, Pressegesetz 5, Pressefreiheit 0
Ansteckung       = 10 × Anteil der Nachbarstaaten mit Unruhe ≥ 60 (Welt-Merkmal
                   „Februarrevolution“ zählt als Nachbar mit 80)
Zustimmung       = Σ Einfluss × Zustimmung über alle Gruppen (−10 … +10) × 2
```

**Umsetzung (seit a3, `core/src/events.cpp`):** Zustimmung und Unruhe nähern sich am Monatsersten ihren
Zielwerten, für alle bestehenden Staaten zugleich: zuerst die Zustimmung, dann die Unruhe, deren Zielwerte
alle aus dem Stand vor der Annäherung entstehen (die Ansteckung hängt nicht von der Reihenfolge der
Staaten ab). Not aus dem Lebensstandard der unteren Schicht (nach Köpfen), Teuerung aus dem Preisfaktor
des Getreides im Markt der Hauptstadt, Zensur aus dem Feld `unruhe` der Gesetzesoptionen
(`gesetze.chron`), Nachbarn über die Nachbarschaft der Orte (nur mit Militärdaten), Einfluss der Gruppen
wie in M8. Alle Faktoren stehen als Spielwerte in `ereignisse.chron` (chronik-skript.md 3.34). Ohne
Ereignisse und ohne Spielereingriff liegt die Unruhe 1848 nach einem Jahr zwischen 12 und 37 (vor der
Kalibrierung).

Spielwerte; die Kalibrierung (M11b) stellt sie so ein, dass im März 1848 die meisten Staaten
Unruhe ≥ 40 haben und ohne Zugeständnisse bis zum Sommer über 60 steigen. Wirkungen:
- Unruhe ≥ 50: Steuerertrag −10 % (Steuerverweigerung), Auswanderung der unteren Schicht +50 %;
- Unruhe ≥ 70: Aufstandsgefahr (Ereignisse, die einen Aufstand beginnen können);
- Kartenmodus „Unruhe“ (Abschnitt 9).

### 4.4 Aufstände

Ein **Aufstand** ist ein Zustand je Staat mit Art (`barrikaden`, `agrarunruhen`, `erhebung`),
Beginn und Stärke, ohne Heere auf der Karte (Entscheidung 7). Er dauert 7 bis 60 Tage und endet nach
dem Erfolgsmodell aus Kapitel 09, Abschnitt 13.1:

| Faktor | Größe im Modell |
|---|---|
| Mobilisierung | Unruhe × Anteil der Städter, Bauern (Agrarunruhen) bzw. aller (Erhebung) |
| Elitenspaltung | Zustimmung von Beamtenschaft und Adel < 0, Märzministerium im Amt |
| Loyalität der Truppen | Zustimmung des Militärs, Soldaten unter Waffen je 1.000 Einwohner |
| Rückendeckung von außen | Intervention eines anderen Staates (Ereignisoption): seine Soldaten des nächsten Heeres zählen mit |

Jede Woche vergleicht der Kern Mobilisierung und Elitenspaltung mit Loyalität und Intervention, mit
Zufall aus dem Strom `Ereignisse`. Ausgänge:
- **Die Regierung gibt nach**: Ereignis mit Zugeständnissen (Gesetze, Verfassung, Ministerwechsel),
  Unruhe −30;
- **Niederschlagung**: Ereignis „Standrecht“ bzw. „Belagerungszustand“, Unruhe −20 auf Zeit,
  Zustimmung Bildungsbürgertum, Demokraten-Neigung, Auswanderung +, Verwüstung in der Hauptstadt,
  Gefallene als Chronik (keine Kennzahl, Kapitel 20);
- **Die Aufständischen siegen**: Flucht des Fürsten, Revolutionsregierung als Merkmal (Baden 1849),
  bis eine Intervention sie beendet.

Die Option, einem anderen Staat mit Truppen zu helfen („Preußen schickt Truppen nach Baden“), ist
ein Ereignis des Helfers; mit M10 macht er mobil, seine Heere marschieren aber nicht (Entscheidung 7).

**Umsetzung (seit a3):** Ein Aufstand beginnt mit der Wirkung `aufstand = <art>` (je Staat höchstens
einer) und hat eine Stärke, anfangs 0. Ab dem siebten Tag (`aufstand_tage`) rechnet der Kern jede Woche:

```
Aufständische = Unruhe ÷ 100 × (½ + Anteil Städter | Bauern | 1) + Spaltung × Treffer
Treffer       = Gruppen aus spaltung_gruppen mit Zustimmung < 0, dazu das Merkmal spaltung_merkmal
Regierung     = (Zustimmung der Gruppe loyalitaet + 10) ÷ 20 × min(2, Soldaten je 1.000 ÷ soldaten_bezug)
              + min(2, Soldaten des Helfers je 1.000 Einwohner ÷ soldaten_bezug)
Stärke       += aufstand_schritt × (Aufständische − Regierung) ± aufstand_zufall (Strom Ereignisse)
```

Bei Stärke ≥ 1 siegen die Aufständischen (Merkmal `revolutionsregierung`), bei ≤ −1 wird der Aufstand
niedergeschlagen (Unruhe −20, Verwüstung der Hauptstadt +0,05); sonst gibt die Regierung bei positiver
Stärke mit der Wahrscheinlichkeit Stärke × `aufstand_nachgeben` nach (Unruhe −30). Nach der Höchstdauer
endet er mit Nachgeben (Stärke > 0) oder Niederschlagung. Soldaten des Helfers: sein größtes Heer, ohne
Militär ein Viertel seines Friedensheeres. Jeder Ausgang schreibt einen Chronikeintrag und kann ein
Folgeereignis haben (`folge_nachgeben`, `folge_niederschlagung`, `folge_sieg`), das die eigentlichen
Zugeständnisse, das Standrecht oder die Flucht des Fürsten mit Optionen erzählt. Wirkungen der Unruhe:
Steuerverweigerung im Haushalt (alle Steuerarten), zusätzliche Auswanderung der unteren Schicht in der
Bevölkerungsrechnung. Seit b3 rüstet der Helfer nach dem Ende des Aufstands ab, wenn er nicht im Krieg
steht und keinem anderen Aufstand mehr Truppen stellt (vorher blieb Preußen nach dem März 1848 bis zum
nächsten Frieden mobil).

## 5. Journale (M11a/b)

### 5.1 Journal als Zustandsautomat

Ein **Journal** (`common/journale/*.chron`) hat **Stufen** und **Ausgänge**. Eine Stufe beschreibt
Text, Ziele (angezeigte Fortschritte), die Ereignisse, die in ihr möglich sind (über
`journal <id> = <stufe>` in deren Bedingungen), und Übergänge mit Bedingung. Ein Ausgang ist eine
Endstufe mit Text, Wertung und Folgen. Die Stufe ist Zustand; jeder Wechsel schreibt die Chronik.

```text
journal revolution {
    titel = "Revolution 1848/49"
    beginn = 1848.03.01
    stufe maerz { titel = "Märzunruhen"  weiter { zu = vorparlament  wenn { welt_merkmal = vorparlament } } }
    stufe vorparlament { … }
    …
    ausgang reich_paulskirche { titel = "Reichsverfassung in Kraft"  wertung = gelungen  … }
    ausgang gegenrevolution   { titel = "Die Gegenrevolution siegt"    wertung = gescheitert … }
}
```

### 5.2 Revolution 1848/49

Nach Kapitel 18, Abschnitt 7, auf die Daten des Lesezeichens gelegt:

```
 [Märzunruhen 3/1848] ── Märzforderungen je Staat (gewähren · hinhalten · Militär)
        │                         │                   │
        │                   Märzministerien     Barrikaden (Berlin 18.3., Wien 13.3.)
        ▼                         ▼                   ▼
 [Vorparlament 31.3.] ◄───────────┴───────────────────┘
        │   (Heckerzug 4/1848: Aufstand Baden)
        ▼
 [Nationalversammlung] ← Wahl 1.5.1848 (M8), Eröffnung 18.5., Reichsverweser 29.6.,
        │                 Bundesversammlung ruht (12.7.)
        │   Malmö (8/1848) · Septemberunruhen · Wiener Oktoberaufstand · Preußen: Wrangel (11/1848)
        ▼
 [Verfassungskonvent] ← Grundrechte (12/1848), Abstimmungen: großdeutsch/kleindeutsch, Wahlrecht,
        │                 Erbkaiser (5.3)
        ▼
 [Kaiserkrone 3/1849] ── Preußen: annehmen · ablehnen · Bedingungen stellen
        │                       │                 │
        ▼                       ▼                 ▼
 Reich der Paulskirche   [Reichsverfassungs-   Verhandlungen mit den Fürsten
 (Österreich, Krise)      kampagne 5/1849]     (Erfurter Union)
                              │
               ┌──────────────┼──────────────────┐
               ▼              ▼                  ▼
        Aufstände siegen   Niederschlagung    Rumpfparlament
        (Republik, Bürger- (Rastatt 7/1849)   (Stuttgart)
        krieg)                │
                              ▼
                     [Union oder Bund 1850] ── Olmütz: nachgeben · Krieg (M10)
                              │
                              ▼
                     Ausgang bis Ende 1851
```

**Ausgänge** (Entscheidung 6):

| Ausgang | Bedingung (Vorschlag) | Folgen im Modell |
|---|---|---|
| **Reich der Paulskirche** (gelungen) | Preußen nimmt die Krone an und mindestens zwei Drittel der Bundesbevölkerung außerhalb Österreichs erkennen die Reichsverfassung an, oder der Krieg darum wird gewonnen | Reichsverfassung: die Mitglieder `konstitutionell`, Grundrechte (Presse, Vereine), Reichstag nach `allgemein_maenner` (neue Wahl), Zollunion aller Mitglieder (M9 Zolleinigung), Bundesversammlung endet, Preußen Präsidialmacht; Österreich außerhalb, Krise mit Österreich |
| **Großdeutsche Lösung** (gelungen) | Die Paulskirche stimmt großdeutsch und Österreich tritt mit seinen Bundesländern bei | wie oben mit Österreich; Erbkaiser oder Direktorium nach Abstimmung |
| **Union** (teilweise) | Preußen lehnt ab, setzt eine Union mit Fürsten durch und gibt in Olmütz nicht nach; Österreich lenkt ein (seit b2) | Union als Bund unter Preußen (wie der Norddeutsche Bund 1867), Bundesversammlung ohne die Union |
| **Gegenrevolution** (gescheitert, historisch) | Krone abgelehnt, Aufstände niedergeschlagen, Olmütz | Bundesversammlung tagt wieder (1851), Bundesreaktionsbeschluss: Gesetze zurück (Presse, Vereine), Unruhe sinkt auf Zeit, Auswanderung |
| **Republik** (selten) | Aufstände siegen in Staaten mit mehr als der Hälfte der Bundesbevölkerung außerhalb Österreichs | Revolutionsregierungen als Merkmal, Krieg der Fürsten (M10) mit offenem Ausgang |

**Umsetzung (seit a4, `core/src/events.cpp`):** Journale stehen in `common/journale/*.chron`
(chronik-skript.md 3.35). Täglich nach den Ereignissen beginnt ein Journal an seinem `beginn` (ohne:
sofort) mit seiner ersten Stufe; danach gilt höchstens ein Übergang je Tag (`weiter { zu = … wenn
{ … } }`, der erste, dessen Bedingung gilt), am `ende` der Ausgang mit `standard = ja`. Jeder Wechsel
schreibt Zustand (`World::journal_states`), Weg (`journal_history`) und Chronik und wendet die
Wirkungen der Stufe an (Welt). Ziele (`ziel`) sind Anzeige: erfüllt nach einer Bedingung oder
Fortschritt als Zahl der Staaten, für die eine Bedingung gilt, von `soll`. Für Bedingungen über viele
Staaten gibt es seit a4 `bevoelkerungsanteil { wenn { … } unter { … } mindestens = x }` und
`staaten { wenn { … } mindestens = n }`.

Die Ausgänge setzen Merkmale, Gesetze und Felder; der Besitz der Orte und die Staaten bleiben
(ein Reich der Paulskirche ist ein Bundesstaat der bestehenden Staaten, Entscheidung 6).

### 5.3 Abstimmungen in der Paulskirche

Die Nationalversammlung stimmt **achtmal** ab: Zentralgewalt (6/1848), Malmö (9/1848),
Grundrechte (12/1848), großdeutsch oder kleindeutsch (10/1848 und 3/1849), Wahlrecht der
Reichsverfassung (3/1849), Erbkaiser (3/1849), Kaiserwahl (28.3.1849). Jede Fraktion stimmt nach
einer **Neigung** je Frage (Daten, z. B. Erbkaiser: Casino und Landsberg ja, Württemberger Hof
geteilt, Donnersberg nein; großdeutsch: Katholiken und Demokraten ja). Die Sitze stammen aus der Wahl
vom 1. Mai (M8); eine andere Wahl ergibt andere Mehrheiten. Im **Parteienmodus** stimmt die eigene
Strömung, wie der Spieler wählt (Ereignis mit Optionen, Entscheidung 9).

**Umsetzung (seit a4):** Abstimmungen stehen in `common/abstimmungen.chron` (chronik-skript.md 3.35) und
beginnen mit der Wirkung `abstimmung = <id>`. Gezählt werden die Abgeordneten der letzten Wahl des
Parlaments je Fraktion: Ja-Stimmen = gerundet Sitze × Neigung der Fraktion (fraktionslose nach
`fraktionslos`); angenommen ist die Vorlage mit mehr Ja-Stimmen als `mehrheit` × abgegebene. Das
Ergebnis steht im Zustand (`World::vote_results`), in der Chronik („Nationalversammlung: Erbliches
Kaisertum angenommen (267 : 263)“) und in der Bedingung `abstimmung_<id> = angenommen`; die Wirkungen
`ja { … }` bzw. `nein { … }` gelten für die Welt. Ohne Wahl des Parlaments findet keine Abstimmung
statt. Die Stimme der eigenen Strömung im Parteienmodus kommt mit der Oberfläche (b4).

**Bundesversammlung (seit a4):** `bundesversammlung = ruht` setzt `World::diet_suspended`; solange sie
ruht, nimmt sie keine Anträge an und stimmt über einen offenen Antrag erst ab, wenn sie wieder tagt
(`bundesversammlung = tagt`).

### 5.4 Die deutsche Frage (bis 1871)

Das Journal **„Deutsche Frage“** läuft vom Start bis zum 1. Juli 1871 und führt den Reiter in
„Diplomatie“ (M9) als Journal mit Wertung weiter. Ausgänge (Entscheidung 8):

| Ausgang | Erkennung |
|---|---|
| **Kleindeutsches Reich** | ein Bundesstaat unter Preußen ohne Österreich mit allen Staaten südlich des Mains (Reich der Paulskirche oder Reichsgründung 1871) |
| **Großdeutsches Reich** | ein Bundesstaat mit Österreich |
| **Zwei Bünde** | ein Bund unter Preußen nördlich des Mains, die süddeutschen Staaten außerhalb (1867–1870) |
| **Bund** | der Deutsche Bund besteht, reformiert oder nicht |
| **Gescheitert** | kein Bund, kein Reich (Auflösung ohne Nachfolge) |

Ziele mit Fortschritt (aus dem Zustand): Anteil der Bundesbevölkerung in der Zollunion Preußens,
Lager in der Bundesversammlung, Verfassungen der Staaten, Stand der Revolution. Ohne Spieler kommt
eine Reichsgründung 1871 nur über Ereignisse zustande, die Kriege auslösen (Entscheidung 10).

**Umsetzung (seit b3):** `common/journale/deutsche_frage.chron` mit drei Stufen (Deutscher Bund,
Deutscher Krieg, Zwei Bünde) und den fünf Ausgängen, vom Start bis zum 1.7.1871; am 30.6.1871 wählt
jede Stufe ihren Ausgang selbst (Bund → `bund`, Krieg → `gescheitert` nach der Auflösung des Bundes,
sonst `bund`; Zwei Bünde → `zwei_buende`). In einer Partie ab 1848 bleiben Besitz, Bundesgebiet und
Bundesversammlung die des Starts; die Übergänge lesen daher die Merkmale der Ereignisse:
`reich_paulskirche` oder `kaiserproklamation` → kleindeutsch, `grossdeutsches_reich` → großdeutsch,
`deutscher_krieg` oder Krieg Preußen–Österreich → Deutscher Krieg, `norddeutscher_bund` oder die
Union der Revolution → Zwei Bünde; `bundesreform_oesterreich` (Österreich gewinnt 1866) führt zurück
zum Bund. In den Lesezeichen 1866 und 1870 beginnt das Journal mit dem Krieg bzw. erkennt den
Norddeutschen Bund am Bundesgebiet (Österreich nicht, Preußen im Bund). Ein Ergebnis der Kriege
lesen die Ereignisse mit der Bedingung `krieg_gewonnen`/`krieg_verloren` (seit b3, aus
`World::war_results`); die Kriege 1864 und 1866 haben Kriegsziele (`krieg { … }`, seit b3): die drei
Herzogtümer bzw. die Führung im Bund, deren Verlierer den Bund verlässt.

## 6. Inhalt (M11b)

### 6.1 Ereignisse 1848–1851 (Revolution)

Rund 55 Ereignisse; `w` = Welt, `s` = je Staat, `W` = Wendepunkt.

| Zeit | Ereignisse |
|---|---|
| Februar/März 1848 | Februarrevolution in Paris (w), Märzforderungen (s, W), Agrarunruhen (s; im Odenwald und Kraichgau mit antijüdischen Ausschreitungen, siehe 6.5), Heidelberger Versammlung (w), Metternichs Sturz (Österreich), Barrikaden in Berlin (Preußen, W: Truppen abziehen · Kampf fortsetzen), Umritt des Königs, Abdankung Ludwigs I. (Bayern), Schleswig-Holsteinische Erhebung (Dänemark/Bund, Krieg M10), Märzministerium (s) |
| April–Juni 1848 | Vorparlament (w), Bundesbeschluss zum Wahlrecht (w), Heckerzug (Baden, Aufstand), Großpolnischer Aufstand in Posen (Preußen), Eröffnung der Paulskirche (w), Preußische Nationalversammlung, Reichsverweser Erzherzog Johann (w, Abstimmung), die Bundesversammlung übergibt (w), Gründung von Vereinen (s), Pressefreiheit und Zeitungen (s, atmosphärisch) |
| Juli–Dezember 1848 | Waffenstillstand von Malmö (w, Abstimmung), Septemberunruhen in Frankfurt (w), Struve-Putsch (Baden), Wiener Oktoberaufstand (Österreich, Aufstand), Hinrichtung Robert Blums (Österreich, dokumentarisch), Ministerium Brandenburg und Wrangel in Berlin (Preußen, W), oktroyierte Verfassung Preußens (W), Grundrechte des deutschen Volkes (w, Abstimmung; je Staat Einführung ja/nein), Ablösung der Grundlasten (s), Arbeiterkongress und Arbeiterverbrüderung (w) |
| 1849 | Oktroyierte Märzverfassung Österreichs, Abstimmungen über Reichsverfassung, Wahlrecht und Erbkaiser (w), Kaiserwahl (w), Kaiserdeputation (Preußen, W: annehmen · ablehnen · Bedingungen), Anerkennung der Reichsverfassung (s), Dresdner Maiaufstand (Sachsen, Aufstand), Badische Revolution und Festung Rastatt (Baden), Pfälzer Aufstand (Bayern), Elberfeld und Iserlohn (Preußen), Intervention (s: Preußen hilft), Rumpfparlament in Stuttgart (w), Standgerichte (Baden, dokumentarisch), Dreiklassenwahlrecht (Preußen, W), Dreikönigsbündnis (Preußen, Sachsen, Hannover), Auswanderung der Achtundvierziger (s) |
| 1850–1851 | Erfurter Unionsparlament (w), Frieden von Berlin und Idstedt (Dänemark), Kurhessischer Verfassungskonflikt (Kurhessen, Bundesexekution M9), Olmütz (Preußen, W: nachgeben · Krieg), Dresdner Konferenzen (w), Wiederherstellung der Bundesversammlung (w), Bundesreaktionsbeschluss (w; je Staat Gesetze zurück) |

**Umsetzung (seit b1, kalibriert in b2):** 65 Ereignisse in `common/ereignisse/1848_maerz.chron`, `1848_paulskirche.chron`,
`1849_reichsverfassung.chron` und `1850_union.chron` (54 historisch, 6 dynamisch, 5 atmosphärisch; die
je Staat wiederholten Ereignisse wie die Märzforderungen zählen einmal), das Journal
`common/journale/revolution.chron` (7 Stufen, 5 Ausgänge) und acht Abstimmungen der Paulskirche in
`common/abstimmungen.chron`. Alle Ereignisse tragen Datumsfenster bis Ende 1851 und treten in den
Lesezeichen 1864–1870 nicht ein; das Journal beginnt dort nicht (seit b1 beginnt ein Journal nicht, wenn
sein Ende schon vor dem Start liegt). Einige Mechanismen aus der Liste oben sind vereinfacht:
- Die Erhebung in Schleswig-Holstein erklärt mit der historischen Option einen preußisch-dänischen Krieg
  (M10); Malmö ist eine Abstimmung, der Krieg endet nach den Regeln des Krieges, nicht durch das
  Ereignis.
- Die Reichsverfassungskampagne besteht aus den Aufständen in Sachsen, der Pfalz (Bayern), Baden und im
  Rheinland (Preußen) und den Interventionen Preußens; eine siegreiche Revolutionsregierung endet mit
  dem Ereignis „Das Ende der Revolutionsregierung“ (Rastatt) und den Standgerichten.
- Ein Reich der Paulskirche oder die großdeutsche Lösung führt die Reichsverfassung in den Staaten ein,
  die sie anerkannt haben (Merkmal `reichsverfassung`, Folgeereignis `reich.mitglied`).
- Olmütz: Mit „standhalten“ macht Preußen mobil, und Österreich antwortet eine Woche später
  (dynamisches Ereignis `aut.olmuetz.antwort`): Es beginnt den Krieg (60) oder nimmt die Union hin
  (40). Nur das Einlenken führt zum Ausgang „Union“; ein Krieg um die Union endet in der Regel nicht
  vor Ende 1851, und sein Ausgang entscheidet das Journal nicht (der Ausgang am Ende ist dann die
  Gegenrevolution). Seit b2; in b1 erklärte Preußen selbst den Krieg, und die Union galt erst nach
  dessen Ende, das im Labor nie vor 1852 kam.
- Die Gesetze von 1850–1854 aus `staaten_historie.csv` gelten nur für spätere Lesezeichen; in einer
  Partie ab 1848 setzt sie der Bundesreaktionsbeschluss (Ereignis „Die Reaktion im eigenen Land“).

### 6.2 Ereignisse 1852–1871

Rund 30 Ereignisse; viele davon geben einer Partie ab 1848 die historischen Anlässe der späteren
Kriege, die Lesezeichen 1864, 1866 und 1870 beginnen danach.

| Zeit | Ereignisse |
|---|---|
| Reaktion 1852–1858 | Londoner Protokoll (Dänemark), Zollvereinskrise und Februarvertrag mit Österreich, Krimkrieg und Neutralität (w), Polizeiverein (w), Auswanderungswelle 1852–1854 (s) |
| Neue Ära 1858–1862 | Regentschaft Wilhelms (Preußen), Italienischer Krieg 1859 und Mobilmachung (w), Deutscher Nationalverein (w), Heeresreform Roons (Preußen), Deutsche Fortschrittspartei (Preußen), Verfassungskonflikt (Preußen, W: Bismarck berufen · nachgeben · Abdankung), „Blut und Eisen“ |
| 1863–1866 | Alvensleben-Konvention (Preußen), Frankfurter Fürstentag (w), ADAV Lassalles (w), dänische Novemberverfassung und Bundesexekution (w, Krieg 1864), Frieden von Wien, Gasteiner Konvention (Preußen, Österreich), Bündnis mit Italien (Preußen), Bundesreformantrag mit allgemeinem Wahlrecht (Preußen), Mobilmachungsbeschluss des Bundes (w, Krieg 1866), Indemnitätsgesetz (Preußen, W) |
| 1867–1871 | Norddeutscher Bund (w), Luxemburgkrise (w), Zollparlament (w), Eisenacher Partei (w), Spanische Thronkandidatur und Emser Depesche (Preußen, W: unverändert · zugespitzt · zurückhalten; Krieg 1870), Novemberverträge (Bayern, Württemberg, Baden, Hessen), Kaiserbrief (Bayern), Kaiserproklamation (w) |

**Umsetzung (seit b3):** 33 Ereignisse in `common/ereignisse/1852_reaktion.chron` (Londoner Protokoll,
Polizeiverein, Zollvereinskrise, Handelsvertrag mit Österreich, Krimkrieg, große Auswanderung, Neue
Ära, Italienischer Krieg, Nationalverein, Schillerfeier, Heeresreform, Fortschrittspartei,
Verfassungskonflikt, „Blut und Eisen“) und `1863_einigung.chron` (Alvensleben-Konvention, ADAV,
Fürstentag, Novemberverfassung, Frieden von Wien, Gastein, Bundesreformantrag, Mobilmachungsbeschluss,
Prager Frieden oder Österreichs Sieg, Indemnität, Luxemburgkrise, Norddeutscher Bund, Zollparlament,
Eisenacher Partei, Emser Depesche, Novemberverträge, Kaiserbrief, Kaiserproklamation). Die Fenster der
Kriegsanlässe enden vor den Lesezeichen „Düppel“ und „Königgrätz“; Frankreich hat in einer Partie ab
1848 keine Orte, die Emser Depesche führt dann zu keinem Krieg, und die Novemberverträge folgen dem
Merkmal der nationalen Krise (im Lesezeichen „Sedan“ dem Krieg mit Frankreich). Vereinfacht: Die
Fortschrittspartei und die Arbeiterparteien sind Merkmale, keine neuen Strömungen; die Annexionen von
1866 kommen nicht aus dem Prager Frieden, sondern nur aus Kriegszielen.

### 6.3 Dynamische Ereignisse

Rund 10 wiederholbare Ereignisse aus Generatoren (Kapitel 18, Abschnitt 3.1) mit Bedingungen aus dem
Zustand: Missernte und Teuerung (Getreidepreis), Hungerkrawall (Teuerung, Unruhe), Cholera (1849,
1854, 1866 mit Seuchenregeln aus M1), Auswanderungswelle (Lebensstandard), Weberelend
(Fabrikarbeiter, Textil), Eisenbahnunglück und Eröffnungsfeier (M7), Streik (Fabrikarbeiter ab
1860), Pressprozess (Pressegesetz), Turnfest und Sängerfest (atmosphärisch), Schillerfeier 1859.

Zusammen gut 95 Ereignisse; die Abnahme verlangt 80 mit Quellen (die dynamischen zählen mit, wenn
sie eine Quelle für ihr Muster nennen).

**Umsetzung (seit b3):** elf wiederholbare Ereignisse in `common/ereignisse/dynamisch.chron`: Missernte
und Teuerung (Getreidepreis ≥ 1,5), Hungerkrawall (≥ 1,7 und Unruhe ≥ 35), Cholera (1849, 1854/55, 1866
bei niedrigem Lebensstandard), Auswanderungswelle (ab 1856), Weberelend (Preußen, Sachsen bis 1866),
Bahneröffnung und Eisenbahnunglück, Streik (ab 1860, nur ohne Vereinsverbot), Pressprozess (unter dem
Pressegesetz), Turn- und Sängerfest; dazu die Schillerfeier 1859 als einmaliges Ereignis. Die Seuchen
aus M1 bleiben eigene Regeln, die Cholera ist ihr politisches Echo. Zusammen 109 Ereignisse, alle mit
Quelle (`uhrwerk-cli info`: „109 mit Quelle“, CTest `ereignisse_quellen`).

### 6.4 Texte, Quellen, Kompass

- Texte 2–6 Sätze, sachlich, im Präsens der Lage; historische Stimmen paraphrasiert, Zitate nur
  belegt („Blut und Eisen“, „Krone aus Dreck und Letten“ mit Quelle).
- Jede Option zeigt ihre Folgen; keine Option ist ohne Kosten (Gestaltungsregel 1).
- `quelle` je Ereignis (Literatur mit Seite, wo bekannt), `sicherheit = ungeprueft`.
- Der Kompass markiert die historische Wahl; die Chronik vermerkt Abweichungen („In dieser
  Geschichte nahm Friedrich Wilhelm IV. die Kaiserkrone an.“), Grundlage für den Vergleich mit der
  realen Geschichte (Kapitel 18, Abschnitt 9.1).

### 6.5 Verantwortung und Erinnerungskultur

Regeln aus Kapitel 20, angewendet auf 1848–1871:
- **Gewalt gegen Aufständische** ist eine historische Option der Regierungen (Militär einsetzen,
  Belagerungszustand). Ihre Folgen erscheinen als Chronik und Text (Tote der Barrikadenkämpfe in
  Berlin, Standgerichte in Baden 1849, die Hinrichtung Robert Blums) – **nie als Kennzahl, Punkt oder
  Belohnung**. Keine Option befiehlt Massaker, Geiselnahmen oder Gefangenenmorde.
- **Antijüdische Ausschreitungen 1848** (Agrarunruhen in Baden und Hessen) werden als historisches
  Geschehen dokumentiert. Die Optionen betreffen den Schutz der Betroffenen (Militär, Ordnung) und
  die Rechtsgleichheit (Grundrechte); es gibt keine Option, die Gewalt duldet oder belohnt.
- **Polen in Posen 1848** und die Polendebatte der Paulskirche: dargestellt mit beiden Perspektiven,
  die Niederschlagung als Geschehen mit Folgen; keine Germanisierung als Spielziel.
- **Dänen in Schleswig**: Nationalitätenkonflikt mit beiden Seiten.
- Humor nur atmosphärisch (Sängerfeste, Bürokratie), nie bei Gewalt und ihren Opfern.

## 7. Ereignislabor

`uhrwerk-cli ereignisse --szenario voelkerfruehling --seeds 16` rechnet das Lesezeichen ohne Spieler
(alle Staaten wählen nach Gewicht) bis Ende 1851 und berichtet je Lauf: Ausgang des Journals
„Revolution“, Stufen mit Datum, Ereignisse je Art, gewählte Optionen der Wendepunkte, Aufstände und
ihr Ausgang, Unruhe je Staat (Median und Höchstwert), Gesetze am Ende. Über alle Läufe: Verteilung
der Ausgänge gegen **Korridore** in `common/ereignisziele_<szenario>.chron` (Exitcode 1 bei Verstößen,
wie Wahl-, Zoll- und Kriegslabor).

Vorschlag für die Korridore (Spielwerte, Entscheidung 11):

| Größe | Korridor |
|---|---|
| Gegenrevolution (historisch) | 50 … 90 % der Läufe |
| Reich der Paulskirche oder großdeutsch | 5 … 30 % |
| Republik | höchstens 10 % |
| kein Ausgang in allen Läufen | ja |
| Märzministerien bis Ende April 1848 | in mindestens 70 % der konstitutionellen Staaten |
| Kaiserwahl findet statt | in mindestens 60 % der Läufe |

Die Abnahme „beide Wege sind spielbar“ prüft das Labor mit **Strategien** für einen Staat:
`--spieler PRU --strategie krone` (Preußen wählt die Optionen, die zum Reich führen) muss in
mindestens 50 % der Läufe zum Reich der Paulskirche führen; `--strategie reaktion` in mindestens 80 %
zur Gegenrevolution. Eine Strategie ist eine Liste bevorzugter Optionen je Ereignis in der
Zieldatei.

`--bis 1871.07.01` rechnet die deutsche Frage (langsamer, nicht in jedem CI-Lauf). Seit b3 mit eigener
Zieldatei: `uhrwerk-cli ereignisse --szenario voelkerfruehling --seeds 16 --ziele
data/common/ereignisziele_voelkerfruehling_1871.chron` (Journal `deutsche_frage`, CI-Schritt
„Ereignislabor 1871“). Korridore: kleindeutsch 15 … 60 %, zwei Bünde 5 … 45 %, Bund 20 … 65 %, ohne Bund
und Reich höchstens 15 %, großdeutsch höchstens 10 %, kein Ausgang in allen Läufen.

**Umsetzung (seit a5, `core/src/event_lab.cpp`, `uhrwerk-cli ereignisse`):** Die Korridore stehen in
`common/ereignisziele_<szenario>.chron` (chronik-skript.md 3.36): ein Block `labor` mit Ende, Journal
und `nicht_festgelegt`, Korridore je Ausgang (`ausgang <id> { anteil = { von bis } }`) und je Ereignis
(Anteil der Läufe mit dem Ereignis oder, mit `staaten`, mittlerer Anteil der Staaten mit Ereignis und
Option bis zu einem Datum) und Strategien (`strategie <id> { spieler = TAG wahl = { <ereignis> =
<option> … } ausgang … }`). Mit `--strategie` wartet der Staat der Strategie (Befehl
`Ereignisspieler`) und das Labor wählt für ihn: die bevorzugte Option, sonst die mit dem größten
Gewicht; geprüft werden dann nur die Ausgänge der Strategie. Der Bericht zeigt je Lauf Ausgang und
Datum, Ereignisse je Art, Aufstände je Ausgang, Unruhe (Median am Ende, höchste an einem
Monatsersten), den Weg des Journals und die Wendepunkte, dazu die Gesetze am Ende des ersten Laufs.
Die CTests `ereignisse_fixture`, `ereignisse_fixture_krone` und `ereignisse_fixture_reaktion` rechnen
eine kleine Revolution auf der Test-Fixture (`tests/daten_ereignisse`); die Korridore 1848 kommen mit
dem Inhalt (b1, b2).

## 8. Zustand, Kern und Spielstand

- **Zustand** (nur in Welten mit Ereignissen; 1648 bleibt bitgleich): eingetretene Ereignisse je
  Staat mit Option und Datum, offene Ereignisse des Spielers mit Frist, geplante Folgeereignisse,
  Flags, befristete Wirkungen, Gesetze je Staat, Zustimmung je Staat und Gruppe, Unruhe je Staat,
  Aufstände, Journale mit Stufe und Datum, Abstimmungsergebnisse, Ruhen der Bundesversammlung.
- **Inhalt:** Ereignisse, Journale, Gesetzesgruppen, Wünsche der Gruppen, Regeln der Unruhe und der
  Aufstände, Ziele des Labors; neuer Inhaltsbereich `ereignisse` im Fingerabdruck.
- **Spielstand Format 5**; Spielstände aus M10 laden nicht.
- **Befehl `Ereigniswahl`** im Protokoll; Wiederholung bitgleich.
- **Zufall:** Strom `Ereignisse` (Auslösung, Wahl ohne Spieler, Aufstände); 1648 zieht daraus wie
  bisher nur für Seuchen.
- **Reihenfolge im Tag:** nach Militär und Diplomatie, vor der Chronik; Journale nach den
  Ereignissen.
- **Leistung:** Bedingungen werden nur in offenen Datumsfenstern geprüft; Ziel unter 1 ms je Tag
  für rund 100 Ereignisse und 38 Staaten.

## 9. Brücke und Godot (M11b)

- **Brücke:** offene Ereignisse des Spielers (Titel, Text, Datum, Quelle, Optionen mit Folgentext,
  historischer Kompass, Frist), `choose_event_option`, eingetretene Ereignisse (Liste mit Option),
  Journale (Stufe, Ziele mit Fortschritt, Ausgänge), Gesetze, Zustimmung und Unruhe je Staat,
  Aufstände, befristete Wirkungen; Kartenmodus 16 **`MAP_UNRUHE`**.
- **Godot:**
  - **Ereignisfenster**: hält die Zeit an, zeigt Datum, Titel, Text, Optionen mit Folgen,
    Kompassmarke (abschaltbar), Quelle; Wendepunkte mit eigener Kopfzeile. Mehrere offene Ereignisse
    nacheinander.
  - **Journalfenster** „Revolution 1848/49“ und „Deutsche Frage“: Stufe, Weg bisher (Stufen mit
    Datum), Ziele mit Fortschritt, mögliche Ausgänge.
  - **Staatsinfo**: Gesetze, Zustimmung der Gruppen (Balken), Unruhe, befristete Wirkungen.
  - **Kartenmodus „Unruhe“**, Aufstände als Symbol in der Hauptstadt.
  - **Chronik** mit Wendepunkten hervorgehoben; Spielmenü „Ereignisse“ (bisherige Ereignisse mit
    Wahl).

## 10. Tests und Abnahme

| Kriterium | Prüfung |
|---|---|
| Mindestens 80 Ereignisse 1848–1871 mit Quellen | `uhrwerk-cli pruefen` und `info` zählen Ereignisse mit `quelle` je Jahrzehnt; CTest `ereignisse_quellen` |
| Die Revolution kann scheitern oder gelingen | Ereignislabor ohne Spieler: alle Ausgänge in ihren Korridoren, keiner in allen Läufen; mit den Strategien `krone` und `reaktion` erreicht Preußen den jeweiligen Ausgang (CI); Klicktest: als Preußen die Krone annehmen und den Ausgang „Reich“ erreichen, als Baden die Märzforderungen ablehnen und einen Aufstand erleben |

Weitere Tests:
- **Einheitentests:** Lader (Fehler, Verweise, Kreise), Bedingungen und Wirkungen einzeln, Auslösung
  mit Wartezeit und Termin, Einmaligkeit, Wahl nach Gewicht, Frist, Befehl, Flags und Befristung,
  Zustimmung und Unruhe (Zielwert, Annäherung), Aufstand mit allen Ausgängen, Journal-Übergänge,
  Abstimmung nach Fraktionen.
- **Determinismus und Spielstände:** gleiche Prüfsumme mit 1 und 4 Threads; Speichern mit offenem
  Ereignis, während eines Aufstands und zwischen Journalstufen; Wiederholung mit Ereigniswahl;
  Formatwache.
- **1648 bitgleich** (`5db13a66d63e6079`); 1848 rechnet mit Ereignissen anders (neue Bilanz,
  Korridore des Balancing-Berichts, der Wahl und des Zolls bleiben erfüllt).
- **Godot:** Einheitentests (Ereignisfenster, Folgentexte), Rauchtest der Brücke, Klicktest (siehe
  oben), Screenshots des Ereignisfensters und des Journals.

## 11. Entscheidungen (wie empfohlen freigegeben)

1. **Teilung M11a/M11b.** Empfehlung: **ja** (Abschnitt 1). Alternative: Inhalt und System
   zusammen, Revolution zuerst (schneller sichtbar, schwerer prüfbar).
2. **Sprache der Ereignisse.** Empfehlung: **festes Vokabular** von Bedingungen und Wirkungen im
   bestehenden Chronik-Skript (Abschnitte 3.2 und 3.3), erweiterbar je Meilenstein. Alternativen:
   - eine Ausdruckssprache mit Variablen und Rechnung (mächtiger, schwerer zu prüfen und zu
     modden);
   - Ereignisse in C++ (nicht moddbar, gegen Kapitel 23).
3. **Gesetze.** Empfehlung: **drei Gruppen** (Presse, Vereine, Grundlasten) zu Verfassung, Wahlrecht
   und Wahlsystem, geändert durch Ereignisse (Abschnitt 4.1). Alternativen:
   - nur Verfassung und Wahlrecht (die Märzforderungen hätten kaum Gegenstand);
   - alle 36 Gruppen aus Kapitel 09 (ohne Gesetzgebung kaum sinnvoll).
4. **Wahl ohne Spieler.** Empfehlung: **Gewichte**, die vom Zustand abhängen dürfen, gezogen aus dem
   Strom `Ereignisse` (Abschnitt 3.5). Alternativen: immer die historische Option (dann gäbe es ohne
   Spieler nur einen Ausgang, gegen die Abnahme); reiner Zufall.
5. **Unruhe.** Empfehlung: **eine Zahl je Staat** mit Zielwert aus Not, Teuerung, Verfassung, Zensur,
   Ansteckung und Zustimmung (Abschnitt 4.3). Alternativen:
   - Unruhe je Ort (genauer, aber 4.000 Werte und eine Karte voller Zahlen);
   - Legitimität und Autorität als zwei Kennzahlen (Kapitel 09, Abschnitt 3), später mit M12.
6. **Ausgänge der Revolution.** Empfehlung: **fünf Ausgänge** (Abschnitt 5.2); ein Reich der
   Paulskirche ist ein **Bundesstaat der bestehenden Staaten** mit Reichsverfassung, Reichstag und
   Zollunion, kein neuer Staat. Alternativen:
   - die Mitglieder verschmelzen zu einem Staat „Deutsches Reich“ (eine Karte in einer Farbe, aber
     ein großer Eingriff in Haushalt, Heer und Verwaltung);
   - nur zwei Ausgänge (gelungen, gescheitert).
7. **Aufstände.** Empfehlung: **ohne Heere auf der Karte** nach dem Erfolgsmodell aus Kapitel 09
   (Abschnitt 4.4); eine Intervention zählt die Soldaten des Helfers, seine Heere marschieren nicht.
   Alternativen:
   - Revolutionsarmeen als Heere auf der Karte (Baden 1849 mit Schlacht bei Waghäusel und Belagerung
     von Rastatt); braucht Staaten, die zur Laufzeit entstehen;
   - Aufstände nur als Text ohne Zustand.
8. **Deutsche Frage.** Empfehlung: **Journal mit fünf Ausgängen** bis zum 1. Juli 1871 (Abschnitt 5.4),
   erkannt aus Bund, Union und Verfassung. Alternative: nur Anzeige ohne Ausgang (wie M9).
9. **Parteienmodus.** Empfehlung: Im Parteienmodus erhält die Strömung die **Abstimmungen der
   Paulskirche** als Ereignisse; die Ereignisse der Staaten wählen nach Gewicht. Alternative:
   Parteienmodus ohne Ereignisse (dann bleibt er nach der Wahl ohne Handlung).
10. **Kriege aus Ereignissen.** Empfehlung: **ja**: Ereignisse dürfen Kriege beginnen (Schleswig-Holstein
    1848, Dänemark 1864, Bundesbeschluss 1866, Ems 1870), auch ohne Spieler nach Gewicht. So kann eine
    Partie ab 1848 die Einigungskriege erleben, ohne dass die Staaten eine KI haben. Alternative:
    Kriege nur aus Spieler, Lesezeichen und Krisen (dann endet die deutsche Frage ohne Spieler fast
    immer beim „Bund“).
11. **Korridore.** Empfehlung: die Werte in Abschnitt 7, Gegenrevolution als häufigster Ausgang.
    Alternative: alle Ausgänge gleich wahrscheinlich ohne Spieler (offener, aber ahistorisch).
12. **Quellen und Fachprüfung.** Empfehlung: Literatur aus der Kenntnis, alle Ereignisse
    `ungeprueft`, Wirkungen `spielwert`; die Fachprüfung schließt an die von M7 bis M10 an.
    Alternative: Texte erst nach einer Prüfung (dann wartet die Abnahme).
13. **Prüfsumme 1648.** Empfehlung: **bitgleich** (Ereignisse nur in Welten mit Ereignisdaten);
    Spielstand Format 5. Alternative: neue Referenzprüfsumme.

## 12. Umsetzungsschritte

| Schritt | Inhalt | Prüfung |
|---|---|---|
| a1 | **Datenmodell:** Ereignisse, Optionen, Bedingungen, Wirkungen (Lader, mehrzeilige Texte, Prüfungen), Gesetzesgruppen und Startwerte, Wünsche der Gruppen, Regeln; Zustand, Inhaltsbereich `ereignisse`, Format 5 | Unit-Tests, `pruefen`, 1648 bitgleich |
| a2 | **Auslösung und Wahl:** tägliche Prüfung, Wartezeit, Termin, Folgeereignisse, Gewichte, offene Ereignisse, Befehl `Ereigniswahl`, Frist; Flags und Befristung | Unit-Tests, Wiederholung, Spielstand |
| a3 | **Zustimmung, Unruhe, Aufstände** | Unit-Tests auf der Fixture |
| a4 | **Journale und Abstimmungen** (Zustandsautomat, Abstimmung nach Fraktionen, Ruhen der Bundesversammlung) | Unit-Tests |
| a5 | **Ereignislabor** `uhrwerk-cli ereignisse` mit Zielen und Strategien auf der Test-Fixture; CTests | Labor auf der Fixture |
| b1 | **Inhalt 1848–1851:** Journal Revolution, ~55 Ereignisse, Gesetze 1848 in `staaten_historie.csv` | Archivar, `pruefen` |
| b2 | **Kalibrierung** mit dem Ereignislabor; Bilanz, Wahl und Zoll 1848 erneut | Korridore erfüllt, CI |
| b3 | **Inhalt 1852–1871** und deutsche Frage, dynamische Ereignisse | Zählung ≥ 80 mit Quellen |
| b4 | **Brücke und Godot:** Ereignisfenster, Journale, Staatsinfo, Kartenmodus „Unruhe“; das Diplomatiefenster zeigt die ruhende Bundesversammlung (seit b1 sperrt sie ab Juli 1848 die Anträge) | Godot-Tests, Screenshots |
| b5 | **Doku und Review** (Abschluss M11) | CI grün |

## 13. Umsetzungsstand

| Schritt | Stand |
|---|---|
| – | Entscheidungen wie empfohlen freigegeben; Umsetzung beginnt mit a1 |
| b3 | **erledigt:** Inhalt 1852–1871: 33 Ereignisse (6.2), elf dynamische (6.3), Journal „Die deutsche Frage“ (5.4) mit Zieldatei `ereignisziele_voelkerfruehling_1871.chron`; 109 Ereignisse, alle mit Quelle (CTest `ereignisse_quellen`). Kern: Bedingungen `krieg_gewonnen`/`krieg_verloren`, Kriegserklärung mit Zielen `krieg { … }`, `mobilmachung = nein`, Abrüstung des Helfers nach einem Aufstand, Regel `bund_durchzug` (militaer.md 3.4), kein Haushalt für Staaten ohne Orte. Inhaltsstände ereignisse 6, militaer 9. Ohne Spieler bis 1871 (48 Seeds): kleindeutsch 35 % (davon ein Teil aus der Paulskirche), zwei Bünde 23 %, Bund 42 %; Revolution, Bilanz, Wahl, Zoll und alle Kriegslabore in ihren Korridoren; 1648 bitgleich |
| b2 | **erledigt:** Kalibrierung mit dem Ereignislabor (16 Seeds wie die CI): Märzforderungen gewähren 75 · hinhalten 15 · Militär 10 (vorher 60/25/15), Kaiserdeputation annehmen 20 · ablehnen 50 · Bedingungen 30 (vorher 15/55/30), Olmütz nachgeben 70 · standhalten 30 (vorher 75/25) mit der Antwort Österreichs (`aut.olmuetz.antwort`), Übergang zur Union über `union_anerkannt`. Ohne Spieler mit den Seeds der CI (1648–1663): Gegenrevolution 87,5 %, Reich 6,3 %, Union 6,3 %; mit 48 weiteren Seeds (2000–2047): 62,5 %, 27,1 %, 10,4 %; über alle 64 Läufe 68,8 %, 21,9 %, 9,4 %, Republik und großdeutsche Lösung nie. Märzministerien bis Ende April in 66,4 % aller Staaten (76 % der 33 Staaten mit dem Ereignis), Kaiserwahl in allen Läufen; `krone` 100 % Reich, `reaktion` 100 % Gegenrevolution. Korridor der Märzministerien auf 70 … 100 % der Staaten mit dem Ereignis (0,61 … 0,87), Korridor der Union (höchstens 20 %). Bilanz, Wahl (Nationalversammlung, Preußen 1849), Zoll und Krieg 1848 mit Ereignissen in ihren Zielwerten |
| b1 | **erledigt:** Inhalt 1848–1851: 64 Ereignisse mit Quellen (Literatur, ungeprüft), Journal „Revolution 1848/49“ mit 7 Stufen und 5 Ausgängen, 8 Abstimmungen der Paulskirche nach Fraktionen, Korridore und Strategien `krone` und `reaktion` in `common/ereignisziele_voelkerfruehling.chron` (vorläufig bis b2); CTest `ereignisse_1848` (bis Ende 1848), CI-Schritt „Ereignislabor 1848“ (16 Seeds, ohne Spieler und mit beiden Strategien). Kernkorrekturen: ein Journal beginnt nicht nach seinem Ende; Folgen aus Journalen gehen nicht in die Kreisprüfung. Erste Läufe siehe CHANGELOG |
| a5 | **erledigt:** Ereignislabor `uhrwerk-cli ereignisse` (Korridore je Ausgang und Ereignis, `nicht_festgelegt`, Strategien mit bevorzugten Optionen, `--bis`), Bericht je Lauf und über alle Läufe, Exitcode 1 bei Verstößen; beendete Aufstände im Zustand (`revolt_history`, Inhaltsstand 5); Test-Fixture `tests/daten_ereignisse` mit kleiner Revolution und drei CTests (ohne Spieler, `krone`, `reaktion`); Unit-Tests der Korridore |
| a4 | **erledigt:** Journale (`common/journale/*.chron`) mit Stufen, Zielen, Übergängen und Ausgängen (Wertung, Ausgang am Ende, Wirkungen), Bedingung und Wirkung `journal_<id>`; Abstimmungen (`common/abstimmungen.chron`) nach Fraktionen mit den Sitzen der letzten Wahl, Bedingung `abstimmung_<id> = angenommen|abgelehnt`, Wirkungen bei Annahme und Ablehnung; Bundesversammlung `ruht`/`tagt` (keine Anträge, keine Abstimmung); Wirkungen `antrag { … }` und `krieg = TAG`; Bedingungen `bevoelkerungsanteil` und `staaten`; `info` zählt Journale und Abstimmungen. Inhaltsstand 4. Tests in `test_events.cpp`; 1648 bitgleich |
| a3 | **erledigt:** Zustimmung und Unruhe nähern sich am Monatsersten ihren Zielwerten (Wünsche der Gruppen; Not, Teuerung, Verfassung, Zensur über `unruhe` an den Gesetzesoptionen, Ansteckung über Nachbarn und das Welt-Merkmal, Zustimmung nach Einfluss); Steuerverweigerung und Auswanderung bei hoher Unruhe; Aufstände mit wöchentlichem Kräftevergleich (Mobilisierung, Elitenspaltung, Loyalität, Intervention) und drei Ausgängen mit Folgeereignissen; Wirkungen `aufstand` und `intervention`, Bedingung `aufstand = ja`; alle Faktoren als Spielwerte in `ereignisse.chron`. Inhaltsstand 3. Tests in `test_events.cpp`; 1648 bitgleich |
| a2 | **erledigt:** tägliche Auslösung mit Wartezeit, Termin und Datumsfenster, Einmaligkeit und Wiederholung mit Abstand, Folgeereignisse (auch ohne Frist am selben Tag, `nur_folge`), Wahl nach Gewicht mit Zuschlägen aus dem Strom `Ereignisse`, offene Ereignisse des Spielers mit Frist, Befehle `Ereigniswahl` und `Ereignisspieler`, alle Wirkungen aus 3.3 außer denen späterer Schritte, befristete Wirkungen (`befristet`, Steuerertrag, Moral, Unruhe, Beziehung); Bedingungen über Gruppen, Lebensstandard, Preise, Krieg, Bund, Zoll, Beziehung, Sitze, Heer und Haushalt. Inhaltsbereich Stand 2. Tests in `test_events.cpp` (Auslösung, Folgen, Wirkungen, Wahl des Spielers, Frist, Spielstand und Wiederholung mit offenem Ereignis, Determinismus mit 1 und 4 Threads); 1648 bitgleich |
| a1 | **erledigt:** Gesetzesgruppen `common/gesetze.chron` (Presse, Vereine, Grundlasten; Startwerte datiert in `staaten_historie.csv`, Feld `gesetz_<gruppe>`), Wünsche der elf Interessengruppen (`wuensche`), Regeln `common/ereignisse.chron` (`seit = 1848.01.01`, Frist 30), Lader der Ereignisse mit dem Vokabular aus 3.2/3.3 (Schlüssel mit `_`, mehrzeilige Texte, späterer Inhalt nie wahr bzw. entfällt, Kreisprüfung der Folgen ohne Frist); Zustand (Verfassung, Wahlrecht und Gesetze je Staat, Ereignischronik, offene und geplante Ereignisse, Merkmale, Zustimmung, Unruhe), Inhaltsbereich `ereignisse`, Spielstand Format 5; `info` zählt Ereignisse je Art und Jahrzehnt und die Gesetze zum Start. Tests `test_events.cpp`; 1648 bitgleich. Noch ohne Ereignisse im Inhalt (ab b1) |
