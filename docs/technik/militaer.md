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
auf der Bahn nimmt ein Heer keine Befehle an. Zugang in a2: eigene und verbündete Orte; seit a3 auch
die der Kriegsgegner und die Orte, die die eigene Seite besetzt hält. Die Bahn fährt nur zwischen
Bahnhöfen, die die eigene Seite kontrolliert. Erreicht ein Heer einen Ort mit einem gegnerischen Heer,
bleibt es dort stehen (Schlacht).

### 3.5 Versorgung und Zermürbung

Versorgt ist ein Heer in eigenem oder verbündetem Gebiet und im Feindesland, solange es höchstens
drei Tagesmärsche von einem Bahnhof hat, den seine Seite hält (Kapitel 15, Abschnitt 4: Eisenbahn ab
1850). Zermürbung je Tag (Krankheit, Abgänge): versorgt 0,05 %, unversorgt 0,5 % und Moral −0,02;
im Winter (Dezember bis Februar) doppelt. Belagerer zählen wie unversorgt, wenn die Festung den
Bahnhof sperrt.

Umsetzung (Schritt a2): Zermürbung gilt nur für Feldarmeen, nicht für Garnisonen im Frieden. Die drei
Tagesmärsche zum Bahnhof sind Marschtage wie bei der Bewegung; Orte ohne Daten liegen nicht auf dem
Weg. In Ruhe und versorgt erholt sich die Moral um 0,01 je Tag. Die Geschütze folgen den Soldaten
(Heerwesen). Seit a3 zählt die Versorgung nach der Kontrolle (besetzte Orte versorgen die Besetzer),
und die Abgänge mindern wie die Gefallenen die Bevölkerung (Abschnitt 4.4). Ohne Krieg stehen alle
Heere in eigenem oder verbündetem Gebiet und sind versorgt.

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

Umsetzung (Schritt a3): Befehl `Kriegserklaerung` (Staat, Gegner; die Kriegsziele kommen mit a4).
Gültig, wenn beide Orte haben, verschieden sind, nicht schon im Krieg miteinander stehen und nicht auf
derselben Seite eines Krieges. Bündnisfall nach `war_sides`: Verbündete und Garanten des Ziels stehen
bei ihm, Verbündete des Angreifers bei diesem, wer zu beiden gehört, bleibt neutral. Mit dem Krieg
enden alle Verträge (Bündnisse und Garantien) zwischen den Seiten, und alle Kriegführenden machen
mobil; im Krieg rüstet niemand ab. Eine Krise, in der nach der dritten Stufe keine Seite nachgibt,
wird zum Krieg der beiden Seiten des Spiels (Ausgang „Krieg“ statt Kompromiss); eine angenommene
Bundesintervention wird Bundesexekution: Antragsteller und Befürworter gegen das Ziel, dem seine
Verbündeten und Garanten beistehen. Wer auf derselben Seite eines Krieges steht, gilt als eigene
Seite (Zugang, Versorgung, Bahn). Lesezeichen im Krieg seit b1 (Abschnitt 6.1).

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

Umsetzung (Schritt a3): Verteidiger ist, wer zum Herrn des Ortes (Besetzer, sonst Besitzer) hält,
Angreifer, wer mit ihm im Krieg ist; Heere auf der Bahn und auf dem Rückzug kämpfen nicht. Feuerkraft =
Soldaten × Bewaffnung + Geschütze × `geschuetz_wert` (50); Frontbreite 50.000 ÷ Geländefaktor des
Ortes; Gelände stärkt den Verteidiger um die Hälfte seines Mehrwerts (Hochgebirge × 1,5), eine
Festung um 0,5 je Stufe, ein Flussübergang des Angreifers um × 1,5 (nach dem Anteil der Soldaten, die
über den Fluss kamen). Führung gibt es noch nicht (Faktor 1). Verlustrate 0,08, Moral −= Verluste ÷
Soldaten × 5; Bruch unter Moral 0,3 oder bei Tagesverlusten über einem Viertel; brechen beide, weicht
die Seite mit der geringeren Moral, bei Gleichstand der Angreifer. Die Verlierer ziehen sich in einen
Nachbarort zurück (zugänglich, ohne Gegner, bevorzugt von der eigenen Seite gehalten, sonst der
nächste) und verlieren dabei ein Zehntel als Gefangene; wer nicht ausweichen kann, ergibt sich ganz.
Gefallene ein Viertel der Verluste, von den Verwundeten kehrt die Hälfte nach 60 Tagen zu ihrem Heer
zurück (sonst zum ersten Heer des Staates), die übrigen sind Invaliden. Gefangene stehen je Staat und
Gewahrsam im Zustand und kehren mit dem Frieden zurück (a4). Verwüstung: Verluste ÷ 100.000. Ein
Heer, dessen Garnison im Krieg unterging, wird mit der Abrüstung neu aufgestellt.

