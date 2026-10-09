# 22 – UI/UX, Grafik & Audio

## 1. Grundsätze der Benutzeroberfläche

1. **Dichte mit Ordnung:** Grand Strategy lebt von Informationen. Jede Zahl ist erklärbar (verschachtelte Tooltips), jede Ansicht hat eine klare Hauptfrage.
2. **Die Karte ist der Mittelpunkt:** Fenster überlagern die Karte, verdrängen sie aber nicht.
3. **Die Epoche ist spürbar:** Rahmen, Typografie, Farben und Klänge der Oberfläche wechseln mit der Epoche.
4. **Lesbarkeit vor Stil:** Jeder Text ist in einer gut lesbaren Schrift verfügbar; Zierschriften sind optional.
5. **Weniger Klicks für häufige Aufgaben:** Kabinettsvorlage, Schnellzugriffe, Automatisierung.

## 2. Hauptbildschirm

```
┌──────────────────────────────────────────────────────────────────────────────────────┐
│ [Wappen] Kgr. Preußen │ Kasse 12.400 Taler │ Legitimität 68 │ Verwaltung 42/50 │     │
│ Einigkeit 41 │ Recht 37 │ Freiheit 22 │ Wohlstand 35 │ Geltung 58 │  14. März 1848 ▶▶ │
├────────────┬────────────────────────────────────────────────────────┬────────────────┤
│ STAAT      │                                                        │ ÜBERSICHT      │
│ Kabinett   │                                                        │ Armeen/Flotten │
│ Gesetze    │                                                        │ Bauprojekte    │
│ Parlament  │                      K A R T E                         │ Gesetzes-      │
│ Gesellsch. │                                                        │   vorhaben     │
│ Wirtschaft │                                                        │ Krisen         │
│ Haushalt   │                                                        │ Journale       │
│ Diplomatie │                                                        │ Missionen      │
│ Militär    │                                                        │ Beobachtete    │
│ Wissen     │                                                        │   Persönl.     │
│ Chronik    │                                                        │                │
│ Lexikon    │                                                        │                │
├────────────┴──────────────────────────────────────┬─────────────────┴────────────────┤
│ NACHRICHTEN: „Barrikaden in Berlin!“ · „Metternich │ KARTENMODI  [Politik] [Gesell.] │
│ geflohen“ · „Volksversammlung in Heidelberg“ …     │ [Wirtschaft] [Wahlen] [Militär] │
└────────────────────────────────────────────────────┴──────────────────────────────────┘
```

- **Oberste Leiste:** Staat, Kasse, Kernwerte und die fünf Vermächtnis-Dimensionen; Datum und Geschwindigkeit.
- **Linke Leiste:** Hauptbereiche.
- **Rechte Leiste (Übersicht):** Laufende Dinge, frei konfigurierbar.
- **Untere Leiste:** Nachrichtenticker im Stil der Epoche und Kartenmodus-Schnellwahl.

## 3. Bildschirme (Auswahl)

| Bildschirm | Inhalt |
|---|---|
| **Staatsübersicht** | Regierungsform, Kennzahlen, Vermächtnis, Hypotheken |
| **Kabinett / Hof** | Herrscher, Regierungschef, Minister, Berater, Ressortvorhaben, Kabinettsvorlage |
| **Gesetze** | 36 Gesetzesgruppen, laufende Vorhaben, Unterstützung und Widerstand |
| **Plenarsaal** | Sitzordnung nach Fraktionen, Abstimmungen, Mehrheitsrechner, Ausschüsse |
| **Wahlen** | Umfragen, Wahlkampf, Wahlabend, Wahlkreiskarte, Wählerwanderung |
| **Koalitionsverhandlung** | Programmpunkte, Ressortverteilung, Zustimmung der Basis |
| **Verfassungskonvent** | Kapitel, Artikel, Fraktionspositionen, Abstimmungen |
| **Parteien** | Programm, Flügel, Mitglieder, Finanzen, Hochburgen |
| **Interessengruppen** | Einfluss, Zustimmung, Forderungen, Führung |
| **Gesellschaft** | Pop-Zusammensetzung, Milieus, Konfessionen, Lebensstandard, Migration |
| **Wirtschaft** | Gebäude, Produktionsmethoden, Märkte und Preise, Zollgrenzen, Unternehmen |
| **Haushalt und Geld** | Einnahmen, Ausgaben, Schulden, Geldmenge, Inflation, Zentralbank |
| **Diplomatie** | Beziehungen, Bündnisse, Verträge, Krisen, Friedenskonferenzen |
| **Militär** | Armeen, Fronten, Heeresgruppen, Mobilmachung, Aufmarschpläne, Versorgung |
| **Flotte und Luft** | Flotten, Seegebiete, Luftlage |
| **Wissen** | Innovationen, Institutionen, Kultur |
| **Journale und Missionen** | Epochen-Journal, Themenjournale, Missionsbaum |
| **Bund und Länder** | Bundesrat, Länderübersicht, Finanzausgleich (Ländermodus) |
| **Netzwerk** | Widerstands- bzw. Oppositionsnetz (Oppositionsmodus) |
| **Statistisches Jahrbuch** | Alle Kennzahlen als Tabellen und Diagramme über die Zeit, vergleichbar mit anderen Staaten und mit der realen Geschichte |
| **Chronik** | Geschichtsbuch der Partie |
| **Lexikon** | Hintergrundwissen mit Quellen |

## 4. Die Zeitung

Wichtige Entwicklungen erscheinen als **Titelseite einer Zeitung** im Stil der Epoche:

