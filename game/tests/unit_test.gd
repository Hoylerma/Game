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
	_test_format()
	_test_launch_options()
	_test_icons()
	_test_map_config()
	_test_paths()
	_test_legend_gradient()
	_test_legend_entries()
	_test_map_modes()
	_test_railway_layer()
	_test_army_layer()
	_test_battle_reports()
	_test_map_detail()
	_test_map_data_and_layout()
	_test_location_catalog()
	_test_save_slots()
	_test_election_tally()
	_test_party_panel()
	_test_event_window()
	_test_chronicle_export()
	_test_window_manager()
	_test_ticker_and_glossary()
	_test_settings_and_lexicon()
	_test_epoch_and_newspaper()
	_test_audio_director()
	_test_onboarding()
	_test_language()
	if _failures.is_empty():
		print("UNIT OK (%d Prüfungen)" % _count)
		quit(0)
	else:
		printerr("UNIT FEHLGESCHLAGEN: %d von %d Prüfungen" % [_failures.size(), _count])
		for failure in _failures:
			printerr("  - ", failure)
		quit(1)


func _test_format() -> void:
	_equal(Format.integer(0), "0", "integer 0")
	_equal(Format.integer(999), "999", "integer 999")
	_equal(Format.integer(1000), "1.000", "integer 1000")
	_equal(Format.integer(1234567), "1.234.567", "integer 1234567")
	_equal(Format.integer(-75300), "−75.300", "integer negativ")
	_equal(Format.number(1234.56, 1), "1.234,6", "number mit Nachkommastelle")
	_equal(Format.number(0.04, 1), "0,0", "number gerundet")
	_equal(Format.number(-0.04, 1), "0,0", "number ohne negatives Null")
	_equal(Format.number(-2.5, 2), "−2,50", "number negativ")
	_equal(Format.number(999.96, 1), "1.000,0", "number Übertrag")
	_equal(Format.signed_number(8339.4), "+8.339", "signed positiv")
	_equal(Format.signed_number(-9667.0), "−9.667", "signed negativ")
	_equal(Format.signed_number(0.2), "0", "signed null ohne Vorzeichen")
	_equal(Format.money(23000.0), "23.000" + NBSP + "Taler", "money")
	_equal(Format.percent(12), "12" + NBSP + "%", "percent")
	_equal(Format.percent(12.5, 1), "12,5" + NBSP + "%", "percent mit Nachkommastelle")
	_equal(Format.area(150.0), "150" + NBSP + "km²", "area")
	_equal(Format.density(82.34), "82,3" + NBSP + "Einw./km²", "density")
	_equal(
		Format.government("staendisches_fuerstentum"),
		"Ständisches Fürstentum",
		"Regierungsform bekannt"
	)
	_equal(Format.government("neue_form"), "Neue Form", "Regierungsform unbekannt")
	_equal(Format.humanize("flussaue"), "Flussaue", "humanize Bezeichner")
	_equal(Format.humanize("Hügelland"), "Hügelland", "humanize Name bleibt")
	_equal(Format.humanize(""), "–", "humanize leer")
	# Seit M13b4: Regeln je Sprache.
	_equal(Format.date(1, 3, 1848), "1. März 1848", "Datum deutsch")
	_equal(Format.date_of({"day": 18, "month": 3, "year": 1848}), "18. März 1848", "Datum aus Dictionary")
	# Seit M14a2: kurze große Zahlen für die obere Leiste.
	_equal(Format.compact(15_979_042.0), "15,98 Mio.", "compact Millionen")
	_equal(Format.compact(250_400.0), "250 Tsd.", "compact Tausende")
	_equal(Format.compact(12_345.0), "12.345", "compact darunter")
	Format.set_locale("en")
	_equal(Format.compact(15_979_042.0), "15.98 M", "compact englisch")
	_equal(Format.integer(1234567), "1,234,567", "integer englisch")
	_equal(Format.number(1234.56, 1), "1,234.6", "number englisch")
	_equal(Format.density(82.34), "82.3" + NBSP + "inh./km²", "density englisch")
	_equal(Format.date(1, 3, 1848), "1 March 1848", "Datum englisch")
	_equal(Format.money(23000.0), "23,000" + NBSP + "Taler", "Taler bleiben Taler")
	Format.set_locale("fr")
	_equal(Format.integer(1000), "1.000", "unbekannte Sprache: deutsch")
	Format.set_locale("de")


