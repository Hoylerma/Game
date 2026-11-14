# Spieltest – Auswertung

Seit M13b6 ([vertical_slice.md](../../docs/technik/vertical_slice.md), Abschnitt 10). Fasst die
Unterlagen des Spieltests zu den Tabellen in
[spieltest_m13_ergebnisse.md](../../docs/pruefung/spieltest_m13_ergebnisse.md) zusammen. Ablauf des
Tests: [spieltest_m13.md](../../docs/pruefung/spieltest_m13.md). Nur Standardbibliothek.

## Eingang

Ein Ordner (üblich: `spieltest/` im Repository-Hauptordner, von Git ausgenommen) mit je Tester:

- dem ausgefüllten Fragebogen `T03_fragebogen.txt` (Vorlage
  [spieltest_m13_fragebogen.txt](../../docs/pruefung/spieltest_m13_fragebogen.txt): je Zeile
  `ID [Typ] Frage: Antwort`, Typen `[1-5]`, `[0-3]`, `[0-10]`, `[ja/nein]`, `[Zahl]`, `[Text]`;
  eingerückte Folgezeilen setzen eine Antwort fort);
- den Rückmeldungen aus dem Spiel (ZIP mit `messwerte.json`, `notiz.txt`, `godot.log`), im Ordner
  `T03/` oder umbenannt in `T03_….zip`.

Die Kennung kommt aus dem Ordner, dem Dateinamen oder (beim Fragebogen) der Antwort `kennung`.

## Aufruf

```bash
python3 tools/spieltest/auswertung.py --eingang spieltest                 # Abschnitte auf stdout
python3 tools/spieltest/auswertung.py --eingang spieltest --streng        # Exitcode 1 bei ungültigen Antworten
python3 tools/spieltest/auswertung.py --eingang spieltest \
    --bericht docs/pruefung/spieltest_m13_ergebnisse.md                  # Abschnitte im Bericht ersetzen
python3 -m unittest discover -s tools/spieltest -v                        # Tests
```

Im Bericht ersetzt das Werkzeug nur den Inhalt zwischen `<!-- auswertung:NAME -->` und
`<!-- /auswertung:NAME -->`; zweimal auswerten ergibt dasselbe.

| Abschnitt | Inhalt |
|---|---|
| `teilnehmende` | Erfahrung (A1–A3), Geschichtsstunde, System, Version, Sitzungsdauer, Lesezeichen und Staat, erreichtes Datum, Ende der Partie (neueste Rückmeldung) |
| `technik` | Grafikkarte, Fenster, Vollbild, Bildratenlimit, Median und 5-%-Perzentil der Bildrate (längste Messung), Ziel 60 Bilder/s (Toleranz 2), Absturz (F2), Fehlerzeilen im Protokoll |
| `skalen` | je Skalenfrage n, Median, Mittel, Verteilung; **Befund** bei Median ≤ 2,5 (Skala 1–5); bei 0–10 Förderer (9–10), Kritiker (0–6) und Saldo |
| `texte` | Freitexte je Frage mit Kennung |
| `notizen` | Notizen aus dem Spiel mit Lesezeichen, Staat und Spieldatum |

Die Messwerte schreibt das Spiel (`game/scripts/core/feedback_report.gd`, Format 1): Version, System,
Grafikkarte, Fenster, Sprache, Bildrate (`FrameMeter`: Bilder je Sekunde über die Sitzung, ohne
minimierte oder unfokussierte Sekunden), Sitzungsdauer, Szenario, Spieldatum, Spieler, Stufe, Verlauf,
Ende der Partie, Prüfsumme. Pfade zum Benutzerordner stehen dort als „~“.
