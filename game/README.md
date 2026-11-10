# EINIGKEIT – Godot-Projekt (Prototyp der Spieloberfläche, Meilensteine M1 bis M9)

Das Projekt zeigt die Simulation des Kerns **Uhrwerk** auf der Karte des **Kernraums**
(Meilenstein M3: rund 4.500 Orte in rund 620 Kreisen): Kartenansicht mit sechzehn Kartenmodi
(seit M4 auch Lebensstandard und Kultur, seit M5 Wirtschaftsleistung und Märkte, seit M7
Verwaltung und Eisenbahnen, seit M8 Wahlen, seit M9 Zollgebiete und Beziehungen, seit M10 Krieg), Zoomstufen mit
Staats-, Kreis- und Ortsnamen, obere Leiste, Staatspanel mit Reglern für Kontribution, Akzise und
Zoll und einem Haushaltsfenster (seit M5), Ortsinfo mit Wirtschaft (Markt, Einkommen und
Bedarfsdeckung je Beruf, Betriebe, Güter; seit M5) und Bevölkerung (Kulturen, Lebensstandard,
Alphabetisierung, Altersaufbau, Bilanz des Vorjahres), Chronik und Legende, seit M6 ein Spielmenü
mit Speichern und Laden, Schnellspeichern (F5/F9) und automatischem Spielstand am 1. Januar. Die Simulation selbst läuft in C++ und wird über die
GDExtension `uhrwerk_bridge` angesprochen (Schnittstelle:
[docs/technik/bridge-api.md](../docs/technik/bridge-api.md)). Die Godot-Seite **liest** nur
und schickt **Befehle**; sie rechnet nichts selbst.

## Voraussetzungen