## Symbole (seit M14a2): jedes Symbol der Bereiche, Kartenmodi und Kennzahlen vorhanden und ladbar, in Größe
## und Farbe gerendert (das Papier bleibt, die Tinte wechselt).
func _test_icons() -> void:
	var wanted := PackedStringArray()
	for area: Array in AreaBar.AREAS:
		wanted.append(str(area[0]))
	for icon in MapModeBar.ICONS:
		wanted.append(str(icon))
	wanted.append_array(["kasse", "legitimitaet", "unruhe", "bevoelkerung", "heer", "vermaechtnis"])
	var missing := PackedStringArray()
	for icon in wanted:
		if icon == "bevoelkerung":
			continue # die Bevölkerung zeigt das Symbol der Gesellschaft
		if not Icons.exists(StringName(icon)) or Icons.texture(StringName(icon), 24.0) == null:
			missing.append(icon)
	_true(missing.is_empty(), "alle Symbole vorhanden (%d), fehlend: %s" % [wanted.size(), ", ".join(missing)])
	var texture := Icons.texture(&"kabinett", 24.0)
	_true(texture != null and texture.get_width() == 48, "Symbol mit doppelter Auflösung (%s)" % str(texture.get_size() if texture != null else Vector2.ZERO))
	_true(Icons.texture(&"kabinett", 24.0) == texture, "Symbole zwischengespeichert")
	var paper := Icons.texture(&"kabinett", 24.0, UiPalette.PAPER)
	_true(paper != null and paper != texture, "Symbol in anderer Tinte")
	_true(Icons.texture(&"gibt_es_nicht") == null and not Icons.exists(&"gibt_es_nicht"), "unbekanntes Symbol: null")
	var seen := PackedInt32Array()
	for group in MapModeBar.GROUPS:
		for mode in group:
			seen.append(int(mode))
	seen.sort()
	var expected := PackedInt32Array(range(MapModes.COUNT))
	_true(seen == expected and MapModeBar.ICONS.size() == MapModes.COUNT, "jeder Kartenmodus in genau einer Gruppe mit Symbol (%s)" % str(seen))


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
		[
			"--tage=-3", "--kartenmodus=18", "--unbekannt", "--zoom=0", "--zoom=viel", "--szenario",
			"--staat=", "--partei=", "--verfassung=ja", "--zollunion=ja", "--diplomatie=krieg",
			"--krieg=frieden", "--heer=1", "--ereignis=ja", "--journal=1"
		]
	)
	var bad := LaunchOptions.parse(bad_args)
	_equal(bad.problems.size(), 15, "fehlerhafte Optionen gemeldet")
	_true(not bad.show_events and not bad.show_journal, "fehlerhaftes --ereignis und --journal gelten nicht")
	var yearbook := LaunchOptions.parse(PackedStringArray(["--jahrbuch"]))
	_true(yearbook.show_yearbook and yearbook.problems.is_empty(), "--jahrbuch öffnet das Jahrbuch")
	_true(not LaunchOptions.parse(PackedStringArray(["--jahrbuch=1"])).show_yearbook, "--jahrbuch=1 gilt nicht")
	var area := LaunchOptions.parse(PackedStringArray(["--bereich=parlament"]))
	_true(area.area == "parlament" and area.problems.is_empty(), "--bereich=parlament")
	var settings_option := LaunchOptions.parse(PackedStringArray(["--einstellungen=tasten", "--farbsehschwaeche"]))
	_true(settings_option.settings_tab == 4 and settings_option.color_blind and settings_option.problems.is_empty(), "--einstellungen=tasten --farbsehschwaeche")
	_true(LaunchOptions.parse(PackedStringArray(["--einstellungen"])).settings_tab == 0, "--einstellungen ohne Reiter")
	_true(LaunchOptions.parse(PackedStringArray(["--bereich=lexikon"])).area == "lexikon", "--bereich=lexikon")
	var bad_area := LaunchOptions.parse(PackedStringArray(["--bereich=hof"]))
	_true(bad_area.area.is_empty() and bad_area.problems.size() == 1, "--bereich=hof gilt nicht")
	var events := LaunchOptions.parse(PackedStringArray(["--ereignis", "--journal", "--kartenmodus=16"]))
	_true(
		events.show_events and events.show_journal and events.map_mode == 16 and events.problems.is_empty()
		and not defaults.show_events and not defaults.show_journal,
		"Optionen --ereignis, --journal und --kartenmodus=16 (seit M11)"
	)
	_true(bad.war_tab == -1 and not bad.select_army, "fehlerhaftes --krieg und --heer öffnen nichts")
	_equal(bad.diplomacy_tab, -1, "fehlerhaftes --diplomatie öffnet nichts")
	var start := LaunchOptions.parse(PackedStringArray(["--startbildschirm=einstellungen"]))
	var start_bad := LaunchOptions.parse(PackedStringArray(["--startbildschirm=hof"]))
	_true(
		start.start_page == 2 and start.problems.is_empty() and defaults.start_page == -1
		and start_bad.start_page == -1 and start_bad.problems.size() == 1
		and LaunchOptions.parse(PackedStringArray(["--startbildschirm"])).start_page == 0,
		"Option --startbildschirm (seit M13)"
	)
	var party := LaunchOptions.parse(PackedStringArray(["--szenario=voelkerfruehling", "--partei=liberale"]))
	_true(party.party_id == "liberale" and party.problems.is_empty(), "Option --partei (seit M8)")
	_equal(defaults.party_id, "", "ohne --partei spielt man den Staat")
	var politics := LaunchOptions.parse(PackedStringArray(["--verfassung"]))
	_true(politics.show_politics and not defaults.show_politics, "Option --verfassung (seit M8)")
	var customs := LaunchOptions.parse(PackedStringArray(["--zollunion", "--kartenmodus=13"]))
	_true(
		customs.show_customs and customs.map_mode == 13 and customs.problems.is_empty()
		and not defaults.show_customs,
		"Optionen --zollunion und --kartenmodus=13 (seit M9)"
	)
	var diplomacy := LaunchOptions.parse(PackedStringArray(["--diplomatie", "--kartenmodus=14"]))
	_true(
		diplomacy.diplomacy_tab == 0 and diplomacy.map_mode == 14 and diplomacy.problems.is_empty()
		and defaults.diplomacy_tab == -1,
		"Optionen --diplomatie und --kartenmodus=14 (seit M9)"
	)
	_equal(LaunchOptions.parse(PackedStringArray(["--diplomatie=bund"])).diplomacy_tab, 1, "--diplomatie=bund")
	_equal(LaunchOptions.parse(PackedStringArray(["--diplomatie=frage"])).diplomacy_tab, 2, "--diplomatie=frage")
	var war := LaunchOptions.parse(PackedStringArray(["--krieg", "--heer", "--kartenmodus=15"]))
	_true(
		war.war_tab == 0 and war.select_army and war.map_mode == 15 and war.problems.is_empty()
		and defaults.war_tab == -1 and not defaults.select_army,
		"Optionen --krieg, --heer und --kartenmodus=15 (seit M10)"
	)
	_equal(LaunchOptions.parse(PackedStringArray(["--krieg=heere"])).war_tab, 1, "--krieg=heere")
	var chosen := LaunchOptions.parse(
		PackedStringArray(["--szenario=voelkerfruehling", "--staat=bav"])
	)
	_equal(chosen.scenario_id, "voelkerfruehling", "Option --szenario")
	_equal(chosen.state_tag, "BAV", "Option --staat (Großbuchstaben)")
	_equal(chosen.problems.size(), 0, "--szenario und --staat ohne Probleme")
	_equal(bad.zoom_factor, 0.0, "fehlerhaftes --zoom ignoriert")
	var night := LaunchOptions.parse(PackedStringArray(["--wahlabend", "--kartenmodus=12"]))
	_true(night.show_election_night and night.problems.is_empty(), "Option --wahlabend (seit M8)")
	_equal(night.map_mode, MapModes.MAP_WAHLEN, "--kartenmodus=12 ist der Kartenmodus Wahlen")
	var budget := LaunchOptions.parse(PackedStringArray(["--haushalt"]))
	_equal(budget.show_budget and budget.problems.is_empty(), true, "Option --haushalt")
	_equal(defaults.show_budget, false, "Standard ohne --haushalt")
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
	_equal(LegendData.states_text(1) + ", " + LegendData.states_text(26), "1 Staat, 26 Staaten", "states_text")
	# Beziehungen (seit M9): grün freundlich, rot feindlich, neutral in der Mitte.
	_true(
		LegendData.relation_color(0.0).is_equal_approx(LegendData.RELATION_NEUTRAL)
		and LegendData.relation_color(100.0).is_equal_approx(LegendData.RELATION_FRIEND)
		and LegendData.relation_color(-250.0).is_equal_approx(LegendData.RELATION_FOE),
		"Farben der Beziehungen"
	)
	var swatch := LegendPanel.hatch_swatch()
	_true(swatch.texture != null, "Schraffurfeld der Legende")
	swatch.free()


