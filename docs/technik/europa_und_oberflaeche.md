# Europa und Oberfläche (Meilenstein M14)

> **Status: Entwurf zur Freigabe** – Entscheidungen 1–8 mit Empfehlung (Abschnitt 7); Umsetzung erst nach
> Freigabe. Anlass ist die erste Rückmeldung aus dem Spiel (18.11.2026). Grundlage sind
> [22 UI/UX, Grafik, Audio](../22_ui_ux_grafik_audio.md), [05 Karte und Geographie](../05_karte_und_geographie.md),
> der Vertrag von M13 ([vertical_slice.md](vertical_slice.md)) und die Werkzeuge der Karte
> ([tools/geodaten](../../tools/geodaten/README.md), [tools/kartengenerator](../../tools/kartengenerator/README.md)).

## 1. Anlass

Die erste Partie mit dem fertigen Paket ergab fünf Punkte:

| Nr. | Rückmeldung | Stand |
|---|---|---|
| 1 | Die ersten Schritte der Geschichtsstunde liegen über der Ortsinfo | **behoben** (18.11.2026): Das Panel sucht sich einen freien Platz über der Karte, wird sonst kompakt, lässt sich verschieben; der Klicktest prüft es in jedem Schritt |
| 2 | Musik etwas entspannter | **behoben**: „Revolution“ nur bei einem Aufstand im eigenen Staat (vorher das ganze erste Jahr die Marseillaise), ruhige Stücke auch für Krisen, langsamere Tempi, leiseres und weicheres Blech, Pausen 20–45 s, Musik ab Werk 55 % |
| 3 | Geld farbig, rot und grün | **behoben**: Kasse, Saldo, Einnahmen, Ausgaben und Haushalt grün oder rot (bei Rot-Grün-Schwäche Blau statt Grün) |
| 4 | Unübersichtlich und unschön, mehr Leben; Orientierung an Victoria 3 | **dieser Vertrag**, Abschnitte 4–5 |
| 5 | Die Karte soll ganz Europa zur damaligen Zeit zeigen, nicht nur die Umrisse von Deutschland und Österreich | **dieser Vertrag**, Abschnitt 3 |

## 2. Ausgangslage

- **Karte:** Plattkarte mit Bezugsbreite 51 °N, Ausschnitt 1,8–23,5 °O × 45,1–56,2 °N, 4096 × 3329 Pixel
  (0,37 km je Pixel), 4.542 Orte im Kernraum (Deutscher Bund, Preußen, Österreichs Bundesländer).
  Außerhalb des Kernraums ist Land nur Fläche ohne Grenzen („Keine Daten“, schraffiert). Die
  Nachbarschaft der Orte (`karte/nachbarn.csv`, mit Grenz- und Flusslängen) bestimmt die Märsche der Heere.
- **Bildschirm:** Obere Leiste mit vier Kennzahlen als Text, links die Staatsübersicht über die volle Höhe
  und daneben 14 Knöpfe der Bereiche, rechts Ortsinfo, Übersicht und eine große Legende, unten ein Ticker
  und 18 Kartenmodi als Textknöpfe in zwei Reihen. Die Karte bekommt knapp die Hälfte der Fläche; überall
  Papierton, kaum Symbole, keine Bewegung auf der Karte außer Heeren.

## 3. Europa auf der Karte (Rückmeldung 5)

**Ziel:** Die Karte zeigt ganz Europa um 1848 mit den Staaten der Zeit; der Kernraum bleibt so fein, wie er
ist, und der Rest Europas wird zur lebendigen Kulisse, nicht zu einem Loch.

- **Ausschnitt:** etwa 11 °W–45 °O × 34–71 °N: von Portugal und Irland bis an Wolga und Kaukasus, vom
  Nordkap bis Kreta und an die Küste Nordafrikas (diese ohne Staaten, nur als Land).
