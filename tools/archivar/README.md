# Archivar

Erzeugt die Historie der Spieldaten (`data/history/`) aus den Tabellen in `data/tabellen/` und
prüft sie dabei auf Verweise, Lücken und Ausreißer. Nur Python-Standardbibliothek (ab 3.10).

- Tabellenformat und Prüfungen: [docs/technik/tabellen.md](../../docs/technik/tabellen.md)
- Format der Ausgabe: [docs/technik/chronik-skript.md](../../docs/technik/chronik-skript.md), Abschnitte 3.5, 3.6, 3.8, 3.9

## Aufruf

```bash
python3 tools/archivar/archivar.py              # prüfen und data/history/ schreiben
python3 tools/archivar/archivar.py --pruefen    # ist data/history/ aktuell? (wie in der CI)
python3 tools/archivar/archivar.py --bericht    # Umfang, Sicherheit der Angaben, Szenarien
python3 tools/archivar/archivar.py --daten DIR  # anderer Datenordner
python3 -m unittest discover -s tools/archivar -v   # 44 Tests
```

Exitcodes: `0` in Ordnung, `1` `data/history/` ist nicht aktuell (`--pruefen`), `2` Fehler in den
Tabellen. Warnungen und Fehler erscheinen auf der Standardfehlerausgabe mit Tabelle und Zeile, z. B.
`Fehler: tabellen/besitz.csv:12: unbekannter Staat 'XYZ' als Besitzer von potsdam`.

## Arbeitsweise

1. Tabelle bearbeiten (Tabellenkalkulation mit „Trennzeichen: Semikolon“ oder Texteditor).
2. `python3 tools/archivar/archivar.py` – schreibt geänderte Dateien und entfernt Dateien in
   `history/orte`, `history/staaten` und `history/kreise`, die keine Tabelle mehr erzeugt.
3. Bei geänderten Orten auch die Karte neu erzeugen: `python3 tools/kartengenerator/kartengenerator.py`.
4. `./build/dev/cli/uhrwerk-cli pruefen` und die Tabellen samt erzeugter Dateien committen.

Die Dateien in `data/history/` tragen den Kopf „Erzeugt von tools/archivar … – nicht von Hand
bearbeiten“. Änderungen dort gehen beim nächsten Lauf verloren.

## Aufbau

- `archivar.py` – Tabellen lesen (`lese_tabelle`), Bezugsdaten aus `common/`, `szenarien/`,
  `karte/` (`lade_bezug`), Modell aufbauen und prüfen (`baue_archiv`, `pruefe_zeitraeume`,
  `pruefe_ausreisser`), Ausgabe erzeugen (`erzeuge`) und vergleichen bzw. schreiben.
- Den Parser für das Chronik-Skript teilt sich der Archivar mit dem Kartengenerator
  (`tools/kartengenerator/chron.py`).
- `testdaten/` – kleiner Datensatz (2 Staaten, 2 Kulturen, 2 Kreise, 4 Orte davon einer ohne Daten, 2 Szenarien,
  ein Besitzwechsel); die
  eingecheckten Dateien unter `testdaten/history/` sind die erwartete Ausgabe.

Die Ausgabe ist deterministisch: Daten sind nach Datum sortiert, Felder in fester Reihenfolge,
Werte werden als Text aus der Tabelle übernommen (keine Gleitkomma-Umwandlung).

## Orte ohne Daten und Kreise (seit M3)

- Ein Ort ohne Zeilen in `besitz.csv` und `bevoelkerung.csv` ist ein **Ort ohne Daten**: Er steht
  auf der Karte, nimmt aber nicht an der Simulation teil. Zu jedem Szenariodatum hat ein Ort
  Besitzer **und** Bevölkerung oder keines von beiden; Hauptstädte müssen Orte mit Daten sein.
- `kreise.csv` erzeugt `history/kreise/<datei>.chron`; die Spalte `kreis` in `orte.csv` verweist
  darauf. Orte ohne Kreis und Kreise ohne Orte sind Warnungen (Orte ohne Kreis als eine
  Sammelwarnung).
- Leistung: Mit den Daten von M3 (4.542 Orte, 626 Kreise, 170 Dateien) dauert `--pruefen` etwa
  0,2 s.

## Lesezeichen 1848 (seit M7)

- `verwaltung.csv` erzeugt `history/verwaltung/<datei>.chron` (Verwaltungseinheiten mit Art,
  übergeordneter Einheit und Sitz); `besitz.csv` setzt je Ort und Datum auch `verwaltung` und
  `bund`, `staaten_historie.csv` das Feld `zollverein` (seit M8 auch `verfassung`, `wahlrecht`,
  `wahlsystem`, `wahl_indirekt` und `wahl_zensus`). Unbekannte Einheiten, Kreise in `ueber` und
  eine Einheit eines anderen Staates sind Fehler.
- Seit M9 heißt das Feld `zollverein` `zollunion`: eine Union aus `common/zollunionen.chron` (die
  Datei darf fehlen) oder `keine`. Eine unbekannte Union und ein Datum vor ihrem `seit` sind Fehler.
- `eisenbahnen.csv` erzeugt `history/eisenbahnen/<datei>.chron`: je Zeile ein Abschnitt einer
  Strecke. Unbekannte Orte, gleicher Anfang und gleiches Ende und doppelte Abschnitte (auch auf
  verschiedenen Strecken) sind Fehler, Abschnitte über 120 km und gleiche Namen verschiedener
  Strecken Warnungen.
- `wahlkreise.csv` (seit M8) erzeugt `history/wahlkreise/<parlament>.chron`: je Zeile ein Wahlkreis
  mit Staat, Sitzen und Orten. Unbekannte Orte und Staaten, ein Ort in zwei Wahlkreisen desselben
  Parlaments und Sitze unter 1 sind Fehler; ob das Parlament besteht und wem die Orte gehören,
  prüft der Kern.
- `beziehungen.csv` (seit M9) erzeugt `history/beziehungen/grundwerte.chron`: je Zeile die
  Grundbeziehung eines Staatenpaares. Unbekannte Staaten, ein Staat zu sich selbst, ein Wert außerhalb
  von −200 bis 200 (nur ganze Zahlen) und ein doppeltes Paar (auch umgekehrt) sind Fehler.
- Ein Staat ohne Ort zum Szenariodatum besteht in diesem Szenario nicht; seine Pflichtfelder werden
  dort nicht verlangt. Spielerstaat und spielbare Staaten (`spielbar`) müssen zum Szenariodatum
  bestehen.
