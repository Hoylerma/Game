# 21 – Künstliche Intelligenz

## 1. Ziele

| Ziel | Bedeutung |
|---|---|
| **Plausibel** | Die KI handelt wie historische Akteure handeln konnten – mit Interessen, Zielen und Fehlern |
| **Historisch verankert** | Im historischen Modus folgt die KI eng den historischen Entscheidungen; im offenen Modus frei |
| **Fordernd ohne Schummeln** | Höhere Schwierigkeit bedeutet bessere Planung, nicht unsichtbare Boni |
| **Erklärbar** | Der Spieler kann nachvollziehen, warum die KI etwas tut |
| **Leistungsfähig** | KI-Berechnungen dürfen das Spieltempo nicht bremsen |

## 2. Architektur

```
┌──────────────────────────────────────────────────────────────────┐
│ STRATEGISCHE EBENE (jährlich / bei Ereignissen)                  │
│ Persönlichkeit + historische Agenda + Lagebild → Ziele (gewichtet)│
└───────────────┬──────────────────────────────────────────────────┘
                ▼
┌──────────────────────────────────────────────────────────────────┐
│ DOMÄNEN-PLANER (monatlich)                                       │
│ Wirtschaft · Politik · Diplomatie · Militär · Länder · Opposition│
│ Utility-Bewertung von Optionen, Budgetaufteilung zwischen Domänen│
└───────────────┬──────────────────────────────────────────────────┘
                ▼
┌──────────────────────────────────────────────────────────────────┐
│ AUSFÜHRUNG (täglich / wöchentlich)                               │
│ Armeebewegungen (HTN-Pläne, Einflusskarten), Bauaufträge,         │
│ Abstimmungsverhalten, Reaktionen auf Ereignisse                  │
└──────────────────────────────────────────────────────────────────┘
          Gemeinsames „Lagebild“ (Blackboard) für alle Ebenen
```

- **Utility-KI** für Entscheidungen mit vielen Faktoren (Gesetze, Bündnisse, Bauprojekte).
- **Hierarchische Aufgabennetze (HTN)** für mehrstufige Pläne (Feldzüge, Mobilmachung, Koalitionsverhandlungen).
- **Einflusskarten** für militärische Lagebeurteilung (Bedrohung, Versorgung, Frontdichte).
- **Lagebild:** Zusammengefasste Sicht eines Staates auf die Welt – mit **Informationslücken** (die KI weiß nicht alles; Spionage verbessert das Lagebild).

## 3. KI-Persönlichkeiten

### 3.1 Staatspersönlichkeiten

| Persönlichkeit | Merkmale | Typische Vertreter |
|---|---|---|
| **Machtpolitiker** | Expansion, Risikobereitschaft, kurzfristige Bündnisse | Preußen unter Friedrich II. |
| **Bewahrer** | Status quo, Legitimität, Stabilität | Österreich unter Metternich |
| **Reformer** | Innere Modernisierung vor äußerer Expansion | Rheinbundstaaten der Reformzeit |
| **Händler** | Neutralität, Handelsverträge, geringe Rüstung | Hansestädte |
| **Konfessionspolitiker** | Schutz der Glaubensgenossen | Geistliche Staaten, Kursachsen als Führer des Corpus Evangelicorum |
| **Hegemon** | Vorherrschaft in einer Region | Frankreich unter Ludwig XIV. und Napoleon |
| **Gleichgewichtswahrer** | Unterstützt stets die Schwächeren | Großbritannien |
| **Revisionist** | Will Verträge und Grenzen ändern | Mehrere Mächte nach 1919 |
| **Integrationist** | Supranationale Zusammenarbeit | Europäische Partner nach 1950 |

### 3.2 Persönlichkeit von Personen

Herrscher, Regierungschefs und Minister beeinflussen die Staatspersönlichkeit über ihre Eigenschaften: Ein *Zauderer* verschiebt Kriegsentscheidungen, ein *Machtpolitiker* sucht Gelegenheiten, ein *Ehrlicher Makler* vermittelt. So verändert ein Thronwechsel spürbar das Verhalten eines Staates.

## 4. Historische Agenden

Jede Außenmacht und jeder bedeutende deutsche Staat hat eine **historische Agenda**: gewichtete Ziele mit Zeitfenstern und Auslösern.

| Akteur | Agenda-Beispiele |
|---|---|
| Frankreich | Rheingrenze (bis 1815) · Revanche für 1871 (bis 1918) · Sicherheit vor Deutschland (1919–1945) · europäische Einbindung Deutschlands (ab 1950) |
| Großbritannien | Gleichgewicht auf dem Kontinent · Seeherrschaft · Schutz Hannovers (bis 1837) |
| Russland / Sowjetunion | Westverschiebung · Polen · Meerengen · Pufferzone (ab 1945) |
| Österreich | Kaiserwürde · Führung im Bund · Balkan (nach 1866) |
| Preußen (KI) | Arrondierung · Großmachtstatus · Führung im Norden |
| Bayern (KI) | Rangerhöhung · Eigenständigkeit · Pfalz und Franken |

**Im historischen Modus** werden historische Entscheidungen stark gewichtet und treten nahe ihrer realen Daten ein. **Im offenen Modus** sind Agenden nur ein Faktor unter vielen.

## 5. Domänen-Planer