### 4.3 Besetzung und Belagerung

- Ein gegnerischer Ort ohne gegnerisches Heer wird nach einem Tag **besetzt**: Er bleibt im Besitz
  seines Staates, wird aber von der anderen Seite kontrolliert. Seine Steuern fehlen dem Besitzer;
  die Hälfte geht als Kontribution an den Besetzer.
- **Festungen** (Daten: Stufe 1–3, z. B. Düppel 3, Mainz 3, Metz 3, Straßburg 3) müssen belagert
  werden: Tage = 30 × Stufe × Besatzung ÷ (Belagerer mit Geschützen), mindestens 10; ein Entsatzheer
  im Ort bricht die Belagerung. Die Besatzung ist Teil eines Heeres.
- Befreit wird ein Ort durch ein eigenes Heer ohne Gegner.

Umsetzung (Schritt a3): Kontrolle je Ort im Zustand (`kontrolle`, leer = Besitzer). Ein Heer in Ruhe,
das einen vollen Tag in einem Ort des Gegners steht, ohne dass dort ein Heer der anderen Seite steht,
besetzt ihn für seinen Staat; ein Verbündeter des Besitzers befreit ihn für diesen. Die Chronik meldet
Besetzung und Befreiung von Hauptstädten. Festungen stehen in `common/militaer.chron` (`festung`,
Stufe 1–3; 1848: 35 Bundes- und Landesfestungen als Spielwert). Die Besatzung ist abweichend vom
Entwurf kein Teil eines Heeres, sondern eine Zahl (`festung_besatzung` 5.000 je Stufe): Belagerung
täglich um 1 ÷ Tage, Tage = 30 × Stufe × Besatzung ÷ (Soldaten + Geschütze × 50 der Belagerer),
mindestens 10; Belagerer in der Lage „Belagerung“. Ziehen sie ab oder kommt ein Entsatzheer
(Schlacht), verfällt der Fortschritt. Die Besatzung geht beim Fall nicht in Gefangenschaft (sie ist
kein Heer). Kontribution: Die Steuern besetzter Orte fehlen dem Besitzer, die Hälfte aller Steuerarten
geht an den Besetzer als Kontribution.

### 4.4 Verluste und Bevölkerung

Gefallene und Zermürbung mindern die Bevölkerung der Heimatorte (Jahrgänge im wehrfähigen Alter,
anteilig nach Einwohnern); so wirken die Verluste auf die Demografie (Kapitel 15, Abschnitt 3).

Umsetzung (Schritt a3): Gefallene und Abgänge sammeln sich je Staat (`gefallene`) und mindern am
Monatsersten die Jungen und Erwerbstätigen aller Orte des Staates um denselben Anteil; sie zählen in
der Bilanz der Orte als Sterbefälle.

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

Umsetzung (Schritt a4): Kriegsziele kommen mit der Kriegserklärung (Wert = Art + 8 × Parameter: 1
Gebiet mit der Verwaltungseinheit, 2 Annexion mit dem Staat, 3 Führung im Bund, 4 Zollunion, 5
Entschädigung mit 1–24 Monaten) oder mit dem neuen Befehl `Kriegsziel` der Hauptpartei. Eine Krise,
die zum Krieg wird, hat das Ziel ihrer Forderung (Führung, Zollunion; beim Bündnis eine Entschädigung
von drei Monatseinnahmen), die Bundesexekution sechs Monatseinnahmen. Kriegspunkte und Müdigkeit
folgen jeweils aus dem Zustand (Bilanz je Krieg: zugefügte Verluste mit Gefangenen, vernichtete Heere,
Kriegsstärke beim Beginn, letzte Bewegung). Ein Gebiet wechselt mit allen Orten der Gegenseite in der
Einheit (auch untergeordneten); die Orte behalten Verwaltungseinheit und Bundesgebiet. Befehl
`Friedensangebot` (Bedingungen 0 ohne Gewinner, 1 eigene Ziele, 2 Kapitulation); die andere
Hauptpartei nimmt Ziele an, wenn die Punkte des Anbietenden oder ihre eigene Müdigkeit die Kosten
decken, einen weißen Frieden, wenn sie selbst unter 25 Punkten steht oder ihre Müdigkeit 50 erreicht,
eine Kapitulation immer. Ohne Angebot fällt am Monatsersten der Frieden nach Regel: Deckt eine Seite
die Kosten ihrer Ziele, siegt sie; ist eine Seite erschöpft (Müdigkeit 100), siegt die andere mit
ihren Zielen (ohne Ziele weißer Frieden); nach 12 Monaten ohne Schlacht und Besetzung endet der Krieg
ohne Gewinner. Mit dem Frieden kehren die Gefangenen heim, die Besetzungen enden, Heere in nun fremdem
Gebiet kehren in ihre Garnison oder Hauptstadt zurück, und wer in keinem Krieg mehr steht, rüstet ab.
Verliert ein Staat seine Hauptstadt, wird sein bevölkerungsreichster Ort Hauptstadt. Führung im Bund:
Der Verlierer verlässt den Bund, das Präsidium geht an den Sieger; hatte der Verlierer das Präsidium,
löst sich der Bund auf (alle Staaten verlassen ihn). Nach einem Besitzwechsel baut die Simulation
Rechentage, Märkte und Wanderungsziele neu auf.

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

