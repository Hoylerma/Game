extends SceneTree
## Einheitentests der Hilfsklassen, die ohne GDExtension auskommen.
##
## Aufruf aus dem Repository-Hauptordner (nach einem Import des Projekts):
##   godot --headless --path game --script res://tests/unit_test.gd
## Ergebnis: "UNIT OK" und Exitcode 0, sonst Fehlermeldungen und Exitcode 1.

const NBSP := " "

var _failures := PackedStringArray()
var _count := 0


func _initialize() -> void:
	print("EINIGKEIT – Einheitentests der Godot-Seite")
	_test_german_format()
	_test_launch_options()
	_test_map_config()
	_test_paths()
	_test_legend_gradient()
	_test_map_modes()
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
	var bad_args := PackedStringArray(["--tage=-3", "--kartenmodus=7", "--unbekannt"])
	var bad := LaunchOptions.parse(bad_args)
	_equal(bad.problems.size(), 3, "fehlerhafte Optionen gemeldet")
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
	max_radius_km = 42
	kueste = {
		9.5 54.6   # erster Punkt
		13.0 54.2
		16.5 54.5
	}
	unbekannt = "Text mit # und {"
}
"""
	var config := MapConfig.from_text(text)
	if not _true(config != null, "karte.chron gelesen"):
		return
	_equal(config.image_width, 1024, "bild_breite")
	_equal(config.image_height, 1152, "bild_hoehe")
	_equal(config.coast.size(), 3, "Küstenpunkte")
	_true(config.coast[1].is_equal_approx(Vector2(13.0, 54.2)), "Küstenpunkt 2")
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


func _test_map_modes() -> void:
	_equal(MapModes.NAMES.size(), MapModes.COUNT, "Namen je Kartenmodus")
	_equal(MapModes.ACTIONS.size(), MapModes.COUNT, "Aktionen je Kartenmodus")
	for action in MapModes.ACTIONS:
		_true(InputMap.has_action(action), "Aktion %s in project.godot" % action)
	for action in MapCamera.PAN_ACTIONS:
		_true(InputMap.has_action(action), "Aktion %s in project.godot" % action)
	for action: StringName in [&"tempo_pause", &"tempo_1", &"tempo_5", &"auswahl_aufheben"]:
		_true(InputMap.has_action(action), "Aktion %s in project.godot" % action)
	var w := InputEventKey.new()
	w.physical_keycode = KEY_W
	_equal(MapModes.mode_for_event(w), MapModes.MAP_DENSITY, "W gehört zum Kartenmodus Dichte")
	_true(MapCamera.is_pan_event(w), "W gehört zur Kamerabewegung")
	var e := InputEventKey.new()
	e.physical_keycode = KEY_E
	_equal(MapModes.mode_for_event(e), MapModes.MAP_CONFESSION, "E gehört zum Modus Konfession")
	_true(not MapCamera.is_pan_event(e), "E bewegt die Kamera nicht")


func _equal(actual: Variant, expected: Variant, what: String) -> void:
	var message := "%s (erwartet „%s“, erhalten „%s“)" % [what, expected, actual]
	_true(actual == expected, message)


func _true(condition: bool, what: String) -> bool:
	_count += 1
	if not condition:
		_failures.append(what)
		printerr("  FEHLER  ", what)
	return condition