| Domäne | Aufgaben | Besonderheiten |
|---|---|---|
| **Wirtschaft** | Bauprojekte, Produktionsmethoden, Haushalt, Steuern | Folgt der Wirtschaftsordnung (Plan- oder Marktlogik) |
| **Politik** | Gesetzesvorhaben, Umgang mit Interessengruppen, Ernennungen | Respektiert die Machtverhältnisse; kann sich verkalkulieren |
| **Wahlen und Parteien** | KI-Parteien wählen Wahlkampfthemen, Spitzenkandidaten, Koalitionspräferenzen | Nutzen dieselben Regeln wie Spieler im Parteienmodus |
| **Koalitionen** | Verhandeln nach Programm, Ressorts und Koalitionswerten | Verhandlungsbereitschaft sinkt mit Wahlverlusten |
| **Diplomatie** | Bündnisse, Krisen, Konferenzen, Friedensschlüsse | Bewertet Bedrohung, Vertrauen und Agenda |
| **Militär** | Aufmarsch, Frontzuweisung, Operationen, Rückzüge | Nutzt das Kräfte-Raum-Modell und die Versorgungslage |
| **Länder** | Bundesratsabstimmungen, Landespolitik | Parteipolitische Färbung vs. Landesinteressen |
| **Opposition** | KI-Widerstandsnetzwerke und Bürgerbewegungen | Vorsichtig, opportunistisch bei Krisen |

## 6. Die Regime-KI (1933–1945)

Die Regime-KI ist eine **Sonderform** (siehe [16 Weltkriege](16_weltkriege.md)):

- **Agenda-getrieben:** Folgt den dokumentierten Zielen des Regimes; Variabilität nur in Zeitpunkt und Reihenfolge, abhängig von Weltlage und Widerstand.
- **Polykratisch:** Mehrere Unteragenten (Partei, SS- und Polizeiapparat, Wehrmachtführung, Wirtschaftsressorts) konkurrieren; daraus entstehen Radikalisierung und Chaos.
- **Interaktion mit Spielerperspektiven:** Erteilt der militärischen Perspektive Befehle und überstimmt sie; verfolgt Widerstandsnetzwerke; reagiert auf militärische Lage.
- **Keine Spielerkontrolle**, keine Optimierungsziele für Verbrechen – die Verbrechen sind Teil der dokumentarischen Ebene, nicht Ergebnis von KI-Abwägungen.

## 7. Die Weltbühne-KI

- Rund 25 Außenmächte werden **aggregiert** simuliert: Wirtschaftskraft, Militär, Stabilität, Ideologie, Bündnisse.
- Sie reagieren auf Deutschland (Bedrohung, Handel, Bündnisangebote) und aufeinander (Ereignisgraph mit Weltereignissen).
- Kriege zwischen Außenmächten werden **abstrahiert** berechnet, sofern deutsche Staaten nicht beteiligt sind.

## 8. Schwierigkeitsgrade

| Stufe | KI-Verhalten |
|---|---|
| **Lehrling** | Langsame Reaktion, vorsichtig, verzeiht Fehler |
| **Geselle** | Standardplanung |
| **Meister** | Vorausschauende Planung, koordinierte Bündnisse |
| **Kanzler** | Nutzt Schwächen konsequent, plant mehrere Züge voraus |
| **Eiserne** (optional) | Wie „Kanzler“, zusätzlich mit kleinen, offen ausgewiesenen Boni |

Unsichtbare Boni gibt es nicht. Optionale Boni der höchsten Stufe werden im Spiel transparent angezeigt.

## 9. Erklärbarkeit

- **„Warum?“-Tooltips:** Bei jeder bedeutenden KI-Entscheidung (Kriegserklärung, Bündnis, Koalition, Gesetz) zeigt ein Tooltip die drei wichtigsten Gründe (z. B. „Bedrohung durch Preußen +35“, „Erbanspruch auf Jülich +20“, „Kurfürst ist Machtpolitiker +15“).
- **Absichten:** Mit ausreichendem Lagebild (Diplomaten, Spionage) sieht der Spieler die Ziele anderer Staaten („Frankreich strebt die Rheingrenze an“).
- **Ereignisprotokoll:** Die Chronik verzeichnet bedeutende KI-Entscheidungen mit Begründung.

## 10. Tests und Kalibrierung

Der **Prüfstand** (siehe [02 Engine](02_engine_und_technik.md)) lässt nachts Tausende Partien ohne Spieler laufen und prüft **Plausibilitätskorridore**:

| Messgröße | Historischer Modus (Ziel) | Plausibler Modus (Ziel) |
|---|---|---|
| Preußen besitzt Schlesien 1763 | 60–80 % der Läufe | 40–60 % |
| Ein deutscher Bundesstaat (in irgendeiner Form) entsteht bis 1880 | 70–90 % | 50–75 % |
| Davon kleindeutsch unter Preußen | 55–75 % | 35–55 % |
| Großer europäischer Krieg zwischen 1905 und 1920 | 70–90 % | 50–75 % |
| Ende der Weimarer Demokratie bis 1935 | 70–85 % | 45–65 % |
| Teilung nach einer Niederlage im Zweiten Weltkrieg | ≥ 80 % | 60–80 % |
| Einheit bis 2000 (falls geteilt) | 60–80 % | 40–60 % |

Zusätzlich: Wirtschaftskennzahlen (Industrialisierungsgrad 1913, Inflation 1923, Arbeitslosigkeit 1932, Wachstum 1950–1970) und Wahlergebnisse (siehe [10 Wahlen](10_parlamente_wahlen_koalitionen.md)) müssen in festgelegten Korridoren liegen.

## 11. Leistungsbudget

| Ebene | Takt | Budget (Referenz-PC) |
|---|---|---|
| Strategisch | jährlich bzw. bei Ereignissen | ≤ 50 ms je Staat, verteilt |
| Domänen-Planer | monatlich | ≤ 5 ms je Staat, parallelisiert |
| Ausführung | täglich/wöchentlich | ≤ 1 ms je Armee bzw. Front |
| Weltbühne | monatlich | ≤ 10 ms gesamt |
