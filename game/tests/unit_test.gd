extends SceneTree
## Einheitentests der Hilfsklassen, die ohne GDExtension auskommen.
##
## Aufruf aus dem Repository-Hauptordner (nach einem Import des Projekts):
##   godot --headless --path game --script res://tests/unit_test.gd
## Ergebnis: "UNIT OK" und Exitcode 0, sonst Fehlermeldungen und Exitcode 1.

const NBSP := " "
## Ordner für die kleine Testkarte (wird angelegt und wieder gelöscht).
const TEST_MAP_DIR := "user://unit_test_karte"

var _failures := PackedStringArray()
var _count := 0


func _initialize() -> void:
	print("EINIGKEIT – Einheitentests der Godot-Seite")
	_test_german_format()
	_test_launch_options()
	_test_map_config()
	_test_paths()
	_test_legend_gradient()
	_test_legend_entries()
	_test_map_modes()
	_test_map_detail()
	_test_map_data_and_layout()
	_test_location_catalog()
	if _failures.is_empty():
		print("UNIT OK (%d Prüfungen)" % _count)
		quit(0)
	else:
		printerr("UNIT FEHLGESCHLAGEN: %d von %d Prüfungen" % [_failures.size(), _count])
		for failure in _failures:
			printerr("  - ", failure)
		quit(1)


func _test_german_format() -> void:
	_equal(GermanFormat.integer(0), "0", "integer 0")
	_equal(GermanFormat.integer(999), "999", "integer 999")
	_equal(GermanFormat.integer(1000), "1.000", "integer 1000")
	_equal(GermanFormat.integer(1234567), "1.234.567", "integer 1234567")
	_equal(GermanFormat.integer(-75300), "−75.300", "integer negativ")
	_equal(GermanFormat.number(1234.56, 1), "1.234,6", "number mit Nachkommastelle")
	_equal(GermanFormat.number(0.04, 1), "0,0", "number gerundet")
	_equal(GermanFormat.number(-0.04, 1), "0,0", "number ohne negatives Null")
	_equal(GermanFormat.number(-2.5, 2), "−2,50", "number negativ")
	_equal(GermanFormat.number(999.96, 1), "1.000,0", "number Übertrag")
	_equal(GermanFormat.signed_number(8339.4), "+8.339", "signed positiv")
	_equal(GermanFormat.signed_number(-9667.0), "−9.667", "signed negativ")
	_equal(GermanFormat.signed_number(0.2), "0", "signed null ohne Vorzeichen")
	_equal(GermanFormat.money(23000.0), "23.000" + NBSP + "Taler", "money")
	_equal(GermanFormat.percent(12), "12" + NBSP + "%", "percent")
	_equal(GermanFormat.percent(12.5, 1), "12,5" + NBSP + "%", "percent mit Nachkommastelle")
	_equal(GermanFormat.area(150.0), "150" + NBSP + "km²", "area")
	_equal(GermanFormat.density(82.34), "82,3" + NBSP + "Einw./km²", "density")
	_equal(
		GermanFormat.government("staendisches_fuerstentum"),
		"Ständisches Fürstentum",
		"Regierungsform bekannt"
	)
	_equal(GermanFormat.government("neue_form"), "Neue Form", "Regierungsform unbekannt")
	_equal(GermanFormat.humanize("flussaue"), "Flussaue", "humanize Bezeichner")
	_equal(GermanFormat.humanize("Hügelland"), "Hügelland", "humanize Name bleibt")
	_equal(GermanFormat.humanize(""), "–", "humanize leer")