| Was | Woher |
|---|---|
| Godot **4.4.1** (Standard-Build, kein .NET) | [godotengine.org](https://godotengine.org/download/archive/4.4.1-stable/); im Repository optional unter `tools/godot/` (nicht versioniert) |
| GDExtension `game/bin/libuhrwerk_bridge.so` (Linux), `uhrwerk_bridge.dll` (Windows), `libuhrwerk_bridge.dylib` (macOS) | `cmake --preset bridge && cmake --build --preset bridge` im Repository-Hauptordner |
| Spieldaten | `data/` im Repository (oder Umgebungsvariable `UHRWERK_DATEN`) |
| Karte `data/karte/provinzen.png`, `fluesse.png` und `definition.csv` (Fassung M3) | `python3 tools/kartengenerator/kartengenerator.py` |

Ohne Erweiterung startet das Projekt trotzdem fehlerfrei und zeigt einen deutschen Hinweis,
wie sie gebaut wird. Fehlt die Karte oder hat `definition.csv` noch das Format vor M3,
erscheint ebenfalls ein Hinweis mit dem Befehl des Kartengenerators; Leisten und Panels
funktionieren dann weiter. Fehlt nur `fluesse.png`, zeigt die Karte keine Flüsse.

## Starten

```bash
# einmalig bzw. nach neuen Dateien: Projekt importieren (legt game/.godot/ an)
godot --headless --path game --import

# Spiel starten
godot --path game

# im Editor öffnen
godot --editor --path game
```

`godot` steht für die Godot-4.4.1-Programmdatei, z. B. `tools/godot/Godot_v4.4.1-stable_linux.x86_64`.
Der Import ist nötig, weil Godot erst dabei die globalen Klassennamen (`class_name`) registriert
und das Programmsymbol importiert; der Editor erledigt das beim Öffnen von selbst.

### Startbildschirm

Seit M13 beginnt das Spiel mit dem **Startbildschirm** (ohne `--szenario`, `--staat`, `--partei` und
`--screenshot`): **Neues Spiel** führt über das Lesezeichen (Völkerfrühling 1848, Düppel, Königgrätz, Sedan;
1648 als Technikprototyp unter „Weitere“), den Staat (empfohlen Preußen, Österreich, Bayern und Sachsen mit
Schwierigkeit; „Anderer Staat …“ für alle übrigen) und die **Spieleinstellungen** (Schwierigkeit Lehrling,
Geselle oder Meister, Geschichtsverlauf historisch oder offen, historischer Kompass) zum Spielbeginn.
Stufe und Verlauf gelten für die ganze Partie ([docs/technik/vertical_slice.md](../docs/technik/vertical_slice.md),
Abschnitte 3.1 und 3.2). **Fortsetzen** lädt den jüngsten Spielstand, **Laden …** zeigt alle. Esc führt eine
Seite zurück. Geschichtsstunde und Einstellungen folgen mit späteren Schritten von M13.

### Datenordner

Standard ist `../data` relativ zum Projektordner (also `data/` im Repository). Die
Umgebungsvariable `UHRWERK_DATEN` überschreibt ihn; relative Angaben beziehen sich auf den
Projektordner `game/`:

```bash
UHRWERK_DATEN=/tmp/meine-daten godot --path game
```

### Screenshots von der Kommandozeile

Optionen stehen nach `--` (Benutzerargumente):

| Option | Bedeutung |
|---|---|
| `--screenshot=<pfad>` | Nach dem Laden simulieren, etwa 20 Bilder warten, den Viewport als PNG speichern und beenden |
| `--tage=<n>` | Simulierte Tage vor dem Screenshot (Standard 60) |
| `--kartenmodus=<0..17>` | 0 Politisch, 1 Bevölkerungsdichte, 2 Konfession, 3 Gelände, 4 Verwüstung, 5 Kreise, 6 Lebensstandard, 7 Kultur (6 und 7 seit M4), 8 Wirtschaft, 9 Märkte (seit M5), 10 Verwaltung, 11 Eisenbahnen (beide seit M7), 12 Wahlen (seit M8), 13 Zollgebiete, 14 Beziehungen (beide seit M9), 15 Krieg (seit M10), 16 Unruhe (seit M11), 17 Bund (seit M13) |
| `--ort=<ort_id>` | Ort auswählen, Ortsinfo geöffnet (z. B. `berlin_coelln`) |
| `--zoom=<faktor>` | Vergrößerung gegenüber der Übersicht (1 = ganze Karte), zentriert auf `--ort`, sonst auf die Kartenmitte (bis zum Faktor 1 bleibt die Bildmitte der Übersicht); gilt auch ohne `--screenshot` |
| `--haushalt` | Haushaltsfenster des Spielerstaates vor dem Screenshot öffnen (seit M5) |
| `--verfassung` | Fenster „Verfassung und Gruppen“ des Spielerstaates vor dem Screenshot öffnen (seit M8; nur in Szenarien mit Parlament, 1848) |
| `--zollunion` | Fenster „Zollunion“ des Spielerstaates vor dem Screenshot öffnen (seit M9; nur in Szenarien mit Zollunion, 1848) |
| `--diplomatie[=<reiter>]` | Fenster „Diplomatie“ vor dem Screenshot öffnen, Reiter `staaten` (Standard), `bund` oder `frage` (seit M9; nur mit dem Deutschen Bund, 1848) |
| `--krieg[=<reiter>]` | Kriegsfenster vor dem Screenshot öffnen, Reiter `kriege` (Standard) oder `heere` (seit M10; nur in Szenarien mit Heeren) |
| `--heer` | Stärkstes eigenes Feldheer auswählen, das Heeresfenster steht dann in der rechten Spalte (seit M10) |
| `--spielmenue` | Spielmenü vor dem Screenshot öffnen (seit M6) |
| `--szenario=<id>` | Szenario beim Start, z. B. `voelkerfruehling` (seit M7; Standard `westfaelischer_frieden`); gilt auch ohne `--screenshot` |
| `--staat=<TAG>` | Spielerstaat beim Start, z. B. `BAV` (seit M7; Standard: Spieler des Szenarios) |
| `--wahlabend` | Wahlabend der letzten Wahl mit dem ganzen Ergebnis vor dem Screenshot öffnen (seit M8; dafür `--tage` über den Wahltag, 1848 z. B. `--tage=62`) |
| `--partei=<strömung>` | Parteienmodus mit dieser Strömung beim Start, z. B. `liberale` (seit M8; nur in Szenarien mit Parlament, 1848) |
| `--ereignis` | Ereignisse des Spielerstaates warten auf die Wahl, das Ereignisfenster zeigt die offenen (seit M11; 1848 z. B. `--staat=BAD --tage=8`). Ohne die Option rechnen Screenshots wie die Kommandozeile ohne Ereignisspieler |
| `--journal` | Journalfenster vor dem Screenshot öffnen (seit M11; ab 1848) |
| `--jahrbuch` | Statistisches Jahrbuch vor dem Screenshot öffnen (seit M13a5a; ab 1848, mit `--tage` über einen 1. Januar) |
| `--bereich=<bereich>` | Fenster eines Bereichs vor dem Screenshot öffnen: `kabinett`, `gesetze`, `gruppen`, `parlament`, `gesellschaft`, `wirtschaft` (seit M13a5b), `chronik`, `lexikon` (seit M13a5d) |
| `--einstellungen[=<reiter>]` | Fenster Einstellungen öffnen, Reiter `bild` (Standard), `ton`, `spiel`, `zugang`, `tasten`, `sprache` (seit M13a5d) |
| `--farbsehschwaeche` | Farben für Rot-Grün-Schwäche für diesen Start, ohne die Einstellung zu speichern (seit M13a5d) |
| `--startbildschirm[=<seite>]` | Startbildschirm vor dem Screenshot öffnen, Seite `menue` (Standard), `lesezeichen` oder `einstellungen` (Spieleinstellungen für den Spielerstaat des Lesezeichens, mit `--szenario`; seit M13) |

Auf der Kernraum-Karte bei 1600 × 900 Pixeln reicht die ferne Stufe (Staatsnamen) bis etwa
`--zoom=2,7`, die mittlere (Kreisnamen) bis etwa `--zoom=6,3`, darüber folgt die nahe
(Ortsnamen); z. B. `--zoom=1`, `--zoom=4` und `--zoom=10`. Die Schwellen stehen unter
[Detailstufen](#detailstufen). Auf der Kommandozeile ist der Dezimalpunkt zu verwenden
(`--zoom=2.5`).

```bash
xvfb-run -a -s "-screen 0 1600x900x24" \
  godot --path game --rendering-driver opengl3 --audio-driver Dummy \
  -- --screenshot=/tmp/einigkeit.png --tage=365 --kartenmodus=5 --ort=nuernberg --zoom=4
```

Relative Pfade beziehen sich auf das Startverzeichnis (`$PWD`). Exitcode 0, wenn das Bild
gespeichert wurde und Simulation und Karte geladen waren; sonst 1 (das Bild des
Fehlerzustands wird trotzdem gespeichert, sofern eine Grafikausgabe vorhanden ist). Im
Modus `--headless` gibt es kein Bild; ohne Bildschirm hilft `xvfb-run`.

## Bedienung

| Eingabe | Wirkung |
|---|---|
| Linksklick auf die Karte | Ort auswählen, Ortsinfo öffnen (Klick ins Leere, ins Meer oder Ausland schließt sie) |
| Linksklick auf eine Heeresmarke | seit M10: Heer auswählen (Rahmen, Weg bis zum Ziel gestrichelt) |
| Rechtsklick ohne Ziehen | seit M10, mit einem eigenen Feldheer ausgewählt: Marschbefehl zum Ort (wirksam ab dem nächsten Tag); der Tooltip nennt beim Überfahren Tage und Ankunft, die Karte zeigt den Weg gestrichelt |
| Esc | seit M13a5a zuerst das zuletzt geöffnete Fenster eines Bereichs schließen; sonst Auswahl aufheben (zuerst das Heer, dann den Ort) |
| Bereichsleiste | seit M13a5a, rechts neben dem Staatspanel; seit M13a5b mit eigenen Fenstern für **Kabinett** (Herrscher und Minister mit Werten, Eigenschaften und Anteil an den Neigungen, die Neigungen des Staates, frühere Amtsinhaber der Partie), **Gesetze** (Verfassung, Wahlrecht, Unruhe mit Ziel, jede Gesetzesoption mit Wirkung auf die Unruhe und den Gruppen dafür und dagegen), **Gruppen** (Anteil, Einfluss, Zustimmung, Forderungen), **Parlament** (Auswahl des Parlaments, Halbrund der letzten Wahl nach Fraktionen, nächste Wahl, Abstimmungen), **Gesellschaft** (Altersaufbau, Kulturen, Konfessionen, Berufe, Vorjahr mit Wanderung) und **Wirtschaft** (Erzeugung, Steuern, Zollgebiet, Märkte, Güter mit Preisen, Betriebe). Dazu wie in a5a Haushalt, Zollunion, Diplomatie, Militär (Kriegsfenster), Journale, Jahrbuch und Chronik (ein- und ausblenden); Bereiche ohne Inhalt im Lesezeichen fehlen. Ein Klick öffnet das Fenster neben der Leiste, ein zweiter schließt es; die Fensterverwaltung hält höchstens eines offen. Die Knöpfe „Haushalt im Einzelnen …“, „Verfassung und Gruppen …“, „Zollunion …“, „Diplomatie …“ und „Journale …“ des Staatspanels sind dafür ausgeblendet |
| Obere Leiste | seit M13a5c nach Kapitel 22: Wappen und Staat, **Kasse · Saldo** des Vormonats, **Legitimität** (Zustimmung der Gruppen im Mittel), **Verwaltung** (Orte), **Vermächtnis** (E R F W G), Datum und Tempo, Menü. Jede Zahl hat einen verschachtelten Tooltip: Kasse mit Einnahmen je Steuer und Ausgaben je Posten, Legitimität mit der Zustimmung je Gruppe und den Ursachen der Unruhe, Vermächtnis mit Satz je Dimension; unterstrichene Begriffe (Akzise, Saldo, Unruhe …) und die Dimensionen öffnen beim Überfahren einen zweiten Tooltip, beide bleiben offen, solange die Maus darüber steht |
| Übersicht (rechts) | seit M13a5c: laufende Dinge – offene Ereignisse, Krisen, Kriege, Journale mit Stufe, die nächste Wahl, Heere auf dem Marsch; ein Klick öffnet das Fenster oder fährt das Heer an; ein Klick auf den Kopf klappt sie ein. Solange eine Ortsinfo offen ist, ruht sie |
| Einstellungen (Startbildschirm, Spielmenü) | seit M13a5d, gespeichert in `user://einstellungen.cfg`: **Bild** (Vollbild, Größe der Oberfläche 75–200 %, Bildrate), **Ton** (Lautstärken; Musik folgt mit b2), **Spiel** (automatischer Spielstand aus, jährlich oder halbjährlich; historischer Kompass; Hinweise; **Pausenregeln**: Kriegserklärung, Forderung, Wahlergebnis, Amtswechsel, Bundesgründung, Frieden – ein Ereignis des Spielerstaates hält immer an), **Barrierefreiheit** (Farben für Rot-Grün-Schwäche: Grün wird Blau auf Karte und Legende; hoher Kontrast; reduzierte Animationen; Schriftgröße 100–150 %), **Tasten** (jede Aktion neu belegbar, doppelte Belegung wird abgelehnt, Zurücksetzen), **Sprache** (Deutsch) |
| Chronik (Bereichsleiste) | seit M13a5d ein Fenster mit allen Einträgen, Filter nach Staat (alle, eigener, Welt) und „nur Wendepunkte“, Suche; ein Klick fährt den Ort an |
| Lexikon (Bereichsleiste) | seit M13a5d: Artikel aus `data/lexikon` mit Suche und Verweisen; ein Klick auf einen unterstrichenen Begriff in einem Tooltip öffnet ihn |
| Wendepunkt | seit M13a5d: Ereignisse mit `wendepunkt = ja` erscheinen breiter mit großem Titel, die Karte dunkelt dahinter ab |
| Ticker | seit M13a5c über der Leiste der Kartenmodi: das Neueste aus der Chronik (Wendepunkte zuerst und rot, sonst Einträge des Spielerstaates und der Welt), alle vier Sekunden der nächste; ein Klick fährt den Ort an. Die Chronik selbst ist über die Bereichsleiste ein- und auszublenden und zu Beginn ausgeblendet |
| B, „Jahrbuch“ | seit M13a5a, ab 1848: Statistisches Jahrbuch mit einer Kennzahl oder Dimension des Vermächtnisses (Auswahl) je Stichtag (Start, jeder 1. Januar, Ende) als Liniendiagramm und Tabelle, für den Spielerstaat und bis zu drei Staaten zum Vergleich |
| Rechte oder mittlere Maustaste ziehen | Karte verschieben |
| Mausrad | Zoomen (zum Mauszeiger hin, von knapp unter der Übersicht, bis ein Kartenpixel 6 Bildschirmpixel groß ist) |
| W A S D, Pfeiltasten | Karte verschieben |
| Pos1 | Übersicht über die ganze Karte |
| H | Zur Hauptstadt des Spielerstaates (mittlere Zoomstufe) |
| Leertaste | Pause / weiter |
| 1 – 5 | Geschwindigkeit: 2, 5, 15, 40 oder 120 Tage je Sekunde |
| Q W E R T Z U I O P Ü + Ä Ö # Y X C | Kartenmodus Politisch, Bevölkerungsdichte, Konfession, Gelände, Verwüstung, Kreise, Lebensstandard, Kultur, Wirtschaft, Märkte, Verwaltung und Eisenbahnen (beide seit M7; „+“ ist die Taste rechts neben Ü, englisch „]“), Wahlen (seit M8; „Ä“ rechts neben Ö, englisch „'“), Zollgebiete (seit M9; „Ö“, englisch „;“) und Beziehungen (seit M9; „#“ rechts neben Ä, englisch „\“: Beziehung jedes Staates zum Spielerstaat, grün freundlich, rot feindlich) und Krieg (seit M10; „Y“ links unten, englisch „Z“: wer die Orte hält, besetzte Orte schraffiert in der Farbe des Besitzers, Fronten als Grenzen, dazu Garnisonen, Schlachten und Belagerungen) und Unruhe (seit M11; „X“: Unruhe des Besitzers von 0 bis 100, Staaten mit Aufstand violett; der Tooltip nennt Unruhe, Ziel und Aufstand) und Bund (seit M13; „C“: das Bundesgebiet, das gilt – der Deutsche Bund oder ein neuer Bund wie der Norddeutsche Bund oder das Reich –, die Präsidialmacht kräftig, die Mitglieder blass) (die Leiste zeigt Kurznamen, die Taste steht im Tooltip). Im Modus Eisenbahnen zeichnet die Karte die Strecken: eröffnete durchgezogen, die der nächsten fünf Jahre gestrichelt |
| Ereignisfenster | seit M11, ab 1848: Ein Ereignis des Spielerstaates hält die Zeit an (das Tempo bleibt gesperrt, bis es entschieden ist). Kopfzeile „EREIGNIS“ bzw. „WENDEPUNKT“, Datum, Text, Optionen mit ihren Folgen, Quelle; der historische Kompass ◆ markiert die historische Wahl (Häkchen „Kompass“ schaltet ihn ab). Gesperrte Optionen sind ausgegraut. Die Wahl gilt ab dem nächsten Tag; danach folgt das nächste offene Ereignis oder die Zeit läuft mit dem Tempo von vorher weiter ([docs/technik/ereignisse.md](../docs/technik/ereignisse.md), Abschnitt 9) |
| J, „Journale“ (Bereichsleiste) | seit M11, ab 1848: Journalfenster mit je einem Reiter für „Revolution 1848/49“ und „Die deutsche Frage“ (Stufe seit wann, Ziele ✓/○ mit Fortschritt, Weg bisher, mögliche Ausgänge mit Wertung) und „Bisherige Ereignisse“ (mit der gewählten Option, ◆ historisch, Wendepunkte rot). Esc oder × schließt |
| Wahlabend | seit M8: öffnet am Wahltag von selbst, die Zeit hält an. Die Wahlkreise treffen in der Meldereihenfolge ein (Tempo ×1, ×4 oder sofort); Halbrund, Balken mit Hochrechnung und Fehlerband, Telegramme (Klick zeigt den Wahlkreis). Danach im Spielmenü „Letzte Wahl“ |
| Steuerregler | Kontribution, Akzise und Zoll des Spielerstaates (wirksam ab dem nächsten simulierten Tag); seit M9 ist der Zollregler eines Mitglieds einer Zollunion gesperrt und zeigt den Tarif der Union |
| „Haushalt im Einzelnen …“ | Haushaltsfenster: Einnahmen je Steuerart, Ausgaben je Posten (Hof, Verwaltung, Heer, Zinsen, Bauten), Schatz, Schulden, Zins, Staatsbankrotte |
| „Verfassung und Gruppen …“ | seit M8, 1848: Verfassung, Wahlrecht und Wahlsystem des Landtags; Interessengruppen des Staates mit Anteil an den Einwohnern ab 15 und Einfluss (Anteil am Einkommen); seit M11 auch die Gesetze (Presse, Vereine, Grundlasten), die Zustimmung jeder Gruppe, Unruhe mit Zielwert, ein laufender Aufstand und befristete Wirkungen |
| „Zollunion …“ | seit M9, 1848: Zollgebiet, Tarif und Anteil an der Vereinskasse; je Union Leitmacht, Mitglieder, Tarif und Kasse mit „Beitreten …“ bzw. „Austreten …“, nach einer Rückfrage mit Zeitpunkt (nächster 1. Januar); „Antrag zurückziehen …“. Ein verbindlicher Beitritt (nach einer Zolleinigung des Bundes oder einer verlorenen Krise, seit M9b) lässt sich nicht zurückziehen. Lehnt die Leitmacht ab (Österreich beim Zollverein), ist der Knopf gesperrt ([docs/technik/diplomatie.md](../docs/technik/diplomatie.md), Abschnitt 3.5) |
| „Diplomatie …“ | seit M9, 1848 (mit dem Deutschen Bund): Fenster mit drei Reitern. **Staaten**: jeder Staat mit Beziehung (Farbfeld wie der Kartenmodus), Verträgen, Gesandten und Lager; für den gewählten Staat Gesandten entsenden/abberufen, Bündnis anbieten/kündigen, Garantie geben/kündigen, Forderungen (Führung im Bund, Beitritt zur Zollunion, Bündnisse lösen; nach einer Rückfrage mit den Seiten der Krise) und Bundesintervention beantragen; gesperrte Knöpfe nennen im Tooltip den Grund. **Bundesversammlung**: Präsidium, Halbrund des Plenums nach Lagern, offener Antrag, Anträge (Bundesreform, Vorsitz, Zolleinigung der eigenen Union) mit Prognose, die letzten Abstimmungen; seit M11 „Die Bundesversammlung ruht“ mit gesperrten Anträgen, solange sie ruht (ab Juli 1848). **Deutsche Frage**: Lager, Zollverein, Reformen, Krisen und Verträge ohne Wertung. Esc oder × schließt ([docs/technik/diplomatie.md](../docs/technik/diplomatie.md), Abschnitt 6) |
| Heeresfenster | seit M10, nach einem Klick auf eine Heeresmarke (oder im Kriegsfenster) anstelle der Ortsinfo: Befehlshaber mit Führung, Stärke, Moral, Versorgung, Standort, Ziel mit Ankunft; für eigene Feldheere „Dem Generalstab übergeben“ bzw. „Selbst führen“ (ab dem nächsten Tag; ein eigener Marschbefehl nimmt das Heer dem Generalstab ab) und „Zeigen“. Esc oder × hebt die Auswahl auf |
| „Krieg …“ (Staatspanel) | seit M10, in Szenarien mit Heeren: Kriegsfenster. **Kriege**: je Krieg Seiten, Ziele mit Kosten, Kriegspunkte gegen die Kosten der eigenen Ziele (Balken), Müdigkeit, Verluste und Schlachten; für die Hauptpartei „Weißer Frieden“, „Unsere Ziele fordern“ und „Kapitulation anbieten“, jeweils mit der Antwort des Gegners vorab („Österreich nähme an“), und ein weiteres Kriegsziel. **Heere**: die eigenen Heere mit Stärke, Moral, Lage und Weg; ein Klick wählt das Heer und fährt es an. Esc oder × schließt ([docs/technik/militaer.md](../docs/technik/militaer.md), Abschnitt 8) |
| Mobilmachung (Staatspanel) | seit M10: Soldaten unter Waffen und Stufe (Frieden, macht mobil, mobil, rüstet ab); „Mobil machen“ bzw. „Abrüsten“ ab dem nächsten Tag, im Krieg keine Abrüstung |
| „Krieg erklären …“ (Diplomatie, Staaten) | seit M10: Rückfrage mit den Seiten des Bündnisfalls und ihren Soldaten und der Wahl des Kriegsziels; der Krieg beginnt am nächsten Tag, alle Beteiligten machen mobil |
| Schlachtbericht | seit M10: nach einer Schlacht mit eigenen Heeren unten rechts über der Karte „Sieg bei …“ bzw. „Niederlage bei …“ mit dem Bericht der Chronik; „Zeigen“ fährt den Ort an, nach 12 Sekunden oder mit × verschwindet die Meldung (höchstens drei zugleich) |
| Krisenleiste | seit M9: über der Karte, solange ein diplomatisches Spiel läuft (das eigene zuerst): Forderung, Stufe (Protest, Ultimatum, Mobilmachung) mit verbleibenden Tagen, beide Seiten mit Soldaten; ist der Spielerstaat Hauptpartei, die Knöpfe Nachgeben, Vermittlung anbieten (der Tooltip sagt, ob die Gegenseite annimmt) und Eskalieren; seit M12 nennt der Tooltip des Titels die Gründe einer fordernden KI |
| KI der Staaten | seit M12, ab 1848: Die Staatsinfo nennt Herrscher und leitenden Minister (Eigenschaften und Werte im Tooltip) und die Neigungen des Staates mit seiner Staatspersönlichkeit. Im Fenster „Diplomatie“ (Reiter Staaten) stehen für den gewählten Staat seine Absichten (Hauptziel, Rivale, aktive Ziele; nur mit eigenem Gesandten dort) und seine letzten drei Entscheidungen mit „Warum?“ (Gründe im Tooltip). Erklärt eine KI dem Spielerstaat den Krieg oder stellt sie ihm eine Forderung, erscheint ein Hinweis in der Kopfleiste mit ihren Gründen; das Kriegsfenster nennt sie im Tooltip des Titels ([docs/technik/ki.md](../docs/technik/ki.md), Abschnitt 9) |
| Klick auf einen Chronikeintrag | Ort des Ereignisses auswählen und anfahren; Einträge eines Wendepunkts tragen seit M11 „WENDEPUNKT“ in Rot |
| F10, Knopf „Menü“ | Spielmenü: Neues Spiel mit Wahl des Lesezeichens (1648 oder 1848, seit M7), Speichern unter einem Namen, Laden aus der Liste, Schnellspeichern und -laden; seit M11 „Ereignisse“ (die bisherigen Ereignisse im Journalfenster); die Zeit hält an, Esc oder F10 schließt es (seit M6) |
| „Diesen Staat spielen“ (Ortsinfo) | Den Besitzer des Ortes als Spielerstaat übernehmen (seit M7); nach einem neuen Spiel nennt ein Hinweis die hervorgehobenen Staaten des Lesezeichens |
| Rolle (Spielmenü) | seit M8, nur in Szenarien mit Parlament (1848): „Staat: …“ oder „Partei: …“. Im **Parteienmodus** steht links das Parteipanel (Mittel, verplante Mittel, Spenden; die nächste Wahl mit Wahlkampf; nach einer Wahl deren Sitze, Fraktion und Abgeordnete) statt des Staatspanels; „Staat spielen“ führt zurück. Nach der Nationalversammlung (1. Mai 1848) gilt der Wahlkampf der Preußischen Zweiten Kammer (17. Juli 1849) |
| Wahlkampf (Ortsinfo) | seit M8, im Parteienmodus für die nächste Wahl: Parlament und Wahlkreis, Stimmungsbericht (Schätzung mit ± 8 Punkten Fehler) und die Knöpfe Versammlung, Flugblätter und Verein gründen; Wirkung ab dem nächsten Tag, gesperrt, wenn die Mittel nach den schon angesetzten Aktionen nicht reichen ([docs/technik/politik.md](../docs/technik/politik.md), Abschnitt 9) |
| F5 | Schnellspeichern (`schnellspeicher.einigkeit`) |
| F9 | Schnellladen |

**Doppelbelegung von W:** W gehört sowohl zur Kamerasteuerung (WASD) als auch zum
Kartenmodus „Bevölkerungsdichte“. Kurz getippt (unter 0,18 s) schaltet W den Kartenmodus um,
gehalten bewegt es die Karte nach oben. Die Pfeiltaste nach oben bewegt immer sofort.

Die Tastenbelegung steht als Aktionen in `project.godot` (`kamera_*`, `kartenmodus_*`,
`tempo_*`, `auswahl_aufheben`, `spielmenue`, `schnellspeichern`, `schnellladen`, `journal`, `jahrbuch`) und lässt sich im Editor unter *Projekteinstellungen →
Eingabebelegung* ändern. Es werden physische Tasten verwendet, die Belegung gilt also auf
jeder Tastatur an derselben Stelle: „Z“ ist die Taste rechts neben T (auf englischen
Tastaturen beschriftet mit „Y“), „Ü“ die rechts neben P (englisch „[“), „Y“ die links unten neben der Umschalttaste (englisch „Z“).

## Aufbau

```
game/
├── project.godot              Name, Hauptszene, Fenster 1600×900 (canvas_items/expand),
│                              Renderer gl_compatibility, Eingabebelegung, Theme
├── icon.svg
├── bin/uhrwerk.gdextension    Beschreibung der GDExtension (Bibliotheken je Plattform)
├── scenes/
│   ├── main.tscn              Hauptszene: Karte + Oberfläche (CanvasLayer)
│   ├── map/map_view.tscn      Kartenbild (Sprite2D mit Shader) und Kamera
│   └── ui/                    top_bar, state_panel, chronicle_panel, location_panel,
│                              legend_panel, map_mode_bar, map_tooltip, notice_layer
├── scripts/
│   ├── main.gd                Hauptszene, oberste Ebene (seit M13a5d): Ablauf (_ready, Zeit, Eingaben),
│   │                          Bereiche und Jahrbuch, Einstellungen und Pausenregeln, Leisten und Tooltips
│   ├── main/                  seit M13a5d: main_core.gd (Felder, Laden, Anzeige, Bedienung, Spielstände,
│   │                          Screenshot) ← main_windows.gd (Diplomatie, Militär, Startbildschirm, Partei,
│   │                          Ereignisse, Urteil, Wahlabend) ← main.gd; dazu legend_view.gd (Legende),
│   │                          map_tooltip_text.gd (Kartentooltip), bar_tooltips.gd (Tooltips der Leiste)
│   ├── core/                  Hilfsklassen ohne Szenenbaum (auch im --script-Modus nutzbar)
│   │   ├── uhrwerk_paths.gd       UhrwerkPaths: Datenordner (UHRWERK_DATEN oder res://../data)
│   │   ├── simulation_client.gd   SimulationClient: typisierte Hülle um UhrwerkSimulation
│   │   ├── location_catalog.gd    LocationCatalog: unveränderliche Ortsangaben (einmal gelesen)
│   │   ├── map_data.gd            MapData: provinzen.png, fluesse.png, definition.csv
│   │   ├── map_config.gd          MapConfig: karte.chron (Ausschnitt, Projektion)
│   │   ├── map_modes.gd           MapModes: Kartenmodi, Namen, Tasten
│   │   ├── german_format.gd       GermanFormat: Tausenderpunkt, Dezimalkomma, Einheiten
│   │   ├── save_slots.gd          SaveSlots: Ordner, Dateinamen und Liste der Spielstände, Autosave (seit M6)
│   │   ├── election_tally.gd      ElectionTally: Meldereihenfolge, Hochrechnung, Halbrund (seit M8)
│   │   ├── user_settings.gd       UserSettings: Einstellungen in user://einstellungen.cfg (seit M13a5d)
│   │   ├── accessibility.gd       Accessibility: Farbsehschwäche, Kontrast, Animationen (seit M13a5d)
│   │   ├── pause_rules.gd         PauseRules: automatische Pause aus der Chronik (seit M13a5d)
│   │   ├── lexicon.gd             Lexicon: Artikel aus data/lexikon/*.md (seit M13a5d)
│   │   ├── glossary.gd            Glossary: Begriffe der verschachtelten Tooltips (seit M13a5c)
│   │   ├── chronicle_export.gd    ChronicleExport: Chronik als HTML (seit M13a4)
│   │   └── launch_options.gd      LaunchOptions: --screenshot, --tage, --kartenmodus, --ort, --zoom
│   ├── map/
│   │   ├── map_view.gd            MapView: Darstellung, Nachschlagetexturen, Auswahl, Rahmen
│   │   ├── map_camera.gd          MapCamera: Ziehen, Zoomen, Übersicht
│   │   ├── map_detail.gd          MapDetail: Detailstufen nach Zoom (Grenzen, Namen)
│   │   ├── map_labels.gd          MapLabels: Namen im Bildschirmraum mit Rastern
│   │   ├── map_label_layout.gd    MapLabelLayout: Namenspunkte von Staaten und Kreisen
│   │   ├── railway_layer.gd       RailwayLayer: Eisenbahnstrecken über der Karte (seit M7)
│   │   └── army_layer.gd          ArmyLayer: Heeresmarken, Wege, Schlachten, Belagerungen (seit M10)
│   └── ui/                    Skripte der Panels, Legendenberechnung, Farbpalette; Wahlabend (election_night.gd) und Parteipanel (party_panel.gd, beide seit M8); Fenster „Diplomatie“ (diplomacy_window.gd) und Krisenleiste (play_bar.gd, beide seit M9); Heeresfenster (army_panel.gd), Kriegsfenster (war_window.gd) und Schlachtberichte (battle_reports.gd, alle seit M10)
├── shaders/map.gdshader       Kartenshader (canvas_item)
├── theme/einigkeit_theme.tres Theme der Oberfläche (Papier und Tinte)
└── tests/                     Einheiten-, Rauch-, Klick-, Leistungs- und Durchspieltest, siehe tests/README.md
```

### Spielstände (seit M6)

Spielstände liegen in `user://spielstaende/` (unter Linux
`~/.local/share/godot/app_userdata/EINIGKEIT/spielstaende/`), Format und Prüfungen in
[docs/technik/speicherstaende.md](../docs/technik/speicherstaende.md). Das **Spielmenü**
(`scripts/ui/game_menu.gd`, im Code aufgebaut) öffnet sich über den Knopf „Menü“ rechts in der
oberen Leiste oder F10 und hält die Zeit an: Name des Spielstands (vorbelegt mit Staat und Datum),
Liste der Spielstände (zuletzt geänderte zuerst, mit Datum, Staat und Änderungszeit; Dateien, die
nicht zu den Spieldaten passen oder keine Spielstände sind, erscheinen ausgegraut mit dem Grund
als Tooltip; Spielstände des anderen Lesezeichens sind ladbar und wechseln es), Neues Spiel
(„Starten“ verlangt einen zweiten Klick, weil die laufende Partie endet), Laden, Schnellspeichern, Schnellladen, Weiterspielen; seit M8 „Letzte Wahl“ (Wahlabend mit dem ganzen Ergebnis) und die Rolle (Staat oder Partei, steht im Spielstand). Die Namen `schnellspeicher`
und `autosave_…` sind reserviert. F5 und F9 wirken auch ohne
Menü. Am **1. Januar** speichert das Spiel automatisch; `autosave_1` ist der neueste, die drei
letzten bleiben. Nach dem Laden steht die Zeit, die Auswahl ist aufgehoben, und Grenzen, Namen,
Panels, Legende und Chronik zeigen den geladenen Stand. Rückmeldungen („Schnellgespeichert“,
„Geladen: …“) erscheinen drei Sekunden lang unter dem Datum.

### Simulation

`SimulationClient` spricht die Klasse `UhrwerkSimulation` nur über `ClassDB` an. Ein direkter
Verweis auf den Klassennamen wäre ohne gebaute Erweiterung ein Parse-Fehler; so startet das
Projekt in jedem Fall. Beim Start prüft die Hülle, ob alle Methoden der Schnittstelle vorhanden
sind (seit M3 auch `get_location_owner_indices`, `get_location_kreis_indices`, `get_kreis*`; seit M5 `set_tax_percent`, `get_market*`, `get_location_market_indices`; seit M6 `save_game`, `load_game`, `get_save_info`, `get_save_format_version`),
und meldet eine veraltete Erweiterung verständlich.

Die Zeit läuft in `main.gd`: Je Bild werden die fälligen ganzen Tage mit `advance_days`
simuliert (höchstens 30 je Bild und seit M6 höchstens bis zum nächsten 1. Januar, damit der
automatische Spielstand den Jahreswechsel trifft), danach liest die Oberfläche Datum, Staat, Ort, Kartenfarben und
neue Chronikeinträge. Die Steuerregler schicken `set_tax_percent` mit der Steuerart (seit M5;
vorher `set_tax_rate_percent` für den einen Steuersatz); der Kern führt den Befehl am nächsten Tag
aus, bis dahin zeigt das Panel „gilt ab dem nächsten Tag“.

Im Kartenmodus **Märkte** zeichnet die Karte statt der Kreis- die Marktgrenzen (dieselbe Ebene des
Shaders, gefüttert mit `get_location_market_indices`); die Farben der aktiven Märkte folgen
nacheinander der Palette, damit sich Nachbarn unterscheiden. Die Modusleiste hat seit M5 keinen
Titel mehr und schmalere Knöpfe, damit zehn Modi bei 1.600 Pixeln Breite zwischen die Spalten
passen.

Abfragen über alle Orte sind gebündelt, damit nichts je Bild 4.500-mal die Brücke ruft:
`LocationCatalog` liest Name, Stadt, Gelände, Kreis und „hat Daten“ einmal nach dem Laden
(diese Angaben ändern sich während einer Partie nicht); Besitzer und Kreise kommen als zwei
Indexarrays (`get_location_owner_indices`, `get_location_kreis_indices`), die Farben als ein
Array je Kartenmodus (`get_map_colors`). Die Legende wird monatlich nur in den Modi neu
aufgebaut, deren Werte sich ändern (Politisch, Dichte, Konfession, Verwüstung).

### Karte

**Daten** ([chronik-skript.md, Abschnitt 6](../docs/technik/chronik-skript.md)):

- `provinzen.png` (4096 × 3329 Pixel) wird mit `Image.load_from_file` geladen. Das `Image`
  bleibt für die Klick-Erkennung im Speicher, eine `ImageTexture` dient der Darstellung.
  `R + G × 256` ist Kartenindex + 1 (0 = kein Ort), der Blaukanal die Flächenart:
  0 Kernraum-Land, 1 Meer, 2 Binnengewässer, 3 Ausland.
- `fluesse.png` (Graustufen, Deckung durch Flüsse) wird mit Mipmaps hochgeladen; beim
  Herauszoomen mittelt die Grafikkarte die Deckung, statt zu flimmern.
- `definition.csv` (`kartenindex;ort;name;x;y;pixel;flaeche_km2`) ordnet Kartenindizes
  Ort-Kennungen zu und liefert je Ort den Beschriftungspunkt (ein Pixel, das garantiert zum
  Ort gehört), die Pixelzahl und die Fläche. `get_location_index` verbindet Karten- und
  Simulationsindex; beide sind unabhängig. Eine Datei im Format vor M3
  (`kartenindex;ort;name`) wird mit einem Hinweis abgelehnt.

**Nachschlagetexturen.** Der Shader holt je Pixel Farbe, Besitzer und Kreis über den
Kartenindex aus zwei Texturen mit 256 Spalten (4.542 Orte → 256 × 18 Texel); eine einzeilige
Textur wäre breiter als die sichere Texturgröße im Kompatibilitätsrenderer.
`color_lut` enthält die Farbe aus `get_map_colors` (Alpha 1 = Farbe, Alpha 0 = keine Daten,
Alpha 0,5 = Ort der Karte, den die Simulation nicht kennt). `info_lut` enthält Besitzer + 1
(R + G × 256) und Kreis + 1 (B + A × 256). Beide werden nur neu geschrieben, wenn sich die
Werte geändert haben (Farben: über `PackedColorArray.to_byte_array` und eine native
Umwandlung, rund 1 ms).

**Shader** (`shaders/map.gdshader`):

- Meer, Binnengewässer und Ausland haben eigene, gedämpfte Farben; Orte die Farbe ihres
  Kartenmodus. Orte ohne Daten (Alpha 0 in den Modi Politisch, Dichte, Konfession,
  Verwüstung) und Kernraum-Land ohne Ort tragen eine diagonale Schraffur „keine Daten“. Sie
  ist an der Karte verankert; ihr Abstand bleibt auf dem Bildschirm etwa gleich, weil die
  Periode in Zweierpotenzen springt und dazwischen überblendet wird.
- Flüsse aus `fluesse.png` über allem außer dem Meer.
- Grenzen entstehen durch Nachbarvergleich im Bildschirmraum (Abstand ein Bildschirmpixel),
  damit sie bei jedem Zoom gleich dick sind: Staatsgrenzen (auch Kernraum ↔ Ausland) immer und
  kräftig, Kreisgrenzen ab mittlerem Zoom, Ortsgrenzen erst nah (Deckkraft aus `MapDetail`,
  im Kartenmodus „Kreise“ sind Kreisgrenzen immer deutlich). Küste und Ufer zeichnen beide
  Seiten der Grenze Land ↔ Wasser.
- Besitzer und Kreis der Nachbarn liest der Shader nur, wenn deren Kartenindex abweicht; im
  Inneren eines Ortes entfällt die Grenzsuche ganz.
- Ausgewählter Ort: Umriss in Siegelrot; überfahrener Ort: aufgehellt. Papierton und eine
  feine Körnung (Rauschtextur mit festem Startwert) geben der Karte die Anmutung eines
  gedruckten Atlas; der Rahmen mit Gradeinteilung folgt den Grenzen aus `karte.chron`.

<a id="detailstufen"></a>**Detailstufen.** Maß ist die Bildschirmgröße eines typischen
Ortes, s = Zoom × √(Median der Pixel je Ort); auf der Kernraum-Karte (Median 1.212 Pixel)
ist ein typischer Ort etwa 35 Kartenpixel breit, in der Übersicht (Zoom ≈ 0,18 bei
1600 × 900) also etwa 6 Bildschirmpixel. So gelten die Stufen für jede Kartenauflösung gleich (`MapDetail`):

| Stufe | Ortsgröße s | Grenzen | Namen |
|---|---|---|---|
| fern | unter 17 px | Staaten, Küste | Staatsnamen |
| mittel | 17 bis 40 px | dazu Kreise (ab s ≈ 10 eingeblendet) | Kreisnamen |
| nah | ab 40 px | dazu Orte (ab s ≈ 28 eingeblendet) | Ortsnamen |

**Namen** (`MapLabels`, `MapLabelLayout`) werden im Bildschirmraum gezeichnet und bleiben
dadurch bei jedem Zoom scharf. Je Zoom ist genau eine Ebene sichtbar (hartes Umschalten an den
Schwellen; eine Überblendung zeigte beide Ebenen halb durchsichtig übereinander). Ist die Ebene
der Zoomstufe leer – ohne Kreisdaten etwa die Kreisebene –, erscheint die nächste nicht leere,
die genauere zuerst (mittlere Stufe ohne Kreise: Ortsnamen).

- *Ortsnamen* stehen am Beschriftungspunkt aus `definition.csv`. Rangfolge: Hauptstädte,
  Hauptorte der Kreise (ein Kreis heißt nach seinem größten Ort), übrige Städte, übrige Orte;
  bei Gleichstand der flächengrößere. Ist der Platz belegt, weicht ein Ortsname eine Zeile
  nach oben oder unten aus. Der Name des ausgewählten Ortes wird immer zuerst gesetzt, im
  selben Stil wie ohne Auswahl (eine ausgewählte Hauptstadt bleibt fett).
- *Kreisnamen* (kursiv) stehen im Schwerpunkt der Beschriftungspunkte der Orte des Kreises,
  gewichtet mit deren Pixelzahl; liegt dieser Punkt nicht im Kreis, rückt er auf den
  nächstgelegenen Ort des Kreises. Ein Kreisname bleibt auch dann stehen, wenn sein
  gleichnamiger Hauptort ausgewählt ist und an derselben Stelle steht.
- *Staatsnamen* (gesperrte Großbuchstaben, Größe nach Fläche) stehen ebenso im
  pixelgewichteten Schwerpunkt. Die Orte eines Staates werden zu Gebieten gebündelt
  (Beschriftungspunkte höchstens 2,5 Ortsdurchmesser auseinander); beschriftet werden das
  größte Gebiet und jedes weitere mit mindestens 35 % davon und mindestens acht typischen
  Orten (Exklaven). Ist schon das größte Gebiet kleiner, steht ein Name im Schwerpunkt aller
  Orte des Staates.
- Jede Ebene liegt in einem Weltraster (256 Kartenpixel je Zelle): Je Bild werden nur die
  Namen der sichtbaren Zellen geprüft, nicht alle 4.500 Orte. Überlappungen verhindert ein
  Belegungsraster im Bildschirmraum (8 Pixel je Zelle); Panels und Leisten sind darin vorab
  belegt, damit keine Namen unter halbtransparenten Panels durchscheinen. Gezeichnet wird nur
  nach einer Kamerabewegung, einer neuen Auswahl oder wenn ein Panel Größe, Lage oder
  Sichtbarkeit ändert (`MapLabels.set_covering_controls`, etwa die Legende beim Moduswechsel);
  Textbreiten werden beim ersten Gebrauch gemessen und gemerkt.

**Auswahl.** `MapView.location_at_screen(punkt)` rechnet einen Bildschirmpunkt über die
Kamera in Kartenpixel um und liest den Kartenindex aus dem Bild; Klick und Hervorhebung nutzen
diese eine Funktion. Der Klicktest prüft sie für jeden Ort bei drei Zoomstufen
(`tests/klick_test.gd`).

**Kamera.** Das Spiel startet mit der Übersicht über die ganze Karte im freien Bereich
zwischen den Panels (Pos1 kehrt dorthin zurück, H springt zur Hauptstadt). Zoomgrenzen: 0,8 ×
Übersicht bis Zoom 6 (ein typischer Ort ist dann etwa 210 Pixel breit). Das Mausrad zoomt in
Stufen von 1,2 mit Glättung zum Mauszeiger hin.

**Ortsinfo und Tooltip.** Für Orte ohne Daten zeigt die Ortsinfo „Keine Daten“, Art, Kreis,
Gelände und Fläche und einen Hinweis, dass der Ort nicht an der Simulation teilnimmt;
Bevölkerung, Wirtschaft, Verwüstung und Konfessionen sind ausgeblendet. Der Tooltip nennt
Besitzer und Kreis (bzw. „Keine Daten · Kreis … · Fläche“), im Kartenmodus „Kreise“ nur den
Kreis.

### Legenden

Die Legenden werden aus den Farben der Simulation (`get_map_colors`) abgeleitet, damit sie
immer zur Karte passen. Für Dichte und Verwüstung schätzt `LegendData` den Verlauf aus den
Orten mit Daten: Passt eine lineare Abbildung Wert → Farbe, zeigt die Legende die exakten
Endfarben, sonst die tatsächlich vorkommenden Farben. In den datenabhängigen Modi steht
zusätzlich ein Feld „Keine Daten“ mit der Schraffur. Der Modus „Kreise“ zeigt die Palette der
Brücke (12 Farben, `palette[kreisindex % 12]`, ohne Bedeutung) und die Zahl der Kreise mit
Orten und der Orte in Kreisen (ein leerer Kreis ist in den Daten erlaubt, erscheint aber nicht
auf der Karte). Die Farben wiederholen sich: Innerhalb einer Landschaftsdatei haben benachbarte
Kreise fast immer verschiedene Farben, an den Grenzen zweier Landschaften nicht immer (mit den
Daten des Kernraums 61 von 1.747 benachbarten Kreispaaren, 60 davon an Landschaftsgrenzen,
z. B. Karlsruhe und Mannheim). Die Kreisgrenzen sind in
diesem Modus deshalb immer deutlich gezeichnet.

### Leistung

Gemessen mit `tests/leistung_test.gd` (Fenster 1600 × 900, echte Daten mit 4.541 Orten)
unter xvfb mit dem **Software-Renderer llvmpipe** auf 4 Prozessorkernen, also ohne
Grafikkarte; Mittelwerte je Bild:

| Abschnitt | ms je Bild | Bilder/s |
|---|---|---|
| Übersicht, ruhend / beim Schwenken | 31 / 31 | 32 / 33 |
| Schwenken, mittlere Zoomstufe | 61 | 16 |
| Schwenken, nahe Zoomstufe | 61 | 16 |
| Zoomfahrt mit dem Mausrad | 54 | 18 |
| Simulation 120 Tage/s, mittlere Zoomstufe | 60 | 17 |

Der Kartenshader macht mit llvmpipe rund 40 ms aus (1,44 Millionen Bildpunkte, je Pixel
fünf Zugriffe auf das ID-Bild, an Grenzen bis zu fünf auf `info_lut`), die halbtransparenten
Panels mit Schatten rund 14 ms, die Namen 1–9 ms (GDScript). Eine Grafikkarte erledigt den
Shader in einem Bruchteil einer Millisekunde; das Ziel „60 Bilder/s auf einem
Mittelklasse-Rechner“ (Abnahme M3) ließ sich hier mangels Grafikkarte nicht messen.

Beseitigte Engpässe: Der Kartenrahmen bestand aus sieben gefüllten Rechtecken, die größer
als die Karte waren und beim Hineinzoomen den ganzen Bildschirm mehrfach überdeckten (jetzt
Ringe; Grundlast 38 → 5,5 ms). Besitzer und Kreis der Nachbarn liest der Shader nur noch an
Grenzen, die Körnung kommt aus einem bilinearen statt zwei trilinearen Zugriffen
(Shader 71 → 45 ms). Mittlere Zoomstufe insgesamt: 113 → 61 ms je Bild.

## Bekannte Grenzen des Prototyps

- Wappen sind schlichte Schilde in der Staatsfarbe.
- Für Orte ohne Daten kennt das Spiel keine Einwohnerzahl; die Rangfolge der Ortsnamen
  stützt sich deshalb auf Stadt, Kreishauptort und Fläche. Unter zwei benachbarten Städten
  gewinnt daher nicht immer die bekanntere (die ausgewählte steht immer).
- Staatsnamen eines zersplitterten Staates stehen im Schwerpunkt aller seiner Orte, auf dem
  nächstgelegenen eigenen Ort; bei weit verstreuten Orten (Testregion 1648) ist das nur eine
  grobe Lage.
- Die Panels der Oberfläche (Theme aus M1) haben weiche Schatten; mit Software-Rendering
  kosten sie spürbar Zeit.
- Mehr als 65.534 Kartenindizes passen nicht in R + G (Vertrag der Karte).
