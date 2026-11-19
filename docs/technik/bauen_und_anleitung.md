# Bauen, Wirtschaft und Anleitung (Meilenstein M15)

> **Status: verbindlich** – Entscheidungen 1–7 wie empfohlen freigegeben (19.11.2026, Abschnitt 9);
> Umsetzung läuft, Stand in Abschnitt 11. Anlass ist die zweite
> Rückmeldung aus dem Spiel (18./19.11.2026). Grundlage sind [wirtschaft.md](wirtschaft.md) (M5, Betriebe,
> Haushalt, Landesausbau), [lesezeichen_1848.md](lesezeichen_1848.md) (Verwaltung, Eisenbahnen, Industrie
> 1848), [ki.md](ki.md) und der Vertrag von M14 ([europa_und_oberflaeche.md](europa_und_oberflaeche.md)).

## 1. Anlass

| Nr. | Rückmeldung | Stand |
|---|---|---|
| 1 | F11 für Vollbild | **erledigt** (19.11.2026): F11 schaltet überall um, belegbar, die Einstellung bleibt |
| 2 | Neue Spieler finden sich schlecht zurecht; bessere Anleitung, alles intuitiver und übersichtlicher | Teil durch M14a1–a3 (freie Karte, Symbole, Meldekarten); der Rest **dieser Vertrag**, Abschnitt 6 |
| 3 | Es gibt keine Gebäude, die man bauen kann und die die Wirtschaft ankurbeln; „noch relativ langweilig“ | **dieser Vertrag**, Abschnitte 3–5 |
| 4 | Beim Bauen bei so vielen Landkreisen nicht unübersichtlich werden | **dieser Vertrag**, Abschnitt 3.1 (Bauen je Landesteil, Vorschläge) |
| 5 | Man erkennt nicht, wie es der Bevölkerung geht; mehr Hinweise in den Tooltips | **erledigt** (19.11.2026): Urteil in Worten mit Farbe am Symbol, je Schicht, Gründe, was hilft; dabei fiel Abschnitt 7 auf |
| 6 | 1973 gibt es noch Preußen – sehr unhistorisch | **erledigt** (M15a1): Nachspiel bis Ende 1880, Abschnitt 8 |

## 2. Ausgangslage

- **Betriebe statt Gebäude** (M5, M7): Jeder Beruf arbeitet in den Betrieben seines Ortes (Bauernhof,
  Weberei, Bergwerk, Hütte, Maschinenbau …); die Anteile folgen langsam dem Gewinn. Es gibt keine
  Kapazität, kein Kapital und nichts zu bauen – Fabrikarbeiter entstehen aus der Wanderung.
- **Landesausbau** (M5, wirtschaft.md 4.6): Ein Staat ohne Schulden steckt jeden Monat 5 % seines
  Überschusses über der Rücklage in „Bauten“, die sich nach Fläche auf die Orte verteilen und die
  Tragfähigkeit bis +30 % heben. Der Spieler sieht davon nur den Posten „Bauten“ im Haushalt.
- **Eisenbahnen** (M7) öffnen zu ihren historischen Tagen, unabhängig vom Spieler.
- **Größe:** 1.447 Orte mit Daten in 626 Kreisen; Preußen allein 327 Kreise, aber 11 Provinzen, Österreich
  10 Kronländer, die Mittelstaaten 3 bis 8 Kreise oder Provinzen, die Kleinstaaten keine Einteilung.
- **Was der Spieler heute tut:** Steuern stellen, Gesetze, Diplomatie, Krieg, Ereignisse beantworten.
  Die Wirtschaft läuft ohne ihn; die einzigen Hebel sind drei Steuersätze.

## 3. Bauen

### 3.1 Bauen je Landesteil, nicht je Ort (Rückmeldung 4)

Gebaut wird in **Landesteilen**: der obersten Verwaltungseinheit eines Staates (Provinz, Kronland,
Regierungsbezirk, wo es keine Provinz gibt; Daten aus `history/verwaltung`), bei Staaten ohne
Einteilung im ganzen Staat. Preußen hat damit 11 Landesteile statt 327 Kreise, Bayern 8, Baden 4,
Sachsen-Weimar einen. Ein Gebäude steht im Landesteil; die Simulation verteilt seine Wirkung auf die
Orte, die dafür taugen (Vorkommen, Stadt oder Land, Gelände, Arbeitskräfte – wie heute die Betriebe).

Drei Wege, damit es übersichtlich bleibt:

