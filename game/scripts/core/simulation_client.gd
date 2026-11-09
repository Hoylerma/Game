class_name SimulationClient
extends RefCounted
## Typisierte Hülle um die GDExtension-Klasse UhrwerkSimulation.
##
## Die Klasse wird nur über ClassDB angesprochen, damit das Projekt auch ohne
## gebaute Erweiterung fehlerfrei startet (ein direkter Verweis auf
## UhrwerkSimulation wäre dann ein Parse-Fehler). Schnittstelle:
## docs/technik/bridge-api.md. Die Godot-Seite liest nur und schickt Befehle.

## Name der Klasse in der Erweiterung.
const NATIVE_CLASS := &"UhrwerkSimulation"
## Erwarteter Dateiname der Bibliothek je Betriebssystem (bridge-api.md).
const LIBRARY_NAMES := {
	"Linux": "libuhrwerk_bridge.so",
	"Windows": "uhrwerk_bridge.dll",
	"macOS": "libuhrwerk_bridge.dylib",
}
## Größte Anzahl Tage je Aufruf von advance_days (Vertrag der Brücke).
const MAX_DAYS_PER_CALL := 3660

## Steuerarten für set_tax_percent (seit M5, wie TAX_* der Brücke).
const TAX_KONTRIBUTION := 0
const TAX_AKZISE := 1
const TAX_ZOLL := 2
const TAX_KIND_COUNT := 3

## Arten des Wahlkampfs für campaign (seit M8, wie CAMPAIGN_* der Brücke).
const CAMPAIGN_VERSAMMLUNG := 0
const CAMPAIGN_FLUGBLAETTER := 1
const CAMPAIGN_VEREIN := 2

## Diplomatie (seit M9, Schritt b3, wie TREATY_*, MOTION_*, DEMAND_* und MANOEUVRE_* der Brücke).
const TREATY_ALLIANCE := 0
const TREATY_GUARANTEE := 1
const TREATY_CANCEL_ALLIANCE := 2
const TREATY_CANCEL_GUARANTEE := 3
const MOTION_REFORM := 0
const MOTION_CUSTOMS := 1
const MOTION_INTERVENTION := 2
const MOTION_PRESIDENCY := 3
const DEMAND_LEADERSHIP := 0
const DEMAND_CUSTOMS_UNION := 1
const DEMAND_BREAK_ALLIANCE := 2
const MANOEUVRE_YIELD := 0
const MANOEUVRE_MEDIATE := 1
const MANOEUVRE_ESCALATE := 2

## Friedensangebote (seit M10, Schritt b3, wie PEACE_* der Brücke).
const PEACE_WHITE := 0
const PEACE_OWN_GOALS := 1
const PEACE_CAPITULATION := 2

## Arten der KI-Entscheidungen (seit M12, wie AiDecisionKind des Kerns; Feld kind in get_ai_decisions).
const AI_DEMAND := 7
const AI_DECLARE_WAR := 11

