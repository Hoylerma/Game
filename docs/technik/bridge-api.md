# GDExtension-Brücke: Schnittstelle für GDScript (Meilensteine M1 bis M9)

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
| `get_state(index: int)` | `Dictionary` | `tag`, `name`, `short_name`, `color` (Color), `ruler`, `government`, `capital_index` (int), `treasury` (float), `tax_rate_percent` (int, gerundet), `income_month` (float), `expenses_month` (float), `population` (int), `location_count` (int); seit M4 `living_standard` (float 0–100, nach Einwohnern gemittelt) und `balance_previous` (Dictionary wie bei `get_location`, Summe der Orte); seit M5 der Haushalt: `tax_rates_percent` (Array mit 3 int, Index = Steuerart), `taxes_month` (Array mit 3 float), `court_month`, `administration_month`, `army_month`, `interest_month`, `building_month` (float, Ausgaben je Posten), `debt`, `interest_percent`, `yearly_income` (gleitende Jahreseinnahmen), `army_per_mille` (float), `soldiers`, `bankruptcies`, `premium_months` (int, Monate mit Zinsaufschlag nach einem Bankrott). `tax_rate_percent` ist die Kontribution. `income_month` und `expenses_month` sind seit M5 Summe der Steuern bzw. der Posten des Haushalts vom letzten Rechentag; vor dem ersten Rechentag die beim Start geschätzten Werte (vorher 0); seit M7 `bund` (bool, mindestens ein Ort im Bundesgebiet); seit M9 `customs_union` (Kennung der Zollunion) und `customs_union_name`, beide leer für ein eigenes Zollgebiet (bis M8 `zollverein`, bool), `customs_union_index` (int, −1 ohne), `customs_leader` (bool, Leitmacht seiner Union), `customs_tariff_percent` (float, Tarif seines Zollgebiets: der Union oder der eigene Zoll), `customs_locked` (bool, Zollregler gesperrt: Mitglied in einer Welt mit Zollunionen), `customs_income` (float, Anteil an der Vereinskasse im letzten Monat) und `customs_request` (offener Antrag `{ customs_union_index (−1 = Austritt), customs_union_name, effective_text, binding }`, sonst leer; `binding` seit Schritt b4: verbindlicher Beitritt nach einer Zolleinigung des Bundes oder einer verlorenen Krise, nicht mehr änderbar); seit M9, Schritt b3 `diplomacy` (bool, die Welt kennt Beziehungen: mit dem Deutschen Bund) und `envoy_cost_month` (float, Posten „Gesandte“ des letzten Rechentags) |
| `get_location_count()` | `int` | |
| `get_location_index(id: String)` | `int` | -1, wenn unbekannt |
| `get_location(index: int)` | `Dictionary` | `id`, `name`, `owner_index` (int), `owner_name`, `terrain`, `is_city` (bool), `population` (int), `area_km2` (float), `density` (float, Einwohner je km²), `devastation_percent` (int), `majority_confession`, `confessions` (Dictionary Name → Prozent als float), `production_month` (float), `taxes_month` (float), `source`; seit M3 zusätzlich `has_data` (bool), `kreis_index` (int, −1 ohne Kreis), `kreis_name` (String); seit M4 `majority_culture` (String), `cultures` (Dictionary Name → Prozent als float), `children`, `young`, `working`, `old` (int: 0–14, 15–29, 30–64, 65+ Jahre), `living_standard` (float 0–100), `literacy_percent` (float), `balance_previous` (Dictionary `births`, `deaths`, `plague_deaths`, `moved_in`, `moved_out`, `emigrated` als int, Bilanz des Vorjahres; im ersten Jahr 0); seit M5 `market_index` (int, −1 ohne Markt), `market_name`, `is_port` (bool), `development_percent` (float, Zuschlag des Landesausbaus auf die Tragfähigkeit), `production_per_capita` (float, Reichstaler je Einwohner und Jahr), `professions` (Array von `{ name, stratum (0 unten, 1 mittel, 2 oben), people, income_year (je Kopf), coverage_percent (Mittel über die Bedürfnisse), food_percent (Grundnahrung) }`), `businesses` (Array von `{ name, profession, people }`), `goods` (Array von `{ name, color, supply, demand (je Monat), price, price_factor (zum Basispreis) }`, nur Güter mit Angebot oder Nachfrage im Ort); für Orte ohne Daten sind die drei Arrays leer. Bevölkerung, Anteile und Mittelwerte umfassen auch die Zugänge, die noch keinen eigenen Block haben (bevoelkerung.md, Abschnitt 4.7). `production_month` und `taxes_month` sind die Werte vom letzten Rechentag des Ortes. Für Orte ohne Daten: `has_data = false`, `owner_index = -1`, `owner_name = ""`, Simulationszahlen (`population`, `density`, `devastation_percent`, `production_month`, `taxes_month`) 0, `majority_confession = ""`, `confessions` leer; Kartenfelder (`terrain`, `is_city`, `area_km2`, Kreis) wie in den Daten; seit M7 `verwaltung` (Array der Verwaltungseinheiten von oben nach unten, je Dictionary mit `index`, `id`, `name`, `kind`; leer ohne Zuordnung) und `bundesgebiet` (bool), `eisenbahnen` (PackedStringArray: Strecken mit einem zum aktuellen Datum eröffneten Abschnitt an diesem Ort, je „Name (Jahr der Eröffnung)“; trägt der Name schon eine Klammer, steht das Jahr darin: „Ludwigsbahn (Nürnberg–Fürth, 1835)“); seit M8 `wahl` (Wahlkreis des Ortes im Parlament nach `get_shown_parliament`: `parliament_index`, `parliament_name`, `district_index`, `district_name`, `seats`, `election_date_text`, `held`; nach der Wahl `boycott`, `runoff`, `electorate`, `turnout_percent`, `winner`, `winner_name`, `winner_short_name`, `shares` (Kurzname der Strömung → Prozent), `colors` (Kurzname → Farbe); leer außerhalb des Wahlgebiets) und `campaign_district` (Wahlkreis des Ortes für die nächste ausstehende Wahl, −1 ohne; Parteienmodus); seit M9 `customs_zone_name` (Zollgebiet: Name der Union oder Kurzname des Besitzers, leer ohne Besitzer) und `customs_tariff_percent` (float, Tarif des Zollgebiets) |
| `get_location_owner_indices()` | `PackedInt32Array` | seit M3: Besitzer-Staatsindex je Ort (−1 ohne Daten), Länge = `get_location_count()` – für Grenzen im Shader ohne 4.500 Einzelabfragen |
| `get_location_kreis_indices()` | `PackedInt32Array` | seit M3: Kreisindex je Ort (−1 ohne Kreis) |
| `get_location_market_indices()` | `PackedInt32Array` | seit M5: Marktindex je Ort (−1 ohne Markt), Länge = `get_location_count()`; Grundlage der Marktgrenzen |
| `get_location_admin_indices()` | `PackedInt32Array` | seit M7: unterste Verwaltungseinheit je Ort (−1 ohne Einheit); Grenzen im Modus „Verwaltung“ |
| `get_admin_unit_count()` | `int` | seit M7 |
| `get_admin_unit(index: int)` | `Dictionary` | seit M7: `id`, `name`, `kind` (z. B. `provinz`), `parent_index` (int, −1 bei der obersten), `source`, `location_count` (Orte, deren unterste Einheit sie ist) |
| `get_railways()` | `Array` | seit M7: je Strecke `{ id, name, source, sections }`, `sections` als Array von `{ from, to (Ortsindizes), opened ({ year, month, day }), open (bool, eröffnet zum aktuellen Datum) }` |
| `get_rail_access()` | `PackedByteArray` | seit M7: Bahnanschluss je Ort zum aktuellen Datum, dieselben Stufen wie die Farben von `MAP_EISENBAHNEN`: 2 Bahnhof, 1 Ort in einem Markt mit Bahnfracht zu einem Nachbarmarkt, 0 ohne Bahn; leer ohne Simulation |
| `get_market_count()` | `int` | seit M5 |
| `get_market(index: int)` | `Dictionary` | seit M5: `id`, `name`, `location_count` (Orte mit Daten), `active` (bool, mindestens ein Ort mit Daten), `is_port` (bool), `harvest` (float, Erntefaktor des Jahres), `goods` (Array je Gut: `id`, `name`, `color`, `base_price`, `price`, `price_factor`, `supply`, `demand`, `imports`, `exports`, `buy_percent`) |
| `get_kreis_count()` | `int` | seit M3 |
| `get_kreis_index(id: String)` | `int` | seit M3; −1, wenn unbekannt |
| `get_kreis(index: int)` | `Dictionary` | seit M3: `id`, `name`, `location_count` (int), `source` |
| `get_map_colors(mode: int)` | `PackedColorArray` | Eine Farbe je Simulations-Ortsindex (Länge = `get_location_count()`), siehe Kartenmodi |
| `get_currents()` | `Array` | seit M8: politische Strömungen `{ id, name, short_name, color, position }` (Position −100 … +100; `short_name` für knappe Anzeigen, sonst wie `name`) |
| `get_parliaments()` | `Array` | seit M8: je Parlament des Szenarios `{ index, id, name, state_index (−1 = Deutscher Bund), seats, election_date ({ year, month, day }), election_date_text, faction_date, faction_date_text, seat_index (Tagungsort), franchise (Text), indirect, district_count, election_index (letzte Wahl oder −1), factions: [{ id, name, color, from, to }] }` |
| `get_election_count()` | `int` | seit M8: Zahl der abgehaltenen Wahlen |
| `get_election(index: int)` | `Dictionary` | seit M8: `index`, `parliament_index`, `parliament_name`, `date`, `date_text`, `seats` (Sollzahl), `seated`, `electorate`, `voters`, `boycott_seats`; `districts` je Wahlkreis `{ district_index, id, name, state_index, state_name, seats, location_index (erster Ort), electorate, voters, turnout, shares (PackedFloat64Array je Strömung, Summe 1), winner (−1 ohne), winner_share, runoff, boycott, report_rank (Meldereihenfolge ab 0) }`; Abgeordnete als gepackte Felder `deputy_district`, `deputy_current`, `deputy_position` (PackedFloat32Array), `deputy_faction` (−1 fraktionslos); `seats_by_current`, `seats_by_faction` (zuletzt die Fraktionslosen) |
| `get_location_electoral_district_indices(parliament: int)` | `PackedInt32Array` | seit M8: Wahlkreis (Index über alle Parlamente) je Ort, −1 ohne; leer bei unbekanntem Parlament; Grenzen im Modus „Wahlen“ |
| `get_shown_parliament()` | `int` | seit M8: Parlament, das Kartenmodus und Ortsinfo zeigen: das der letzten Wahl, vorher das mit dem nächsten Wahltag; −1 ohne Parlament |
| `get_state_politics(state_index: int)` | `Dictionary` | seit M8: `constitution` (Text), `franchise` (Text, Wahlrecht des Landtags), `elected` (bool, Volkswahl: Zensus, Dreiklassen oder allgemein), `runoff` (bool, absolute Mehrheit mit Stichwahl), `indirect` (bool), `census_threshold` (Steuern je Kopf und Jahr, 0 ohne Zensus), `interest_groups` (je Gruppe `{ id, name, color, share_percent, influence_percent }`, politik.md 3.3, aus den Orten des Staates gerechnet); leer ohne Parlament im Szenario oder bei ungültigem Index. `get_state` meldet mit `politik` (bool), ob es Angaben gibt |
| `get_customs_unions()` | `Array` | seit M9: je Zollunion `{ index, id, name, color, leader_index, leader_name, tariff_percent, member_indices (PackedInt32Array), member_count, population, revenue_month (Vereinskasse des letzten Monats), dissolved (die Leitmacht gehört ihr nicht mehr an) }`; leer ohne Zollunion im Szenario |
| `get_customs_change(state_index: int, customs_union: int)` | `Dictionary` | seit M9: Vorab-Prüfung eines Wechsels in die Union `customs_union` (−1 = Austritt), ohne Wirkung (diplomatie.md 3.5): `decision` (`angenommen`, `abgelehnt` – die Leitmacht lehnt ab –, `zurueckgezogen` – das Ziel ist die eigene Union und ein Antrag ist offen –, `ungueltig`), `effective_text` (nächster 1. Januar), `current_name`, `target_name` (leer für keine Union), `dissolves` (die Leitmacht verlässt ihre Union, die sich dann auflöst), `leader_name` (Leitmacht des Ziels); leer bei ungültigem Index |
| `request_customs_union(state_index: int, customs_union: int)` | `bool` | seit M9: reiht einen `CustomsUnion`-Befehl ein (wirksam am nächsten Tag, der Wechsel zum nächsten 1. Januar); `false`, wenn die Vorab-Prüfung `ungueltig` ergibt. Ein abgelehnter Antrag wird angenommen und erscheint nur in der Chronik |
| `campaign(current: int, district: int, action: int)` | `bool` | seit M8 (Parteienmodus): reiht einen `Campaign`-Befehl ein (Strömung, Wahlkreis, Art `CAMPAIGN_*`), Wirkung am nächsten Tag; `false` bei ungültigem Index oder unbekannter Art, wenn die Wahl des Wahlkreises nicht mehr aussteht oder die Mittel nach den schon eingereihten Befehlen nicht reichen (politik.md 9.1) |
| `get_party(current: int)` | `Dictionary` | seit M8: `index`, `id`, `name`, `color`, `funds`, `donations` (letzte Monatsspende), `reserved` (Kosten eingereihter Wahlkampfbefehle, ab morgen); `open` (eine Wahl steht aus) mit `parliament_name`, `election_date_text` und `campaign_districts` (Wahlkreise dieser Wahl mit eigenem Wahlkampf) für die nächste ausstehende Wahl; `held` (es gab eine Wahl) mit `last_parliament_name`, `last_election_date_text`, `faction_date_text`, `seats`, `seated`, `deputies` (je Abgeordnetem `{ district_name, state_name, faction_name, position }`), `factions_formed`, `faction_name`, `faction_own`, `faction_seats` (Fraktion mit den meisten eigenen Abgeordneten) für die letzte Wahl; leer bei ungültigem Index |
| `get_campaign(district: int, current: int)` | `Dictionary` | seit M8: `district_name`, `seats`, `parliament_name`, `election_date_text`, `open` (Wahl steht aus), `mobilization`, `persuasion`, `club`, `actions` (je Art `{ action, name, cost, allowed }`; `allowed` rechnet die schon eingereihten Befehle der Strömung als verplant); solange `open`: `boycott`, `poll_error` (± Punkte), `poll` (PackedFloat64Array, Stimmungsbericht in Prozent je Strömung, Summe 100; Schätzung ohne Zufall plus Anzeigefehler je Wahlkreis, Strömung und Monat) |
| `set_player_current(current: int)` | `bool` | seit M8: Parteienmodus mit dieser Strömung, −1 = Staat; `false` bei ungültigem Index und in Szenarien ohne Parlament. Steht im Spielstand (`save_game`), `load_scenario` setzt ihn zurück |
| `get_player_current()` | `int` | seit M8: gespielte Strömung, −1 = Staat |
| `set_tax_rate_percent(state_index: int, percent: int)` | `bool` | Reiht einen `SetTaxRate`-Befehl für die Kontribution ein; `false`, wenn Index ungültig oder Prozent außerhalb 0..Maximum |
| `set_tax_percent(state_index: int, kind: int, percent: int)` | `bool` | seit M5: wie oben für eine Steuerart (`TAX_KONTRIBUTION` 0, `TAX_AKZISE` 1, `TAX_ZOLL` 2); `false` auch bei unbekannter Art und seit M9 für den Zoll eines Mitglieds einer Zollunion (`customs_locked`) |
| `get_max_tax_rate_percent()` | `int` | Höchstsatz, gilt für alle drei Steuerarten |
| `get_chronicle_size()` | `int` | |
| `get_chronicle(from_index: int)` | `Array` | Einträge ab `from_index` als Dictionaries `{ "date_text": String, "text": String, "location_index": int, "state_index": int }` |
| `get_checksum()` | `String` | Prüfsumme als 16-stellige Hexadezimalzahl |

