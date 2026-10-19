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
## { date_text, text, location_index, state_index }.
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
## district_count, election_index, factions: [{ id, name, color, from, to }] }.
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
