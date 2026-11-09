# Spielbarer Vertical Slice 1848–1871 (Meilenstein M13)

> **Status: verbindlich** – Entscheidungen 1–19 wie empfohlen freigegeben (Abschnitt 11); Umsetzung
> läuft, Stand in Abschnitt 13. Grundlage sind [22 UI/UX, Grafik, Audio](../22_ui_ux_grafik_audio.md),
> [03 Kernspielablauf](../03_kernspielablauf.md) (Abschnitte 6–8), [00 Vision](../00_vision_und_pitch.md)
> (Abschnitt 7, Vermächtnis), [21 Künstliche Intelligenz](../21_kuenstliche_intelligenz.md) (Abschnitt 8,
> Schwierigkeitsgrade), [24 Produktion](../24_produktion_roadmap_dlc.md) (Phase 2) und der Meilensteinplan
> ([meilensteine.md](../meilensteine.md#m13--spielbarer-vertical-slice-18481871)). Die Systeme, auf
> denen der Slice steht, beschreiben [politik.md](politik.md), [diplomatie.md](diplomatie.md),
> [militaer.md](militaer.md), [ereignisse.md](ereignisse.md) und [ki.md](ki.md); die Schnittstelle zu
> Godot [bridge-api.md](bridge-api.md).

Mit M12 rechnet der Kern die Epoche III vollständig: Bevölkerung, Wirtschaft und Haushalt,
Wahlen, Zollverein, Diplomatie und Bundesversammlung, Krieg, Ereignisse und Journale, dazu eine KI,
die die Staaten ohne Spieler führt. Die Oberfläche ist dagegen noch ein **Prototyp**: Das Spiel
startet in der Testregion 1648, die Fenster sind aus Knöpfen am Staatspanel gewachsen, es gibt
weder Musik noch Klänge, keine Einstellungen, kein Tutorial, kein Ende der Partie, und gespielt
wird nur aus dem Repository mit selbst gebauter Erweiterung.

M13 macht daraus **eine Epoche, die sich wie das fertige Spiel anfühlt** (Meilensteinplan) – die
Grundlage für Förderung, Publisher und Early Access (Kapitel 24, Phase 2):
- ein **Startbildschirm** mit Lesezeichen, empfohlenen Staaten, Schwierigkeit und Geschichtsverlauf;
- eine **Partie mit Anfang und Ende**: Die deutsche Frage verändert die Ordnung auf der Karte, die
  Wahlen gehen bis 1871 weiter, das **Vermächtnis** wird laufend gemessen, und 1871 steht ein
  **Urteil der Geschichte**;
- ein **Hauptbildschirm nach Kapitel 22** mit allen Fenstern der Epoche, verschachtelten Tooltips,
  Statistischem Jahrbuch, Lexikon, Zeitung und Einstellungen;
- **Präsentation** im Stil der Epoche III (Rahmen, Schriften, Musik und Klänge in Grundfassung);
- **Onboarding**: eine Geschichtsstunde, Hinweise, „Was ist passiert?“;
- **Lokalisierung** Deutsch, Englisch vorbereitet;
- **installierbare Fassungen** für Windows, Linux und macOS;
- die **Abnahme**: eine Partie 1848–1871 ohne Fehler, ein Spieltest mit externen Testern, die
  Leistungsziele aus M3 und M4 mit allen Systemen.

## 1. Umfang

Der Meilensteinplan verlangt:

- Oberfläche nach Kapitel 22: alle Fenster der Epoche, Einführung bzw. Tutorial, Musik und Geräusche
  in Grundfassung;
- Lokalisierung Deutsch (Englisch vorbereitet);
- installierbare Versionen für Windows, Linux und macOS;
- Abnahme: Eine Partie 1848–1871 lässt sich vollständig ohne Fehler spielen; Spieltest mit externen
  Testern, Rückmeldungen dokumentiert; Leistungsziele aus M3 und M4 gelten auch mit allen Systemen.

Vorschlag (Entscheidung 1): **zwei Teile** wie bei M7 bis M12.

| Teil | Inhalt | Ergebnis |
|---|---|---|
| **M13a – die Partie** | Startbildschirm und Spieleinstellungen, Schwierigkeitsgrade und Geschichtsverlauf, Bünde zur Laufzeit (Ordnung nach der deutschen Frage), Wahlen bis 1871, Vermächtnis und Jahresreihen, Ende 1871 mit Urteil der Geschichte, Hauptbildschirm und Fenster der Epoche, Leistung | Eine Partie 1848–1871 lässt sich von Anfang bis Ende spielen und auswerten (automatischer Durchspieltest) |
| **M13b – Präsentation und Auslieferung** | Epochenrahmen und Schriften, Zeitung, Musik und Klänge, Geschichtsstunde und Hinweise, Lokalisierung, Pakete für drei Systeme, Rückmeldefunktion, Spieltest, Review | Installierbare Fassungen; Spieltest mit externen Testern dokumentiert |

**Nicht in M13:**
- **Weitere Epochen und Lesezeichen** (1815, 1862, 1871, 1914 kommen mit Early Access 1, Kapitel 24).
  1648 bleibt als Technikprototyp erreichbar (Entscheidung 2).
- **Multiplayer und Modding** (Kapitel 23).
- **Erzählstimme** („der Chronist“), Leitmotive je Staat, Orchesteraufnahmen (Kapitel 22, Abschnitt 6).
- **Porträts und Ereignisbilder** (Entscheidung 12); Wappen und eigene Ornamente ja.
- **Koalitionsverhandlung, Verfassungskonvent als eigener Bildschirm, Parteien mit Programm und
  Flügeln, Flotte, Wissen, Bund und Länder, Netzwerk** (Kapitel 22, Abschnitt 3): Die Systeme dahinter
  gibt es in der Epoche III noch nicht oder erst später.
- **Gesetzgebungsverfahren, Ernennungen, Automatisierung, Kabinettsvorlage** (Kapitel 03,
  Abschnitt 6): Die Kabinettsvorlage braucht Planer für den Spieler, die Automatisierung die KI für
  einzelne Bereiche des Spielerstaates; beides kommt mit Early Access.
- **Ironman und Erfolge**, Steam-Anbindung, Signieren und Notarisieren der Pakete (Entscheidung 16).
- **Fachprüfung** der Daten durch den Beirat; sie bleibt für alle Meilensteine offen.

## 2. Ausgangslage

| Bereich | Stand nach M12 | Lücke für M13 |
|---|---|---|
| Start | Das Spiel lädt 1648 (Testregion); „Neues Spiel“ im Spielmenü wählt Lesezeichen und Staat | kein Startbildschirm, keine Spieleinstellungen, keine Empfehlung |
| Schwierigkeit | eine KI-Stufe („Geselle“), historischer Modus; Regel `agenda_offen` vorbereitet (ki.md 3.4) | Stufen und Geschichtsverlauf wählbar |
| Deutsche Frage | Journal mit fünf Ausgängen (M11); Bünde und Bundesgebiet bleiben die des Starts (militaer.md 6.1), ein Reich ist ein Merkmal | Die Ordnung nach dem Ausgang ist auf der Karte und im Bund nicht zu sehen |
| Wahlen | Nationalversammlung 1848, Preußische Zweite Kammer 1849 (M8) | keine Wahlen nach 1849; der Parteienmodus hat danach nichts zu tun |
| Ende | keins: Die Partie läuft weiter, das Journal endet am 30. Juni 1871 | kein Urteil, keine Auswertung |
| Hauptbildschirm | obere Leiste, Staatspanel links mit Knöpfen für sieben Fenster, Chronik, Legende, Kartenmodi (17), Krisenleiste | Aufbau nach Kapitel 22 (Bereiche, Übersicht, Ticker), fehlende Fenster |
| Fenster | Haushalt, Verfassung und Gruppen, Zollunion, Diplomatie (drei Reiter), Krieg, Heer, Ereignis, Journale, Wahlabend, Parteipanel, Spielmenü | Staatsübersicht, Kabinett, Gesetze, Plenarsaal, Gesellschaft, Wirtschaft, Jahrbuch, Lexikon, Zeitung, Wendepunkt, Einstellungen |
| Tooltips | einzelne Tooltips, keine Verschachtelung | jede Zahl der Leiste und der Staatsübersicht erklärt sich |
| Grafik | eigenes Theme, Systemschriften (Noto Serif, Georgia …), Wappen aus Farben | Epochenrahmen III, eingebettete Schriften |
| Audio | keins | Musik und Klänge in Grundfassung |
| Onboarding | Hinweise nach dem Start (Lesezeichen), historischer Kompass | Geschichtsstunde, Hinweise, „Was ist passiert?“ |
| Lokalisierung | alle Texte deutsch im GDScript, Inhalt deutsch im Chronik-Skript, Texte des Kerns deutsch | Texte der Oberfläche übersetzbar, Englisch vorbereitet |
| Auslieferung | nur aus dem Repository; Erweiterung für Linux und Windows in der CI gebaut, keine für macOS; Daten unter `../data` | Pakete für drei Systeme mit Daten |
| Leistung | 1848 mit allen Systemen 2,5 s je Spieljahr (4 Kerne, Release, Cloud-Rechner); Karte mit llvmpipe 16–32 Bilder/s, mit Grafikkarte nicht gemessen | Ziele aus M3 (60 Bilder/s) und M4 (2 s je Spieljahr) |
| Tests | Rauch-, Klick-, Einheiten- und Leistungstest; Labore im Kern | Durchspieltest 1848–1871 über die Oberfläche, Spieltest mit Menschen |

## 3. Die Partie (M13a)

### 3.1 Startbildschirm und neues Spiel

Das Spiel startet mit einem **Startbildschirm** statt mit 1648 (Entscheidung 2): Titel, ein
Kartenausschnitt 1848 im Hintergrund, die Knöpfe **Neues Spiel**, **Fortsetzen** (letzter Spielstand),
**Laden**, **Geschichtsstunde**, **Einstellungen**, **Beenden**.

**Neues Spiel** führt in drei Schritten:
1. **Lesezeichen:** „Völkerfrühling“ (1. März 1848) und die drei Kriegslesezeichen „Düppel“ (1864),
   „Königgrätz“ (1866), „Sedan“ (1870), je mit Kurztext und Lage. Unter „Weitere“ steht 1648 als
   „Technikprototyp (Testregion)“.
2. **Staat:** Karte zum Anklicken und Liste; oben die **empfohlenen Startstaaten** mit Schwierigkeit
   und Kurztext (Entscheidung 3):

   | Staat | Schwierigkeit | Kurztext |
   |---|---|---|
   | Preußen | mittel | Zollverein, Revolution in Berlin, Führung im Norden |
   | Österreich | schwer | Präsidium im Bund, Völker des Kaiserstaats, Revolution in Wien und Ungarn |
   | Bayern | mittel | größter Mittelstaat, Trias, Lola-Montez-Affäre |
   | Sachsen | schwer | Industrie, Maiaufstand 1849, zwischen den Großmächten |

   Alle übrigen Staaten bleiben spielbar, mit dem Hinweis „nicht abgestimmt“. Im Parteienmodus
   wählt man statt des Staates eine Strömung (M8).
3. **Spieleinstellungen:** Schwierigkeit, Geschichtsverlauf, historischer Kompass, Pausenregeln
   (Abschnitt 4.4); die Werte des letzten Spiels sind vorausgewählt.

Die Kriegslesezeichen beginnen im Krieg und enden wie die Partie ab 1848 (Abschnitt 3.5).

### 3.2 Schwierigkeit und Geschichtsverlauf

**Schwierigkeit** (Entscheidung 4) in drei Stufen nach Kapitel 21, Abschnitt 8 – als **Parameter
derselben Planer**, ohne Boni:

| Stufe | Schwelle | Rauschen | Abkühlung | Stärke für Krieg und Forderung |
|---|---|---|---|---|
| **Lehrling** | ×1,5 | ×2 | ×2 | +0,25 (vorsichtiger) |
| **Geselle** (Standard, wie M12) | ×1 | ×1 | ×1 | ±0 |
| **Meister** | ×0,8 | ×0,5 | ×0,75 | −0,1, Bündnisfall bei der Kriegserklärung mit Verbündeten (die KI rechnet Verbündete der Gegenseite schon heute ein; Meister bietet vorher Bündnisse gegen den Spieler an) |

Die Faktoren stehen in `common/ki.chron` (neuer Block `stufe <name> { … }`, chronik-skript.md 3.37).
Kanzler und Eiserne folgen, wenn die Planer mehrere Züge vorausrechnen (Early Access).

**Geschichtsverlauf** (Kapitel 03, Abschnitt 7): **historisch** (Standard, wie M12, kalibriert) und
**offen** (Regel `agenda_offen`: Gewichte halbiert, Zeitfenster offen, ki.md 3.4). „Plausibel“ braucht
eigene Korridore und kommt mit Early Access. Im offenen Modus gelten die Korridore der Labore nicht;
das KI-Labor rechnet ihn mit `--offen` und prüft nur Abstürze.

Beides sind **Spieleinstellungen** im Zustand: Befehl `Spieleinstellung` am Start (Stufe, Verlauf),
im Spielstand (Format 7, Abschnitt 8), unveränderlich während der Partie. Das KI-Labor bekommt
`--stufe` und `--offen`.

### 3.3 Ordnung nach der deutschen Frage: Bünde zur Laufzeit

Bis M12 bleiben Besitz, Bundesgebiet und Bundesversammlung einer Partie ab 1848 die des Starts;
nur Kriege ändern den Besitz (Annexionen). Ein kleindeutsches Reich ist ein Merkmal ohne Folge auf
der Karte. Für einen Slice, der 1871 ein Urteil spricht, muss die neue Ordnung **sichtbar und
wirksam** sein (Entscheidung 5):

- **Vorlagen für Bünde:** Die Bundesversammlungen in `common/bundesversammlung.chron` (chronik-skript.md
  3.29) bekommen neben
  `seit` (datiert, für die Kriegslesezeichen) die Form `vorlage = ja`: Name, Präsidium, Sitz und
  Stimmen je Staat, aber ohne Datum. Neue Vorlagen: **Reich der Paulskirche** (Reichstag nach der
  Reichsverfassung 1849), **Norddeutscher Bund** (wie 1867), **Deutsches Reich** (Bundesrat 1871 mit
  58 Stimmen), **Großdeutsches Reich** (Spielwert), **Süddeutscher Bund** (Spielwert, für „zwei Bünde“).
- **Wirkung `bund { vorlage = … mitglieder = … }`** an Ereignisoptionen und Journalstufen
  (chronik-skript.md 3.34 und 3.35): löst die bisherige Bundesversammlung auf oder ersetzt sie und gründet den
  Bund mit den Mitgliedern. Mitglieder sind wählbar als Liste, als „Sieger des Krieges gegen X und
  ihre Verbündeten“, als „Staaten mit Merkmal `reichsbeitritt`“ oder als „Bundesstaaten nördlich der
  Mainlinie“ (Verwaltungsgebiete). Staaten treten später mit der Wirkung `bund_beitritt` bei.
- **Folgen im Kern:** Bundesgebiet = Orte der Mitglieder; die Bundesversammlung tagt mit den Stimmen
  der Vorlage (Anträge und Abstimmungen wie M9); Bundesstaaten verlieren das Recht auf eigene
  Kriegserklärungen gegen Mitglieder und Bündnisse mit Nichtmitgliedern (Bundespflicht, wie der Bund
  1815); der Zollverein bleibt eine eigene Union. Keine Mediatisierung: Die Staaten bleiben Staaten.
- **Karte:** Im Kartenmodus „Politisch“ trägt das Bundesgebiet eine kräftige Außengrenze und im
  Staatsnamen-Layer den Namen des Bundes in der Übersicht; ein neuer Kartenmodus **„Bund“** färbt nach
  Bund und Präsidialmacht. Die Diplomatie zeigt den Bund im Reiter „Bundesversammlung“.
- **Umsetzung (Schritt a2):** Es gibt höchstens **einen** Bund zur Laufzeit; „zwei Bünde“ bleibt der
  Norddeutsche Bund neben selbständigen Südstaaten (ein Süddeutscher Bund folgt mit Early Access). Die
  Bundespflicht verbietet nur Kriege unter Mitgliedern; Bündnisse mit Nichtmitgliedern bleiben erlaubt
  (die Schutz- und Trutzbündnisse von 1866 verbanden Preußen mit den Südstaaten). Der Kartenmodus heißt
  „Bund“ (Taste C, Nummer 17); eine eigene Außengrenze im Kartenmodus „Politisch“ folgt mit dem
  Epochenrahmen (b1).
- **Inhalt:** Die Journale „Revolution“ (Reich der Paulskirche) und „Die deutsche Frage“
  (Norddeutscher Bund nach einem preußischen Sieg 1866, Reich nach der Kaiserproklamation,
  zwei Bünde, großdeutsches Reich) und die Ereignisse 1866–1871 rufen die Wirkung auf; die
  Kalibrierung prüft die Korridore der deutschen Frage mit KI neu (ki.md 7).

### 3.4 Wahlen bis 1871

Bis M12 wählt nach 1849 niemand mehr. Vorschlag (Entscheidung 6):

- **Preußisches Abgeordnetenhaus** zu den historischen Terminen (1852, 1855, 1858, 1861, 1862, 1863,
  1866) mit dem Dreiklassenwahlrecht und den Wahlkreisen von 1849 (M8); Neuwahl auch nach einer
  Auflösung durch Ereignis (Verfassungskonflikt 1862).
- **Reichstag eines gegründeten Bundes** (Abschnitt 3.3) mit allgemeinem Männerwahlrecht: erste Wahl
  drei Monate nach der Gründung, danach alle drei Jahre; Wahlkreise der Nationalversammlung 1848
  (633, M8), soweit sie im Bundesgebiet liegen (Spielwert statt der 297 Kreise von 1867).
- Wahlen erscheinen mit **Wahlabend** und Kartenmodus wie 1848; der **Parteienmodus** geht weiter und
  die KI führt für die übrigen Strömungen Wahlkampf (ki.md 6.4).
- Andere Landtage wählen nicht (Daten fehlen); ihre Verfassungen ändern weiter die Ereignisse.

### 3.5 Vermächtnis und Ende der Partie

**Vermächtnis** (Entscheidung 7, Kapitel 00, Abschnitt 7.1): fünf Dimensionen von 0 bis 100 je Staat,
aus dem Zustand berechnet (seit a4 jederzeit statt am Monatsersten, weil es abgeleitet ist und wenig kostet),
mit den Faktoren als Tooltip. Formeln als Spielwerte in
`common/vermaechtnis.chron`, für die Epoche III zum Beispiel:

| Dimension | Faktoren (Auswahl) |
|---|---|
| **Einigkeit** | Bund oder Reich und Rolle darin (Präsidium, Mitglied), Unruhe, laufende Aufstände, Anteil der Kulturen |
| **Recht** | Verfassung (absolut, ständisch, konstitutionell), Zustimmung der Gruppen, Staatsbankrotte, Aufstände |
| **Freiheit** | Gesetze (Presse, Vereine, Grundlasten), Wahlrecht, Wahlen im Zeitraum |
| **Wohlstand** | Lebensstandard, Alphabetisierung, Auswanderung, Schuldenquote |
| **Geltung** | Stärke im Vergleich, Bündnisse, gewonnene Kriege und Krisen, Führung im Bund |

Die obere Leiste zeigt die fünf Werte des Spielerstaates (Kapitel 22, Abschnitt 2), das Jahrbuch ihren
Verlauf. Die Werte sind abgeleitet; nur die Jahreswerte stehen im Zustand (Abschnitt 4.3).

**Ende** (Entscheidung 8): am **1. Juli 1871**, nachdem die Journale am 30. Juni ihren Ausgang gewählt
haben, hält die Partie an und zeigt das **Urteil der Geschichte**:
- Ausgang der deutschen Frage mit Wertung und dem historischen Vergleich („Historisch: Kaiserproklamation
  in Versailles am 18. Januar 1871“), ebenso der Revolution 1848/49;
- die fünf Dimensionen mit ihrem Verlauf seit 1848 und einem Satz je Dimension;
- die Wendepunkte der Partie aus der Chronik, die Kriege mit Ausgang, die Herrscher und Minister;
- **Weiterspielen** (ohne neue Ereignisse, das Journal bleibt beim Ausgang) oder **Hauptmenü**;
- **Chronik exportieren** als HTML-Datei im Benutzerordner (PDF mit Early Access).

### 3.6 Leistung

Ziel (Entscheidung 17): **ein Spieljahr 1848 mit allen Systemen in höchstens 2 Sekunden** (Median aus
drei Jahren, 4 Kerne, Release; wie die Abnahme M4) und die Oberfläche bei Höchsttempo (120 Tage/s)
ohne Rückstau. Heute sind es 2,5 s. Vorgehen wie in M5 und M12: Profil mit callgrind über ein Jahr
1848, die größten Posten verringern, bitgleich. Die Bildrate prüft Abschnitt 10.

## 4. Oberfläche (Gerüst in M13a, Feinschliff in M13b)

### 4.1 Hauptbildschirm

Aufbau nach Kapitel 22, Abschnitt 2 (Entscheidung 9):

- **Obere Leiste:** Wappen und Staat, Kasse mit Saldo, Legitimität (Zustimmung im Mittel),
  Verwaltung (Orte und Kreise), die fünf Vermächtnis-Dimensionen, Datum und Tempo, Menü.
- **Linke Leiste – Bereiche:** Staat, Kabinett, Gesetze, Parlament, Gesellschaft, Wirtschaft, Haushalt,
  Diplomatie, Militär, Chronik, Lexikon. Ein Klick öffnet das Fenster des Bereichs; das bisherige
  Staatspanel wird zur **Staatsübersicht**.
- **Rechte Leiste – Übersicht:** laufende Dinge (Krisen, Kriege, offene Ereignisse, Journale mit Stufe,
  nächste Wahl, Heere in Bewegung), je Eintrag ein Klick zum Fenster; einklappbar.
- **Untere Leiste:** **Nachrichtenticker** mit den Einträgen der Chronik (Wendepunkte zuerst) und die
  Schnellwahl der Kartenmodi.
- Die Karte bleibt Mittelpunkt: Fenster überlagern sie, nie mehr als eines in der Mitte; Esc schließt.

### 4.2 Fenster der Epoche

Entscheidung 10. **Neu** oder **umgebaut** (aus bestehenden Teilen):

| Fenster | Inhalt | Quelle |
|---|---|---|
| **Staatsübersicht** | Regierungsform, Kennzahlen, Vermächtnis mit Faktoren, Steuern, Mobilmachung | umgebaut aus dem Staatspanel |
| **Kabinett und Hof** | Herrscher und leitender Minister mit Werten, Eigenschaften, Amtszeit und Neigungen; frühere Amtsinhaber der Partie | neu (Daten aus M12) |
| **Gesetze** | drei Gesetzesgruppen mit Stand, Wünschen der Gruppen und Wirkung; Verfassung und Wahlrecht | umgebaut aus „Verfassung und Gruppen“ |
| **Parlament (Plenarsaal)** | Halbrund nach Fraktionen der letzten Wahl, Abstimmungen (Paulskirche, M11), nächste Wahl | neu (Halbrund aus dem Wahlabend) |
| **Interessengruppen** | Anteil, Einfluss, Zustimmung, Forderungen | umgebaut aus „Verfassung und Gruppen“ |
| **Gesellschaft** | Bevölkerung des Staates: Kulturen, Konfessionen, Altersaufbau, Lebensstandard, Alphabetisierung, Wanderung, Auswanderung | neu (Summen über die Orte, Brücke neu) |
| **Wirtschaft** | Güter mit Preis und Menge in den Märkten des Staates, Betriebe nach Art, Zollgebiet | neu (Brücke neu) |
| **Haushalt**, **Zollunion**, **Diplomatie**, **Krieg**, **Heer**, **Journale**, **Ereignis**, **Wahlabend** | wie M5–M12 | bestehend, im neuen Rahmen |
| **Statistisches Jahrbuch** | rund 15 Kennzahlen je Jahr (Bevölkerung, Einnahmen, Schulden, Lebensstandard, Alphabetisierung, Unruhe, Soldaten, Vermächtnis …) als Tabelle und Liniendiagramm, Vergleich mit bis zu vier Staaten | neu (Jahresreihen im Kern, Abschnitt 4.3) |
| **Chronik** | Vollbild mit Filter (Staat, Art, Wendepunkte), Suche, Sprung zum Ort | umgebaut aus dem Chronikpanel |
| **Lexikon** | Begriffe aus dem Glossar (Anhang B) und den Ereignisquellen, Verweise aus Tooltips | neu (Artikel als Markdown in `data/lexikon/`, gelesen von Godot wie die Kartendateien) |
| **Wendepunkt** | großer Bildschirm für Ereignisse mit `wendepunkt = ja` (Kapitel 18, Abschnitt 6) mit Optionen, Folgen und historischem Kompass | umgebaut aus dem Ereignisfenster |
| **Zeitung** | Titelseite bei Wendepunkten, Kriegsbeginn und Frieden, Wahlergebnissen, Bundesgründung (Abschnitt 5.2) | neu |
| **Urteil der Geschichte** | Abschnitt 3.5 | neu |
| **Einstellungen** | Abschnitt 4.5 | neu |

### 4.3 Jahresreihen und Tooltips

**Jahresreihen** (Entscheidung 11): Der Kern schreibt am 1. Januar je Staat die Kennzahlen des
Jahrbuchs und das Vermächtnis in eine Reihe im Zustand (rund 20 Werte × 40 Staaten × 24 Jahre, im
Spielstand). Die Godot-Seite rechnet nichts selbst (Grundregel).

**Verschachtelte Tooltips** (Kapitel 03, Abschnitt 6) in Grundfassung: Jede Zahl der oberen Leiste
und der Staatsübersicht hat einen Tooltip mit ihrer Aufschlüsselung (Kasse: Einnahmen je Steuer und
Ausgaben je Posten; Vermächtnis: Faktoren; Unruhe: Ursachen). Begriffe mit Lexikoneintrag sind
unterstrichen; ein Klick bei offenem Tooltip öffnet den Eintrag. Ein Tooltip in einem Tooltip bleibt
offen, solange die Maus darüber steht (eigene Tooltip-Ebene, nicht Godots Standard).

### 4.4 Pausenregeln und Meldungen

Automatische Pause (Kapitel 03, Abschnitt 6), einzeln abschaltbar: Kriegserklärung an oder durch den
Spielerstaat, Forderung an ihn, Ereignis des Spielerstaates (immer, wie M11), Wahlergebnis,
Amtswechsel im eigenen Staat, Bundesgründung, Frieden. Meldungen erscheinen als Hinweis oben und im
Ticker; Wendepunkte und Bundesgründung zusätzlich als Zeitung.

### 4.5 Einstellungen und Barrierefreiheit

Fenster **Einstellungen** (Entscheidung 10), gespeichert in `user://einstellungen.cfg`:

| Gruppe | Einstellungen |
|---|---|
| **Bild** | Vollbild oder Fenster, Größe der Oberfläche 75–200 % (Kapitel 22, Abschnitt 7), Bildrate begrenzen |
| **Ton** | Lautstärke Musik, Klänge, gesamt; Musik aus |
| **Spiel** | Autosave (aus, jährlich, halbjährlich), Pausenregeln, historischer Kompass, Hinweise |
| **Barrierefreiheit** | Farben für Farbsehschwäche (Paletten der Kartenmodi mit unterscheidbaren Helligkeiten, Schraffur für Besetzung und Krieg wie heute), hoher Kontrast der Fenster, reduzierte Animationen, Schriftgröße |
| **Tasten** | frei belegbar für Kamera, Tempo, Kartenmodi und Fenster; Zurücksetzen |
| **Sprache** | Deutsch; Englisch (unvollständig) erst sichtbar, wenn die Übersetzung vollständig ist (Abschnitt 7) |

## 5. Präsentation (M13b)

### 5.1 Epochenrahmen und Schriften

Rahmen der Epoche III (Kapitel 22, Abschnitt 5.2: „klassizistische Klarheit, Biedermeier-Ornamente“):
Papierton, feine Doppellinien, Eckornamente und Trennzeichen als **eigene Vektorgrafiken** (SVG im
Repository), Farben aus der bestehenden Palette. Entscheidung 12:

- **Schriften eingebettet** (statt Systemschriften, die unter Windows und macOS anders aussehen), alle
  unter der SIL Open Font License mit Lizenztext im Paket: **Source Serif 4** für Text und Zahlen,
  **Playfair Display** für Überschriften (klassizistisch), **UnifrakturMaguntia** nur für den Kopf der
  Zeitung. Monospace für Tabellen: **Source Code Pro**.
- **Wappen** wie bisher aus Farben; dazu Kartuschen als Rahmen.
- **Keine Porträts und Ereignisbilder** in M13: Eigene Illustrationen sind Teil der Produktion
  (Kapitel 22, Abschnitt 5.3), historische Bilder brauchen eine Rechteprüfung. Ereignisse zeigen eine
  Vignette nach Art (Revolution, Krieg, Diplomatie, Wirtschaft, Hof) aus eigenen Ornamenten.

### 5.2 Zeitung

Titelseite (Kapitel 22, Abschnitt 4: „mehrspaltige Zeitung in Fraktur, Lithografie“) mit einem
**erfundenen Titel** je Region und Richtung (zum Beispiel „Der Rheinische Beobachter“, „Allgemeine
Vaterlandszeitung“), Datum, Schlagzeile, Unterzeile und zwei Spalten Text. Schlagzeilen kommen aus
**Vorlagen** je Anlass in `common/zeitung.chron` (Inhalt des Kerns, chronik-skript.md neu 3.41; die
Brücke liefert die fertige Titelseite zu einem Chronikeintrag) mit Platzhaltern aus den Spieldaten
(`{staat}`, `{gegner}`, `{ort}`, `{sitze}` …); pointiert, aber nie bei Verbrechen und Opfern (Kapitel 22
und 20). Kopf in Fraktur, Text in einer Antiqua (Lesbarkeit vor Stil). Ein Klick schließt; die Zeitungen
einer Partie stehen im Jahrbuch nach.

### 5.3 Musik und Klänge

Entscheidung 13. Grundfassung **ohne fremde Aufnahmen und ohne Samples**:

- **Musik:** sechs bis acht Stücke nach Kapitel 22, Abschnitt 6.1 (Epoche III: Romantik und Volkslieder
  des Vormärz) – **gemeinfreie Werke** (Beethoven, Schubert, Schumann, Mendelssohn; „Die Gedanken sind
  frei“, „Freiheit, die ich meine“), als Notentext im Godot-Projekt (`game/audio/noten/*.noten`, ein
  einfaches Textformat) und mit einem **eigenen Synthesizer** gerendert (`tools/klang/klang.py`,
  Python mit numpy; Klavier, Streicher, Holzbläser als Modelle). Die Musik gehört zur Präsentation,
  nicht zu den Spieldaten. Ergebnis: WAV-Dateien unter `game/audio/musik/`, beim Bauen erzeugt und
  **nicht versioniert** (rund 8 MB je Stück); Godot importiert sie komprimiert (QOA). Das Rendern ist
  deterministisch, ein Test prüft eine Prüfsumme je Stück.
- **Stimmung:** Frieden, Krise (Spiel ab Stufe 2), Krieg, Revolution (Journal in einer revolutionären
  Stufe); Wechsel mit Überblendung, Pausen zwischen den Stücken.
- **Bedienklänge der Epoche III:** Gänsekiel (Befehl), Siegel (Vertrag), Papier (Fenster), Glocke
  (Ereignis), Trommel (Kriegserklärung), ab 1849 Telegraf (Meldung) – ebenfalls synthetisiert.
- **Klangkulisse** je Zoomstufe (Stadt, Land) nicht in M13.

### 5.4 Onboarding

Entscheidung 14:

- **Geschichtsstunde „März 1848 in Berlin“** als Preußen, rund zwölf Schritte mit Anleitung und
  Prüfung des Schritts: Karte und Kartenmodi, Staatsübersicht und Kasse, Steuern, ein Ereignis
  (Märzforderungen) mit Kompass, Kabinett, Diplomatie (ein Gesandter), Zollverein, Krisenleiste,
  Wahl zur Nationalversammlung, Journal, Speichern, Tempo. Die Zeit läuft nur, wenn der Schritt es
  verlangt. Die Schritte stehen als Daten im Godot-Projekt (`game/tutorial/maerz_1848.tres`) mit
  übersetzbaren Texten.
- **Hinweise beim ersten Öffnen** jedes Fensters (zwei, drei Sätze), abschaltbar.
- **„Was ist passiert?“** nach dem Laden: die wichtigsten Einträge der Chronik seit dem letzten Spielen
  des Spielstands (Wendepunkte, Kriege, Wahlen, Amtswechsel), höchstens zehn.

## 6. Lokalisierung (M13b)

Entscheidung 15:

- **Oberfläche:** Alle Texte im GDScript gehen über `tr()` bzw. `tr_n()` mit **Deutsch als
  Quellsprache** (die deutschen Texte sind die Schlüssel); Godot liest gettext-Dateien
  `game/locale/de.po`, `en.po` und die Vorlage `einigkeit.pot`, erzeugt mit Godots POT-Export.
  Platzhalter mit Namen statt Reihenfolge, damit Übersetzungen umstellen können.
- **Englisch vorbereitet:** `en.po` mit dem Startbildschirm, dem Spielmenü und den Einstellungen
  übersetzt; der Rest bleibt deutsch, bis die Übersetzung vollständig ist. Ein **Pseudo-Locale**
  (Texte um 40 % länger, mit Akzenten) prüft im Klicktest, dass keine Beschriftung abgeschnitten wird
  (Kapitel 22, Abschnitt 8).
- **Zahlen und Datum:** `GermanFormat` wird zu `Format` mit Regeln je Sprache (Tausenderpunkt bzw.
  -komma, „1. März 1848“ bzw. „1 March 1848“); Währungen bleiben historisch (Taler, Gulden).
- **Inhalt und Kern bleiben deutsch:** Ereignisse, Journale, Namen und die Texte des Kerns (Chronik,
  Gründe der KI) sind in M13 nur deutsch. Vorbereitet wird nur, dass der Kern statt fertiger Sätze
  einen **Schlüssel mit Werten** liefern kann (für die Gründe der KI und die Chronik zuerst); die
  Umstellung folgt mit Early Access.

## 7. Auslieferung (M13b)

Entscheidung 16:

| System | Paket | Bau |
|---|---|---|
| **Linux** x86_64 | `einigkeit-0.13.x-linux-x86_64.tar.gz` mit Startskript | CI (ubuntu), Godot-Export |
| **Windows** x86_64 | Installer `einigkeit-0.13.x-windows-setup.exe` (Inno Setup) und ZIP | CI (windows, MSVC wie der Job `kern-windows`) |
| **macOS** (arm64 und x86_64) | `einigkeit-0.13.x-macos.dmg` mit App, **unsigniert** (Hinweis zum Öffnen trotz Gatekeeper) | CI (macos, neu: Kern und Erweiterung mit Apple Clang) |

- **Godot-Export** mit `game/export_presets.cfg` und den Exportvorlagen 4.4.1 (in der CI geladen und
  gecacht); Release-Build der Erweiterung je System.
- **Daten neben dem Programm:** `data/` liegt als Ordner im Paket (12 MB); `UhrwerkPaths` sucht in
  exportierten Fassungen neben der Programmdatei (macOS: `Contents/Resources/data`), im Repository
  weiter unter `../data`. Spielstände, Einstellungen und Berichte im Benutzerordner (`user://`).
- **Workflow `pakete.yml`**: manuell und bei Tags `v*`; Artefakte je System; ein **Paketrauchtest** je
  System startet die exportierte Fassung headless mit `--tage=30` und prüft die Prüfsumme gegen die
  CI.
- **Lizenzhinweise:** Das Projekt hat vorerst keine eigene Lizenz (Vorgabe); die Pakete enthalten
  `DRITTANBIETER.txt` mit den Lizenzen der mitgelieferten Fremdteile (Godot und godot-cpp MIT, zstd
  BSD, Schriften OFL), die diese verlangen.
- **Rückmeldung im Spiel:** Menüpunkt „Rückmeldung …“ schreibt ein ZIP in den Benutzerordner mit
  Spielstand, Godot-Protokoll, Version, System und einem Freitext; **nichts wird gesendet**. Die Tester
  geben die Datei selbst weiter (Abschnitt 10).
- **Nicht in M13:** Signieren (Windows), Notarisieren (macOS), automatische Updates, Steam.

## 8. Zustand, Kern und Spielstand

- **Spielstand Format 7:** Spieleinstellungen (Stufe, Verlauf), Bünde zur Laufzeit (Vorlage,
  Mitglieder, Gründung), Wahltermine, Jahresreihen; Spielstände aus M12 laden nicht. Inhaltsbereiche:
  `ki` (Stufen), `bund` (neu: Vorlagen), `wahlen` (Termine), `vermaechtnis` (neu).
- **1648 bitgleich:** Alles Neue gilt nur mit Ereignissen bzw. KI-Daten; die Prüfsumme nach 100
  Jahren bleibt `5db13a66d63e6079`.
- **1848 rechnet anders** (Bünde, Wahlen); alle Labore 1848 werden neu gerechnet, die Korridore der
  deutschen Frage und der KI geprüft (wie M12).
- **Brücke:** neue Methoden für Spieleinstellungen, Bünde, Vermächtnis, Jahresreihen, Gesellschaft,
  Wirtschaft des Staates, Lexikon und Zeitung (bridge-api.md, neuer Abschnitt).

## 9. Godot-Projekt

- **Aufteilung:** `main.gd` (2.530 Zeilen) wird zerlegt: Startbildschirm, Spielablauf (Laden, Tempo,
  Ende), Hauptbildschirm (Leisten), Fensterverwaltung; jedes Fenster eine eigene Szene.
- **Fensterverwaltung:** ein Fenster in der Mitte, Seitenfenster (Ortsinfo, Heer) daneben; Esc schließt
  das oberste; Tastenkürzel je Bereich; die Fenster merken Reiter und Lage je Spiel.
- **Hauptmenü und Spiel** als zwei Szenen; „Hauptmenü“ aus dem Spiel fragt nach dem Speichern.
- Die Tests (Einheiten-, Rauch-, Klick-, Leistungstest) wandern mit; der Klicktest bedient die neuen
  Bereiche.

## 10. Tests und Abnahme

| Kriterium | Prüfung |
|---|---|
| Eine Partie 1848–1871 lässt sich vollständig ohne Fehler spielen | **Durchspieltest** (`game/tests/durchspiel_test.gd`): startet über den Startbildschirm, spielt als Preußen, Österreich, Bayern und Sachsen (je ein Seed) bis zum Urteil der Geschichte, beantwortet Ereignisse über das Fenster (Standardoption, in Preußen die Strategie `krone`), öffnet jedes Fenster einmal je Jahr, speichert und lädt alle fünf Jahre (Prüfsumme gleich), keine `SCRIPT ERROR`, kein `push_error`; in der CI wöchentlich und vor dem Abschluss (rund 15 Minuten je Staat). Dazu einmal von Hand in jeder Paketfassung |
| Spieltest mit externen Testern, Rückmeldungen dokumentiert | **Vorbereitet** in M13b: Leitfaden `docs/pruefung/spieltest_m13.md` (Ablauf, Aufgaben, Beobachtungsbogen, Fragebogen), Rückmeldefunktion im Spiel, Vorlage für die Auswertung. **Durchgeführt** von Menschen (Entscheidung 18): mindestens fünf Tester ohne Projektwissen, je eine Partie oder die Geschichtsstunde und zwei Stunden freies Spiel; Ergebnis in `docs/pruefung/spieltest_m13_ergebnisse.md` mit Befunden und Antworten darauf |
| Leistungsziele aus M3 und M4 mit allen Systemen | **Spieljahr 1848 ≤ 2 s** (Median aus drei Jahren, 4 Kerne, Release; neuer CTest-Bericht `leistung_1848`); **Bildrate:** Leistungstest mit 1848 und laufender Simulation, Wache gegen Rückschritte mit llvmpipe (höchstens 10 % schlechter als M10); **60 Bilder/s** auf einem Mittelklasse-Rechner mit Grafikkarte gemessen im Spieltest (der Cloud-Rechner hat keine) |

Weitere Tests:
- **Einheitentests** für Spieleinstellungen, Stufen, Bünde (Gründung, Beitritt, Bundespflicht,
  Bundesgebiet), Wahltermine, Vermächtnis, Jahresreihen, Spielstand Format 7.
- **Labore:** KI-Labor mit 100 Partien und den Korridoren aus ki.md 7 (Geselle, historisch); mit
  `--stufe lehrling|meister` und `--offen` je 16 Partien ohne Absturz; Ereignislabor und Kriegslabore
  mit Bünden neu.
- **Godot:** Rauchtest der neuen Methoden, Klicktest durch alle Bereiche und Einstellungen,
  Pseudo-Locale ohne abgeschnittene Texte, Screenshots des Startbildschirms, des Hauptbildschirms,
  der Zeitung und des Urteils in der CI.
- **Pakete:** Paketrauchtest je System (Abschnitt 7).

## 11. Entscheidungen (wie empfohlen freigegeben)

1. **Teilung M13a/M13b.** Empfehlung: **ja** (Abschnitt 1): erst die Partie mit Anfang und Ende, dann
   Präsentation und Auslieferung. Alternative: Oberfläche zuerst (früher vorzeigbar, aber ohne Ende
   und ohne Bünde fehlt der Kern des Slice).
2. **Startbildschirm und Lesezeichen.** Empfehlung: **Startbildschirm; 1848 und die drei
   Kriegslesezeichen; 1648 unter „Weitere“ als Technikprototyp** (Abschnitt 3.1). Alternative: 1648
   ganz aus dem Menü nehmen (nur noch über `--szenario`).
3. **Startstaaten.** Empfehlung: **Preußen, Österreich, Bayern, Sachsen empfohlen** (Kapitel 24,
   Phase 2), alle übrigen spielbar mit Hinweis. Alternative: nur die vier spielbar (abgestimmter,
   aber ein Rückschritt gegenüber M7).
4. **Schwierigkeit und Verlauf.** Empfehlung: **drei Stufen als Parameter der Planer** (Lehrling,
   Geselle, Meister) **und zwei Verläufe** (historisch, offen) als Spieleinstellung (Abschnitt 3.2).
   Alternative: nur die Stufen; der offene Modus mit Early Access.
5. **Bünde zur Laufzeit.** Empfehlung: **ja**: Vorlagen, Wirkung `bund`, Bundesgebiet und
   Bundespflicht, Kartenmodus „Bund“ (Abschnitt 3.3). Alternative: Ausgang nur im Urteil der Geschichte
   (weniger Arbeit, aber ein Reich ohne Folgen auf der Karte).
6. **Wahlen bis 1871.** Empfehlung: **Abgeordnetenhaus Preußen zu den historischen Terminen und
   Reichstag eines gegründeten Bundes auf den Wahlkreisen von 1848** (Abschnitt 3.4). Alternative:
   keine weiteren Wahlen (der Parteienmodus endet 1849).
7. **Vermächtnis.** Empfehlung: **fünf Dimensionen am Monatsersten aus dem Zustand**, Formeln als
   Spielwerte in den Daten, in der Leiste und im Urteil (Abschnitt 3.5). Alternative: nur der Ausgang
   der Journale im Urteil.
8. **Ende der Partie.** Empfehlung: **1. Juli 1871** mit Urteil, danach **Weiterspielen** ohne neue
   Ereignisse oder Hauptmenü (Abschnitt 3.5). Alternative: Ende am 18. Januar 1871 (historische
   Kaiserproklamation; die Journale entscheiden dann früher).
9. **Hauptbildschirm.** Empfehlung: **Aufbau nach Kapitel 22** mit Bereichen links, Übersicht rechts
   und Ticker (Abschnitt 4.1). Alternative: das Staatspanel behalten und nur Knöpfe ergänzen.
10. **Fenster und Einstellungen.** Empfehlung: **die Liste aus Abschnitt 4.2 und das Fenster
    Einstellungen mit Barrierefreiheit aus 4.5**; nicht Koalition, Konvent, Parteien, Flotte, Wissen
    (Abschnitt 1). Alternative: Gesellschaft und Wirtschaft erst mit Early Access.
11. **Jahrbuch.** Empfehlung: **Jahresreihen im Kern** (Zustand, Spielstand) (Abschnitt 4.3).
    Alternative: Godot sammelt die Werte selbst (bricht die Grundregel; nach dem Laden fehlt die
    Vergangenheit).
12. **Grafik.** Empfehlung: **eigene Ornamente als SVG, eingebettete OFL-Schriften, keine Porträts und
    Ereignisbilder** (Abschnitt 5.1). Alternative: gemeinfreie Gemälde (Wikimedia) nach
    Rechteprüfung je Bild.
13. **Musik und Klänge.** Empfehlung: **gemeinfreie Werke als eigener Notentext, mit eigenem
    Synthesizer gerendert, keine fremden Aufnahmen oder Samples** (Abschnitt 5.3). Alternative:
    freie Aufnahmen (etwa CC0 von Musopen) nach Rechteprüfung – besserer Klang, aber Prüfaufwand und
    Abhängigkeit.
14. **Onboarding.** Empfehlung: **eine Geschichtsstunde (Preußen, März 1848), Hinweise beim ersten
    Öffnen, „Was ist passiert?“** (Abschnitt 5.4). Alternative: nur Hinweise.
15. **Lokalisierung.** Empfehlung: **gettext mit Deutsch als Quelle, Englisch für Start, Menü und
    Einstellungen, Pseudo-Locale-Test; Inhalt und Kern deutsch** (Abschnitt 6). Alternative: auch die
    Inhaltsdaten mit Schlüsseln (deutlich mehr Arbeit an 109 Ereignissen und den Journalen).
16. **Auslieferung.** Empfehlung: **Linux tar.gz, Windows-Installer und ZIP, macOS-DMG unsigniert;
    Workflow `pakete.yml` mit Paketrauchtest; kein Signieren in M13** (Abschnitt 7). Alternative:
    zuerst nur Linux und Windows (macOS braucht einen neuen CI-Job für den Kern).
17. **Leistung.** Empfehlung: **Spieljahr 1848 ≤ 2 s durch Optimierung, Bildrate als Wache mit
    llvmpipe und gemessen auf echter Hardware im Spieltest** (Abschnitte 3.6, 10). Alternative: das
    Ziel für 1848 auf 3 s setzen.
18. **Spieltest.** Empfehlung: **Leitfaden, Rückmeldefunktion und Auswertung bereitet das Projekt vor;
    die Tests führen Menschen durch**; der Abnahmepunkt bleibt offen, bis die Ergebnisse dokumentiert
    sind (Abschnitt 10). Alternative: nur der automatische Durchspieltest (erfüllt den
    Meilensteinplan nicht).
19. **Spielstand Format 7** mit Spieleinstellungen, Bünden, Wahlterminen und Jahresreihen; alte
    Spielstände laden nicht (Abschnitt 8). Empfehlung: **ja**. Alternative: Migration aus Format 6
    (lohnt vor Early Access nicht).

## 12. Umsetzungsschritte

| Schritt | Inhalt | Prüfung |
|---|---|---|
| a1 | **Start und Einstellungen:** Startbildschirm, neues Spiel in drei Schritten, Befehl `Spieleinstellung`, Stufen in `common/ki.chron`, offener Modus wählbar, KI-Labor `--stufe`/`--offen`; Format 7 | Unit-Tests, Labor je Stufe ohne Absturz, Klicktest |
| a2 | **Bünde zur Laufzeit:** Vorlagen, Wirkung `bund`/`bund_beitritt`, Bundesgebiet, Bundespflicht, Kartenmodus „Bund“; Inhalt in Journalen und Ereignissen | Unit-Tests, Ereignislabor, Korridore der deutschen Frage |
| a3 | **Wahlen bis 1871:** Termine des Abgeordnetenhauses, Reichstag eines Bundes, Parteienmodus und Wahlkampf der KI | Wahllabor je Wahl, Unit-Tests |
| a4 | **Vermächtnis, Jahresreihen, Ende:** `common/vermaechtnis.chron`, Reihen im Zustand, Ende am 1. Juli 1871, Urteil der Geschichte, Chronik als HTML | Unit-Tests, KI-Labor (Verteilung des Vermächtnisses) |
| a5 | **Hauptbildschirm und Fenster:** Leisten, Übersicht, Ticker, Fensterverwaltung, die neuen Fenster aus 4.2, Tooltips, Pausenregeln, Einstellungen; `main.gd` zerlegt. In vier Teilen: **a5a** Fensterverwaltung, Bereichsleiste, Statistisches Jahrbuch; **a5b** Fenster Kabinett und Hof, Gesetze, Parlament, Interessengruppen, Gesellschaft, Wirtschaft (Brücke neu); **a5c** obere Leiste, Staatsübersicht, rechte Übersicht, Ticker mit den Kartenmodi, verschachtelte Tooltips; **a5d** Pausenregeln und Meldungen, Einstellungen mit Barrierefreiheit, Chronik als Vollbild, Lexikon, Wendepunkt, `main.gd` zerlegt | Klicktest, Rauchtest, Screenshots |
| a6 | **Leistung und Durchspieltest:** Profil 1848, Optimierung auf ≤ 2 s je Spieljahr, Leistungstest 1848, Durchspieltest über vier Staaten; alle Labore 1848 neu | Leistung, Durchspieltest, Labore in Korridoren |
| b1 | **Epochenrahmen, Schriften, Zeitung:** Theme III, SVG-Ornamente, eingebettete Schriften, `common/zeitung.chron`, Zeitungsfenster, Vignetten | Screenshots, Klicktest |
| b2 | **Musik und Klänge:** Notenformat, `tools/klang/klang.py`, sechs bis acht Stücke, Bedienklänge, Stimmungswechsel, Lautstärke | Tests des Werkzeugs, `--pruefen`, Rauchtest |
| b3 | **Onboarding:** Geschichtsstunde, Hinweise beim ersten Öffnen, „Was ist passiert?“, Lexikon | Klicktest durch die Geschichtsstunde |
| b4 | **Lokalisierung:** `tr()` überall, POT, `de.po`/`en.po`, Pseudo-Locale, `Format` je Sprache; Schlüssel mit Werten für die Gründe der KI | Klicktest mit Pseudo-Locale, Einheitentests |
| b5 | **Auslieferung:** Exportvorlagen, `export_presets.cfg`, Datenpfad in Paketen, macOS-Job, `pakete.yml`, Installer, DMG, Paketrauchtest, `DRITTANBIETER.txt`, Rückmeldefunktion | Paketrauchtest je System |
| b6 | **Spieltest:** Leitfaden, Fragebogen, Auswertungsvorlage; Durchführung durch Menschen und Auswertung der Rückmeldungen, Befunde beheben | `spieltest_m13_ergebnisse.md` |
| b7 | **Doku und Review** (Abschluss M13 und der Phase 2) | CI grün, Abnahme aus Abschnitt 10 |

## 13. Umsetzungsstand

| Schritt | Stand |
|---|---|
| – | Entscheidungen 1–19 wie empfohlen freigegeben; Umsetzung beginnt mit a1 |
| a5a | **erledigt:** Fensterverwaltung (`window_manager.gd`): Diplomatie, Krieg, Journale und Jahrbuch teilen sich einen Platz, höchstens eines ist offen, Esc schließt das zuletzt geöffnete; die bisherige Platzierung je Fenster in `main.gd` entfällt. Bereichsleiste (`area_bar.gd`) senkrecht rechts neben der linken Spalte (in der Spalte selbst und unter der oberen Leiste fehlte der Platz: das Diplomatiefenster passte mit Krisenleiste nicht mehr über die Kartenmodi) mit Haushalt, Gesetze, Zollunion, Diplomatie, Militär, Journale, Jahrbuch, Chronik; Bereiche ohne Inhalt fehlen, der offene ist gedrückt; die Fensterknöpfe des Staatspanels sind ausgeblendet. Statistisches Jahrbuch (`yearbook_window.gd`, Taste B, Startoption `--jahrbuch`): eine der 17 Spalten (zwölf Kennzahlen, fünf Dimensionen) je Stichtag als Liniendiagramm mit Nulllinie und Tabelle, bis zu drei Staaten zum Vergleich. Leiste der Kartenmodi bricht in zwei Reihen um, wenn sie nicht zwischen die Spalten passt (mit 18 Modi überdeckte sie bei 1.600 Pixeln Chronik und Legende). Tests: Einheitentest (Fensterverwaltung, `--jahrbuch`), Klicktest (Bereiche, Jahrbuch mit Vergleich, Esc, Taste B), Screenshot `einigkeit_jahrbuch.png` in der CI. Neue Fenster der Bereiche Kabinett, Parlament, Gesellschaft, Wirtschaft und Lexikon folgen mit a5b und a5d |
| a4 | **erledigt:** Vermächtnis in `common/vermaechtnis.chron` (chronik-skript.md 3.41): fünf Dimensionen mit Grundwert, Faktoren (Bedingung wie in Ereignissen oder gewichtete Größe aus 21 Kennzahlen, optional begrenzt) und Sätzen; berechnet jederzeit aus dem Zustand (`legacy.hpp`), nicht nur am Monatsersten. Jahresreihen `World::yearbook` (zwölf Kennzahlen und die Dimensionen je Staat) am Start, an jedem 1. Januar und am letzten Tag; Zustand im Spielstand (Format 7, Inhaltsbereich ereignisse Stand 9). Szenariofeld `ende` (chronik-skript.md 3.7) für 1848 und die drei Kriegslesezeichen: am 1. Juli 1871 keine Ereignisse mehr, die Journale wählen ihren Ausgang, Chronik „Ende der Partie“, danach `game_over`. Journalfeld `historisch` (Vergleich im Urteil). Brücke `get_legacy`, `get_yearbook`, `get_verdict`, `is_game_over`; Godot: Fenster „Urteil der Geschichte“ (`verdict_window.gd`) mit Ausgängen, Vermächtnis mit Verlauf als Linie und Satz, Herrscher und Minister, Wendepunkten und Kriegen, Knöpfen Weiterspielen, Chronik exportieren (`chronicle_export.gd`, HTML unter `user://chronik`), Hauptmenü; öffnet am Ende von selbst und aus dem Spielmenü („Urteil“); die Zeit läuft höchstens bis zum letzten Tag; Staatspanel mit der Zeile „Vermächtnis“ und den Faktoren im Tooltip. KI-Labor mit dem Bericht „Vermächtnis am Ende“ (16 Partien, Mittel und Spanne): Preußen Einigkeit 69 (39–92), Geltung 86; Österreich Einigkeit 27; Bayern, Sachsen, Baden dazwischen; Wohlstand streut kaum (50–54). Alle Labore 1848 in ihren Korridoren (Wahl, KI 62,5 % Einigung, Ereignisse). Grenzen: die Jahrbuchansicht (Tabelle und Diagramm) folgt mit a5, Screenshots von Urteil und Zeitung mit b1; Gewichte sind Spielwerte. 1648 bitgleich |
| a3 | **erledigt:** Wahlen bis 1871 ([politik.md](politik.md) 5.4): Parlamente mit weiteren Wahltagen (`wiederwahl`) und Parlamente eines Bundes (`bund`, `nach_gruendung`, `wahlperiode`, `wahlkreise_von`; chronik-skript.md 3.21). Preußische Zweite Kammer zu neun Urwahltagen 1852–1870 (Dreiklassen, Wahlkreise 1849; Wahlkreise, deren erster Ort Preußen verloren hat, wählen nicht), **Reichstag** jedes Bundes aus vier Vorlagen 90 Tage nach der Gründung und dann alle drei Jahre, allgemeines direktes Männerwahlrecht in den Wahlkreisen der Nationalversammlung im Bundesgebiet, sechs Fraktionen. Kern: `next_election_day` und `district_votes` für Simulation, Wahlkampf, KI-Wahlkampf und Brücke; jede weitere Wahl desselben Parlaments mischt die Zahl der früheren Wahlen in den Zufall, die erste rechnet wie bisher (Wahlziele 1848/49 unverändert erfüllt). Brücke: `get_parliaments` mit `pending`, `federal`, `last_election_date`, Wahltag als nächster ausstehender; `get_campaign`/Ortsinfo nach `district_votes`. Godot: Wahltage einmal im Monat neu (ein Bund kann entstehen), Legende mit dem Jahr der gezeigten Wahl. Probe: Seed 1655 gründet das Reich der Paulskirche am 28.4.1849, der Reichstag wählt am 27.7.1849 mit 332 Sitzen; Seed 1648 wählt das Abgeordnetenhaus 1849–1870 zehnmal. Labore 1848 mit 16 Seeds in ihren Korridoren: Wahllabor (auch Preußen 1849), KI-Labor Einigung 62,5 %, Ereignislabore. Grenzen: Ein Ereignis löst keine Neuwahl aus (1862/63 als feste Termine), andere Landtage wählen nicht, das Zentrum fehlt als Fraktion. 1648 bitgleich |
| a2 | **erledigt:** Bünde zur Laufzeit: Vorlagen (`vorlage = ja`, Name des Bundes `bund`) in `common/bundesversammlung.chron` – Norddeutscher Bund (auch Kriegslesezeichen Sedan), Reich der Paulskirche, großdeutsches Reich, Deutsches Reich 1871 –, Wirkungen `bund { … }` und `bund_beitritt` (chronik-skript.md 3.29, 3.34), Zustand `World::federation` (Vorlage, Mitglieder, Gründung), `active_diet`, `in_federal_area`, `federal_state` nach den Mitgliedern, Bundespflicht in `can_declare_war`, Durchzug über das Bundesgebiet, das gilt; Inhaltsbereich ereignisse Stand 8. Inhalt: Reichsverfassung (Revolution), Norddeutscher Bund (1867), Novemberverträge (Beitritt), Kaiserproklamation (Reich). Journal „Die deutsche Frage“: „Der Deutsche Krieg“ nur ohne gleichzeitigen Krieg gegen Dänemark (die KI verbündete 1848 Österreich mit Dänemark, das Journal sprang im März 1848 in den Krieg), ein gegründetes Reich beantwortet die Frage auch in dieser Stufe. Brücke: `get_diet` mit `federation_name` und `founded`, `get_state.bund` und `bundesgebiet` nach dem Bund, der gilt, Kartenmodus `MAP_BUND`; Godot: Kartenmodus „Bund“ (Taste C) mit Legende. Probe (Seed 1648): Norddeutscher Bund 1867 mit 29 Staaten, Beitritt Bayerns, Württembergs und Hessens 1870, Deutsches Reich am 1. März 1871 mit 32 Staaten. Labore 1848 mit 16 Seeds in ihren Korridoren: KI-Labor Einigung 56,3 % (kleindeutsch 50 %, zwei Bünde 6,3 %, Bund 43,8 %), Krieg Preußen–Österreich 50 %; Ereignislabor 1871 ebenso, Revolution ohne Spieler und mit krone/reaktion. 1648 bitgleich |
| a1 | **erledigt:** Startbildschirm (`game/scripts/ui/start_screen.gd`) mit Hauptmenü (Neues Spiel, Fortsetzen mit dem jüngsten Spielstand, Laden, Beenden; Geschichtsstunde und Einstellungen gesperrt bis b3 bzw. a5), neues Spiel in drei Schritten (Lesezeichen mit 1648 unter „Weitere“, Staat mit den vier empfohlenen und allen übrigen, Spieleinstellungen); hinter ihm ruhen Leisten und Zeit. Stufen Lehrling, Geselle (Standard, rechnet wie M12) und Meister in `common/ki.chron` (Block `stufe`, `standard_stufe`; chronik-skript.md 3.37), angewandt über `effective_ai_rules`; offener Verlauf wie `agenda_offen`. Befehl `Spieleinstellung` nur am ersten Tag; Zustand und Spielstand **Format 7**, Inhaltsbereich ki Stand 5. Brücke `get_game_settings`/`set_game_settings`; KI-Labor `--stufe`/`--offen`, CTests `ki_1848_lehrling` und `ki_1848_meister_offen`; Startoption `--startbildschirm[=lesezeichen|einstellungen]` und zwei Screenshots in der CI. Probe 1848–1850 (2 Seeds): Lehrling 84/82 Entscheidungen, Geselle 112/128 (wie M12), Meister 125/182, offen 141/141. Die empfohlenen Staaten und Texte der Lesezeichen stehen vorerst im Startbildschirm (Präsentation); 1648 bitgleich |