func _test_map_modes() -> void:
	_equal(MapModes.COUNT, 18, "achtzehn Kartenmodi (Bund seit M13)")
	_equal(MapModes.MAP_BUND, 17, "Kartenmodus Bund = 17 (bridge-api.md)")
	_equal(MapModes.KEY_HINTS[MapModes.MAP_BUND], "C", "Taste C für Bund")
	var c_key := InputEventKey.new()
	c_key.physical_keycode = KEY_C
	_equal(MapModes.mode_for_event(c_key), MapModes.MAP_BUND, "Taste C: Bund")
	_equal(MapModes.MAP_UNRUHE, 16, "Kartenmodus Unruhe = 16 (bridge-api.md)")
	_equal(MapModes.KEY_HINTS[MapModes.MAP_UNRUHE], "X", "Taste X für Unruhe")
	_true(InputMap.has_action(&"kartenmodus_unruhe"), "Aktion für den Kartenmodus Unruhe")
	var x_key := InputEventKey.new()
	x_key.physical_keycode = KEY_X
	_equal(MapModes.mode_for_event(x_key), MapModes.MAP_UNRUHE, "Taste X: Unruhe")
	_equal(MapModes.MAP_KRIEG, 15, "Kartenmodus Krieg = 15 (bridge-api.md)")
	_equal(MapModes.KEY_HINTS[MapModes.MAP_KRIEG], "Y", "Taste Y (links neben X) für Krieg")
	_true(InputMap.has_action(&"kartenmodus_krieg"), "Aktion für den Kartenmodus Krieg")
	_true(MapModes.DATA_MODES.has(MapModes.MAP_KRIEG), "Krieg mit Schraffur")
	var y_key := InputEventKey.new()
	y_key.physical_keycode = KEY_Z
	_equal(MapModes.mode_for_event(y_key), MapModes.MAP_KRIEG, "Taste links neben X: Krieg")
	_equal(MapModes.MAP_BEZIEHUNGEN, 14, "Kartenmodus Beziehungen = 14 (bridge-api.md)")
	_equal(MapModes.KEY_HINTS[MapModes.MAP_BEZIEHUNGEN], "#", "Taste # für Beziehungen")
	_true(InputMap.has_action(&"kartenmodus_beziehungen"), "Aktion für den Kartenmodus Beziehungen")
	_true(MapModes.DATA_MODES.has(MapModes.MAP_BEZIEHUNGEN), "Beziehungen mit Schraffur")
	_equal(MapModes.MAP_ZOLLGEBIETE, 13, "Kartenmodus Zollgebiete = 13 (bridge-api.md)")
	_equal(MapModes.KEY_HINTS[MapModes.MAP_ZOLLGEBIETE], "Ö", "Taste Ö für Zollgebiete")
	_true(InputMap.has_action(&"kartenmodus_zollgebiete"), "Aktion für den Kartenmodus Zollgebiete")
	_true(MapModes.DATA_MODES.has(MapModes.MAP_ZOLLGEBIETE), "Zollgebiete mit Schraffur")
	_equal(MapModes.MAP_WAHLEN, 12, "Kartenmodus Wahlen = 12 (bridge-api.md)")
	_equal(MapModes.KEY_HINTS[MapModes.MAP_WAHLEN], "Ä", "Taste Ä für Wahlen")
	_true(InputMap.has_action(&"kartenmodus_wahlen"), "Aktion für den Kartenmodus Wahlen")
	_true(MapModes.DATA_MODES.has(MapModes.MAP_WAHLEN), "Wahlen mit Schraffur")
	_equal(MapModes.name_of(MapModes.MAP_VERWALTUNG), "Verwaltung", "Name des Modus Verwaltung")
	_equal(MapModes.MAP_KREISE, 5, "Kartenmodus Kreise = 5 (bridge-api.md)")
	_equal(MapModes.MAP_LEBENSSTANDARD, 6, "Kartenmodus Lebensstandard = 6 (bridge-api.md)")
	_equal(MapModes.MAP_KULTUR, 7, "Kartenmodus Kultur = 7 (bridge-api.md)")
	_equal(MapModes.MAP_WIRTSCHAFT, 8, "Kartenmodus Wirtschaft = 8 (bridge-api.md)")
	_equal(MapModes.MAP_MAERKTE, 9, "Kartenmodus Märkte = 9 (bridge-api.md)")
	_equal(MapModes.KEY_HINTS[8] + MapModes.KEY_HINTS[9], "OP", "Tasten O und P für Wirtschaft und Märkte")
	_true(
		InputMap.has_action(&"kartenmodus_wirtschaft") and InputMap.has_action(&"kartenmodus_maerkte"),
		"Aktionen für die Kartenmodi Wirtschaft und Märkte"
	)
	_true(MapModes.DATA_MODES.has(MapModes.MAP_LEBENSSTANDARD), "Lebensstandard mit Schraffur")
	_true(MapModes.DATA_MODES.has(MapModes.MAP_KULTUR), "Kultur mit Schraffur")
	_equal(MapModes.NAMES.size(), MapModes.COUNT, "Namen je Kartenmodus")
	_equal(MapModes.SHORT_NAMES.size(), MapModes.COUNT, "Kurznamen je Kartenmodus")
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
	var u := InputEventKey.new()
	u.physical_keycode = KEY_U
	_equal(MapModes.mode_for_event(u), MapModes.MAP_LEBENSSTANDARD, "U: Lebensstandard")
	var i_key := InputEventKey.new()
	i_key.physical_keycode = KEY_I
	_equal(MapModes.mode_for_event(i_key), MapModes.MAP_KULTUR, "I: Kultur")
	var w := InputEventKey.new()
	w.physical_keycode = KEY_W
	_equal(MapModes.mode_for_event(w), MapModes.MAP_DENSITY, "W gehört zum Kartenmodus Dichte")
	_true(MapCamera.is_pan_event(w), "W gehört zur Kamerabewegung")
	var e := InputEventKey.new()
	e.physical_keycode = KEY_E
	_equal(MapModes.mode_for_event(e), MapModes.MAP_CONFESSION, "E gehört zum Modus Konfession")
	_true(not MapCamera.is_pan_event(e), "E bewegt die Kamera nicht")


## Gestrichelt nur, was in den nächsten Jahren eröffnet (Review M7): 1648 zeigt keine Strecken.
func _test_railway_layer() -> void:
	var date := {"year": 1848, "month": 3, "day": 1}
	var section := {"open": false, "opened": {"year": 1853, "month": 3, "day": 1}}
	_true(RailwayLayer.is_shown(section, date), "Abschnitt genau fünf Jahre später gestrichelt")
	section["opened"] = {"year": 1853, "month": 3, "day": 2}
	_true(not RailwayLayer.is_shown(section, date), "Abschnitt später als fünf Jahre ausgeblendet")
	_true(
		not RailwayLayer.is_shown(section, {"year": 1648, "month": 10, "day": 24}),
		"1648 keine gestrichelten Strecken"
	)
	_true(RailwayLayer.is_shown({"open": true}, date), "eröffneter Abschnitt immer sichtbar")
	_true(not RailwayLayer.is_shown({"open": false}, date), "Abschnitt ohne Datum ausgeblendet")


## Heere auf der Karte (seit M10, Schritt b3): Stärke in Tausend, Garnisonen nur auf Wunsch,
## Auswahl per Weltpunkt.
## Schlachtberichte (seit M10, Schritt b3): höchstens MAX_REPORTS, die älteste weicht; × entfernt.
func _test_battle_reports() -> void:
	var reports := BattleReports.new()
	root.add_child(reports)
	_true(not reports.visible and reports.report_count() == 0, "ohne Meldung unsichtbar")
	for i in BattleReports.MAX_REPORTS + 1:
		reports.add_report("Sieg bei Ort %d" % i, "Schlacht bei Ort %d" % i, i, true)
	_equal(reports.report_count(), BattleReports.MAX_REPORTS, "höchstens MAX_REPORTS Meldungen")
	_true(
		not reports.reports_text().contains("Ort 0") and reports.reports_text().contains("Ort 3"),
		"die älteste Meldung weicht"
	)
	var shown: Array[int] = [-1] # Lambdas fangen Werte ein, ein Array teilen sie
	reports.location_requested.connect(func(index: int) -> void: shown[0] = index)
	(reports.get_child(0).find_child("Show", true, false) as Button).pressed.emit()
	_equal(shown[0], 1, "„Zeigen“ meldet den Ort der Schlacht")
	reports.clear()
	_true(not reports.visible and reports.report_count() == 0, "nach clear leer und unsichtbar")
	reports.free()


func _test_army_layer() -> void:
	_equal(ArmyLayer.strength_text(93000), "93", "Stärke in Tausend")
	_equal(ArmyLayer.strength_text(1499), "1", "Stärke gerundet")
	_equal(ArmyLayer.strength_text(800), "<1", "unter tausend")
	_equal(ArmyLayer.strength_text(205000), "205", "große Armee")
	var layer := ArmyLayer.new()
	var armies: Array[Dictionary] = [
		{"id": 1, "soldiers": 93000, "garrison": false, "staff": true},
		{"id": 2, "soldiers": 16000, "garrison": true, "staff": true},
		{"id": 3, "soldiers": 5000, "garrison": false, "staff": false},
	]
	var positions := PackedVector2Array([Vector2(100, 100), Vector2(300, 300), Vector2(100, 100)])
	var colors := PackedColorArray([Color.BLUE, Color.BLUE, Color.YELLOW])
	var own: Array[bool] = [true, true, false]
	layer.set_armies(armies, positions, colors, own)
	_equal(layer.marker_count(), 2, "Garnisonen ohne Kartenmodus Krieg ausgeblendet")
	layer.show_garrisons = true
	layer.set_armies(armies, positions, colors, own)
	_equal(layer.marker_count(), 3, "Garnisonen im Kartenmodus Krieg")
	layer.set_zoom(1.0)
	var first := layer.marker_center(1)
	var third := layer.marker_center(3)
	_true(first.y < 100.0 and absf(first.x - third.x) > 1.0, "zwei Heere im selben Ort nebeneinander über dem Ort")
	_equal(layer.army_at(first), 1, "Klick auf die Marke wählt das Heer")
	_equal(layer.army_at(third), 3, "zweite Marke im selben Ort")
	_equal(layer.army_at(Vector2(500, 500)), -1, "daneben kein Heer")
	layer.set_selection(1, PackedVector2Array([Vector2(100, 100), Vector2(200, 100)]))
	_equal(layer.selected_army(), 1, "Auswahl gespeichert")
	layer.free()


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
	# Leere Ebene (ohne Kreisdaten): die nächste nicht leere, die genauere zuerst.
	var labels := MapLabels.new()
	_equal(labels.shown_tier(26.0), -1, "keine Namen: keine Ebene")
	var entry: Array[Dictionary] = [{"world": Vector2(1, 1), "text": "A", "style": 0, "rank": 1.0}]
	labels.set_layer(MapDetail.Tier.STATE, entry)
	labels.set_layer(MapDetail.Tier.PLACE, entry)
	_equal(labels.shown_tier(8.0), MapDetail.Tier.STATE, "fern: Staatsnamen")
	_equal(labels.shown_tier(26.0), MapDetail.Tier.PLACE, "mittel ohne Kreise: Ortsnamen")
	_equal(labels.shown_tier(64.0), MapDetail.Tier.PLACE, "nah: Ortsnamen")
	labels.set_layer(MapDetail.Tier.KREIS, entry)
	_equal(labels.shown_tier(26.0), MapDetail.Tier.KREIS, "mittel mit Kreisen: Kreisnamen")
	labels.set_layer(MapDetail.Tier.PLACE, [] as Array[Dictionary])
	_equal(labels.shown_tier(64.0), MapDetail.Tier.KREIS, "nah ohne Orte: Kreisnamen")
	labels.free()


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
	catalog.kreis_names = PackedStringArray(["Teltow", "Leer"])
	_equal(catalog.kreise_with_places(), 1, "Legende: nur Kreise mit Orten")
	_equal(catalog.places_in_kreisen(), 1, "Legende: nur Orte mit Kreis")
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