| Epoche | Gestaltung |
|---|---|
| 1648–1800 | Einblattdruck, Flugschrift, „Relation“ mit Holzschnitt |
| 1800–1870 | Mehrspaltige Zeitung in Fraktur, Lithografie |
| 1870–1918 | Massenpresse, Extrablätter |
| 1918–1945 | Boulevard- und Parteipresse; in der Diktatur gleichgeschaltete Blätter (als Quelle gekennzeichnet) |
| 1945–1990 | Lizenzpresse, Illustrierte, in der DDR Parteizeitung; dazu Fernsehnachrichten-Optik |
| ab 1990 | Nachrichtenseiten, Eilmeldungen, soziale Medien |

Zeitungen tragen **erfundene Titel** im Stil der Zeit. Schlagzeilen werden aus Spieldaten generiert und dürfen pointiert sein – außer bei Verbrechen und Opfern.

## 5. Grafikstil

### 5.1 Karte

Die Karte „altert mit“ (siehe [02 Engine](02_engine_und_technik.md) und [05 Karte](05_karte_und_geographie.md)): Kupferstich → Klassizismus → Lithografie → Schulatlas → Digital.

### 5.2 Oberflächenrahmen je Epoche

| Epoche | Rahmen und Typografie |
|---|---|
| I–II | Barocke Kartuschen, Kupferstichlinien, Antiqua mit Zierinitialen |
| III | Klassizistische Klarheit, Biedermeier-Ornamente |
| IV | Historismus und Jugendstil, Prägedruck-Optik |
| V | Neue Sachlichkeit und Bauhaus-Typografie (Weimar); in der NS-Zeit neutrale, nüchterne Gestaltung ohne Regime-Ästhetik |
| VI | Wirtschaftswunder-Pastell (BRD) bzw. Behördenoptik der DDR; 1970er-Grafik |
| VII–VIII | Moderne, flache Gestaltung |

### 5.3 Bilder

- **Porträts:** Illustriert; Stil passend zur Epoche (Ölgemälde, Lithografie, stilisierte Fotografie). Lebende Personen nur stilisiert.
- **Ereignisbilder:** Eigene Illustrationen im Epochenstil; historische Fotos nur in der dokumentarischen Ebene (lizenziert, mit Quellenangabe).
- **3D-Elemente der Karte:** Städte, Festungen, Industrie, Häfen, Tagebaue, Einheiten – zurückhaltend, damit die Karte lesbar bleibt.

## 6. Audio

### 6.1 Musik

| Epoche | Musikalisches Konzept |
|---|---|
| I | Barock: Neueinspielungen gemeinfreier Werke (Bach, Händel, Telemann, Buxtehude) und Originalkompositionen im Stil der Zeit |
| II | Klassik und Militärmusik der Zeit (Haydn, Mozart, frühe Märsche) |
| III | Romantik (Beethoven, Schubert, Schumann, Mendelssohn) und Volkslieder des Vormärz („Die Gedanken sind frei“) |
| IV | Spätromantik (Brahms, Bruckner, Strauss) und Originalmusik |
| V (Weimar) | Originalkompositionen im Stil von Kabarett, Jazz und Neuer Musik |
| V (Diktatur und Krieg) | Zurückhaltende, ernste Originalmusik; **keine** Musik des Regimes |
| VI | Originalmusik, inspiriert von Schlager, Ostrock und frühen elektronischen Klängen |
| VII–VIII | Elektronik und moderne Orchestermusik |
| Dokumentarische Ebene | Stille oder sparsames Klavier und Streicher |

- **Leitmotive:** Jede Fokus-Nation und jedes Bundesland hat ein Motiv, das sich im Stil der Epoche verwandelt.
- **Aufnahmen** mit einem deutschen Orchester und Ensembles für Alte Musik, Jazz und Elektronik.

### 6.2 Klangkulisse und Bedienklänge

- Umgebungsklänge je Kartenzoom (Stadt, Hafen, Industrie, Land).
- Bedienklänge im Stil der Epoche: Gänsekiel, Siegel, Telegraf, Schreibmaschine, Fernschreiber, Telefon, Smartphone.
- **Erzählstimme** („der Chronist“) für Wendepunkte und Epochenurteile (optional).

## 7. Barrierefreiheit

| Bereich | Maßnahmen |
|---|---|
| **Sehen** | Modi für Farbsehschwächen (alle Kartenmodi mit Mustern statt nur Farben), skalierbare Oberfläche (75–200 %), hoher Kontrast, gut lesbare Schriften |
| **Hören** | Untertitel für die Erzählstimme, visuelle Hinweise für akustische Signale |
| **Motorik** | Pausieren jederzeit, frei belegbare Tasten, Controller-Layout (Steam Deck), keine Zeitdruck-Elemente außer optional |
| **Kognition** | Kabinettsvorlage, Tutorials, Glossar, reduzierte Komplexität optional (vereinfachte Wirtschaft) |
| **Bewegung** | Reduzierte Animationen |

## 8. Lokalisierung in der Oberfläche

- Deutsche Texte sind im Schnitt deutlich länger als englische → flexible Layouts, keine festen Textfelder.
- Historische Begriffe werden in Übersetzungen beibehalten und per Tooltip erklärt.
- Datums- und Zahlenformate je Sprache; Währungen mit historischen Zeichen und Einheiten.

## 9. Onboarding

- **Geschichtsstunden** (Tutorialkampagnen je Epoche) und kontextsensitive Hinweise (siehe [03 Kernspielablauf](03_kernspielablauf.md)).
- **„Was ist passiert?“** – Rückblick nach dem Laden eines Spielstands: die letzten wichtigen Entwicklungen auf einen Blick.
- **Empfohlene Startstaaten** mit Schwierigkeitsangaben im Lesezeichen-Menü.
