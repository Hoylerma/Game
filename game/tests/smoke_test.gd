extends SceneTree
## Rauchtest der Brücke und der Kartendaten (Godot-Seite).
##
## Aufruf aus dem Repository-Hauptordner (nach einem Import des Projekts):
##   godot --headless --path game --script res://tests/smoke_test.gd
## Optionen nach "--": --ohne-karte (Kartenprüfung überspringen).
## Datenordner: $UHRWERK_DATEN, sonst data/ im Repository.
## Ergebnis: "SMOKE OK" und Exitcode 0, sonst Fehlermeldungen und Exitcode 1.

const DAYS_TO_SIMULATE := 365
const STATE_FIELDS: PackedStringArray = [
	"tag",
	"name",
	"short_name",
	"color",
	"ruler",
	"government",
	"capital_index",
	"treasury",
	"tax_rate_percent",
	"income_month",
	"expenses_month",
	"population",
	"location_count",
]
const LOCATION_FIELDS: PackedStringArray = [
	"id",
	"name",
	"owner_index",
	"owner_name",
	"terrain",
	"is_city",
	"population",
	"area_km2",
	"density",
	"devastation_percent",
	"majority_confession",
	"confessions",
	"production_month",
	"taxes_month",
	"source",
]
const CHRONICLE_FIELDS: PackedStringArray = ["date_text", "text", "location_index", "state_index"]

var _failures := PackedStringArray()
var _check_map := true


func _initialize() -> void:
	_check_map = not OS.get_cmdline_user_args().has("--ohne-karte")
	print("EINIGKEIT – Rauchtest der Godot-Seite")
	print("Datenordner: %s (%s)" % [UhrwerkPaths.data_dir(), UhrwerkPaths.data_dir_origin()])
	_run()
	if _failures.is_empty():
		print("SMOKE OK")
		quit(0)
	else:
		printerr("SMOKE FEHLGESCHLAGEN: %d Fehler" % _failures.size())
		for failure in _failures:
			printerr("  - ", failure)
		quit(1)


func _run() -> void:
	var library := SimulationClient.expected_library_file()
	if not _check(
		ClassDB.class_exists(SimulationClient.NATIVE_CLASS),
		"Klasse UhrwerkSimulation registriert",
		"Klasse UhrwerkSimulation fehlt – ist game/%s gebaut?" % library
	):
		return
	var missing := SimulationClient.missing_methods()
	if not _check(
		missing.is_empty(),
		"alle Methoden der Schnittstelle vorhanden",
		"fehlende Methoden: " + ", ".join(missing)
	):
		return

	var sim := SimulationClient.create()
	if not _check(sim != null, "Simulation erzeugt", "UhrwerkSimulation ließ sich nicht erzeugen"):
		return
	_check_before_load(sim)

	var broken := SimulationClient.create()
	var broken_errors := broken.load_scenario("/gibt/es/nicht/einigkeit-daten")
	_check(
		not broken_errors.is_empty() and not broken.is_loaded(),
		"falscher Datenordner wird mit Fehlermeldung abgelehnt",
		"falscher Datenordner lieferte keinen Fehler"
	)

	var errors := sim.load_scenario(UhrwerkPaths.data_dir())
	if not _check(
		errors.is_empty() and sim.is_loaded(), "Daten geladen", "Laden fehlgeschlagen:\n" + errors
	):
		return
	_check_counts(sim)
	var player := sim.get_player_state_index()
	var start := sim.get_date()
	var max_tax := sim.get_max_tax_rate_percent()
	_check(max_tax > 0, "Höchststeuersatz %d %%" % max_tax, "Höchststeuersatz ist %d" % max_tax)
	_check_invalid_indices(sim, max_tax, player)
	_check_map_colors(sim)

	# Steuerbefehl: wirksam am nächsten Tag.
	var old_tax := int(sim.get_state(player).get("tax_rate_percent", -1))
	var new_tax := old_tax + 2 if old_tax + 2 <= max_tax else maxi(old_tax - 2, 0)
	_check(
		sim.set_tax_rate_percent(player, new_tax),
		"Steuerbefehl angenommen",
		"set_tax_rate_percent(%d, %d) lieferte false" % [player, new_tax]
	)
	sim.advance_days(1)
	var tax_after := int(sim.get_state(player).get("tax_rate_percent", -1))
	_check(
		tax_after == new_tax,
		"Steuersatz nach einem Tag %d %% (vorher %d %%)" % [tax_after, old_tax],
		"Steuersatz nach einem Tag %d statt %d" % [tax_after, new_tax]
	)

	sim.advance_days(DAYS_TO_SIMULATE - 1)
	_check_date(sim, start)
	_check_after_simulation(sim, player)
	_check_chronicle(sim)
	_check_determinism(sim, player, new_tax)
	if _check_map:
		_check_map_files(sim)
	else:
		print("  –   Kartenprüfung übersprungen (--ohne-karte)")