Umsetzung (Schritt a4): Befehl `Generalstab` (Heer übergeben oder zurücknehmen); ein eigener
`Marsch` nimmt das Heer dem Generalstab ab. Ob die Heere zu Beginn beim Generalstab stehen, sagt
`generalstab` in `common/militaer.chron` (1848: ja). Täglich entscheidet er für jedes seiner Heere, das
seit einem Tag ruht und dessen Staat im Krieg steht, in dieser Reihenfolge: Rückzug in die Hauptstadt
vor doppelter Übermacht im Umkreis von 60 km; Angriff auf das nächste gegnerische Heer in 300 km, wenn
die eigenen Heere im Umkreis es samt seinen Nachbarn um das 1,2-Fache übertreffen; sonst Vormarsch auf
den nächsten Ort mit Vorrang Befreiung eigener Orte, Gebiet der Kriegsziele, Hauptstadt der
gegnerischen Hauptpartei, übrige Orte der Gegenseite (eine verteidigende Seite ohne Ziele befreit nur),
wenn dort keine Übermacht steht. Mobilmachung kommt mit dem Krieg (a3), Festungen werden nur belagert,
wenn ein Heer dort ruht, Frieden nach Abschnitt 4.5.

Die Regeln sind deterministisch; Unterschiede zwischen Läufen entstehen aus dem Zufall der
Schlachten. Das genügt der Abnahme („plausibel, nicht festgelegt“); bessere Entscheidungen bringt
die KI in M12. Seit der Kalibrierung (b2) gelten die geänderten Regeln aus Abschnitt 6.3 (Kampfkraft,
Sturm oder Einschließung, Sammeln, Rückzug in die stärkere Festung, Erholung).

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

Umsetzung (Schritt b1, Kern): Der Aufmarsch steht in `common/aufmarsch_<lesezeichen>.chron`
(chronik-skript.md 3.32): Heere mit Staat, Name, Befehlshaber, Führung (0–3), Ort, Soldaten und Moral,
die Kriege zum Start mit festen Seiten, Beginn, Anlass und den Zielen beider Seiten, dazu Bündnisse und
Garantien. Beim Start stehen die Heere als Feldarmeen, ihre Staaten sind mobil, die Garnisonen bleiben
als Besatzung; die übrigen Kriegführenden machen wie bei einer Erklärung mobil. Die Kriegsstärke einer
Seite ist seitdem mindestens die Zahl ihrer Soldaten. Die Führung wirkt als Faktor
1 + `fuehrung` × (Führung − 1) auf die Kampfkraft eines Heeres (Regel in `militaer.chron`, ohne
Befehlshaber Führung 1, also ohne Wirkung). Nach dem Frieden lösen sich Heere ohne Garnison mit der
Abrüstung auf. Das Kriegslabor beobachtet einen Krieg aus dem Lesezeichen, statt ihn zu erklären.