- **Vorschläge zuerst:** Das Fenster „Bauen“ öffnet mit einer Liste der fünf besten Vorhaben des
  Staates, sortiert nach Nutzen je Taler: „Spinnerei in Schlesien – 12.000 Taler, 18 Monate, 900
  Arbeitsplätze, +1.100 Taler Steuern im Jahr, Lebensstandard der Weber +3“. Ein Klick gibt den Auftrag.
- **Landesteile auf der Karte:** Der Kartenmodus „Bauen“ färbt die Landesteile nach dem besten Vorhaben;
  ein Klick auf einen Landesteil zeigt seine Gebäude, laufenden Bauten und Vorschläge.
- **Bauamt:** Laufende Bauten in einer Liste mit Fortschritt; höchstens so viele zugleich, wie das Bauamt
  des Staates fasst (nach Einwohnern, mit Gesetzen und Eisenbahnen mehr).

### 3.2 Gebäude

Eigene Gebäude der Epoche, keine Kopie anderer Spiele. Erste Liste (Werte nach Kalibrierung):

| Gebäude | Wirkt auf | Bedingung | Wirkung |
|---|---|---|---|
| Melioration | Bauern, Landarme | Land | Tragfähigkeit und Ernte (ersetzt den automatischen Landesausbau) |
| Mustergut | Bauern | Land | Ertrag der Agrarbetriebe, Fortschritt der Landwirtschaft |
| Spinnerei | Fabrikarbeiter | Stadt | Arbeitsplätze, Faser → Tuch, Ertrag je Kopf höher als in der Weberei |
| Zeche | Fabrikarbeiter | Vorkommen Kohle | Arbeitsplätze, Kohle |
| Eisenhütte | Fabrikarbeiter | Vorkommen Erz oder Kohle | Arbeitsplätze, Eisen |
| Maschinenfabrik | Fabrikarbeiter | Stadt, Eisen im Markt | Arbeitsplätze, Waren; hebt die übrigen Betriebe |
| Chaussee | Handel | – | Kosten zwischen den Märkten des Landesteils sinken |
| Bahnstrecke | Handel, Heer | geplante Strecke | eröffnet eine Strecke aus `eisenbahnen` früher (oder eine eigene) |
| Volksschule | alle | – | Alphabetisierung, langfristig Ertrag |

Jedes Gebäude hat **Stufen** (1, 2, 3 …): Jede Stufe kostet und bringt mehr. Eine Stufe schafft
**Arbeitsplätze** für einen Beruf; die Menschen kommen aus der Wanderung (Landarme, Handwerker →
Fabrikarbeiter), die heute schon nach Einkommen zieht. So entstehen Industriestädte dort, wo gebaut
wird, statt überall ein wenig.

### 3.3 Wirkung auf die Wirtschaft

```
Gebäude (Stufe, Landesteil) → Arbeitsplätze und Ertrag je Kopf → Einkommen → Lebensstandard, Wanderung
                            → Angebot im Markt → Preise → Kaufkraft der übrigen Schichten
                            → Steuern (Kontribution, Akzise) → Staatskasse → nächstes Gebäude
```

Kosten: einmal aus der Staatskasse (Baustoffe aus dem Markt: Holz, Eisen, Waren – Bauen hebt die
Preise), dazu Unterhalt je Monat. Ohne Geld kein Bau; Bauen auf Schulden ist erlaubt, aber teuer (Zins).
Ein Gebäude, das keinen Gewinn abwirft, verfällt langsam (Stufe sinkt) – ein Anreiz, dort zu bauen,
wo es sich lohnt, und ein Grund, die Vorschläge zu zeigen.

### 3.4 Private Bauherren (später)

In einem zweiten Schritt bauen auch **Bürger und Adel** mit ihren Ersparnissen, wo es sich lohnt
(Unternehmer, Aktiengesellschaften ab 1850); Gesetze (Gewerbefreiheit, Aktienrecht) öffnen das. Der
Staat baut Infrastruktur und gibt den Anstoß. Nicht im ersten Schritt (Entscheidung 4).

## 4. KI

Staaten ohne Spieler bauen mit derselben Vorschlagsliste, gewichtet nach ihrer Agenda (Preußen:
Bahnen und Hütten; Österreich: Bahnen nach Triest; Kleinstaaten: Chausseen und Schulen), so dass
Preußen und Sachsen wie historisch industrialisieren. Deterministisch wie die übrige KI.

## 5. Daten, Kern und Prüfsumme

- Neue Datei `common/gebaeude.chron` (Art, Beruf, Arbeitsplätze und Ertrag je Stufe, Kosten, Bauzeit,
  Bedingungen), neue Befehle `Build`, `CancelBuild`; Zustand: Gebäude je Landesteil und Ort, Bauaufträge.
