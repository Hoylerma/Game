# Militär und Einigungskriege (Meilenstein M10)

> **Status: verbindlich** – Entscheidungen 1–14 wie empfohlen freigegeben (Abschnitt 10); Umsetzung
> läuft, Stand in Abschnitt 12. Grundlage sind
> [15 Militär und Kriegsführung](../15_militaer_und_kriegsfuehrung.md) (Abschnitte 1, 3–5, 7–9, 12
> und 13), [14 Diplomatie](../14_diplomatie.md) (Abschnitte 5 und 7), [04 Zeitrahmen](../04_zeitrahmen_epochen_startdaten.md)
> (Startdaten) und der Meilensteinplan
> ([meilensteine.md](../meilensteine.md#m10--militär-und-einigungskriege)). Die Diplomatie steht in
> [diplomatie.md](diplomatie.md), Wirtschaft und Haushalt in [wirtschaft.md](wirtschaft.md), das
> Lesezeichen 1848 in [lesezeichen_1848.md](lesezeichen_1848.md).

Zwischen 1864 und 1871 entscheiden drei kurze Kriege die deutsche Frage: Preußen und Österreich
gegen Dänemark um Schleswig und Holstein (1864), Preußen gegen Österreich und die Mehrheit des Bundes
(1866) und der Norddeutsche Bund mit den süddeutschen Staaten gegen Frankreich (1870/71). Alle drei
sind Bewegungskriege mit Eisenbahnaufmarsch, wenigen großen Schlachten und Belagerungen; keiner
dauert länger als ein gutes halbes Jahr.

M10 bringt dafür in die Simulation:
- **Heere** als Einheiten auf der Karte, Mobilmachung, Bewegung über Straßen und Eisenbahnen,
  Versorgung, Schlachten, Besetzung und Belagerung;
- **Krieg und Frieden**: Kriegserklärung, Bündnisfall, Kriegsziele, Kriegspunkte, Friedensschluss
  mit Gebietsabtretung, Zahlung und Untergang von Staaten;
- die **Ausgangslagen** der drei Einigungskriege als eigene Lesezeichen.

Ereignisse (M11), die KI der Staaten (M12) und der durchgehend spielbare Zeitraum 1848–1871 (M13)
bauen darauf auf.

## 1. Umfang

Der Meilensteinplan verlangt:

- Heere, Aushebung, Versorgung, Bewegung auf der Karte, Schlachten, Fronten
- Eisenbahnen als Aufmarschwege
- Kriegsziele, Friedensverhandlungen, Gebietsabtretungen
- Ausgangslagen der Kriege 1864, 1866 und 1870/71
- Abnahme: Die drei Kriege lassen sich durchspielen; ein ungestörter Lauf ohne Spieler führt zu
  plausiblen, nicht festgelegten Ergebnissen. Heereskosten belasten den Haushalt aus M5.

Vorschlag (Entscheidung 1): **zwei Teile** wie bei M7 bis M9.

| Teil | Inhalt | Ergebnis |
|---|---|---|
| **M10a** | Nachbarschaft der Orte, Heere, Mobilmachung, Bewegung mit Eisenbahn, Versorgung, Schlacht, Besetzung und Belagerung, Krieg und Frieden mit Kriegspunkten, Gebietsabtretung, Generalstab nach Regel, Kriegslabor auf der Test-Fixture | Krieg im Kern, prüfbar ohne Oberfläche |
| **M10b** | Daten 1849–1870 (Staaten, Eisenbahnen, Zollverein, Verträge, Festungen, Frankreich), drei Kriegslesezeichen mit Aufmarsch, Kalibrierung mit dem Kriegslabor, Heere und Krieg in Godot; Doku und Review | Abnahme: die drei Kriege spielbar, Läufe ohne Spieler plausibel |

**Nicht in M10:**
- **Fronten und Heeresgruppen** (Kapitel 15, Abschnitt 6). 1864–1871 sind Bewegungskriege; die
  Formel für geschlossene Fronten wird erst mit Massenheeren und Schnellfeuerwaffen wichtig
  (Weltkriege). M10 legt die Kampfkraft so an, dass Fronten später darauf aufbauen.
- Seekrieg, Blockade und Luftkrieg. Meerengen (Alsen) sind Übergänge mit Wartezeit (Abschnitt 3.4).
- Aufmarschpläne als Generalstabsarbeit über Jahre (Kapitel 15, Abschnitt 8.2): Die Lesezeichen
  bringen den historischen Aufmarsch mit.
- Feldherren als Persönlichkeiten mit Laufbahn und Eigenschaften (Kapitel 13): In M10 hat jede Armee
  einen Befehlshaber mit Namen und einem Führungswert.
- Kriegswirtschaft (Rationierung, Ersatzstoffe), Heimatfront und Revolution aus Kriegsmüdigkeit
  (M11). Die Kriegsmüdigkeit wirkt in M10 nur auf den Friedensschluss.
- Außenmächte außer Frankreich (Entscheidung 12): Italien bindet 1866 die österreichische
  Südarmee, Russland und Großbritannien bleiben neutral. Das bilden die Lesezeichen ab, ohne diese
  Staaten.
- Friedenskonferenzen mit mehreren Mächten (Kapitel 14, Abschnitt 7.3); Frieden schließen in M10 die
  Kriegsparteien untereinander.
- Entscheidungen der Staaten ohne Spieler über einfache Regeln hinaus (M12): Wer ohne Spieler Krieg
  führt, tut es nach den Regeln des Generalstabs (Abschnitt 5). Kriege beginnen ohne Spieler nur aus
  den Lesezeichen, aus Krisen und aus Bundesbeschlüssen, nicht aus eigenem Antrieb.

## 2. Ausgangslage

**Vorhanden:**
- Das **Heer** ist eine Zahl je Staat: Soldaten je 1.000 Einwohner (`heer`, 1848 Preußen 9,
  Österreich 10, Mittelstaaten 7–8, Städte 4) mal Bevölkerung, nur als Kosten (Sold 30 Taler je
  Soldat und Jahr). 1848 sind das in Preußen 144.000 Soldaten und 360.000 Taler je Monat, die Hälfte
  der Ausgaben; in Österreich (nur die Orte im Kartenausschnitt) 116.000 Soldaten und 290.000 Taler.
- **Kriegsverwüstung** je Ort (seit M1) mit Abbau über die Jahre; Seuchen und Verwüstung als
  Ereignisse der Testregion 1648.
- **Diplomatie** (M9): Beziehungen, Bündnisse, Garantien, Bundesversammlung mit Bundesintervention
  (Vollzug offen), diplomatische Spiele mit Mobilmachung (Heereskosten × 1,5) und Auflösung nach
  Stärke. Die Stärke einer Seite ist bisher `heer × Bevölkerung`.
- **Eisenbahnen** (M7): 176 Abschnitte bis zum 1. November 1848, rund 5.000 km Luftlinie zwischen
  Orten, mit Bahnfracht für den Handel. **Danach keine.**
- **Orte:** 4.542 im Kartenausschnitt, der nächste Nachbar im Median 10 km entfernt, die sechs
  nächsten im Mittel 14,5 km – ungefähr ein Tagesmarsch. Der Kern kennt keine Nachbarschaft; die
  Karte (`provinzen.png`) und der Kartengenerator kennen sie.
- **Kriegsschauplätze auf der Karte:** Schleswig, Holstein und Südjütland (Kolding, Ribe), Böhmen
  und Mähren (Königgrätz, Olmütz), Elsass (59 Orte), Lothringen bis Verdun und Toul (100) und die
  Ardennen mit Sedan (9), Luxemburg. Nicht auf der Karte: Paris, Kopenhagen, Oberitalien. Elsass,
  Lothringen und die Ardennen sind Orte ohne Daten; Frankreich ist kein Staat des Modells.
- **Verwaltung:** Schleswig, Holstein und Lauenburg sind Einheiten des dänischen Gesamtstaats
  (M7); Provinzen und Kronländer Preußens und Österreichs ebenso.
- **Besitz** wechselt nur beim Laden (datierte Historie); zur Laufzeit gehört jeder Ort für immer
  seinem Staat.
- Bevölkerung nur 1846; Staaten, Zollverein, Bundesversammlung und Verträge nur bis 1848.

**Es fehlt:** Heere als Einheiten, Nachbarschaft und Wege, Krieg als Zustand, Besetzung, Frieden
und Besitzwechsel zur Laufzeit; Daten nach 1848; Frankreich.

**Quellen:** wie in M7 bis M9 ohne Seitenzugriff. Verlauf, Schlachten und Ergebnis der drei Kriege
sind gut bekannt; Heeresstärken, Verluste und Ausrüstung in der Größenordnung aus der Literatur.
Kampfwerte, Marschleistungen und Regeln sind Spielwerte, `spielwert`; Stärken und Daten der
Lesezeichen `ungeprueft` (Entscheidung 14).

## 3. Heere und Bewegung (M10a)

### 3.1 Nachbarschaft der Orte

Der Kartengenerator schreibt neben `definition.csv` eine Tabelle **`karte/nachbarn.csv`**
([chronik-skript.md](chronik-skript.md), Abschnitt 6.6): je Paar von Orten mit gemeinsamer Grenze im
Ort-ID-Bild eine Zeile mit der Grenzlänge in Pixelkanten und dem Rang des größten Flusses, den die
gerade Verbindung der Lagen beider Orte kreuzt (0 = keiner). Ein **Flussübergang** ist ein gekreuzter
Fluss bis Rang `fluss_rang` (3: Rang 1 Rhein, Elbe, Donau, Oder, Weichsel; Rang 2 Weser, Mosel,
Memel, Po; Rang 3 Main, Inn, Moldau, Maas, Ems, Warthe, Etsch; Spree, Saale und Neckar haben Rang 4). Meerengen und Inseln, die das Bild nicht verbindet (Rügen, Alsen,
Fehmarn, die Nordfriesischen und Ostfriesischen Inseln, Ærø), stehen als Ergänzung in
`karte/ueberfahrten.csv` (Abschnitt 6.7) mit der Dauer in Tagen. Der Kern lädt beides in Welten mit
Militär; ein Ort mit Daten ohne Nachbarn ist eine Warnung. Die Nachbarschaft gehört zum
Inhaltsfingerabdruck (`militaer`).

Daten 1848: 13.069 Kanten aus dem Bild, davon 1.261 mit Flussübergang bis Rang 3, und 13
Überfahrten; alle Orte mit Daten hängen an einem Netz (nur Helgoland und Nordby, beide ohne Daten,
bleiben allein).

### 3.2 Heere

Ein **Heer** (Armee) ist eine Einheit eines Staates in einem Ort:

| Feld | Bedeutung |
|---|---|
| Name, Befehlshaber | z. B. „I. Armee“, „Prinz Friedrich Karl“; Führung 0–3 (Spielwert) |
| Soldaten | Infanterie und Kavallerie zusammen |
| Geschütze | Feldartillerie |
| Moral | 0–1; sinkt mit Verlusten, Rückzug und Hunger, erholt sich in Ruhe |
| Ort, Ziel, Weg | Standort; Marschziel und berechneter Weg; Tage bis zum nächsten Ort |
| Lage | Ruhe, Marsch, Bahnfahrt, Schlacht, Belagerung, Rückzug |

**Je Staat** (Daten, datiert): Bewaffnung (Feuerkraft der Infanterie, z. B. Zündnadelgewehr
Preußens seit 1841: ×1,5 gegen Vorderlader; Chassepot Frankreichs ab 1866), Güte der Artillerie
(gezogene Hinterlader), Doktrin (Auftragstaktik: Bonus bei Bewegung), **Mobilmachungsfaktor**
(Kriegsstärke ÷ Friedensstärke, z. B. Preußen 2,5 mit Landwehr) und **Mobilmachungstage**.

**Friedensheer** (Entscheidung 4): Die Friedensstärke ergibt sich wie bisher aus `heer`
× Bevölkerung. Sie steht als Garnison in den Korpsstandorten aus den Daten (sonst in der
Hauptstadt) und kostet wie bisher. Feldarmeen entstehen erst mit der Mobilmachung.

Umsetzung (Schritt a1): Regeln, Heerwesen und Garnisonen stehen in `common/militaer.chron`
([chronik-skript.md](chronik-skript.md), Abschnitt 3.30). Das Heerwesen hat in a1 Bewaffnung,
Geschütze je 1.000 Soldaten, Mobilmachungsfaktor und -tage; Artillerie und Doktrin folgen, wenn die
Schlacht sie braucht (a3). Jede Garnison hat einen Anteil an der Friedensstärke ihres Staates; beim
Start entsteht je Garnison ein Heer (Lage „Garnison“, Moral 1), dessen Soldaten und Geschütze am
Monatsersten der Friedensstärke folgen. Name, Befehlshaber, Ziel und Weg kommen mit der Bewegung
(a2).

### 3.3 Mobilmachung

Befehl **`Mobilmachung(staat)`** (und Demobilisierung): Über die Mobilmachungstage wächst die
Stärke auf Friedensstärke × Mobilmachungsfaktor; die Garnisonen werden zu Feldarmeen an ihren
Standorten. Die Heereskosten steigen mit den Soldaten (Haushaltsposten „Heer“, wie die Mobilmachung
der Krisen aus M9, die darin aufgeht). Nach dem Frieden demobilisiert ein Staat über 30 Tage.

Ob die Reservisten der Wirtschaft als Arbeitskräfte fehlen, ist Entscheidung 7 (Empfehlung: in M10
nicht; nur die Kosten und die Gefallenen wirken).

Umsetzung (Schritt a2): Befehl `Mobilmachung` (Wert 1 mobil, 0 Abrüstung). Phasen je Staat: Frieden,
Mobilmachung, mobil, Abrüstung. Mit der Mobilmachung werden alle Garnisonen eines Staates Feldarmeen
(Lage „Ruhe“) und wachsen jeden Tag um ihren Anteil an der Friedensstärke × (Faktor − 1) ÷
Mobilmachungstage, höchstens bis zur Kriegsstärke; Reserven stoßen dort zum Heer, wo es steht. Die
Abrüstung führt die Stärke in `demobilmachung_tage` (30) gleichmäßig auf den Anteil zurück; danach
stehen die Heere wieder als Garnisonen an ihren Standorten. Die Heereskosten sind die bisherigen mal
Soldaten ÷ Friedensstärke, solange ein Staat nicht im Frieden ist; im Frieden bleiben sie unverändert
(1848 ohne Befehle zeichengleich). Eine Krise auf der Stufe Mobilmachung (M9) macht in Welten mit
Militär beide Hauptparteien wirklich mobil, statt die Kosten mit dem Faktor der Diplomatie (1,5) zu
erhöhen; endet die Krise, rüsten die Staaten ab, die sie mobil gemacht hat. Befiehlt die Regierung
selbst die Mobilmachung, bleibt sie nach der Krise bestehen.

### 3.4 Bewegung

- **Marsch** zu einem Nachbarort: Tage = Abstand der Mittelpunkte ÷ Marschleistung (Spielwert
  20 km je Tag) × Geländefaktor des Zielorts (Gebirge 2, Wald und Sumpf 1,5, sonst 1); Flussübergang
  +1 Tag; Überfahrt nach den Daten (Alsen: 2 Tage). Ein Heer marschiert auf dem kürzesten Weg
  (Dijkstra über die Marschtage).
- **Eisenbahn:** Steht ein Heer an einem Bahnhof (Endpunkt eines eröffneten Abschnitts), fährt es
  über eröffnete Abschnitte im eigenen, verbündeten oder besetzten Gebiet: 1 Tag Ein- und 1 Tag
  Ausladen, 300 km je Tag; je Strecke höchstens 20.000 Soldaten je Tag (größere Heere brauchen
  mehrere Tage). So wird der Aufmarsch 1866 und 1870 schnell, wer kein Netz hat, marschiert.
- **Zugang:** Eigene, verbündete und gegnerische Orte (im Krieg); neutrale Staaten und Orte ohne
  Daten sind gesperrt.

Umsetzung (Schritt a2): Befehl `Marsch` (Heer, Zielort; ohne Ziel hält das Heer an). Nur
Feldarmeen marschieren, nicht die Garnisonen im Frieden. Der Weg ist der schnellste über Marsch und
Bahn zugleich (Dijkstra über Ort × zu Fuß/auf der Bahn; Einladen an einem Bahnhof, Ausladen am Ende
der Fahrt, beides eingerechnet). Das Gelände steht in `common/militaer.chron` (`gelaende`; 1848:
Hochgebirge 2, Mittelgebirge, Wald und Bruch 1,5). Eine Bahnfahrt ist ein Abschnitt des Weges: Beim
Einsteigen wartet das Heer, bis alle Strecken seiner Fahrt frei sind, lädt `Soldaten ÷ 20.000` Tage
(aufgerundet) und belegt die Strecken so lange; dann fährt es und lädt einen Tag aus. Wird der nächste
Ort unterwegs gesperrt (ein Bündnis endet), bleibt das Heer stehen. Wer schon auf einem Abschnitt
marschiert und ein neues Ziel über denselben nächsten Ort bekommt, behält den zurückgelegten Teil;
auf der Bahn nimmt ein Heer keine Befehle an. Zugang in a2: eigene und verbündete Orte (Krieg und
Besetzung folgen mit a3).

### 3.5 Versorgung und Zermürbung

Versorgt ist ein Heer in eigenem oder verbündetem Gebiet und im Feindesland, solange es höchstens
drei Tagesmärsche von einem Bahnhof hat, den seine Seite hält (Kapitel 15, Abschnitt 4: Eisenbahn ab
1850). Zermürbung je Tag (Krankheit, Abgänge): versorgt 0,05 %, unversorgt 0,5 % und Moral −0,02;
im Winter (Dezember bis Februar) doppelt. Belagerer zählen wie unversorgt, wenn die Festung den
Bahnhof sperrt.

Umsetzung (Schritt a2): Zermürbung gilt nur für Feldarmeen, nicht für Garnisonen im Frieden. Die drei
Tagesmärsche zum Bahnhof sind Marschtage wie bei der Bewegung; Orte ohne Daten liegen nicht auf dem
Weg. In Ruhe und versorgt erholt sich die Moral um 0,01 je Tag. Die Geschütze folgen den Soldaten
(Heerwesen). Die Abgänge mindern die Bevölkerung nicht; Gefallene kommen mit der Schlacht (a3). Ohne
Krieg stehen alle Heere in eigenem oder verbündetem Gebiet und sind versorgt.

## 4. Krieg (M10a)

### 4.1 Krieg und Kriegsparteien

Ein **Krieg** hat zwei Seiten mit je einer Hauptpartei, Mitkriegführenden, Kriegszielen, Beginn
und Kriegspunkten. Er beginnt durch:

- Befehl **`Kriegserklaerung(staat, ziel, kriegsziel)`**;
- eine **Krise**, die nach der dritten Stufe nicht endet (Entscheidung 9; M9 versprach: „M10 ersetzt
  die letzte Stufe durch Krieg“);
- eine angenommene **Bundesintervention**: die Bundesstaaten, die dafür stimmten, gegen das Ziel
  (Bundesexekution, Vollzug aus M9);
- ein **Lesezeichen**, das im Krieg beginnt.

**Bündnisfall:** Verbündete und Garanten des Angegriffenen treten auf seiner Seite bei; Verbündete
des Angreifers, wenn sie nicht mit dem Ziel verbündet sind. Wer mit beiden verbündet ist, bleibt
neutral. Kriegserklärungen kosten Beziehung bei allen Bundesstaaten (Spielwert −20) und lösen
Bündnisse mit dem Gegner.

### 4.2 Schlacht

Stehen gegnerische Heere im selben Ort, beginnt eine **Schlacht**; sie läuft täglich, bis eine Seite
bricht (Kapitel 15, Abschnitt 5).

```
Kampfkraft K = eingesetzte Soldaten × Feuerkraft × Führung × Moral
               × (Verteidiger: Gelände × Flussübergang × Festung)
eingesetzt   = min(Soldaten, Frontbreite des Ortes)            (Frontbreite nach Gelände)
Verluste je Tag einer Seite = Verlustrate × K(Gegner) × Zufall(0,8 … 1,2)
Moral       −= Verluste ÷ Soldaten × Moralfaktor
Bruch: Moral < 0,3 oder Verluste > 25 % des Tagesanfangs → Rückzug in einen Nachbarort
       Richtung eigener Bahnhof, Verfolgung kostet Gefangene
```

Die Feuerkraft kommt aus Bewaffnung und Geschützen; der Zufall aus einem **eigenen Zufallsstrom
`krieg`** (Entscheidung 6). Verluste teilen sich in Gefallene (ein Viertel), Verwundete (die Hälfte
kehrt nach 60 Tagen zurück) und Gefangene (beim Rückzug, kehren mit dem Frieden zurück). Die
Chronik meldet jede Schlacht: „Schlacht bei Königgrätz: Die I. und II. Armee schlagen die
Nordarmee; Verluste 9.000 gegen 44.000.“ Der Ort erhält Verwüstung nach den Verlusten.

### 4.3 Besetzung und Belagerung

- Ein gegnerischer Ort ohne gegnerisches Heer wird nach einem Tag **besetzt**: Er bleibt im Besitz
  seines Staates, wird aber von der anderen Seite kontrolliert. Seine Steuern fehlen dem Besitzer;
  die Hälfte geht als Kontribution an den Besetzer.
- **Festungen** (Daten: Stufe 1–3, z. B. Düppel 2, Mainz 3, Metz 3, Straßburg 3) müssen belagert
  werden: Tage = 30 × Stufe × Besatzung ÷ (Belagerer mit Geschützen), mindestens 10; ein Entsatzheer
  im Ort bricht die Belagerung. Die Besatzung ist Teil eines Heeres.
- Befreit wird ein Ort durch ein eigenes Heer ohne Gegner.

### 4.4 Verluste und Bevölkerung

Gefallene und Zermürbung mindern die Bevölkerung der Heimatorte (Jahrgänge im wehrfähigen Alter,
anteilig nach Einwohnern); so wirken die Verluste auf die Demografie (Kapitel 15, Abschnitt 3).

### 4.5 Kriegsziele, Kriegspunkte und Frieden

**Kriegsziele** (bei der Erklärung, je Seite):

| Ziel | Wirkung beim Frieden | Kosten (Kriegspunkte) |
|---|---|---|
| Gebiet: Orte oder Verwaltungseinheiten (z. B. Schleswig, Holstein; Elsass und Lothringen) | Besitzwechsel | nach Bevölkerung: 100 × Anteil an der Bevölkerung des Gegners, mindestens 10 |
| Annexion eines Staates | alle seine Orte; der Staat geht unter | 80 |
| Führung im Bund | der Gegner verlässt den Bund; ohne Österreich löst er sich auf (1866) | 50 |
| Beitritt zur Zollunion | wie die Krise aus M9 | 30 |
| Kriegsentschädigung | Zahlung: Jahreseinnahmen des Gegners × Anteil | 10 je Monatseinnahme |

**Kriegspunkte** einer Seite (0–100, Kapitel 14, Abschnitt 7.2):
- Besetzung: 60 × besetzte Bevölkerung ÷ Bevölkerung des Gegners; Hauptstadt besetzt +20;
- Schlachten: 30 × (zugefügte − erlittene Verluste) ÷ Kriegsstärke des Gegners, beschränkt;
- Heere: +10 je vernichtetes oder gefangenes Feldheer.

**Kriegsmüdigkeit** je Staat wächst mit Verlusten (relativ zur Bevölkerung), Besetzung und Dauer.

**Frieden:** Befehl **`Friedensangebot(staat, krieg, bedingungen)`**. Ein Staat ohne Spieler nimmt
an, wenn die Kriegspunkte des Gegners die Kosten der Bedingungen decken oder seine Kriegsmüdigkeit
sie übersteigt; ohne Spieler stellt die führende Seite ihre Ziele, sobald ihre Kriegspunkte reichen,
und nach 12 Monaten ohne Bewegung endet der Krieg ohne Gewinner. Mitkriegführende schließen mit der
Hauptpartei Frieden.

**Besitzwechsel zur Laufzeit** (Entscheidung 11): Abgetretene Orte wechseln den Besitzer mit
Bevölkerung, Betrieben und Verwaltungseinheit; Zollgebiete, Bundesgebiet und Beziehungen rechnen
sich am nächsten Monatsersten neu. Ein Staat ohne Orte **geht unter**: Heere werden aufgelöst,
Verträge enden, Kasse und Schulden gehen an den Erwerber, seine Stimmen in der Bundesversammlung
entfallen. Wahlkreise bleiben bis zur nächsten Wahl beim alten Zuschnitt.

## 5. Generalstab nach Regel (M10a)

Ohne Spieler führt ein **Generalstab** die Heere einer Seite nach Regeln; der Spieler kann ihm
einzelne Heere übergeben (Entscheidung 3). Täglich je Seite:

1. **Mobilmachung**, sobald Krieg oder Krise Stufe 3; Aufmarsch per Bahn an die Grenze zum
   Kriegsziel.
2. **Schwerpunkt:** Heere, die näher als drei Tagesmärsche beieinander stehen, operieren gemeinsam
   („getrennt marschieren, vereint schlagen“).
3. **Angriff**, wenn die eigene Kampfkraft im Umkreis die des nächsten gegnerischen Heeres um das
   1,2-Fache übertrifft; sonst **Verteidigung** an einer Festung, einem Fluss oder der Hauptstadt,
   **Rückzug** vor einer doppelten Übermacht.
4. **Besetzung:** freie Heere rücken auf das Kriegsziel und die Hauptstadt des Gegners vor und
   belagern Festungen auf dem Weg.
5. **Frieden** nach Abschnitt 4.5.

Die Regeln sind deterministisch; Unterschiede zwischen Läufen entstehen aus dem Zufall der
Schlachten. Das genügt der Abnahme („plausibel, nicht festgelegt“); bessere Entscheidungen bringt
die KI in M12.

## 6. Die Einigungskriege (M10b)

### 6.1 Lesezeichen

Drei **Kriegslesezeichen** (Entscheidung 13), die dieselben Daten wie 1848 nutzen und die datierte
Historie bis zu ihrem Datum anwenden:

| Lesezeichen | Datum | Lage | Spieler |
|---|---|---|---|
| **Düppel** | 1. Februar 1864 | Preußen und Österreich (verbündet) gegen Dänemark; Bundestruppen in Holstein | Preußen, Österreich, Dänemark |
| **Königgrätz** | 16. Juni 1866 | nach dem Bundesbeschluss vom 14. Juni: Preußen gegen Österreich, Sachsen, Bayern, Hannover, Württemberg, Baden, beide Hessen, Nassau; drei preußische Armeen an der böhmischen Grenze | Preußen, Österreich, Sachsen, Bayern, Hannover |
| **Sedan** | 19. Juli 1870 | Frankreich gegen den Norddeutschen Bund und Bayern, Württemberg, Baden, Hessen; Aufmarsch am Rhein | Preußen (Norddeutscher Bund), Frankreich, Bayern |

Jedes Lesezeichen bringt die Heere im historischen Aufmarsch mit (Ort, Soldaten, Geschütze,
Befehlshaber) und beginnt im Krieg mit den historischen Kriegszielen (1864: Schleswig und Holstein;
1866: Führung im Bund und Annexionen; 1870: Elsass und Lothringen und eine Entschädigung).

### 6.2 Daten 1849–1870

- **Staaten und Besitz:** Hohenzollern an Preußen (1850), Anhalt vereinigt (1863), Schleswig,
  Holstein und Lauenburg (1864/65 gemeinsam verwaltet, Gastein 1865), die Annexionen von 1866
  (Hannover, Kurhessen, Nassau, Frankfurt, Hessen-Homburg, Schleswig-Holstein), der Norddeutsche
  Bund (1867), Luxemburg neutral (1867).
- **Frankreich** (Entscheidung 12) als Staat mit den Orten des Kartenausschnitts (Elsass,
  Lothringen, Ardennen; 168 Orte mit Bevölkerung nach der Zählung von 1866) und einem
  **Hinterland** als Zahlen (Bevölkerung, Einnahmen, Ersatz für das Heer), das Nachschub an den Rand
  der Karte schickt. Paris liegt außerhalb; das bildet die Kriegspunkte ab (Abschnitt 4.5).
- **Eisenbahnen** 1849–1870: die Hauptstrecken (Schätzung: rund 200 Abschnitte), darunter die
  Aufmarschlinien nach Böhmen, an den Rhein und nach Schleswig.
- **Zollverein** 1854 mit Hannover und Oldenburg (Steuerverein aufgelöst), Mecklenburg 1868;
  **Bundesversammlung** bis zum 24. August 1866; **Verträge** datiert (1864 Preußen–Österreich,
  1866 die Bündnisse, 1870 die Militärkonventionen und Schutz- und Trutzbündnisse).
- **Festungen** (rund 30 Orte) und Korpsstandorte; **Militär je Staat** (Bewaffnung,
  Mobilmachung) datiert.
- **Bevölkerung:** 1846 (Entscheidung 14).

### 6.3 Kriegslabor

`uhrwerk-cli krieg --szenario <lesezeichen> --seeds N` rechnet ein Kriegslesezeichen ohne Spieler
bis zum Frieden (höchstens zwei Jahre) und berichtet je Lauf Sieger, Dauer, Friedensbedingungen,
Schlachten und Verluste, über alle Läufe die Verteilung, gegen **Kriegsziele** in
`common/kriegsziele_<lesezeichen>.chron` (Korridore wie bei Wahl- und Zolllabor), zum Beispiel:

| Lesezeichen | Ziel (Spielwert, nach Literatur) |
|---|---|
| Düppel | Dänemark verliert Schleswig in mindestens 80 % der Läufe; Dauer 2–9 Monate |
| Königgrätz | Preußen setzt sich in 60–90 % der Läufe durch; Entscheidungsschlacht in Böhmen; Dauer 3–16 Wochen |
| Sedan | die deutsche Seite gewinnt in mindestens 75 % der Läufe; Elsass und Lothringen wechseln; Dauer 2–10 Monate |

„Plausibel, nicht festgelegt“ heißt: Die Ziele sind Korridore, und kein Ausgang tritt in allen
Läufen ein. Das Kriegslabor läuft in der CI wie das Zolllabor.

## 7. Zustand, Kern und Spielstand

- **Neuer Zustand:** Heere (mit Weg und Lage), Garnisonen und Mobilmachung je Staat, Kriege mit
  Seiten, Zielen, Kriegspunkten und Kriegsmüdigkeit, Kontrolle je Ort, Belagerungen, Schlachten im
  Gang, Gefangene, untergegangene Staaten. Besitz und Bundesgebiet je Ort werden in Welten mit
  Militärdaten Zustand (bisher Inhalt), sonst bleibt der Inhaltsfingerabdruck wie er ist.
- **Neue Befehle:** `Marsch`, `Mobilmachung`, `Kriegserklaerung`, `Friedensangebot`, `Generalstab`
  (Heer an den Generalstab übergeben oder zurücknehmen). Spielstand **Format 4**.
- **Neuer Zufallsstrom** `krieg` (Schlachten). Ohne Krieg wird er nicht gezogen.
- **Inhaltsfingerabdruck:** neuer Bereich `militaer` (Nachbarschaft, Festungen, Militär je Staat,
  Regeln, Aufmarsch).
- **1648 bitgleich** (Entscheidung 14): Heere und Krieg nur in Szenarien mit Militärdaten. 1848
  bekommt Friedensheere und ändert seine Prüfsumme; ohne Befehle bleiben die Zahlen 1848 gleich
  (die Garnisonen kosten wie bisher).
- **Leistung:** Wegesuche über rund 13.000 Kanten nur bei neuen Marschzielen; Schlachten und
  Versorgung je Heer und Tag. Vernachlässigbar neben der Bevölkerung.

## 8. Brücke und Godot (M10b)

- **Brücke:** Heere (je Staat, je Ort), Weg eines Heeres, Kriege mit Seiten, Zielen und Punkten,
  Kontrolle je Ort, Befehle aus Abschnitt 7, Vorab-Prüfung eines Friedensangebots; Kartenmodus 15
  **`MAP_KRIEG`** (Kontrolle: besetzte Orte in der Farbe des Besetzers schraffiert, Fronten als
  Grenze der Kontrolle).
- **Godot:**
  - **Heere auf der Karte** als Marken mit Flagge und Stärke, Auswahl per Klick, Marschbefehl per
    Rechtsklick mit Weg und Ankunft; Schlachten mit Symbol.
  - **Heeresfenster:** Stärke, Moral, Versorgung, Befehlshaber; „Dem Generalstab übergeben“.
  - **Kriegsfenster:** Kriege, Seiten, Ziele, Kriegspunkte, Kriegsmüdigkeit; Friedensangebot mit
    Vorab-Prüfung; Kriegserklärung aus dem Fenster „Diplomatie“.
  - **Schlachtbericht** als Meldung; Mobilmachung im Staatspanel.
  - Die Kriegslesezeichen im Menü „Neues Spiel“.

## 9. Tests und Abnahme

| Kriterium | Prüfung |
|---|---|
| Die drei Kriege lassen sich durchspielen | Klicktest je Lesezeichen (Heer bewegen, Schlacht, Frieden schließen); Rauchtest der Brücke |
| Ein Lauf ohne Spieler führt zu plausiblen, nicht festgelegten Ergebnissen | Kriegslabor in der CI: alle Korridore der Kriegsziele über 16 Seeds erfüllt, kein Ausgang in allen Läufen |
| Heereskosten belasten den Haushalt | Einheitentest: Mobilmachung erhöht den Posten „Heer“ um den Faktor; Kriegslabor berichtet Schulden vor und nach dem Krieg |

Weitere Tests:
- **Einheitentests:** Nachbarschaft und Wege (Gelände, Fluss, Überfahrt, Eisenbahn mit Kapazität),
  Versorgung, Schlacht (Verluste, Bruch, Rückzug, Frontbreite), Belagerung, Besetzung, Bündnisfall,
  Kriegspunkte, Frieden mit Gebietsabtretung und Untergang eines Staates, Krise → Krieg,
  Bundesexekution, Generalstab.
- **Determinismus und Spielstände:** gleiche Prüfsumme mit 1 und 4 Threads; Speichern während
  einer Schlacht, einer Belagerung und einer Bahnfahrt; Formatwache.
- **1648 bitgleich:** `5db13a66d63e6079` nach 100 Jahren.
- **Godot:** Rauchtest der neuen Brückenmethoden, Klicktest, Screenshots der Lesezeichen mit
  Heeren und des Kartenmodus „Krieg“.

## 10. Entscheidungen (wie empfohlen freigegeben)

1. **Teilung M10a/M10b.** Empfehlung: **ja** (Abschnitt 1). Alternative: Lesezeichen und Daten
   zuerst, Krieg danach (dann lässt sich der Krieg erst spät prüfen).
2. **Netz der Bewegung.** Empfehlung: **Orte** mit Nachbarschaft aus der Karte (Abschnitt 3.1);
   ein Nachbarort ist etwa ein Tagesmarsch. Alternativen:
   - **Kreise** (626): gröber, weniger Wege, aber Schlachten und Besetzung nur je Kreis;
   - **Märkte** (77): zu grob für Feldzüge.
3. **Steuerung.** Empfehlung: **direkte Führung** (Heere auf der Karte bewegen) und ein
   **Generalstab nach Regel**, dem der Spieler Heere übergeben kann (Abschnitt 5). Alternativen:
   - nur strategisch (Kriegsziele setzen, alles andere führt der Generalstab, wie *Victoria 3*);
   - Fronten und Heeresgruppen schon jetzt (*Hearts of Iron*): für 1864–1871 unpassend.
4. **Friedensheer und Feldarmeen.** Empfehlung: **Garnisonen an Korpsstandorten**, Feldarmeen erst
   mit der Mobilmachung (Abschnitt 3.2). Alternative: auch im Frieden bewegliche Heere (mehr
   Zustand, ohne Nutzen vor M12).
5. **Truppengattungen.** Empfehlung: **Soldaten und Geschütze** je Heer, Bewaffnung und Doktrin
   je Staat. Alternative: Infanterie, Kavallerie und Artillerie getrennt mit eigenen Werten
   (genauer, für 1864–1871 ohne großen Unterschied).
6. **Schlacht.** Empfehlung: **tägliche Runden** mit Moral, Frontbreite und Zufall aus einem
   eigenen Strom (Abschnitt 4.2). Alternativen:
   - Ergebnis in einem Schritt (einfacher, ohne Entsatz und Verstärkung während der Schlacht);
   - ohne Zufall (dann wäre jeder Lauf ohne Spieler gleich, gegen die Abnahme).
7. **Arbeitskräfte.** Empfehlung: **in M10 nicht**: Die Mobilmachung kostet Geld, Gefallene mindern
   die Bevölkerung, die Wirtschaft verliert keine Arbeitskräfte. Alternative: Reservisten fehlen in
   ihren Orten als Arbeitskräfte (genauer, aber ein Eingriff in die Kalibrierung der Wirtschaft).
8. **Versorgung.** Empfehlung: **Bahnhöfe der eigenen Seite** in drei Tagesmärschen (Abschnitt 3.5).
   Alternativen: Magazine und Nachschubwege (für 1864–1871 zu fein); keine Versorgung.
9. **Krieg aus Krise und Bund.** Empfehlung: **ja**: Eine Krise, in der nach der dritten Stufe
   keine Seite nachgibt (Stärke über zwei Dritteln der anderen), wird Krieg statt Kompromiss; eine
   angenommene Bundesintervention wird Bundesexekution. Alternative: Krieg nur durch Erklärung,
   Krisen enden weiter mit Kompromiss.
10. **Frieden.** Empfehlung: **Kriegspunkte und Bedingungen mit Kosten** (Abschnitt 4.5), Annahme
    nach Regel. Alternative: Friedenskonferenz als eigener Bildschirm (Kapitel 14, Abschnitt 7.3),
    erst mit M12/M13.
11. **Besitzwechsel zur Laufzeit.** Empfehlung: **ja**, mit Untergang von Staaten und Auflösung des
    Bundes (Abschnitt 4.5); nötig für 1866 und 1871. Alternative: Gebiete wechseln nur beim Laden
    eines Lesezeichens, der Frieden setzt nur Zahlungen und Bundesführung um.
12. **Außenmächte.** Empfehlung: **Frankreich** als Staat mit den 168 Orten im Kartenausschnitt und
    Hinterland als Zahlen; Italien, Russland und Großbritannien nicht (Abschnitt 6.2).
    Alternativen: Frankreich ohne Orte (der Krieg 1870 fände nur auf deutschem Boden statt); oder
    Italien als Akteur ohne Orte, der 1866 die Südarmee bindet.
13. **Ausgangslagen.** Empfehlung: **drei Kriegslesezeichen** 1864, 1866 und 1870 auf denselben
    Daten mit datierter Historie (Abschnitt 6.1). Alternativen:
    - ein Lesezeichen „Blut und Eisen“ (23.9.1862, Kapitel 04) und die Kriege daraus (braucht
      Ereignisse und KI: M11, M12);
    - die Kriege nur aus dem Lesezeichen 1848 (keine historischen Ausgangslagen).
14. **Daten, Quellen und 1648.** Empfehlung: **1648 bitgleich** (Heere nur mit Militärdaten);
    Bevölkerung der Kriegslesezeichen aus 1846 ohne Hochrechnung (die Heere kommen aus den Daten des
    Aufmarschs), Abweichung dokumentiert; Stärken und Daten `ungeprueft`, Kampfwerte `spielwert`.
    Alternativen: Bevölkerung je Staat auf 1864/1867 hochrechnen (Zollvereinszählungen); Fachprüfung
    vor der Umsetzung.

## 11. Umsetzungsschritte

| Schritt | Inhalt | Prüfung |
|---|---|---|
| a1 | **Nachbarschaft und Heere:** `nachbarn.csv` aus dem Kartengenerator, Überfahrten, Lader; Militär je Staat, Garnisonen, Friedensheer; Zustand, Format 4, Fingerabdruck `militaer` | Einheitentests, 1648 bitgleich |
| a2 | **Bewegung:** Wege, Marsch, Eisenbahn mit Kapazität, Versorgung, Mobilmachung mit Kosten; Befehle `Marsch`, `Mobilmachung` | Einheitentests, Spielstände |
| a3 | **Krieg und Schlacht:** Kriegserklärung, Bündnisfall, Schlacht, Besetzung, Belagerung, Verluste in der Bevölkerung, Chronik; Krise → Krieg, Bundesexekution | Einheitentests, Wiederholung mit Befehlen |
| a4 | **Frieden und Generalstab:** Kriegsziele, Kriegspunkte, Kriegsmüdigkeit, Friedensangebot, Besitzwechsel und Untergang von Staaten; Generalstab nach Regel | Einheitentests, Lauf ohne Spieler auf der Fixture |
| a5 | **Kriegslabor** `uhrwerk-cli krieg` mit Kriegszielen; CTest auf der Test-Fixture | CI |
| b1 | **Daten 1849–1870** und **Kriegslesezeichen**: Staaten, Besitz, Eisenbahnen, Zollverein, Bund, Verträge, Festungen, Frankreich mit Hinterland, Aufmarsch | Archivar, `pruefen`, Rauchtest |
| b2 | **Kalibrierung** mit dem Kriegslabor gegen die Kriegsziele der drei Lesezeichen | Kriegslabor in der CI |
| b3 | **Brücke und Godot:** Heere auf der Karte, Befehle, Heeres- und Kriegsfenster, Frieden, Kartenmodus „Krieg“, Lesezeichen im Menü | Godot-Tests, Screenshots |
| b4 | **Doku und Review** (Abschluss M10) | CI grün |

## 12. Umsetzungsstand

| Schritt | Stand |
|---|---|
| a1 Nachbarschaft und Heere | Kartengenerator: `nachbarn.csv` mit Grenzlänge und gekreuztem Fluss (Kreuzung der Verbindungslinie mit den Flusslinien statt einer Pixelzählung an der Grenze, die bei Flüssen in der Ortsmitte versagt), Testdaten und Tests. `karte/ueberfahrten.csv` mit 13 Fähren und Übergängen (Rügen, Hiddensee, Alsen, Fehmarn, Sylt, Föhr, Pellworm, Ærø, Wangerooge, Langeoog, Norderney, Juist, Borkum). `common/militaer.chron` (chronik-skript.md 3.30): Militär ab 1848.03.01, Flussrang 3, Heerwesen Standard, Preußen und Österreich, 18 Garnisonen (Preußen: Garde und acht Armeekorps, Österreich sechs Generalkommanden, Bayern drei), die übrigen Staaten in ihrer Hauptstadt: 53 Garnisonen, 382.528 Soldaten. Kern: `military.hpp` (Friedensstärke, Nachbarschaft, Flussübergang, Heere beim Start und am Monatsersten), Lader mit Prüfungen, Zustand `heere`, Inhaltsbereich `militaer` mit eigenem Stand, Spielstand Format 4, `uhrwerk-cli info` mit Nachbarschaft und Garnisonen. Tests: Lader, Heere, Spielstand und Wiederholung, Fingerabdruck, Ladefehler; Format 3 zeigt Metadaten, lädt nicht. 1648 bitgleich (`5db13a66d63e6079`). |
| a2 Bewegung | Befehle `Marsch` (Heer, Zielort; ohne Ziel Halt) und `Mobilmachung` (1 mobil, 0 Abrüstung). Mobilmachung je Staat mit Phasen, Wachstum der Feldarmeen bis zur Kriegsstärke, Abrüstung in 30 Tagen zurück in die Garnisonen; Heereskosten nach Soldaten ÷ Friedensstärke. Wege mit Dijkstra über Marsch und Bahn (Gelände, Flussübergang, Überfahrt, Einladen nach Kapazität, Fahrt, Ausladen), Belegung je Bahnstrecke mit Warten, Bewegung mit Übertrag der Resttage. Versorgung (eigenes und verbündetes Gebiet, sonst Bahnhof in drei Tagesmärschen) und Zermürbung mit Winter, Moral. Krise Stufe 3 → Mobilmachung beider Hauptparteien, Ende der Krise → Abrüstung (statt Faktor 1,5 der Diplomatie). Regeln in `common/militaer.chron` (chronik-skript.md 3.30), Inhaltsbereich `militaer` Stand 2, Zustand: Weg und Resttage je Heer, Mobilmachung je Staat, Belegung je Strecke. Testbefehle des Speichertests mit Marsch und Mobilmachung. Brücke: `get_diplomacy` liefert `army_cost_factor`, das Diplomatiefenster zeigt den echten Faktor statt fest 1,5 (Heere auf der Karte und Befehle in der Oberfläche folgen mit b3). Tests: Marschtage, Zugang, Wege zu Fuß und mit der Bahn, Bündnis, Mobilmachung mit Kosten, Bahnfahrt mit Warten, Halt, Abrüstung, Versorgung und Winter, Krise, Spielstand während Mobilmachung, Marsch und Bahnfahrt, Wiederholung, Ladefehler. 1648 bitgleich, Wirtschaft 1848 ohne Befehle zeichengleich. |