func _test_launch_options() -> void:
	var options := LaunchOptions.parse(PackedStringArray([
		"--screenshot=/tmp/bild.png", "--tage=365", "--kartenmodus=4", "--ort=berlin_coelln"
	]))
	_equal(options.screenshot_path, "/tmp/bild.png", "Option --screenshot")
	_equal(options.days, 365, "Option --tage")
	_equal(options.map_mode, 4, "Option --kartenmodus")
	_equal(options.location_id, "berlin_coelln", "Option --ort")
	_equal(options.problems.size(), 0, "keine Probleme")
	_true(options.wants_screenshot(), "Screenshot gewünscht")
	var defaults := LaunchOptions.parse(PackedStringArray())
	_equal(defaults.days, LaunchOptions.DEFAULT_SCREENSHOT_DAYS, "Standard --tage")
	_equal(defaults.map_mode, -1, "Standard --kartenmodus")
	_true(not defaults.wants_screenshot(), "ohne --screenshot kein Screenshot")
	var bad_args := PackedStringArray(
		["--tage=-3", "--kartenmodus=7", "--unbekannt", "--zoom=0", "--zoom=viel"]
	)
	var bad := LaunchOptions.parse(bad_args)
	_equal(bad.problems.size(), 5, "fehlerhafte Optionen gemeldet")
	_equal(bad.zoom_factor, 0.0, "fehlerhaftes --zoom ignoriert")
	var zoomed := LaunchOptions.parse(PackedStringArray(["--zoom=4.5", "--kartenmodus=5"]))
	_equal(zoomed.zoom_factor, 4.5, "Option --zoom")
	_equal(zoomed.map_mode, MapModes.MAP_KREISE, "Option --kartenmodus=5 (Kreise)")
	_equal(zoomed.problems.size(), 0, "--zoom und --kartenmodus=5 ohne Probleme")
	_equal(defaults.zoom_factor, 0.0, "Standard --zoom (Übersicht)")
	_equal(bad.days, LaunchOptions.DEFAULT_SCREENSHOT_DAYS, "fehlerhafte --tage ignoriert")
	var absolute := LaunchOptions.parse(PackedStringArray(["--screenshot=/tmp/a/../b.png"]))
	_equal(absolute.resolved_screenshot_path(), "/tmp/b.png", "absoluter Pfad normalisiert")
	var relative := LaunchOptions.parse(PackedStringArray(["--screenshot=bild.png"]))
	_true(relative.resolved_screenshot_path().is_absolute_path(), "relativer Pfad wird absolut")


func _test_map_config() -> void:
	var text := """# Kommentar mit { Klammern } und = Zeichen
karte {
	bild_breite = 1024
	bild_hoehe  = 1152   # Zeilenkommentar
	laenge_min  = 9.5
	laenge_max  = 16.5
	breite_min  = 50.0
	breite_max  = 54.8
	bezugsbreite = 52.4
	max_radius_km = 42
	kueste = {
		9.5 54.6   # Feld aus M1, wird seit M3 ignoriert
		13.0 54.2
	}
	unbekannt = "Text mit # und {"
}
"""
	var config := MapConfig.from_text(text)
	if not _true(config != null, "karte.chron gelesen"):
		return
	_equal(config.image_width, 1024, "bild_breite")
	_equal(config.image_height, 1152, "bild_hoehe")
	_equal(config.reference_latitude, 52.4, "bezugsbreite")
	_equal(config.max_radius_km, 42.0, "max_radius_km")
	_true(
		config.pixel_to_geo(Vector2(512, 576)).is_equal_approx(Vector2(13.0, 52.4)),
		"Rückprojektion Mitte"
	)
	var top_left := config.geo_to_pixel(9.5, 54.8)
	var bottom_right := config.geo_to_pixel(16.5, 50.0)
	_true(top_left.is_equal_approx(Vector2.ZERO), "Projektion linke obere Ecke")
	_true(bottom_right.is_equal_approx(Vector2(1024, 1152)), "Projektion rechte untere Ecke")
	_true(config.geo_to_pixel(13.0, 52.4).is_equal_approx(Vector2(512, 576)), "Projektion Mitte")
	var unusable := MapConfig.from_text("karte { bild_breite = 0 }")
	_true(unusable == null, "unbrauchbare karte.chron abgelehnt")
	_true(MapConfig.from_text("") == null, "leere karte.chron abgelehnt")