## Alle Methoden, die die Oberfläche benutzt.
const REQUIRED_METHODS: Array[StringName] = [
	&"load_scenario",
	&"is_loaded",
	&"get_load_warnings",
	&"advance_days",
	&"get_date",
	&"get_date_text",
	&"get_days_elapsed",
	&"get_scenario_name",
	&"get_player_state_index",
	&"get_state_count",
	&"get_state_index",
	&"get_state",
	&"get_location_count",
	&"get_location_index",
	&"get_location",
	&"get_location_owner_indices",
	&"get_location_kreis_indices",
	&"get_location_market_indices",
	&"get_location_admin_indices",
	&"get_admin_unit_count",
	&"get_admin_unit",
	&"get_railways",
	&"get_rail_access",
	&"get_scenarios",
	&"get_scenario",
	&"set_player_state",
	&"get_game_settings",
	&"set_game_settings",
	&"get_state_society",
	&"get_state_economy",
	&"get_state_laws",
	&"get_votes",
	&"get_office_history",
	&"get_legacy",
	&"get_yearbook",
	&"get_verdict",
	&"is_game_over",
	&"get_kreis_count",
	&"get_kreis_index",
	&"get_kreis",
	&"get_market_count",
	&"get_market",
	&"get_map_colors",
	&"get_currents",
	&"get_parliaments",
	&"get_election_count",
	&"get_election",
	&"get_location_electoral_district_indices",
	&"get_shown_parliament",
	&"get_state_politics",
	&"get_customs_unions",
	&"get_customs_change",
	&"request_customs_union",
	&"get_relations",
	&"get_diplomacy",
	&"get_diplomacy_actions",
	&"send_envoy",
	&"propose_treaty",
	&"get_diet",
	&"get_motion_preview",
	&"file_motion",
	&"get_diplomatic_plays",
	&"make_demand",
	&"manoeuvre",
	&"get_german_question",
	&"get_armies",
	&"get_army",
	&"get_route_preview",
	&"order_march",
	&"set_army_staff",
	&"order_mobilization",
	&"get_location_control",
	&"get_battles",
	&"get_sieges",
	&"get_wars",
	&"get_declaration_goals",
	&"get_declaration_sides",
	&"get_war_goal_options",
	&"declare_war",
	&"add_war_goal",
	&"get_peace_preview",
	&"offer_peace",
	&"get_open_events",
	&"choose_event_option",
	&"set_event_player",
	&"get_event_player",
	&"get_event_log",
	&"get_journals",
	&"get_state_events",
	&"get_revolts",
	&"get_state_persons",
	&"get_intentions",
	&"get_ai_decisions",
	&"campaign",
	&"get_party",
	&"get_campaign",
	&"set_player_current",
	&"get_player_current",
	&"set_tax_rate_percent",
	&"set_tax_percent",
	&"get_max_tax_rate_percent",
	&"get_chronicle_size",
	&"get_chronicle",
	&"get_checksum",
	&"save_game",
	&"load_game",
	&"get_save_info",
	&"get_save_format_version",
]

var _native: Object = null


## true, wenn die Erweiterung geladen ist und die Klasse bereitstellt.
static func is_extension_available() -> bool:
	return ClassDB.class_exists(NATIVE_CLASS)


## Erwarteter Dateiname der Bibliothek für dieses Betriebssystem.
static func expected_library_file() -> String:
	var os_name := OS.get_name()
	if LIBRARY_NAMES.has(os_name):
		return "bin/" + str(LIBRARY_NAMES[os_name])
	return "bin/libuhrwerk_bridge.so"


## Methoden der Schnittstelle, die die geladene Erweiterung nicht kennt.
static func missing_methods() -> PackedStringArray:
	var missing := PackedStringArray()
	if not is_extension_available():
		return missing
	for method in REQUIRED_METHODS:
		if not ClassDB.class_has_method(NATIVE_CLASS, method):
			missing.append(str(method))
	return missing


## Erzeugt die Simulation oder liefert null, wenn die Erweiterung fehlt oder
## nicht zur Schnittstelle passt.
static func create() -> SimulationClient:
	if not is_extension_available() or not missing_methods().is_empty():
		return null
	var native := ClassDB.instantiate(NATIVE_CLASS) as Object
	if native == null:
		return null
	var client := SimulationClient.new()
	client._native = native
	return client


## Lädt Daten und Szenario. Leerer String bei Erfolg, sonst die Fehler zeilenweise.
func load_scenario(
	data_path: String,
	scenario_id: String = UhrwerkPaths.DEFAULT_SCENARIO,
	seed_value: int = UhrwerkPaths.DEFAULT_SEED
) -> String:
	return str(_native.call(&"load_scenario", data_path, scenario_id, seed_value))


func is_loaded() -> bool:
	return bool(_native.call(&"is_loaded"))


func get_load_warnings() -> PackedStringArray:
	return PackedStringArray(_native.call(&"get_load_warnings"))


## Simuliert days Tage; größere Werte werden in zulässige Schritte geteilt.
func advance_days(days: int) -> void:
	var remaining := maxi(days, 0)
	while remaining > 0:
		var step := mini(remaining, MAX_DAYS_PER_CALL)
		_native.call(&"advance_days", step)
		remaining -= step


func get_date() -> Dictionary:
	return _as_dictionary(_native.call(&"get_date"))


func get_date_text() -> String:
	return str(_native.call(&"get_date_text"))


