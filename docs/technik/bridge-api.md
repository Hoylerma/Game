# GDExtension-Brücke: Schnittstelle für GDScript (Meilensteine M1 bis M6)

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
| `get_player_state_index()` | `int` | Index des Spielerstaates oder -1: aus dem Szenario, nach `load_game` aus dem Spielstand |
| `get_scenarios(data_path: String)` | `Array` | seit M7: Szenarien des Datenordners, ohne sie zu laden (auch ohne geladene Simulation); je Dictionary `id`, `name`, `date_text`, `year` (int), `player_tag`, `playable` (PackedStringArray der Staatstags), `currency` („Reichstaler“, wenn das Szenario keine nennt) |
| `get_scenario()` | `Dictionary` | seit M7: das geladene Szenario wie ein Eintrag von `get_scenarios`; leer ohne Simulation |
| `set_player_state(state_index: int)` | `bool` | seit M7: Spielerstaat wählen (nur Oberfläche und Spielstand, die Simulation kennt keinen Spieler); `false` bei ungültigem Index |
| `get_state_count()` | `int` | |
| `get_state_index(tag: String)` | `int` | -1, wenn unbekannt |
| `get_state(index: int)` | `Dictionary` | `tag`, `name`, `short_name`, `color` (Color), `ruler`, `government`, `capital_index` (int), `treasury` (float), `tax_rate_percent` (int, gerundet), `income_month` (float), `expenses_month` (float), `population` (int), `location_count` (int); seit M4 `living_standard` (float 0–100, nach Einwohnern gemittelt) und `balance_previous` (Dictionary wie bei `get_location`, Summe der Orte); seit M5 der Haushalt: `tax_rates_percent` (Array mit 3 int, Index = Steuerart), `taxes_month` (Array mit 3 float), `court_month`, `administration_month`, `army_month`, `interest_month`, `building_month` (float, Ausgaben je Posten), `debt`, `interest_percent`, `yearly_income` (gleitende Jahreseinnahmen), `army_per_mille` (float), `soldiers`, `bankruptcies`, `premium_months` (int, Monate mit Zinsaufschlag nach einem Bankrott). `tax_rate_percent` ist die Kontribution. `income_month` und `expenses_month` sind seit M5 Summe der Steuern bzw. der Posten des Haushalts vom letzten Rechentag; vor dem ersten Rechentag die beim Start geschätzten Werte (vorher 0); seit M7 `zollverein` (bool) und `bund` (bool, mindestens ein Ort im Bundesgebiet) |
| `get_location_count()` | `int` | |
| `get_location_index(id: String)` | `int` | -1, wenn unbekannt |
| `get_location(index: int)` | `Dictionary` | `id`, `name`, `owner_index` (int), `owner_name`, `terrain`, `is_city` (bool), `population` (int), `area_km2` (float), `density` (float, Einwohner je km²), `devastation_percent` (int), `majority_confession`, `confessions` (Dictionary Name → Prozent als float), `production_month` (float), `taxes_month` (float), `source`; seit M3 zusätzlich `has_data` (bool), `kreis_index` (int, −1 ohne Kreis), `kreis_name` (String); seit M4 `majority_culture` (String), `cultures` (Dictionary Name → Prozent als float), `children`, `young`, `working`, `old` (int: 0–14, 15–29, 30–64, 65+ Jahre), `living_standard` (float 0–100), `literacy_percent` (float), `balance_previous` (Dictionary `births`, `deaths`, `plague_deaths`, `moved_in`, `moved_out`, `emigrated` als int, Bilanz des Vorjahres; im ersten Jahr 0); seit M5 `market_index` (int, −1 ohne Markt), `market_name`, `is_port` (bool), `development_percent` (float, Zuschlag des Landesausbaus auf die Tragfähigkeit), `production_per_capita` (float, Reichstaler je Einwohner und Jahr), `professions` (Array von `{ name, stratum (0 unten, 1 mittel, 2 oben), people, income_year (je Kopf), coverage_percent (Mittel über die Bedürfnisse), food_percent (Grundnahrung) }`), `businesses` (Array von `{ name, profession, people }`), `goods` (Array von `{ name, color, supply, demand (je Monat), price, price_factor (zum Basispreis) }`, nur Güter mit Angebot oder Nachfrage im Ort); für Orte ohne Daten sind die drei Arrays leer. Bevölkerung, Anteile und Mittelwerte umfassen auch die Zugänge, die noch keinen eigenen Block haben (bevoelkerung.md, Abschnitt 4.7). `production_month` und `taxes_month` sind die Werte vom letzten Rechentag des Ortes. Für Orte ohne Daten: `has_data = false`, `owner_index = -1`, `owner_name = ""`, Simulationszahlen (`population`, `density`, `devastation_percent`, `production_month`, `taxes_month`) 0, `majority_confession = ""`, `confessions` leer; Kartenfelder (`terrain`, `is_city`, `area_km2`, Kreis) wie in den Daten; seit M7 `verwaltung` (Array der Verwaltungseinheiten von oben nach unten, je Dictionary mit `index`, `id`, `name`, `kind`; leer ohne Zuordnung) und `bundesgebiet` (bool) |
| `get_location_owner_indices()` | `PackedInt32Array` | seit M3: Besitzer-Staatsindex je Ort (−1 ohne Daten), Länge = `get_location_count()` – für Grenzen im Shader ohne 4.500 Einzelabfragen |
| `get_location_kreis_indices()` | `PackedInt32Array` | seit M3: Kreisindex je Ort (−1 ohne Kreis) |
| `get_location_market_indices()` | `PackedInt32Array` | seit M5: Marktindex je Ort (−1 ohne Markt), Länge = `get_location_count()`; Grundlage der Marktgrenzen |
| `get_location_admin_indices()` | `PackedInt32Array` | seit M7: unterste Verwaltungseinheit je Ort (−1 ohne Einheit); Grenzen im Modus „Verwaltung“ |
| `get_admin_unit_count()` | `int` | seit M7 |
| `get_admin_unit(index: int)` | `Dictionary` | seit M7: `id`, `name`, `kind` (z. B. `provinz`), `parent_index` (int, −1 bei der obersten), `source`, `location_count` (Orte, deren unterste Einheit sie ist) |
| `get_market_count()` | `int` | seit M5 |
| `get_market(index: int)` | `Dictionary` | seit M5: `id`, `name`, `location_count` (Orte mit Daten), `active` (bool, mindestens ein Ort mit Daten), `is_port` (bool), `harvest` (float, Erntefaktor des Jahres), `goods` (Array je Gut: `id`, `name`, `color`, `base_price`, `price`, `price_factor`, `supply`, `demand`, `imports`, `exports`, `buy_percent`) |
| `get_kreis_count()` | `int` | seit M3 |
| `get_kreis_index(id: String)` | `int` | seit M3; −1, wenn unbekannt |
| `get_kreis(index: int)` | `Dictionary` | seit M3: `id`, `name`, `location_count` (int), `source` |
| `get_map_colors(mode: int)` | `PackedColorArray` | Eine Farbe je Simulations-Ortsindex (Länge = `get_location_count()`), siehe Kartenmodi |
| `set_tax_rate_percent(state_index: int, percent: int)` | `bool` | Reiht einen `SetTaxRate`-Befehl für die Kontribution ein; `false`, wenn Index ungültig oder Prozent außerhalb 0..Maximum |
| `set_tax_percent(state_index: int, kind: int, percent: int)` | `bool` | seit M5: wie oben für eine Steuerart (`TAX_KONTRIBUTION` 0, `TAX_AKZISE` 1, `TAX_ZOLL` 2); `false` auch bei unbekannter Art |
| `get_max_tax_rate_percent()` | `int` | Höchstsatz, gilt für alle drei Steuerarten |
| `get_chronicle_size()` | `int` | |
| `get_chronicle(from_index: int)` | `Array` | Einträge ab `from_index` als Dictionaries `{ "date_text": String, "text": String, "location_index": int, "state_index": int }` |
| `get_checksum()` | `String` | Prüfsumme als 16-stellige Hexadezimalzahl |

