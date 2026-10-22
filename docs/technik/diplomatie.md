# Diplomatie und Zollverein (Meilenstein M9)

> **Status: verbindlich** – Entscheidungen 1–12 wie empfohlen freigegeben (Abschnitt 9); Umsetzung
> läuft, Stand in Abschnitt 11. Grundlage sind
> [14 Diplomatie](../14_diplomatie.md) (Abschnitte 1, 3, 4 und 5),
> [08 Wirtschaft und Finanzen](../08_wirtschaft_und_finanzen.md) (Abschnitt 5.3, Zollverein),
> [12 Föderalismus](../12_foederalismus_und_bundeslaender.md) und der Meilensteinplan
> ([meilensteine.md](../meilensteine.md#m9--diplomatie-und-zollverein)). Das Wirtschaftsmodell steht
> in [wirtschaft.md](wirtschaft.md), das Lesezeichen 1848 in [lesezeichen_1848.md](lesezeichen_1848.md).

Im März 1848 ist der Deutsche Bund ein Staatenbund unter österreichischem Vorsitz. Wirtschaftlich
geht ein Teil davon schon eigene Wege: Der **Zollverein** umfasst seit 1834 Preußen, die
Mittelstaaten des Südens und Thüringen, nicht aber Österreich, Hannover, Mecklenburg und die
Hansestädte. Zwischen Wien und Berlin entscheidet sich in den nächsten Jahren die deutsche Frage,
zuerst diplomatisch (Erfurter Union, Olmütz 1850), dann militärisch (1866).

M9 bringt dafür zwei Dinge in die Simulation:
- den **Zollverein** als Wirtschaftssystem: Zollgebiete, Zoll im Handel, gemeinsame Kasse, Beitritt
  und Austritt;
- die **Diplomatie** der deutschen Staaten: Beziehungen, Bündnisse und Garantien, die
  Bundesversammlung und diplomatische Spiele um die Führung im Bund.

Krieg (M10), Ereignisse und Revolution (M11) und die KI der Staaten (M12) bauen darauf auf.

## 1. Umfang

Der Meilensteinplan verlangt:

- Beziehungen, Bündnisse, Garantien, Bundesversammlung des Deutschen Bundes
- Zollverein: Beitritt, Austritt, Zolleinnahmen, Wirkung auf die Märkte aus M5
- Diplomatische Spiele zur deutschen Frage (kleindeutsch, großdeutsch)
- Abnahme: Zollverein-Mitgliedschaften 1848 historisch korrekt; Beitritte verändern Handel und
  Kassen messbar.

Vorschlag (Entscheidung 1): **zwei Teile** wie bei M7 und M8.

| Teil | Inhalt | Ergebnis |
|---|---|---|
| **M9a** | Zollunionen als Daten, Zollgebiete je Markt, Zoll als Handelshemmnis, Vereinskasse mit Verteilung nach Bevölkerung, Beitritt und Austritt, Kommandozeile `zoll` mit Beitrittsvergleich, Kalibrierung, Kartenmodus „Zollgebiete“ | Abnahme der Zollverein-Kriterien, in der CI |
| **M9b** | Beziehungen, Gesandte, Bündnisse und Garantien, Bundesversammlung mit Anträgen und Abstimmungen, diplomatische Spiele, Anzeige „Die deutsche Frage“; Doku und Review | Diplomatie im Spiel bedienbar |

**Nicht in M9:**
- Krieg, Mobilmachung und Bündnisfall mit Folgen (M10). Bündnisse und Garantien sind in M9 Verträge
  mit Wirkung auf Beziehungen und diplomatische Spiele.
- Die Außenmächte Frankreich, Russland und Großbritannien als Akteure (Entscheidung 10).
- Ereignisse der Revolution: Vorparlament, Reichsverweser, Erfurter Union als Ereigniskette (M11).
  M9 liefert die Mechanik, M11 die historischen Anlässe.
- Entscheidungen der Staaten ohne Spieler über einfache Regeln hinaus (M12).
- Handelsverträge außerhalb der Zollunionen, Münzverträge, Zollparlament (1868).
- Heirat, Erbfolge und Reichspolitik vor 1806 (Abschnitt 2 von Kapitel 14).

## 2. Ausgangslage

**Vorhanden:**
- Seit M7 trägt jeder Staat das datierte Feld `zollverein` (ja/nein), bisher ohne Wirkung. 1848
  sind 26 der 38 Staaten Mitglied, darunter Preußen, Bayern, Sachsen, Württemberg, Baden, beide
  Hessen, Nassau, Thüringen, Braunschweig, Lippe, Waldeck, Frankfurt und Luxemburg. Nicht Mitglied
  sind Österreich, Hannover, Oldenburg, Schaumburg-Lippe, beide Mecklenburg, Hamburg, Bremen,
  Lübeck, Liechtenstein, Dänemark (Holstein, Lauenburg) und die Niederlande (Limburg).
- Seit M5 ist der **Zoll** ein Aufschlag auf den Einfuhranteil `e_g` der Waren im Markt
  (`Q_g = P_g × (1 + akzise) × (1 + zoll × e_g)`, [wirtschaft.md](wirtschaft.md), Abschnitt 4.3).
  Es gilt der Satz des Besitzers des Ortes; 1848 überall 3 %. Der Zoll bremst den Handel bisher
  nicht, und jede Einfuhr zählt, auch aus einem Markt desselben Staates.
- Zoll ist 1848 eine kleine Einnahme: 226.000 von 3,93 Mio. Talern je Monat (5,7 %), Preußen
  90.000 von 1,35 Mio. (gemessen nach 60 Tagen).
- **Märkte** sind Landschaften, keine Staaten (77, davon 63 mit Daten 1848). Nur 8 Märkte mischen
  Zollgebiete (weniger als 95 % in einem Gebiet): Braunschweig (Hannover 17 %), Bremen-Verden,
  Calenberg, Göttingen, Holstein, Mecklenburg, Trier (Oldenburgs Birkenfeld 7 %) und Vorarlberg
  (Liechtenstein 5 %). Die großen Zollgrenzen laufen zwischen Märkten.
- Handel zwischen Nachbarmärkten nach Preislücke und Fracht; Häfen handeln mit dem Weltmarkt.
- Das Heer ist eine Zahl je Staat (Soldaten je 1.000 Einwohner), nur als Kosten.
- Bundesgebiet je Ort; Parlamente und Wahlen seit M8.

**Es fehlt:** jeder Zustand zwischen zwei Staaten, eine Kasse über mehreren Staaten und eine Wirkung
der Zollgrenzen auf den Handel.

**Quellen:** wie in M7 und M8 ohne Seitenzugriff. Die Mitgliedschaften 1848 und die Daten der
Beitritte sind gut bekannt (Kapitel 08, Abschnitt 5.3); Tarifhöhen, Stimmen der Bundesversammlung
und Ausgangsbeziehungen sind Spielwerte nach Literaturkenntnis, `ungeprueft` (Entscheidung 11).

## 3. Zollunionen und Zollgebiete (M9a)

### 3.1 Daten

Neue Datei `common/zollunionen.chron` (Format: [chronik-skript.md](chronik-skript.md), Abschnitt 3.25):

```text
zollunion zollverein {
    name    = "Zollverein"   # steht in der Chronik: „Hannover tritt dem Zollverein bei“
    seit    = 1834.01.01
    leitung = PRU            # Leitmacht: entscheidet über Beitritte (Abschnitt 3.5), selbst Mitglied
    tarif   = 0.03           # Vereinstarif, gilt für alle Mitglieder (Spielwert)
    quelle  = "…"
}

zollunion steuerverein {
    name    = "Steuerverein"
    seit    = 1834.05.01
    leitung = HAN
    tarif   = 0.03
    quelle  = "…"
}
```

Die Einnahmen werden nach der Bevölkerung verteilt (Abschnitt 3.4, Entscheidung 4); ein Feld dafür
gibt es nicht, solange es nur diesen Schlüssel gibt.

Das Staatsfeld `zollverein = ja/nein` wird zu **`zollunion = <kennung>`** (`keine`: eigenes
Zollgebiet), datiert wie bisher. Der Archivar übernimmt die Werte aus M7; dazu kommen die Mitglieder
des **Steuervereins** (Entscheidung 6): Hannover, Oldenburg (seit 1836) und Schaumburg-Lippe (seit
1838). Braunschweig wechselte 1841/44 in den Zollverein.

Ein **Zollgebiet** ist eine Zollunion oder ein Staat ohne Union. 1848 sind das elf: Zollverein,
Steuerverein und neun Staaten für sich (Österreich, Liechtenstein, beide Mecklenburg, Hamburg,
Bremen, Lübeck, Dänemark, die Niederlande).

### 3.2 Anteile je Markt

Am Monatsersten, vor dem Handel: `s_m(z)` = Anteil des Zollgebiets *z* an der Bevölkerung des
Marktes *m* (Orte mit Daten). In 55 von 63 Märkten ist ein Gebiet fast allein (über 95 %); in den
übrigen acht teilen die Anteile Handel und Einnahmen auf (Entscheidung 2). Grenzen innerhalb eines
Marktes zählen nicht: Der Markt bleibt ein gemeinsamer Handelsplatz.

### 3.3 Zoll im Handel

Eine Ware, die von Markt *m* nach *n* geht, zahlt den Tarif des Gebiets, in dem sie gekauft wird,
wenn sie aus einem anderen Gebiet kommt. Erwarteter Tarif:

```
t_mn   = Σ_z s_n(z) × (1 − s_m(z)) × tarif(z)
lücke  = P_n,g − P_m,g × (1 + fracht + t_mn)            (statt 1 + fracht, wirtschaft.md 4.1)
t_Welt = Σ_z s_n(z) × tarif(z)                           (Einfuhr aus dem Weltmarkt)
```

`tarif(z)` ist der Vereinstarif einer Union oder der Zollsatz eines Staates ohne Union (der Regler
des Spielers). Innerhalb des Zollvereins ist `t_mn` null; zwischen Hannover und Preußen etwa der
Tarif. Der Zoll wirkt so wie zusätzliche Fracht: Er schließt kleine Preislücken und verringert den
Handel über die Grenze (Entscheidung 3).

### 3.4 Einnahmen und Vereinskasse

Der Zoll bleibt ein Aufschlag beim Kauf (wirtschaft.md 4.3), aber nur auf den **fremden**
Einfuhranteil des Gebiets, in dem der Ort liegt:

```
e_g,z = (Σ_m Einfuhr_m→n,g × (1 − s_m(z)) + Welteinfuhr_n,g) / A'_n,g
Q_g   = P_g × (1 + akzise) × (1 + tarif(z) × e_g,z)
```

- **Staat ohne Union:** Der Zoll seiner Orte geht wie bisher in seine Kasse.
- **Mitglied einer Union:** Der Zoll geht in die **Vereinskasse** (Zustand der Union). Am
  Monatsersten, nach den Märkten, sammelt sie den zuletzt festgestellten Zoll der Orte aller
  Mitglieder und verteilt ihn nach der Bevölkerung (Bevölkerungsschlüssel, Entscheidung 4); der
  Anteil ist die Einnahme „Zoll“ im Haushalt jedes Mitglieds an seinem Rechentag.

Wirkung: Kleine Mitglieder ohne eigenen Hafen und ohne Außengrenze bekommen mehr als in ihren
eigenen Orten anfällt; Staaten mit viel Einfuhr (Häfen, Grenzmärkte) geben ab. Das war historisch der
Anreiz für die kleinen Staaten.

### 3.5 Beitritt und Austritt

Neuer Befehl **`Zollunion(staat, union)`**; `union = kNone` heißt Austritt.

- **Beitritt** braucht die Zustimmung der Union (Einstimmigkeit, Kapitel 08). Bis zur KI (M12)
  entscheidet die **Leitmacht** nach einer Regel: Sie stimmt zu, wenn ihre Beziehung zum
  Antragsteller mindestens 0 ist (Abschnitt 4.1; in M9a ohne Beziehungen: immer), und lehnt die
  Staaten in `ausgeschlossen` ab (Daten: Österreich beim Zollverein). Ist der Spieler die Leitmacht,
  entscheidet er selbst über Anträge anderer, sobald es solche gibt (M12).
- **Austritt der Leitmacht** (seit Schritt a3): Tritt die Leitmacht aus oder wechselt sie, löst sich
  ihre Union zum selben 1. Januar auf; die übrigen Mitglieder werden eigene Zollgebiete (Hannover 1854:
  Ende des Steuervereins). Einer aufgelösten Union kann nur ihre Leitmacht wieder beitreten. (In
  Schritt a2 konnte die Leitmacht nicht austreten; damit wäre der historische Beitritt Hannovers
  unmöglich gewesen.)
- **Wirksam** zum nächsten 1. Januar (Vertragsjahr, Entscheidung 5); bis dahin „beantragt“.
- **Austritt** ohne Zustimmung, ebenfalls zum nächsten 1. Januar. Historisch liefen die Verträge
  zwölf Jahre (1834, 1842, 1854); eine Kündigungsfrist bis zum Ende der Periode wäre genauer, aber im
  Spiel zu träge.
- Mitglieder können ihren Zoll nicht selbst einstellen: Der Regler zeigt den Vereinstarif.
  Änderungen des Tarifs (Generalkonferenz, Einstimmigkeit) kommen mit M11 und M12.
- Ein neuer Antrag ersetzt einen offenen; ein Antrag auf die bisherige Union zieht ihn zurück. Ein
  Wechsel von einer Union in die andere ist ein Antrag (Austritt und Beitritt zum selben Tag).
- Chronik bei der Entscheidung: „Hannover tritt zum 1. Januar 1849 dem Zollverein bei.“, „Baden tritt
  zum 1. Januar 1849 aus dem Zollverein aus.“, „Hannover verlässt zum 1. Januar 1849 den Steuerverein
  und tritt dem Zollverein bei; der Steuerverein löst sich auf.“, „Preußen lehnt den Beitritt von
  Österreich zum Zollverein ab.“

### 3.6 Messung und Abnahme

Neuer Befehl **`uhrwerk-cli zoll`** (Zolllabor) nach dem Vorbild von `wahl`:

```
uhrwerk-cli zoll --szenario voelkerfruehling [--beitritt TAG[:UNION]] [--austritt TAG]
                 [--jahre N] [--seeds N] [--ziele DATEI]
```

Er rechnet je Seed einen Lauf ohne Wechsel und je Wechsel einen Lauf mit ihm (Befehl `Zollunion` am
ersten Tag, wirksam zum ersten 1. Januar), jeweils bis zum 1. Januar N Jahre danach (Standard 5), und
misst im letzten Jahr an jedem Monatsersten (Kern: `uhrwerk/customs_lab.hpp`, Ströme über
`Simulation::record_trade`, kein Zustand):
- **Grenzhandel:** Wert der Ströme zwischen den Märkten des Staates und denen der übrigen Mitglieder
  der Union (beim Austritt: der bisherigen), gewichtet mit den Bevölkerungsanteilen zum Start;
- **Handel:** Wert aller Ströme der Märkte des Staates nach seinem Anteil, auch mit dem Weltmarkt;
- **Zoll:** Zolleinnahme des Staates im Haushalt;
- **Preise:** Preis ÷ Basispreis in den Märkten des Staates, nach seiner Bevölkerung gewichtet;
- im Lauf ohne Wechsel der **Zollanteil** jeder Union: Zoll ÷ Einnahmen ihrer Mitglieder.

Ausgegeben werden die Faktoren „mit ÷ ohne Wechsel“ als Median mit 5- und 95-%-Wert über die Seeds.
Ohne `--beitritt` und `--austritt` rechnet das Labor die Wechsel aus den Zollzielen
(`common/zollziele_<szenario>.chron`, Format: [chronik-skript.md](chronik-skript.md), Abschnitt 3.26).

**Abnahme:**
1. Die Mitgliedschaften 1848 stimmen mit der Liste in Abschnitt 2 und den Beitrittsdaten aus
   Kapitel 08 überein (Test gegen eine Tabelle).
2. Beitrittsvergleich in der CI, z. B. Hannover und Mecklenburg-Schwerin in den Zollverein, über
   fünf Jahre: Der Handel über die frühere Zollgrenze steigt, die Zolleinnahme des Staates ändert
   sich, beides deutlich über dem Rauschen der Seeds. Die Zielwerte stehen in einer Datei wie bei
   der Wahl (`zollziele_voelkerfruehling.chron`).
3. Der Balancing-Bericht 1848 bleibt in allen Zielwerten (Kassen, Preise, Bevölkerung).

### 3.7 Godot

- **Kartenmodus „Zollgebiete“** (Modus 13): Farbe je Zollgebiet, Unionen kräftig, Einzelstaaten
  blass; Legende mit Unionen und Mitgliederzahl. Kapitel 08: „1648 ist die Karte ein Gewirr;
  1834 entsteht ein großer, einfarbiger Block.“
- **Staatspanel:** Zollunion des Staates; der Zollregler zeigt bei Mitgliedern den Vereinstarif
  und ist gesperrt; im Haushaltsfenster der Anteil aus der Vereinskasse.
- **Knopf** „Zollunion …“ (umgesetzt statt „Zollverein beitreten …“, weil es zwei Unionen gibt):
  Fenster mit Zollgebiet, Tarif und Kassenanteil des Staates und je Union „Beitreten …“ bzw.
  „Austreten …“, dazu „Antrag zurückziehen …“; jede Aktion mit Rückfrage und Zeitpunkt. Was die
  Leitmacht ablehnen würde, ist vorab gesperrt („Preußen lehnt ab“; Brücke `get_customs_change`).
- Ortsinfo: Zollgebiet des Ortes.

## 4. Diplomatie (M9b)

### 4.1 Beziehungen

Ein Wert **Beziehung** je Staatenpaar, −200 bis +200, symmetrisch (Entscheidung 7). Vertrauen,
Bedrohung und Einfluss aus Kapitel 14 kommen später; sie brauchen Krieg und KI.

**Zielwert und Drift:** Jede Beziehung bewegt sich monatlich um 5 % auf einen Zielwert zu:

| Anteil | Wert (Spielwert) |
|---|---|
| Grundwert aus den Daten (`beziehungen.csv`, sonst 0) | z. B. Österreich–Preußen −40 (Dualismus), Sachsen–Österreich +30 |
| gemeinsame Zollunion | +30 |
| beide im Deutschen Bund | +10 |
| gleiche Konfession der Staaten (Herrscherhäuser; lutherisch und reformiert zählen gleich) | +10 |
| Bündnis, Garantie | +40, +20 |
| Gesandter (Abschnitt 4.2) | +5 je Monat, bis +60 über dem Ziel |
| diplomatisches Spiel auf verschiedenen Seiten | einmalig −30 |

Zum Start steht jede Beziehung auf ihrem Zielwert. Die Werte und Regeln stehen in
`common/diplomatie.chron` (chronik-skript.md 3.27), die Grundwerte in `tabellen/beziehungen.csv`
(tabellen.md). Diplomatie gilt nur in Szenarien mit Deutschem Bund; 1648 bleibt bitgleich.

**Lager:** Für jeden Staat ist die Neigung zu Preußen oder Österreich die Differenz seiner beiden
Beziehungen. Sie bestimmt Stimmen und Parteinahme (Abschnitte 4.4 und 4.5).

### 4.2 Gesandte

Befehl **`Gesandter(staat, ziel)`**: Ein Gesandter verbessert die Beziehung zum Ziel um 5 je Monat
(bis zur Grenze) und kostet 0,5 % der Monatseinnahmen (Haushaltsposten „Gesandte“). Derselbe Befehl
ruft ihn ab; je Ziel höchstens ein Gesandter. Zahl der Gesandten je Staat: 1 + 1 je 5 Mio.
Einwohner im Modell (Preußen 4, Österreich 3, die übrigen 1). Kapitel 14 nennt das die
diplomatische Kapazität.

### 4.3 Bündnisse und Garantien

Befehl **`Vertrag(staat, ziel, art)`** mit den Arten Bündnis (gegenseitig) und Garantie (einseitig:
der Staat schützt das Ziel), dazu Kündigung.

- **Annahme** durch das Ziel nach Regel (bis M12): Bündnis ab Beziehung 100, Garantie ab 50; kein
  Bündnis mit einem Staat, der mit dem eigenen Gegner verbündet ist (in beiden Richtungen geprüft).
  Gegner sind Staaten mit einer Beziehung unter −25 (Spielwert; 1848 Österreich–Preußen und
  Dänemark–Preußen). Zwischen Verbündeten gibt es keine Garantie.
- **Kündigung:** ein Bündnis durch jede der beiden Seiten, eine Garantie durch den Garanten; sofort,
  ohne weitere Folgen in M9.
- **Wirkung in M9:** Zielwert der Beziehung, Parteinahme in diplomatischen Spielen (ein Verbündeter
  steht immer auf der eigenen Seite, ein Garant auf der Seite des Geschützten). Den Bündnisfall im
  Krieg bringt M10.
- 1848 bestehen keine Bündnisse unter den Bundesstaaten außer dem Bund selbst (Bundesakte, Art. 11).

### 4.4 Bundesversammlung

Die **Bundesversammlung** in Frankfurt (Präsidialmacht Österreich) ist ein Gremium mit Stimmen je
Staat aus den Daten (`common/bundesversammlung.chron`):
- **Engerer Rat:** 17 Stimmen, elf Virilstimmen und sechs Kuriatstimmen (Gruppen kleiner Staaten
  stimmen gemeinsam; ihre Mehrheit entscheidet die Stimme). Einfache Mehrheit.
- **Plenum:** rund 70 Stimmen, gewichtet nach Größe. Zwei Drittel für Grundgesetze des Bundes,
  Einstimmigkeit für Änderungen der Bundesakte.

**Anträge** (Befehl `Bundesantrag(art, ziel)`, Entscheidung 8), je Monat höchstens einer im Bund:

| Antrag | Gremium | Wirkung in M9 |
|---|---|---|
| Bundesreform (Volksvertretung beim Bund) | Plenum, einstimmig | Anzeige „Deutsche Frage“ (Abschnitt 4.6); M11 knüpft daran an |
| Zolleinigung des Bundes | Plenum, zwei Drittel | alle Bundesstaaten werden Mitglied einer Union nach Wahl des Antragstellers |
| Bundesintervention gegen einen Staat | Engerer Rat | Beziehungen; Vollzug mit M10 |
| Präsidium (Wechsel des Vorsitzes) | Plenum, einstimmig | Leitung des Bundes |

**Stimmen** ohne Spieler nach Regel: Ein Staat stimmt mit Ja, wenn seine Beziehung zum
Antragsteller höher ist als zum stärksten Gegner des Antrags (für Zolleinigung: die Leitmacht der
anderen Union; für Bundesreform: Österreich als Präsidialmacht). Die Abstimmung läuft am nächsten
Monatsersten und steht in der Chronik.

Präzisiert in Schritt b2:
- **Gegner des Antrags:** Bundesreform und Präsidium die Präsidialmacht, Intervention ihr Ziel,
  Zolleinigung der bevölkerungsreichste Bundesstaat außerhalb der Union, der keiner anderen Union bloß
  als Mitglied angehört (eigenes Zollgebiet oder Leitmacht; 1848 also Österreich, nicht nur Hannover).
  Ist der Antragsteller selbst dieser Staat, gilt der bevölkerungsreichste übrige Bundesstaat. Der
  Antragsteller stimmt mit Ja, der Gegner mit Nein.
- **Einstimmigkeit** heißt: auch der Gegner müsste zustimmen. Nach Regel scheitern Bundesreform und
  Präsidium daher immer (wie historisch); sie stehen als Versuche in der Anzeige, M11 knüpft daran an.
- **Engerer Rat:** In einer Kurie entscheidet die Mehrheit ihrer Plenarstimmen, Gleichstand ist Nein.
  Angenommen ist ein Antrag mit mehr als der Hälfte der Stimmen.
- **Wirkungen:** Bundesintervention: das Ziel verliert einmalig 20 Punkte Beziehung zu jedem Staat, der
  dafür stimmte. Zolleinigung: Jeder Bundesstaat außerhalb der Union tritt ihr zum nächsten 1. Januar
  bei, ohne Vorbehalt der Leitmacht. Präsidium: Der Antragsteller wird Präsidialmacht.
- **Ein Antrag zur Zeit:** Solange ein Antrag offen ist, nimmt die Bundesversammlung keinen weiteren
  an. Ein Antrag vom Monatsersten wird am folgenden Monatsersten abgestimmt.
- **Daten:** `common/bundesversammlung.chron` (chronik-skript.md 3.29): 68 Plenarstimmen und
  17 Stimmen im Engeren Rat für die 38 Staaten von 1848; die Stimme für Luxemburg und Limburg teilen
  sich Luxemburg und die Niederlande.

Historisch gab die Bundesversammlung ihre Befugnisse im Juli 1848 an den Reichsverweser ab und kehrte
1850/51 zurück. Diese Wechsel sind Ereignisse (M11); in M9 tagt sie durchgehend.

### 4.5 Diplomatische Spiele

Ablauf nach Kapitel 14, Abschnitt 5, ohne Krieg (Entscheidung 9):

1. **Forderung:** Ein Staat stellt eine Forderung an einen anderen. M9 kennt drei:
   - **Führung im Bund** (kleindeutsch: eine Union unter Preußens Führung, großdeutsch: der Bund
     unter Österreichs Führung) – Vorbild Erfurter Union und Olmütz 1849/50;
   - **Beitritt zu einer Zollunion** (der Gegner tritt bei oder zahlt);
   - **Bündnis lösen** (der Gegner kündigt ein Bündnis).
2. **Parteinahme** der übrigen Bundesstaaten nach Regel: Bündnis und Garantie zuerst, sonst das
   Lager, wenn die Neigung über 20 liegt, sonst neutral.
3. **Eskalation** in drei Stufen zu je 30 Tagen: Protest, Ultimatum, Mobilmachung. Jede Stufe
   senkt die Beziehungen zwischen den Seiten und kostet auf der Stufe Mobilmachung Geld (Heer
   × 1,5 je Monat).
4. **Manöver** des Spielers je Stufe: nachgeben, Vermittlung anbieten (Kompromiss: halbe Wirkung),
   weiter eskalieren.
5. **Auflösung:** Nach der dritten Stufe gibt die schwächere Seite nach, wenn ihre Stärke unter
   zwei Dritteln der anderen liegt (Stärke = Soldaten der Seite); sonst endet das Spiel mit einem
   Kompromiss. Krieg statt Auflösung kommt mit M10.

Die Seiten ohne Spieler eskalieren nach Regel, solange ihre Stärke mindestens so groß ist wie die der
Gegenseite.

Präzisiert in Schritt b2:
- **Kein Spieler im Kern:** Jede Hauptpartei entscheidet am Ende einer Stufe nach Regel, außer sie hat
  in dieser Stufe das Manöver „eskalieren“ befohlen. Nach Regel eskaliert die stärkere Seite; die
  schwächere bietet Vermittlung an, und die Gegenseite nimmt an, wenn die anbietende Seite mindestens
  zwei Drittel ihrer Stärke hat (Kompromiss), sonst geht das Spiel weiter. Manöver sind Befehle
  (`Manoever`): nachgeben (sofort), Vermittlung (dieselbe Annahmeregel), eskalieren (für die laufende
  Stufe). So bleiben Wiederholungen deterministisch, und der Spieler entscheidet nur, wenn er will.
- **Parteinahme:** relativ zu den beiden Hauptparteien (Lager = Unterschied der Beziehungen zu ihnen),
  festgelegt zu Beginn des Spiels; nur Bundesstaaten.
- **Beziehungen:** zu Beginn einmalig −30 zwischen allen Staaten auf verschiedenen Seiten, je weiterer
  Stufe −10 zwischen den Hauptparteien. Mobilmachung: Heereskosten der beiden Hauptparteien × 1,5.
- **Ausgang** (halbe Wirkung beim Kompromiss): Führung im Bund – Sieg oder Kompromiss stehen in der
  Anzeige „Die deutsche Frage“; Zollunion – Beitritt zum nächsten 1. Januar (ohne Vorbehalt der
  Leitmacht), beim Kompromiss zahlt der Gegner ein halbes Monatseinkommen; Bündnis lösen – der Gegner
  kündigt seine Bündnisse mit Gegnern des Fordernden, beim Kompromiss belastet −30 diese Bündnisse.
- Ein Staat ist höchstens in einem Spiel Hauptpartei; die Forderung „Zollunion“ verlangt eine Union
  des Fordernden, „Bündnis lösen“ ein Bündnis des Gegners mit einem Gegner des Fordernden.

### 4.6 Die deutsche Frage (Anzeige)

Ein Fenster zeigt die Lage, nach der M13 die Ausgänge des Journals „Die deutsche Frage“ bewertet
(Kapitel 14, Abschnitt 3):

- Anteil des Zollvereins an der Bevölkerung des Bundes;
- Lager: Bevölkerung der Bundesstaaten mit Neigung zu Preußen, zu Österreich, unentschieden;
- Stand der Bundesreform (Anträge, Ergebnisse) und der Führung im Bund (gewonnene Spiele);
- Bündnisse und Garantien als Netz.

In M9 ist das eine Anzeige ohne Wertung; die sechs Ausgänge kommen mit M13.

## 5. Zustand, Kern und Spielstand

- **Neuer Zustand** (Zustandsverzeichnis, Prüfsumme, Spielstand):
  - je Union die Vereinskasse des Monats; je Staat seine Zollunion und sein Anteil an der Kasse;
    die beantragten Wechsel; je Markt die Zollgebiete mit Anteil und fremdem Einfuhranteil
    (seit Schritt a2, nur in Welten mit Zollunionen);
  - je Staatenpaar die Beziehung (Fixed, 38 × 37 / 2 = 703 Paare 1848);
  - Gesandte, Verträge, offene Bundesanträge und laufende diplomatische Spiele.
- **Neue Befehle:** `Zollunion`, `Gesandter`, `Vertrag`, `Bundesantrag`, `Forderung`,
  `Manoever`. Sie laufen wie `Wahlkampf` über das Befehlsprotokoll (Spielstand **Format 3**).
- **Inhaltsfingerabdruck:** neuer Bereich `diplomatie` (Zollunionen mit Leitung, Tarif und
  ausgeschlossenen Staaten, Grundbeziehungen, Stimmen). Die Mitgliedschaft der Staaten ist Zustand
  (Befehl `Zollunion`) und steht nicht im Fingerabdruck.
- **Kein neuer Zufallsstrom:** Alle Regeln sind deterministisch.
- **1648 bitgleich** (Entscheidung 11): Zollgebiete gelten erst mit einer Zollunion im Szenario,
  Diplomatie nur mit einer Bundesversammlung. Ohne sie rechnet der Zoll wie in M5, und der neue
  Zustand wird nicht besucht. Reichspolitik und Diplomatie vor 1806 kommen mit dem übrigen
  Kernraum.
- **Leistung:** Anteile je Markt und Tarife einmal im Monat (63 × 11 Werte); Beziehungen einmal im
  Monat (703 Paare). Vernachlässigbar.

## 6. Brücke und Godot (M9b)

- **Brücke:** `get_customs_unions`, `get_relations(state)`, `get_treaties`, `get_diet` (Stimmen,
  Anträge), `get_diplomatic_plays`, `get_german_question`; Befehle wie in Abschnitt 5; Kartenmodus 13
  `MAP_ZOLLGEBIETE`, dazu 14 `MAP_BEZIEHUNGEN` (Beziehung des Spielerstaates zu jedem Staat, rot bis
  grün).
- **Godot:**
  - Fenster **Diplomatie** (Knopf im Staatspanel): Liste der Staaten mit Beziehung, Lager, Verträgen
    und Gesandten; Aktionen je Staat.
  - Fenster **Bundesversammlung**: Stimmenverteilung, Anträge, letzte Abstimmungen als Halbrund wie
    beim Wahlabend.
  - **Diplomatisches Spiel** als Leiste über der Karte mit Seiten, Stärke, Stufe und Knöpfen.
  - Fenster **Die deutsche Frage** (Abschnitt 4.6).

Präzisiert in Schritt b3:

- **Brücke** (bridge-api.md, Abschnitt Diplomatie): statt `get_treaties` stehen die Verträge in
  `get_relations` (je Staat) und `get_german_question` (alle); dazu `get_diplomacy` (Gesandte,
  Schwellen, Krise, Mobilmachung), Vorab-Prüfungen ohne Wirkung (`get_diplomacy_actions`,
  `get_motion_preview` mit der Auszählung von heute) und je Befehl eine Methode (`send_envoy`,
  `propose_treaty`, `file_motion`, `make_demand`, `manoeuvre`). `get_state` meldet `diplomacy` und
  den Posten `envoy_cost_month`.
- **Ein Fenster mit drei Reitern** (Staaten, Bundesversammlung, Deutsche Frage) statt dreier
  Fenster, rechts neben der linken Spalte; der Knopf „Diplomatie …“ im Staatspanel erscheint nur
  mit dem Deutschen Bund, im Parteienmodus schließt das Fenster. Die Staatenliste ist nach der
  Beziehung geordnet, mit Farbfeld wie im Kartenmodus. Handlungen, die der Kern ablehnen würde,
  sind gesperrt und nennen im Tooltip den Grund; eine Forderung fragt mit den Seiten der Krise
  nach. Die Bundesversammlung bietet Bundesreform, Vorsitz und die Zolleinigung der eigenen Union
  mit Prognose an, die Intervention steht beim gewählten Staat.
- **Lager:** Ein Staat neigt zu Preußen oder Österreich, wenn seine Beziehung zu ihm um mehr als 20
  besser ist (wie die Neigung in Abschnitt 4.5); das Halbrund zeigt die Plenarstimmen nach Lagern.
- **Krisenleiste** mittig unter der oberen Leiste (das eigene Spiel zuerst, sonst das erste); die
  Knöpfe nur für die Hauptparteien; „Vermittlung anbieten“ sagt im Tooltip, ob die Gegenseite
  annimmt. Das offene Fenster rückt unter die Leiste, die Staatenliste wird dann kürzer.
- **Kartenmodus 14 „Beziehungen“** (Taste #): vom Papierton zu Grün (+100) bzw. Rot (−100), der
  Spielerstaat in seiner Farbe; die Legende zählt die Staaten in fünf Bändern.

## 7. Tests und Abnahme

| Kriterium | Prüfung |
|---|---|
| Zollverein-Mitgliedschaften 1848 historisch korrekt | Test gegen die Liste in Abschnitt 2 (Kern), `info` nennt Unionen und Mitglieder |
| Beitritte verändern Handel und Kassen messbar | `uhrwerk-cli zoll` mit Zielwerten in der CI (Abschnitt 3.6) |

Weitere Tests:
- **Einheitentests:** Anteile je Markt, Tarif im Handel (innerhalb einer Union 0), Vereinskasse und
  Verteilung, Beitritt zum 1. Januar, Austritt, gesperrter Zollregler; Beziehungen mit Drift,
  Gesandte, Verträge, Abstimmungen mit Kuriatstimmen, diplomatisches Spiel mit Nachgeben und
  Kompromiss.
- **Determinismus und Spielstände:** gleiche Prüfsumme mit 1 und 4 Threads; Speichern während eines
  Spiels und eines beantragten Beitritts; Formatwache.
- **1648 bitgleich:** `5db13a66d63e6079` nach 100 Jahren.
- **Godot:** Rauchtest der neuen Brückenmethoden, Klicktest (Beitritt über den Knopf, Gesandter,
  Antrag, Spiel bis zur Auflösung), Screenshots der Kartenmodi und Fenster.

## 8. Daten

| Datei | Inhalt |
|---|---|
| `common/zollunionen.chron` | Zollverein und Steuerverein mit Leitung und Tarif |
| `tabellen/staaten_historie.csv` | Feld `zollunion` statt `zollverein`, datiert (Steuerverein neu) |
| `tabellen/beziehungen.csv` (neu) | Grundbeziehungen für einzelne Paare mit Quelle (Dualismus, Dynastien, Konfessionsblöcke) |
| `common/bundesversammlung.chron` | Stimmen im Engeren Rat (mit Kurien) und im Plenum, Präsidialmacht |
| `common/zollziele_voelkerfruehling.chron` | Zielwerte des Zolllabors |

Der Archivar prüft `zollunion` (bekannte Union, nicht vor ihrem `seit`) und `beziehungen.csv`
(bekannte Staaten, Wert im Bereich, kein Paar doppelt).

## 9. Entscheidungen (wie empfohlen freigegeben)

1. **Teilung M9a/M9b.** Empfehlung: ja (Abschnitt 1). Alternative: Diplomatie zuerst, dann
   Zollverein.
2. **Zollgebiete je Markt.** Empfehlung: **Bevölkerungsanteile je Markt** (Abschnitt 3.2); Grenzen
   innerhalb eines Marktes zählen nicht. Alternativen:
   - jeder Markt ganz in einem Gebiet (nach Mehrheit): einfacher, aber der Beitritt eines kleinen
     Staates ändert dann oft nichts;
   - Märkte an Zollgrenzen teilen: genauer, aber neue Märkte und eine neue Kalibrierung.
3. **Zoll als Handelshemmnis.** Empfehlung: **ja**, als Aufschlag auf die Fracht (Abschnitt 3.3),
   mit den Sätzen aus den Daten (1848: 3 %). *Umgesetzt mit der Alternative (Schritt a3, nach Rückfrage):
   Vereinstarif 10 % für den Zollverein, weil der Zoll mit 3 % auf 1 % der Einnahmen fiel.* Alternative: ein höherer Vereinstarif nach den
   historischen Sätzen (rund 10 % auf Fertigwaren); dann muss der Haushalt 1848 neu kalibriert
   werden, weil der Zoll dann gut das Dreifache einbringt.
4. **Verteilung der Vereinskasse.** Empfehlung: **nach Bevölkerung** (historisch). Alternative: nach
   dem Zoll, der in den Orten jedes Mitglieds anfällt (dann bringt der Beitritt einem kleinen Staat
   wenig).
5. **Zeitpunkt von Beitritt und Austritt.** Empfehlung: **zum nächsten 1. Januar**. Alternative:
   sofort zum Monatsersten; oder Austritt nur zum Ende der Vertragsperiode (1853).
6. **Steuerverein.** Empfehlung: **als zweite Union** (Hannover, Oldenburg, Schaumburg-Lippe).
   Alternative: nur der Zollverein, die anderen Staaten je für sich.
7. **Umfang der Beziehungen.** Empfehlung: **ein symmetrischer Wert** mit Zielwert und Drift,
   Gesandte, Bündnis und Garantie. Alternative: alle vier Werte aus Kapitel 14 (Beziehung,
   Vertrauen, Bedrohung, Einfluss) schon jetzt.
8. **Bundesversammlung.** Empfehlung: **Anträge mit Abstimmung** nach Regeln (vier Arten,
   Abschnitt 4.4). Alternative: nur Anzeige der Stimmen bis M11.
9. **Diplomatische Spiele ohne Krieg.** Empfehlung: **ja**, Auflösung nach Stärke wie in Olmütz
   1850 (Abschnitt 4.5); M10 ersetzt die letzte Stufe durch Krieg. Alternative: diplomatische Spiele
   erst mit M10.
10. **Außenmächte.** Empfehlung: **nicht in M9**; Frankreich, Russland und Großbritannien kommen mit
    M10 und M12 als Akteure ohne Orte. Alternative: schon jetzt als feste Unterstützer in Spielen.
11. **1648 und Quellen.** Empfehlung: **1648 bitgleich** (Zollgebiete und Diplomatie nur mit Union
    bzw. Bundesversammlung im Szenario); Tarife, Stimmen und Grundbeziehungen als `ungeprueft`
    bzw. `spielwert`, Fachprüfung mit M7 und M8. Alternative: neue Referenzprüfsumme für 1648 mit
    Zollgrenzen zwischen allen Staaten.
12. **Deutsche Frage.** Empfehlung: **Anzeige der Lage** in M9, Ausgänge in M13. Alternative: schon
    jetzt eine Wertung je Ausgang.

## 10. Umsetzungsschritte

| Schritt | Inhalt | Prüfung |
|---|---|---|
| a1 | **Daten:** `zollunionen.chron`, Feld `zollunion`, Steuerverein; Lader, Archivar, Fingerabdruck, `info` | Mitgliedschaften 1848 als Test, `pruefen` für beide Szenarien |
| a2 | **Kern:** Anteile je Markt, Tarif im Handel, fremder Einfuhranteil, Vereinskasse, Befehl `Zollunion` zum 1. Januar, Spielstand Format 3 | Einheitentests, Determinismus, 1648 bitgleich |
| a3 | **Zolllabor:** `uhrwerk-cli zoll`, Zielwerte, Kalibrierung, Balancing-Bericht 1848, CI | Zielwerte erfüllt |
| a4 | **Godot:** Kartenmodus „Zollgebiete“, Staatspanel (Union, Vereinstarif, Beitritt) | Godot-Tests, Screenshots |
| b1 | **Kern Diplomatie:** Beziehungen mit Drift, Gesandte, Verträge; `beziehungen.csv` | Einheitentests, Spielstände |
| b2 | **Bundesversammlung** und **diplomatische Spiele** im Kern | Einheitentests, Wiederholung mit Befehlen |
| b3 | **Brücke und Godot:** Fenster Diplomatie, Bundesversammlung, Spiel, Deutsche Frage, Kartenmodus „Beziehungen“ | Godot-Tests, Screenshots |
| b4 | **Doku und Review** (Abschluss M9) | CI grün |

## 11. Umsetzungsstand

| Schritt | Stand |
|---|---|
| a1 Daten | `common/zollunionen.chron` mit Zollverein (Leitung Preußen) und Steuerverein (Leitung Hannover), beide mit Tarif 3 % (chronik-skript.md 3.25). Staatsfeld `zollunion` statt `zollverein` (chronik-skript.md 3.5; die Brücke liefert `customs_union` und `customs_union_name` statt `zollverein`); 1848: 26 Staaten im Zollverein, Hannover, Oldenburg und Schaumburg-Lippe im Steuerverein, neun für sich. Lader mit Prüfungen (unbekannte oder spätere Union, Leitmacht ohne Ort oder nicht Mitglied, `keine` reserviert), Archivar (bekannte Union, Datum nicht vor ihrem `seit`), Inhaltsbereich `diplomatie`, Spielstand Format 3 (speicherstaende.md, Abschnitt 6), `uhrwerk-cli info` mit Unionen und Mitgliedern. Tests: Lader (Mitglieder vor und nach einem Wechsel, Fehler), Fingerabdruck, Format 1 und 2, CTest `zollunionen_1848` mit der Mitgliederliste; Godot-Rauchtest. Ohne Wirkung auf die Simulation; 1648 bitgleich (`5db13a66d63e6079`). |
| a2 Kern | `core/src/customs.cpp` (`uhrwerk/customs.hpp`): Zollgebiete je Markt am Monatsersten (Abschnitt 3.2), erwarteter Tarif im Handel und bei der Einfuhr vom Weltmarkt (3.3), fremder Einfuhranteil je Markt und Gebiet für Verbraucherpreis und Zoll der Orte, Vereinskasse am Monatsersten nach Bevölkerung, Anteil als Zoll im Haushalt (3.4); Befehl `Zollunion` mit Entscheidung der Leitmacht (`ausgeschlossen`, Daten: Österreich beim Zollverein), Rücknahme, Wechsel und Chronik, wirksam zum 1. Januar; Zollregler der Mitglieder gesperrt (3.5). Zustand nur mit Zollunionen (Mitgliedschaft, Vereinsanteil, Vereinskasse, Anträge, Zollgebiete der Märkte); die Mitgliedschaft ist kein Inhalt mehr. Testbefehle der Kommandozeile mit Zollunion. Tests: Gebiete und Anteile, Tarif, Handel über die Grenze (Fixture: 10 % Tarif, ein Drittel weniger Einfuhr), fremder Einfuhranteil, Vereinskasse und Haushalt, Beitritt, Austritt, Rücknahme, Ablehnung, Wechsel, gesperrter Regler, Spielstand, Wiederholung, 1 und 4 Threads. 1648 bitgleich (`5db13a66d63e6079`). Messung 1848 (12 Monate gegen die Rechnung aus M5): Zoll 5,7 % → 1,1 % der Staatseinnahmen, weil Einfuhr aus dem eigenen Gebiet nicht mehr verzollt wird; Zollverein-Mitglieder je 0,7–0,9 %, Steuerverein 3,8–4,1 %, Bremen 13,4 %, Österreich 0,5 %; Einfuhr aller Märkte +9 %. Bilanz 1848 (8 Seeds, 30 Jahre) und Wahllabor 1848/49 erfüllen alle Zielwerte (Liberale 296 statt 293 Sitze, Preußen 1849 72/16/225/36). Die Höhe des Zolls kalibriert Schritt a3. |
| a3 Zolllabor | `uhrwerk-cli zoll` (Abschnitt 3.6, Kern `uhrwerk/customs_lab.hpp`): je Seed ein Lauf ohne und je Wechsel einer mit ihm, Messung im letzten Jahr; Zollziele `common/zollziele_voelkerfruehling.chron` (chronik-skript.md 3.26), `pruefen` liest sie mit; CI-Schritt „Zolllabor 1848“ im Job Balancing, CTest `zoll_1848`. Regeländerung: Die Leitmacht kann austreten, ihre Union löst sich dann auf (Abschnitt 3.5), sonst wäre Hannovers Beitritt unmöglich. **Kalibrierung** (Entscheidung 3, Alternative): Mit 3 % brachte der Zoll dem Zollverein 1 % der Einnahmen; selbst 25 % brächten nur gut 4 %, weil die Einfuhr von außerhalb im Modell klein ist (der Zollverein ist ein großer Binnenmarkt; Kolonialwaren nur 4–7 % der fremden Einfuhr nach Wert). Gewählt: Zollverein 10 %, Steuerverein 3 % (niedriger verzollt). Ergebnis mit 8 Seeds, Messung 1853: Zollanteil Zollverein 2,9 % (2,6 … 3,0 %), Steuerverein 3,7 %; Beitritt Hannovers: Grenzhandel ×1,40 (×1,21 … ×1,80), Zoll ×0,67 (×0,61 … ×0,76), Preise ×1,02; Beitritt Mecklenburg-Schwerins: Grenzhandel ×2,08 (×1,79 … ×2,34), Handel ×1,61, Zoll ×0,49, Preise ×1,08. Hannover verliert Zoll wie historisch 1854 (dort mit Vorabanteil ausgeglichen). Bilanz 1848 (8 Seeds, 30 Jahre) und Wahllabor 1848/49 erfüllen alle Zielwerte (Liberale 296, Preußen 1849 71/16/227/36). Varianten: 10 %/5 % (Mecklenburg streut stark), 15 %/5 % (Zollanteil 3,7 %), 20 %/5 % (4,1 %). Der Zollanteil bleibt unter dem historischen (eher 10–20 %, Wissensstand ohne Quellen); dafür bräuchte das Modell mehr Einfuhr von Kolonial- und englischen Fabrikwaren. |
| a4 Godot | Kartenmodus „Zollgebiete“ (13, Taste Ö, Abschnitt 3.7): Farbe der Union aus `farbe` in `zollunionen.chron` (chronik-skript.md 3.25; nur Anzeige, nicht im Fingerabdruck), Staaten ohne Union blass in ihrer Farbe; Legende mit Unionen und Mitgliederzahl, Tooltip mit Zollgebiet und Tarif. Staatspanel: Zollregler der Mitglieder gesperrt mit dem Tarif der Union, Haushaltszeile „Zoll (Vereinskasse, Tarif …)“, offener Antrag im Hinweis; Knopf „Zollunion …“ mit Fenster (Beitreten, Austreten, Antrag zurückziehen, jeweils mit Rückfrage und Zeitpunkt; was die Leitmacht ablehnt, ist gesperrt). Ortsinfo mit Zollgebiet. Kern: `evaluate_customs_change` (Vorab-Prüfung ohne Wirkung, von `request_customs_change` benutzt) und `customs_effective_date`. Brücke: `get_customs_unions`, `get_customs_change`, `request_customs_union`, Felder in `get_state` und `get_location`, `MAP_ZOLLGEBIETE`; `set_tax_percent` lehnt den Zoll eines Mitglieds ab (bridge-api.md). Startoption `--zollunion`. Tests: Kern (Vorab-Prüfung), Rauchtest (Unionen, gesperrter Regler, Vorab-Prüfung, Antrag Hannovers mit Chronik, 1648 ohne Union), Klicktest (Taste Ö, Legende, Regler, Ortsinfo, Beitritt Hannovers über das Fenster mit Rückfrage, Antrag am nächsten Tag, Rücknahme), CI-Screenshot. 1648 bitgleich (`5db13a66d63e6079`). **M9a abgeschlossen.** |
| b1 Kern Diplomatie | `core/src/diplomacy.cpp` (`uhrwerk/diplomacy.hpp`): Beziehung je Staatenpaar (−200 bis +200, symmetrisch, 703 Paare 1848), monatlich 5 % der Lücke zum Zielwert (Abschnitt 4.1); zum Start auf dem Zielwert. Gesandte (4.2): Befehl `Gesandter` (Wert 1 entsenden, 0 abberufen), Kapazität 1 + Einwohner / 5 Mio. (Preußen 4, Österreich 3), +5 je Monat bis 60 über dem Zielwert, Kosten 0,5 % der Monatseinnahmen als Haushaltsposten. Verträge (4.3): Befehl `Vertrag` (Wert = Handlung: Bündnis, Garantie, Kündigung des Bündnisses, Kündigung der Garantie), Annahme nach Regel mit Chronik, Gegner unter −25. Daten: `common/diplomatie.chron` (chronik-skript.md 3.27), `tabellen/beziehungen.csv` mit 24 Grundwerten (Archivar, `history/beziehungen`, chronik-skript.md 3.28). Zustand (Beziehungen, Gesandte, Verträge, Kosten), Spielstandprüfung und Inhaltsbereich `diplomatie` nur mit Deutschem Bund; `uhrwerk-cli info` mit Gegnern, Gesandten und Lagern (1848: Preußen 21 Staaten, Österreich 5 – Bayern, Sachsen, Hannover, Liechtenstein, Dänemark –, unentschieden 10), Testbefehle mit Gesandten und Verträgen. Tests: Zielwerte und Drift, Gesandte mit Kapazität, Wirkung und Kosten, Bündnis nach Regel mit Lagerprüfung und Kündigung, Garantie, ungültige Befehle, Spielstand, Wiederholung, 1 und 4 Threads, Ladefehler, 1648 ohne Wirkung. 1648 bitgleich (`5db13a66d63e6079`); die Zahlen 1848 ändern sich ohne Befehle nicht. |
| b2 Bundesversammlung und Spiele | Kern `core/src/diet.cpp` und `core/src/plays.cpp` (`uhrwerk/diplomacy.hpp`). **Bundesversammlung** (Abschnitt 4.4): Daten `common/bundesversammlung.chron` (Präsidium Österreich, Plenum 68 Stimmen, Engerer Rat 17 Stimmen mit Kurien), Befehl `Bundesantrag` (Bundesreform, Zolleinigung, Intervention, Präsidium), ein Antrag zur Zeit, Abstimmung am nächsten Monatsersten nach Regel mit Chronik und Wirkung; Gegner des Antrags und Einstimmigkeit wie oben präzisiert. **Diplomatische Spiele** (4.5): Befehle `Forderung` (Führung im Bund, Zollunion, Bündnis lösen) und `Manoever` (nachgeben, Vermittlung, eskalieren), Parteinahme nach Bündnis, Garantie und Neigung, drei Stufen zu 30 Tagen, Entscheidung nach Regel ohne Spieler im Kern, Auflösung nach Stärke, Mobilmachungskosten, Wirkungen mit Kompromiss. Regeln in `common/diplomatie.chron` erweitert. Zustand (Präsidium, Anträge, Abstimmungen, Spiele, Ergebnisse), Spielstandprüfung und Inhaltsbereich `diplomatie` (Bundesversammlung, Regeln) nur mit Deutschem Bund; `uhrwerk-cli info` mit Bundesversammlung, Testbefehle mit Anträgen, Forderungen und Manövern. Tests: Abstimmung mit Kurie, Annahme und Termin, Einstimmigkeit, Zolleinigung mit Beitritt, Parteinahme, Stufen und Auflösung, Kompromiss nach der ersten Stufe, Manöver, Zollunion mit Beitritt und Zahlung, Bündnis lösen, Mobilmachungskosten, Spielstand, Wiederholung, Threads, Ladefehler. 1648 bitgleich (`5db13a66d63e6079`); ohne Befehle ändern sich die Zahlen 1848 nicht. |
| b3 Brücke und Godot | Brücke (bridge-api.md, Abschnitt Diplomatie): `get_relations`, `get_diplomacy`, `get_diplomacy_actions`, `send_envoy`, `propose_treaty`, `get_diet`, `get_motion_preview`, `file_motion`, `get_diplomatic_plays`, `make_demand`, `manoeuvre`, `get_german_question`, Konstanten `TREATY_*`, `MOTION_*`, `DEMAND_*`, `MANOEUVRE_*`, Kartenmodus 14 `MAP_BEZIEHUNGEN`, `get_state` mit `diplomacy` und `envoy_cost_month`. Godot (Abschnitt 6, präzisiert): Fenster „Diplomatie“ mit den Reitern Staaten, Bundesversammlung (Halbrund nach Lagern, Anträge mit Prognose, letzte Abstimmungen) und Deutsche Frage; Krisenleiste mit Manövern; Kartenmodus „Beziehungen“ (Taste #) mit Legende und Tooltip; Haushaltsposten „Gesandte“; Startoption `--diplomatie[=staaten|bund|frage]`. Die Leiste der Kartenmodi hat mit fünfzehn Modi Schrift 12. Tests: Einheitentest (Kartenmodus, Taste, Option, Farben), Rauchtest (Beziehungen, Handlungen, Bundesversammlung, Prognose, Gesandter, Antrag und Krise mit Nachgeben, deutsche Frage, Kartenmodus; 1648 leer), Klicktest (Taste #, Gesandter über das Fenster, Forderung mit Rückfrage, Krisenleiste mit Nachgeben, Antrag Bundesreform, Esc), CI-Screenshots. 1648 bitgleich (`5db13a66d63e6079`). |
