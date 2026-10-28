# Vic3-Berater

KI-Berater für **Victoria 3**, der live neben dem Spiel läuft. Ein kleines Fenster bleibt über
dem Spiel; auf Tastendruck macht der Berater einen Screenshot, schickt ihn an Claude
(`claude-opus-5-5`) und sagt, welche Entscheidung jetzt am besten ist: welches Gebäude in
welchem Staat, welches Gesetz, welche Produktionsmethode, was am Haushalt oder in der
Diplomatie zu tun ist.

| Taste / Knopf | Wirkung |
|---|---|
| **F9** oder „Analysieren“ | Screenshot des aktuellen Bildschirms analysieren |
| **F10** | Fenster in den Vordergrund holen und eine Frage eintippen (Enter sendet; mit Screenshot, solange „Screenshot“ angehakt ist) |
| „Auto“ | in festen Abständen selbst prüfen; ein Tipp erscheint nur, wenn es etwas Neues gibt (Titel „★ Neuer Tipp“ und Signalton) |

Der Berater schaut auf das, was gerade offen ist. Für einen Rat zum Bauen also das Bau- oder
Marktfenster öffnen, für Gesetze das Politikfenster, dann F9. Fehlt ihm etwas, sagt er, welches
Fenster er als Nächstes sehen will. Das Gespräch läuft weiter: Fragen wie „Und in welchem Staat?“
beziehen sich auf die vorige Antwort.

## Einrichtung (Windows)

1. Python 3.10 oder neuer von python.org installieren (Haken bei „Add python.exe to PATH“).
2. Einen API-Schlüssel unter <https://platform.claude.com/> anlegen und als Umgebungsvariable
   setzen (einmalig, danach ein neues Terminal öffnen):
   ```bat
   setx ANTHROPIC_API_KEY "sk-ant-…"
   ```
3. In Victoria 3 unter *Einstellungen → Grafik* den Fenstermodus **Randloses Fenster**
   (*Borderless*) wählen. Im exklusiven Vollbild verdeckt das Spiel das Berater-Fenster.
4. `tools\vic3_berater\start.bat` doppelklicken. Beim ersten Start werden die Pakete installiert.

Linux und macOS:

```bash
cd tools/vic3_berater
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt
export ANTHROPIC_API_KEY="sk-ant-…"
.venv/bin/python berater.py
```

Unter Linux braucht das Fenster `python3-tk`; unter macOS muss das Terminal in den
Datenschutz-Einstellungen Bildschirmaufnahme und Bedienungshilfen (für die Tasten) erlauben.

## Optionen

```text
python berater.py --ziel "Deutschland einigen und Großmacht werden"   Ziel der Partie
python berater.py --auto 5            alle 5 Minuten automatisch prüfen
python berater.py --bei-autosave      bei jedem neuen Spielstand automatisch prüfen
python berater.py --aufwand high      gründlicher (langsamer, teurer); low = schnell und günstig
python berater.py --monitor 2         Spiel läuft auf dem zweiten Monitor
python berater.py --taste-analyse "<ctrl>+<alt>+a" --taste-frage "<ctrl>+<alt>+q"
python berater.py --spielstaende "D:\Spiele\save games"   anderer Ordner der Spielstände
python berater.py --spielstaende ""   Spielstände nicht beobachten
python berater.py --probe             nur Screenshot (probe.jpg) und Spielstand testen, ohne API
```

Unter Windows gehen dieselben Optionen an `start.bat`, z. B. `start.bat --auto 5`.

## Spielstände

Der Berater beobachtet den Ordner der Spielstände
(Windows: `Dokumente\Paradox Interactive\Victoria 3\save games`) und schickt beim nächsten Rat
einen Auszug des neuesten Spielstands mit (Spieldatum, Land, Version aus dem Block
`meta_data`). Normale Spielstände sind binär kodiert; dann zählt nur der Screenshot. Lesbare
Spielstände entstehen mit der Startoption `-debug_mode` (Steam: Victoria 3 → Eigenschaften →
Startoptionen), nicht im Ironman-Modus. Mit `--bei-autosave` löst jeder Autosave (die
Häufigkeit stellt man im Spiel ein) eine automatische Prüfung aus.

## Kosten und Datenschutz

Jede Analyse schickt einen Screenshot (bis 2576 Pixel breit, rund 2.000–5.000 Bild-Token) und den
bisherigen Verlauf an die Anthropic-API. Mit dem Standardaufwand `medium` kostet eine Analyse
grob 3 bis 10 Cent; der Verlauf wird zwischengespeichert (Prompt-Caching) und nach acht
Antworten zu einer Zusammenfassung verdichtet, damit die Kosten nicht wachsen. Der automatische
Modus mit `--auto 5` kommt so auf etwa 0,50 bis 1 € je Spielstunde; `--aufwand low` ist günstiger. Auf dem Screenshot ist alles zu sehen, was auf dem Monitor
liegt – andere Fenster vorher schließen oder `--monitor` passend wählen.

Wird eine Anfrage von den Sicherheitsfiltern abgelehnt, beantwortet sie automatisch ein
Ersatzmodell (`fallbacks: "default"`); abschalten mit `--ohne-fallback`.

## Aufbau

- `berater.py` – Fenster (tkinter, immer im Vordergrund), globale Tasten (pynput), automatische
  Prüfung, Kommandozeile.
- `beratung.py` – Systemprompt mit Victoria-3-Grundsätzen, Anfrage an Claude (Streaming,
  adaptives Denken, Prompt-Caching), Gesprächsverlauf. Der Verlauf wird nur angehängt; ist er
  voll, beginnt ein neues Gespräch mit einer Zusammenfassung der letzten Ratschläge.
- `bildschirm.py` – Screenshot (mss), verkleinert auf die Bildgrenzen von Claude, als JPEG.
- `spielstand.py` – Spielstände finden, auf neue warten, `meta_data` lesen (nur Standardbibliothek).
- `test_berater.py` – Tests ohne Netz und ohne Bildschirm:
  `python3 -m unittest discover -s tools/vic3_berater -v`