Umsetzung (Schritt b1, Lesezeichen): `szenarien/dueppel.chron`, `koeniggraetz.chron` und
`sedan.chron` mit Aufmarsch in `common/aufmarsch_<lesezeichen>.chron`. 1864: Preußen und Österreich
(Bündnis) gegen Dänemark um Schleswig, Holstein und Lauenburg; I. Korps bei Eckernförde,
Gardedivision und VI. Korps (Gablenz) bei Rendsburg, die dänische Armee am Danewerk, die Düppeler
Schanzen als Festung bei Gravenstein; Dänemark mit Hinterland. 1866: Preußen und die norddeutschen
Staaten gegen Österreich, Sachsen, Hannover, Bayern, Württemberg, Baden, beide Hessen, Nassau,
Frankfurt, Sachsen-Meiningen, Reuß ä. L. und Liechtenstein; Ziele Führung im Bund, die Annexion von
Hannover, Kurhessen, Nassau und Frankfurt und Holstein, Österreich eine Entschädigung; Elbarmee bei
Torgau, I. Armee bei Görlitz, II. Armee bei Neisse, Westarmee bei Minden, Division Manteuffel in
Rendsburg gegen die Nordarmee bei Olmütz, ihr I. Korps bei Jungbunzlau, die Sachsen in Dresden, die
Hannoveraner in Göttingen und die Bundeskorps am Main. 1870: Frankreich gegen den Norddeutschen Bund und
die süddeutschen Staaten (Schutz- und Trutzbündnisse), deutsche Ziele Elsass, Moselle und eine
Entschädigung, französisches Ziel die Pfalz (Spielwert); drei deutsche Armeen an Saar und Pfalz gegen die
Rheinarmee in Metz, Forbach, Bitsch, Straßburg, Belfort und Verdun. Erste Läufe ohne Spieler (je zwei
Seeds, höchstens ein Jahr) zeigen den Kalibrierungsbedarf für b2: 1864 kein Frieden binnen eines Jahres,
1866 siegt Österreich nach 6–9 Monaten, 1870 nimmt Frankreich in 13 Tagen die Pfalz (das Gebietsziel
ist zu billig).

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

Umsetzung (Schritt b1, Daten): Besitz, Bundesgebiet und Verwaltung 1849–1870 stehen als datierte
Änderungen in `data/tabellen/aenderungen_1849_1870.csv`; das Werkzeug
`tools/geodaten/historie_1849_1870.py` wendet sie auf die Zuordnung von 1848 an und schreibt
`besitz.csv` (tabellen.md). Frankreich (FRK) gehört ab 1849 zur Welt, das Lesezeichen 1848 bleibt
ohne Frankreich: 217 Orte im Kartenausschnitt (Elsass, Lothringen, Ardennen, Französisch-Flandern,
Hennegau und Burgundische Pforte) mit 4,01 Millionen Einwohnern nach der Zählung 1846. Vereinfacht:
Schleswig und Lauenburg werden schon mit dem Wiener Frieden preußisch, Holstein österreichisch (die
Teilung von Gastein 1865); die annektierten Länder erhalten 1866 nur ihre Provinz als Verwaltung.
Das Feld `bund` heißt ab dem 1. Juli 1867 Gebiet des Norddeutschen Bundes, dessen Bundesrat die
Bundesversammlung ersetzt (`bis` bei Bundesversammlung, Zollunion und Parlament,
chronik-skript.md 3.21, 3.25 und 3.29). Dänemark hat nach 1864 keinen Ort im Kartenausschnitt
mehr und fehlt in den Lesezeichen 1866 und 1870.

Umsetzung (Schritt b1, Eisenbahnen): 62 Strecken mit 185 Abschnitten von 1849 bis 1870 in
`eisenbahnen.csv` (zusammen 361 Abschnitte), darunter die Aufmarschlinien nach Böhmen (Dresden–
Bodenbach–Prag, Görlitz und Zittau–Reichenberg–Turnau–Josefstadt–Königgrätz–Pardubitz, Berlin–Görlitz
1867), an den Rhein (Main-Weser, Rhein-Nahe, Saarbrücken, Pfalz, Ludwigshafen–Weißenburg–Straßburg,
Metz–Forbach) und nach Schleswig (Rendsburg–Flensburg). Weil die Strecken an ihrem Tag eröffnen, fährt
auch ein Spiel ab 1848 von 1849 an auf ihnen; die Zahlen 1848 ändern sich damit nach dem ersten Jahr,
alle Korridore von Balancing-Bericht, Wahl-, Zoll- und Kriegslabor 1848 bleiben erfüllt.

Umsetzung (Schritt b1, Kern): Das Hinterland steht je Staat in `militaer.chron` (`hinterland <TAG>`,
chronik-skript.md 3.30) mit Bevölkerung, Einnahmen je Monat, Ersatz je Kriegsmonat und den Orten am
Kartenrand. Am Monatsersten fließen die Einnahmen in die Kasse; im Krieg trifft der Ersatz im ersten
dieser Orte ein, den die eigene Seite hält (bei einer Feldarmee ohne Garnison dort, sonst als neues
Heer „Ersatz <Ort>“). Die Bevölkerung des Hinterlands zählt bei Besetzung, Kriegspunkten und
Kriegsmüdigkeit mit und kann nicht besetzt werden; die Hauptstadt auf der Karte bringt dem Gegner keine
Punkte (Paris liegt außerhalb).

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