## Spielstände (seit M6): Dateinamen, Jahreswechsel, Rotation, Tastenbelegung.
func _test_save_slots() -> void:
	_equal(SaveSlots.sanitize("  Brandenburg 1650-03-12  "), "Brandenburg 1650-03-12", "sanitize Rand")
	_equal(SaveSlots.sanitize("a/b:c*d?e\"f<g>h|i\\j"), "a_b_c_d_e_f_g_h_i_j", "sanitize verbotene Zeichen")
	_equal(SaveSlots.sanitize("Zeile\nzwei\t"), "Zeile_zwei_", "sanitize Steuerzeichen")
	_equal(SaveSlots.sanitize("partie.einigkeit"), "partie", "sanitize Endung")
	_equal(SaveSlots.sanitize(" ..versteckt.. "), "versteckt", "sanitize Punkte am Rand")
	_equal(SaveSlots.sanitize(" . "), "", "sanitize leer")
	_equal(SaveSlots.sanitize("Ä".repeat(80)).length(), SaveSlots.MAX_NAME_LENGTH, "sanitize Länge")
	var date := {"year": 1650, "month": 3, "day": 12}
	_equal(SaveSlots.default_name("Brandenburg", date), "Brandenburg 1650-03-12", "default_name")
	_equal(SaveSlots.default_name("", date), "1650-03-12", "default_name ohne Staat")
	for reserved: String in ["schnellspeicher", "Schnellspeicher", "autosave_1", "AUTOSAVE_3"]:
		_true(SaveSlots.is_reserved(reserved), "reservierter Name %s" % reserved)
	_true(not SaveSlots.is_reserved("Brandenburg 1650-03-12"), "freier Name")
	_true(SaveSlots.is_leap_year(1648) and SaveSlots.is_leap_year(1600), "Schaltjahre 1648, 1600")
	_true(not SaveSlots.is_leap_year(1700) and not SaveSlots.is_leap_year(1649), "keine Schaltjahre")
	_equal(SaveSlots.days_until_new_year({"year": 1648, "month": 10, "day": 24}), 69, "bis 1649 (Start)")
	_equal(SaveSlots.days_until_new_year({"year": 1649, "month": 12, "day": 31}), 1, "31. Dezember")
	_equal(SaveSlots.days_until_new_year({"year": 1649, "month": 1, "day": 1}), 365, "1. Januar 1649")
	_equal(SaveSlots.days_until_new_year({"year": 1648, "month": 1, "day": 1}), 366, "1. Januar 1648")
	_equal(SaveSlots.days_until_new_year({"year": 1700, "month": 2, "day": 28}), 307, "28. Februar 1700")
	_true(
		SaveSlots.format_modified(0).length() == 17
		and SaveSlots.format_modified(0).substr(2, 1) == ".",
		"format_modified %s" % SaveSlots.format_modified(0)
	)

	# Rotation der automatischen Spielstände in einem eigenen Ordner.
	var dir := "user://unit_test_spielstaende"
	var absolute := SaveSlots.absolute_dir(dir)
	DirAccess.make_dir_recursive_absolute(absolute)
	for n in [1, 2, 3]:
		var file := FileAccess.open(SaveSlots.file_path(dir, "autosave_%d" % n), FileAccess.WRITE)
		file.store_string(str(n))
		file.close()
	SaveSlots.rotate_autosaves(dir, 3)
	_true(not FileAccess.file_exists(SaveSlots.autosave_path(dir)), "Rotation: autosave_1 frei")
	_equal(
		FileAccess.get_file_as_string(SaveSlots.file_path(dir, "autosave_2")), "1", "Rotation 1 → 2"
	)
	_equal(
		FileAccess.get_file_as_string(SaveSlots.file_path(dir, "autosave_3")), "2", "Rotation 2 → 3"
	)
	var listed := SaveSlots.list(null, dir)
	_equal(listed.size(), 2, "Liste der Spielstände")
	var names := PackedStringArray()
	for entry in listed:
		names.append(str(entry["name"]))
	names.sort()
	_equal(names, PackedStringArray(["autosave_2", "autosave_3"]), "Namen in der Liste")
	var access := DirAccess.open(absolute)
	for file in access.get_files():
		access.remove(file)
	DirAccess.remove_absolute(absolute)

	# Tasten (project.godot): F10 Spielmenü, F5 Schnellspeichern, F9 Schnellladen.
	for pair: Array in [[&"spielmenue", KEY_F10], [&"schnellspeichern", KEY_F5], [&"schnellladen", KEY_F9]]:
		var events := InputMap.action_get_events(pair[0]) if InputMap.has_action(pair[0]) else []
		var key := events[0] as InputEventKey if not events.is_empty() else null
		_true(key != null and key.physical_keycode == pair[1], "Taste für %s" % pair[0])


func _equal(actual: Variant, expected: Variant, what: String) -> void:
	var message := "%s (erwartet „%s“, erhalten „%s“)" % [what, expected, actual]
	_true(actual == expected, message)


func _true(condition: bool, what: String) -> bool:
	_count += 1
	if not condition:
		_failures.append(what)
		printerr("  FEHLER  ", what)
	return condition


