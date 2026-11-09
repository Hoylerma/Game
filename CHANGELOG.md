# Änderungsprotokoll

Alle nennenswerten Änderungen an EINIGKEIT werden in dieser Datei festgehalten.

Das Format basiert auf [Keep a Changelog](https://keepachangelog.com/de/1.1.0/),
die Versionsnummern folgen [Semantic Versioning](https://semver.org/lang/de/).
Vor Version 1.0 kann sich jede Schnittstelle ändern.

## [Unveröffentlicht]

### Meilenstein M13 – Spielbarer Vertical Slice 1848–1871

Vertrag: [technik/vertical_slice.md](docs/technik/vertical_slice.md), Entscheidungen 1–19 wie empfohlen
freigegeben; Stand je Schritt in Abschnitt 13 dort.

- **Einstellungen, Pausenregeln, Chronik, Lexikon (Schritt a5d):** Ein Fenster **Einstellungen** (aus
  Startbildschirm und Spielmenü) speichert Bild, Ton, Spiel, Barrierefreiheit, Tasten und Sprache: unter
  anderem halbjährliche Spielstände, **Pausenregeln** (Krieg, Forderung, Wahl, Amtswechsel, Bundesgründung,
  Frieden), **Farben für Rot-Grün-Schwäche**, hoher Kontrast, größere Schrift und frei belegbare Tasten. Die
  **Chronik** ist ein Fenster mit Filter und Suche, das neue **Lexikon** erklärt 26 Begriffe der Epoche und
  öffnet sich auch aus den Tooltips. **Wendepunkte** erscheinen auf einem großen Bildschirm. `main.gd` ist in
  drei Ebenen und drei Hilfsklassen zerlegt.
- **Leisten und Tooltips (Schritt a5c):** Die obere Leiste zeigt Kasse mit Saldo, Legitimität, Verwaltung und
  das Vermächtnis; jede Zahl hat einen **verschachtelten Tooltip** (Einnahmen je Steuer, Zustimmung je Gruppe
  und Ursachen der Unruhe, Faktoren je Dimension), unterstrichene Begriffe erklärt ein zweiter Tooltip. Rechts
  steht eine **Übersicht** der laufenden Dinge (Ereignisse, Krisen, Kriege, Journale, nächste Wahl, Heere auf
  dem Marsch), unten ein **Ticker** mit dem Neuesten aus der Chronik, Wendepunkte zuerst. Das Staatspanel
  heißt Staatsübersicht.
- **Fenster der Bereiche (Schritt a5b):** Die Bereichsleiste öffnet sechs neue Fenster: **Kabinett und Hof**
  (Herrscher und Minister mit Werten und Eigenschaften, Neigungen des Staates, frühere Amtsinhaber der
  Partie), **Gesetze** (Verfassung, Wahlrecht, jede Gesetzesoption mit ihrer Wirkung auf die Unruhe und den
  Gruppen dafür und dagegen), **Interessengruppen** (Anteil, Einfluss, Zustimmung, Forderungen), **Parlament**
  (Halbrund der letzten Wahl nach Fraktionen, nächste Wahl, Abstimmungen), **Gesellschaft** (Altersaufbau,
  Kulturen, Konfessionen, Berufe, Wanderung) und **Wirtschaft** (Güter mit Preisen, Betriebe, Märkte,
  Zollgebiet). 1648 bitgleich.
- **Bereiche und Jahrbuch (Schritt a5a):** Eine **Bereichsleiste** neben dem Staatspanel öffnet Haushalt,
  Gesetze, Zollunion, Diplomatie, Militär, Journale und das neue **Statistische Jahrbuch** (Taste B) und
  blendet die Chronik ein und aus. Das Jahrbuch zeigt jede Kennzahl und jede Dimension des Vermächtnisses
  je Jahr als Diagramm und Tabelle, mit bis zu drei Staaten zum Vergleich. Eine **Fensterverwaltung** hält
  höchstens ein Fenster offen; Esc schließt das zuletzt geöffnete. Die Leiste der Kartenmodi bricht in zwei
  Reihen um, statt Chronik und Legende zu überdecken.
- **Vermächtnis, Jahresreihen und Ende (Schritt a4):** Jeder Staat hat ein **Vermächtnis** in fünf
  Dimensionen – Einigkeit, Recht, Freiheit, Wohlstand, Geltung – von 0 bis 100, berechnet aus Faktoren in
  `common/vermaechtnis.chron` (Bedingungen wie in Ereignissen oder gewichtete Kennzahlen wie Unruhe,
  Lebensstandard, Stärke, Bündnisse, Siege). Das Staatspanel zeigt die Werte mit den Faktoren im Tooltip.
  Der Kern schreibt **Jahresreihen** (zwölf Kennzahlen und das Vermächtnis je Staat) am Start, an jedem
  1. Januar und am letzten Tag in den Spielstand. Die Lesezeichen ab 1848 **enden am 1. Juli 1871**
  (Szenariofeld `ende`): Die Journale wählen ihren Ausgang, dann öffnet das **Urteil der Geschichte** mit
  dem Ausgang der deutschen Frage und der Revolution samt historischem Vergleich, dem Vermächtnis mit
  Verlauf und Satz, den Wendepunkten, Kriegen, Herrscher und Minister. Weiterspielen geht ohne neue
  Ereignisse; „Chronik exportieren“ schreibt die Chronik als HTML-Datei in den Benutzerordner. Das
  KI-Labor berichtet die Verteilung des Vermächtnisses. 1648 bitgleich.
- **Wahlen bis 1871 (Schritt a3):** Nach 1849 wird weiter gewählt. Das **preußische Abgeordnetenhaus**
  wählt an den Urwahltagen 1852 bis 1870 mit dem Dreiklassenwahlrecht in den Wahlkreisen von 1849 (neues
  Feld `wiederwahl`). Gründet sich ein Bund, wählt drei Monate später der **Reichstag** nach allgemeinem,
  direktem Männerwahlrecht in den Wahlkreisen der Nationalversammlung im Bundesgebiet, danach alle drei
  Jahre (Felder `bund`, `nach_gruendung`, `wahlperiode`, `wahlkreise_von`). Jede Wahl öffnet den Wahlabend;
  Parteienmodus und Wahlkampf der KI gelten bis zur nächsten ausstehenden Wahl. Ein Landtag wählt nur, wo
  sein Staat noch herrscht. Die erste Wahl jedes Parlaments rechnet wie bisher; 1648 bitgleich.
- **Bünde zur Laufzeit (Schritt a2):** Die deutsche Frage verändert jetzt die Ordnung im Bund. Die
  Bundesversammlungen in `common/bundesversammlung.chron` können **Vorlagen** sein (Norddeutscher Bund,
  Reich der Paulskirche, großdeutsches Reich, Deutsches Reich 1871); die neue Wirkung `bund { … }` gründet
  daraus einen Bund mit Mitgliedern (nach Bundesgebiet, bisherigem Bund, Merkmal oder Liste), `bund_beitritt`
  lässt einen Staat beitreten. Dann gelten seine Mitglieder als Bundesstaaten, ihre Orte als Bundesgebiet,
  seine Stimmen und sein Präsidium in der Bundesversammlung, und Mitglieder führen keinen Krieg gegeneinander.
  In einer Partie ab 1848 gründen die Reichsverfassung, der Norddeutsche Bund (1867), die Novemberverträge und
  die Kaiserproklamation ihre Bünde. Neuer Kartenmodus **„Bund“** (Taste C). Das Journal „Die deutsche Frage“
  springt nicht mehr im März 1848 in „Der Deutsche Krieg“, wenn Österreich an Dänemarks Seite steht. Alle
  Labore 1848 halten ihre Korridore; 1648 bitgleich.
- **Start und Spieleinstellungen (Schritt a1):** Das Spiel beginnt mit einem **Startbildschirm** statt
  mit 1648: Neues Spiel, Fortsetzen (jüngster Spielstand), Laden, Beenden. Ein neues Spiel führt über
  Lesezeichen (1848 und die drei Kriegslesezeichen, 1648 als Technikprototyp unter „Weitere“), Staat
  (Preußen, Österreich, Bayern und Sachsen empfohlen, mit Schwierigkeit; alle übrigen wählbar) und
  **Spieleinstellungen**: Schwierigkeit Lehrling, Geselle oder Meister (Faktoren auf Schwelle, Rauschen
  und Abkühlungen der KI, Meister verbündet sich gegen einen starken Spieler; keine Boni) und
  Geschichtsverlauf historisch oder offen. Neuer Block `stufe` in `common/ki.chron`, Befehl
  `Spieleinstellung` am ersten Tag, Spielstand **Format 7** (Spielstände aus M12 laden nicht). Geselle
  rechnet wie M12. KI-Labor mit `--stufe` und `--offen`; Godot-Startoption `--startbildschirm`. 1648
  bitgleich.

### Meilenstein M12 – Künstliche Intelligenz der Staaten

Vertrag: [technik/ki.md](docs/technik/ki.md), Entscheidungen 1–14 wie empfohlen freigegeben; Stand je
Schritt in Abschnitt 13 dort.

- **Datenmodell (Schritt a1):** Neue Datei `common/ki.chron` ([chronik-skript.md](docs/technik/chronik-skript.md)
  Abschnitt 3.37) mit den Regeln der KI ab 1848, sechs Staatspersönlichkeiten (Machtpolitiker, Bewahrer,
  Reformer, Händler, Hegemon, Gleichgewichtswahrer) und 16 Eigenschaften aus Kapitel 13, je mit Beitrag zu
  sechs Neigungen (Risiko, Expansion, Legitimität, Reform, Sparsamkeit, Vermittlung). Lader für Personen
  mit Amt, Amtszeit, Bedingung, Werten und Eigenschaften (`common/persoenlichkeiten.chron`, 3.38) und für
  Agenden mit Staatspersönlichkeit und Zielen (`common/agenden.chron`, 3.39). Die Ämter (Herrscher,
  leitender Minister) werden beim Start besetzt und am Monatsersten nach Datum und Bedingung neu
  bestimmt; ein Wechsel steht in der Chronik. Der Befehl `Ereignisspieler` heißt jetzt `Spielerstaat`.
  Inhaltsbereich `ki`, Spielstand **Format 6** (Spielstände aus M11 laden nicht mehr); `uhrwerk-cli info`
  mit Zeile „KI“. Personen und Agenden 1848–1871 folgen mit Schritt b1; 1648 bitgleich.
- **Rahmen der Planer, Haushalt und Mobilmachung (Schritt a2):** Am Monatsersten bestimmt die KI für
  jeden bestehenden Staat außer dem Spielerstaat Hauptziel und Rivalen (größte Bedrohung aus Stärke,
  Feindschaft und Nähe) und rechnet ihre Planer. Jede Option ist eine Summe benannter Faktoren mit
  Rauschen aus dem Strom `Ki`; über der Schwelle gibt die KI denselben Befehl wie ein Spieler (ohne
  Eintrag im Befehlsprotokoll) und schreibt ihr KI-Protokoll mit den drei wichtigsten Gründen
  („Schuldenquote +30“). **Haushalt:** Kontribution oder Akzise steigen bei Defizit und hoher
  Schuldenquote und sinken bei großem Überschuss (erst mit einer Rücklage von einer Jahreseinnahme)
  oder hoher Unruhe, mit drei Monaten Abkühlung.
  **Mobilmachung:** gegen einen bedrohlichen, feindlichen mobilen Nachbarn; Abrüstung nach zwei
  ruhigen Monatsersten (bisher blieben Staaten nach Kriegen mobil). Inhaltsbereich `ki` Stand 2;
  1648 bitgleich.
- **Diplomatie und Politik der KI (Schritt a3):** Staaten ohne Spieler entsenden Gesandte (Agenda,
  Rivale, Verbündete, Nachbarn, Großmächte), bieten Bündnisse gegen einen gemeinsamen Rivalen an und
  nehmen sie nach Nutzen an (statt „ab Beziehung 100“), garantieren kleine Nachbarn und entscheiden in
  Krisen nach Stärke und Persönlichkeit (nachgeben, vermitteln, eskalieren). Forderungen, Anträge in
  der Bundesversammlung und Beitritte zu Zollunionen folgen der Agenda (Daten mit b1).
  Ereignisoptionen dürfen einen Block `ki { risiko = 1 … }` tragen, der ihr Gewicht nach den Neigungen
  des Staates verschiebt. Strömungen ohne Spieler führen in den vier Monaten vor einer Wahl Wahlkampf
  in den knappsten Wahlkreisen; dafür kennt der Kern die Strömung des Spielers (Befehl
  `Spielerströmung`). Inhaltsbereiche ki Stand 3 und ereignisse Stand 7; 1648 bitgleich.
- **Krieg und Frieden der KI (Schritt a4):** Staaten ohne Spieler erklären Krieg nur aus einem aktiven
  Ziel ihrer Agenda (Gebiet, Annexion, Schwächen eines Rivalen) und nur bei Übermacht ihrer Seite
  nach dem Bündnisfall, mit doppelter Schwelle und Gründen in der Chronik; Taktiker warten, bis der
  Gegner allein steht. Im Krieg stellen sie bei klarem Vorsprung weitere Ziele aus der Agenda und bieten
  Frieden (eigene Ziele, weißer Frieden, Kapitulation) nur an, wenn die Gegenseite annähme. Bündnisse
  und Garantien bietet die KI im Krieg nicht mehr an: Ein Bündnis mitten im Krieg machte den Partner
  zur eigenen Seite, und im Lesezeichen Königgrätz siegte Preußen mit a3 nur noch in 3 von 16 Läufen
  (jetzt wieder 11). Einen weißen Frieden nimmt eine Seite ohne Spieler nicht an, solange ihr Gegner
  kriegsmüder oder ihre Seite klar überlegen ist; sonst bot Frankreich im Lesezeichen Sedan früh weißen
  Frieden an, und der Verteidiger siegte nur in 4 von 16 Läufen (jetzt 14). Inhaltsbereich ki Stand 4;
  1648 bitgleich.
- **Review und Abschluss (Schritt b4):** Review über Kern, Brücke und Godot, Daten und Doku; die
  Einheitentests und das KI-Labor 1848/49 laufen unter ASan/UBSan, das KI-Labor parallel unter TSan,
  ohne Befund. Behoben:
  bridge-api.md nannte für Welten ohne KI eine falsche Rückgabe von `get_intentions`; die Gründe in der
  Chronik kommen ohne den ungenutzten Parameter aus dem Protokoll; die Verweise auf die Abschnitte M11 und
  M12 im Meilensteinplan zeigen wieder auf ihr Ziel. Meilensteinplan mit Ergebnis, Abnahme (100 Partien ohne Absturz, Einigung 67 %,
  kleindeutsch 44 %, zwei Bünde 23 %, Bund 33 %) und bekannten Grenzen. **M12 ist abgeschlossen.**
- **Brücke und Godot (Schritt b3):** Neue Brückenmethoden `get_state_persons`, `get_intentions` und
  `get_ai_decisions` ([bridge-api.md](docs/technik/bridge-api.md)). Die **Staatsinfo** zeigt Herrscher
  und leitenden Minister (Eigenschaften und Werte im Tooltip) und die sechs Neigungen des Staates mit
  seiner Staatspersönlichkeit. Das **Diplomatiefenster** zeigt für den gewählten Staat die Absichten
  (Hauptziel, Rivale, aktive Ziele) – nur mit eigenem Gesandten dort – und seine letzten drei
  Entscheidungen mit „Warum?“ (Gründe im Tooltip). Forderungen der KI schreiben ihre Gründe jetzt auch in
  die Chronik; Kriegserklärungen und Forderungen an den Spieler erscheinen zusätzlich als Hinweis in der
  Kopfleiste. Krisenleiste und Kriegsfenster nennen im Tooltip die Gründe der Gegenseite. Rauch- und
  Klicktest prüfen Personen, Absichten und „Warum?“ 1848; 1648 bitgleich.
- **Kalibrierung mit 100 Partien (Schritt b2):** Korridore `common/kiziele_voelkerfruehling.chron` (für
  16 Partien je Push `kiziele_voelkerfruehling_16.chron`), CI-Schritt „KI-Labor 1848–1871“ und Workflow
  `ki-partien.yml` mit 100 Partien. Ergebnis: Einigung 67 %, kleindeutsch 44 %, zwei Bünde 23 %, Bund
  33 %, Krieg Preußen–Österreich 70 %, keine Abstürze. Dafür: Die KI berechnet die Stärken einmal je
  Monatserstem (von 45 % auf 4 % der Rechenzeit, bitgleich); Bündnisse gegen einen gemeinsamen Rivalen nur
  noch, wenn er ein Gegner ist (19 statt 55 Bündnisse); Taktiker wie Bismarck erklären den Krieg, wenn die
  Gegenseite nicht mehr Staaten hat (vorher erklärte die KI nie); die Neigungen an Ereignisoptionen wirken
  halb so stark und Vermittlung nur noch außenpolitisch (sonst scheiterte die Revolution zu oft und Olmütz
  endete in jedem vierten Lauf im Krieg); die KI beantragt keine Zolleinigung mehr (sie hatte 1851 alle
  Bundesstaaten samt Österreich in den Zollverein geführt) und fordert Zollbeitritte nur von Mitgliedern
  einer anderen Union. Das Zolllabor rechnet ohne KI. Alle Labore 1848 halten ihre Korridore.
- **Daten 1848–1871 (Schritt b1):** 71 Herrscher und leitende Minister von Preußen, Österreich,
  Bayern, Sachsen, Hannover, Württemberg, Baden, Hessen-Darmstadt, Kurhessen, Frankreich und Dänemark
  (`common/persoenlichkeiten.chron`) mit Amtszeit, Werten und Eigenschaften; die Wechsel der Revolution
  folgen den Ereignissen (Märzministerien, Metternich, Abdankung Ludwigs I., Bismarck). Staats-
  persönlichkeiten und Agenden von 14 Staaten (`common/agenden.chron`): Zollverein, Führung im Bund,
  Bündnisse der Mittelstaaten, Schleswig 1864 und das Schwächen Österreichs und Frankreichs nur mit
  Bismarck. 35 Ereignisoptionen tragen einen Block `ki { … }`. Forderungen der KI höchstens eine je
  Abkühlung und Zollforderungen nur an Bundesstaaten (vorher forderte Preußen alle zwei Monate von einem
  anderen Nachbarn den Beitritt zum Zollverein, auch von den Niederlanden); die Zolleinigung beantragt nur
  noch die Leitmacht, und Zoll- und Führungsziele gelten erst nach der Revolution. Erstes KI-Labor mit
  16 Partien bis 1871: Einigung 75 %, kleindeutsch 31 %, zwei Bünde 44 %, Bund 25 %, keine Abstürze.
  Kalibrierung folgt mit b2.
- **KI-Labor (Schritt a5):** `uhrwerk-cli ki` rechnet Partien ohne Spieler bis `--bis` und berichtet je
  Lauf Ausgang der deutschen Frage, Kriege, untergegangene Staaten, Bankrotte, höchste Unruhe,
  Entscheidungen der KI je Art, Laufzeit mit dem Anteil der KI und Prüfsumme; über alle Läufe Einigung,
  Form der Einigung, Kriege je Paar und Abstürze gegen die Korridore aus `common/kiziele_<szenario>.chron`
  ([chronik-skript.md](docs/technik/chronik-skript.md) Abschnitt 3.40). `--parallel N` rechnet N Läufe
  gleichzeitig mit denselben Prüfsummen, `--ohne-ki` zum Vergleich wie M11; ein Absturz zählt, der
  Bericht läuft weiter. CTests auf der Fixture `tests/daten_ki`; die Korridore für 1848 folgen mit der
  Kalibrierung in b2. In der Hilfe stehen jetzt auch `--strategie` und `--bis`. Das erste Labor 1848
  zeigte einen Fehler der KI-Diplomatie: Die Grenze von drei Bündnissen galt nur für den Anbietenden,
  und am 2. März 1848 entstanden 36 Bündnisse auf einmal (Ende 1849: 91). Jetzt lehnt ein Staat ohne
  Spieler ab, wenn er oder der Anbietende die Grenze erreicht hat. Rauch- und Klicktest rechnen jetzt
  damit, dass Staaten und Strömungen ohne Spieler handeln: Der Rauchtest meldet Preußen als
  Spielerstaat und die Liberalen als Spielerströmung, der Klicktest erwartet die Gesandten und
  Wahlkreise, die die KI vor der Übernahme schon eingesetzt hat.

### Meilenstein M11 – Ereignisse und Revolution 1848/49

Vertrag: [technik/ereignisse.md](docs/technik/ereignisse.md), Entscheidungen 1–13 wie empfohlen
freigegeben; Stand je Schritt in Abschnitt 13 dort.

- **Datenmodell (Schritt a1):** Neue Datei `common/gesetze.chron`
  ([chronik-skript.md](docs/technik/chronik-skript.md) Abschnitt 3.33) mit drei Gesetzesgruppen
  (Presse, Vereine, Grundlasten); die Staaten tragen ihre Option datiert im Feld `gesetz_<gruppe>`
  (`staaten_historie.csv`, 82 Zeilen: Grundherrschaft und Ablösung 1848, preußisches Vereins- und
  Pressegesetz 1850/51, Bundesgesetze 1854), die Interessengruppen ihre Wünsche (`wuensche`, 3.20).
  Neue Regeldatei `common/ereignisse.chron` (Ereignisse ab 1848.01.01, Frist 30 Tage) und der Lader der
  Ereignisse in `common/ereignisse/*.chron` (3.34): Titel und mehrzeilige Texte, Art, Quelle,
  Wartezeit oder Termin, Wiederholung, Optionen mit Gewichten und Zuschlägen, ein festes Vokabular von
  Bedingungen und Wirkungen; Verweise auf späteren Inhalt sind nie wahr bzw. entfallen, Folgen ohne
  Frist dürfen keinen Kreis bilden. Zustand in Welten mit Ereignissen: Verfassung, Wahlrecht und
  Gesetze je Staat, Ereignischronik, offene und geplante Ereignisse, Merkmale, Zustimmung je Staat und
  Gruppe, Unruhe je Staat. Inhaltsbereich `ereignisse`, Spielstand **Format 5** (Spielstände aus M10
  laden nicht mehr); `uhrwerk-cli info` zählt Ereignisse je Art und Jahrzehnt und die Gesetze zum
  Start, `pruefen` die Gesetzesgruppen und Ereignisse. Noch ohne Auslösung und ohne Ereignisse im
  Inhalt; 1648 bitgleich.
- **Auslösung und Wahl (Schritt a2):** Täglich nach dem Militär treten Ereignisse ein, deren Bedingung
  gilt: sofort, mit mittlerer Wartezeit (Strom `Ereignisse`) oder am Termin, je Staat einmal oder
  wiederholt mit Abstand. Staaten ohne Spieler wählen nach Gewicht mit Zuschlägen; Ereignisse des
  Spielerstaates warten (neue Befehle `Ereignisspieler` und `Ereigniswahl`), ohne Wahl gilt nach der
  Frist die Option mit dem größten Gewicht. Folgeereignisse mit und ohne Frist (`nur_folge`), alle
  Wirkungen aus [ereignisse.md](docs/technik/ereignisse.md) 3.3 bis auf Aufstände, Journale und Kriege,
  befristete Wirkungen (`befristet`: Steuerertrag, Moral, Unruhe, Beziehung) und Chronikeinträge der
  gewählten Optionen. Die Testbefehle der Kommandozeile üben die neuen Befehle; 1648 bitgleich.
- **Zustimmung, Unruhe, Aufstände (Schritt a3):** Am Monatsersten nähern sich die Zustimmung der
  Gruppen ihrem Zielwert aus den Gesetzen und die Unruhe ihrem Zielwert aus Not, Teuerung des Getreides,
  Verfassung, Zensur (`unruhe` an den Gesetzesoptionen), Ansteckung und Zustimmung. Ab Unruhe 50
  verweigern die Untertanen einen Teil der Steuern und wandern mehr aus. Aufstände (`aufstand =
  barrikaden|agrarunruhen|erhebung`) vergleichen jede Woche Mobilisierung und Elitenspaltung mit der
  Loyalität der Truppen und einer Intervention (`intervention = TAG`) und enden mit Nachgeben,
  Niederschlagung oder Sieg der Aufständischen, je mit Chronik und optionalem Folgeereignis. Alle
  Faktoren sind Spielwerte in `common/ereignisse.chron`; 1648 bitgleich.
- **Journale und Abstimmungen (Schritt a4):** Journale (`common/journale/*.chron`) als Zustandsautomaten
  mit Stufen, Zielen, Übergängen und Ausgängen mit Wertung; Abstimmungen
  (`common/abstimmungen.chron`) nach Fraktionen mit den Sitzen der letzten Wahl und Wirkungen bei
  Annahme oder Ablehnung; die Bundesversammlung kann ruhen; Ereignisse stellen Anträge in der
  Bundesversammlung und erklären Kriege (`krieg = TAG`). Neue Bedingungen `journal_<id>`,
  `abstimmung_<id>`, `bevoelkerungsanteil`, `staaten`, `bundesversammlung`; `info` zählt Journale.
  1648 bitgleich.
- **Ereignislabor (Schritt a5):** `uhrwerk-cli ereignisse` rechnet ein Lesezeichen je Seed ohne
  Spieler oder mit einer Strategie für einen Staat (`--strategie`) bis zum Ende der Korridore und
  berichtet Ausgang und Weg des Journals, Ereignisse, Wendepunkte, Aufstände, Unruhe und Gesetze;
  Korridore in `common/ereignisziele_<szenario>.chron`, Exitcode 1 bei Verstößen. Beendete Aufstände
  stehen im Zustand. Eine kleine Revolution auf der Test-Fixture (`tests/daten_ereignisse`) mit drei
  CTests; 1648 bitgleich.
- **Inhalt 1848–1851 (Schritt b1):** 64 Ereignisse der Revolution von der Februarrevolution bis zum
  Silvesterpatent (Märzforderungen in jedem Staat, Barrikaden in Berlin, Metternichs Sturz, Heckerzug,
  Posen, Schleswig-Holstein, Paulskirche, Malmö, Wiener Oktober, Ministerium Brandenburg, Grundrechte,
  Kaiserdeputation, Maiaufstände, Badische Revolution, Rumpfparlament, Dreiklassenwahlrecht, Union,
  Olmütz, Bundesreaktion), das Journal „Revolution 1848/49“ mit sieben Stufen und fünf Ausgängen und
  acht Abstimmungen der Paulskirche nach Fraktionen. Gewalt erscheint als Geschehen in Text und Chronik,
  nie als Belohnung. Im Ereignislabor (16 Seeds, vorläufige Korridore) endet die Revolution ohne Spieler
  zu 75 % mit der Gegenrevolution, zu 19 % im Reich der Paulskirche und zu 6 % in der Union; mit der
  Strategie `krone` für Preußen immer im Reich, mit `reaktion` immer in der Gegenrevolution. Ein Journal
  beginnt nicht mehr, wenn sein Ende vor dem Start liegt (Lesezeichen 1864–1870). CI-Schritt
  „Ereignislabor 1848“, CTest `ereignisse_1848`. Rauch- und Klicktest kennen die Revolution: Preußen
  darf mobil sein (Eingreifen gegen Aufstände), der Antrag im Diplomatiefenster kommt vor dem Juli 1848,
  in dem die Bundesversammlung zu ruhen beginnt; 1648 bitgleich.
- **Kalibrierung 1848 (Schritt b2):** Mit dem Ereignislabor (16 Seeds wie die CI) geben mehr Staaten
  im März nach (gewähren 75, hinhalten 15, Militär 10), Preußen nimmt die Kaiserkrone öfter an
  (annehmen 20, ablehnen 50, Bedingungen 30) und hält in Olmütz öfter stand (30). Hält es stand,
  antwortet Österreich mit einem eigenen Ereignis: Krieg oder die Union hinnehmen; nur das Einlenken
  führt zum Ausgang „Union“. Ohne Spieler endet die Revolution über 64 Läufe zu 69 % mit der
  Gegenrevolution, zu 22 % im Reich der Paulskirche und zu 9 % in der Union (mit den 16 Seeds der CI
  88 %, 6 %, 6 %); Märzministerien bis Ende April in 76 % der Staaten mit dem Ereignis. Korridore auf die Zielwerte aus ereignisse.md Abschnitt 7 (Märzministerien
  mindestens 70 %, Union höchstens 20 %). Balancing-Bericht, Wahl-, Zoll- und Kriegslabor 1848 bleiben
  mit Ereignissen in ihren Zielwerten; 1648 bitgleich.
- **Inhalt 1852–1871 und die deutsche Frage (Schritt b3):** 33 Ereignisse von der Zollvereinskrise
  über Neue Ära, Heereskonflikt und „Blut und Eisen“ bis zu den Kriegen von 1864 und 1866, dem
  Norddeutschen Bund, der Emser Depesche, den Novemberverträgen und der Kaiserproklamation; elf
  wiederholbare Ereignisse aus dem Zustand (Missernte, Hungerkrawall, Cholera, Auswanderung,
  Weberelend, Eisenbahn, Streik, Pressprozess, Turn- und Sängerfest). Zusammen 109 Ereignisse, alle mit
  Quelle (`info`: „mit Quelle“, CTest `ereignisse_quellen`). Neues Journal „Die deutsche Frage“ bis
  1871 mit fünf Ausgängen und eigener Zieldatei für das Labor (`--ziele …_1871.chron`). Kern: Bedingungen
  `krieg_gewonnen`/`krieg_verloren`, Kriegserklärung mit Kriegszielen `krieg { gegner = … gebiet = …
  fuehrung = ja … }` (ohne Ziel endete jeder Ereigniskrieg ohne Gewinner), `mobilmachung = nein`; wer
  gegen einen Aufstand eingriff, rüstet danach ab (Preußen blieb sonst von 1848 bis 1864 mobil); Regel
  `bund_durchzug`: Bundesstaaten ziehen durch Bundesgebiet, außer im Krieg gegen einen Staat mit
  Hauptstadt im Bund (sonst fand Preußen keinen Weg nach Holstein); ein Staat ohne Orte führt keinen
  Haushalt mehr. Ohne Spieler endet die deutsche Frage über 48 Läufe zu 35 % kleindeutsch, zu 23 % in
  zwei Bünden und zu 42 % im alten Bund. Inhaltsstände ereignisse 6, militaer 9; 1648 bitgleich.
- **Ereignisse in Godot (Schritt b4):** Das **Ereignisfenster** hält die Zeit an, sobald ein Ereignis
  des Spielerstaates wartet (Befehl `Ereignisspieler` beim Start, nach dem Laden und beim
  Staatswechsel; im Parteienmodus keiner), und sperrt das Tempo, bis es entschieden ist: Kopfzeile
  „Ereignis“ oder „Wendepunkt“, Datum, Text, Quelle, Optionen mit ihren Folgen als Text (neu im Kern:
  `describe_effects`, z. B. „Zustimmung Bauernschaft +3 · Unruhe −10“, Zufallszweige „mit 30 %: …“),
  gesperrte Optionen ausgegraut, historischer Kompass ◆ (abschaltbar). Das **Journalfenster** (Taste
  J, Knopf „Journale …“) zeigt je Journal Stufe, Ziele mit Fortschritt (`journal_goal_status`), Weg
  und mögliche Ausgänge, dazu die bisherigen Ereignisse (auch über das Spielmenü „Ereignisse“). Die
  Staatsinfo „Verfassung und Gruppen“ nennt Gesetze, Zustimmung je Gruppe, Unruhe mit Zielwert,
  Aufstand und befristete Wirkungen; neuer **Kartenmodus 16 „Unruhe“** (Taste X) mit Legende und
  Tooltip; das Diplomatiefenster sagt, dass die Bundesversammlung ruht, und sperrt dann die Anträge;
  Wendepunkte stehen in der Chronik rot. Brücke: `get_open_events`, `choose_event_option`,
  `set_event_player`, `get_event_player`, `get_event_log`, `get_journals`, `get_state_events`,
  `get_revolts`, `MAP_UNRUHE`, `turning_point` in `get_chronicle`, `suspended` in `get_diet`
  ([bridge-api.md](docs/technik/bridge-api.md)). Neue Startoptionen `--ereignis` und `--journal`, zwei
  Screenshots in der CI. Tests: Einheitentest des Ereignisfensters, Rauchtest der Ereignisse 1848
  (Baden wählt, Journale, Unruhe, ruhende Bundesversammlung), Klicktest (Ereignis hält die Zeit an,
  Taste J, Spielmenü „Ereignisse“, Taste X, ruhende Bundesversammlung); 1648 bitgleich.
- **Review und Abschluss (Schritt b5):** Die Abnahme aus [ereignisse.md](docs/technik/ereignisse.md)
  Abschnitt 10 steht im Klicktest: Als Preußen nimmt das Ereignisfenster die Kaiserkrone an
  (Wahlen der Strategie `krone`), das Journal erreicht am 5. April 1849 das Reich der Paulskirche;
  als Baden beantwortet es die Märzforderungen mit dem Militär, Baden erlebt Barrikadenkämpfe.
  Behoben: Die Brücke gab die Aufstandsart „Barrikadenkämpfe“ und die Ereignisart „atmosphärisch“ als
  Latin-1 statt UTF-8 an Godot. Meilensteinplan mit Ergebnis, Abnahme und Grenzen; M11 abgeschlossen,
  1648 bitgleich.

### Meilenstein M10 – Militär und Einigungskriege

Vertrag: [technik/militaer.md](docs/technik/militaer.md), Entscheidungen 1–14 wie empfohlen
freigegeben; Stand je Schritt in Abschnitt 12 dort.

- **Nachbarschaft und Friedensheere (Schritt a1):** Der Kartengenerator schreibt
  `karte/nachbarn.csv`: 13.069 Paare benachbarter Orte mit Grenzlänge und dem größten Fluss, den die
  Verbindung der Orte kreuzt ([chronik-skript.md](docs/technik/chronik-skript.md) Abschnitt 6.6);
  13 Fähren zu Inseln und über Meerengen stehen von Hand in `karte/ueberfahrten.csv` (6.7). Neue
  Datei `common/militaer.chron` (3.30) mit Regeln, Heerwesen je Staat (Bewaffnung, Geschütze,
  Mobilmachung) und Garnisonen: 1848 Gardekorps und acht Armeekorps Preußens, sechs
  Generalkommanden Österreichs, drei Garnisonen Bayerns, die übrigen Staaten in ihrer Hauptstadt.
  Beim Start steht je Garnison ein Heer mit ihrem Anteil an der Friedensstärke (zusammen 382.528
  Soldaten), am Monatsersten folgt es der Bevölkerung. Inhaltsbereich `militaer`, Spielstand
  **Format 4** (Spielstände aus M9 laden nicht mehr), `uhrwerk-cli info` mit Nachbarschaft und
  Garnisonen. Noch ohne Bewegung und Krieg; 1648 bitgleich.
- **Bewegung, Eisenbahn, Versorgung, Mobilmachung (Schritt a2):** Neue Befehle `Mobilmachung` und
  `Marsch`. Mit der Mobilmachung werden die Garnisonen Feldarmeen und wachsen in den
  Mobilmachungstagen auf die Kriegsstärke (Preußen ×2,5 in 28 Tagen); die Heereskosten folgen den
  Soldaten, die Abrüstung führt die Heere in 30 Tagen zurück in ihre Garnisonen. Eine Krise auf der
  Stufe Mobilmachung macht beide Hauptparteien wirklich mobil und endet mit ihrer Abrüstung. Märsche
  gehen auf dem schnellsten Weg über Land und Bahn: 20 km je Tag, Gebirge, Wald und Bruch langsamer,
  ein Flussübergang einen Tag mehr, Fähren nach den Daten; die Bahn fährt 300 km je Tag, lädt 20.000
  Soldaten je Strecke und Tag ein, und wer auf eine belegte Strecke trifft, wartet. Zugang vorerst nur
  im eigenen und verbündeten Gebiet. Versorgung und Zermürbung (versorgt 0,05 %, sonst 0,5 % je Tag
  und Moralverlust, im Winter doppelt). Regeln in `common/militaer.chron`; der Speichertest 1848 übt
  Märsche und Mobilmachungen mit. Ohne Befehle rechnet 1848 zeichengleich; 1648 bitgleich.
- **Krieg, Schlacht, Besetzung, Belagerung (Schritt a3):** Neuer Befehl `Kriegserklaerung` mit
  Bündnisfall (Verbündete und Garanten des Ziels stehen ihm bei, wer zu beiden gehört, bleibt
  neutral); Verträge zwischen den Seiten enden, alle Kriegführenden machen mobil. Eine Krise, in der
  nach der dritten Stufe keine Seite nachgibt, wird zum Krieg; eine angenommene Bundesintervention
  zur Bundesexekution. Stehen gegnerische Heere im selben Ort, kämpfen sie täglich (neuer Zufallsstrom
  `krieg`) mit Frontbreite, Gelände, Festung, Flussübergang, Geschützen und Moral, bis eine Seite
  bricht; die Verlierer ziehen sich mit Gefangenen zurück oder ergeben sich. Ein gegnerischer Ort
  ohne Verteidiger wird nach einem Tag besetzt (seine Steuern gehen zur Hälfte als Kontribution an
  den Besetzer), 35 Festungen von 1848 müssen belagert werden. Verwundete kehren nach 60 Tagen
  zurück, Gefallene und Abgänge mindern am Monatsersten die Bevölkerung. Die Chronik meldet Krieg,
  Schlachten („Schlacht bei Dresden: … schlagen …; Verluste … gegen …“), besetzte Hauptstädte und
  gefallene Festungen. Frieden folgt mit Schritt a4. Ohne Befehle rechnet 1848 zeichengleich; 1648
  bitgleich.
- **Frieden und Generalstab (Schritt a4):** Kriegsziele (Gebiet einer Verwaltungseinheit, Annexion,
  Führung im Bund, Beitritt zur Zollunion, Entschädigung) mit der Kriegserklärung oder dem neuen Befehl
  `Kriegsziel`; Kriegspunkte aus Besetzung, Hauptstadt, Schlachten und vernichteten Heeren;
  Kriegsmüdigkeit. Neuer Befehl `Friedensangebot` (ohne Gewinner, eigene Ziele, Kapitulation) mit
  Annahme nach Regel; am Monatsersten fällt der Frieden nach Regel, wenn eine Seite ihre Ziele deckt,
  die andere erschöpft ist oder ein Jahr lang nichts geschieht. Orte wechseln zur Laufzeit den
  Besitzer, Staaten ohne Orte gehen unter, der Bund verliert Mitglieder oder löst sich auf, Gefangene
  kehren heim, Heere kehren zurück und rüsten ab. Ein Generalstab führt die Heere nach Regeln
  (Rückzug vor doppelter, Angriff mit 1,2-facher Übermacht, Vormarsch auf die Ziele); Befehl
  `Generalstab`, ein eigener Marsch übernimmt das Heer. Probelauf 1848: Preußen annektiert Sachsen in
  zwei Monaten. Ohne Befehle rechnet 1848 zeichengleich; 1648 bitgleich.
- **Kriegslabor (Schritt a5):** `uhrwerk-cli krieg` rechnet je Seed die Kriege aus
  `common/kriegsziele_<szenario>.chron` ([chronik-skript.md](docs/technik/chronik-skript.md)
  Abschnitt 3.31) ohne Spieler bis zum Frieden (höchstens zwei Jahre): Der Angreifer erklärt den Krieg
  mit seinen Zielen, der Generalstab führt alle Heere. Bericht je Lauf mit Ausgang, Dauer,
  Friedensschluss, Schlachten, Verlusten, gewechselten Orten und Schulden, über alle Läufe die
  Verteilung gegen die Korridore (Anteil der Siege, Dauer, kein Ausgang in allen Läufen); Exitcode 1
  bei Verstößen, `pruefen` prüft die Datei. Kriegsziele 1848: Preußen annektiert Sachsen in 16 von
  16 Läufen nach rund zwei Monaten. Der Zustand hält die Ergebnisse beendeter Kriege
  (Inhaltsbereich `militaer` Stand 5, Spielstände 1848 aus a4 laden nicht mehr). Test-Fixture mit
  Militärdaten (`tests/daten_krieg`), CTests `krieg_fixture` und `krieg_1848`, CI-Schritt
  „Kriegslabor 1848“. 1648 bitgleich, 1848 ohne Befehle zeichengleich.
- **Daten 1849–1870 und Kriegslesezeichen (Schritt b1):** Drei neue Lesezeichen „Düppel“ (1.2.1864),
  „Königgrätz“ (16.6.1866) und „Sedan“ (19.7.1870), die im Krieg beginnen: Aufmarsch in
  `common/aufmarsch_<lesezeichen>.chron` (chronik-skript.md 3.32) mit Heeren unter Befehlshabern mit
  Führung, Kriegen mit festen Seiten und Zielen beider Seiten, Bündnissen. Frankreich gehört ab 1849 mit
  217 Orten im Kartenausschnitt zur Welt, Paris und das übrige Land als Hinterland (Einnahmen, Ersatz
  am Kartenrand, Bevölkerung bei Besetzung und Kriegspunkten). Die Historie 1849–1870 schreibt das neue
  Werkzeug `tools/geodaten/historie_1849_1870.py` aus datierten Änderungen: Hohenzollern, Anhalt,
  Schleswig-Holstein, die Annexionen von 1866, Ende des Deutschen Bundes und Bundesrat des
  Norddeutschen Bundes, Zollverein 1854 und 1868. 62 weitere Bahnstrecken (185 Abschnitte) bis 1870,
  die auch ein Spiel ab 1848 zur Eröffnung bekommt (Korridore 1848 weiter erfüllt). Heerwesen,
  Garnisonen und Festungen bis 1870. `bis` bei Bundesversammlung, Zollunion und Parlament.
  Inhaltsbereich `militaer` Stand 6. Die Kalibrierung der drei Kriege folgt mit b2. 1648 bitgleich.
- **Kalibrierung der Einigungskriege (Schritt b2):** Das Kriegslabor rechnet die drei Lesezeichen über
  16 Seeds in den Korridoren der Spezifikation: Düppel 16/16 für Preußen in 90 Tagen, Königgrätz
  11/16 in 107 Tagen, Sedan 14/16 für die deutsche Seite in 197 Tagen (CI-Schritt „Kriegslabor
  Lesezeichen“). Dafür: Generalstab mit Kampfkraft, Sturm oder Einschließung, Sammeln vor einer
  Übermacht, Rückzug in die stärkere Festung, Ziele in Reichweite; Besetzung im Vorbeimarsch; ein Heer
  in der Festung wird eingeschlossen und ergibt sich mit ihr; die Festung mindert die Verluste der
  Verteidiger; Moral für die Sieger; vernichtete Heere zählen erst ab einem Zehntel der Kriegsstärke;
  Ziele kosten nach der Bevölkerung der Gegenseite mit Hinterland; Frieden auch bei Müdigkeit über den
  Kosten; Dänemark verlässt die Karte, statt unterzugehen; `besetzt` im Aufmarsch (Holstein 1864).
  Neue Regeln `generalstab_moral`, `moral_sieg`, `punkte_heer_anteil`; Inhaltsbereich `militaer`
  Stand 7 (Spielstände der Lesezeichen aus b1 laden nicht mehr). 1648 bitgleich, 1848 ohne Befehle
  bitgleich zu b1.
- **Krieg in Godot (Schritt b3):** Heere stehen als Fähnchen in der Staatsfarbe mit ihrer Stärke in
  Tausend auf der Karte, Schlachten als gekreuzte Linien, Belagerungen als Ring. Ein Klick wählt ein
  Heer, ein Rechtsklick ohne Ziehen gibt einem eigenen Feldheer den Marschbefehl; beim Überfahren
  nennt der Tooltip Tage und Ankunft, die Karte zeigt den Weg gestrichelt. Das **Heeresfenster**
  (anstelle der Ortsinfo) zeigt Befehlshaber mit Führung, Stärke, Moral, Versorgung, Standort und
  Ziel und übergibt das Heer dem Generalstab oder nimmt es zurück. Das **Kriegsfenster** („Krieg …“
  im Staatspanel) zeigt je Krieg Seiten, Ziele mit Kosten, Kriegspunkte, Müdigkeit und Verluste,
  bietet der Hauptpartei weißen Frieden, die eigenen Ziele oder die Kapitulation mit Vorab-Prüfung
  („Österreich nähme an“) und weitere Kriegsziele an; der Reiter „Heere“ listet die eigenen Heere.
  Im Fenster „Diplomatie“ steht „Krieg erklären …“ mit Rückfrage (Seiten des Bündnisfalls mit
  Soldaten, Wahl des Kriegsziels). Das Staatspanel zeigt die Soldaten unter Waffen und die Stufe der
  Mobilmachung mit „Mobil machen“ bzw. „Abrüsten“. Schlachten mit eigenen Heeren erscheinen als
  Meldung „Sieg bei …“ bzw. „Niederlage bei …“ unten rechts über der Karte. Kartenmodus 15 **Krieg**
  (Taste Y): Kontrolle der Orte, besetzte Orte schraffiert in der Farbe des Besitzers, Fronten als
  Grenzen, Garnisonen sichtbar. Brücke: Heere, Wege, Kontrolle, Schlachten, Belagerungen, Kriege,
  Kriegsziele, Seiten einer Erklärung, Vorab-Prüfung eines Friedens und die Befehle
  ([bridge-api.md](docs/technik/bridge-api.md), Abschnitt Militär). Startoptionen `--krieg[=heere]`
  und `--heer`, zwei neue CI-Screenshots; Einheiten-, Rauch- und Klicktest erweitert (u. a. die drei
  Kriegslesezeichen im Menü „Neues Spiel“). 1648 bitgleich.
- **Review und Abschluss M10 (Schritt b4):** Review über Kern, Brücke und Godot, Daten und Doku.
  Behoben: Das Hinterland zahlte seine vollen Einnahmen in die Kasse, ohne die Ausgaben seiner
  Bevölkerung; Frankreich tilgte 1870 mitten im Krieg seine ganzen Schulden und häufte in einer Partie
  ab 1849 Jahr für Jahr Millionen an. Dänemark verfehlte damit seit Schritt b1 die Korridore des
  Balancing-Berichts 1848 (Vermögen am Ende 2,8 Jahreseinnahmen statt höchstens 1,5; die CI lief
  nicht). Jetzt ist der Überschuss des Hinterlands ein Posten „Hinterland“ im Haushalt (zählt zu
  Monats- und Jahreseinnahmen, im Haushaltsfenster eine eigene Zeile), im Verhältnis, das der
  Kartenteil im Frieden übrig behält (Frankreich 690.000 statt 2,6 Mio. Taler je Monat, Dänemark
  40.000 statt 130.000); Inhaltsbereich `militaer` Stand 8. Im Parteienmodus ließ sich ein Heer des Staates per Rechtsklick in Marsch
  setzen. Verliert der Spielerstaat alle Orte, hält die Zeit an und ein Hinweis nennt den Weg zu einem
  anderen Staat. Besitzer, Grenzen und Staatsnamen folgen einem Frieden am selben Tag statt am
  nächsten Monatsersten, die Legende im Kartenmodus „Krieg“ einem neuen Krieg ebenso. Das
  Kriegsfenster füllt die Zielauswahl nur noch bei Änderungen (vorher jede Sekunde rund 18 ms, eine
  offene Auswahl schloss sich). Der Klicktest spielt alle drei Kriegslesezeichen als Preußen mit
  Marschbefehl und angenommenem Friedensangebot bis zum Frieden (Abnahme M10). Doku:
  [meilensteine.md](docs/meilensteine.md) mit Ergebnis, Abnahme und Grenzen. 1648 bitgleich; Partien ab 1848
  rechnen ab 1849 (Frankreich) anders, Spielstände 1848 und der Kriegslesezeichen aus b3 laden nicht mehr
  (Inhaltsbereich `militaer`). **M10 abgeschlossen.**

### Meilenstein M9 – Diplomatie und Zollverein

Vertrag: [technik/diplomatie.md](docs/technik/diplomatie.md), Entscheidungen 1–12 wie empfohlen
freigegeben; Stand je Schritt in Abschnitt 11 dort.

- **Zollunionen als Daten (Schritt a1):** `common/zollunionen.chron` mit Zollverein (Leitung
  Preußen) und Steuerverein (Leitung Hannover), Tarif je 3 %
  ([chronik-skript.md](docs/technik/chronik-skript.md) Abschnitt 3.25). Das Staatsfeld
  `zollverein = ja/nein` heißt jetzt `zollunion = <kennung>|keine`; 1848 gehören 26 Staaten zum
  Zollverein, Hannover, Oldenburg und Schaumburg-Lippe zum Steuerverein, neun Staaten sind je ein
  eigenes Zollgebiet. Lader und Archivar prüfen die Verweise, `uhrwerk-cli info` zeigt Unionen und
  Mitglieder, neuer CTest `zollunionen_1848`. Inhaltsbereich `diplomatie`, Spielstand **Format 3**
  (Spielstände aus M8 laden nicht mehr). Brücke: `get_state` liefert `customs_union` und
  `customs_union_name` statt `zollverein`. Noch ohne Wirkung auf Handel und Haushalt; 1648 bitgleich.
- **Zollgebiete im Kern (Schritt a2):** Gibt es Zollunionen, teilt sich jeder Markt nach der
  Bevölkerung in Zollgebiete. Der Zoll wirkt im Handel wie zusätzliche Fracht und trifft beim Kauf
  nur die Einfuhr von außerhalb des eigenen Gebiets; die Mitglieder einer Union teilen ihren Zoll
  über die Vereinskasse nach der Bevölkerung. Neuer Befehl `Zollunion` (Beitritt mit Zustimmung der
  Leitmacht, Austritt, Wechsel, wirksam zum 1. Januar, mit Chronik); Mitglieder können ihren
  Zollsatz nicht selbst ändern; der Zollverein lehnt Österreich ab (`ausgeschlossen`). 1848 sinkt
  der Zoll von 5,7 % auf 1,1 % der Staatseinnahmen, der Handel steigt um 9 %; Bilanz und Wahlen 1848
  erfüllen weiter alle Zielwerte. 1648 bitgleich.
- **Zolllabor und Kalibrierung (Schritt a3):** `uhrwerk-cli zoll` vergleicht je Seed Läufe ohne und
  mit dem Wechsel eines Staates in eine Zollunion (Grenzhandel, Handel, Zoll, Preise im letzten Jahr)
  und prüft den Zollanteil der Unionen gegen `common/zollziele_voelkerfruehling.chron`; neuer
  CI-Schritt „Zolllabor 1848“. Der Vereinstarif des Zollvereins steigt auf 10 % (Zollanteil 2,9 % statt
  1 %); der Beitritt Hannovers erhöht den Grenzhandel um 40 % und senkt seinen Zoll auf zwei Drittel,
  der Mecklenburg-Schwerins verdoppelt ihn. Tritt eine Leitmacht aus, löst sich ihre Union auf
  (Hannover und der Steuerverein). Bilanz und Wahlen 1848 erfüllen alle Zielwerte.
- **Zollunionen in Godot (Schritt a4):**
  - Kartenmodus „Zollgebiete“ (13, Taste Ö): die Unionen in ihrer Farbe (`farbe` in
    `zollunionen.chron`), Staaten ohne Union blass; Legende mit Mitgliederzahl, Tooltip mit Tarif.
  - Staatspanel: Der Zollregler eines Mitglieds ist gesperrt und zeigt den Tarif der Union; im
    Haushaltsfenster steht der Zoll als Anteil an der Vereinskasse. Neuer Knopf „Zollunion …“ mit
    Fenster: Zollgebiet, Tarif, Anteil an der Kasse, je Union Leitmacht, Mitglieder, Tarif und Kasse;
    „Beitreten …“, „Austreten …“ und „Antrag zurückziehen …“ mit Rückfrage und Zeitpunkt (nächster
    1. Januar, Auflösung der eigenen Union beim Austritt einer Leitmacht). Ein offener Antrag steht
    im Hinweis des Panels.
  - Ortsinfo: Zollgebiet des Ortes mit Tarif. Startoption `--zollunion`.
  - Brücke: `get_customs_unions`, `get_customs_change` (Vorab-Prüfung ohne Wirkung, Kern
    `evaluate_customs_change`), `request_customs_union`, neue Felder in `get_state` und
    `get_location`, `MAP_ZOLLGEBIETE`; `set_tax_percent` lehnt den Zoll eines Mitglieds ab.
  - Tests: Rauchtest mit Unionen, Vorab-Prüfung und Antrag Hannovers; Klicktest mit Beitritt und
    Rücknahme über das Fenster; CI-Screenshot `einigkeit_zoll.png`. 1648 bitgleich.
- **Diplomatie im Kern (Schritt b1):** In Szenarien mit Deutschem Bund hat jedes Staatenpaar eine
  Beziehung von −200 bis +200, die sich monatlich auf ihren Zielwert zu bewegt: Grundwert aus der neuen
  Tabelle `beziehungen.csv` (24 Paare 1848, z. B. Österreich–Preußen −40), dazu gemeinsame Zollunion,
  Bund, Konfession, Bündnis und Garantie (`common/diplomatie.chron`). Neue Befehle `Gesandter`
  (verbessert die Beziehung, kostet 0,5 % der Monatseinnahmen; Preußen 4, Österreich 3, sonst 1
  Gesandter) und `Vertrag` (Bündnis ab Beziehung 100, Garantie ab 50, kein Bündnis mit dem Verbündeten
  eines Gegners; Kündigung). `uhrwerk-cli info` zeigt Gegner, Gesandte und die Lager zwischen Preußen
  und Österreich. Spielstände 1848 aus M9a laden nicht mehr (Inhalt `diplomatie`); 1648 bitgleich.
- **Bundesversammlung und diplomatische Spiele im Kern (Schritt b2):**
  - Neue Daten `common/bundesversammlung.chron`: Präsidium Österreich, Plenum mit 68 Stimmen, Engerer
    Rat mit 17 Stimmen (Kurien der kleinen Staaten). Befehl `Bundesantrag` für Bundesreform,
    Zolleinigung, Bundesintervention und Präsidium; Abstimmung am nächsten Monatsersten nach den
    Beziehungen zum Antragsteller und zum stärksten Gegner, mit Chronik und Wirkung (Zolleinigung:
    alle Bundesstaaten treten der Union bei).
  - Diplomatische Spiele: Befehl `Forderung` (Führung im Bund, Beitritt zur Zollunion, Bündnis lösen)
    mit Parteinahme der Bundesstaaten, drei Stufen (Protest, Ultimatum, Mobilmachung) zu 30 Tagen,
    Befehl `Manoever` (nachgeben, Vermittlung, eskalieren) und Auflösung nach Stärke; Mobilmachung
    kostet das 1,5-Fache des Heeres.
  - `uhrwerk-cli info` zeigt die Bundesversammlung. Spielstände 1848 aus Schritt b1 laden nicht mehr;
    1648 bitgleich.
- **Diplomatie in Godot (Schritt b3):**
  - Knopf „Diplomatie …“ im Staatspanel (1848) mit einem Fenster in drei Reitern: **Staaten** nach
    Beziehung mit Verträgen, Gesandten und Lager, dazu für den gewählten Staat Gesandte, Bündnis,
    Garantie, Forderungen (mit Rückfrage und den Seiten der Krise) und Bundesintervention;
    **Bundesversammlung** mit Halbrund der Plenarstimmen nach Lagern, offenem Antrag, Anträgen mit
    Prognose und den letzten Abstimmungen; **Deutsche Frage** mit Lagern, Zollverein, Reformen,
    Krisen und Verträgen.
  - Krisenleiste über der Karte mit Stufe, Seiten und den Manövern Nachgeben, Vermittlung anbieten
    und Eskalieren.
  - Kartenmodus „Beziehungen“ (Taste #, fünfzehnter Modus): grün freundlich, rot feindlich zum
    Spielerstaat. Haushaltsposten „Gesandte“, Startoption `--diplomatie[=staaten|bund|frage]`.
  - Brücke: Abfragen und Befehle der Diplomatie (bridge-api.md, Abschnitt Diplomatie).
- **Review und Abschluss M9 (Schritt b4):**
  - Ein Beitritt nach einer angenommenen Zolleinigung des Bundes oder einer verlorenen Krise
    „Beitritt zur Zollunion“ ist jetzt verbindlich. Vorher konnte der betroffene Staat ihn bis zum
    1. Januar zurückziehen. Das Fenster „Zollunion“ zeigt „verbindlich“ und sperrt die Rücknahme
    (Brücke: `customs_request.binding`).
  - Eine Abstimmung oder Krise am 1. Januar kündigte den Beitritt zum selben Tag an, obwohl dessen
    Jahreswechsel schon gelaufen war. Jetzt gilt er zum folgenden 1. Januar, wie er tatsächlich wirkt.
  - Ein Mitglied einer Kurie ohne eigene Plenarstimme zählte im Engeren Rat immer als Nein (1848 ohne
    Folgen).
  - Die Liste der Beziehungen (Fenster, Tooltip im Kartenmodus „Beziehungen“) rechnet die Zielwerte
    in einem Durchgang statt je Staat neu.
  - Godot: Der Knopf des Kartenmodus heißt „Beziehung“ statt „Diplomatie“ (nicht mit dem Fenster zu
    verwechseln); das Fenster „Diplomatie“ blendet Staaten eines anderen Spielstands aus.
  - Doku zum Abschluss: diplomatie.md verbindlich mit Grenzen, Meilensteinplan mit Ergebnis und
    Abnahme. Beide Abnahmekriterien sind erfüllt; 1648 bleibt bitgleich.

### Meilenstein M8 – Politik, Parteien und Wahlen

Im Lesezeichen 1848 wählt der Deutsche Bund am 1. Mai die **Nationalversammlung**: 633 Wahlkreise,
649 Sitze, davon 587 besetzt; die Tschechen in Böhmen boykottieren. Die Sitzverteilung trifft alle
Zielkorridore der Fraktionen, die Abweichungen sind dokumentiert. Der **Wahlabend** inszeniert das
Ergebnis mit Meldungen, Hochrechnung und Halbrund. Im **Parteienmodus** spielt man eine Strömung und
führt bis zur Wahl Wahlkampf. Die Preußische Zweite Kammer von 1849 (Dreiklassenwahlrecht) ist
plausibel. Das Szenario 1648 bleibt bitgleich (`5db13a66d63e6079`).

Alle Werte sind Spielwerte gegen ungeprüfte Richtwerte; die Fachprüfung mit Quellen schließt sich an
die von M7 an. Ergebnis und Grenzen:
[docs/meilensteine.md](docs/meilensteine.md#m8--politik-parteien-und-wahlen-).

- **Vertrag:** [technik/politik.md](docs/technik/politik.md) (neu) mit Umsetzungsstand je Schritt;
  [chronik-skript.md](docs/technik/chronik-skript.md) Abschnitte 3.19–3.24 (Strömungen,
  Interessengruppen, Parlamente, Wahlkreise, Wahlregeln, Wahlziele).
- **Daten (M8a):**
  - 5 Strömungen von den Konservativen bis zur frühen Arbeiterbewegung, 11 Interessengruppen,
    2 Parlamente: Nationalversammlung 1848 mit 8 Fraktionen, Preußische Zweite Kammer 1849 mit 3.
  - Verfassung, Wahlrecht und Wahlsystem der 38 Staaten in `staaten_historie.csv`.
  - `wahlkreise.csv`: 633 Wahlkreise der Nationalversammlung (649 Sitze) und 337 des Preußischen
    Zweiten Kammer (350 Sitze), erzeugt mit `tools/geodaten/wahlkreise_1848.py`.
  - `common/wahlen.chron` mit Wahlregeln, Wahlkampf und 14 Regionen der Wahlgeographie;
    Wahlziele `common/wahlziele_voelkerfruehling.chron`.
- **Kern:**
  - Wahl am Wahltag im Tagesablauf: Wahlberechtigte nach Wahlrecht (allgemein, Zensus, Dreiklassen),
    Beteiligung, Neigung je Block aus Interessengruppen, Bildung, Not, Stadt und Region, Wahlmänner,
    Boykott, Stichwahl, Abgeordnete mit Position und Fraktion ab dem Stichtag; Zufallsstrom `Wahlen`.
  - Parteienmodus: Spenden am Monatsersten, Befehl `Wahlkampf` (Versammlung, Flugblätter, Verein)
    mit abnehmendem Ertrag; Schätzung je Wahlkreis für den Stimmungsbericht.
  - Spielstand Format 2: Wahlen und Wahlkampf im Zustandsverzeichnis (nur mit Parlamenten), Befehle
    mit Ziel und Aktion, Metadaten `spielerstroemung`, Inhaltsbereich `politik`.
- **CLI:** `uhrwerk-cli wahl` (Wahllabor über viele Seeds, mit Wahlzielen, `--parlament`,
  `--wahlkreise`); `info` mit Parlamenten und Wahlkreisen; Testbefehle mit Wahlkampf.
- **Brücke und Godot:**
  - Kartenmodus „Wahlen“ (Taste Ä) mit Wahlkreisgrenzen, Legende und Abschnitt „Wahl“ in der Ortsinfo.
  - Wahlabend am Wahltag: Meldungen in Meldereihenfolge (×1, ×4, sofort), Hochrechnung mit
    Fehlerband, Halbrund nach Strömung oder Fraktion, Telegramme; „Letzte Wahl“ im Spielmenü.
  - Parteienmodus: Rolle im Spielmenü, Parteipanel, Wahlkampf und Stimmungsbericht in der Ortsinfo.
  - Staatsinfo „Verfassung und Gruppen …“: Verfassung, Wahlrecht und Wahlsystem des Landtags, Anteil
    und Einfluss der Interessengruppen (`get_state_politics`, Kern `interest_group_standing`).
  - Startoptionen `--wahlabend`, `--partei` und `--verfassung`.
  - Einheitentest mit **260 Prüfungen** (vorher 228); Kern mit **169 Testfällen** (vorher 153).
- **CI:** CTests `wahl_fixture` und `wahl_1848`, Schritt „Wahllabor 1848/49“, Prüfung der Tabelle
  der Wahlkreise, Screenshots von Wahlabend und Parteienmodus.
- **Review M8** (Kern; Brücke und Godot; Werkzeuge, Daten und Doku):
  - Wahlkreise optimal geschnitten (gleiche Einwohnerzahl, kurze Wege): bis auf Oldenburgs Exklaven
    beim 2/3- bis 1,5-Fachen des Regionsmittels statt 0,2 bis 1,8; größte Ausdehnung 116 statt 232
    km. Alle Wahlziele bleiben erfüllt.
  - Parteienmodus: Wahlkampf zur nächsten ausstehenden Wahl, nach dem 1. Mai 1848 also zur
    Preußischen Zweiten Kammer 1849; eingereihte Befehle zählen als verplant; Knöpfe und Telegramme
    bleiben bei laufender Zeit bestehen; nur in Szenarien mit Parlament.
  - Wahlabend: passt mit acht Fraktionen ins Bild, die stärkste Fraktion ist nie „fraktionslos“,
    Ortsinfo und Legende zeigen vor der Meldung kein Ergebnis; „Letzte Wahl“ setzt das Tempo fort.
  - Lader: Wahltag vor dem Start und leere `boykott_maerkte` sind Fehler; Wahlziele prüfen
    `beteiligung` und doppelte Einträge.
  - Daten: Niederlande, Dänemark, Liechtenstein und Schaumburg-Lippe ständisch; „Preußische Zweite
    Kammer“ statt des erst 1855 eingeführten „Abgeordnetenhauses“; Kurznamen der Strömungen.
  - Doku nach Daten und Code; Archivar lehnt `sitze` mit Nicht-Ziffern ab, statt abzubrechen.

### Meilenstein M7 – Lesezeichen 1848

Das zweite Lesezeichen **„Völkerfrühling“ (1. März 1848)** ist spielbar. Es umfasst den Deutschen
Bund mit 38 Staaten, 3.967 Orte mit Besitzer, Verwaltung und Bevölkerung 1846 (45,0 Mio.
Einwohner), 42 Eisenbahnstrecken, frühe Industrie und Haushalte in Talern. Der Balancing-Bericht
1848 erfüllt alle Zielwerte. Das Szenario 1648 bleibt bitgleich (`5db13a66d63e6079`).

Technisch ist M7 fertig, **die Fachprüfung der Daten mit Quellen steht noch aus** (Entscheidung 7).
Die Quellen waren in der Umgebung nicht erreichbar, deshalb tragen alle historischen Angaben
`ungeprueft` oder `schaetzung`. Ergebnis und Grenzen:
[docs/meilensteine.md](docs/meilensteine.md#m7--lesezeichen-1848-).

- **Vertrag:** [technik/lesezeichen_1848.md](docs/technik/lesezeichen_1848.md) (neu) mit
  Umsetzungsstand je Schritt; [chronik-skript.md](docs/technik/chronik-skript.md) Abschnitte
  3.16–3.18 (Verwaltung, Eisenbahnen, `seit`) sowie Ergänzungen in 3.4, 3.7 und 3.15.
- **Daten (M7a):**
  - 31 neue Staaten und 7 aus 1648 fortgeführte, mit Hauptstadt, Regierungsform, Herrscher,
    Konfession und Zollverein.
  - Zuordnung der Orte über `territorien_1848.csv` (476 Regeln), mit Bundesgebiet.
  - 458 Verwaltungseinheiten: Preußen mit 8 Provinzen, 25 Regierungsbezirken und 327 Kreisen;
    Österreich mit 11 Kronländern und 51 Kreisen; die oberste Ebene der Mittelstaaten.
  - Bevölkerung 1846 mit Konfession, Kultur und Berufen aus Summen je Staat, Provinz und Kronland
    (`einwohner_1846.csv`, `staedte_1846.csv`, `merkmale_1846.csv`).
- **Daten (M7b):**
  - `eisenbahnen.csv`: 176 Abschnitte, zum Start 165 eröffnet (rund 4.700 km).
  - Ab 1800 Kohle und Eisen; Betriebe Zeche, Hütte, Spinnerei, Maschinenbau, Brennerei und
    Salzwerk; Beruf Fabrikarbeiter.
  - Vorkommen an 34 Kohle-, 19 Erz-, 30 Salz- und 43 Weinorten.
  - Hof, Schatz und Schulden 1848 im Maßstab des Modells.
  - Zielwerte `zielwerte_voelkerfruehling.chron`.
- **Werkzeuge:**
  - `tools/geodaten/zuordnung.py`, `bevoelkerung_1846.py` und `stichprobe_1848.py`.
  - Archivar mit `verwaltung.csv` und `eisenbahnen.csv` (Bahnhöfe über den nächsten passenden Ort
    oder eine Kennung) und mit `spielbar`; Staaten ohne Ort zum Startdatum bestehen nicht.
  - `--bericht` mit Verwaltungseinheiten, Strecken und Szenarien.
- **Kern:**
  - Verwaltung (Einheiten, Kette, Sitz), Bundesgebiet und Zollverein je Ort und Staat;
    spielbare Staaten, Währung und Szenarioliste.
  - Eisenbahnen mit Regel `bahnfracht`: Nachbarmärkte im selben Netz handeln ab dem Tag der
    Verbindung zur Bahnfracht.
  - `seit` für Güter, Berufe und Betriebe: Späterer Inhalt fehlt in Welt, Fingerabdruck und
    Prüfsumme.
  - `anteil_aus` für Berufe.
  - Szenariofeld `tragfaehigkeit_faktor` (Agrartechnik; 1848: 2,5).
  - Szenarien werden vor `common/` gelesen.
  - **153 Testfälle** (vorher 146).
- **CLI:**
  - `--szenario voelkerfruehling` in allen Befehlen.
  - `info` mit Eisenbahnen (Strecken, Abschnitte, Verbindungen mit Bahnfracht).
  - `bilanz` und `pruefen` mit Zielwerten je Szenario (`zielwerte_<szenario>.chron`).
- **Brücke und Godot:**
  - „Neues Spiel“ mit Wahl des Lesezeichens; „Diesen Staat spielen“ in der Ortsinfo.
  - Kartenmodi „Verwaltung“ (Taste Ü) und „Eisenbahnen“ (Taste +, Strecken über der Karte).
  - Ortsinfo mit Verwaltungskette, Bundesgebiet und Bahnhof; Staatsinfo mit Bund und Zollverein.
  - Startoptionen `--szenario` und `--staat`.
  - Beim Szenariowechsel werden Ortskatalog, Farben und Ladeliste neu aufgebaut.
  - `unit_test.gd` mit **228 Prüfungen** (vorher 217); Rauchtest mit den Eisenbahnen 1648 und 1848.
- **CI:**
  - CTests `daten_pruefen_1848`, `determinismus_1848` und `speichertest_1848`.
  - Der Determinismus-Job vergleicht auch die Prüfsumme 1848.
  - Der Balancing-Job hat einen zweiten Bericht für 1848 (8 Seeds, 30 Jahre).
  - Screenshot 1848.
- **Review M7b:**
  - Das Bahnnetz verbindet Orte, nicht Märkte. Zwei Strecken, die nur im selben Markt enden, gelten
    nicht mehr als verbunden; 1848 sind 55 Paare von Nachbarmärkten verbunden.
  - Die Landfracht wird wieder in der dokumentierten Reihenfolge gerundet.
  - `anteil_aus` hängt nicht mehr von der Reihenfolge in der Datei ab; Kreise sind ein Fehler.
  - Spätere Definitionen werden vollständig geprüft (Kennungen, Verweise). `bahnfracht` darf nicht
    über `handel_fracht` liegen.
  - Karte: „Markt mit Bahnfracht“ folgt dem Kern (`get_rail_access`). Gestrichelt werden nur
    Abschnitte der nächsten fünf Jahre, 1648 also keine; die Linien werden täglich aktualisiert.
  - Der Archivar meldet doppelte Abschnitte auch über Strecken hinweg und gleiche Streckennamen.
  - Spielstände aus M6 sind mit M7 nicht mehr ladbar, weil sich der Inhaltsfingerabdruck geändert hat.
- **Stichprobe:** [pruefung/stichprobe_1848.md](docs/pruefung/stichprobe_1848.md) mit 60
  Datensätzen. Davon sind 46 plausibel, 6 Vereinfachungen und 2 zweifelhaft; 6 Fehler sind behoben.
  Die Flächen der großen Staaten liegen innerhalb ±5 % der Richtwerte.

### Meilenstein M6 – Speicherstände und Wiederholungen

Partien lassen sich speichern, laden und **bitgleich fortsetzen**, und aus Startzustand und
Befehlsprotokoll lässt sich jede Partie **exakt wiederholen** – geprüft über 50 Jahre mit 200
Testbefehlen in allen drei Compiler-Jobs. Damit ist die Vorproduktion (Phase 1) abgeschlossen.
Ergebnis und bekannte Grenzen:
[docs/meilensteine.md](docs/meilensteine.md#m6--speicherstände-und-wiederholungen-).

- **Vertrag:** [technik/speicherstaende.md](docs/technik/speicherstaende.md) (neu, verbindlich):
  Zustandsverzeichnis, Inhaltsfingerabdruck, Prüfungen beim Laden, Dateiformat v1,
  Befehlsprotokoll und Wiederholung, Versionen, Kommandozeile, Godot, Abnahme mit Messwerten.
  [bridge-api.md](docs/technik/bridge-api.md) ergänzt.
- **Kern:**
  - Zustandsverzeichnis (`core/src/state_access.hpp`): eine Liste aller veränderlichen Felder für
    Prüfsumme, Textexport (`Simulation::state_text`), Schreiben und Lesen; Referenzprüfsummen
    unverändert.
  - Spielstände (`uhrwerk/savegame.hpp`): 48-Byte-Kopf, Metadaten als Text, Nutzdaten mit zstd
    1.5.6 (per `FetchContent`, statisch); Inhaltsfingerabdruck je Bereich; Laden in frisch
    geladene Spieldaten mit Prüfung aller Indizes und Größen und Gegenprobe der Prüfsumme;
    gleicher Zustand ergibt dieselben Bytes; atomares Schreiben über eine Nebendatei.
  - Befehlsprotokoll (`Simulation::command_log`, beim Einreihen, mit wartenden Befehlen) und
    Startprüfsumme; Wiederholung mit jährlichen Kontrollpunkten, die den ersten abweichenden Tag
    meldet; Protokolldatei `.protokoll`.
  - Neuer Zufallsstrom `Testbefehle = 6` (nur Kommandozeile).
  - **146 Testfälle** (vorher 136), u. a. Weiterspielen an 17 Speicherpunkten mit 1 und 4 Threads,
    beschädigte und fremde Dateien, Formatwache, Wiederholung mit abweichendem Befehl.
- **CLI:** `lauf --speichern/--laden/--protokoll/--testbefehle`, `speichertest`, `wiederholung`,
  `spielstand [--text]`, `lasttest --speichern`.
- **Brücke und Godot:** `save_game`, `load_game`, `get_save_info`, `get_save_format_version`;
  Spielmenü (Knopf „Menü“, F10) mit Speichern unter einem Namen und Laden aus der Liste,
  Schnellspeichern (F5) und -laden (F9), automatischer Spielstand am 1. Januar (drei bleiben),
  Startoption `--spielmenue`. `unit_test.gd` mit **217 Prüfungen** (vorher 187); Rauch- und
  Klicktest mit Spielständen.
- **CI:** CTests `speichertest_50_jahre`, `protokoll_50_jahre`, `wiederholung_50_jahre`,
  `wiederholung_spielstand`, `spielstand_laden`, `spielstand_fremder_inhalt`; der
  Determinismus-Job vergleicht zusätzlich den Prüfwert eines Spielstands nach 100 Jahren
  (`17078dcdeecde792`); Lasttest-Job mit Spielstand; vierter Screenshot (Spielmenü).
- **Behoben:** MSVC brach mit `/WX` an unerreichbarem Code im Zustandsverzeichnis ab (C4702); das
  Datum der Metadaten wird ohne `sscanf` gelesen.
- **Review M6:** Kurzname der Staaten und Ortsname gehören zum Inhaltsfingerabdruck (sie stehen in
  der Chronik und damit in der Prüfsumme); Feldzahl statt Größe von `Rules` als Erinnerung an den
  Fingerabdruck; Laden prüft ungerade Inkremente der Zufallsströme; Wiederholung lehnt fremde
  Formatversionen ab; reservierte Namen im Spielmenü; am Jahreswechsel gehen keine Tage mehr
  verloren; je eine Umsetzung von FNV-1a, ISO-Datum und Hex-Darstellung.

### Meilenstein M5 – Wirtschaft und Staatshaushalt

Einkommen, Preise und Steuern entstehen jetzt aus einem Wirtschaftskreislauf mit Gütern, Betrieben
und regionalen Märkten, und die Staaten führen einen Haushalt mit Hof, Verwaltung, Heer, Zinsen und
Bauten, mit Kredit und Staatsbankrott. **Über 100 Jahre und 32 Seeds bleibt jeder Staat ohne
Bankrott bei etwa einer Jahreseinnahme Vermögen.** Ergebnis und bekannte Grenzen:
[docs/meilensteine.md](docs/meilensteine.md#m5--wirtschaft-und-staatshaushalt-).

- **Vertrag:** [technik/wirtschaft.md](docs/technik/wirtschaft.md) (neu, verbindlich): Güter,
  Bedürfnisse, Betriebe, Märkte und Handel, Ortsrechnung, Staatshaushalt, Ernte, Einschwingen,
  Balancing-Bericht, Abnahme mit Messwerten. [chronik-skript.md](docs/technik/chronik-skript.md)
  Abschnitte 3.11–3.15, [bridge-api.md](docs/technik/bridge-api.md),
  [tabellen.md](docs/technik/tabellen.md) ergänzt.
- **Kern:**
  - Wirtschaft (`uhrwerk/economy.hpp`): Märkte am Monatsersten mit Preisen aus Angebot und
    Nachfrage, Handel mit Nachbarmärkten und über Häfen mit dem Weltmarkt; Ortsrechnung am
    Rechentag (parallel) mit Erzeugung, Grundrente und Zehnt, Kontribution, Verbrauch nach
    Warenkorb, Akzise und Zoll, Bedarfsdeckung und Betriebsanteilen; Erntefaktor je Markt und
    Jahr (Strom Wirtschaft); Einschwingen über 60 Monate beim Laden. Hunger erhöht die
    Sterblichkeit, der Lebensstandard folgt dem Realeinkommen.
  - Staatshaushalt am Rechentag mit Kredit, Tilgung, Zins nach Verschuldung, Staatsbankrott
    (Chronik), Bauten und Landesausbau; Staatsnachfrage auf dem Markt der Hauptstadt.
    Steuerbefehl mit Steuerart (`Command::kind`, ungültige Arten werden verworfen).
  - Balancing-Bericht (`uhrwerk/balance.hpp`): Zielwerte lesen, Läufe messen, Median und
    Quantile in Festkomma, Verstöße.
  - **136 Testfälle** (vorher 123), u. a. Ortsrechnung, Markt, Handel und Haushalt exakt gegen
    Handrechnung, Bericht und Zielwerte.
- **CLI:** `uhrwerk-cli wirtschaft` (Güter, Berufe, Haushalt je Staat, `--maerkte`, `--jahre`),
  `uhrwerk-cli bilanz` (`--seeds`, `--jahre`, `--ziele`; Exitcode 1 bei Verstoß); `pruefen` prüft
  die Zielwerte mit; `lauf` zeigt Lebensstandard und Alphabetisierung, `--bericht` die Schulden.
- **Daten:** `common/gueter.chron` (11), `beduerfnisse.chron` (6), `betriebe.chron` (12),
  `maerkte.chron` (77), `zielwerte.chron`; Agrarabgaben der Berufe; Kontribution, Akzise, Zoll,
  Heer und Schulden je Staat; Vorkommen und Häfen je Ort; Hofhaltung neu bemessen; Regelwerte
  für Preise, Handel, Ernte, Hunger, Haushalt und Kredit. Das Feld `ertrag` der Berufe entfällt.
- **Archivar:** neue Spalten und Felder mit Prüfungen; `orte.py` behält die Spalten `vorkommen`
  und `hafen`.
- **Brücke und Godot:** `set_tax_percent` mit Steuerart, Haushalt in `get_state`, Wirtschaft in
  `get_location`, `get_market*`, Kartenmodi 8 „Wirtschaft“ (Taste O) und 9 „Märkte“ (Taste P) mit
  Marktgrenzen; drei Steuerregler und Haushaltsfenster im Staatspanel, Abschnitt Wirtschaft in der
  Ortsinfo, Startoption `--haushalt`. `unit_test.gd` mit **187 Prüfungen** (vorher 179).
- **CI:** neuer Job „Balancing (Linux, Release)“ (16 Seeds, 100 Jahre); CTests `bilanz_fixture`
  und `bilanz_verstoss`; dritter Screenshot (Märkte mit Haushaltsfenster).
- **Referenz-Prüfsumme** nach 100 Jahren jetzt `5db13a66d63e6079` (vorher `ed6b14a02cfbfa5d`),
  Bevölkerung 713.456; Lasttest-Referenz `8602a1273080ee9e` (vorher `a5a4c0d80a2f23ea`).

### Meilenstein M4 – Bevölkerung und Lasttest

Die Bevölkerung entsteht jetzt aus Geburten, Sterbefällen, Altern und Wanderung statt aus einem
festen Wachstum, und der Kern rechnet **500.000 Bevölkerungsblöcke in 0,33–0,40 s je Spieljahr**
(4 Kerne, Release) – bitgleich bei jeder Zahl von Threads und unter GCC, Clang und MSVC. Ergebnis
und bekannte Grenzen: [docs/meilensteine.md](docs/meilensteine.md#m4--bevölkerung-und-lasttest-).

- **Vertrag:** [technik/bevoelkerung.md](docs/technik/bevoelkerung.md) (neu, verbindlich):
  Datenmodell, Ablauf eines Tages, alle Formeln in Rechenreihenfolge, Regelwerte und
  Kalibrierung, Mehrkernberechnung, Lasttest, Abnahme mit Messwerten.
  [chronik-skript.md](docs/technik/chronik-skript.md) Abschnitte 3.1–3.6 und 3.10 (Kulturen,
  neue Felder), [bridge-api.md](docs/technik/bridge-api.md), [tabellen.md](docs/technik/tabellen.md)
  ergänzt.
- **Kern:**
  - Bevölkerungsblock mit Kultur, vier Altersgruppen, Lebensstandard und Alphabetisierung
    (64 Byte); `World::cultures`, Zugänge je Ort, Momentaufnahmen, Jahresbilanzen und
    Ortssteuern als neuer Zustand (alle in der Prüfsumme).
  - Bevölkerungsmodell (`uhrwerk/population.hpp`): Tragfähigkeit und Landreserve, Lebensstandard
    aus dem Netto-Einkommen, Geburten und Sterbefälle nach Lebensstandard, Altern, Bildung,
    Wanderung der Jungen (bis zu 4 Nachbarorte im Umkreis von 60 km und die nächste Stadt),
    Auswanderung, monatliche Neuordnung der Blöcke. Die Felder `wachstum_*` entfallen.
  - Mehrkernberechnung mit eigenem Thread-Pool (`uhrwerk/parallel.hpp`): Orte eines Tages und das
    Buchen der Wanderung je Zielort laufen parallel; feste Aufteilung und feste Reihenfolge beim
    Zusammenführen. `SimConfig::threads`.
  - Bitgleiche Schnellwege in `Fixed` (Knuth-Division, 64-Bit-Multiplikation), Rechentag-Listen,
    Steuern je Ort: 18 statt 450 ns je Block und Monat mit dem Modell aus M1.
  - Lasttest-Welt `make_load_test_world` (`uhrwerk/lasttest.hpp`, Zufallsstrom `Lasttest = 5`).
  - **123 Testfälle** (vorher 101), u. a. Formeln gegen Nachrechnung, exakte Buchführung,
    gleiche Prüfsummen mit 1–8 Threads, Kalibrierung mit den echten Daten.
- **CLI:** `uhrwerk-cli lasttest` (`--bloecke`, `--jahre`, `--threads`, `--vergleich`,
  `--grenze`, `--seed`); `--threads` für `lauf` und `determinismus`; `lauf` zeigt die Bevölkerung
  und die Jahresbilanz in ‰; `determinismus` rechnet den zweiten Lauf mit einem Thread.
- **Daten:** 38 Kulturen in `common/kulturen.chron`; Spalte `kultur` in `dateien.csv`
  (Standard je Landschaft) und `bevoelkerung.csv` (Anteile je Ort); Tragfähigkeit je Geländetyp,
  Bildungsfaktor je Konfession, Agrar-, Bildungs- und Stadtberuf je Beruf; kalibrierte
  Regelwerte (1648 → 1700 ×1,29, → 1816 ×2,33).
- **Archivar:** Kulturen und Kulturanteile mit Prüfungen; **35 Tests** (vorher 33). `orte.py`
  erhält die Spalte `kultur`.
- **Brücke und Godot:** `get_location` mit Kultur, Altersgruppen, Lebensstandard,
  Alphabetisierung und Bilanz des Vorjahres; `get_state` mit Lebensstandard und Bilanz;
  Kartenmodi 6 „Lebensstandard“ (Taste U) und 7 „Kultur“ (Taste I); Ortspanel mit Kulturen,
  Gesellschaft, Altersaufbau und Bilanz; Kartenleiste mit Kurznamen. `unit_test.gd` mit
  **179 Prüfungen** (vorher 170).
- **CI:** Determinismus vergleicht zusätzlich die Prüfsumme des Lasttests (50.000 Blöcke,
  2 Jahre); neuer Job „Lasttest (Linux, Release)“ mit 500.000 Blöcken, Grenze 3 s und Vergleich
  mit einem Thread; CTest `lasttest_threads`.
- **Referenz-Prüfsumme** nach 100 Jahren jetzt `ed6b14a02cfbfa5d` (vorher `40de8c42e8f7a0ff`),
  Bevölkerung 642.668; Lasttest-Referenz `a5a4c0d80a2f23ea`.

### Meilenstein M3 – Karte des Kernraums

Die Karte zeigt den ganzen Kernraum mit **4.542 Orten in 626 Kreisen**. Historische Daten haben
weiterhin nur die 60 Orte der Testregion; die übrigen 4.482 sind *Orte ohne Daten*. Ergebnis und
bekannte Grenzen: [docs/meilensteine.md](docs/meilensteine.md#m3--karte-des-kernraums-).

- **Verträge:** [chronik-skript.md](docs/technik/chronik-skript.md) Abschnitt 6 in Fassung M3
  (Plattkarte mit Bezugsbreite, GeoJSON-Geometrie für Land, Seen, Flüsse und Kernraum,
  Flächenart im Blaukanal von `provinzen.png`, Flussebene `fluesse.png`, `definition.csv` mit
  Beschriftungspunkt, Pixelzahl und Fläche), Abschnitt 3.6 *Orte ohne Daten* und 3.9 *Kreise*;
  [bridge-api.md](docs/technik/bridge-api.md) und [tabellen.md](docs/technik/tabellen.md) ergänzt.
- **Kern:** Orte ohne Daten – die vier Simulationsfelder (`besitzer`, `bevoelkerung`,
  `konfession`, `verwuestung`) sind zum Startdatum alle vorhanden oder keines; ohne sie hat ein Ort
  keinen Besitzer und keine Bevölkerungsblöcke und wird beim Jahreswechsel übersprungen (zieht
  keine Zufallszahlen). Hauptstädte müssen Orte mit Daten sein. Kreise aus `history/kreise/`
  (`World::districts`, `Location::district`, Feld `kreis` im Ort, nicht datierbar). `uhrwerk-cli`
  zeigt Orte mit und ohne Daten, Kreise und die Ladezeit. **101 Testfälle** (vorher 91).
- **Brücke:** `has_data`, `kreis_index` und `kreis_name` in `get_location`; gepackte Indexarrays
  `get_location_owner_indices` und `get_location_kreis_indices`; `get_kreis_count`,
  `get_kreis_index`, `get_kreis`; Kartenmodus 5 „Kreise“ (Palette mit 12 Farben); Alpha 0 für
  Orte ohne Daten in den Modi Politisch, Dichte, Konfession und Verwüstung.
- **Archivar:** Spalte `kreis` in `orte.csv`, neue Tabelle `kreise.csv` → `history/kreise/`;
  Orte ohne Zeilen in `besitz.csv` und `bevoelkerung.csv` sind Orte ohne Daten; Bericht mit Orten
  mit und ohne Daten und Kreisen. **33 Tests** (vorher 25).
- **Geodaten** (`tools/geodaten/`, neu, Python mit numpy):
  - `geodaten.py holen` lädt die offenen Quellen nur in den Zwischenspeicher
    `build/geodaten-cache/` und prüft sie gegen die SHA-256-Werte in `quellen.json`: Natural Earth
    5.1.2 (1:10 Mio., gemeinfrei), GeoNames `cities1000` über das npm-Paket
    `cities-with-1000@1.0.4` (CC BY 4.0), 224 Höhenkacheln der AWS Terrain Tiles (Zoomstufe 8).
    Quellenangaben: [Anhang C](docs/anhang/c_quellen_und_daten.md#5-verwendete-geodaten-stand-m3).
  - `geodaten.py geometrie` schneidet Land, Seen und Flüsse auf den Kartenausschnitt zu
    (`data/karte/geometrie/`, zusammen 0,26 MB; byte-gleich unter Python 3.10 und 3.13);
    `pruefen` vergleicht mit den Quellen, `vorschau` zeichnet eine Prüfkarte.
  - `data/karte/geometrie/kernraum.geojson` von Hand: 398 Punkte, jede Grenzentscheidung
    begründet, 93 Prüforte mit erwarteter Lage.
  - `orte.py` erzeugt die Orte und Kreise des Kernraums: aus 14.404 Kandidaten 4.482 neue Orte
    (gewichtete Poisson-Scheibe nach Bedeutung, Inseln, Lückenfüllung), 626 Kreise mit 4–12 Orten
    (Median 7) in 77 Landschaften; deutsche Namen nur mit Beleg in den GeoNames-Alternativnamen
    (978 belegte Einträge in `exonyme.csv`), Gelände aus dem Höhenmodell, Fläche aus der Karte.
  - **96 Tests** ohne Netz und ohne Zwischenspeicher (ein Volltest mit `ORTE_VOLLTEST=1`).
- **Kartengenerator v2** (jetzt mit numpy): Rasterung der Polygone nach der Pixelmitte,
  Flächenarten Kernraum-Land, Meer, Binnengewässer und Ausland, Zuordnung zum nächsten Ort mit
  deterministischem Rauschen (nur Grundrechenarten), genau eine zusammenhängende Fläche je Ort,
  Flussebene, Beschriftungspunkt, Fläche in Festkomma, Vorschau mit Ausschnitt und Namen. Die
  volle Karte (4096 × 3329 Pixel, 4.542 Orte) entsteht in 13–21 s; Kernraum-Land ist zu
  99,96 % Orten zugeordnet. Ausgabe byte-gleich unter Python 3.10 (numpy 2.2.6) und 3.13 (numpy
  2.5.3). **88 Tests.**
- **Daten:** 4.542 Orte (60 mit, 4.482 ohne Daten) in 77 Orts- und 77 Kreisdateien; neuer
  Geländetyp `hochgebirge` (jetzt 11); `karte.chron` in Fassung M3 (1,8–23,5 °O × 45,1–56,2 °N,
  4096 × 3329 Pixel, etwa 0,37 km je Pixel, `max_radius_km = 32`). `flaeche` aller Orte kommt aus
  der Karte (1–1.147 km², Median 169 km²); die 60 Orte der Testregion sind sonst unverändert.
- **Godot:** Großkarte mit Meer, Seen, Ausland und Flüssen, Schraffur „Keine Daten“, Staats-,
  Kreis- und Ortsgrenzen nach Zoomstufe; Namen nach Zoomstufe (Staaten fern, Kreise mittel, Orte
  nah) ohne Überlappung; Kartenmodus „Kreise“ (Taste Z); Ortsinfo und Tooltip für Orte ohne
  Daten; Startoption `--zoom`. Tests: `unit_test.gd` (170 Prüfungen), `smoke_test.gd` mit den
  Kartendaten v2, neu `klick_test.gd` (jeder der 4.542 Orte in drei Zoomstufen richtig erkannt,
  dazu rund 120 Klicks über echte Eingabeereignisse und Prüfungen der Namen an den Panels) und
  `leistung_test.gd`.
- **CI:** Job „Python-Werkzeuge“ mit numpy (2.2.6 unter Python 3.10, 2.5.3 unter 3.13) und den
  Tests der Geodaten; Godot-Job zusätzlich mit Einheiten- und Klicktest, alle Protokolle werden
  auf Skriptfehler geprüft; zweiter Screenshot im Kartenmodus „Kreise“ (mittlere Zoomstufe).
- **Referenz-Prüfsumme** nach 100 Jahren jetzt `40de8c42e8f7a0ff` (vorher `8d44cf792e143bb0`):
  Der Rechentag eines Ortes hängt von seinem Index ab (Index mod 28 + 1), und durch die
  eingefügten Orte ohne Daten haben die meisten der 60 Orte mit Daten einen anderen Rechentag.
  Bevölkerung nach 100 Jahren 627.680 statt 627.721. GCC, Clang und ASan liefern dieselbe
  Prüfsumme. Kreise gehören bewusst nicht zur Prüfsumme (unveränderlicher Inhalt).
- **Korrekturen nach dem Review von M3:**
  - Kern: Daten nur noch streng als `JJJJ.MM.TT` (genau 4, 2 und 2 Ziffern), wie im Archivar und
    in der Spezifikation; `1701.1.18` ist jetzt ein Ladefehler.
  - Archivar: Zeilenumbruch (LF oder einzelnes CR) in Namen, Kurznamen, Herrschern und Quellen
    ist ein Fehler (der Kern lehnt ihn in Zeichenketten ab); Jahr `0000` ist ungültig; eine
    Fläche, die im Festkomma des Kerns 0 wird (unter etwa 0,0000076 km²), ist ein Fehler statt
    einer Warnung.
  - Geodaten: neue Hilfstabelle `einwohner.csv` für Verwaltungssitze, die GeoNames mit 0
    Einwohnern führt (Osterode am Harz und Avesnes-sur-Helpe werden nicht mehr von Gittelde
    bzw. Avesnelles verdrängt); Orte im Trentino entfallen (`AUSLAND_GEBIETE`, statt
    Castelfondo jetzt Laurein in Deutschnonsberg); „ Markt“ und „ Stadt“ bleiben, wenn sie zum
    Namen gehören (Stetten am Kalten Markt); 26 neue belegte Namen in `exonyme.csv` (Meißen,
    Großarl, Straßwalchen, Großpetersdorf, Laßnitzhöhe sowie deutsche Namen wie Horitz,
    Eibenschitz, Mariensee, Laurein); die Lückenfüllung bricht auf Land fern von jedem Ort
    nicht mehr mit `OverflowError` ab; der Fluss Mies/Beraun heißt nicht mehr „Angel“.
  - Godot: Namen werden neu gesetzt, wenn ein Panel Größe, Lage oder Sichtbarkeit ändert
    (keine Namen mehr unter Legende und Ortsinfo); der Kreisname bleibt, wenn sein
    gleichnamiger Hauptort ausgewählt ist; eine ausgewählte Hauptstadt behält ihren Stil; die
    Legende „Kreise“ zählt nur Kreise mit Orten; ohne Kreisdaten zeigt die mittlere Stufe
    Ortsnamen.
  - Die Referenz-Prüfsumme des ersten M3-Stands (`5548a5fa80f75c0d`, 4.541 Orte) ändert sich
    dadurch auf `40de8c42e8f7a0ff`: Osterode am Harz in `goettingen` verschiebt die Indizes der
    38 Orte mit Daten in den Dateien `lausitz` bis `thueringen`.

### Meilenstein M2 – Datenpipeline „Archivar“

- **Datierte Blöcke** im Chronik-Skript: `JJJJ.MM.TT = { … }` in `staat` und `ort` gilt ab seinem
  Datum; der Lader liest die Szenarien vor der Historie und wendet alle Blöcke bis zum Startdatum
  an (Spezifikation Abschnitt 3.8, 4 neue Testfälle).
- **Tabellen** `data/tabellen/*.csv` als Quelle der Wahrheit für Orte und Staaten: Stammdaten,
  Besitz, Bevölkerung je Stichtag, Staatshistorie, Kopfkommentare. Jede Zeile mit Geschichte trägt
  Quelle und Sicherheit (`gesichert`, `schaetzung`, `spielwert`, `ungeprueft`). Format:
  [`docs/technik/tabellen.md`](docs/technik/tabellen.md).
- **Archivar** (`tools/archivar/`): prüft Verweise, Lücken zu allen Szenariodaten und Ausreißer,
  erzeugt `data/history/` deterministisch, `--pruefen` für die CI, `--bericht` für die Übersicht;
  25 Tests mit eigener Fixture.
- Die Testregion 1648 ist verlustfrei in die Tabellen umgezogen (Prüfsumme nach 100 Jahren
  unverändert); die Kopfkommentare der Dateien sind erhalten.
- Kartengenerator: Die Vorschau liest den Besitzer aus datierten Blöcken.
- CI: Job „Archivar und Kartengenerator“ mit Tests und Aktualitätsprüfung.

### Meilenstein M1 – technischer Durchstich

Meilenstein **M1 – technischer Durchstich**: Daten → Simulationskern → GDExtension → Godot, mit
einer Testregion um 1648. Geplant als Version 0.1.0 (so steht es bereits in `CMakeLists.txt` und
`game/project.godot`).

### Hinzugefügt

- **Git-Grundkonfiguration:** `.gitignore`, `.gitattributes` (LF-Zeilenenden, Git LFS für große
  Quell-Assets), `.editorconfig`, `.clang-format`, Beitragsregeln in `CONTRIBUTING.md` (Struktur,
  Branches, Conventional Commits mit Bereich, Code-Stil), Issue-Vorlagen „Fehler“, „Historischer
  Fehler“ und „Vorschlag“, Vorlage für Pull Requests. godot-cpp 4.4.1 als Submodul unter
  `third_party/godot-cpp`.
- **Technische Spezifikation:** [`docs/technik/chronik-skript.md`](docs/technik/chronik-skript.md)
  (Syntax, Dateischemata, Startzustand, Formeln, Kartenformat) und
  [`docs/technik/bridge-api.md`](docs/technik/bridge-api.md) (GDScript-Schnittstelle).
- **Simulationskern „Uhrwerk“** (`core/`, C++20, ohne Abhängigkeiten außer der
  Standardbibliothek):
  - Festkommazahl `Fixed` (Q47.16) mit festgelegten Rundungsregeln, 128-Bit-Zwischenwerte ohne
    Compiler-Erweiterungen.
  - Zufallsgenerator PCG32 mit getrennten Strömen je System, abgeleitet aus dem Partie-Seed.
  - Kalender (gregorianisch) mit deutschen Datumsformaten.
  - Parser für das Chronik-Skript mit deutschen Fehlermeldungen (`datei:zeile:spalte`) und
    Wiederaufsetzen nach Fehlern.
  - Lader für Regeln, Gelände, Konfessionen, Berufe, Staaten, Orte und Szenarien mit
    Unterscheidung zwischen fatalen Fehlern und Warnungen.
  - Simulation mit Tagestick: Befehle (Steuersatz), gestaffelte monatliche Demografie und
    Wirtschaft, Seuchen und Abbau der Kriegsverwüstung zum Jahreswechsel, Chronik.
  - Prüfsumme (FNV-1a, 64 Bit) über den gesamten veränderlichen Zustand.
  - CMake-Presets `dev` (Debug, Warnungen als Fehler), `release` und `bridge`.
- **Kommandozeile `uhrwerk-cli`** mit den Befehlen `pruefen`, `info`, `lauf` (`--bericht`,
  `--chronik`) und `determinismus`; Optionen `--daten`, `--szenario`, `--jahre`, `--tage`,
  `--seed`; Exitcodes 0 (in Ordnung), 1 (Daten- oder Determinismusfehler), 2 (Bedienfehler).
- **Tests:** 87 Testfälle mit doctest 2.4.11 (Festkomma, Zufall, Kalender, Parser, Weltmodell,
  Lader, Simulation) und eigenen Testdaten in `tests/daten/`; dazu die CTest-Läufe
  `daten_pruefen` und `determinismus_50_jahre` mit den echten Spieldaten.
- **Testregion „Der Westfälische Frieden“ (24.10.1648):** 16 Staaten, 60 Orte in Mittel- und
  Nordostdeutschland, 10 Geländetypen, 4 Konfessionen, 6 Berufsgruppen, Spieler-Vorauswahl
  Brandenburg (siehe [`data/README.md`](data/README.md)).
- **Kartengenerator** (`tools/kartengenerator/`, nur Python-Standardbibliothek): erzeugt das
  Ort-ID-Bild `data/karte/provinzen.png` und `data/karte/definition.csv` deterministisch aus den
  Lagen der Orte; Vorschaubild mit `--vorschau`, Aktualitätsprüfung mit `--pruefen`; eigene Tests.
- **GDExtension-Brücke** (`bridge/`): Klasse `UhrwerkSimulation` für GDScript nach
  `bridge-api.md`; die Bibliothek wird direkt nach `game/bin/` gebaut.
- **Godot-Prototyp** (`game/`, Godot 4.4.1): Karte der Testregion mit fünf Kartenmodi
  (politisch, Bevölkerungsdichte, Konfession, Gelände, Verwüstung), obere Leiste mit Datum und
  Geschwindigkeit, Staatspanel mit Steuersatz-Regler, Ortsinfo, Chronik und Legende;
  Screenshots von der Kommandozeile; Rauchtest `game/tests/smoke_test.gd`. Ohne gebaute
  Erweiterung startet das Projekt mit einem Hinweis statt mit Fehlern.
- **CI** (GitHub Actions, `.github/workflows/ci.yml`): Kern mit GCC und Clang unter Linux und mit
  MSVC unter Windows, plattformübergreifender Vergleich der Prüfsummen, Formatprüfung mit
  clang-format 18, Tests und Aktualitätsprüfung des Kartengenerators, Godot-Job mit Brücke,
  Rauchtest und Screenshot der Hauptszene als Artefakt.
- **Dokumentation:** Abschnitt „Entwicklung“ in der `README.md`, technischer Überblick in
  [`docs/technik/README.md`](docs/technik/README.md), dieses Änderungsprotokoll,
  Meilensteinplan [`docs/meilensteine.md`](docs/meilensteine.md) (M1–M13).

## [0.0.1] – 2026-10-09

### Hinzugefügt

- **Game Design Document** (`README.md`, `docs/00`–`docs/24`, Anhänge A–C): Vision und Marke,
  Engine-Empfehlung (Godot 4 mit eigenem C++20-Kern „Uhrwerk“), Kernspielablauf, Epochen und
  Lesezeichen, Karte, Staaten, Bevölkerung, Wirtschaft, Politik, Parlamente und Wahlen,
  Parteien-Datenbank, Föderalismus, Persönlichkeiten, Diplomatie, Militär, Weltkriege, Teilung
  und Einheit, Ereignisse, Technologie und Kultur, Verantwortung und Erinnerungskultur, KI,
  Oberfläche und Audio, Multiplayer und Modding, Produktion und Roadmap; Zeitleiste, Glossar,
  Quellen.

[Unveröffentlicht]: https://github.com/Hoylerma/Game/compare/143c84ff750e89f80fae0e8c5bb4672ea5724cc8...HEAD
[0.0.1]: https://github.com/Hoylerma/Game/tree/143c84ff750e89f80fae0e8c5bb4672ea5724cc8