### Diplomatie (seit M9, Schritt b3)

Regeln: [diplomatie.md](diplomatie.md), Abschnitte 4.1 bis 4.6. Alle Abfragen sind leer (bzw.
`active == false`), solange die Welt keine Beziehungen kennt (1648, ohne Deutschen Bund). Die
Befehle prüfen vorab wie der Kern und reihen nur Befehle ein, die nicht `ungueltig` sind (Antrag
und Forderung nur bei `angenommen`); wirksam werden sie am nächsten Tag. **Entscheidungen** sind Texte: `angenommen`, `abgelehnt` (der andere
Staat lehnt ab; der Befehl wird trotzdem angenommen und erscheint nur in der Chronik) oder
`ungueltig`. **Lager** (`camp`): 1 Preußen, −1 Österreich, 0 unentschieden – ein Bundesstaat neigt
zu der Großmacht, zu der seine Beziehung um mehr als 20 besser ist.

| Methode | Rückgabe | Beschreibung |
|---|---|---|
| `get_relations(state_index: int)` | `Array` | je anderem Staat `{ index, tag, name, short_name, color, relation, target (Zielwert), rival, allied, guarantees (wir garantieren ihn), guaranteed (er garantiert uns), envoy (unser Gesandter dort), envoy_from (sein Gesandter bei uns), federal (Bundesstaat), plenum_votes, camp, camp_text, in_play (in einer laufenden Krise), customs_union_name }` |
| `get_diplomacy(state_index: int)` | `Dictionary` | `active` (bool), `diet` (bool, Bundesversammlung), `envoy_capacity`, `envoy_count`, `envoy_cost_month`, `envoy_cost_percent` (je Gesandtem, in Prozent der Monatseinnahmen), `alliance_min`, `guarantee_min`, `rival_below` (Schwellen der Beziehung), `play_index` (Krise des Staates, −1 ohne), `mobilized` (bool, Heereskosten × 1,5), `camp`, `federal`; leer bei ungültigem Index |
| `get_diplomacy_actions(state_index: int, target: int)` | `Dictionary` | Vorab-Prüfung je Handlung (Entscheidung als Text): `envoy_send`, `envoy_recall`, `alliance`, `guarantee`, `cancel_alliance`, `cancel_guarantee`, `demand_leadership`, `demand_customs_union`, `demand_break_alliance`, `motion_intervention`; dazu `customs_union_name` (Union für `demand_customs_union`), `own_side`, `other_side` (PackedStringArray, Kurznamen der Seiten einer Krise) und `own_strength`, `other_strength` (Soldaten); leer ohne Diplomatie oder bei `state_index == target` |
| `send_envoy(state_index: int, target: int, send: bool)` | `bool` | Gesandten entsenden (`true`) oder abberufen (`Envoy`-Befehl) |
| `propose_treaty(state_index: int, target: int, action: int)` | `bool` | `TREATY_ALLIANCE` (0), `TREATY_GUARANTEE` (1), `TREATY_CANCEL_ALLIANCE` (2), `TREATY_CANCEL_GUARANTEE` (3) (`Treaty`-Befehl) |
| `get_diet()` | `Dictionary` | `name`, `presidency_index`, `presidency_name`, `seat_name`, `plenum` (je Staat mit Stimmen `{ index, short_name, color, votes, camp }`), `plenum_total`, `council` (je Stimme des Engeren Rats `{ name, state_indices, states }`), `motion` (offener Antrag `{ kind, text, applicant_index, applicant_name, filed_text, vote_text, majority_text }`, sonst leer), `results` (Abstimmungen, älteste zuerst, je `{ kind, text, applicant_index, applicant_name, opponent_name, date_text, plenum_yes, plenum_total, council_yes, council_total, council_body (der Engere Rat entscheidet), majority_text, accepted }`), `reform_attempts`, `reform_accepted`; leer ohne Bundesversammlung |
| `get_motion_preview(applicant: int, kind: int, target: int)` | `Dictionary` | Vorab-Prüfung eines Antrags `MOTION_REFORM` (0), `MOTION_CUSTOMS` (1, `target` = Zollunion), `MOTION_INTERVENTION` (2, `target` = Staat) oder `MOTION_PRESIDENCY` (3) (`target` −1 ohne Ziel): `decision`, `text`, `majority_text`; bei `angenommen` die Auszählung von heute wie in `results` (`plenum_yes` … `accepted`, `opponent_name`) |
| `file_motion(applicant: int, kind: int, target: int)` | `bool` | Antrag stellen (`Motion`-Befehl); nur bei `angenommen` |
| `get_diplomatic_plays()` | `Array` | laufende Krisen `{ index, kind, kind_text, demander_index, demander_name, target_index, target_name, stage (1–3), stage_text (Protest, Ultimatum, Mobilmachung), days_left, started_text, demander_strength, target_strength, demander_side, target_side (PackedStringArray, Unterstützer ohne die Hauptpartei), demander_escalates, target_escalates, demander_mediation, target_mediation (Vermittlung dieser Seite: `angenommen` oder `abgelehnt`) }` |
| `make_demand(demander: int, target: int, kind: int)` | `bool` | Forderung `DEMAND_LEADERSHIP` (0), `DEMAND_CUSTOMS_UNION` (1), `DEMAND_BREAK_ALLIANCE` (2) (`Demand`-Befehl); nur bei `angenommen` |
| `manoeuvre(state_index: int, kind: int)` | `bool` | Manöver der Hauptpartei: `MANOEUVRE_YIELD` (0), `MANOEUVRE_MEDIATE` (1), `MANOEUVRE_ESCALATE` (2) (`Manoeuvre`-Befehl) |
| `get_german_question()` | `Dictionary` | Anzeige „Die deutsche Frage“ (Abschnitt 4.6): `federal_population`, `customs_union_name` und `customs_union_percent` (Zollverein, Anteil der Bundesbevölkerung), `prussia_percent`, `austria_percent`, `undecided_percent` und `prussia_states`, `austria_states`, `undecided_states` (Lager nach Bevölkerung und Zahl), `reform_attempts`, `reform_accepted`, `leadership_prussia`, `leadership_austria`, `leadership_compromises` (Krisen um die Führung im Bund), `play_results` (je `{ text, demander_name, target_name, date_text, outcome_text }`), `treaties` (je `{ alliance, from_index, to_index, from_name, to_name, since_text }`) |