## Wahlabend (seit M8): Meldereihenfolge, Hochrechnung, Halbrund, Telegramme, Tagesnummern.
func _test_election_tally() -> void:
	# Zwei Staaten: Staat 0 mit zwei Wahlkreisen (je 1 Sitz), Staat 1 mit einem Wahlkreis (2 Sitze)
	# und einem Boykott. Zwei Strömungen.
	var election := {
		"districts": [
			{"district_index": 10, "name": "A", "state_index": 0, "state_name": "Nord", "seats": 1,
				"winner": 0, "winner_share": 0.6, "boycott": false, "report_rank": 2},
			{"district_index": 11, "name": "B", "state_index": 0, "state_name": "Nord", "seats": 1,
				"winner": 1, "winner_share": 0.55, "boycott": false, "report_rank": 0, "runoff": true},
			{"district_index": 12, "name": "C", "state_index": 1, "state_name": "Süd", "seats": 2,
				"winner": 1, "winner_share": 0.7, "boycott": false, "report_rank": 1},
			{"district_index": 13, "name": "D", "state_index": 1, "state_name": "Süd", "seats": 1,
				"winner": -1, "boycott": true, "report_rank": 3},
		],
		"deputy_district": PackedInt32Array([10, 11, 12, 12]),
		"deputy_current": PackedInt32Array([0, 1, 1, 1]),
		"deputy_position": PackedFloat32Array([40.0, -30.0, -50.0, -20.0]),
		"deputy_faction": PackedInt32Array([1, 0, -1, 0]),
	}
	_equal(ElectionTally.report_order(election), PackedInt32Array([1, 2, 0, 3]), "Meldereihenfolge")
	var none := ElectionTally.tally(election, 0, false, 2)
	_equal(none["counts"], PackedInt32Array([0, 0]), "vor der ersten Meldung keine Sitze")
	_equal(none["open_seats"], 5, "alle Sitze offen")
	# Nach der ersten Meldung (B in Nord, Strömung 1): Nord hochgerechnet nach B, Süd nach allen.
	var first := ElectionTally.tally(election, 1, false, 2)
	_equal(first["counts"], PackedInt32Array([0, 1]), "erste Meldung")
	_true(is_equal_approx(first["projection"][1], 5.0), "Hochrechnung: alle offenen Sitze an Strömung 1")
	var all := ElectionTally.tally(election, 4, false, 2)
	_equal(all["counts"], PackedInt32Array([1, 3]), "Endergebnis je Strömung")
	_true(
		is_equal_approx(all["projection"][0], 1.0) and is_equal_approx(all["projection"][1], 3.0),
		"am Ende Hochrechnung = Ergebnis"
	)
	_true(is_zero_approx(all["band"][1]), "am Ende kein Fehlerband")
	_equal(all["empty"], 1, "ein Sitz leer (Boykott)")
	_equal(all["seated"], 4, "vier Abgeordnete")
	var factions := ElectionTally.tally(election, 4, true, 3)
	_equal(factions["counts"], PackedInt32Array([2, 1, 1]), "je Fraktion, zuletzt fraktionslos")
	_equal(ElectionTally.seat_order(election), PackedInt32Array([2, 1, 3, 0]), "Sitzordnung links nach rechts")
	var layout := ElectionTally.hemicycle(587, Vector2(200, 180), 170.0)
	var points: PackedVector2Array = layout["points"]
	_equal(points.size(), 587, "Halbrund mit allen Sitzen")
	_true(points[0].x < points[points.size() - 1].x, "Halbrund von links nach rechts")
	_true(float(layout["dot_size"]) > 1.0, "Sitze sichtbar groß")
	var currents: Array[Dictionary] = [{"name": "Rechte"}, {"name": "Linke"}]
	_equal(
		ElectionTally.telegram(election["districts"][1], currents),
		"B (Nord): Linke 55" + NBSP + "% nach Stichwahl",
		"Telegramm mit Stichwahl"
	)
	_equal(
		ElectionTally.telegram(election["districts"][3], currents),
		"D (Süd): Boykott, kein Abgeordneter",
		"Telegramm Boykott"
	)
	_equal(
		ElectionTally.day_number({"year": 1848, "month": 5, "day": 1})
		- ElectionTally.day_number({"year": 1848, "month": 3, "day": 1}),
		61,
		"Tage vom 1. März bis 1. Mai 1848"
	)
	_equal(
		ElectionTally.day_number({"year": 1849, "month": 3, "day": 1})
		- ElectionTally.day_number({"year": 1849, "month": 2, "day": 28}),
		1,
		"1849 ohne 29. Februar"
	)


func _test_party_panel() -> void:
	var panel := PartyPanel.new()
	panel._ready() # baut den Inhalt; in _initialize ist der Baum noch nicht bereit
	panel.show_party({
		"name": "Liberale", "funds": 1234.0, "donations": 100.0, "reserved": 400.0, "open": true,
		"parliament_name": "Nationalversammlung", "held": false, "election_date_text": "1. Mai 1848",
		"campaign_districts": 2,
	})
	var before := panel.facts_text()
	_true(
		before.contains("1.234") and before.contains("1. Mai 1848") and before.contains("in 2 Wahlkreisen")
		and before.contains("400") and not before.contains("Sitze"),
		"Parteipanel vor der Wahl: Mittel, verplant, Wahltag, Wahlkampf (%s)" % before
	)
	panel.show_party({
		"name": "Liberale", "open": false, "held": true, "last_parliament_name": "Nationalversammlung",
		"seats": 3, "seated": 10, "factions_formed": true, "faction_name": "Casino", "faction_own": 2,
		"faction_seats": 4,
		"deputies": [{"district_name": "Mannheim", "state_name": "Baden", "faction_name": "Casino"}],
	})
	var after := panel.facts_text()
	_true(
		after.contains("3 von 10") and after.contains("Casino (2 eigene von 4)")
		and not after.contains("Wahlkampf"),
		"Parteipanel nach der Wahl: Sitze und Fraktion (%s)" % after
	)
	# Nächste Wahl und letzte Wahl zugleich (nach der Nationalversammlung, vor Preußen 1849).
	panel.show_party({
		"name": "Liberale", "open": true, "parliament_name": "Preußische Zweite Kammer",
		"election_date_text": "17. Juli 1849", "held": true, "last_parliament_name": "Nationalversammlung",
		"seats": 3, "seated": 10,
	})
	var both := panel.facts_text()
	_true(
		both.contains("Preußische Zweite Kammer") and both.contains("3 von 10"),
		"Parteipanel mit nächster und letzter Wahl (%s)" % both
	)
	panel.free()


func _test_event_window() -> void:
	var window := EventWindow.new()
	window._ready() # baut den Inhalt; in _initialize ist der Baum noch nicht bereit
	_true(not window.is_open(), "Ereignisfenster zu Beginn geschlossen")
	var effects := PackedStringArray(["Presse: Pressefreiheit", "Unruhe \u221220"])
	var events: Array[Dictionary] = [
		{
			"event_index": 7, "title": "Märzforderungen", "text": "Volksversammlungen fordern …",
			"state_name": "Baden", "date_text": "", "kind_text": "Ereignis", "turning_point": true,
			"source": "Siemann 1985", "options": [
				{"index": 0, "text": "Gewähren", "historical": true, "available": true, "effects": effects},
				{"index": 1, "text": "Militär", "historical": false, "available": false, "effects": PackedStringArray()},
			],
		},
		{"event_index": 9, "title": "Zweites", "options": []},
	]
	events[0]["vignette"] = "revolution"
	window.show_events(events)
	_true(window.custom_minimum_size.x == EventWindow.TURNING_POINT_WIDTH, "Wendepunkt auf dem großen Bildschirm (seit M13a5d)")
	var vignette := window.find_child("Vignette", true, false) as TextureRect
	_true(vignette != null and vignette.visible and vignette.texture == EpochTheme.vignette("revolution"), "Vignette der Art (seit M13b1)")
	var text := window.window_text()
	_true(window.is_open() and text.contains("WENDEPUNKT") and text.contains("1 von 2"), "Wendepunkt und Zähler (%s)" % text)
	_true(text.contains("◆ Gewähren") and text.contains("[Militär gesperrt]"), "Kompassmarke und gesperrte Option")
	_true(text.contains("Presse: Pressefreiheit · Unruhe \u221220") and text.contains("ohne unmittelbare Folgen"), "Folgentexte")
	_true(text.contains("Quelle: Siemann 1985"), "Quelle")
	window.set_compass(false)
	_true(not window.window_text().contains("◆"), "Kompass abschaltbar")
	var chosen: Array[int] = [-1, -1]
	window.option_chosen.connect(func(e: int, o: int) -> void:
		chosen[0] = e
		chosen[1] = o
	)
	window.option_button(0).pressed.emit()
	_true(chosen[0] == 7 and chosen[1] == 0, "Wahl meldet Ereignis und Option")
	var none: Array[Dictionary] = []
	window.show_events(none)
	_true(not window.is_open(), "ohne offene Ereignisse geschlossen")
	window.free()