func _test_paths() -> void:
	var data_dir := UhrwerkPaths.data_dir()
	_true(data_dir.is_absolute_path(), "Datenordner absolut: %s" % data_dir)
	_true(not data_dir.contains(".."), "Datenordner normalisiert")
	_true(not data_dir.ends_with("/"), "Datenordner ohne abschließenden Schrägstrich")
	var map_dir := data_dir.path_join("karte")
	_equal(UhrwerkPaths.province_image_path(), map_dir.path_join("provinzen.png"), "provinzen.png")
	_equal(UhrwerkPaths.definition_csv_path(), map_dir.path_join("definition.csv"), "CSV-Pfad")


func _test_legend_gradient() -> void:
	var start := Color(0.9, 0.8, 0.7)
	var finish := Color(0.3, 0.2, 0.1)
	var values := PackedFloat32Array([10.0, 30.0, 60.0, 90.0, 150.0])
	var samples := PackedColorArray()
	for value in values:
		samples.append(start.lerp(finish, minf(value / 120.0, 1.0)))
	var gradient := LegendData.gradient_from_samples(values, samples, 120.0)
	_equal(gradient.get_point_count(), 2, "linearer Verlauf erkannt")
	_true(gradient.get_color(0).is_equal_approx(start), "Verlauf beginnt bei der Farbe für 0")
	_true(gradient.get_color(1).is_equal_approx(finish), "Verlauf endet bei der Farbe für 120")
	var stepped := PackedColorArray([Color.RED, Color.GREEN, Color.BLUE, Color.RED, Color.GREEN])
	var fallback := LegendData.gradient_from_samples(values, stepped, 120.0)
	_true(fallback.get_point_count() >= 3, "nicht linearer Verlauf nutzt vorkommende Farben")
	var empty := LegendData.gradient_from_samples(PackedFloat32Array(), PackedColorArray(), 120.0)
	_equal(empty.get_point_count(), 1, "leerer Verlauf")


func _test_legend_entries() -> void:
	var entry := LegendData.no_data_entry(4481)
	_equal(entry["text"], "Keine Daten", "Legendeneintrag „Keine Daten“")
	_true(bool(entry["hatch"]), "Eintrag „Keine Daten“ mit Schraffur")
	_true(str(entry["detail"]).contains("4.481 Orte"), "Anzahl der Orte ohne Daten")
	_equal(LegendData.count_text(1), "1 Ort", "count_text Einzahl")
	_equal(LegendData.count_text(1234), "1.234 Orte", "count_text Mehrzahl")
	var swatch := LegendPanel.hatch_swatch()
	_true(swatch.texture != null, "Schraffurfeld der Legende")
	swatch.free()


func _test_map_modes() -> void:
	_equal(MapModes.COUNT, 6, "sechs Kartenmodi")
	_equal(MapModes.MAP_KREISE, 5, "Kartenmodus Kreise = 5 (bridge-api.md)")
	_equal(MapModes.NAMES.size(), MapModes.COUNT, "Namen je Kartenmodus")
	_equal(MapModes.DESCRIPTIONS.size(), MapModes.COUNT, "Beschreibungen je Kartenmodus")
	_equal(MapModes.KEY_HINTS.size(), MapModes.COUNT, "Tastenhinweise je Kartenmodus")
	_equal(MapModes.ACTIONS.size(), MapModes.COUNT, "Aktionen je Kartenmodus")
	_true(not MapModes.DATA_MODES.has(MapModes.MAP_TERRAIN), "Gelände ohne Schraffur")
	_true(not MapModes.DATA_MODES.has(MapModes.MAP_KREISE), "Kreise ohne Schraffur")

	for action in MapModes.ACTIONS:
		_true(InputMap.has_action(action), "Aktion %s in project.godot" % action)
	for action in MapCamera.PAN_ACTIONS:
		_true(InputMap.has_action(action), "Aktion %s in project.godot" % action)
	for action: StringName in [
		&"tempo_pause", &"tempo_1", &"tempo_5", &"auswahl_aufheben", &"kamera_uebersicht",
		&"kamera_hauptstadt"
	]:
		_true(InputMap.has_action(action), "Aktion %s in project.godot" % action)
	var z := InputEventKey.new()
	z.physical_keycode = KEY_Y
	_equal(MapModes.mode_for_event(z), MapModes.MAP_KREISE, "Taste rechts neben T: Kreise")
	var w := InputEventKey.new()
	w.physical_keycode = KEY_W
	_equal(MapModes.mode_for_event(w), MapModes.MAP_DENSITY, "W gehört zum Kartenmodus Dichte")
	_true(MapCamera.is_pan_event(w), "W gehört zur Kamerabewegung")
	var e := InputEventKey.new()
	e.physical_keycode = KEY_E
	_equal(MapModes.mode_for_event(e), MapModes.MAP_CONFESSION, "E gehört zum Modus Konfession")
	_true(not MapCamera.is_pan_event(e), "E bewegt die Kamera nicht")