- **Staaten 1848 außerhalb des Kernraums** (Entscheidung 1): Großbritannien und Irland, Frankreich (mit
  Algerien nur als Land), Spanien, Portugal, die Schweiz, Belgien, die Niederlande, Dänemark (außerhalb des
  Bundes), Schweden-Norwegen, das Russische Reich (mit Kongresspolen, Finnland, den Ostseeprovinzen), das
  Osmanische Reich (mit Serbien, der Walachei und der Moldau als Vasallen), Griechenland, Montenegro, die
  Staaten Italiens (Sardinien-Piemont, Toskana, Parma, Modena, Kirchenstaat, beide Sizilien) sowie
  Österreichs Länder außerhalb des Bundes (Ungarn, Galizien, Lombardo-Venetien, Dalmatien, Siebenbürgen,
  Kroatien-Slawonien). Preußen liegt schon ganz im Kernraum.
- **Was sie können:** Name, Farbe, Hauptstadt, Wappenfarbe, Tooltip und Lexikon; Außenmächte, die die
  Simulation schon kennt (Frankreich, Dänemark), bekommen ihr Gebiet; die übrigen sind Kulisse ohne eigene
  Simulation (Entscheidung 1). Österreichs Länder außerhalb des Bundes tragen Österreichs Farbe.
- **Grenzen** (Entscheidung 2): aus Natural Earth, Admin-1 (Länder und Provinzen, gemeinfrei, 10 m),
  zusammengesetzt zu den Einheiten von 1848 über eine neue Tabelle `data/tabellen/europa_1848.csv`
  (Region → Staat 1848, mit Begründung); wo heutige Provinzgrenzen nicht passen (etwa Kongresspolen,
  Bessarabien, Savoyen und Nizza bei Sardinien), mit wenigen von Hand gezeichneten Korrekturen. Die
  Grenzen sind **vereinfacht**: für die Kulisse genau genug, für eine spätere Simulation der Außenmächte
  zu prüfen. Die Quelle kommt in `tools/geodaten/quellen.json` und `DRITTANBIETER.txt`.
- **Projektion** (Entscheidung 3): Eine Plattkarte über ganz Europa zieht den Norden in die Breite
  (Skandinavien auf 70 °N fast doppelt so breit, Spanien und Italien um ein Fünftel zu schmal). Empfohlen
  ist eine **Lambertsche Kegelprojektion** (winkeltreu, Standardbreiten 43 °N und 62 °N, Mittelmeridian
  15 °O) für die ganze Karte. Dafür wird der Kernraum neu erzeugt: Orte und Kreise bleiben dieselben, ihre
  Pixel und damit die Nachbarschaft der Orte ändern sich leicht. Folge: Die Kriegs- und KI-Labore 1848
  werden neu gerechnet und müssen ihre Korridore halten; 1648 bleibt voraussichtlich bitgleich (keine Heere).
- **Technik** (Entscheidung 4): zwei Ebenen in derselben Projektion. Der Kernraum wie bisher als
  ID-Bild mit Orten (gleiche Auflösung, rund 0,37 km je Pixel); Europa als zweites ID-Bild mit rund
  1,5 km je Pixel (etwa 3.000 × 3.200 Pixel) mit einer ID je Region. Der Kartenshader zeichnet beide; die
  Kamera darf bis auf ganz Europa herauszoomen.

## 4. Die Karte lebendiger (Rückmeldung 4, Kartenbild)

Ziel nach Kapitel 22: Die Karte ist der Star; man sieht Land, Wasser und Gelände und darüber die Staaten.

- **Gelände statt Vollfarbe:** Schummerung aus den Höhendaten (Terrain Tiles, schon vorhanden), Wald und
  Gebirge als dezente Textur, darüber die Staatsfarben halbdurchsichtig; in der Nähe mehr Gelände, in der
  Ferne mehr Farbe.
- **Wasser:** Meer mit Tiefenverlauf und Küstensaum, Seen und Flüsse in Blau statt Tinte.
- **Grenzen in Rangfolge:** Staatsgrenzen kräftig, Bundesgrenze als eigene Linie, Kreise und Orte erst beim
  Heranzoomen.