## Chronik als HTML (seit M13a4): Maskierung, Jahre, Wendepunkte, Dateiname; Tooltip des Urteils.
func _test_chronicle_export() -> void:
	_true(ChronicleExport.escape("a<b & \"c\">") == "a&lt;b &amp; &quot;c&quot;&gt;", "HTML maskiert")
	_true(ChronicleExport.year_of("1. März 1848") == 1848 and ChronicleExport.year_of("01.03.1849") == 1849, "Jahr aus dem Datum")
	_true(ChronicleExport.year_of("ohne") == 0, "ohne Jahr 0")
	var sections: Array[Dictionary] = [{"heading": "Ausgang", "lines": PackedStringArray(["Die deutsche Frage: Reich"])}]
	var entries: Array[Dictionary] = [
		{"date_text": "1. März 1848", "text": "Revolution <Paris>", "turning_point": false},
		{"date_text": "18. März 1848", "text": "Barrikaden", "turning_point": true},
		{"date_text": "1. Mai 1849", "text": "Wahl", "turning_point": false},
	]
	var html := ChronicleExport.build_html("Chronik – Test", PackedStringArray(["Gespielt als Preußen"]), sections, entries)
	_true(html.begins_with("<!DOCTYPE html>") and html.contains("<title>Chronik – Test</title>"), "Kopf der Seite")
	_true(html.contains("<h3>1848</h3>") and html.contains("<h3>1849</h3>"), "Jahre als Überschriften")
	_true(html.contains("Revolution &lt;Paris&gt;") and not html.contains("<Paris>"), "Einträge maskiert")
	_true(html.contains("<li class=\"wende\"><span class=\"datum\">18. März 1848</span> Barrikaden</li>"), "Wendepunkt hervorgehoben")
	_true(html.contains("<h2>Ausgang</h2>") and html.contains("<li>Die deutsche Frage: Reich</li>"), "Abschnitt vor der Chronik")
	_true(html.count("<ul>") == html.count("</ul>"), "Listen geschlossen")
	_true(
		ChronicleExport.file_name("voelkerfruehling", {"year": 1871, "month": 7, "day": 2})
		== "chronik_voelkerfruehling_1871-07-02.html", "Dateiname"
	)
	_true(ChronicleExport.file_name("a/b", {}).begins_with("chronik_a_b_"), "Dateiname ohne Pfadzeichen")
	var tip := VerdictWindow._factor_text({
		"base": 30.0,
		"factors": [
			{"name": "Bund", "value": 25.0, "active": true},
			{"name": "Zollverein", "value": 0.0, "active": false},
			{"name": "Unruhe", "value": -6.2, "active": true},
		],
	})
	_true(tip == "Grundwert +30\nBund +25\nUnruhe -6", "Faktoren im Tooltip (%s)" % tip)


class FakeWindow:
	extends Control
	var opened := false

	func is_open() -> bool:
		return opened

	func close() -> void:
		opened = false

	func fit_height(max_height: float) -> void:
		custom_minimum_size.y = minf(300.0, max_height)


## Fensterverwaltung (seit M13a5a): höchstens ein Fenster offen, Escape schließt das jüngste, Platz.
func _test_window_manager() -> void:
	var manager := WindowManager.new()
	var a := FakeWindow.new()
	var b := FakeWindow.new()
	manager.add(a)
	manager.add(b)
	manager.add(a)
	_true(manager.windows().size() == 2, "Fenster nur einmal angemeldet")
	_true(manager.open_window() == null and not manager.close_top(), "ohne offenes Fenster nichts zu schließen")
	manager.opening(a)
	a.opened = true
	manager.opening(b)
	b.opened = true
	_true(not a.opened and b.opened and manager.open_window() == b, "das neue Fenster schließt das alte")
	manager.place(100.0, 80.0, 280.0)
	_true(b.position == Vector2(100, 80) and b.custom_minimum_size.y == 200.0, "Platz und Höhe (%s)" % str(b.position))
	_true(manager.close_top() and not b.opened and manager.open_window() == null, "Escape schließt das jüngste")
	a.opened = true
	manager.close_all()
	_true(not a.opened and not b.opened, "alle geschlossen")
	a.free()
	b.free()


## Ticker und Glossar (seit M13a5c): Wendepunkte zuerst, Begrenzung, Verweise.
func _test_ticker_and_glossary() -> void:
	var ticker := TickerBar.new()
	var entries: Array[Dictionary] = []
	for i in 20:
		entries.append({"date_text": "1848", "text": "Eintrag %d" % i, "turning_point": i == 15})
	ticker.push(entries)
	_true(ticker.shown_text().contains("WENDEPUNKT · Eintrag 15"), "Ticker zeigt den Wendepunkt zuerst (%s)" % ticker.shown_text())
	_true(ticker.get("_queue").size() <= TickerBar.QUEUE_LIMIT, "Ticker begrenzt die Warteschlange")
	_true(str((ticker.get("_queue") as Array)[0].get("text", "")) == "Eintrag 0", "danach die übrigen in Folge")
	ticker.clear()
	_true(ticker.shown_text() == "–", "Ticker geleert")
	ticker.free()
	_true(Glossary.link("akzise", "Akzise") == "[url=begriff:akzise]Akzise[/url]", "Verweis im Tooltip")
	_true(Glossary.text("akzise").begins_with("Akzise:") and Glossary.text("gibt_es_nicht").is_empty(), "Glossar")
	_true(AreaWindow.sorted_shares({"a": 10.0, "b": 50.0, "c": 40.0}, 2) == [["b", 50.0], ["c", 40.0], ["übrige", 10.0]], "Anteile sortiert mit Rest")


## Einstellungen, Pausenregeln, Farbsehschwäche und Lexikon (seit M13a5d).
func _test_settings_and_lexicon() -> void:
	var path := "user://unit_test_einstellungen.cfg"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	var settings := UserSettings.new(path)
	_true(settings.get_value("spiel/autosave") == "jaehrlich" and settings.pause_on("pause_krieg"), "Vorgaben der Einstellungen")
	settings.set_value("spiel/autosave", "halbjaehrlich")
	settings.set_value("spiel/pause_krieg", false)
	settings.set_value("gibt/es_nicht", true)
	var journal_key := settings.key_of("journal")
	settings.set_key("journal", KEY_F7)
	_true(settings.save_file(), "Einstellungen gespeichert")
	var again := UserSettings.new(path)
	_true(again.load_file() and again.get_value("spiel/autosave") == "halbjaehrlich" and not again.pause_on("pause_krieg"), "Einstellungen geladen")
	_true(again.key_of("journal") == KEY_F7 and again.action_with_key(KEY_F7) == "journal", "Taste neu belegt")
	_true(again.action_with_key(journal_key).is_empty(), "alte Taste frei")
	again.reset_group("tasten")
	_true(again.key_of("journal") == journal_key, "Tasten zurückgesetzt")
	again.reset_group("spiel")
	_true(again.pause_on("pause_krieg"), "Spiel zurückgesetzt")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	# Pausenregeln aus der Chronik.
	var rules := UserSettings.new("user://gibt_es_nicht.cfg")
	var war: Array[Dictionary] = [{"text": "Österreich erklärt Preußen den Krieg.", "state_index": 3}]
	_true(PauseRules.reason(war, "Preußen", 1, rules).contains("den Krieg"), "Pause bei Kriegserklärung")
	rules.set_value("spiel/pause_krieg", false)
	_true(PauseRules.reason(war, "Preußen", 1, rules).is_empty(), "Pausenregel abschaltbar")
	var office: Array[Dictionary] = [{"text": "Preußen: Otto von Bismarck wird Ministerpräsident", "state_index": 1}]
	_true(PauseRules.reason(office, "Preußen", 1, rules).is_empty(), "Amtswechsel ohne Pause (Vorgabe aus)")
	rules.set_value("spiel/pause_amt", true)
	_true(not PauseRules.reason(office, "Preußen", 1, rules).is_empty(), "Amtswechsel mit Pause")
	var railway: Array[Dictionary] = [{"text": "Preußen: Eine Bahn wird eröffnet – Die Eröffnung feiern", "state_index": 1}]
	_true(PauseRules.reason(railway, "Preußen", 1, rules).is_empty(), "Ereignis mit „wird“ ist kein Amtswechsel")
	var federation: Array[Dictionary] = [{"text": "Gründung: Norddeutscher Bund mit 22 Staaten, Präsidium Preußen.", "state_index": -1}]
	_true(not PauseRules.reason(federation, "Preußen", 1, rules).is_empty(), "Pause bei Bundesgründung")
	# Farbsehschwäche: Grün wird Blau, Rot bleibt.
	Accessibility.color_blind = true
	var green := Accessibility.adjust(Color(0.2, 0.6, 0.25))
	var red := Accessibility.adjust(Color(0.7, 0.2, 0.2))
	_true(green.b > green.g and is_equal_approx(red.r, 0.7), "Farbsehschwäche: Grün wird Blau (%s)" % green)
	Accessibility.color_blind = false
	_true(Accessibility.adjust(Color(0.2, 0.6, 0.25)) == Color(0.2, 0.6, 0.25), "ohne Einstellung unverändert")
	# Lexikon der Daten: jeder Artikel hat einen Titel, jeder Verweis ein Ziel.
	var lexicon := Lexicon.load_dir(UhrwerkPaths.data_dir().path_join("lexikon"))
	_true(lexicon.articles.size() >= 20 and lexicon.problems.is_empty(), "Lexikon: %d Artikel, Probleme %s" % [lexicon.articles.size(), ", ".join(lexicon.problems)])
	_true(lexicon.bbcode("zollverein").contains("[url=begriff:zoll]Zoll[/url]"), "Lexikon: Verweis als BBCode")
	_true(Lexicon.to_bbcode("**fett** und *kursiv* [Klammer]") == "[b]fett[/b] und [i]kursiv[/i] [lb]Klammer[rb]", "Markdown nach BBCode")
	_true(lexicon.ids("Wahlrecht").has("dreiklassenwahlrecht"), "Lexikon: Suche")
	for key: String in Glossary.TERMS:
		_true(lexicon.has(key), "Glossarbegriff %s hat einen Lexikonartikel" % key)



