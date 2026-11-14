# Spieltest M13 – Leitfaden

Meilenstein M13, Schritt b6. Der Leitfaden gehört zum Abnahmekriterium „Spieltest mit externen
Testern; Rückmeldungen dokumentiert“ ([vertical_slice.md](../technik/vertical_slice.md), Abschnitt 10,
Entscheidung 18). Das Projekt bereitet den Test vor; **durchführen müssen ihn Menschen**. Die
Ergebnisse stehen in [spieltest_m13_ergebnisse.md](spieltest_m13_ergebnisse.md).

| Unterlage | Zweck |
|---|---|
| dieser Leitfaden | Ablauf, Aufgaben, Beobachtungsbogen, Datenschutz, Auswertung |
| [spieltest_m13_fragebogen.txt](spieltest_m13_fragebogen.txt) | Fragebogen zum Ausfüllen (Textdatei, maschinell auswertbar) |
| Rückmeldung im Spiel | Spielmenü → Rückmeldung → „Bericht anlegen“: ZIP mit Messwerten, Notiz, Spielstand, Protokoll |
| [`tools/spieltest/auswertung.py`](../../tools/spieltest/auswertung.py) | Tabellen der Auswertung aus Fragebögen und ZIPs |
| [spieltest_m13_ergebnisse.md](spieltest_m13_ergebnisse.md) | Vorlage der Ergebnisse mit Befunden und Antworten |

## 1 Ziele

1. **Verständlichkeit:** Finden Menschen ohne Projektwissen in das Spiel – mit der Geschichtsstunde, den
   Hinweisen und den Tooltips? Wo bleiben sie hängen?
2. **Fehler:** Abstürze, Hänger, falsche Anzeigen, abgeschnittene Texte auf echten Rechnern und in den
   Paketen aller drei Systeme.
3. **Bildrate:** 60 Bilder/s auf einem Mittelklasse-Rechner mit Grafikkarte (Abschnitt 10; der
   Cloud-Rechner der CI hat keine Grafikkarte). Die Rückmeldung misst sie selbst.
4. **Eindruck:** Fühlt es sich nach 1848–1871 an? Wollen die Tester weiterspielen? Ist die Darstellung
   der Geschichte sorgfältig ([20 Verantwortung und Erinnerungskultur](../20_verantwortung_und_erinnerungskultur.md))?

Abgenommen ist der Punkt, wenn **mindestens fünf Tester ohne Projektwissen** gespielt haben, je eine
Partie oder die Geschichtsstunde und **zwei Stunden freies Spiel**, und jeder Befund der Schwere 3 oder
4 eine Antwort hat (Abschnitt 7).

## 2 Rollen

- **Spielleitung:** bereitet vor, begrüßt, gibt die Aufgaben, hilft nur nach den Regeln in Abschnitt 4,
  sammelt die Unterlagen ein, wertet aus.
- **Beobachtung** (kann die Spielleitung selbst sein): füllt den Beobachtungsbogen aus, möglichst
  wörtlich und ohne zu werten.
- **Tester:** spielt und denkt laut. Kein Projektwissen: nicht am Projekt beteiligt, das Design-Dokument
  nicht gelesen. Gemischte Erfahrung ist erwünscht (mindestens ein Tester ohne Erfahrung mit
  Strategiespielen, mindestens einer mit viel).

Möglich ist auch ein **Test ohne Spielleitung** (zu Hause): Der Tester bekommt Paket, Aufgabenliste
(Abschnitt 5) und Fragebogen, spielt allein und schickt Fragebogen und Rückmeldung. Beobachtungen fehlen
dann; das ist im Ergebnis zu vermerken.

## 3 Vorbereitung

**Pakete:** aus dem Workflow `pakete.yml` (Actions → Pakete → Run workflow, oder ein Tag `v0.13.0`):
`einigkeit-<version>-linux-x86_64.tar.gz`, `…-windows-x86_64-setup.exe` (oder das ZIP),
`…-macos.dmg`. Version und Commit im Ergebnis festhalten. Alle Tester bekommen dieselbe Version.

**Rechner:** möglichst alle drei Systeme, mindestens einer **Mittelklasse mit Grafikkarte** (für die
Bildrate), dazu gern ein schwächerer Laptop. Bildschirm mindestens 1600 × 900.

**Frischer Start:** Vor jedem Test den Benutzerordner des Spiels löschen, damit Hinweise und
Einstellungen wie beim ersten Start sind:

| System | Benutzerordner |
|---|---|
| Linux | `~/.local/share/godot/app_userdata/EINIGKEIT` |
| Windows | `%APPDATA%\Godot\app_userdata\EINIGKEIT` |
| macOS | `~/Library/Application Support/Godot/app_userdata/EINIGKEIT` |

**Kennungen:** Jeder Tester bekommt eine Kennung `T01`, `T02`, …; Namen stehen nirgends in den
Unterlagen. Die Zuordnung von Kennung zu Person (falls nötig, etwa für Rückfragen) bleibt bei der
Spielleitung und kommt nicht ins Repository.