- Nur in Szenarien mit `bauen = ja` (1848 und die Lesezeichen); **1648 bleibt bitgleich**, dort läuft der
  Landesausbau wie bisher.
- Spielstände: neues Format (Version hoch), alte Spielstände 1848 laden ohne Gebäude.
- Brücke: Landesteile, Gebäude, Vorschläge mit erwarteter Wirkung, Bauaufträge.

## 6. Anleitung und Übersicht (Rückmeldung 2)

- **„Was jetzt?“ – der Ratgeber:** oben rechts unter der Übersicht drei Hinweise zur Lage, jeder mit
  einem Knopf, der dorthin führt: „Die Kasse ist im Minus – Haushalt öffnen“, „In Schlesien hungern
  120.000 Menschen – Akzise senken?“, „Ein Bauvorhaben lohnt sich: Zeche im Ruhrgebiet“, „Wahl in zwei
  Monaten“. Abschaltbar; in der Geschichtsstunde eingeführt.
- **Geschichtsstunde, zweiter Teil:** nach dem März 1848 eine kurze Lektion „Das Land entwickeln“
  (Wie geht es dem Volk? Haushalt, erster Bau, Kartenmodus Bauen).
- **Erklärung beim ersten Öffnen** jedes Fensters (zwei Sätze, „Verstanden“), wie heute bei der
  Diplomatie, für alle Bereiche.
- **Jede Zahl mit „Was hilft“:** nach dem Vorbild der Bevölkerung auch Kasse, Legitimität, Unruhe und
  Heer mit Gründen und Hebeln.

## 7. Hunger in der Wirtschaft 1848 (gefunden bei Rückmeldung 5)

Die neuen Werte je Schicht zeigen eine Lücke in der Kalibrierung: Die Grundnahrung ist kein Ziel des
Balancing-Berichts (nur der Lebensstandard, und der liegt im Ziel). In ungesteuerten Läufen ab 1848 deckt
die Unterschicht ihre Grundnahrung in Preußen zu 88 %, in Sachsen zu 65 %; **in Not** (Grundnahrung unter
75 %) sind in Preußen 22 % der Einwohner, in Hannover 34 %, in Sachsen 89 % – und es wird mit den Jahren
mehr (Preußen 1861: 45 %). Hungerjahre 1846/47 sind historisch, ein dauerndes Hungern bis 1871 nicht.

Vorschlag: ein Ziel **Grundnahrung der Unterschicht** im Balancing-Bericht (Median 90–100 %, in
Hungerjahren darunter) und eine Nachkalibrierung der Daten 1848 (Erträge, Getreidehandel zwischen den
Märkten, Warenkorb der Unterschicht), vor dem Bauen, damit Gebäude auf einer gesunden Wirtschaft
aufsetzen. 1648 bleibt unberührt.

## 8. Nach 1871 (Rückmeldung 6)

Die Partie endet 1871 mit dem Urteil der Geschichte. „Weiterspielen“ lässt die Zeit danach ohne Ende
laufen, aber ohne Ereignisse, Journale und Ziele nach 1871: Die Welt bleibt politisch auf dem Stand von
1871 stehen, auch in hundert Jahren. Preußen als Gliedstaat des Reichs ist 1871 richtig (bis 1947), 1973
nicht. Vorschlag: **Nachspiel bis Ende 1880** mit einem deutlichen Hinweis beim Weiterspielen („Freies
Spiel ohne Geschichte, endet 1880“), dann endgültig das Urteil. Spätere Epochen kommen mit eigenen
Meilensteinen.

## 9. Entscheidungen

1. **Ebene des Bauens.** Empfehlung: **Landesteile** (oberste Verwaltungseinheit, Kleinstaaten als
   Ganzes) mit Vorschlagsliste. Alternativen: je Kreis (626, genauer, unübersichtlich), je Staat (am
   einfachsten, aber ohne Geographie).
2. **Gebäude.** Empfehlung: **die neun aus 3.2** mit Stufen. Alternative: nur Industrie und Bahnen
   (schneller, weniger Spielraum auf dem Land).
3. **Landesausbau.** Empfehlung: **in 1848 durch Melioration und Mustergut ersetzen** (der Spieler
   entscheidet), in 1648 bleibt er. Alternative: beides nebeneinander.
4. **Private Bauherren.** Empfehlung: **später** (M15b oder M16). Alternative: gleich mit.
5. **Hunger zuerst.** Empfehlung: **Nachkalibrierung 1848 vor dem Bauen** (Abschnitt 7). Alternative:
   mit dem Bauen zusammen.