func _test_map_detail() -> void:
	_equal(MapDetail.tier_alpha(MapDetail.Tier.STATE, 8.0), 1.0, "fern: Staatsnamen")
	_equal(MapDetail.tier_alpha(MapDetail.Tier.KREIS, 8.0), 0.0, "fern: keine Kreisnamen")
	_equal(MapDetail.tier_alpha(MapDetail.Tier.PLACE, 8.0), 0.0, "fern: keine Ortsnamen")
	_equal(MapDetail.tier_alpha(MapDetail.Tier.KREIS, 26.0), 1.0, "mittel: Kreisnamen")
	_equal(MapDetail.tier_alpha(MapDetail.Tier.STATE, 26.0), 0.0, "mittel: keine Staatsnamen")
	_equal(MapDetail.tier_alpha(MapDetail.Tier.PLACE, 64.0), 1.0, "nah: Ortsnamen")
	_equal(MapDetail.tier_alpha(MapDetail.Tier.KREIS, 64.0), 0.0, "nah: keine Kreisnamen")
	_equal(
		MapDetail.label_tier(MapDetail.KREIS_LABELS_FROM), MapDetail.Tier.KREIS,
		"Kreisnamen ab der Schwelle"
	)
	_equal(
		MapDetail.label_tier(MapDetail.PLACE_LABELS_FROM - 0.01), MapDetail.Tier.KREIS,
		"Kreisnamen bis knapp unter die Ortsschwelle"
	)
	var sizes := PackedFloat32Array([1.0, 9.0, 15.0, 17.0, 20.0, 37.0, 40.0, 45.0, 90.0])
	for size in sizes:
		var total := 0.0
		for tier in MapDetail.Tier.size():
			total += MapDetail.tier_alpha(tier, size)
		_equal(total, 1.0, "genau eine Namen-Ebene bei %s px" % size)
	_equal(MapDetail.kreis_border_alpha(5.0), 0.0, "fern: keine Kreisgrenzen")
	_equal(MapDetail.kreis_border_alpha(26.0), 1.0, "mittel: Kreisgrenzen")
	_equal(MapDetail.location_border_alpha(26.0), 0.0, "mittel: keine Ortsgrenzen")
	_equal(MapDetail.location_border_alpha(64.0), 1.0, "nah: Ortsgrenzen")
	for level in MapDetail.LEVEL_SIZES.size():
		_equal(
			MapDetail.level_name(MapDetail.LEVEL_SIZES[level]),
			MapDetail.LEVEL_NAMES[level],
			"Stufe %s" % MapDetail.LEVEL_NAMES[level]
		)
	_true(
		is_equal_approx(MapDetail.screen_size(MapDetail.zoom_for(26.0, 36.0), 36.0), 26.0),
		"zoom_for und screen_size"
	)