Umsetzung (Schritt a5): Bis die Kriegslesezeichen kommen (b1), nennen die Kriegsziele die Kriege
selbst (`krieg <kennung> { angreifer verteidiger ziel … }`, chronik-skript.md 3.31): Je Seed erklärt
der Angreifer am Startdatum den Krieg mit seinen Zielen (führt das Szenario ihn schon, entfällt die
Erklärung), alle Heere führt der Generalstab, gerechnet wird bis zum Frieden oder höchstens `tage`
(Standard 730). Je Lauf berichtet das Labor Ausgang (Angreifer siegt, Verteidiger siegt, ohne Gewinner,
kein Frieden), Tage, Schlachten, Verluste beider Seiten, gewechselte Orte, die Schulden der
Hauptparteien (Ende minus Start) und die Chronik des Friedensschlusses; über die Läufe die Anteile
der Ausgänge und Median mit 5- und 95-%-Wert. Geprüft werden die Anteile (`sieg`, `niederlage`,
`weiss`), der Median der Dauer (`dauer`) und mit `offen = ja`, dass kein Ausgang in allen Läufen
eintritt; ein Lauf ohne gültige Erklärung ist ein Verstoß (Exitcode 1). Dafür hält der Zustand je
beendetem Krieg ein Ergebnis (`kriegsergebnisse`: Seiten, Anlass, Beginn, Ende, Sieger, Schlachten,
Verluste, gewechselte Orte) und je Krieg die Zahl der Schlachten. Kriegsziele 1848
(`kriegsziele_voelkerfruehling.chron`): Preußen gegen Sachsen um die Annexion, Sieg in 80–100 % der
Läufe, Dauer 30–180 Tage; `offen` erst mit der Kalibrierung (b2). CTest `krieg_fixture` auf der
Fixture mit Militärdaten (`tests/daten_krieg` über `tests/daten`): drei Kriege über 16 Seeds.

Umsetzung (Schritt b2, Kalibrierung): Die drei Kriege liefen zuerst ins Leere (der Generalstab
marschierte ohne Unterlass auf ferne Hauptstädte, Heere zogen aneinander vorbei, kleine Ersatzheere
entschieden Kriege, Festungen schützten ihre Verteidiger nicht). Geändert haben sich Regeln, keine
Kriegsziele der Spieler:

- **Generalstab** (Abschnitt 5) rechnet mit Kampfkraft (Abschnitt 4.2) statt Soldaten, Gegner in einer
  Festung, die sie halten, mal deren Faktor. Ein Ziel kann er nehmen, wenn die Kampfkraft im Umkreis
  die der Gegner um `generalstab_angriff` übertrifft (Sturm) oder, bei einer Festung, die Soldaten um so
  viel (Einschließung). Angriff auf das nächste gegnerische Heer in Reichweite, das er nehmen kann,
  sofern keine Übermacht näher steht; mit einer Übermacht in Reichweite rückt er nicht vor, sondern
  sammelt sich beim stärksten eigenen Heer; Ziele des Vormarschs nur in Reichweite (`generalstab_
  reichweite_km` 150), sonst der nächste Ort überhaupt (Vormarsch an die Front); je Stufe der nächste
  Ort ohne Übermacht. Rückzug vor doppelter Übermacht oder mit Moral unter `generalstab_moral` (0,5)
  bei Gegnern im Umkreis: in die nächste stärkere eigene Festung, die weiter vom Gegner liegt (aus
  einer Festung schon vor der Übermacht, die einschließen würde: 1864 vom Danewerk nach Düppel), sonst
  hält das Heer eine Festung in einem Gebiet, das der Gegner fordert, oder geht zum stärksten eigenen
  Heer in Reichweite, zuletzt in die Hauptstadt. Unter `generalstab_moral` ohne Gegner erholt es sich.
  Ein eingeschlossenes Heer bleibt.
- **Besetzung im Vorbeimarsch** (Abschnitt 4.3): Ein Heer auf dem Marsch nimmt einen Ort ohne Festung
  und ohne Heer der Gegenseite bei der Ankunft und beim Abmarsch nach einem vollen Tag.
- **Einschließung** (4.3): Ein Heer in einer Festung, das die Angreifer an Soldaten um
  `generalstab_angriff` übertreffen, an Kampfkraft aber nicht um so viel über den Festungsfaktor
  hinaus, wird nicht gestürmt, sondern mit der Festung belagert (seine Soldaten zählen zur Besatzung)
  und ergibt sich mit ihr in Gefangenschaft. Die Festung mindert seit b2 auch die **Verluste** der
  Verteidiger (÷ Festungsfaktor).
- **Moral** (4.2): Die Sieger einer Schlacht gewinnen `moral_sieg` (0,3) zurück; `moral_erholung`
  0,04.