### Spielstände (seit M6)

Format, Prüfungen und Godot-Seite: [speicherstaende.md](speicherstaende.md). Pfade sind
Dateipfade des Betriebssystems (in GDScript `ProjectSettings.globalize_path("user://…")`).

| Methode | Rückgabe | Beschreibung |
|---|---|---|
| `save_game(path: String, player_state_index: int)` | `String` | Schreibt den Spielstand (Spielerstaat für die Metadaten; ungültiger Index = bisheriger Spielerstaat), legt den Ordner an. Leer bei Erfolg, sonst die Meldung |
| `load_game(data_path: String, path: String)` | `String` | Lädt die Spieldaten des Szenarios aus dem Spielstand frisch und den Zustand hinein. Ersetzt die laufende Simulation **nur bei Erfolg**; danach gelten Spielerstaat und Ladehinweise aus diesem Ladevorgang. Leer bei Erfolg, sonst die Meldung (z. B. „Spielstand passt nicht zu den Spieldaten (abweichend: orte)“) |
| `get_save_info(path: String)` | `Dictionary` | Ohne zu laden: `ok` (bool, Datei lesbar), `compatible` (bool, passt zum Inhalt der geladenen Spieldaten), `reason` (String, leer wenn passend), `scenario`, `date` (`{ year, month, day }`), `date_text`, `days` (int), `player_tag`, `player_name` (Kurzname, sonst Kennung), `format_version` (int), `game_version`, `checksum` (16-stellig hex). Bei `ok == false` nur `ok`, `compatible` und `reason` |
| `get_save_format_version()` | `int` | Formatversion, die diese Brücke schreibt (1) |