func _check_before_load(sim: SimulationClient) -> void:
	_check(
		(
			not sim.is_loaded()
			and sim.get_state_count() == 0
			and sim.get_location_count() == 0
			and sim.get_state(0).is_empty()
			and sim.get_location(0).is_empty()
			and sim.get_map_colors(MapModes.MAP_POLITICAL).is_empty()
			and sim.get_chronicle_size() == 0
		),
		"vor dem Laden leere Abfragen",
		"vor dem Laden liefern Abfragen schon Werte"
	)


func _check_counts(sim: SimulationClient) -> void:
	var states := sim.get_state_count()
	var locations := sim.get_location_count()
	var player := sim.get_player_state_index()
	_check(states > 0, "%d Staaten" % states, "keine Staaten geladen")
	_check(locations > 0, "%d Orte" % locations, "keine Orte geladen")
	_check(
		player >= 0 and player < states,
		"Spielerstaat %d" % player,
		"Spielerstaat-Index %d ungültig" % player
	)
	var scenario := sim.get_scenario_name()
	_check(not scenario.is_empty(), "Szenario „%s“" % scenario, "Szenarioname leer")

	var state := sim.get_state(player)
	for key in STATE_FIELDS:
		_check(state.has(key), "", "get_state ohne Feld „%s“" % key)
	var state_population := int(state.get("population", 0))
	var state_locations := int(state.get("location_count", 0))
	_check(
		state_population > 0 and state_locations > 0,
		(
			"Spielerstaat %s: %s Einwohner in %d Orten"
			% [str(state.get("tag", "")), GermanFormat.integer(state_population), state_locations]
		),
		"Spielerstaat ohne Bevölkerung oder Orte"
	)

	var total_population := 0
	var total_area := 0.0
	for index in locations:
		var location := sim.get_location(index)
		for key in LOCATION_FIELDS:
			if not location.has(key):
				_check(false, "", "get_location(%d) ohne Feld „%s“" % [index, key])
				return
		total_population += int(location["population"])
		total_area += float(location["area_km2"])
		var id := str(location["id"])
		if sim.get_location_index(id) != index:
			_check(false, "", "get_location_index(„%s“) liefert nicht %d" % [id, index])
	_check(
		total_population > 0 and total_area > 0.0,
		(
			"Orte: %s Einwohner auf %s km²"
			% [GermanFormat.integer(total_population), GermanFormat.number(total_area)]
		),
		"Orte ohne Bevölkerung oder Fläche"
	)
	var date := sim.get_date()
	_check(
		int(date.get("year", 0)) > 0 and not sim.get_date_text().is_empty(),
		"Startdatum %s" % sim.get_date_text(),
		"Startdatum fehlt"
	)


func _check_invalid_indices(sim: SimulationClient, max_tax: int, player: int) -> void:
	var rejected := (
		sim.get_state(-1).is_empty()
		and sim.get_state(sim.get_state_count()).is_empty()
		and sim.get_location(-1).is_empty()
		and sim.get_location(sim.get_location_count()).is_empty()
		and sim.get_location_index("gibt_es_nicht") == -1
		and sim.get_state_index("???") == -1
		and not sim.set_tax_rate_percent(-1, 10)
		and not sim.set_tax_rate_percent(sim.get_state_count(), 10)
		and not sim.set_tax_rate_percent(player, max_tax + 1)
		and not sim.set_tax_rate_percent(player, -1)
	)
	_check(
		rejected,
		"ungültige Indizes und Befehle werden abgewiesen",
		"ungültige Indizes oder Befehle werden nicht abgewiesen"
	)
	# Darf nicht abstürzen; das Ergebnis wird nur angezeigt.
	var odd_colors := sim.get_map_colors(99).size()
	print("  ok  unbekannter Kartenmodus liefert %d Farben" % odd_colors)


