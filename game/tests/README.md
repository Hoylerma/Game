# Tests der Godot-Seite

| Test | Braucht die GDExtension | Ausgabe bei Erfolg |
|---|---|---|
| `unit_test.gd` – Hilfsklassen ohne Simulation | nein | `UNIT OK` |
| `smoke_test.gd` – Brücke und Kartendaten | ja | `SMOKE OK` |

`godot` steht für Godot 4.4.1, z. B. `tools/godot/Godot_v4.4.1-stable_linux.x86_64`. Beide Tests
laufen im `--script`-Modus und brauchen einen vorherigen Import, damit Godot die Klassennamen
(`class_name`) kennt:

```bash
godot --headless --path game --import
```

## Einheitentests (`unit_test.gd`)

Prüft ohne Simulation: deutsche Zahlenformate (`GermanFormat`), Kommandozeilenoptionen
(`LaunchOptions`), das Lesen von `karte.chron` und die Projektion (`MapConfig`), die
Pfadauflösung (`UhrwerkPaths`), die Verlaufsschätzung der Legenden (`LegendData`) sowie die
Kartenmodi und die Eingabeaktionen aus `project.godot`.

```bash
godot --headless --path game --script res://tests/unit_test.gd
```

Am Ende steht `UNIT OK` (Exitcode 0) oder `UNIT FEHLGESCHLAGEN` mit allen Fehlern (Exitcode 1).

## Rauchtest (`smoke_test.gd`)

Prüft die GDExtension und die Kartendaten ohne Szene und ohne Grafikausgabe:

1. `ClassDB.class_exists("UhrwerkSimulation")` und alle Methoden der Schnittstelle
   ([bridge-api.md](../../docs/technik/bridge-api.md)) vorhanden
2. vor dem Laden leere Abfragen; ein falscher Datenordner liefert eine Fehlermeldung
3. Daten laden; Anzahl Staaten und Orte, Spielerstaat, Bevölkerung und Fläche > 0; alle
   Felder von `get_state` und `get_location`; `get_location_index` passt zu jedem Ort
4. ungültige Indizes und Befehle werden abgewiesen (leere Rückgabe bzw. `false`)
5. `get_map_colors` liefert für alle fünf Kartenmodi eine Farbe je Ort
6. Steuerbefehl: `set_tax_rate_percent` wird angenommen und ist nach einem Tag wirksam
7. 365 Tage simulieren; Datum (Kalenderrechnung im Test), `get_days_elapsed`, Datumstext
8. Einnahmen, Ausgaben und Produktion > 0
9. Chronik: Größe, Einträge mit allen Feldern, leere Abfrage hinter dem Ende
10. Prüfsumme 16-stellig hexadezimal; ein zweiter Lauf mit denselben Befehlen ergibt dieselbe
11. Karte: `provinzen.png` und `definition.csv` vorhanden, jeder Eintrag der CSV über
    `get_location_index` auflösbar, jeder Ort der Simulation in der CSV, keine Lücken,
    Bildgröße wie in `karte.chron`, jeder Pixel mit gültigem Kartenindex und B = 0, jeder Ort
    mit mindestens einem Pixel

Am Ende steht `SMOKE OK` (Exitcode 0) oder `SMOKE FEHLGESCHLAGEN` mit allen Fehlern (Exitcode 1).

```bash
# aus dem Repository-Hauptordner, nach dem Import
godot --headless --path game --script res://tests/smoke_test.gd

# anderer Datenordner (z. B. mit frisch erzeugter Karte)
python3 tools/kartengenerator/kartengenerator.py --ausgabe /tmp/karte
UHRWERK_DATEN=/tmp/daten godot --headless --path game --script res://tests/smoke_test.gd

# ohne Kartenprüfung
godot --headless --path game --script res://tests/smoke_test.gd -- --ohne-karte
```

## Start ohne Erweiterung

Das Projekt muss auch ohne `game/bin/libuhrwerk_bridge.so` ohne Skriptfehler starten und einen
Hinweis zeigen:

```bash
godot --headless --path game --quit-after 30 2>&1 | grep -E "SCRIPT ERROR|Parse Error" && echo "Fehler!"
```

Godot selbst meldet dabei, dass die Bibliothek der Erweiterung fehlt
(`ERROR: GDExtension dynamic library not found`); das ist erwartet.

## Screenshot

```bash
xvfb-run -a -s "-screen 0 1600x900x24" \
  godot --path game --rendering-driver opengl3 --audio-driver Dummy \
  -- --screenshot=/tmp/einigkeit.png --tage=60 --kartenmodus=0 --ort=berlin_coelln
```

`--audio-driver Dummy` vermeidet Warnungen auf Rechnern ohne Soundkarte. Mit dem
Software-Renderer (llvmpipe) meldet Godot außerdem, dass sich V-Sync nicht einstellen lässt;
auch das ist erwartet.
