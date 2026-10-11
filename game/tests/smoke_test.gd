extends SceneTree
## Rauchtest der Brücke und der Kartendaten (Godot-Seite, Fassung M3).
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
	"balance_previous",
	"living_standard",
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
	"has_data",
	"kreis_index",
	"kreis_name",
	"majority_culture",
	"cultures",
	"children",
	"young",
	"working",
	"old",
	"living_standard",
	"literacy_percent",
	"balance_previous",
]
const KREIS_FIELDS: PackedStringArray = ["id", "name", "location_count", "source"]
## Kartenmodi, in denen Orte ohne Daten Alpha 0 haben (bridge-api.md).
const NO_DATA_ALPHA_MODES: PackedInt32Array = [0, 1, 2, 4, 6, 7]
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
	_check_data_and_kreise(sim)

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
			and sim.get_location_owner_indices().is_empty()
			and sim.get_location_kreis_indices().is_empty()
			and sim.get_kreis_count() == 0
			and sim.get_kreis(0).is_empty()
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
		and sim.get_kreis(-1).is_empty()
		and sim.get_kreis(sim.get_kreis_count()).is_empty()
		and sim.get_kreis_index("gibt_es_nicht") == -1
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


## Orte ohne Daten und Kreise (seit M3): Indexarrays, get_kreis, Alpha der Kartenfarben.
func _check_data_and_kreise(sim: SimulationClient) -> void:
	var count := sim.get_location_count()
	var owners := sim.get_location_owner_indices()
	var kreise := sim.get_location_kreis_indices()
	if not _check(
		owners.size() == count and kreise.size() == count,
		"Indexarrays für Besitzer und Kreise mit %d Einträgen" % count,
		"get_location_owner_indices/get_location_kreis_indices: %d/%d statt %d Einträge"
		% [owners.size(), kreise.size(), count]
	):
		return
	var kreis_count := sim.get_kreis_count()
	var kreis_names := PackedStringArray()
	var kreis_members := PackedInt32Array()
	kreis_members.resize(kreis_count)
	for kreis in kreis_count:
		var entry := sim.get_kreis(kreis)
		for key in KREIS_FIELDS:
			if not entry.has(key):
				_check(false, "", "get_kreis(%d) ohne Feld „%s“" % [kreis, key])
				return
		kreis_names.append(str(entry["name"]))
		if sim.get_kreis_index(str(entry["id"])) != kreis:
			_check(false, "", "get_kreis_index(„%s“) liefert nicht %d" % [entry["id"], kreis])
	var with_data := 0
	var without_kreis := 0
	var mismatches := PackedStringArray()
	for index in count:
		var location := sim.get_location(index)
		var has_data := bool(location["has_data"])
		var owner := int(location["owner_index"])
		var kreis := int(location["kreis_index"])
		if has_data:
			with_data += 1
		if owners[index] != owner or (has_data != (owner >= 0)):
			mismatches.append("%s: Besitzer %d/%d, Daten %s" % [
				location["id"], owners[index], owner, has_data
			])
		if kreise[index] != kreis or kreis >= kreis_count:
			mismatches.append("%s: Kreis %d/%d" % [location["id"], kreise[index], kreis])
		elif kreis < 0:
			without_kreis += 1
			if not str(location["kreis_name"]).is_empty():
				mismatches.append("%s: kreis_name ohne Kreis" % location["id"])
		else:
			kreis_members[kreis] += 1
			if str(location["kreis_name"]) != kreis_names[kreis]:
				mismatches.append("%s: kreis_name „%s“" % [location["id"], location["kreis_name"]])
		if not has_data and (
			int(location["population"]) != 0 or not str(location["owner_name"]).is_empty()
		):
			mismatches.append("%s: ohne Daten, aber mit Bevölkerung oder Besitzer" % location["id"])
	_check(
		mismatches.is_empty(),
		(
			"%d Orte mit Daten, %d ohne Daten; Indexarrays passen zu get_location"
			% [with_data, count - with_data]
		),
		"Abweichungen bei Orten ohne Daten oder Kreisen: " + "; ".join(mismatches.slice(0, 10))
	)
	_check(with_data > 0, "", "kein Ort hat Daten")
	var counts_ok := true
	for kreis in kreis_count:
		if int(sim.get_kreis(kreis)["location_count"]) != kreis_members[kreis]:
			counts_ok = false
	_check(
		counts_ok,
		"%d Kreise, %d Orte ohne Kreis; location_count stimmt" % [kreis_count, without_kreis],
		"location_count der Kreise passt nicht zu den Orten"
	)
	for mode in MapModes.COUNT:
		var colors := sim.get_map_colors(mode)
		var wrong := 0
		for index in mini(colors.size(), count):
			var expect_clear := NO_DATA_ALPHA_MODES.has(mode) and owners[index] < 0
			if expect_clear != (colors[index].a == 0.0) or (
				not expect_clear and colors[index].a != 1.0
			):
				wrong += 1
		_check(
			wrong == 0,
			"",
			"Kartenmodus %d: %d Orte mit falschem Alpha (0 nur für Orte ohne Daten in 0, 1, 2, 4, 6, 7)"
			% [mode, wrong]
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
	_check(
		map.river_texture != null,
		"fluesse.png vorhanden und so groß wie provinzen.png",
		"fluesse.png fehlt oder passt nicht (siehe Hinweise oben)"
	)
	var rivers := Image.load_from_file(UhrwerkPaths.map_dir().path_join("fluesse.png"))
	if rivers != null and not rivers.is_empty():
		_check(
			rivers.get_format() == Image.FORMAT_L8,
			"fluesse.png ist ein Graustufenbild",
			"fluesse.png hat das Format %d statt L8 (Graustufen)" % rivers.get_format()
		)
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


## Pixel von provinzen.png (chronik-skript.md 6.3): Blaukanal = Flächenart 0..3, bei
## B ≠ 0 kein Ort; Kartenindizes bekannt; Pixelzahl und Beschriftungspunkt je Ort wie in
## der definition.csv.
func _check_map_pixels(map: MapData) -> void:
	var bytes := map.image.get_data()
	var count := map.map_count()
	var pixels := PackedInt32Array()
	pixels.resize(count)
	var areas := PackedInt32Array()
	areas.resize(4)
	var invalid := 0
	var bad_area := 0
	var water_with_location := 0
	var land_without := 0
	for offset in range(0, bytes.size(), 3):
		var area := bytes[offset + 2]
		var value := bytes[offset] + bytes[offset + 1] * 256
		if area > 3:
			bad_area += 1
			continue
		areas[area] += 1
		if area != 0:
			if value != 0:
				water_with_location += 1
		elif value == 0:
			land_without += 1
		elif value > count:
			invalid += 1
		else:
			pixels[value - 1] += 1
	_check(
		bad_area == 0,
		(
			"Flächenarten: %s Kernraum-Land, %s Meer, %s Binnengewässer, %s Ausland"
			% [
				GermanFormat.integer(areas[0]), GermanFormat.integer(areas[1]),
				GermanFormat.integer(areas[2]), GermanFormat.integer(areas[3])
			]
		),
		"%d Pixel mit Blaukanal > 3 (Flächenart 0..3)" % bad_area
	)
	_check(
		water_with_location == 0,
		"Meer, Binnengewässer und Ausland ohne Ort",
		"%d Pixel mit B ≠ 0 und Kartenindex (Spezifikation: wert = 0)" % water_with_location
	)
	_check(
		invalid == 0,
		"alle Pixel tragen bekannte Kartenindizes",
		"%d Pixel mit Kartenindex außerhalb der definition.csv" % invalid
	)
	var without := PackedStringArray()
	var wrong_count := PackedStringArray()
	var wrong_point := PackedStringArray()
	for map_index in count:
		if map.map_ids[map_index].is_empty():
			continue
		if pixels[map_index] == 0:
			without.append(map.map_ids[map_index])
		if pixels[map_index] != map.pixel_counts[map_index]:
			wrong_count.append("%s (%d statt %d)" % [
				map.map_ids[map_index], pixels[map_index], map.pixel_counts[map_index]
			])
		var point := map.label_points[map_index]
		if point.x >= 0.0 and map.map_index_at_pixel(Vector2i(point.floor())) != map_index:
			wrong_point.append(map.map_ids[map_index])
	_check(
		without.is_empty(),
		(
			"jeder Ort hat Kartenfläche (%s Pixel Kernraum-Land ohne Ort)"
			% GermanFormat.integer(land_without)
		),
		"Orte ohne Pixel in provinzen.png: " + ", ".join(without)
	)
	_check(
		wrong_count.is_empty(),
		"Pixelzahl je Ort wie in definition.csv",
		"Pixelzahl weicht ab: " + ", ".join(wrong_count.slice(0, 10))
	)
	_check(
		wrong_point.is_empty(),
		"jeder Beschriftungspunkt liegt im eigenen Ort",
		"Beschriftungspunkt nicht im Ort: " + ", ".join(wrong_point.slice(0, 10))
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