- **Beschriftung:** Staatsnamen groß und der Fläche folgend, Hauptstädte und große Städte mit Zeichen.
- **Leben** (Entscheidung 5): leichte Bewegung auf dem Wasser, ziehende Wolkenschatten am Rand, Rauch über
  den Industriestädten, Züge auf den Bahnen, Schiffe auf den großen Routen; eine Klangkulisse (Wind, Meer,
  Stadt) leise unter der Musik. Alles abschaltbar („reduzierte Animationen“ gibt es schon).

## 5. Ein übersichtlicher Bildschirm (Rückmeldung 4, Oberfläche)

Leitlinie (Entscheidung 6): die Konventionen des Genres – die Karte zuerst, Kennzahlen oben, Fenster nur
auf Anforderung, Symbole statt Textknöpfe, Meldungen als Karten – in **eigener Gestalt** der Epoche III
(Kupferstich, Papier, eigene Symbole und Schriften). Victoria 3 ist Maßstab für Übersicht und Ordnung,
nicht Vorlage für Aussehen, Grafiken oder Anordnung im Einzelnen.

- **Karte frei:** Beim Spielen sind nur die obere Leiste, eine schmale Symbolleiste links, die Übersicht
  rechts (einklappbar) und die Kartenmodi unten zu sehen; Staatsübersicht, Ortsinfo und Fenster öffnen
  auf Klick als Schubladen und schließen wieder. Die Karte bekommt rund 85 % der Fläche statt rund 45 %.
- **Obere Leiste:** Symbole mit Zahl und Veränderung (grün und rot, Pfeile), Tooltip mit Aufschlüsselung;
  Kasse, Saldo, Legitimität, Unruhe, Bevölkerung, Heer, Vermächtnis.
- **Symbolleiste links** statt 14 Textknöpfen: Bereiche als Symbole mit Namen im Tooltip und Zähler, wo
  etwas zu tun ist.
- **Kartenmodi** als eine Reihe Symbole in Gruppen (Politik, Gesellschaft, Wirtschaft, Militär) statt 18
  Textknöpfen; die Legende klein und einklappbar über den Kartenmodi.
- **Meldungen:** Statt Ticker und Chronikzeile kleine Karten mit Symbol (Wahl, Krieg, Vertrag, Aufstand),
  die sich stapeln und wegklicken lassen; Wichtiges hält die Zeit an wie bisher.
- **Symbole** (Entscheidung 7): ein eigener Satz SVG-Symbole im Stil der Ornamente aus M13b1 (rund 60:
  Kennzahlen, Bereiche, Kartenmodi, Meldungen).
- **Lesbarkeit:** größere Grundschrift für Zahlen, klarere Abstände, Zahlen rechtsbündig mit Einheit, ein
  Wechsel zwischen „kompakt“ und „ausführlich“ in den Einstellungen.

## 6. Tests und Abnahme

- **Karte:** Werkzeugtests für Projektion und Zuordnung (jede Admin-1-Region genau einem Staat oder dem
  Meer/Ausland zugeordnet, keine Lücken), Klicktest auf jeden Ort des Kernraums wie bisher und auf jede
  Region Europas; Prüfsumme 1648 nach 100 Jahren unverändert (`5db13a66d63e6079`), sonst begründet neu;
  Labore 1848 in ihren Korridoren.
- **Oberfläche:** Klick- und Durchspieltest mit dem neuen Aufbau; Sprachtest (Pseudo und Englisch); ein
  neuer Test „freie Karte“: Ohne offenes Fenster verdecken Leisten höchstens 20 % des Bildes.
- **Leistung:** Bildzeiten mit llvmpipe höchstens 10 % über M13 trotz Gelände und Animationen; mit
  Grafikkarte 60 Bilder/s (Spieltest).
- **Abnahme:** ein zweiter Spieltest (Leitfaden aus M13b6) mit der Frage, ob Übersicht und Aussehen
  besser geworden sind (Fragen C3, C5, D6 des Fragebogens im Median mindestens 4).