## Epochenrahmen und Zeitung (seit M13b1, vertical_slice.md 5.1 und 5.2): Schriften, Ornamente, Vignetten,
## Theme, Zeitungsfenster, Startoption --zeitung und Pausenregel.
func _test_epoch_and_newspaper() -> void:
	for role in ["text", "text_kraeftig", "kursiv", "titel", "kopf", "fraktur", "tabelle"]:
		var f := EpochTheme.font(role)
		_true(f != null and f.get_height(15) > 15.0 and f.get_height(15) < 30.0, "Schrift %s geladen (Höhe %s)" % [role, f.get_height(15) if f != null else 0.0])
	_true(EpochTheme.font("fraktur").get_string_size("Zeitung", HORIZONTAL_ALIGNMENT_LEFT, -1, 20).x > 0.0, "Fraktur zeichnet Text")
	for name in ["ecke", "trenner"]:
		_true(EpochTheme.texture(name) != null, "Ornament %s" % name)
	for kind in EpochTheme.VIGNETTES:
		var t := EpochTheme.vignette(kind)
		_true(t != null and t.get_width() > 0, "Vignette %s" % kind)
	_true(EpochTheme.vignette("") == null and EpochTheme.vignette("landschaft") == null, "ohne oder unbekannte Vignette: keine")
	var theme := Theme.new()
	EpochTheme.apply(theme)
	_true(theme.default_font == EpochTheme.font("text"), "Grundschrift im Theme")
	_true(theme.get_font(&"font", &"MastheadLabel") == EpochTheme.font("fraktur") and theme.get_font_size(&"font_size", &"MastheadLabel") == 44, "Zeitungskopf in Fraktur, 44 px")
	_true(theme.get_type_variation_base(&"HeadlineLabel") == &"Label", "Variante Schlagzeile")
	_true(theme.get_font(&"bold_font", &"RichTextLabel") == EpochTheme.font("text_kraeftig"), "fette Schrift im RichTextLabel")

	var window := NewspaperWindow.new()
	window._ready()
	_true(not window.is_open() and not window.show_page({}), "Zeitung ohne Seite bleibt zu")
	var page := {
		"paper": "Der Bote", "leaning": "liberal", "issue": 85, "date_text": "26. März 1849",
		"headline": "Barrikaden", "subline": "Bürger und Truppen", "text": "In den Straßen.", "report": "Preußen: Barrikadenkämpfe",
		"miscellany": ["Erste Meldung", "Zweite Meldung"],
	}
	_true(window.show_page(page) and window.is_open(), "Zeitung zeigt die Seite")
	var text := window.page_text()
	_true(text.contains("Der Bote") and text.contains("Nr. 85  ·  26. März 1849  ·  Liberales Blatt"), "Kopf und Ausgabe (%s)" % text.left(120))
	_true(text.contains("Drahtbericht: Preußen: Barrikadenkämpfe") and text.contains("Zweite Meldung"), "Drahtbericht und Vermischtes")
	var closed: Array[bool] = [false]
	window.close_requested.connect(func() -> void: closed[0] = true)
	var click := InputEventMouseButton.new()
	click.button_index = MOUSE_BUTTON_LEFT
	click.pressed = true
	window._gui_input(click)
	_true(closed[0], "ein Klick legt die Zeitung beiseite")
	page["miscellany"] = []
	window.show_page(page)
	_true(window.page_text().contains("nichts Neues"), "ohne Vermischtes ein Satz")
	window.free()
	_true(LaunchOptions.parse(PackedStringArray(["--zeitung"])).show_newspaper, "Startoption --zeitung")
	_true(UserSettings.DEFAULTS.get("spiel/pause_zeitung", false) == true, "Pausenregel Zeitung an")


## Musik und Klänge (seit M13b2, vertical_slice.md 5.3): Busse, Liste der Stücke, Stimmungen, Wechsel ohne
## Wiederholung, Klänge. Ohne gerenderte Dateien (tools/klang/klang.py) prüft der Test nur die Stille.
func _test_audio_director() -> void:
	AudioDirector.ensure_buses()
	_true(AudioServer.get_bus_index(AudioDirector.MUSIC_BUS) > 0 and AudioServer.get_bus_index(AudioDirector.SOUND_BUS) > 0, "Busse Musik und Klaenge")
	var settings := UserSettings.new("user://unit_test_einstellungen_ton.cfg")
	settings.set_value("ton/musik", 40)
	settings.set_value("ton/musik_aus", true)
	settings.apply(null)
	var music := AudioServer.get_bus_index(AudioDirector.MUSIC_BUS)
	_true(AudioServer.is_bus_mute(music) and absf(AudioServer.get_bus_volume_db(music) - linear_to_db(0.4)) < 0.01, "Lautstärke und „Musik aus“ auf dem Bus")
	settings.set_value("ton/musik_aus", false)
	settings.apply(null)
	_true(not AudioServer.is_bus_mute(music), "Musik wieder an")
	var director := AudioDirector.new()
	director._ready() # außerhalb des Baums: Spieler und Liste, ohne Wiedergabe
	if not director.has_music():
		print("  (ohne gerenderte Musik: python3 tools/klang/klang.py)")
		director.set_mood("krieg")
		_true(director.current_piece().is_empty() and not director.play_sound("glocke"), "ohne Dateien still")
		director.free()
		return
	for mood in AudioDirector.MOODS:
		_true(not director.pieces_for(mood).is_empty(), "Musik für die Stimmung %s" % mood)
	var peace := director.pieces_for("frieden")
	var piece: Dictionary = director.pieces.get(peace[0], {})
	_true(not str(piece.get("titel", "")).is_empty() and float(piece.get("dauer", 0.0)) > 30.0, "stuecke.cfg mit Titel und Dauer (%s)" % str(piece))
	director.set_mood("frieden")
	var first := director.current_piece()
	_true(peace.has(first) and director.mood() == "frieden", "Frieden spielt ein Stück des Friedens (%s)" % first)
	director.set_mood("frieden")
	_true(director.current_piece() == first, "dieselbe Stimmung wechselt das Stück nicht")
	director.set_mood("krieg")
	_true(director.pieces_for("krieg").has(director.current_piece()), "Krieg blendet zu einem Stück des Krieges über")
	director.set_mood("frieden")
	_true(peace.size() < 2 or director.current_piece() != first, "ein neues Stück, nicht das zuletzt gespielte")
	director.set_mood("unbekannt")
	_true(director.mood() == "frieden", "unbekannte Stimmung ändert nichts")
	for sound in AudioDirector.SOUNDS:
		_true(director.play_sound(sound), "Klang %s" % sound)
	_true(int(director.sounds_played.get("glocke", 0)) == 1 and not director.play_sound("pfeife"), "Zähler der Klänge, unbekannter Klang")
	director.free()