- **Kriegspunkte und Frieden** (4.5): Ein vernichtetes Heer zählt nur ab `punkte_heer_anteil` (0,1)
  der Kriegsstärke seiner Seite; Gebiets- und Annexionsziele kosten nach dem Anteil an der Bevölkerung
  der Gegenseite **mit** Hinterland (`ziel_annexion` 80 nur für die ganze Gegenseite); Frieden nach
  Regel auch, wenn die Kriegsmüdigkeit der Gegenseite die Kosten der Ziele erreicht (wie beim
  Friedensangebot). Ein Staat mit Hinterland geht ohne Orte nicht unter, er **verlässt die Karte**
  (Kasse und Schulden bleiben bei ihm). Regeln: `ziel_gebiet_mindestens` 5, `ziel_fuehrung` 20,
  `muedigkeit_besetzung` 100.
- **Aufmarsch:** `besetzt = { gebiet = … }` im Krieg zum Start (chronik-skript.md 3.32): 1864 halten
  die Verbündeten Holstein und Lauenburg seit der Bundesexekution.
- **Daten:** dänische Armee 31.000 (mit der Besatzung des Danewerks 38.000), Düppel Stufe 3;
  1866 die II. Armee bei Landeshut, Österreich Bewaffnung 1,2 und Führung Benedeks 1,0, österreichische
  Ziele Schlesien und zwei Monatseinnahmen; 1870 französische Ziele Pfalz und neun Monatseinnahmen.

Ergebnis über 16 Seeds (`kriegsziele_<lesezeichen>.chron`, CI-Schritt „Kriegslabor Lesezeichen“):

| Lesezeichen | Ausgang | Dauer (Median, 5–95 %) | Korridor |
|---|---|---|---|
| Düppel | Preußen siegt 16 von 16 | 90 (60–90) Tage | Sieg 80–100 %, Dauer 60–270 Tage |
| Königgrätz | Preußen siegt 11, Österreich 5 | 107 (77–138) Tage | Sieg 60–90 %, Dauer 21–112 Tage, offen |
| Sedan | die deutsche Seite siegt 14, Frankreich 2 | 197 (135–347) Tage | Niederlage des Angreifers 75–100 %, Dauer 60–300 Tage, offen |