func _check_map_colors(sim: SimulationClient) -> void:
	var expected := sim.get_location_count()
	for mode in MapModes.COUNT:
		var count := sim.get_map_colors(mode).size()
		_check(
			count == expected,
			"Kartenmodus %d (%s): %d Farben" % [mode, MapModes.NAMES[mode], count],
			"Kartenmodus %d liefert %d statt %d Farben" % [mode, count, expected]
		)


func _check_date(sim: SimulationClient, start: Dictionary) -> void:
	var expected := _add_days(start, DAYS_TO_SIMULATE)
	var actual := sim.get_date()
	var same: bool = (
		int(actual.get("year", 0)) == expected["year"]
		and int(actual.get("month", 0)) == expected["month"]
		and int(actual.get("day", 0)) == expected["day"]
	)
	var expected_text := "%d.%d.%d" % [expected["day"], expected["month"], expected["year"]]
	_check(
		same,
		"Datum nach %d Tagen: %s" % [DAYS_TO_SIMULATE, sim.get_date_text()],
		"Datum nach %d Tagen %s, erwartet %s" % [DAYS_TO_SIMULATE, str(actual), expected_text]
	)
	var elapsed := sim.get_days_elapsed()
	_check(
		elapsed == DAYS_TO_SIMULATE,
		"%d Tage vergangen" % elapsed,
		"get_days_elapsed() = %d statt %d" % [elapsed, DAYS_TO_SIMULATE]
	)
	var date_text := sim.get_date_text()
	_check(
		date_text.contains(str(expected["year"])),
		"Datumstext „%s“" % date_text,
		"Datumstext „%s“ enthält das Jahr nicht" % date_text
	)


func _check_after_simulation(sim: SimulationClient, player: int) -> void:
	var state := sim.get_state(player)
	var income := float(state.get("income_month", 0.0))
	var expenses := float(state.get("expenses_month", 0.0))
	_check(
		income > 0.0 and expenses > 0.0,
		(
			"Einnahmen %s, Ausgaben %s je Monat"
			% [GermanFormat.money(income), GermanFormat.money(expenses)]
		),
		"nach einem Jahr keine Einnahmen oder Ausgaben"
	)
	var production := 0.0
	for index in sim.get_location_count():
		production += float(sim.get_location(index).get("production_month", 0.0))
	_check(
		production > 0.0,
		"Produktion aller Orte %s je Monat" % GermanFormat.money(production),
		"keine Produktion"
	)


func _check_chronicle(sim: SimulationClient) -> void:
	var count := sim.get_chronicle_size()
	var entries := sim.get_chronicle(0)
	_check(
		entries.size() == count,
		"Chronik: %d Einträge" % count,
		"get_chronicle(0) liefert %d statt %d Einträge" % [entries.size(), count]
	)
	for entry in entries:
		for key in CHRONICLE_FIELDS:
			if not entry.has(key):
				_check(false, "", "Chronikeintrag ohne Feld „%s“: %s" % [key, str(entry)])
				return
	if count > 0:
		var newest := entries[count - 1]
		print("      neuester Eintrag: %s – %s" % [newest["date_text"], newest["text"]])
		_check(
			sim.get_chronicle(count - 1).size() == 1,
			"",
			"get_chronicle(size − 1) liefert nicht genau einen Eintrag"
		)
	_check(
		sim.get_chronicle(count).is_empty() and sim.get_chronicle(count + 10).is_empty(),
		"Chronik ab dem Ende leer",
		"get_chronicle hinter dem Ende liefert Einträge"
	)


func _check_determinism(sim: SimulationClient, player: int, new_tax: int) -> void:
	var twin := SimulationClient.create()
	twin.load_scenario(UhrwerkPaths.data_dir())
	twin.set_tax_rate_percent(player, new_tax)
	twin.advance_days(DAYS_TO_SIMULATE)
	var checksum := sim.get_checksum()
	_check(
		checksum.length() == 16 and checksum.is_valid_hex_number(),
		"Prüfsumme %s" % checksum,
		"Prüfsumme „%s“ ist keine 16-stellige Hexzahl" % checksum
	)
	_check(
		twin.get_checksum() == checksum,
		"zweiter Lauf mit gleicher Prüfsumme",
		"zweiter Lauf: %s statt %s" % [twin.get_checksum(), checksum]
	)