## Onboarding (seit M13b3, vertical_slice.md 5.4): die Geschichtsstunde als Daten, ihr Panel, Hinweise beim
## ersten Öffnen und „Was ist passiert?“.
func _test_onboarding() -> void:
	var lesson := load("res://tutorial/maerz_1848.tres") as TutorialLesson
	_true(lesson != null and lesson.problems().is_empty(), "Geschichtsstunde ohne Fehler (%s)" % (", ".join(lesson.problems()) if lesson != null else "fehlt"))
	if lesson == null:
		return
	_true(lesson.state == "PRU" and lesson.scenario == "voelkerfruehling" and lesson.steps.size() >= 12, "März 1848 in Berlin, %d Schritte" % lesson.steps.size())
	var checks := {}
	var lexicon := Lexicon.load_dir(UhrwerkPaths.data_dir().path_join("lexikon"))
	for step in lesson.steps:
		checks[step.check] = true
		if not step.lexicon.is_empty():
			_true(lexicon.articles.has(step.lexicon), "Lexikonartikel „%s“ zu Schritt %s" % [step.lexicon, step.id])
	for needed in ["kartenmodus", "haushalt", "steuern", "ereignis", "kabinett", "gesandter", "zollunion", "wahl", "journal", "speichern", "tempo"]:
		_true(checks.has(needed), "Schritt mit Prüfung %s" % needed)
	var broken := TutorialLesson.new()
	var bad := TutorialStep.new()
	bad.id = "x"
	bad.title = "X"
	bad.text = "x"
	bad.check = "fliegen"
	bad.target = "mond"
	broken.steps = [bad, bad]
	var problems := "\n".join(broken.problems())
	_true(problems.contains("unbekannte Prüfung „fliegen“") and problems.contains("unbekanntes Ziel „mond“") and problems.contains("doppelt") and problems.contains("Aufgabe fehlt"), "Fehler der Daten erkannt")

	var panel := TutorialPanel.new()
	panel._ready()
	var entered: Array[String] = []
	var finished: Array[bool] = []
	panel.step_entered.connect(func(step: TutorialStep) -> void: entered.append(step.id))
	panel.closed.connect(func(completed: bool) -> void: finished.append(completed))
	panel.start(lesson)
	_true(panel.is_active() and entered == ["karte"] and panel.panel_text().contains("Schritt 1 von %d" % lesson.steps.size()), "Panel beginnt mit Schritt 1")
	_true(panel.button("Next").disabled and not panel.advance(), "Weiter erst nach der Aufgabe")
	panel.mark_done()
	_true(not panel.button("Next").disabled and panel.panel_text().contains("✓"), "erledigt mit Haken")
	panel.advance()
	_true(panel.step_index() == 1 and not panel.is_done(), "nächster Schritt offen")
	while panel.is_active():
		panel.mark_done()
		panel.advance()
	_true(finished == [true] and entered.size() == lesson.steps.size(), "alle Schritte bis zum Ende")
	panel.start(lesson)
	panel.button("End").pressed.emit()
	_true(finished == [true, false] and not panel.is_active(), "Beenden bricht ab")
	panel.free()

	var hint := HintCard.new()
	hint.seen_path = "user://unit_test_hinweise.cfg"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(hint.seen_path))
	hint._ready()
	for key in ["CabinetWindow", "LawsWindow", "GroupsWindow", "ParliamentWindow", "SocietyWindow", "EconomyWindow",
			"ChronicleWindow", "DiplomacyWindow", "WarWindow", "JournalWindow", "YearbookWindow", "LexiconWindow",
			"EventWindow", "NewspaperWindow", "ElectionNight", "VerdictWindow", "RecapWindow", "Haushalt", "Zollunion"]:
		_true(hint.has_hint(key), "Hinweis für %s" % key)
	_true(hint.offer("DiplomacyWindow") and hint.visible and hint.current_key() == "DiplomacyWindow", "Hinweis beim ersten Öffnen")
	hint.dismiss()
	_true(not hint.offer("DiplomacyWindow") and hint.was_seen("DiplomacyWindow"), "kein zweites Mal")
	var again := HintCard.new()
	again.seen_path = hint.seen_path
	again._ready()
	_true(again.was_seen("DiplomacyWindow") and not again.offer("DiplomacyWindow"), "gesehen bleibt gesehen (Datei)")
	again.reset_seen()
	_true(again.offer("DiplomacyWindow"), "zurückgesetzt zeigt er wieder")
	_true(not hint.offer("Gibtsnicht"), "ohne Text kein Hinweis")
	hint.free()
	again.free()
	DirAccess.remove_absolute(ProjectSettings.globalize_path("user://unit_test_hinweise.cfg"))
	_true(RecapWindow._day("05.04.1849") - RecapWindow._day("05.04.1848") == 365, "Tagesnummer aus dem Datum")
	var o := LaunchOptions.parse(PackedStringArray(["--geschichtsstunde=4"]))
	_true(o.lesson_step == 4 and LaunchOptions.parse(PackedStringArray(["--geschichtsstunde"])).lesson_step == 1, "Startoption --geschichtsstunde")
	_true(not LaunchOptions.parse(PackedStringArray(["--geschichtsstunde=0"])).problems.is_empty(), "Schritt 0 abgelehnt")
	_true(UserSettings.DEFAULTS.get("spiel/rueckblick", false) == true, "Einstellung Rückblick an")


## Sprache (seit M13b4, vertical_slice.md 6): Übersetzungen, Pseudo-Locale, Gründe der KI als Schlüssel mit Werten.
func _test_language() -> void:
	var loaded := TranslationServer.get_loaded_locales()
	_true(loaded.has("en") and loaded.has("de"), "Übersetzungen de und en geladen (%s)" % str(loaded))
	Translation_.use("en")
	_true(TranslationServer.translate("Neues Spiel") == "New game" and Format.locale == "en", "Englisch: Neues Spiel → New game")
	_true(TranslationServer.translate("Spielbare Staaten: {staaten}").format({"staaten": "BAV"}) == "Playable states: BAV", "benannte Platzhalter")
	_true(TranslationServer.translate("Eine Zeile, die es nicht gibt") == "Eine Zeile, die es nicht gibt", "ohne Übersetzung bleibt Deutsch")
	_true(AiReasons.text({"key": "staerke", "state_name": "Austria", "value": -12}) == "Strength against Austria −12", "Grund der KI auf Englisch")
	Translation_.use("pseudo")
	var pseudo := TranslationServer.translate("Neues Spiel")
	_true(Translation_.is_pseudo() and pseudo.length() >= int("Neues Spiel".length() * 1.4) and pseudo.begins_with("["), "Pseudo-Locale: länger, in Klammern (%s)" % pseudo)
	Translation_.use("de")
	_true(not Translation_.is_pseudo() and TranslationServer.translate("Neues Spiel") == "Neues Spiel" and Format.locale == "de", "zurück auf Deutsch")
	_true(AiReasons.text({"key": "staerke", "state_name": "Österreich", "value": 12}) == "Stärke gegen Österreich +12", "Grund der KI wie im Kern")
	_true(AiReasons.text({"key": "agenda", "state_name": "", "value": 30}) == "Agenda +30", "Agenda ohne Staat")
	_true(AiReasons.text({"key": "neu", "state_name": "Bayern", "value": 3}) == "Bayern +3", "unbekannter Schlüssel")
	_true(AiReasons.of({"reason_keys": [{"key": "defizit", "value": 5}, {"key": "nachbar", "state_name": "Sachsen", "value": 2}]}) == "Defizit +5, Nachbar Sachsen +2", "mehrere Gründe")
	_true(AiReasons.of({"reasons_text": "fertig"}) == "fertig", "ohne Schlüssel der Text des Kerns")
	var o := LaunchOptions.parse(PackedStringArray(["--sprache=pseudo"]))
	_true(o.language == "pseudo" and not LaunchOptions.parse(PackedStringArray(["--sprache=fr"])).problems.is_empty(), "Startoption --sprache")
