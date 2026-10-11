# Wirtschaft und Staatshaushalt (Meilenstein M5)

> **Status: verbindlich** für Kern (`core/src/economy.cpp`), Daten und Tests, soweit umgesetzt: Datenmodell
> (Abschnitt 2), Märkte, Ortsrechnung, Staatshaushalt, Ernte und Einschwingen (Abschnitte 4.1–4.8) und
> Balancing-Bericht (7); offen sind Brücke und Oberfläche (Schritt 6 von M5,
> [meilensteine.md](../meilensteine.md#m5--wirtschaft-und-staatshaushalt)). Das Dokument entstand als
> Entwurf und wurde vor der Umsetzung freigegeben; Abweichungen der Umsetzung sind eingearbeitet
> (u. a. Handel bis zur ganzen Menge, Überschuss nicht in Grundnahrung, Hunger quadratisch, Bauten nur
> ohne Schulden, Kalibrierung in 4.9).

Das Design dahinter steht in [08 Wirtschaft und Finanzen](../08_wirtschaft_und_finanzen.md)
(Güter, Märkte, Staatsfinanzen) und [07 Bevölkerung und Gesellschaft](../07_bevoelkerung_und_gesellschaft.md),
Abschnitt 3 (Lebensstandard und Bedürfnisse).

## 1. Ziel und Abgrenzung

Bis M4 entsteht Wohlstand aus einem festen Ertrag je Beruf, und jeder Staat kassiert einen
einzigen Steuersatz. Die Kassen wachsen ohne Grenze: Im Referenzlauf 1648–1748 hat jeder Staat am
Ende 20- bis 1.000-mal so viel im Schatz wie am Anfang (Brandenburg 8,7 Mio. Reichstaler bei
27.000 Einnahmen im Monat). M5 ersetzt das durch einen Wirtschaftskreislauf:

```
Berufe arbeiten in Betrieben ──► Güter ──► Markt (Preis aus Angebot und Nachfrage) ◄──► Nachbarmärkte, Weltmarkt
        ▲                                         │
        │ Einkommen (Erlös − Vorleistungen − Abgaben − Steuern)
        │                                         ▼
  Lebensstandard ◄── Realeinkommen ◄── Verbrauch nach Warenkorb (Bedürfnisse) ──► Nachfrage
        │
  Staat: Kontribution, Akzise, Zölle ──► Hof, Verwaltung, Heer, Bauten, Zinsen ──► Nachfrage
```

| Thema aus Kapitel 07/08 | in M5 | später |
|---|---|---|
| Güter | ✔ 11 Güter der Epoche 1648–1806 (Abschnitt 2.1) | rund 75 Güter nach Epochen; Substitute |
| Gebäude und Produktionsmethoden | ✔ **Betriebe** als Arbeitsplätze eines Berufs im Ort, Anteile passen sich dem Gewinn an (Abschnitt 2.3) | Gebäude mit Stufen, Kapital und Bau; Produktionsmethoden (Industrialisierung) |
| Märkte und Handel | ✔ ein **Markt je Landschaft** (77), Handel mit Nachbarmärkten und über Häfen mit dem Weltmarkt | Handelsräume zusammenlegen, Verkehr (Chausseen, Kanäle, Eisenbahn) |
| Zölle | ✔ Zoll als Einnahme und Preisaufschlag auf eingeführte Waren | Schutzwirkung, Binnen- und Flusszölle, Zollverein (M9) |
| Bedürfnisse | ✔ Warenkorb je Schicht mit 6 Bedürfnissen, Bedarfsdeckung | Ersatzgüter, Bedürfnisse späterer Epochen |
| Lebensstandard | ✔ aus dem **Realeinkommen** (Einkommen ÷ Preisniveau), Hunger bei fehlender Grundnahrung | relative Deprivation (M8) |
| Staatseinnahmen | ✔ Kontribution, Akzise, Zölle | Domänen und Regalien, Subsidien, spätere Steuersysteme |
| Staatsausgaben | ✔ Hof, Verwaltung (je Einwohner), Heer (je Einwohner), Bauten, Zinsen | Ressorts, Budgetrecht (M8), echtes Heer (M10) |
| Staatsschulden | ✔ Kredit bei Defizit, Zins nach Verschuldung, Staatsbankrott mit Schuldenschnitt | Anleihen, Kriegsfinanzierung, Geld und Inflation |
| Konjunktur und Krisen | ✔ **Ernteschwankung** je Markt und Jahr – Teuerung und Hungerjahre entstehen daraus | Hungerkrisen als Ereignisse (M11), Konjunkturzyklen |

Nicht in M5: Geld und Währung (alle Beträge bleiben Reichstaler), Kapital und Investitionspool,
Unternehmen, Arbeitslosigkeit, Wirtschaftsordnungen, KI-Haushaltsführung (M12). Staaten ohne
Spieler führen ihren Haushalt nach festen Regeln (Abschnitt 4.6).

## 2. Datenmodell

### 2.1 Güter (`common/gueter.chron`, neu)

```text
gut getreide {
    name       = "Getreide"          # Pflicht
    farbe      = { 214 180 90 }      # Pflicht (Kartenmodus, Diagramme)
    basispreis = 1                   # Pflicht: Reichstaler je Einheit, > 0
    weltmarkt  = 0.9                 # optional: Preis am Weltmarkt als Faktor auf den Basispreis
}
```

| Gut | Einheit (Anzeige) | Basispreis (Rt) | erzeugt von (Abschnitt 2.3) | gebraucht für | Weltmarkt |
|---|---|---|---|---|---|
| `getreide` | Scheffel | 1 | Bauernhof, Gutsarbeit | Grundnahrung, Brauerei | 0,9 |
| `vieh` (Fleisch, Milch, Leder) | Zentner | 4 | Bauernhof, Schäferei | Grundnahrung | – |
| `salz` | Tonne | 4 | Saline (nur mit Vorkommen) | Grundnahrung | 1,2 |
| `holz` | Klafter | 2 | Forst, Bauernhof | Wärme, Werkstatt, Saline, Bauten | 1,0 |
| `faser` (Flachs, Wolle) | Stein | 2 | Schäferei, Bauernhof | Weberei, Heimweberei, Manufaktur | – |
| `tuch` | Stück | 5 | Weberei, Heimweberei | Kleidung | 1,1 |
| `bier` (Bier, Branntwein) | Tonne | 2 | Brauerei | Genuss | – |
| `wein` | Eimer | 6 | Weinberg (nur mit Vorkommen) | Genuss, Luxus | 1,3 |
| `waren` (Werkzeug, Hausrat) | Kiste | 3 | Werkstatt | Haushalt, Bauten | – |
| `luxuswaren` (Porzellan, Seide, Bücher) | Kiste | 10 | Manufaktur | Luxus | 1,2 |
| `kolonialwaren` (Zucker, Tabak, Kaffee, Gewürze) | Pfund | 8 | – (nur Weltmarkt) | Luxus | 1,0 |

Eisen, Kohle und Bergbau fehlen bewusst: Sie werden erst mit der Industrialisierung und dem Heer
(M10) wichtig und brauchen Rohstoffdaten für den ganzen Kernraum.

### 2.2 Bedürfnisse und Warenkörbe (`common/beduerfnisse.chron`, neu)

Sechs Bedürfnisse aus Kapitel 07, Abschnitt 3, in **fester Reihenfolge** (Dateireihenfolge = Rang
beim Einkauf). Jedes Bedürfnis hat je Schicht einen Warenkorb: Menge je Kopf und Jahr.

```text
beduerfnis grundnahrung {
    name   = "Grundnahrung"
    hunger = ja                                  # optional: fehlt es, steigt die Sterblichkeit (4.4)
    unten  = { getreide = 3.4 vieh = 0.15 salz = 0.04 }
    mittel = { getreide = 3.8 vieh = 0.3 salz = 0.06 }
    oben   = { getreide = 5 vieh = 1 salz = 0.1 }
}
```

Werte des Warenkorbs, umgerechnet in Reichstaler zum Basispreis je Kopf und Jahr (kalibriert,
Abschnitt 4.9):

| Bedürfnis | unten | mittel | oben |
|---|---|---|---|
| Grundnahrung (Getreide, Vieh, Salz) | 4,2 | 5,2 | 9,4 |
| Wärme (Holz) | 0,6 | 1,2 | 2,5 |
| Kleidung (Tuch) | 1,0 | 2,5 | 8 |
| Genuss (Bier, Wein) | 0,8 | 2,4 | 4,5 |
| Haushalt (Waren) | 0,6 | 2 | 5 |
| Luxus (Luxuswaren, Kolonialwaren, Wein) | 0,2 | 2,8 | 28 |
| **Summe** | **7,4** | **16,1** | **57,4** |

Die Schicht eines Berufs steht schon in `berufe.chron` (`schicht = unten | mittel | oben`). Der
Staat kauft mit denselben Körben ein (Abschnitt 4.6): Hof wie „oben“, Verwaltung wie „mittel“,
Heer wie „unten“.

### 2.3 Betriebe (`common/betriebe.chron`, neu)

Ein Betrieb ist die **Arbeitsstätte eines Berufs**: Er legt fest, welche Güter die Menschen dieses
Berufs im Ort herstellen und was sie dafür verbrauchen. Jeder Ort hat je Beruf eine Verteilung der
Arbeitskraft auf die dort möglichen Betriebe (**Betriebsanteile**, Summe 1); sie verschiebt sich
langsam zu den gewinnträchtigeren Betrieben (Abschnitt 4.5). Eigene Gebäude mit Stufen, Bau und
Kapital kommen erst, wenn Produktionsmethoden wechseln (Industrialisierung).

```text
betrieb weberei {
    name       = "Weberei"            # Pflicht
    beruf      = handwerker           # Pflicht
    erzeugt    = { tuch = 2.8 }       # Pflicht (außer handel = ja): Menge je Kopf und Jahr
    verbraucht = { faser = 2 }        # optional: Vorleistungen je Kopf und Jahr
    agrar      = nein                 # Pflicht: Ertrag hängt an Tragfähigkeit und Ernte (4.2)
    ort        = alle                 # optional: alle | stadt | land
    vorkommen  = salz                 # optional: nur in Orten mit diesem Vorkommen
    handel     = nein                 # optional: Einkommen aus der Handelsspanne statt aus Gütern
    gewicht    = 1                    # Pflicht: Anteil beim Start (relativ zu den anderen Betrieben des Berufs)
    gelaende   = { wald = 4 }         # optional: Faktor auf das Gewicht je Geländetyp (Standard 1)
}
```

Betriebe, Werte in Reichstalern zum Basispreis je Kopf und Jahr (Kopf = alle Angehörigen, wie bisher
beim Ertrag; kalibriert, Abschnitt 4.9):

| Betrieb | Beruf | erzeugt | verbraucht | Wertschöpfung | Bedingung, Gewicht |
|---|---|---|---|---|---|
| Bauernhof | Bauern | Getreide 12, Vieh 3,2, Faser 1, Holz 0,8 | – | 17 | agrar; 1 |
| Schäferei | Bauern | Faser 7, Vieh 6 | – | 13 | agrar; 0,1, Heide × 3, Hügelland und Mittelgebirge × 2 |
| Weinberg | Bauern | Wein 15 | – | 15 | agrar; Vorkommen `weinlage`; 1 |
| Gutsarbeit | Landarme | Getreide 7,5, Vieh 1 | – | 8,5 | agrar; 1 |
| Forst | Landarme | Holz 6,5 | – | 6,5 | 0,6, Wald × 4, Mittelgebirge × 3, Heide/Bruch/Seenplatte × 2 |
| Heimweberei (Verlag) | Landarme | Tuch 7,5 | Faser 2 | 5,5 | nur Land; 0,3 |
| Weberei | Handwerker | Tuch 14 | Faser 4 | 10 | 1 |
| Werkstatt | Handwerker | Waren 12 | Holz 2 | 10 | 0,8 |
| Saline | Handwerker | Salz 20 | Holz 5 | 15 | Vorkommen `salz`; 6 |
| Brauerei | Bürger | Bier 24 | Getreide 6 | 18 | 0,5 (Braurecht der Bürger) |
| Manufaktur | Bürger | Luxuswaren 40 | Faser 4, Holz 2 | 34 | nur Stadt; 0,15 |
| Handelshaus | Bürger | – | – | Handelsspanne | `handel = ja`; 1 |

Adel und Geistlichkeit arbeiten in keinem Betrieb. Sie leben von **Abgaben auf den Agrarertrag**
ihres Ortes (Grundrente, Zehnt; neues Berufsfeld `agrarabgabe`, Abschnitt 4.2).

### 2.4 Märkte (`common/maerkte.chron`, neu)

Ein **Markt je Landschaft** (Datei in `history/orte/`, 77 Stück). Das passt zur Größe der
„Handelsräume“ aus Kapitel 08 (Abschnitt 5.1) und braucht keine neue Zuordnung der 4.542 Orte.

```text
markt sachsen {
    name         = "Kursachsen (Leipzig)"   # Pflicht
    landschaften = { sachsen }               # Pflicht: Dateien in history/orte/ (ohne .chron)
}
```

Jede Landschaft gehört zu genau einem Markt (Ladefehler sonst). Mehrere Landschaften je Markt sind
erlaubt; damit lassen sich später Handelsräume bilden, ohne das Format zu ändern. In M5 ist jede
Landschaft ihr eigener Markt.

- **Lage** eines Marktes: Mittelpunkt seiner Orte mit Daten, nach Fläche gewichtet (fest; Orte ohne
  Daten ändern nichts an der Rechnung, wie seit M3 üblich).
- **Nachbarn:** die bis zu `handel_nachbarn` (6) nächsten Märkte mit Mittelpunkt im Umkreis von
  `handel_radius_km` (180 km). Abstand wie bei der Wanderung (Grad, Länge mit cos 51° gestaucht).
- **Hafen:** Ein Markt mit mindestens einem Hafenort (neues Ortsfeld `hafen = ja`) handelt mit dem
  **Weltmarkt** (feste Preise aus `gueter.chron`, unbegrenzte Menge).
- **Markt ohne Orte mit Daten:** nimmt nicht teil (keine Preise, kein Handel).

### 2.5 Neue und geänderte Felder

| Datei | Änderung |
|---|---|
| `common/gueter.chron`, `beduerfnisse.chron`, `betriebe.chron`, `maerkte.chron` | neu (Abschnitte 2.1–2.4) |
| `common/berufe.chron` | `ertrag` entfällt (kommt aus den Betrieben); neu `agrarabgabe` (0..1, Anteil am Agrarertrag des Ortes; Adel 0,08 Grundrente, Geistlichkeit 0,05 Zehnt) |
| `common/regeln.chron` | neue Pflichtfelder (Abschnitt 4.9); `verwuestung_ertrag` bleibt |
| `history/orte/*.chron` | optional, nicht datiert: `vorkommen = { salz weinlage }`, `hafen = ja` |
| `history/staaten/*.chron` | `steuersatz` wird zu `kontribution`; neu `akzise`, `zoll` (0..`steuersatz_max`), `heer` (Soldaten je 1.000 Einwohner), optional `schulden` (Reichstaler, Standard 0) |
| `tabellen/orte.csv` | Spalten `vorkommen` und `hafen` (bleiben bei `orte.py` erhalten, wie `kultur`) |
| `tabellen/staaten_historie.csv` | Felder `kontribution`, `akzise`, `zoll`, `heer`, `schulden` statt `steuersatz` |

Vorkommen und Häfen der 60 Orte mit Daten (Spielwerte mit Quelle in `orte.csv`):
**Salz** Halle (Saline der Halloren), Kolberg; **Weinlage** Dresden (Elbtal), Leitmeritz, Guben;
**Hafen** Stralsund, Rostock, Wismar, Greifswald, Kolberg, Stettin, Anklam, Cammin.

### 2.6 Zustand (neu)

| Daten | Inhalt | gelesen von |
|---|---|---|
| **Betriebsanteile** je Ort und Betrieb | Anteil der Arbeitskraft des Berufs (Summe je Beruf 1) | Ort selbst |
| **Wirtschaft des Ortes** (`LocationEconomy`, erweitert) | Erzeugung und Nachfrage je Gut (Monatsmengen), Einkommen je Beruf, Steuern je Art, Bedarfsdeckung je Schicht, Ausbau (Abschnitt 4.6) | Markt (Summen), Staat (Steuern), Anzeige |
| **Markt** | Preis, Kaufquote, Verkaufsquote, Einfuhranteil je Gut; Mengen des Vormonats (Angebot, Nachfrage, Ein- und Ausfuhr); Handelsspanne je Händler; Erntefaktor des Jahres | Orte (veröffentlicht am Monatsersten) |
| **Staat** | drei Steuersätze, Heeresstärke, Schatz, Schulden, Zinsaufschlag bis Datum, gleitende Jahreseinnahmen, Haushalt des Vormonats je Posten, Staatsnachfrage | Staat, Markt, Anzeige |

Alles davon gehört zum Zustand (Prüfsumme, später Spielstand). Abgeleitet und nicht im Zustand:
Markt je Ort, Orte je Markt, Nachbarn, Hafen, Mittelpunkt, Betriebe je Beruf und Ort (Liste der
erlaubten Betriebe).

## 3. Ablauf eines Tages

| Schritt | Was | Wann | parallel |
|---|---|---|---|
| 1 | Befehle (Steuersätze) | jeden Tag | – |
| 2 | Jahresabschluss; **Ernte**: Erntefaktor je Markt würfeln (4.7) | 1. Januar | – |
| 3 | Neuordnung der Blöcke | Monatserster | – |
| 4 | **Märkte**: Summen aus den Orten, Handel, Preise, Quoten veröffentlichen (4.1) | Monatserster | ✔ je Markt (Summen) |
| 5 | **Orte** mit heutigem Rechentag: **Wirtschaft** (4.2–4.5), dann Bevölkerung wie in M4 | Ort *i* am Tag `(i % 28) + 1` | ✔ |
| 6 | Zusammenführen (Momentaufnahmen, Wanderung) wie in M4 | nach Schritt 5 | ✔ (Buchen) |
| 7 | **Haushalt** der Staaten mit heutigem Rechentag (4.6) | Staat *j* am Tag `(j % 28) + 1` | – |
| 8 | Jahreswechsel: Seuchen, Abbau der Verwüstung | 1. Januar | – |
| 9 | Datum weiterschalten | jeden Tag | – |

Ein Ort sieht damit immer die Preise vom letzten Monatsersten; der Markt sieht die Mengen, die jeder
seiner Orte an seinem letzten Rechentag gemeldet hat (höchstens einen Monat alt). Diese
Verzögerung um einen Schritt macht die Rechnung parallel und unabhängig von der Thread-Zahl
(Abschnitt 5) und dämpft zugleich Schwingungen.

## 4. Formeln

Alle Rechnungen in `Fixed`, Raten sind Jahreswerte; „Monat“ heißt ÷ 12, einmal je Ort vorab
berechnet. Bezeichnungen: Gut *g*, Betrieb *b*, Beruf *p*, Schicht *s(p)*, Ort *o*, Markt *m*,
Basispreis *B_g*.

### 4.1 Markt am Monatsersten

1. **Lokale Mengen** (Monatsmengen aus den Ortsmeldungen): `A_g = Σ_o erzeugung(o, g)`,
   `N_g = Σ_o nachfrage(o, g)` + Staatsnachfrage der Staaten, deren Hauptstadt im Markt liegt.
2. **Handel** mit jedem Nachbarn *n* (Paare in fester Reihenfolge, nur aus den Preisen des
   Vormonats und den lokalen Mengen, daher unabhängig von der Reihenfolge):

   ```
   fracht   = handel_fracht × abstand_km / 100            (z. B. 0,12 je 100 km)
   lücke    = P_n,g − P_m,g × (1 + fracht)                 (Ware fließt von m nach n, wenn > 0)
   menge    = min(1, handel_rate × lücke / P_n,g) × min(Angebot_m,g, Nachfrage_n,g)   (Vormonat, mit Handel)
   ```

   Bei `handel_rate` 5 fließt ab 20 % Preislücke die ganze mögliche Menge; so kann ein Markt ohne
   eigene Erzeugung (Salz, Kolonialwaren) seinen Bedarf decken. Mengen des **Vormonats** erlauben auch
   die Weitergabe eingeführter Ware an den nächsten Markt. Jeder Markt gibt höchstens 4/5 seines
   Angebots des Vormonats ab und nimmt höchstens 4/5 seiner Nachfrage auf; darüber werden alle seine
   Ströme anteilig gekürzt (Ausfuhr und Einfuhr eines Paars bleiben bitgenau gleich).
   Mit dem Weltmarkt (nur Hafenmärkte) gilt dieselbe Formel mit `fracht = seefracht`, Weltpreis
   `W_g × B_g` und unbegrenzter Menge auf der Weltseite (statt `min(…)` das eigene Angebot bzw. die
   eigene Nachfrage). Ausfuhr zählt zur Nachfrage des Herkunftsmarktes, Einfuhr zum Angebot des
   Zielmarktes: `A'_g = A_g + Einfuhr_g`, `N'_g = N_g + Ausfuhr_g`.
3. **Preis** (gedämpft, begrenzt):

   ```
   ziel_g  = B_g × clamp(N'_g / A'_g, preis_min, preis_max)        (0,25 … 4; A' = 0 → preis_max)
   P_g    ← P_g + (ziel_g − P_g) × preis_anpassung                  (0,3 je Monat)
   ```
4. **Quoten:** Kaufquote `k_g = min(1, A'_g / N'_g)`, Verkaufsquote `v_g = min(1, N'_g / A'_g)`,
   Einfuhranteil `e_g = Einfuhr_g / A'_g`.
5. **Handelsspanne:** `spanne_je_kopf = handelsspanne × Σ_g P_g × min(A'_g, N'_g) / Händler`, wobei
   Händler die Köpfe aller Handelshäuser im Markt sind (Vormonat).

### 4.2 Erzeugung und Einkommen im Ort (am Rechentag)

Für jeden Beruf *p* mit `n_p` Köpfen und jeden erlaubten Betrieb *b* mit Anteil `a_b`:

```
faktor_b   = gelaende.ertrag × (1 − verwuestung × verwuestung_ertrag)
             × (agrar ? min(1, tragfähigkeit / n) × ernte_m : 1)                (Tragfähigkeit wie M4)
versorgt_b = min über Vorleistungen g von k_g  (1 ohne Vorleistungen)
menge_b,g  = n_p × a_b × erzeugt_b,g / 12 × faktor_b × versorgt_b               (Erzeugung, Meldung an den Markt)
erlös_b    = Σ_g menge_b,g × P_g × (1 − handelsspanne)
kosten_b   = Σ_g n_p × a_b × verbraucht_b,g / 12 × faktor_b × versorgt_b × P_g   (Vorleistungen, Nachfrage)
gewinn_b   = (erlös_b − kosten_b) / (n_p × a_b)                                  (je Kopf, für 4.5)
```

Handelshäuser erhalten statt eines Erlöses `spanne_je_kopf` aus Abschnitt 4.1. Der Preis räumt den
Markt; die Verkaufsquote mindert den Erlös nicht noch einmal (sie dient der Anzeige).

**Abgaben auf den Agrarertrag:** `abgaben = Σ agrarabgabe` der Berufe mit Köpfen im Ort. Jeder
Agrarbetrieb gibt `erlös_b × abgaben` ab; die Summe wird nach den Anteilen `agrarabgabe ÷ abgaben`
auf die Empfänger verteilt. Ohne Adel im Ort bleibt die Grundrente bei den Bauern (ebenso der Zehnt
ohne Geistlichkeit).

**Kontribution** (nur steuerpflichtige Berufe): in Orten ohne Stadtrecht auf das ganze Einkommen, in
Orten mit Stadtrecht nur auf das Einkommen aus Agrarbetrieben (das Amt um die Stadt; das städtische
Gewerbe trägt die Akzise): `kontribution = grundlage × satz_kontribution`.

Ergebnis: Netto-Einkommen je Kopf und Monat `y_p`, Steuern des Ortes je Art.

### 4.3 Verbrauch

**Verbraucherpreis** im Ort: `Q_g = P_g × (1 + akzise_satz) × (1 + zoll_satz × e_g)`; Akzise nur in
Orten mit Stadtrecht, Zoll in allen Orten (Satz des Besitzers). Die Aufschläge sind Steuern des
Ortes: `akzise = Σ Ausgaben × akzise_satz / (1 + akzise_satz)`, entsprechend der Zoll.

**Einkauf** je Beruf (Warenkorb der Schicht, Bedürfnisse in Rangfolge):

```
kosten_n  = Σ_g korb_s,n,g × Q_g                       (je Kopf und Monat)
voll      = Σ_n kosten_n
wenn y ≥ voll: Bedürfnisse mit hunger = ja zu 1, die übrigen zu (y − grund) / (voll − grund)
              (grund = Kosten der Grundnahrung: mehr Einkommen geht in Kleidung, Genuss, Luxus)
sonst:     Bedürfnisse der Reihe nach; f_n = min(1, rest / kosten_n), rest −= f_n × kosten_n
nachfrage_g += n_p × korb_s,n,g × f_n / 12             (Meldung an den Markt)
deckung_n  = min(1, f_n) × Σ_g (w_g × k_g) / Σ_g w_g    (w_g = korb × Basispreis; knappe Ware ist nicht zu haben)
```

### 4.4 Lebensstandard und Hunger

Der Lebensstandard folgt wie in M4 dem Einkommen, jetzt aber dem **Realeinkommen**: Netto-Einkommen
geteilt durch das Preisniveau des eigenen Warenkorbs.

```
preisniveau_s = Σ_g korb_s,g × Q_g / Σ_g korb_s,g × B_g       (1 zu Basispreisen ohne Steuern)
real_p        = 12 × y_p / preisniveau_s
ziel_p        = 100 × real_p / (real_p + lebensstandard_bezug)  (Formel aus M4, Abschnitt 4.2)
```

Damit bleibt die Kalibrierung aus M4 erhalten, solange Preise nahe am Basispreis liegen; Teuerung
senkt den Lebensstandard und damit Geburten und Lebenserwartung.

**Hunger:** Ist die Grundnahrung nicht gedeckt (`deckung < 1`, Bedürfnis mit `hunger = ja`), steigt
die Sterblichkeit aller Altersgruppen des Berufs: Faktor `1 + sterblichkeit_hunger × (1 − deckung)²`
(bei 1,5: 10 % Lücke → 1,5 % mehr Sterbefälle, halbe Deckung → 37,5 %). Quadratisch, damit ein wenig
fehlendes Salz oder Fleisch kaum zählt, echte Not aber stark; so entstehen Hungerjahre aus Missernten.

### 4.5 Betriebsanteile

Monatlich je Ort und Beruf, nach der Einkommensrechnung (Replikator, langsam):

```
ø        = Σ_b a_b × gewinn_b
a_b     ← a_b × (1 + betrieb_anpassung × (gewinn_b − ø) / ø)      (0,03 je Monat; ø ≤ 0 → unverändert)
a_b     ← max(a_b, betrieb_min_anteil)                              (0,02; kein Betrieb stirbt ganz aus)
a       ← a / Σ a                                                   (Summe wieder 1)
```

Ist Tuch knapp und teuer, wechseln Handwerker in die Weberei, das Angebot steigt, der Preis fällt.

### 4.6 Staatshaushalt (am Rechentag des Staates)

**Einnahmen:** Summe der Steuern seiner Orte vom jeweils letzten Rechentag (Kontribution, Akzise,
Zoll). Gleitende Jahreseinnahmen: `J ← J + (12 × einnahmen − J) / 12`.

**Ausgaben je Monat:**

| Posten | Formel |
|---|---|
| Hof | `hofhaltung / 12` (Daten, fest) |
| Verwaltung | `verwaltung_je_einwohner × Einwohner / 12` |
| Heer | `heer / 1000 × Einwohner × sold / 12` (Soldaten zählen nur als Kosten; das Heer kommt mit M10) |
| Zinsen | `schulden × zins / 12`, `zins = min(zins_max, zins_basis + zins_risiko × max(0, schulden / J − 1) + aufschlag)` |
| Bauten | nur ohne Schulden, wenn `verfügbar > reserve`: `(verfügbar − reserve) × bau_anteil` (0,05 je Monat); `verfügbar = schatz + einnahmen − Hof − Verwaltung − Heer − Zinsen`, `reserve = reserve_jahre × (Hof + Verwaltung + Heer) × 12` |

Einwohner sind die Menschen in den Orten des Staates am Rechentag (Blöcke und Zugänge). Die Posten
eines Monats stehen im Haushalt des Staates (`StateBudget`: Steuern je Art, Hof, Verwaltung, Heer,
Zinsen, Bauten) und gehen in die Prüfsumme ein.

**Kredit:** Reicht der Schatz nicht, leiht der Staat den Fehlbetrag (`schulden += fehlbetrag`, Schatz
0). Überschüsse tilgen zuerst die Schulden, dann füllen sie den Schatz. Solange Schulden bestehen,
wird nicht gebaut.

**Staatsbankrott:** Überschreiten die Schulden `bankrott_quote × J` (4 Jahreseinnahmen), werden sie
um `bankrott_schnitt` (50 %) gekürzt; der Zins steigt `bankrott_jahre` (20) lang um
`bankrott_aufschlag` (0,04). Eintrag in der Chronik („Sachsen-Weimar erklärt den Staatsbankrott: Die
Gläubiger verlieren 50 % ihrer Forderungen.“). Geprüft wird nach der Buchung des Monats; die
Jahreseinnahmen sind dabei schon fortgeschrieben.

**Landesausbau:** Die Bauten verbessern die Orte des Staates (nach Fläche verteilt):
`ausbau_o += bauten × fläche_o / Σ fläche`; die Tragfähigkeit steigt um
`ausbau_max × A / (A + ausbau_halb)` mit `A = ausbau_o / fläche_o` (Reichstaler je km², sättigend,
höchstens `ausbau_max` = 30 %). Vorbild: Meliorationen und Kolonisten der Peuplierungspolitik.

**Staatsnachfrage:** Hof, Verwaltung und Heer kaufen mit ihrem Betrag nach den Warenkörben „oben“,
„mittel“ und „unten“, die Bauten wie die Verwaltung (Korb „mittel“; im Entwurf Holz und Waren je zur
Hälfte – ein eigener Korb lohnt erst mit Baustoffen), alles im Markt der Hauptstadt und mit dem
Haushalt des Vormonats (`state_spending`, am Monatsersten vor den Preisen). Steuern fließen so in die
Wirtschaft zurück; ohne diesen Kreislauf fielen alle Preise.

Staaten ohne Spieler ändern ihre Steuersätze in M5 nicht. Der Spieler stellt die drei Sätze ein
(Befehl `SetTaxRate` mit Steuerart); Heer und Bauten folgen den Regeln.

### 4.7 Ernte

Am 1. Januar zieht jeder Markt mit Orten mit Daten aus dem Strom **Wirtschaft** (bisher ungenutzt)
in Marktreihenfolge: `ernte_m = 1 + ernte_land + ernte_markt` mit `ernte_land` einmal je Jahr
gleichverteilt in ±`ernte_schwankung_land` (0,08) und `ernte_markt` je Markt in
±`ernte_schwankung_markt` (0,10). Ein schlechtes Jahr trifft also alle, ein örtliches Unwetter einen
Markt. Der Faktor wirkt auf alle Agrarbetriebe des Marktes (Abschnitt 4.2).

### 4.8 Einschwingen beim Start

Betriebsanteile aus den Gewichten passen nicht genau zur Nachfrage. Damit das Spiel nicht mit
einem Preissprung beginnt, rechnet der Lader beim Start (`initialize_population`) `einschwingen_monate`
(60) Monate **nur Wirtschaft** für alle Orte mit Daten: Märkte, Erzeugung, Verbrauch, Betriebsanteile –
ohne Bevölkerung, ohne Buchung im Staatshaushalt, mit Erntefaktor 1. Die Staaten kaufen dabei mit
geschätzten Ausgaben ein (`estimate_budgets`: Hof, Verwaltung und Heer aus der Bevölkerung). Danach
werden Lebensstandard und Alphabetisierung der Blöcke wie in M4 auf ihre Zielwerte gesetzt und die
Momentaufnahmen veröffentlicht. Das ist deterministisch und Teil des Startzustands. Für die 60 Orte
dauert es wenige Millisekunden, für die Lasttest-Welt (4.542 Orte) etwa 1 s. Eine von Hand gebaute
Welt ohne diesen Zustand (Tests) rechnet beim Start der Simulation einmal die Wirtschaft jedes Ortes.

### 4.9 Regelwerte und Kalibrierung

Neue Pflichtfelder in `regeln.chron` (vorläufige Werte):

| Feld | Wert | Bedeutung |
|---|---|---|
| `preis_min`, `preis_max` | 0,25, 4 | Grenzen des Preises als Faktor auf den Basispreis |
| `preis_anpassung` | 0,3 | Anteil der Lücke zum Zielpreis je Monat |
| `handel_nachbarn`, `handel_radius_km` | 6, 180 | Nachbarmärkte |
| `handel_rate` | 5 | Handelsmenge je Preislücke (0..100): ab 20 % Lücke die ganze mögliche Menge |
| `handel_fracht`, `seefracht` | 0,12, 0,10 | Frachtkosten je 100 km bzw. zum Weltmarkt (Anteil am Preis) |
| `handelsspanne` | 0,05 | Anteil der Händler am Umsatz |
| `betrieb_anpassung`, `betrieb_min_anteil` | 0,03, 0,02 | Anpassung der Betriebsanteile |
| `ernte_schwankung_land`, `ernte_schwankung_markt` | 0,08, 0,10 | Erntefaktor |
| `sterblichkeit_hunger` | 1,5 | Zuschlag auf die Sterblichkeit bei fehlender Grundnahrung |
| `einschwingen_monate` | 60 | Abschnitt 4.8 |
| `verwaltung_je_einwohner`, `sold` | 0,15, 30 | Reichstaler je Einwohner bzw. Soldat und Jahr |
| `reserve_jahre`, `bau_anteil` | 0,5, 0,05 | Rücklage; Anteil des Überschusses, der monatlich verbaut wird |
| `ausbau_max`, `ausbau_halb` | 0,3, 200 | Landesausbau |
| `zins_basis`, `zins_risiko`, `zins_max` | 0,05, 0,02, 0,2 | Zins |
| `bankrott_quote`, `bankrott_schnitt`, `bankrott_aufschlag`, `bankrott_jahre` | 4, 0,5, 0,04, 20 | Staatsbankrott |
| `steuersatz_max` | 0,5 | gilt für alle drei Steuerarten |

**Kalibrierung** (Schritte 2 bis 4), mit den 60 Orten der Testregion und dem Prüfbefehl
`uhrwerk-cli wirtschaft` (Angebot und Nachfrage je Gut, Einkommen, Lebensstandard und Bedarfsdeckung
je Beruf, Haushalt je Staat; mit `--jahre N` nach N Jahren, mit `--maerkte` Preise je Markt):

1. **Gleichgewicht nach dem Einschwingen:** Nachfrage ÷ Angebot je heimischem Gut 0,7–1,4. Erreicht
   mit drei Eingriffen: Die Berufsanteile der Orte mit Stadtrecht (Stadt mit Amt) aus M1 waren für einen
   Markt zu städtisch – jetzt Bürger 8 % (vorher 15 %), Handwerker 16 % (30 %), Bauern 30 % (20 %),
   Landarme 44 % (33 %); Handwerksbetriebe erzeugen etwas weniger; die Grundnahrung der Unterschicht
   ist kleiner (3,4 statt 4,2 Scheffel Getreide je Kopf). Mit dem Balancing-Bericht (Schritt 4) kam
   ein vierter hinzu: Nur Halle und Kolberg sieden Salz, und mit 5 Tonnen je Kopf deckten sie 61 % der
   Nachfrage – Salz kostete im Binnenland bis zum Vierfachen. Die Saline erzeugt jetzt 9 Tonnen je Kopf
   (Holz 4,5 statt 2,5 Klafter); Nachfrage ÷ Angebot 1,01.
2. **Lebensstandard wie in M4:** Die Märkte ergeben niedrigere Einkommen als die festen Erträge aus
   M4 (Landarme 4, Bauern 7,5, Handwerker 7, Bürger 18, Geistlichkeit 17, Adel 63 Reichstaler je Kopf
   und Jahr nach dem Einschwingen). Mit `lebensstandard_bezug` 9 statt 12 liegt der Lebensstandard der
   Schichten wieder bei den Werten aus M4 (Landarme 29, Bauern 44, Adel 83).
3. **Bevölkerung:** `fruchtbarkeit` 0,093 statt 0,088 gleicht die zusätzliche Sterblichkeit durch
   Hunger aus: 1648 → 1700 ×1,33 (Ziel ×1,32), 1750 ×1,73, 1816 ×2,23 (Ziel ×2,27); Geburten 37–39 ‰,
   Sterbefälle 32–33 ‰ ([bevoelkerung.md, Abschnitt 4.9](bevoelkerung.md#49-regelwerte-startwerte-und-kalibrierung)).
   Nach 50 Jahren liegen die Preise je Markt bei 0,6–1,4 des Basispreises, Salz im Binnenland bei bis zu
   2,7, Kolonialwaren je nach Entfernung zum Hafen bei 1,9–3,7.
4. **Staatshaushalt** (erster Durchgang in Schritt 3): Die Steuern nach dem Einschwingen liegen bei
   etwa 0,8 Reichstalern je Einwohner und Jahr, rund 60 % der Einnahmen aus M4 – die Steuer trifft
   jetzt die Wertschöpfung nach Handelsspanne und Agrarabgabe, nicht mehr den festen Ertrag. Die
   Steuersätze bleiben wie in M4; stattdessen sind die Ausgaben neu bemessen: `verwaltung_je_einwohner`
   0,15 statt 0,3 (Ämter waren billig; Hof und Heer trugen die Last) und die **Hofhaltung** je Staat
   so, dass Hof + Verwaltung + Heer die monatlichen Steuern nach dem Einschwingen um etwa 10 % übersteigen
   (Nachkriegsdefizit wie in M1, Abschnitt „Wirtschaftswerte“ in
   [data/README.md](../../data/README.md#wirtschaftswerte)); drei Durchgänge, weil die geringere
   Staatsnachfrage die Preise und damit die Steuern senkt. Ergebnis mit Seed 1648: kein Staatsbankrott
   in 100 Jahren; die Staaten zehren in den ersten Jahren vom Schatz, Sachsen-Weimar leiht kurz; nach
   100 Jahren hält jeder Staat ohne Schulden etwa eine Jahreseinnahme im Schatz, der Rest geht in den
   Landesausbau. Bevölkerung 1648 → 1700 ×1,34, nach 100 Jahren ×1,84 (Landesausbau erhöht die
   Tragfähigkeit).
5. **Langlauf:** Der Balancing-Bericht (Abschnitt 7) erfüllt über 32 Seeds und 100 Jahre alle
   Zielwerte; Ergebnis dort.

## 5. Mehrkernberechnung und Determinismus

Es gilt die Regel aus [bevoelkerung.md, Abschnitt 5](bevoelkerung.md#5-mehrkernberechnung): Jede
parallele Aufgabe schreibt nur Daten ihres Ortes bzw. Marktes und liest fremde Daten nur aus dem,
was vorher nacheinander veröffentlicht wurde.

- **Orte** (Schritt 5) lesen den veröffentlichten Zustand ihres Marktes und den Steuersatz ihres
  Besitzers, schreiben ihre Wirtschaft, Betriebsanteile und Blöcke.
- **Märkte** (Schritt 4): Summen je Markt parallel (jeder Markt liest nur die Meldungen seiner Orte);
  Handel, Preise und Quoten danach nacheinander in Marktreihenfolge (77 Märkte × 11 Güter, billig).
- **Staaten** (Schritt 7): nacheinander wie bisher.
- Zufall nur am Jahreswechsel im Strom Wirtschaft, in fester Reihenfolge.

Prüfung wie in M4: gleiche Prüfsummen bei 1–8 Threads (Unit-Test, `lasttest --vergleich`) und unter
GCC, Clang, MSVC.

## 6. Leistung

Die Wirtschaft rechnet je **Ort und Beruf**, nicht je Block: Alle Blöcke eines Berufs im Ort haben
dasselbe Einkommen und denselben Ziel-Lebensstandard (wie schon in M4). Je Ort und Monat sind das
6 Berufe × (bis zu 4 Betriebe × 4 Güter + 6 Bedürfnisse × 3 Güter) ≈ 200 Festkomma-Operationen,
also für 4.542 Orte unter 0,1 s je Spieljahr mit einem Thread. Märkte und Staaten fallen nicht ins
Gewicht. Ziel: **Lasttest mit 500.000 Blöcken bleibt unter 2 s je Spieljahr** (M4: 0,33–0,40 s
mit 4 Kernen), erwartet höchstens +30 %.

**Messung** (Schritt 5, Release-Build, 4 Kerne, alle 4.542 Orte mit Daten und alle 77 Märkte aktiv):
Auf derselben Maschine braucht M4 0,29–0,30 s je Spieljahr, M5 0,56–0,64 s (Median 0,58 s; mit einem
Thread 1,26 s) – **erfüllt**, aber etwa doppelt so viel wie geschätzt. Callgrind zeigt, warum: Die
Ortsrechnung kostet rund 34.000 Instruktionen je Ort und Monat statt der geschätzten 200
Festkomma-Operationen, weil jede Multiplikation und Division 128 Bit breit rechnet und der Einkauf die
Warenkörbe je Beruf mehrfach durchläuft; sie macht etwa 40 % eines Spieljahres aus, verteilt über viele
Zeilen ohne einzelnen Engpass. Das Einschwingen beim Aufbau (60 Monate für 4.542 Orte, ein Thread)
dauert 1,0 s statt 0,15 s; mit den 60 Orten der Testregion sind es wenige Millisekunden. Optimierungen
(Warenkorb je Schicht statt je Beruf, zulässige Betriebe je Ort vorberechnen, Einschwingen parallel)
lohnen sich erst, wenn die Grenze näher rückt.

Determinismus: gleiche Prüfsummen mit GCC und Clang bei 1–4 Threads (100 Jahre `5db13a66d63e6079`,
Lasttest 50.000 Blöcke/2 Jahre `6545173879bcb4f9`), MSVC im CI-Job „Determinismus“; Unit-Tests,
Lasttest und Bilanz laufen ohne Befund unter ThreadSanitizer sowie AddressSanitizer mit
UndefinedBehaviorSanitizer.

## 7. Balancing-Bericht

```
uhrwerk-cli bilanz [--seeds N] [--jahre J] [--seed S] [--threads T] [--ziele DATEI]
```

Der Befehl rechnet N Läufe (Seeds S, S+1, …; Standard 32 Läufe ab 1648) über J Jahre (Standard 100)
ohne Eingriffe des Spielers und misst am Jahrestag des Starts je Staat das Vermögen, nach jedem
Monatsersten die Preise aller Märkte mit Orten, dazu Bevölkerung und Lebensstandard. Er meldet je Staat
und Kennzahl Median, 5- und 95-%-Wert über alle Läufe (nächster Rang, in Festkomma: auf jeder Plattform
gleich), markiert Verstöße mit `!` und listet sie am Ende; Exitcode 1 bei Verstoß. Die Zielwerte stehen
im Datenordner (`data/common/zielwerte.chron`, Format in
[chronik-skript.md, Abschnitt 3.15](chronik-skript.md#315-commonzielwertechron-seit-m5)); `pruefen`
prüft die Datei mit. Umsetzung: `core/include/uhrwerk/balance.hpp` (`record_run`, `summarize`) – kein
Teil der Simulation, ohne Einfluss auf Zustand und Prüfsumme.

| Kennzahl | Zielwert |
|---|---|
| Vermögen = (Schatz − Schulden) ÷ Jahreseinnahmen, jedes Staatsjahr, je Staat | −3 … +3 in mindestens 95 % der Staatsjahre; Median am Ende 0 … 1,5 |
| Staatsbankrotte je Staat und Jahrhundert | Median 0, höchstens 2 im 95-%-Wert |
| Steuerquote (Steuern ÷ Wirtschaftsleistung der Orte) am Ende gegen den Start, Median je Staat | 0,6 … 1,4 (Einnahmen wachsen mit der Wirtschaft, nicht darüber hinaus) |
| Preis je Gut und Markt (Monatswerte) | 0,4 … 2,5 × Basispreis in 95 % der Markt-Monate je Gut; Kolonialwaren 0,4 … 5 |
| Bevölkerung 1648 → 1700 (Median) | ×1,20 … ×1,45 (Kalibrierung aus M4) |
| Lebensstandard je Schicht (Median über alle Jahre und Läufe) | unten 25–55, mittel 45–75, oben 70–95 |

Abweichungen vom Entwurf: Die Steuerquote durfte ursprünglich nicht unter 0,7 fallen. Die Kontribution
trifft in Städten aber nur Agrareinkommen, und die Städte wachsen im Modell schneller als das Land
(Bürger nach 100 Jahren ×3, Landarme ×1,4); mit festen Sätzen sinkt die Quote daher auf 0,7–1,0, in
Sachsen-Gotha mit der wachsenden Residenz auf 0,69. Das ist das Steuersystem der Zeit, kein Fehler – der
Spieler kann mit der Akzise gegensteuern –, deshalb gilt 0,6. Kolonialwaren haben einen eigenen Bereich,
weil ihr Basispreis am Weltmarkt gilt und im Binnenland die Fracht hinzukommt.

**Ergebnis** mit `data/` (32 Läufe, 100 Jahre, Seeds 1648–1679; 11 s im Release-Build):

| Kennzahl | Ergebnis |
|---|---|
| Vermögen je Staatsjahr | Median 0,70–0,97, alle Staatsjahre im Bereich; tiefster Wert −1,77 (Anhalt-Dessau nach einer Seuche) |
| Vermögen am Ende | Median 0,85–1,12 Jahreseinnahmen: Überschüsse über der Rücklage gehen in den Landesausbau |
| Staatsbankrotte | keine in 512 Staatsjahrhunderten (16 Staaten × 32 Läufe) |
| Steuerquote Ende ÷ Start | Median 0,69 (Sachsen-Gotha) bis 0,99 (Böhmen) |
| Preise im Bereich | 96,5 % (Tuch) bis 100 %; Salz 98,7 %, höchstens 2,95 × Basispreis im Binnenland |
| Bevölkerung 1700 | ×1,36 (×1,34 … ×1,38) |
| Lebensstandard | unten 42, mittel 58, oben 78 |

**CI:** Job „Balancing (Linux, Release)“: `bilanz --seeds 16 --jahre 100`, Bericht in der
Zusammenfassung des Laufs, auch bei Verstoß. Im Kleinen prüfen zwei CTests den Befehl auf der
Test-Fixture (`bilanz_fixture` mit weiten Zielwerten, `bilanz_verstoss` mit den Zielwerten der
Spieldaten, die die Fixture verfehlen muss).

## 8. Abnahme

| Kriterium | Prüfung |
|---|---|
| Über 100 Spieljahre ohne Spielereingriff bleiben die Kassen in einer plausiblen Spanne; kein Staat wächst ins Unendliche | Balancing-Bericht mit den Zielwerten aus Abschnitt 7, in der CI |
| Balancing-Bericht über mehrere Seeds in der CI | Job „Balancing“ |
| Prüfsummen bitgleich unter GCC, Clang, MSVC und bei jeder Zahl von Threads | wie M4 (Determinismus-Job, Unit-Test, Lasttest-Vergleich) |
| Lasttest bleibt unter 2 s je Spieljahr | Job „Lasttest“ |
| Buchführung der Wirtschaft | Test: Ausfuhr eines Marktes = Einfuhr seiner Nachbarn (je Gut, bitgenau); Haushalt: Schatz und Schulden ändern sich genau um Einnahmen − Ausgaben |
| Formeln | Tests rechnen Markt, Ort und Haushalt für kleine Fixturen nach (wie M4) |
| Oberfläche | Haushaltsfenster, Kartenmodi Wirtschaft und Märkte; Godot-Tests in der CI |

## 9. Prüfsumme

Neu in der Prüfsumme: Betriebsanteile, Wirtschaft der Orte (Mengen, Einkommen, Steuern, Deckung,
Ausbau), Märkte (Preise, Quoten, Mengen, Erntefaktor), Staaten (drei Sätze, Heer, Schulden,
Jahreseinnahmen, Zinsaufschlag, Haushalt des Vormonats), Zufallsstrom Wirtschaft. Die
Referenzprüfsumme nach 100 Jahren ändert sich (M4: `ed6b14a02cfbfa5d`).

## 10. Brücke und Oberfläche

**Brücke** ([bridge-api.md](bridge-api.md)):
- `get_state`: Einnahmen je Steuerart, Ausgaben je Posten, Schatz, Schulden, Zins, drei Sätze, Heer,
  letzter Bankrott.
- `set_tax_rate_percent(art, prozent)` für den Spieler (Art 0 Kontribution, 1 Akzise, 2 Zoll).
- `get_location`: Markt, Betriebe mit Köpfen, Erzeugung je Gut, Einkommen je Beruf, Bedarfsdeckung
  je Schicht.
- `get_market_count`, `get_market(index)`: Name, Preise (absolut und zum Basispreis), Angebot,
  Nachfrage, Ein- und Ausfuhr je Gut, Erntefaktor.
- Kartenmodi `MAP_WIRTSCHAFT` (8, Wirtschaftsleistung je Einwohner) und `MAP_MAERKTE` (9, Farbe je
  Markt; Marktgrenzen gezeichnet).

**Godot:**
- **Haushaltsfenster** (aus dem Staatspanel): Einnahmen nach Art, Ausgaben nach Posten, Schatz,
  Schulden und Zins; drei Regler für Kontribution, Akzise und Zoll (ersetzen den einen
  Steuerregler).
- Ortspanel: Abschnitt **Wirtschaft** (Markt, Betriebe, wichtigste Güter mit Preis gegenüber dem
  Basispreis, Bedarfsdeckung).
- Zwei Kartenmodi mit Legende (Tasten O und P), damit zehn.

## 11. Umsetzungsschritte

Nacheinander, jeder Schritt mit Build unter GCC und Clang, Tests und clang-format, dann Commit:

| Schritt | Inhalt | Prüfstein |
|---|---|---|
| 1 | **Datenmodell:** Güter, Bedürfnisse, Betriebe, Märkte, neue Orts- und Staatsfelder, Regeln; Lader, Archivar, Tests, chronik-skript.md. Die Simulation rechnet noch wie in M4 (`kontribution` statt `steuersatz`) | `uhrwerk-cli pruefen` ohne Fehler; Prüfsumme unverändert |
| 2 | **Wirtschaft im Ort und Märkte:** 4.1–4.5, 4.7, 4.8; Steuern je Art im Ort; Prüfbefehl Gleichgewicht; Kalibrierung der Startwerte | Formeltests; Einkommen und Bevölkerung nach Abschnitt 4.9 |
| 3 | **Staatshaushalt:** 4.6 mit Kredit, Bankrott, Landesausbau, Staatsnachfrage; Befehl mit Steuerart | Buchführungstest Haushalt |
| 4 | **Balancing-Bericht** (`uhrwerk-cli bilanz`, Zielwerte, CI-Job); Kalibrierung über 100 Jahre und viele Seeds | Zielwerte erfüllt |
| 5 | **Leistung und Lasttest:** Lasttest-Welt mit Märkten, Messung, Determinismus | ≤ 2 s je Spieljahr; Threads und Compiler bitgleich |
| 6 | **Brücke und Godot:** Abschnitt 10 | Godot-Tests in der CI grün |
| 7 | **Doku und Review** | CI grün |

## 12. Entscheidungen (vor der Umsetzung zu bestätigen)

1. **Ein Markt je Landschaft** (77) über `common/maerkte.chron`; Handelsräume aus mehreren
   Landschaften (Leipzig, Frankfurt, Hamburg …) sind im Format möglich, werden aber erst später
   gebildet.
2. **11 Güter** ohne Eisen und Bergbau; fester Warenkorb je Schicht ohne Ersatzgüter.
3. **Betriebe als Arbeitsanteile** eines Berufs im Ort, die dem Gewinn folgen – keine Gebäude mit
   Bau und Kapital in M5.
4. **Lebensstandard aus dem Realeinkommen** (Formel aus M4, geteilt durch das Preisniveau) plus
   Hunger bei fehlender Grundnahrung; die Bedarfsdeckung je Bedürfnis dient der Anzeige.
5. **Drei Steuerarten** (Kontribution auf dem Land, Akzise in Städten, Zoll auf Einfuhren); der Zoll
   bremst den Handel in M5 noch nicht.
6. **Ausgaben nach Regeln:** Hof fest, Verwaltung und Heer je Einwohner, Überschüsse über der
   Rücklage werden verbaut (Landesausbau); Defizite automatisch auf Kredit, Bankrott bei
   4 Jahreseinnahmen Schulden. Der Spieler stellt in M5 nur die drei Steuersätze ein.
7. **Ernteschwankung** je Jahr (landesweit und je Markt) als erste Quelle von Teuerung und
   Hungerjahren.
8. **Einschwingen** über 60 Monate beim Start statt handverlesener Startwerte für jeden Betrieb.