**Einverständnis:** vor dem Test mündlich erklären und bestätigen lassen: Was eingesammelt wird
(Fragebogen, Beobachtungsbogen, Rückmeldungs-ZIP), dass keine Namen gespeichert werden, dass Ergebnisse
zusammengefasst und anonym im Repository dokumentiert werden. Bildschirm- oder Tonaufnahmen nur mit
ausdrücklicher Zustimmung und nur zur Auswertung; sie kommen nicht ins Repository.

**Material:** dieser Leitfaden (Abschnitte 4–6 ausgedruckt), ein Beobachtungsbogen je Tester
(Abschnitt 6), der Fragebogen als Datei auf dem Testrechner oder ausgedruckt, eine Uhr.

## 4 Ablauf (rund drei Stunden)

| Zeit | Teil | Was passiert |
|---|---|---|
| 5 min | Begrüßung | Text unten vorlesen; Einverständnis; Kennung nennen |
| 10 min | Installation | Der Tester installiert und startet das Paket selbst (beobachten, nicht helfen) |
| 20–30 min | Geschichtsstunde | Hauptmenü → Geschichtsstunde „März 1848 in Berlin“ bis zum Ende |
| 2 h | Freies Spiel | Neues Spiel, Staat frei wählbar (empfohlen: Preußen, Österreich, Bayern, Sachsen); die Aufgaben aus Abschnitt 5 nach und nach stellen, wenn sie sich nicht von selbst ergeben |
| 5 min | Rückmeldung | Spielmenü → Rückmeldung → Notiz („was mir zuletzt aufgefallen ist“) → „Bericht anlegen“ (Aufgabe 10) |
| 15 min | Fragebogen | allein ausfüllen |
| 10 min | Nachgespräch | Was war gut, was hat genervt, was fehlt? Offene Beobachtungen nachfragen |

Wer statt des freien Spiels eine ganze Partie bis 1871 spielen will, darf das; die reine Rechenzeit
ist auf Tempo 4 knapp vier Minuten, die Dauer hängt also fast nur an Ereignissen, Wahlen und Pausen
(mit mehreren Stunden rechnen, gern an zwei Terminen mit Spielstand). Die Aufgaben gelten ebenso.

**Lautes Denken:** Der Tester sagt, was er sieht, sucht und erwartet. Erinnern, wenn er verstummt
(„Was denkst du gerade?“).

**Helfen:** Die Spielleitung erklärt nichts von sich aus. Hängt ein Tester **zwei Minuten** an
derselben Stelle, erst einen **Hinweis** geben („Schau dir die Leiste links an“), nach weiteren zwei
Minuten **helfen**. Beides im Bogen vermerken (H = Hinweis, X = Hilfe). Bei einem Absturz: Uhrzeit
notieren, das Spiel neu starten, nach Möglichkeit den letzten automatischen Spielstand laden; das
Protokoll des Absturzes liegt in `logs/` im Benutzerordner (mitnehmen).

**Begrüßung (vorlesen):**

> Danke, dass du mitmachst. Du testest **EINIGKEIT**, ein Strategiespiel über die deutsche Geschichte
> von 1848 bis 1871, in einer frühen Fassung. Getestet wird das Spiel, nicht du: Wenn etwas nicht klappt,
> ist das ein Fehler des Spiels, und genau den suchen wir. Bitte denk laut – sag, was du siehst, was du
> suchst und was du erwartest. Ich helfe absichtlich erst spät, damit wir sehen, wo das Spiel besser
> erklären muss. Wir speichern keine Namen; am Ende füllst du einen Fragebogen aus, und das Spiel legt
> eine Datei mit technischen Angaben und deinem Spielstand an. Du kannst jederzeit abbrechen.

## 5 Aufgaben

Die Aufgaben stellt die Spielleitung im freien Spiel, wenn der Tester sie nicht von selbst angeht (nicht
alle auf einmal). „Erfolg“ heißt: ohne Hinweis gelöst.

| Nr. | Aufgabe (so sagen) | Erfolg |
|---|---|---|
| 1 | „Starte ein neues Spiel mit einem Staat deiner Wahl.“ | Hauptmenü → Neues Spiel → Lesezeichen → Staat → Einstellungen → im Spiel |
| 2 | „Wie viel Geld hat dein Staat im Monat übrig? Erhöhe eine Steuer und finde heraus, was sich ändert.“ | Saldo in der oberen Leiste oder im Haushalt gefunden, Regler bewegt, Änderung benannt |
| 3 | „Wer regiert deinen Staat, und was hält er von Reformen?“ | Kabinett und Hof geöffnet, Herrscher und Neigung genannt |
| 4 | „Warum sind die Menschen in deinem Staat unzufrieden, und wo am meisten?“ | Kartenmodus Unruhe oder Gesellschaft; Grund aus einem Tooltip genannt |
| 5 | „Verbessere die Beziehung zu einem Nachbarn.“ | Diplomatie geöffnet, Gesandter geschickt oder Vertrag angeboten |
| 6 | „Ändere ein Gesetz.“ | Fenster Gesetze, Änderung beantragt; Wirkung oder Hindernis verstanden |
| 7 | „Wie ist die letzte Wahl ausgegangen?“ | Wahlabend, Parlament oder Spielmenü → Letzte Wahl |
| 8 | „Wie hat sich die Bevölkerung seit Spielbeginn entwickelt?“ | Statistisches Jahrbuch geöffnet, Reihe gelesen |
| 9 | „Speichere das Spiel, spiele weiter und lade dann den Spielstand.“ | Spielmenü, Speichern unter Namen, Laden; Zustand wieder da |
| 10 | „Lege eine Rückmeldung mit einer kurzen Notiz an.“ | Spielmenü → Rückmeldung → Bericht anlegen; Pfad gesehen |