func _check_map_files(sim: SimulationClient) -> void:
	var map := MapData.load_from_dir(UhrwerkPaths.map_dir())
	if not _check(
		map.is_valid(),
		"Karte geladen (%dx%d, %d Kartenindizes)" % [map.size.x, map.size.y, map.map_count()],
		map.error
	):
		return
	for warning in map.warnings:
		_check(false, "", warning)
	var unresolved := map.bind_simulation(sim)
	_check(
		unresolved.is_empty(),
		"jeder Eintrag der definition.csv über get_location_index auflösbar",
		"Kennungen der definition.csv ohne Ort: " + ", ".join(unresolved)
	)
	var absent := PackedStringArray()
	for sim_index in map.sim_to_map.size():
		if map.sim_to_map[sim_index] < 0:
			absent.append(str(sim.get_location(sim_index).get("id", sim_index)))
	_check(
		absent.is_empty(),
		"jeder Ort der Simulation steht in der definition.csv",
		"Orte ohne Eintrag in der definition.csv: " + ", ".join(absent)
	)
	for map_index in map.map_count():
		if map.map_ids[map_index].is_empty():
			_check(false, "", "definition.csv: Kartenindex %d fehlt (Lücke)" % map_index)
	if map.config != null:
		var configured := Vector2i(map.config.image_width, map.config.image_height)
		_check(
			map.size == configured,
			"Bildgröße passt zu karte.chron",
			(
				"provinzen.png ist %dx%d, karte.chron verlangt %dx%d"
				% [map.size.x, map.size.y, configured.x, configured.y]
			)
		)
	_check_map_pixels(map)


## Jeder Pixel muss einen gültigen Kartenindex tragen (B = 0), jeder Ort mindestens einen Pixel.
func _check_map_pixels(map: MapData) -> void:
	var bytes := map.image.get_data()
	var count := map.map_count()
	var pixels := PackedInt32Array()
	pixels.resize(count)
	var invalid := 0
	var blue := 0
	var empty := 0
	for offset in range(0, bytes.size(), 3):
		if bytes[offset + 2] != 0:
			blue += 1
		var value := bytes[offset] + bytes[offset + 1] * 256
		if value == 0:
			empty += 1
		elif value > count:
			invalid += 1
		else:
			pixels[value - 1] += 1
	_check(
		invalid == 0,
		"alle Pixel tragen bekannte Kartenindizes",
		"%d Pixel mit Kartenindex außerhalb der definition.csv" % invalid
	)
	_check(
		blue == 0,
		"Blaukanal überall 0",
		"%d Pixel mit Blauanteil (Spezifikation: B = 0)" % blue
	)
	var without := PackedStringArray()
	for map_index in count:
		if pixels[map_index] == 0 and not map.map_ids[map_index].is_empty():
			without.append(map.map_ids[map_index])
	_check(
		without.is_empty(),
		"jeder Ort hat Kartenfläche (%s Pixel ohne Ort)" % GermanFormat.integer(empty),
		"Orte ohne Pixel in provinzen.png: " + ", ".join(without)
	)


func _add_days(date: Dictionary, days: int) -> Dictionary:
	var year := int(date.get("year", 0))
	var month := int(date.get("month", 1))
	var day := int(date.get("day", 1))
	for _i in days:
		day += 1
		if day > _days_in_month(year, month):
			day = 1
			month += 1
			if month > 12:
				month = 1
				year += 1
	return {"year": year, "month": month, "day": day}


func _days_in_month(year: int, month: int) -> int:
	if month == 2:
		var leap := (year % 4 == 0 and year % 100 != 0) or year % 400 == 0
		return 29 if leap else 28
	return 30 if month in [4, 6, 9, 11] else 31


## Meldet eine Prüfung; liefert condition zurück.
func _check(condition: bool, success: String, failure: String) -> bool:
	if condition:
		if not success.is_empty():
			print("  ok  ", success)
	else:
		_failures.append(failure)
		printerr("  FEHLER  ", failure)
	return condition