func get_days_elapsed() -> int:
	return int(_native.call(&"get_days_elapsed"))


func get_scenario_name() -> String:
	return str(_native.call(&"get_scenario_name"))


func get_player_state_index() -> int:
	return int(_native.call(&"get_player_state_index"))


## Szenarien eines Datenordners ohne sie zu laden (seit M7): je { id, name, date_text, year,
## player_tag, playable (PackedStringArray), currency }.
func get_scenarios(data_path: String) -> Array[Dictionary]:
	var result: Array[Dictionary] = []
	var raw: Variant = _native.call(&"get_scenarios", data_path)
	if raw is Array:
		for entry: Variant in raw:
			if entry is Dictionary:
				result.append(entry)
	return result


## Geladenes Szenario wie ein Eintrag von get_scenarios (seit M7).
func get_scenario() -> Dictionary:
	return _as_dictionary(_native.call(&"get_scenario"))


## Spielerstaat wählen (seit M7); false bei ungültigem Index.
func set_player_state(state_index: int) -> bool:
	return bool(_native.call(&"set_player_state", state_index))


func get_state_count() -> int:
	return int(_native.call(&"get_state_count"))


func get_state_index(tag: String) -> int:
	return int(_native.call(&"get_state_index", tag))


## Staat als Dictionary (Felder siehe bridge-api.md); leer bei ungültigem Index.
func get_state(index: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_state", index))


func get_location_count() -> int:
	return int(_native.call(&"get_location_count"))


func get_location_index(id: String) -> int:
	return int(_native.call(&"get_location_index", id))


## Ort als Dictionary (Felder siehe bridge-api.md); leer bei ungültigem Index.
func get_location(index: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_location", index))


## Besitzer (Staatsindex) je Ort, −1 für Orte ohne Daten; Länge = get_location_count().
func get_location_owner_indices() -> PackedInt32Array:
	var result: Variant = _native.call(&"get_location_owner_indices")
	if result is PackedInt32Array:
		return result
	return PackedInt32Array()


## Kreisindex je Ort, −1 ohne Kreis; Länge = get_location_count().
func get_location_kreis_indices() -> PackedInt32Array:
	var result: Variant = _native.call(&"get_location_kreis_indices")
	if result is PackedInt32Array:
		return result
	return PackedInt32Array()


## Marktindex je Ort, −1 ohne Markt; Länge = get_location_count() (seit M5).
func get_location_market_indices() -> PackedInt32Array:
	var result: Variant = _native.call(&"get_location_market_indices")
	if result is PackedInt32Array:
		return result
	return PackedInt32Array()


## Index der untersten Verwaltungseinheit je Ort, −1 ohne Einheit (seit M7).
func get_location_admin_indices() -> PackedInt32Array:
	var result: Variant = _native.call(&"get_location_admin_indices")
	if result is PackedInt32Array:
		return result
	return PackedInt32Array()


func get_admin_unit_count() -> int:
	return int(_native.call(&"get_admin_unit_count"))


## Verwaltungseinheit { id, name, kind, parent_index, source, location_count } (seit M7).
func get_admin_unit(index: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_admin_unit", index))


## Eisenbahnstrecken [{ id, name, source, sections: [{ from, to, opened, open }] }] (seit M7);
## from/to sind Ortsindizes, open gilt zum aktuellen Datum.
func get_railways() -> Array:
	var result: Variant = _native.call(&"get_railways")
	return result if result is Array else []


## Bahnanschluss je Ort zum aktuellen Datum (seit M7): 2 Bahnhof, 1 Markt mit Bahnfracht zu einem
## Nachbarmarkt, 0 ohne Bahn; dieselben Stufen wie die Farben im Modus „Eisenbahnen“.
func get_rail_access() -> PackedByteArray:
	var result: Variant = _native.call(&"get_rail_access")
	return result if result is PackedByteArray else PackedByteArray()


func get_kreis_count() -> int:
	return int(_native.call(&"get_kreis_count"))


func get_kreis_index(id: String) -> int:
	return int(_native.call(&"get_kreis_index", id))


## Kreis als Dictionary { id, name, location_count, source }; leer bei ungültigem Index.
func get_kreis(index: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_kreis", index))


func get_market_count() -> int:
	return int(_native.call(&"get_market_count"))


## Markt als Dictionary { id, name, location_count, active, is_port, harvest, goods } (seit M5);
## leer bei ungültigem Index.
func get_market(index: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_market", index))


## Eine Farbe je Simulations-Ortsindex für den Kartenmodus mode (MapModes).
func get_map_colors(mode: int) -> PackedColorArray:
	var result: Variant = _native.call(&"get_map_colors", mode)
	if result is PackedColorArray:
		return result
	return PackedColorArray()


## Reiht einen Steuerbefehl ein; wirksam am nächsten simulierten Tag.
func set_tax_rate_percent(state_index: int, percent: int) -> bool:
	return bool(_native.call(&"set_tax_rate_percent", state_index, percent))


## Reiht einen Steuerbefehl für eine Steuerart ein (TAX_KONTRIBUTION, TAX_AKZISE, TAX_ZOLL; seit M5).
func set_tax_percent(state_index: int, kind: int, percent: int) -> bool:
	return bool(_native.call(&"set_tax_percent", state_index, kind, percent))


func get_max_tax_rate_percent() -> int:
	return int(_native.call(&"get_max_tax_rate_percent"))


func get_chronicle_size() -> int:
	return int(_native.call(&"get_chronicle_size"))


## Chronikeinträge ab from_index als Dictionaries
## { date_text, text, location_index, state_index, turning_point (seit M11: von einem Wendepunkt) }.
func get_chronicle(from_index: int) -> Array[Dictionary]:
	var entries: Array[Dictionary] = []
	var result: Variant = _native.call(&"get_chronicle", from_index)
	if result is Array:
		for entry: Variant in result:
			if entry is Dictionary:
				entries.append(entry)
	return entries


## Politische Strömungen (seit M8): { id, name, color, position }.
func get_currents() -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_currents"))


## Parlamente des Szenarios (seit M8): { index, id, name, state_index, seats, election_date,
## election_date_text, faction_date, faction_date_text, seat_index, franchise, indirect,
## district_count, election_index, factions: [{ id, name, color, from, to }] }. Seit M13a3: election_date
## ist der nächste ausstehende Wahltag (sonst der letzte), pending ob eine Wahl aussteht, federal für das
## Parlament eines Bundes, last_election_date (mit einer Wahl) der Tag der letzten Wahl.
func get_parliaments() -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_parliaments"))


## Zahl der abgehaltenen Wahlen (seit M8).
func get_election_count() -> int:
	return int(_native.call(&"get_election_count"))


## Ergebnis einer Wahl (seit M8, bridge-api.md): Kopf, districts (je Wahlkreis), Abgeordnete als
## gepackte Felder deputy_*, seats_by_current und seats_by_faction (zuletzt fraktionslos).
func get_election(index: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_election", index))


## Wahlkreis (Index) je Ort für ein Parlament, -1 = keiner (seit M8).
func get_location_electoral_district_indices(parliament: int) -> PackedInt32Array:
	return PackedInt32Array(_native.call(&"get_location_electoral_district_indices", parliament))


## Parlament, das Kartenmodus und Ortsinfo zeigen: das der letzten Wahl, vorher das mit dem
## nächsten Wahltag; -1 ohne Parlament (seit M8).
func get_shown_parliament() -> int:
	return int(_native.call(&"get_shown_parliament"))


## Verfassung, Wahlrecht und Interessengruppen eines Staates (seit M8, politik.md 3.3 und 4):
## { constitution, franchise, elected, runoff, indirect, census_threshold, interest_groups: [{ id, name,
## color, share_percent, influence_percent }] }; leer ohne Parlament im Szenario.
func get_state_politics(state_index: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_state_politics", state_index))


## Zollunionen (seit M9, diplomatie.md 3.7): [{ index, id, name, color, leader_index, leader_name,
## tariff_percent, member_indices, member_count, population, revenue_month, dissolved }].
func get_customs_unions() -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_customs_unions"))


## Vorab-Prüfung eines Wechsels in die Union customs_union (-1 = Austritt), ohne Wirkung:
## { decision ("angenommen", "abgelehnt", "zurueckgezogen", "ungueltig"), effective_text,
## current_name, target_name, dissolves, leader_name }; leer bei ungültigen Indizes.
func get_customs_change(state_index: int, customs_union: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_customs_change", state_index, customs_union))


## Befehl Zollunion für den nächsten Tag (wirksam zum nächsten 1. Januar); false, wenn der Wechsel
## ungültig ist (get_customs_change: "ungueltig").
func request_customs_union(state_index: int, customs_union: int) -> bool:
	return bool(_native.call(&"request_customs_union", state_index, customs_union))


## Diplomatie (seit M9, Schritt b3, bridge-api.md): Beziehungen des Staates zu allen anderen,
## je { index, tag, name, short_name, color, relation, target, rival, allied, guarantees, guaranteed,
## envoy, envoy_from, federal, plenum_votes, camp, camp_text, in_play, customs_union_name }; leer ohne
## Diplomatie.
func get_relations(state_index: int) -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_relations", state_index))


## Überblick eines Staates: { active, diet, envoy_capacity, envoy_count, envoy_cost_month,
## envoy_cost_percent, alliance_min, guarantee_min, rival_below, play_index, mobilized, army_cost_factor
## (seit M10), camp, federal }.
func get_diplomacy(state_index: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_diplomacy", state_index))


## Vorab-Prüfung aller Handlungen gegenüber target ("angenommen", "abgelehnt", "ungueltig") und die
## Seiten eines möglichen Spiels mit Stärke.
func get_diplomacy_actions(state_index: int, target: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_diplomacy_actions", state_index, target))


## Gesandten entsenden (send) oder abberufen; Befehl für den nächsten Tag, false wenn ungültig.
func send_envoy(state_index: int, target: int, send: bool) -> bool:
	return bool(_native.call(&"send_envoy", state_index, target, send))


## Vertragshandlung TREATY_*; ein abgelehntes Angebot geht trotzdem hinaus (Chronik).
func propose_treaty(state_index: int, target: int, action: int) -> bool:
	return bool(_native.call(&"propose_treaty", state_index, target, action))


## Bundesversammlung: { name, presidency_index, presidency_name, seat_name, plenum, plenum_total,
## council, motion, results, reform_attempts, reform_accepted }; leer ohne Bundesversammlung.
func get_diet() -> Dictionary:
	return _as_dictionary(_native.call(&"get_diet"))


## Voraussichtliche Abstimmung eines Antrags MOTION_* (target: Union bzw. Staat, sonst -1).
func get_motion_preview(applicant: int, kind: int, target: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_motion_preview", applicant, kind, target))


## Antrag stellen; abgestimmt wird am nächsten Monatsersten. false, wenn ungültig.
func file_motion(applicant: int, kind: int, target: int) -> bool:
	return bool(_native.call(&"file_motion", applicant, kind, target))


## Laufende diplomatische Spiele mit Seiten, Stufe, Tagen bis zum Ende der Stufe und Stärke.
func get_diplomatic_plays() -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_diplomatic_plays"))


## Forderung DEMAND_* stellen: das Spiel beginnt am nächsten Tag. false, wenn ungültig.
func make_demand(demander: int, target: int, kind: int) -> bool:
	return bool(_native.call(&"make_demand", demander, target, kind))


## Manöver MANOEUVRE_* einer Hauptpartei. false, wenn ungültig.
func manoeuvre(state_index: int, kind: int) -> bool:
	return bool(_native.call(&"manoeuvre", state_index, kind))


## Die deutsche Frage (diplomatie.md 4.6): Zollverein, Lager, Bundesreform, Führung im Bund,
## Verträge; leer ohne Diplomatie.
func get_german_question() -> Dictionary:
	return _as_dictionary(_native.call(&"get_german_question"))


## Militär (seit M10, Schritt b3, bridge-api.md): Heere eines Staates (-1: alle), je { id, state_index,
## name, commander, leadership, location_index, soldiers, guns, morale, status, status_text, garrison,
## staff, supplied, route, destination_index, on_rail, days_left }.
func get_armies(state_index: int = -1) -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_armies", state_index))


## Ein Heer wie in get_armies; leer, wenn es nicht (mehr) besteht.
func get_army(army_id: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_army", army_id))


## Weg eines Heeres zum Ort ohne Befehl: { ok, days, path, rail_legs, arrival_text }.
func get_route_preview(army_id: int, location_index: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_route_preview", army_id, location_index))


## Marschbefehl für den nächsten Tag; das Heer verlässt den Generalstab. false ohne Weg.
func order_march(army_id: int, location_index: int) -> bool:
	return bool(_native.call(&"order_march", army_id, location_index))


## Heer dem Generalstab übergeben (on) oder zurücknehmen. false, wenn schon so.
func set_army_staff(army_id: int, on: bool) -> bool:
	return bool(_native.call(&"set_army_staff", army_id, on))


## Mobilmachung (mobilize) oder Abrüstung; false, wenn schon so oder im Krieg.
func order_mobilization(state_index: int, mobilize: bool) -> bool:
	return bool(_native.call(&"order_mobilization", state_index, mobilize))


## Wer jeden Ort hält (Staatsindex, -1 ohne Daten); weicht er vom Besitzer ab, ist der Ort besetzt.
func get_location_control() -> PackedInt32Array:
	var control: Variant = _native.call(&"get_location_control")
	if control is PackedInt32Array:
		return control
	return PackedInt32Array()


## Laufende Schlachten: je { location_index, days, attacker_losses, defender_losses }.
func get_battles() -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_battles"))


## Laufende Belagerungen: je { location_index, besieger_index, progress }.
func get_sieges() -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_sieges"))


## Laufende Kriege: je { id, name, attackers, defenders, started_text, goals, attacker_score,
## defender_score, attacker_weariness, defender_weariness, attacker_cost, defender_cost,
## attacker_losses, defender_losses, battles, player_side, player_leads }.
func get_wars() -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_wars"))


## Ereignisse (seit M11, Schritt b4, bridge-api.md): offene Ereignisse des Spielers, je { event_index,
## id, title, text, state_index, state_name, date_text, since_text, due_text, days_left, source, kind_text,
## turning_point, chosen_option (-1: noch nicht gewählt), default_option, options: je { index, id, text,
## historical, available, effects (PackedStringArray) } }.
func get_open_events() -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_open_events"))


## Option eines offenen Ereignisses wählen (wirksam am nächsten Tag); false, wenn ungültig.
func choose_event_option(event_index: int, option_index: int) -> bool:
	return bool(_native.call(&"choose_event_option", event_index, option_index))


## Staat, dessen Ereignisse auf die Wahl warten (-1: keiner); false ohne Ereignisse (1648).
func set_event_player(state_index: int) -> bool:
	return bool(_native.call(&"set_event_player", state_index))


func get_event_player() -> int:
	return int(_native.call(&"get_event_player"))


## Eingetretene Ereignisse, neueste zuerst: je { event_index, title, date_text, state_index, state_name,
## turning_point, kind_text, option_text, historical }.
func get_event_log(limit: int) -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_event_log", limit))


## Journale: je { index, id, title, text, end_text, started, stage_id, stage_title, stage_text, finished,
## rating, since_text, goals: je { text, done, counting, count, target }, history: je { title,
## date_text, outcome }, outcomes: je { id, title, rating, text, fallback } }.
func get_journals() -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_journals"))


## Ereignisse eines Staates: { laws: je { group, option }, approvals: je { id, name, color, value }, unrest,
## unrest_target, revolt: { kind_text, strength, since_text, helper_index, helper_name } oder leer,
## timed: je { text, until_text }, pending }; leer ohne Ereignisse.
func get_state_events(state_index: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_state_events", state_index))


## KI der Staaten (seit M12, Schritt b3, ki.md Abschnitt 9): Herrscher und leitender Minister im Amt, je
## { id, name, role ("herrscher"/"minister"), title, since_text, values: { staatskunst, diplomatie,
## kriegskunst, wirkung }, traits, inclinations (Beitrag der Eigenschaften je Neigung), weight, source };
## leer ohne KI-Daten.
func get_state_persons(state_index: int) -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_state_persons", state_index))


## Spieleinstellungen (seit M13, vertical_slice.md 3.2): { active, difficulties: je { id, name, text,
## default }, difficulty_id, difficulty_name, open, editable (nur am ersten Tag) }; ohne KI nur active.
func get_game_settings() -> Dictionary:
	return _as_dictionary(_native.call(&"get_game_settings"))


## Gesellschaft eines Staates (seit M13a5b): { population, locations, cities, children, young, working, old,
## living_standard, literacy_percent, confessions, cultures (Name → Prozent), balance_previous, professions:
## je { name, stratum, people, percent } }.
func get_state_society(state_index: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_state_society", state_index))


## Wirtschaft eines Staates (seit M13a5b): { goods: je { name, color, supply, demand, price, price_factor },
## businesses: je { name, profession, people }, markets: je { name, locations }, production_month,
## taxes_month, customs_zone_name, customs_tariff_percent }.
func get_state_economy(state_index: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_state_economy", state_index))


## Gesetze eines Staates (seit M13a5b): je Gruppe { id, name, current, options: je { id, name, unrest,
## current, wishes: je { group, color, value } } }.
func get_state_laws(state_index: int) -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_state_laws", state_index))


## Abstimmungen in Parlamenten (seit M13a5b), neueste zuerst: je { title, parliament_index,
## parliament_name, date_text, yes, no, accepted }.
func get_votes() -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_votes"))


## Frühere Amtsinhaber eines Staates (seit M13a5b), zuletzt Ausgeschiedene zuerst: je { name, role, title,
## from_text, to_text }; leer ohne KI-Daten.
func get_office_history(state_index: int) -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_office_history", state_index))


## Vermächtnis eines Staates (seit M13a4, vertical_slice.md 3.5): { active, dimensions: je { id, name,
## value (0–100), base, sentence, factors: je { name, value, active } } }; ohne Regeln active = false.
func get_legacy(state_index: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_legacy", state_index))


## Jahresreihen eines Staates (seit M13a4, vertical_slice.md 4.3): { columns (Namen: erst die Kennzahlen,
## dann die Dimensionen), measure_count, rows: je { date, year, date_text, values (PackedFloat64Array) } }.
func get_yearbook(state_index: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_yearbook", state_index))


## Ende der Partie (seit M13a4): { has_end, over, active, end_date ({ year, month, day }), end_date_text, start_date_text, turning_points: je
## { date_text, title, state_name, option_text }, wars: je { attacker_name, defender_name, started_text,
## ended_text, winner (1, −1, 0), winner_name, transfers } }.
func get_verdict() -> Dictionary:
	return _as_dictionary(_native.call(&"get_verdict"))


## Ob die Partie zu Ende ist (der letzte Tag des Lesezeichens ist gerechnet).
func is_game_over() -> bool:
	return bool(_native.call(&"is_game_over"))


## Stufe (Kennung, leer = Standard) und Geschichtsverlauf; nur am ersten Tag einer Partie mit KI.
func set_game_settings(difficulty_id: String, open: bool) -> bool:
	return bool(_native.call(&"set_game_settings", difficulty_id, open))


## Absichten eines Staates: { player, personality, inclinations: { risiko … vermittlung }, inclination_keys,
## inclination_names, rival_index, rival_name, goal_text, goals: je { kind, text, weight, main } (nur
## aktive Ziele) }; leer ohne KI-Daten.
func get_intentions(state_index: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_intentions", state_index))


## Letzte Entscheidungen der KI für den Staat, neueste zuerst (höchstens limit, 0 = alle aus dem
## Protokoll): je { date_text, kind, text, target_index, utility, reasons, reasons_text }.
func get_ai_decisions(state_index: int, limit: int) -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_ai_decisions", state_index, limit))


## Laufende Aufstände: je { state_index, state_name, capital_index, kind_text, strength, since_text,
## helper_index }.
func get_revolts() -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_revolts"))


## Mögliche Kriegsziele einer Erklärung an target: je { code, kind, text, cost }; leer, wenn keine
## Erklärung möglich ist.
func get_declaration_goals(state_index: int, target: int) -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_declaration_goals", state_index, target))


## Seiten einer Kriegserklärung an target (Bündnisfall): { attackers, defenders (Staatsindizes,
## Hauptpartei zuerst), attacker_soldiers, defender_soldiers }; leer, wenn keine möglich ist.
func get_declaration_sides(state_index: int, target: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_declaration_sides", state_index, target))


## Weitere Kriegsziele der Hauptpartei in einem laufenden Krieg (wie get_declaration_goals).
func get_war_goal_options(state_index: int, war_id: int) -> Array[Dictionary]:
	return _as_dictionaries(_native.call(&"get_war_goal_options", state_index, war_id))


## Kriegserklärung mit einem Kriegsziel (code aus get_declaration_goals, 0: keines).
func declare_war(state_index: int, target: int, goal: int = 0) -> bool:
	return bool(_native.call(&"declare_war", state_index, target, goal))


## Weiteres Kriegsziel stellen; false, wenn ungültig.
func add_war_goal(state_index: int, war_id: int, goal: int) -> bool:
	return bool(_native.call(&"add_war_goal", state_index, war_id, goal))


## Vorab-Prüfung eines Friedensangebots PEACE_*: { valid, accepted, text }.
func get_peace_preview(state_index: int, war_id: int, terms: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_peace_preview", state_index, war_id, terms))


## Friedensangebot für den nächsten Tag; false, wenn ungültig.
func offer_peace(state_index: int, war_id: int, terms: int) -> bool:
	return bool(_native.call(&"offer_peace", state_index, war_id, terms))


## Wahlkampf der Strömung current im Wahlkreis district (seit M8): Befehl für den nächsten Tag;
## false, wenn die Wahl nicht mehr aussteht oder die Mittel nicht reichen.
func campaign(current: int, district: int, action: int) -> bool:
	return bool(_native.call(&"campaign", current, district, action))


## Partei (seit M8): { index, id, name, color, funds, donations, campaign_districts, parliament_name,
## election_date_text, faction_date_text, held, seats, seated, deputies, factions_formed,
## faction_name, faction_own, faction_seats }.
func get_party(current: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_party", current))


## Wahlkampf in einem Wahlkreis (seit M8): { district_name, seats, open, mobilization, persuasion,
## club, actions: [{ action, name, cost, allowed }], poll (Prozent je Strömung), poll_error, boycott }.
func get_campaign(district: int, current: int) -> Dictionary:
	return _as_dictionary(_native.call(&"get_campaign", district, current))


## Parteienmodus (seit M8): Strömung, die man spielt; -1 = Staat. Steht im Spielstand.
func set_player_current(current: int) -> bool:
	return bool(_native.call(&"set_player_current", current))


func get_player_current() -> int:
	return int(_native.call(&"get_player_current"))


func get_checksum() -> String:
	return str(_native.call(&"get_checksum"))


## Spielstand in die Datei path (absoluter Pfad) schreiben; player_state_index ist der Spielerstaat.
## Leer bei Erfolg, sonst die Meldung (seit M6).
func save_game(path: String, player_state_index: int) -> String:
	return str(_native.call(&"save_game", path, player_state_index))


## Spielstand laden: Spieldaten aus data_path frisch laden, Zustand hinein. Ersetzt die laufende
## Simulation nur bei Erfolg; leer bei Erfolg, sonst die Meldung (seit M6).
func load_game(data_path: String, path: String) -> String:
	return str(_native.call(&"load_game", data_path, path))


## Angaben zu einem Spielstand ohne ihn zu laden: { ok, compatible, reason, scenario, date,
## date_text, days, player_tag, player_name, player_current, format_version, game_version, checksum }
## (seit M6; player_current seit M8: Strömung im Parteienmodus, leer = Staat).
func get_save_info(path: String) -> Dictionary:
	return _as_dictionary(_native.call(&"get_save_info", path))


func get_save_format_version() -> int:
	return int(_native.call(&"get_save_format_version"))


static func _as_dictionaries(value: Variant) -> Array[Dictionary]:
	var result: Array[Dictionary] = []
	if value is Array:
		for entry: Variant in value:
			if entry is Dictionary:
				result.append(entry)
	return result


static func _as_dictionary(value: Variant) -> Dictionary:
	if value is Dictionary:
		return value
	return {}