### Spielstände (seit M6)

Format, Prüfungen und Godot-Seite: [speicherstaende.md](speicherstaende.md). Pfade sind
Dateipfade des Betriebssystems (in GDScript `ProjectSettings.globalize_path("user://…")`).

| Methode | Rückgabe | Beschreibung |
|---|---|---|
| `save_game(path: String, player_state_index: int)` | `String` | Schreibt den Spielstand (Spielerstaat für die Metadaten; ungültiger Index = bisheriger Spielerstaat; seit M8 auch die Strömung aus `set_player_current`), legt den Ordner an. Leer bei Erfolg, sonst die Meldung |
| `load_game(data_path: String, path: String)` | `String` | Lädt die Spieldaten des Szenarios aus dem Spielstand frisch und den Zustand hinein. Ersetzt die laufende Simulation **nur bei Erfolg**; danach gelten Spielerstaat und Ladehinweise aus diesem Ladevorgang. Leer bei Erfolg, sonst die Meldung (z. B. „Spielstand passt nicht zu den Spieldaten (abweichend: orte)“) |
| `get_save_info(path: String)` | `Dictionary` | Ohne zu laden: `ok` (bool, Datei lesbar), `compatible` (bool, passt zum Inhalt der geladenen Spieldaten; ein Spielstand eines anderen Szenarios passt nie, `load_game` prüft ihn gegen sein eigenes), `reason` (String, leer wenn passend), `scenario`, `date` (`{ year, month, day }`), `date_text`, `days` (int), `player_tag`, `player_name` (Kurzname, sonst Kennung; nachgeschlagen nur im laufenden Szenario), `player_current` (seit M8: Kennung der Strömung im Parteienmodus, leer = Staat), `format_version` (int), `game_version`, `checksum` (16-stellig hex). Bei `ok == false` nur `ok`, `compatible` und `reason` |
| `get_save_format_version()` | `int` | Formatversion, die diese Brücke schreibt (2 seit M8, 3 seit M9) |

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
| 11 | `MAP_EISENBAHNEN` | seit M7: Bahnhof (Endpunkt eines zum aktuellen Datum eröffneten Abschnitts) dunkelrot, Ort in einem Markt, der mit einem Nachbarmarkt zur Bahnfracht handelt, hellbraun (wie im Kern, wirtschaft.md 4.1), sonst blass; Orte ohne Daten Alpha 0. Die Strecken zeichnet Godot aus `get_railways`: eröffnete durchgezogen, spätere gestrichelt, wenn sie in den nächsten 5 Jahren eröffnen |
| 12 | `MAP_WAHLEN` | seit M8: Parlament nach `get_shown_parliament`. Nach der Wahl die Farbe der siegreichen Strömung, vom Papierton (Anteil des Siegers 25 %) bis zur vollen Farbe (70 %); Boykott kühles Grau; vor der Wahl die Wahlkreise aus der Palette von `MAP_KREISE`, aufgehellt; außerhalb des Wahlgebiets Papierton; Orte ohne Daten Alpha 0. Die Grenzen der Wahlkreise zeichnet Godot aus `get_location_electoral_district_indices`; während des Wahlabends setzt Godot noch nicht gemeldete Wahlkreise auf Alpha 0 (Schraffur) |
| 13 | `MAP_ZOLLGEBIETE` | seit M9: Zollunionen in ihrer Farbe (`farbe` in `common/zollunionen.chron`), Staaten ohne Union in der Staatsfarbe, zu 62 % mit Weiß gemischt; Orte ohne Daten Alpha 0. Ohne Zollunion im Szenario (1648) ist jeder Staat blass |
| 14 | `MAP_BEZIEHUNGEN` | seit M9, Schritt b3: Beziehung des Besitzers zum Spielerstaat (`get_player_state_index`), vom Papierton (0) zu Grün (+100) bzw. Rot (−100); der Spielerstaat in seiner Farbe; Orte ohne Daten Alpha 0. Ohne Beziehungen (1648) alles Papierton |