6. **Nach 1871.** Empfehlung: **Nachspiel bis Ende 1880** mit Hinweis. Alternativen: Weiterspielen
   abschaffen, oder ohne Ende lassen und nur den Hinweis zeigen.
7. **Reihenfolge zu M14.** Empfehlung: **M15 vor M14b (Europa)**: Bauen und Anleitung machen das Spiel
   spannender, Europa macht es schöner; die freie Karte aus M14a reicht vorerst. Alternative: erst Europa.

## 10. Umsetzungsschritte

| Schritt | Inhalt | Prüfung |
|---|---|---|
| a1 | **Nach 1871:** Nachspiel bis 1880 mit Hinweis | Klicktest |
| a2 | **Hunger:** Ziel Grundnahrung im Balancing-Bericht, Nachkalibrierung 1848 | `bilanz`, Labore 1848, 1648 bitgleich |
| b1 | **Kern:** Landesteile, `gebaeude.chron`, Befehle, Arbeitsplätze, Kosten, Verfall, Spielstand | Kerntests, Determinismus, 1648 bitgleich |
| b2 | **Vorschläge und KI:** erwartete Wirkung je Vorhaben, KI baut nach Agenda | Wirtschaftslabor „bauen“ (Industrialisierung Preußens, Sachsens bis 1871) |
| b3 | **Oberfläche:** Fenster Bauen, Kartenmodus Bauen, Landesteil, Bauamt, Meldekarten | Klicktest |
| c1 | **Ratgeber „Was jetzt?“**, „Was hilft“ für alle Kennzahlen | Klicktest |
| c2 | **Geschichtsstunde Teil 2**, Erklärungen beim ersten Öffnen | Klicktest, Spieltest |
| c3 | **Spieltest, Doku, Review** | Abnahme |

## 11. Umsetzungsstand

| Schritt | Stand |
|---|---|
| a1 | **erledigt:** Nach dem Urteil heißt „Weiterspielen“ „Nachspiel bis 1880“ (Tooltip: freies Spiel ohne Ereignisse, Journale und Ziele bis zum 31. Dezember 1880), eine Meldekarte bleibt liegen und sagt es. Die Zeit läuft bis einschließlich 31. Dezember 1880 (Ende der Partie + 9 Jahre, `EPILOGUE_YEARS`); am 1. Januar 1881 steht sie, das Urteil kommt wieder, der Knopf heißt „Partie beendet“ und ist gesperrt. Ein Spielstand nach dem Nachspiel (etwa 1973) öffnet beim Laden gleich das Urteil. Nur in der Oberfläche, Kern und Prüfsummen unverändert. Durchspieltest bis 1. Januar 1881, Klicktest (Knopf, Meldekarte) |
| a2 | **erledigt (Ursache im Handel, nicht in den Daten):** Der Hunger kam aus einem Pendeln des Handels: Ein Markt zählte die Einfuhr des Vormonats als Angebot und führte sie wieder aus, und viel Ausfuhr ließ ihn teuer erscheinen und zog Einfuhr an; in den Ausfuhrmonaten bekamen die eigenen Leute zu wenig (Kurpfalz: Einfuhr 500.000 und Ausfuhr 476.000 Scheffel im Wechsel, eigene Versorgung 46–73 %). Neu: **Handel nach Eigenbedarf** (`handel_eigenbedarf = ja` in 1848 und den drei Lesezeichen; wirtschaft.md 4.1): Ausfuhr höchstens eigener Überschuss plus Einfuhr des Vormonats, Einfuhr höchstens eigener Mangel plus Ausfuhr des Vormonats. Ergebnis 1848: Grundnahrung aller Berufe 97–99 % statt 90 % (1861: 99 % statt 78 %), Preußen in Not 1,3 % statt 13 %, Preise nahe der Basis statt beim 1,3- bis 1,6-fachen. **Neues Ziel** `grundnahrung { unten = { 90 100 } }` im Balancing-Bericht (optional, nur 1848). 1648 bitgleich (`5db13a66d63e6079`). **Folge für den Zoll:** Das Pendeln über die Zollgrenzen trug den größten Teil des Zolls; der Anteil am Haushalt fällt von rund 3 % auf 0,5 %, Beitritte senken den Zoll stärker und heben den Grenzhandel weniger (Mecklenburg ×1,02). Die Zollziele sind nachgezogen (zollziele_voelkerfruehling.chron mit Begründung). **Offen:** Die eigentliche Zollbasis, Kolonial- und Fabrikwaren vom Weltmarkt bis ins Binnenland, fehlt weiter (Kolonialwaren decken 1848 wie vorher nur rund ein Sechstel der Nachfrage) – ein Thema für die Wirtschaft nach dem Bauen |