## 6 Beobachtungsbogen

Ein Bogen je Tester (zum Ausdrucken oder als Datei `T03_beobachtung.md` neben die übrigen Unterlagen).

```
Kennung: ____   Datum: ________   System/Rechner: ______________________   Version: ______
Beobachtung: Spielleitung selbst / eigene Person / ohne (Test zu Hause)

Installation:  Dauer ___ min   Probleme: _______________________________________________
Geschichtsstunde:  bis Schritt ___ von 14   Dauer ___ min   H/X bei Schritt: ____________

Aufgabe | Zeit (min) | Ergebnis: ✓ allein / H Hinweis / X Hilfe / – abgebrochen | Beobachtung
   1    |            |                                                          |
   2    |            |                                                          |
  ...   |            |                                                          |
  10    |            |                                                          |

Weitere Beobachtungen (Uhrzeit · Stelle im Spiel · was geschah · wörtliches Zitat · Schwere 1–4):
_________________________________________________________________________________________
```

**Schwere** (für Beobachtungen und später für die Befunde):

| Stufe | Bedeutung | Beispiel |
|---|---|---|
| 4 blockierend | Weiterspielen unmöglich oder Spielstand verloren | Absturz, Hänger, Laden schlägt fehl |
| 3 erheblich | Tester kommt ohne Hilfe nicht weiter oder versteht Wesentliches falsch | Wahlausgang nicht auffindbar, Ziel des Spiels unklar |
| 2 gering | Umweg, Irritation, kurz hängen geblieben | Fenster erst beim zweiten Versuch gefunden |
| 1 kosmetisch | stört nicht beim Spielen | Tippfehler, unschöner Umbruch |

## 7 Einsammeln und auswerten

**Unterlagen je Tester:** ausgefüllter Fragebogen `T03_fragebogen.txt`, Beobachtungsbogen, die
Rückmeldungs-ZIPs aus dem Benutzerordner (`rueckmeldungen/`, siehe Tabelle in Abschnitt 3), bei einem
Absturz das Protokoll aus `logs/`. Alles in einen Ordner `spieltest/T03/` legen. Der Ordner `spieltest/`
im Repository-Hauptordner ist von Git ausgenommen: Spielstände, Protokolle und Rohdaten werden **nicht**
eingecheckt, nur die Auswertung.

**Datenschutz:** Die Rückmeldung enthält keine Namen; Pfade zum Benutzerordner stehen dort als „~“.
Vor dem Weitergeben trotzdem einmal in `notiz.txt` und `info.txt` schauen. Fragebögen ohne Namen.

**Auswertung:**

```bash
python3 tools/spieltest/auswertung.py --eingang spieltest --streng      # prüft die Antworten
python3 tools/spieltest/auswertung.py --eingang spieltest \
    --bericht docs/pruefung/spieltest_m13_ergebnisse.md                 # Tabellen in den Bericht
```

Das Werkzeug füllt die Abschnitte Teilnehmende, Technik und Bildrate, Skalen, Freitexte und Notizen.
Von Hand kommen dazu: die Durchführung (Termine, Rechner, Abweichungen), die **Beobachtungen** aus den
Bögen (Aufgaben mit Erfolg, Hinweis, Hilfe je Tester) und die **Befunde**:

1. Jede Beobachtung der Schwere 2–4, jede Skalenfrage mit dem Vermerk **Befund**, jedes nicht erreichte
   Bildratenziel und jeder Absturz wird zu einem Befund (gleiche zusammenfassen, Zahl der Tester
   notieren).
2. Jeder Befund bekommt eine **Antwort**: *behoben* (mit Commit), *geplant* (mit Meilenstein),
   *kein Handlungsbedarf* (mit Begründung) oder *offen*.
3. Befunde der Schwere 3 und 4 dürfen für die Abnahme nicht *offen* bleiben.

## 8 Nach dem Test

- Befunde beheben, die in M13 gehören (b6), die übrigen in den Meilensteinplan.
- Die Ergebnisse ersetzen die Vorlage; Abschnitt 13 von vertical_slice.md bekommt den Stand.
- Der Abnahmepunkt in Abschnitt 10 gilt erst mit dokumentierten Ergebnissen als erfüllt.