## Kartenmodi (`get_map_colors`)

| Wert | Konstante (GDScript-Seite) | Farbe je Ort |
|---|---|---|
| 0 | `MAP_POLITICAL` | Farbe des Besitzers |
| 1 | `MAP_DENSITY` | Verlauf nach Einwohnern je km² (0 → hell, ≥ 120 → dunkel) |
| 2 | `MAP_CONFESSION` | Farbe der Mehrheitskonfession |
| 3 | `MAP_TERRAIN` | Farbe des Geländes |
| 4 | `MAP_DEVASTATION` | Verlauf nach Verwüstung (0 → neutral, 1 → rot) |
| 5 | `MAP_KREISE` | seit M3: Farbe je Kreis aus einer festen Palette (`palette[kreisindex % n]`, benachbarte Kreisindizes unterschiedlich); Orte ohne Kreis grau |
| 6 | `MAP_LEBENSSTANDARD` | seit M4: linearer Verlauf nach mittlerem Lebensstandard (0 → rot, 100 → grün) |
| 7 | `MAP_KULTUR` | seit M4: Farbe der Mehrheitskultur (`common/kulturen.chron`) |
| 8 | `MAP_WIRTSCHAFT` | seit M5: linearer Verlauf nach Wirtschaftsleistung je Einwohner und Jahr (0 → hell, ≥ 16 Reichstaler → dunkelbraun) |
| 9 | `MAP_MAERKTE` | seit M5: Farbe je Markt aus der Palette von `MAP_KREISE`; die aktiven Märkte laufen nacheinander durch die Palette, damit sich Nachbarn unterscheiden |
| 10 | `MAP_VERWALTUNG` | seit M7: Farbe der obersten Verwaltungseinheit (Provinz, Kronland …) aus der Palette von `MAP_KREISE` (`palette[einheitsindex % n]`); die Grenzen der untersten Einheiten zeichnet Godot aus `get_location_admin_indices`; Orte von Staaten ohne Gliederung in der aufgehellten Staatsfarbe |

**Orte ohne Daten** (seit M3) erhalten in den Modi 0, 1, 2, 4, 6, 7, 8 und 10 eine Farbe mit **Alpha 0**;
die Godot-Seite zeichnet dort eine Schraffur „keine Daten“. In den Modi 3 und 5 haben sie normale
Farben (Alpha 1), im Modus 9 dann, wenn ihr Markt Orte mit Daten hat.

**Steuerarten** (seit M5) sind als Konstanten `TAX_KONTRIBUTION` (0), `TAX_AKZISE` (1) und
`TAX_ZOLL` (2) gebunden, die Kartenmodi als `MAP_*`.

**Threads (seit M4):** Die Brücke legt die Simulation mit `SimConfig::threads = 0` an, rechnet
also auf allen Kernen. Das Ergebnis ist bei jeder Thread-Zahl gleich (bevoelkerung.md,
Abschnitt 5).

## Bibliothek und Einbindung

- CMake-Ziel `uhrwerk_bridge` (Shared Library), Einstiegspunkt `uhrwerk_library_init`.
- Ausgabe nach `game/bin/` mit den Namen `libuhrwerk_bridge.so` (Linux), `uhrwerk_bridge.dll` (Windows), `libuhrwerk_bridge.dylib` (macOS).
- Beschreibung in `game/bin/uhrwerk.gdextension`, `compatibility_minimum = "4.4"`.