## Kleine Testkarte 8 × 6 Pixel: Orte A (4 × 3), B (4 × 3), C (2 × 3), Meer, See, Ausland.
##   y 0..2:  A A A A B B B B
##   y 3..4:  C C ~ ~ ~ ~ s s      (~ Meer, s Binnengewässer)
##   y 5:     C C ~ ~ ~ ~ x x      (x Ausland)
func _test_map_data_and_layout() -> void:
	var dir := ProjectSettings.globalize_path(TEST_MAP_DIR)
	DirAccess.make_dir_recursive_absolute(dir)
	var image := Image.create_empty(8, 6, false, Image.FORMAT_RGB8)
	for y in 6:
		for x in 8:
			var value := 0
			var area: int = MapData.Area.CORE_LAND
			if y <= 2:
				value = 1 if x <= 3 else 2
			elif x <= 1:
				value = 3
			elif x <= 5:
				area = MapData.Area.SEA
			elif y <= 4:
				area = MapData.Area.INLAND_WATER
			else:
				area = MapData.Area.FOREIGN
			image.set_pixel(x, y, Color8(value & 0xFF, value >> 8, area))
	image.save_png(dir.path_join("provinzen.png"))
	_write_text(
		dir.path_join("definition.csv"),
		(
			"kartenindex;ort;name;x;y;pixel;flaeche_km2\n0;a_ort;A-Ort;1;1;12;5\n"
			+ "1;b_ort;B-Ort;5;1;12;5\n2;c_ort;C-Ort;0;4;6;2\n"
		)
	)
	DirAccess.remove_absolute(dir.path_join("fluesse.png"))

	var map := MapData.load_from_dir(dir)
	if not _true(map.is_valid(), "Testkarte geladen: %s" % map.error):
		return
	_equal(map.map_count(), 3, "drei Kartenindizes")
	_equal(map.map_names[1], "B-Ort", "Name aus definition.csv")
	_equal(map.label_points[0], Vector2(1.5, 1.5), "Beschriftungspunkt = Pixelmitte")
	_equal(map.pixel_counts[2], 6, "Pixelzahl aus definition.csv")
	_equal(map.areas_km2[2], 2, "Fläche aus definition.csv")
	_equal(map.map_index_at_pixel(Vector2i(5, 2)), 1, "Kartenindex am Pixel")
	_equal(map.map_index_at_pixel(Vector2i(3, 4)), -1, "Meer ohne Ort")
	_equal(map.map_index_at_pixel(Vector2i(8, 0)), -1, "außerhalb des Bildes")
	_equal(map.area_at_pixel(Vector2i(3, 4)), MapData.Area.SEA, "Flächenart Meer")
	_equal(map.area_at_pixel(Vector2i(6, 3)), MapData.Area.INLAND_WATER, "Binnengewässer")
	_equal(map.area_at_pixel(Vector2i(7, 5)), MapData.Area.FOREIGN, "Flächenart Ausland")
	_equal(map.area_at_pixel(Vector2i(0, 0)), MapData.Area.CORE_LAND, "Kernraum-Land")
	_true(map.river_texture == null, "ohne fluesse.png keine Flussebene")
	_true(_any_contains(map.warnings, "Flussebene fehlt"), "Hinweis auf fehlende Flussebene")
	_true(is_equal_approx(map.typical_location_diameter(), sqrt(12.0)), "typischer Durchmesser")
	_equal(MapData.lut_size(4541), Vector2i(256, 18), "Nachschlagetextur 256 × 18")
	_equal(MapData.lut_size(0), Vector2i(256, 1), "Nachschlagetextur mindestens eine Zeile")
	_equal(MapData.lut_coords(300), Vector2i(44, 1), "Texel eines Kartenindex")

	var rivers := Image.create_empty(8, 6, false, Image.FORMAT_L8)
	rivers.save_png(dir.path_join("fluesse.png"))
	_true(MapData.load_from_dir(dir).river_texture != null, "fluesse.png wird geladen")

	_test_label_layout(map)

	_write_text(dir.path_join("definition.csv"), "kartenindex;ort;name\n0;a_ort;A-Ort\n")
	var outdated := MapData.load_from_dir(dir)
	_true(not outdated.is_valid(), "definition.csv vor M3 wird abgelehnt")
	_true(outdated.error.contains("Format vor M3"), "Meldung nennt das alte Format")
	for file: String in ["provinzen.png", "fluesse.png", "definition.csv"]:
		DirAccess.remove_absolute(dir.path_join(file))
	DirAccess.remove_absolute(dir)