**Orte ohne Daten** (seit M3) erhalten in den Modi 0, 1, 2, 4, 6, 7, 8, 10, 11, 12, 13 und 14 eine Farbe mit **Alpha 0**;
die Godot-Seite zeichnet dort eine Schraffur „keine Daten“. In den Modi 3 und 5 haben sie normale
Farben (Alpha 1), im Modus 9 dann, wenn ihr Markt Orte mit Daten hat.

**Steuerarten** (seit M5) sind als Konstanten `TAX_KONTRIBUTION` (0), `TAX_AKZISE` (1) und
`TAX_ZOLL` (2) gebunden, die Kartenmodi als `MAP_*`, die Arten des Wahlkampfs (seit M8) als
`CAMPAIGN_VERSAMMLUNG` (0), `CAMPAIGN_FLUGBLAETTER` (1) und `CAMPAIGN_VEREIN` (2), die Arten der
Diplomatie (seit M9) als `TREATY_*`, `MOTION_*`, `DEMAND_*` und `MANOEUVRE_*` (Abschnitt Diplomatie).

**Threads (seit M4):** Die Brücke legt die Simulation mit `SimConfig::threads = 0` an, rechnet
also auf allen Kernen. Das Ergebnis ist bei jeder Thread-Zahl gleich (bevoelkerung.md,
Abschnitt 5).

## Bibliothek und Einbindung

- CMake-Ziel `uhrwerk_bridge` (Shared Library), Einstiegspunkt `uhrwerk_library_init`.
- Ausgabe nach `game/bin/` mit den Namen `libuhrwerk_bridge.so` (Linux), `uhrwerk_bridge.dll` (Windows), `libuhrwerk_bridge.dylib` (macOS).
- Beschreibung in `game/bin/uhrwerk.gdextension`, `compatibility_minimum = "4.4"`.