## 7. Entscheidungen

1. **Rolle der Außenmächte.** Empfehlung: **Kulisse** (Name, Farbe, Grenzen, Tooltip, Lexikon), nur
   Frankreich und Dänemark wie bisher mit Simulation. Alternative: alle Außenmächte simulieren
   (Bevölkerung, Wirtschaft, KI) – Phase 3, viel Datenarbeit.
2. **Quelle der Grenzen.** Empfehlung: **Natural Earth Admin-1 (gemeinfrei) plus eigene Zuordnung 1848**.
   Alternativen: *historical-basemaps* (fertige Grenzen 1815/1880, aber GPL-3.0 und damit nicht mit der
   PolyForm-Lizenz verträglich), Euratlas (genauer, kommerziell, Kosten).
3. **Projektion.** Empfehlung: **Lambertsche Kegelprojektion für die ganze Karte**, Kernraum neu erzeugt,
   Labore 1848 neu. Alternative: Plattkarte behalten (schneller, Kernraum unverändert, Norden und Süden
   Europas sichtbar verzerrt).
4. **Auflösung.** Empfehlung: **Kernraum fein (0,37 km), Europa grob (1,5 km)** in zwei Ebenen.
   Alternative: eine Ebene für alles (einfacher, aber ein Bild von über 10.000 Pixeln Kantenlänge).
5. **Leben auf der Karte.** Empfehlung: **Gelände, Wasser, Wolken, Rauch, Züge und Schiffe**, alles
   abschaltbar. Alternative: nur Gelände und Wasser (weniger Aufwand, weniger Leben).
6. **Leitlinie der Oberfläche.** Empfehlung: **Karte zuerst, Fenster auf Anforderung, Symbole**, in eigener
   Gestalt. Alternative: den heutigen Aufbau behalten und nur aufräumen (schneller, löst „unübersichtlich“
   nur teilweise).
7. **Symbole.** Empfehlung: **eigener SVG-Satz** im Stil der Ornamente. Alternative: freie Symbolsätze
   (etwa Game-icons.net, CC BY 3.0) – schneller, aber fremder Stil und Namensnennung je Symbol.
8. **Reihenfolge.** Empfehlung: **erst die Oberfläche (M14a), dann Europa (M14b)**: Der neue Aufbau ist
   schneller spielbar und bestimmt, wie viel Karte zu sehen ist. Alternative: Europa zuerst.

## 8. Umsetzungsschritte

| Schritt | Inhalt | Prüfung |
|---|---|---|
| a1 | **Aufbau:** Karte frei, Schubladen für Staatsübersicht, Ortsinfo und Fenster, Übersicht einklappbar, Legende kompakt | Klicktest, Test „freie Karte“, Screenshots |
| a2 | **Symbole und Kennzahlen:** SVG-Satz, obere Leiste mit Symbolen und Veränderungen, Symbolleiste der Bereiche, Kartenmodi als Symbole | Klick- und Sprachtest |
| a3 | **Meldungen:** Karten statt Ticker, Stapel, Pausenregeln | Klick- und Durchspieltest |
| b1 | **Europa-Daten:** Admin-1, `europa_1848.csv`, Korrekturen, Projektion im Kartengenerator, Kernraum neu, Nachbarschaft | Werkzeugtests, Labore 1848, 1648 bitgleich |
| b2 | **Kartenbild:** zweite Ebene, Gelände, Wasser, Grenzen in Rangfolge, Namen der Außenmächte, Tooltip und Lexikon | Klicktest auf Europa, Leistung |
| b3 | **Leben:** Wasser, Wolken, Rauch, Züge, Schiffe, Klangkulisse, abschaltbar | Leistung, Einstellungen |
| b4 | **Zweiter Spieltest, Doku und Review** | Abnahme aus Abschnitt 6 |

## 9. Umsetzungsstand

| Schritt | Stand |
|---|---|
| – | Entwurf; Rückmeldungen 1–3 sofort behoben (18.11.2026) |