func _test_label_layout(map: MapData) -> void:
	var all := PackedInt32Array([0, 1, 2])
	_equal(MapLabelLayout.clusters(all, map, 4.5).size(), 1, "ein Gebiet bei 4,5 Pixeln")
	var split := MapLabelLayout.clusters(all, map, 3.5)
	_equal(split.size(), 2, "zwei Gebiete bei 3,5 Pixeln")
	if split.size() == 2:
		_equal(split[0], PackedInt32Array([0, 2]), "A und C bilden ein Gebiet")
	_equal(
		MapLabelLayout.weighted_center(PackedInt32Array([0, 1]), map),
		Vector2(3.5, 1.5),
		"pixelgewichteter Schwerpunkt"
	)
	_equal(MapLabelLayout.pixel_sum(all, map), 30, "Pixelsumme")
	var groups := PackedInt32Array([0, 1, 1])
	_equal(
		MapLabelLayout.anchor(PackedInt32Array([1, 2]), map, groups, 1),
		Vector2(5.5, 1.5),
		"Schwerpunkt außerhalb der Gruppe rückt auf den nächsten Ort"
	)
	var inside := PackedInt32Array([0, 1, 0])
	var center := MapLabelLayout.anchor(PackedInt32Array([0, 2]), map, inside, 0)
	_true(center.is_equal_approx(Vector2(21.0 / 18.0, 2.5)), "Schwerpunkt in der Gruppe bleibt")
	var states := MapLabelLayout.state_labels(map, PackedInt32Array([0, 0, 1]), 3, sqrt(12.0))
	_equal(states.size(), 2, "ein Staatsname je Staat mit Orten")
	if states.size() == 2:
		_equal(states[0]["point"], Vector2(3.5, 1.5), "Staatsname im Schwerpunkt")
		_equal(states[1]["point"], Vector2(0.5, 4.5), "Staatsname eines Einzelortes")
		_equal(int(states[0]["pixels"]), 24, "Fläche des Staates")
	var kreise := MapLabelLayout.kreis_labels(map, PackedInt32Array([0, 0, -1]), 2)
	_equal(kreise.size(), 1, "Kreise ohne Orte haben keinen Namen")


func _test_location_catalog() -> void:
	var catalog := LocationCatalog.new()
	catalog.names = PackedStringArray(["Teltow", "Zossen"])
	catalog.kreis_indices = PackedInt32Array([0, -1])
	catalog.kreis_names = PackedStringArray(["Teltow"])
	catalog.data_count = 1
	_equal(catalog.kreis_name_of(0), "Teltow", "Kreisname eines Ortes")
	_equal(catalog.kreis_name_of(1), "", "Ort ohne Kreis")
	_equal(catalog.kreis_name_of(5), "", "ungültiger Index")
	_equal(catalog.no_data_count(), 1, "Orte ohne Daten")
	_equal(LocationCatalog.build(null).size(), 0, "Katalog ohne Simulation leer")


func _write_text(path: String, text: String) -> void:
	var file := FileAccess.open(path, FileAccess.WRITE)
	file.store_string(text)
	file.close()


func _any_contains(lines: PackedStringArray, part: String) -> bool:
	for line in lines:
		if line.contains(part):
			return true
	return false


func _equal(actual: Variant, expected: Variant, what: String) -> void:
	var message := "%s (erwartet „%s“, erhalten „%s“)" % [what, expected, actual]
	_true(actual == expected, message)


func _true(condition: bool, what: String) -> bool:
	_count += 1
	if not condition:
		_failures.append(what)
		printerr("  FEHLER  ", what)
	return condition