Abweichung: **Düppel** ohne `offen`: Dänemark verliert Schleswig in allen Läufen; auf der Karte hat es
nur die Herzogtümer, deren Besetzung seine Kriegsmüdigkeit in zwei bis drei Monaten über die Kosten
hebt. Die Läufe unterscheiden sich im Verlauf (Sturm auf Düppel oder Einschließung) und in der Dauer.
Ebenso 1848 (Preußen annektiert Sachsen in 16 von 16 Läufen nach 31 Tagen). Ein Spiel ab 1848 rechnet
ohne Befehle bitgleich zu b1 (keine Kriege). Die Fixture `tests/daten_krieg` hält das neue Verhalten
fest (Brandenburg mit Mobilmachung 2, Sachsen siegt am ersten Monatsersten).

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
| a3 Krieg und Schlacht | Befehl `Kriegserklaerung` mit Bündnisfall, Beziehung −20 bei allen Bundesstaaten, Ende der Verträge zwischen den Seiten, Mobilmachung aller Kriegführenden; Krise → Krieg (Ausgang „Krieg“), Bundesintervention → Bundesexekution. Kontrolle je Ort, Zugang und Bahn nach Kontrolle. Tägliche Schlacht mit Zufallsstrom `krieg` (Frontbreite, Gelände, Festung, Flussübergang, Geschütze, Moral, Bruch), Rückzug mit Gefangenen, Kapitulation, Verwüstung, Chronik. Besetzung nach einem vollen Tag, Befreiung, Kontribution; Festungen (35 im Jahr 1848) mit Belagerung. Verwundete kehren nach 60 Tagen zurück, Gefangene je Staat, Gefallene und Abgänge mindern am Monatsersten die Bevölkerung. Regeln in `common/militaer.chron`, Inhaltsbereich `militaer` Stand 3, Zustand mit Kriegen, Kontrolle, Schlachten, Belagerungen, Verwundeten, Gefangenen, Gefallenen und Zufallsstrom; Testbefehle mit Kriegserklärungen. Tests: Bündnisfall, Erklärung, Schlacht mit Rückzug, Besetzung und Verlusten, Kontribution, Belagerung, Krise → Krieg, Bundesexekution, Spielstand während Schlacht und Belagerung mit Wiederholung und Threads, Ladefehler. 1648 bitgleich, 1848 ohne Befehle zeichengleich. |
| a4 Frieden und Generalstab | Kriegsziele (Gebiet, Annexion, Führung im Bund, Zollunion, Entschädigung) mit der Erklärung oder dem Befehl `Kriegsziel`, Kosten in Kriegspunkten; Kriegspunkte aus Besetzung, Hauptstadt, Schlachtbilanz und vernichteten Heeren; Kriegsmüdigkeit aus Verlusten, Besetzung und Dauer. Befehl `Friedensangebot` mit Annahme nach Regel, Frieden am Monatsersten nach Regel (Ziele gedeckt, Erschöpfung, 12 Monate Stillstand). Wirkung: Besitzwechsel, Untergang von Staaten (Heere, Verträge, Gesandte, Kasse und Schulden an den Erwerber, Abschied aus dem Bund), Austritt aus dem Bund oder seine Auflösung, Beitritt zur Zollunion, Zahlung; Gefangene heim, Besetzung aufgehoben, Heimkehr, Abrüstung, neue Hauptstadt, Neuaufbau der abgeleiteten Daten. Generalstab nach Regel (Befehl `Generalstab`, eigener Marsch übernimmt das Heer). Regeln in `common/militaer.chron` (1848 mit Generalstab), Inhaltsbereich `militaer` Stand 4, Zustand: Ziele und Bilanz je Krieg, Generalstab je Heer, Austritt aus dem Bund; Testbefehle mit Zielen, Angeboten und Generalstab. Probelauf 1848: Preußen annektiert Sachsen nach zwei Monaten; Preußen gegen Österreich um die Führung endet nach 13 Monaten erschöpft ohne Gewinner (Kalibrierung in b2). Tests: Ziele und Kosten, Punkte und Angebote mit Annexion und Untergang, Gebiet, Entschädigung, Stillstand, Auflösung des Bundes, Generalstab mit Lauf ohne Spieler, Threads, Spielstand und Wiederholung. 1648 bitgleich, 1848 ohne Befehle zeichengleich. |
| a5 Kriegslabor | `uhrwerk-cli krieg` (Seeds, `--ziele`, `--tage`) mit Kriegszielen in `common/kriegsziele_<szenario>.chron` (chronik-skript.md 3.31): je Krieg und Seed ein Lauf ohne Spieler bis zum Frieden, Bericht je Lauf (Ausgang, Tage, Schlachten, Verluste, Orte, Schulden, Friedensschluss) und Verteilung über die Läufe, Verstöße mit Exitcode 1; `pruefen` prüft die Datei. Kern: `war_lab.hpp`, Ergebnis je beendetem Krieg im Zustand (`kriegsergebnisse`), Schlachten je Krieg, Inhaltsbereich `militaer` Stand 5. Kriegsziele 1848: Preußen gegen Sachsen (Sieg in 16 von 16 Läufen nach 61 Tagen im Median, 103 Orte). Fixture mit Militärdaten `tests/daten_krieg` (Nachbarschaft, Fähre, Generalstab, Festung Dresden) und drei Kriegen; CTests `pruefen_krieg_fixture`, `krieg_fixture` (16 Seeds), `krieg_1848`; CI-Schritt „Kriegslabor 1848“ (16 Seeds) im Job „Balancing“. Tests: Leser mit Fehlern, Lauf bis zum Frieden und mit Höchstdauer, gleiche Ergebnisse mit Threads, Ergebnis im Zustand, Zusammenfassung mit Verstößen. 1648 bitgleich, 1848 ohne Befehle zeichengleich. |
| b1 Daten 1849–1870 und Kriegslesezeichen | Kern (b1a): Aufmarsch je Lesezeichen in `common/aufmarsch_<szenario>.chron` (chronik-skript.md 3.32) mit Heeren (Befehlshaber, Führung), Kriegen zum Start mit Zielen beider Seiten, Bündnissen und Garantien; Regel `fuehrung`; Hinterland je Staat (Einnahmen, Ersatz am Kartenrand, Bevölkerung bei Besetzung und Kriegspunkten); Auflösung der Aufmarschheere nach dem Frieden; Kriegsstärke mindestens die Soldaten unter Waffen; `uhrwerk-cli info` mit Aufmarsch, Kriegen und Hinterland; das Kriegslabor beobachtet Kriege aus dem Lesezeichen. Inhaltsbereich `militaer` Stand 6. Tests: Aufmarsch, Bündnis und Krieg zum Start, Hinterland, Führung, Auflösung, Spielstand und Wiederholung, Ladefehler. Daten (b1b): Historie 1849–1870 mit `historie_1849_1870.py` und `aenderungen_1849_1870.csv` (38 Änderungen), Frankreich mit 217 Orten ab 1849, Bundesrat des Norddeutschen Bundes, `bis` bei Bundesversammlung, Zollunion und Parlament; 1648 bitgleich, 1848 zeichengleich. Eisenbahnen (b1c): 62 Strecken mit 185 Abschnitten 1849–1870, ein Spiel ab 1848 bekommt sie zur Eröffnung (Korridore 1848 erfüllt). Militärdaten (b1d): Heerwesen 1859–1867, Garnisonen IX.–XII. Korps und Frankreich, Danewerk, Düppel und 13 französische Festungen, Hinterland Frankreich und Dänemark, Regel `fuehrung` 0,2. Lesezeichen (b1e): `dueppel`, `koeniggraetz`, `sedan` mit Aufmarsch (4, 15 und 14 Heere), Kriegen und Zielen beider Seiten, Bündnissen; Kriegsziele-Dateien ohne Korridore (b2). CTests `daten_pruefen_<lesezeichen>`, `krieg_<lesezeichen>` (ein Seed, 60 Tage) und `speichertest_koeniggraetz`. 1648 bitgleich. |
| b2 Kalibrierung | Kern: Generalstab mit Kampfkraft, Sturm oder Einschließung, Sammeln, Rückzug in die stärkere Festung, Halten der geforderten Festung, Erholung unter `generalstab_moral`; Besetzung im Vorbeimarsch; Einschließung eines Heeres in der Festung mit Kapitulation; Festung mindert die Verluste der Verteidiger; `moral_sieg`; vernichtete Heere ab `punkte_heer_anteil`; Gebiet und Annexion nach dem Anteil der Gegenseite mit Hinterland; Frieden nach Regel bei Müdigkeit ≥ Kosten; ein Staat mit Hinterland verlässt die Karte; `besetzt` im Aufmarsch. Daten: Regeln und Aufmarsch (Abschnitt 6.3), Korridore in `kriegsziele_<lesezeichen>.chron`: Düppel 16/16 in 90 Tagen, Königgrätz 11/16 in 107 Tagen, Sedan 14/16 für die deutsche Seite in 197 Tagen; Düppel ohne `offen` (Abweichung, Abschnitt 6.3). CI-Schritt „Kriegslabor Lesezeichen“, CTests `krieg_<lesezeichen>` prüfen nur den Lauf. Inhaltsbereich `militaer` Stand 7. Tests: Einschließung und Kapitulation, Moral nach Sieg und vor dem Vorrücken, Annexionskosten, Frieden bei Müdigkeit, besetzte Gebiete zum Start, Staat mit Hinterland verlässt die Karte, Ladefehler `besetzt`. 1648 bitgleich, 1848 ohne Befehle bitgleich zu b1. |
| b3 Brücke und Godot | Brücke (b3a): Heere je Staat und einzeln, Wegvorschau mit Ankunft, Kontrolle je Ort, Schlachten, Belagerungen, Kriege mit Seiten, Zielen, Punkten, Müdigkeit und Verlusten, Kriegsziele einer Erklärung und eines laufenden Krieges, Seiten einer Erklärung (`get_declaration_sides`, b3c), Vorab-Prüfung des Friedens (`peace_acceptance` im Kern), Befehle Marsch, Generalstab, Mobilmachung, Kriegserklärung, Kriegsziel und Friedensangebot; Kartenmodus 15 `MAP_KRIEG`. Godot (b3b): Heeresmarken mit Auswahl per Klick, Marschbefehl per Rechtsklick ohne Ziehen mit Vorschau von Weg und Ankunft, Schlachten und Belagerungen als Symbol, Kartenmodus „Krieg“ (Taste Y) mit Schraffur besetzter Orte, Fronten als Grenzen der Kontrolle und Legende der Kriegsparteien. Godot (b3c): Heeresfenster in der rechten Spalte (Befehlshaber, Stärke, Moral, Versorgung, Standort, Ziel; „Dem Generalstab übergeben“ / „Selbst führen“), Kriegsfenster mit den Reitern Kriege (Seiten, Ziele, Punkte, Müdigkeit, Verluste, drei Friedensangebote mit Vorab-Prüfung, weitere Kriegsziele) und Heere, „Krieg erklären …“ im Fenster „Diplomatie“ mit Rückfrage, Mobilmachung im Staatspanel, Schlachtberichte als Meldung; Startoptionen `--krieg`, `--heer`. Tests: Einheitentest (Kartenmodus, Heeresmarken, Meldungen, Optionen), Rauchtest Königgrätz, Klicktest (Lesezeichen im Menü, Auswahl, Marsch, Generalstab, Kriegsfenster, Schlachtbericht, Kriegserklärung). 1648 bitgleich. |
