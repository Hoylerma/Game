# GDExtension-Brücke: Schnittstelle für GDScript (Meilenstein M1)

Die Brücke (`bridge/`) stellt dem Godot-Projekt genau eine Klasse bereit: **`UhrwerkSimulation`** (erbt von `RefCounted`). Sie besitzt einen `uhrwerk::Simulation`-Kern und übersetzt zwischen Godot-Typen und Kern-Typen.

**Regeln**

- Die Godot-Seite **liest** nur und schickt **Befehle**; sie verändert den Kern nie direkt.
- Werte, die in die Simulation gehen, werden als **Ganzzahlen** übergeben (z. B. Steuersatz in Prozent), damit keine Gleitkommazahlen in den Kern gelangen.
- Werte, die aus der Simulation kommen, dürfen `float` sein (nur Anzeige).
- Alle Methoden sind gegen ungültige Indizes abgesichert (leere Rückgabe statt Absturz).
- Vor erfolgreichem `load_scenario` liefern alle Abfragen leere Werte.

## Methoden

| Methode | Rückgabe | Beschreibung |
|---|---|---|
| `load_scenario(data_path: String, scenario_id: String = "westfaelischer_frieden", seed: int = 1648)` | `String` | Lädt Daten und erzeugt die Simulation. Leerer String bei Erfolg, sonst alle fatalen Ladefehler, zeilenweise |
| `is_loaded()` | `bool` | |
| `get_load_warnings()` | `PackedStringArray` | Nicht fatale Hinweise des letzten Ladevorgangs |
| `advance_days(days: int)` | – | Simuliert `days` Tage (0 ≤ days ≤ 3660) |
| `get_date()` | `Dictionary` | `{ "year": int, "month": int, "day": int }` |
| `get_date_text()` | `String` | `"24. Oktober 1648"` |
| `get_days_elapsed()` | `int` | |
| `get_scenario_name()` | `String` | |
| `get_player_state_index()` | `int` | Index des vorausgewählten Spielerstaates oder -1 |
| `get_state_count()` | `int` | |
| `get_state_index(tag: String)` | `int` | -1, wenn unbekannt |
| `get_state(index: int)` | `Dictionary` | `tag`, `name`, `short_name`, `color` (Color), `ruler`, `government`, `capital_index` (int), `treasury` (float), `tax_rate_percent` (int, gerundet), `income_month` (float), `expenses_month` (float), `population` (int), `location_count` (int) |
| `get_location_count()` | `int` | |
| `get_location_index(id: String)` | `int` | -1, wenn unbekannt |
| `get_location(index: int)` | `Dictionary` | `id`, `name`, `owner_index` (int), `owner_name`, `terrain`, `is_city` (bool), `population` (int), `area_km2` (float), `density` (float, Einwohner je km²), `devastation_percent` (int), `majority_confession`, `confessions` (Dictionary Name → Prozent als float), `production_month` (float), `taxes_month` (float), `source` |
| `get_map_colors(mode: int)` | `PackedColorArray` | Eine Farbe je Simulations-Ortsindex (Länge = `get_location_count()`), siehe Kartenmodi |
| `set_tax_rate_percent(state_index: int, percent: int)` | `bool` | Reiht einen `SetTaxRate`-Befehl ein; `false`, wenn Index ungültig oder Prozent außerhalb 0..Maximum |
| `get_max_tax_rate_percent()` | `int` | |
| `get_chronicle_size()` | `int` | |
| `get_chronicle(from_index: int)` | `Array` | Einträge ab `from_index` als Dictionaries `{ "date_text": String, "text": String, "location_index": int, "state_index": int }` |
| `get_checksum()` | `String` | Prüfsumme als 16-stellige Hexadezimalzahl |

## Kartenmodi (`get_map_colors`)

| Wert | Konstante (GDScript-Seite) | Farbe je Ort |
|---|---|---|
| 0 | `MAP_POLITICAL` | Farbe des Besitzers |
| 1 | `MAP_DENSITY` | Verlauf nach Einwohnern je km² (0 → hell, ≥ 120 → dunkel) |
| 2 | `MAP_CONFESSION` | Farbe der Mehrheitskonfession |
| 3 | `MAP_TERRAIN` | Farbe des Geländes |
| 4 | `MAP_DEVASTATION` | Verlauf nach Verwüstung (0 → neutral, 1 → rot) |

## Bibliothek und Einbindung

- CMake-Ziel `uhrwerk_bridge` (Shared Library), Einstiegspunkt `uhrwerk_library_init`.
- Ausgabe nach `game/bin/` mit den Namen `libuhrwerk_bridge.so` (Linux), `uhrwerk_bridge.dll` (Windows), `libuhrwerk_bridge.dylib` (macOS).
- Beschreibung in `game/bin/uhrwerk.gdextension`, `compatibility_minimum = "4.4"`.
