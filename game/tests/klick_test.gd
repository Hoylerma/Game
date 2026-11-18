extends SceneTree
## Klicktest der Kartenansicht (Abnahme M3: Klickerkennung für alle Orte).
##
## 1. Jeder Ort der definition.csv: Sein Beschriftungspunkt wird bei drei Zoomstufen
##    (fern = Übersicht, mittel, nah; MapDetail.LEVEL_SIZES) mit der Zeichen-
##    transformation der Karte (Canvas-Transformation, mit der Godot das Kartenbild
##    zeichnet) in Bildschirmkoordinaten umgerechnet und über die Auswahlfunktion der
##    Karte (MapView.location_at_screen, dieselbe wie bei Klick und Hervorhebung)
##    aufgelöst. Die gefundene Ortskennung muss die des Ortes sein. Die Kamera fährt
##    die Karte dazu in Kacheln ab, damit die Punkte an verschiedenen Bildschirmstellen
##    liegen.
## 2. Stichproben über echte Eingabeereignisse: Linksklick (Input.parse_input_event)
##    auf den Beschriftungspunkt an einer freien Stelle der Karte (nicht unter Panels);
##    danach muss die Hauptszene genau diesen Ort ausgewählt haben. Ein Klick ins Meer
##    hebt die Auswahl auf.
## 4. Wirtschaft (seit M5): drei Steuerregler, Haushaltsfenster, Abschnitt Wirtschaft im
##    Ortspanel, Kartenmodus „Märkte“.
## 5. Spielstände (seit M6): F10 öffnet das Spielmenü und hält die Zeit an, Speichern unter
##    einem Namen, Escape schließt, F5/F9 schnellspeichern und -laden, Laden aus der Liste,
##    unpassende Dateien ausgegraut, automatischer Spielstand am 1. Januar.
## 3. Namen und Panels (MapLabels.record_drawn, vor 1. und 2.): Nach einem Moduswechsel
##    und nach der ersten Auswahl per Klick liegt ohne Kamerabewegung kein Name unter
##    einem Panel;
##    ein Kreisname bleibt, wenn sein gleichnamiger Hauptort ausgewählt ist; eine
##    ausgewählte Hauptstadt behält den Hauptstadt-Stil.
##
## Aufruf aus dem Repository-Hauptordner, nach einem Import (ohne Grafikausgabe möglich):
##   godot --headless --path game --script res://tests/klick_test.gd
## Ergebnis: "KLICK OK n/n" (n = Orte mit Fläche; jeder bei allen drei Zoomstufen
## richtig) und Exitcode 0, sonst "KLICK FEHLGESCHLAGEN" mit den ersten Abweichungen.

const MAIN_SCENE := "res://scenes/main.tscn"
## Stichproben mit echten Klicks je Zoomstufe.
const EVENT_SAMPLES_PER_LEVEL := 40
const MAX_REPORTED := 20
## Seit M14a1: Anteil des Bildes, den Leisten und Panels ohne offenes Fenster höchstens verdecken
## (europa_und_oberflaeche.md 6; gemessen 13 %, vor M14 nach Augenmaß rund die Hälfte).
const FREE_MAP_LIMIT := 0.20

var _main: Node = null
var _view: MapView = null
var _map: MapData = null
var _sprite: Sprite2D = null
var _failures := PackedStringArray()
var _failure_count := 0
## Zeitungen, die der Test beiseitegelegt hat (seit M13b1).
var _newspapers_seen := 0


func _initialize() -> void:
	_run()


func _run() -> void:
	print("EINIGKEIT – Klicktest der Kartenansicht")
	_main = (load(MAIN_SCENE) as PackedScene).instantiate()
	root.add_child(_main)
	if not bool(_main.get("view_is_ready")):
		await _main.view_ready
	_quiet_hints()
	_view = _main.call("map_view")
	var sim: SimulationClient = _main.call("simulation")
	if not _view.has_map() or sim == null:
		printerr("KLICK FEHLGESCHLAGEN: Karte oder Simulation nicht geladen.")
		quit(1)
		return
	_view.hover_enabled = false
	_map = _view.map_data()
	_sprite = _view.get_node("MapSprite")

	# Erwartung je Kartenindex: Simulationsindex der Kennung aus der CSV.
	var expected := PackedInt32Array()
	expected.resize(_map.map_count())
	var testable := PackedInt32Array()
	var without_area := PackedStringArray()
	for map_index in _map.map_count():
		expected[map_index] = sim.get_location_index(_map.map_ids[map_index])
		if _map.map_ids[map_index].is_empty():
			continue
		if _map.label_points[map_index].x < 0.0:
			without_area.append(_map.map_ids[map_index])
		else:
			testable.append(map_index)
	print("Viewport %s, Fenster %s" % [str(root.get_visible_rect().size), str(root.size)])
	print("Orte in definition.csv: %d, davon mit Fläche: %d" % [
		_map.map_count(), testable.size()
	])
	if not without_area.is_empty():
		print("  ohne Fläche (nicht klickbar): ", ", ".join(without_area))

	# Seit M13: Das Spiel beginnt mit dem Startbildschirm über dem Technikprototyp 1648.
	await _check_start_screen_menu()

	# Zuerst, solange die Ortsinfo noch nie angezeigt wurde (erste Auswahl der Sitzung).
	await _check_labels()

	var diameter := _view.location_diameter()
	var zooms := PackedFloat32Array([
		_view.camera.overview_zoom,
		MapDetail.zoom_for(MapDetail.LEVEL_SIZES[1], diameter),
		MapDetail.zoom_for(MapDetail.LEVEL_SIZES[2], diameter),
	])
	var passed := PackedInt32Array()
	passed.resize(_map.map_count())
	for level in zooms.size():
		await _check_level(level, zooms[level], testable, expected, passed)
	var all_levels := 0
	for map_index in testable:
		if passed[map_index] == zooms.size():
			all_levels += 1

	var event_total := 0
	var event_ok := 0
	var skipped := 0
	for level in zooms.size():
		var result: PackedInt32Array = await _check_events(level, zooms[level], testable, expected)
		event_total += result[0]
		event_ok += result[1]
		skipped += result[2]
	print("  Eingabeereignisse: %d/%d Klicks richtig (%d Stichproben unter Panels übersprungen)"
		% [event_ok, event_total, skipped])
	await _check_click_into_sea()
	await _check_economy_ui(sim)
	await _check_save_ui(sim)
	await _check_new_game()
	await _check_start_screen_flow()
	await _check_free_map()
	await _check_areas()
	await _check_verdict()
	await _check_newspaper()
	await _check_audio()
	await _check_onboarding()

	if _failure_count == 0 and all_levels == testable.size() and event_ok == event_total:
		print("KLICK OK %d/%d" % [all_levels, testable.size()])
		quit(0)
	else:
		printerr("KLICK FEHLGESCHLAGEN: %d/%d Orte bei allen Zoomstufen richtig, %d Fehler"
			% [all_levels, testable.size(), _failure_count])
		for failure in _failures:
			printerr("  - ", failure)
		quit(1)


## Fährt die Karte bei einem Zoom in Kacheln ab und löst jeden Beschriftungspunkt auf.
func _check_level(
	level: int,
	zoom: float,
	testable: PackedInt32Array,
	expected: PackedInt32Array,
	passed: PackedInt32Array
) -> void:
	var camera := _view.camera
	var viewport_size := root.get_visible_rect().size
	var screen := Rect2(Vector2.ZERO, viewport_size)
	var rect := _view.map_rect()
	var view_world := viewport_size / zoom
	var tiles := Vector2i(
		maxi(ceili(rect.size.x / (view_world.x * 0.9)), 1),
		maxi(ceili(rect.size.y / (view_world.y * 0.9)), 1)
	)
	var tested := PackedByteArray()
	tested.resize(_map.map_count())
	var correct := 0
	for tile_y in tiles.y:
		for tile_x in tiles.x:
			var center := rect.position + Vector2(
				(tile_x + 0.5) * rect.size.x / tiles.x, (tile_y + 0.5) * rect.size.y / tiles.y
			)
			camera.focus_on(center, zoom)
			await process_frame
			var to_screen := _sprite.get_global_transform_with_canvas()
			var visible_world := camera.visible_world_rect()
			for map_index in testable:
				if tested[map_index] != 0:
					continue
				var point := _map.label_points[map_index]
				if not visible_world.has_point(point):
					continue
				var on_screen := to_screen * point
				if not screen.has_point(on_screen):
					continue
				tested[map_index] = 1
				var found := _view.location_at_screen(on_screen)
				if found == expected[map_index] and found >= 0:
					passed[map_index] += 1
					correct += 1
				else:
					_fail("%s: Ort %s (Kartenindex %d) bei Bildschirm %s ergibt %s" % [
						MapDetail.LEVEL_NAMES[level], _map.map_ids[map_index], map_index,
						str(on_screen.round()), _describe(found)
					])
	var untested := 0
	for map_index in testable:
		if tested[map_index] == 0:
			untested += 1
			_fail("%s: Ort %s lag in keiner Kachel im Bild" % [
				MapDetail.LEVEL_NAMES[level], _map.map_ids[map_index]
			])
	print("  %s (Zoom %.3f, %d Kacheln): %d/%d Orte richtig%s" % [
		MapDetail.LEVEL_NAMES[level], camera.zoom.x, tiles.x * tiles.y, correct, testable.size(),
		", %d nicht erreicht" % untested if untested > 0 else ""
	])


## Echte Linksklicks auf Stichproben. Liefert [Klicks, richtig, übersprungen].
func _check_events(
	level: int, zoom: float, testable: PackedInt32Array, expected: PackedInt32Array
) -> PackedInt32Array:
	var camera := _view.camera
	var viewport_size := root.get_visible_rect().size
	var stride := maxi(testable.size() / EVENT_SAMPLES_PER_LEVEL, 1)
	var random := RandomNumberGenerator.new()
	random.seed = 1648 + level
	var clicks := 0
	var correct := 0
	var skipped := 0
	for sample in range(level * 7 % stride, testable.size(), stride):
		var map_index := testable[sample]
		var point := _map.label_points[map_index]
		# Zielpunkt auf dem Bildschirm, möglichst nicht unter einem Panel.
		var target := viewport_size * 0.5
		for _attempt in 30:
			var candidate := Vector2(
				random.randf_range(0.0, viewport_size.x), random.randf_range(0.0, viewport_size.y)
			)
			if not _covered(candidate):
				target = candidate
				break
		camera.focus_on(point - (target - viewport_size * 0.5) / zoom, zoom)
		await process_frame
		var on_screen := _sprite.get_global_transform_with_canvas() * point
		if _covered(on_screen) or not Rect2(Vector2.ZERO, viewport_size).has_point(on_screen):
			skipped += 1
			continue
		clicks += 1
		await _click(on_screen)
		var selected := int(_main.call("selected_location"))
		if selected == expected[map_index]:
			correct += 1
		else:
			_fail("Klick %s: Ort %s bei Bildschirm %s, ausgewählt wurde %s" % [
				MapDetail.LEVEL_NAMES[level], _map.map_ids[map_index], str(on_screen.round()),
				_describe(selected)
			])
	return PackedInt32Array([clicks, correct, skipped])


## Ein Klick ins Meer hebt die Auswahl auf.
func _check_click_into_sea() -> void:
	var sea := Vector2(-1, -1)
	for y in range(8, _map.size.y, 64):
		for x in range(8, _map.size.x, 64):
			if _map.area_at_pixel(Vector2i(x, y)) == MapData.Area.SEA:
				sea = Vector2(x + 0.5, y + 0.5)
				break
		if sea.x >= 0.0:
			break
	if sea.x < 0.0:
		print("  Klick ins Meer: kein Meer auf der Karte, übersprungen")
		return
	var viewport_size := root.get_visible_rect().size
	_main.call("select_location", 0)
	_view.camera.focus_on(sea, _view.camera.overview_zoom * 4.0)
	await process_frame
	var on_screen := _sprite.get_global_transform_with_canvas() * sea
	if _covered(on_screen):
		print("  Klick ins Meer: Punkt unter einem Panel, übersprungen")
		return
	await _click(on_screen)
	var selected := int(_main.call("selected_location"))
	if selected == -1 and Rect2(Vector2.ZERO, viewport_size).has_point(on_screen):
		print("  Klick ins Meer hebt die Auswahl auf")
	else:
		_fail("Klick ins Meer bei %s ergibt %s" % [str(on_screen.round()), _describe(selected)])


## Namen im Zusammenspiel mit Panels und Auswahl (ohne Kamerabewegung dazwischen).
func _check_labels() -> void:
	var labels: MapLabels = _main.call("map_labels")
	var sim: SimulationClient = _main.call("simulation")
	labels.record_drawn = true
	var diameter := _view.location_diameter()
	var near := MapDetail.zoom_for(MapDetail.LEVEL_SIZES[2], diameter)
	var middle := MapDetail.zoom_for(MapDetail.LEVEL_SIZES[1], diameter)
	var checks := 0

	# Moduswechsel: Die Legende ändert ihre Größe, die Kamera bleibt stehen.
	_main.call("select_location", -1)
	var nuernberg := sim.get_location_index("nuernberg")
	if nuernberg >= 0:
		_main.call("set_map_mode", MapModes.MAP_KREISE)
		_view.camera.focus_on(_view.location_center(nuernberg), near)
		await _frames(3)
		_main.call("set_map_mode", MapModes.MAP_POLITICAL)
		await _frames(3)
		_check_labels_free(labels, "nach Moduswechsel Kreise → Politisch")
		checks += 1

	# Erste Auswahl per Klick: Die Ortsinfo erscheint, die Kamera bleibt stehen.
	# Ziel: der freie Beschriftungspunkt nächst der Bildmitte.
	var viewport_size := root.get_visible_rect().size
	var to_screen := _sprite.get_global_transform_with_canvas()
	var target := -1
	var best := INF
	for map_index in _map.map_count():
		var on_screen := to_screen * _map.label_points[map_index]
		var distance := on_screen.distance_to(viewport_size * 0.5)
		if (
			_map.label_points[map_index].x >= 0.0
			and distance < best
			and Rect2(Vector2.ZERO, viewport_size).grow(-40.0).has_point(on_screen)
			and not _covered(on_screen)
		):
			target = map_index
			best = distance
	if target >= 0:
		await _click(to_screen * _map.label_points[target])
		await _frames(3)
		if int(_main.call("selected_location")) < 0:
			_fail("Namen: Klick auf %s wählt keinen Ort aus" % _map.map_ids[target])
		_check_labels_free(labels, "nach der ersten Auswahl von %s" % _map.map_ids[target])
		checks += 1

	# Mittlere Stufe: Kreisname bleibt, wenn der gleichnamige Hauptort ausgewählt ist.
	var wilhelmshaven := sim.get_location_index("wilhelmshaven")
	if wilhelmshaven >= 0:
		_main.call("select_location", wilhelmshaven)
		_view.camera.focus_on(_view.location_center(wilhelmshaven), middle)
		await _frames(3)
		if not _drawn_with_style(labels, "Wilhelmshaven", MapLabels.Style.KREIS):
			_fail("Namen: Kreisname Wilhelmshaven fehlt, wenn der Ort ausgewählt ist")
		checks += 1

	# Ausgewählte Hauptstadt: Stil CAPITAL wie ohne Auswahl.
	var berlin := sim.get_location_index("berlin_coelln")
	if berlin >= 0:
		_main.call("select_location", berlin)
		_view.camera.focus_on(_view.location_center(berlin), near)
		await _frames(3)
		if not _drawn_with_style(labels, "Berlin-Cölln", MapLabels.Style.CAPITAL):
			_fail("Namen: ausgewählte Hauptstadt Berlin-Cölln nicht im Hauptstadt-Stil")
		checks += 1

	_main.call("select_location", -1)
	labels.record_drawn = false
	print("  Namen und Panels: %d Prüfungen" % checks)


## Wirtschaft in der Oberfläche (seit M5).
func _check_economy_ui(sim: SimulationClient) -> void:
	var checks := 0
	var panel: StatePanel = _main.call("state_panel")
	var sliders := panel.find_children("*", "HSlider", true, false)
	if sliders.size() != 3:
		_fail("Wirtschaft: %d statt 3 Steuerregler im Staatspanel" % sliders.size())
	checks += 1
	panel.show_budget()
	await _frames(2)
	var budget := panel.budget_text()
	for part in ["Kontribution", "Akzise", "Zoll", "Hof", "Verwaltung", "Heer (", "Zinsen", "Schulden"]:
		if not budget.contains(part):
			_fail("Wirtschaft: Haushaltsfenster ohne „%s“" % part)
	if not panel.is_budget_visible():
		_fail("Wirtschaft: Haushaltsfenster nicht sichtbar")
	checks += 2
	# Ein Regler (ohne Ziehen) schickt sofort einen Befehl; der Hinweis nennt die Steuerart.
	var excise: HSlider = sliders[1]
	excise.value = excise.value + 1.0
	var hint := panel.find_child("TaxHint", true, false) as Label
	if hint == null or not hint.text.contains("Akzise"):
		_fail("Wirtschaft: kein Hinweis zur neuen Akzise (%s)" % (hint.text if hint else "–"))
	checks += 1

	var player := sim.get_player_state_index()
	var capital := int(sim.get_state(player).get("capital_index", -1))
	_main.call("select_location", capital)
	await _frames(2)
	var location: LocationPanel = _main.call("location_panel")
	var economy := location.find_child("Economy", true, false) as VBoxContainer
	var texts := PackedStringArray()
	for label: Node in economy.find_children("*", "Label", true, false):
		texts.append((label as Label).text)
	var joined := " | ".join(texts)
	if not (joined.contains("Markt") and joined.contains("Taler") and joined.contains("×")):
		_fail("Wirtschaft: Ortspanel ohne Markt, Einkommen oder Preise (%s)" % joined.left(120))
	checks += 1
	_main.call("set_map_mode", MapModes.MAP_MAERKTE)
	await _frames(2)
	_main.call("set_map_mode", MapModes.MAP_POLITICAL)
	_main.call("select_location", -1)
	print("  Wirtschaft: %d Prüfungen" % checks)


## Spielmenü, Schnellspeichern und -laden, automatischer Spielstand (seit M6).
func _check_save_ui(sim: SimulationClient) -> void:
	var dir := "user://klick_test_spielstaende"
	_clear_saves(dir)
	_main.set("save_dir", dir)
	var menu: GameMenu = _main.call("game_menu")
	var bar: TopBar = _main.call("top_bar")
	_main.call("set_speed", 3)
	# Das Haushaltsfenster aus der Wirtschaftsprüfung ist ein eigenes Fenster und nimmt Tasten
	# entgegen; Escape schließt es (wie im Spiel), danach erreicht F10 die Hauptszene.
	await _key(KEY_ESCAPE)

	await _key(KEY_F10)
	# 1648 hat kein Parlament: keine Rolle „Partei“ im Spielmenü (Review M8).
	if menu.role_count() != 0:
		_fail("Spielmenü 1648: %d Rollen statt keiner" % menu.role_count())
	if not menu.is_open() or int(_main.call("speed")) != 0:
		_fail("Spielstände: F10 öffnet das Menü nicht oder die Zeit läuft weiter")
	menu.save_as("autosave_1")
	if FileAccess.file_exists(SaveSlots.autosave_path(dir)):
		_fail("Spielstände: reservierter Name „autosave_1“ wurde angenommen")
	menu.save_as("Klicktest")
	var saved_checksum := sim.get_checksum()
	if not FileAccess.file_exists(SaveSlots.file_path(dir, "Klicktest")):
		_fail("Spielstände: „Klicktest“ wurde nicht gespeichert")
	if not menu.entry_names().has("Klicktest"):
		_fail("Spielstände: Liste ohne „Klicktest“ (%s)" % ", ".join(menu.entry_names()))
	if not bar.speed_line_text().contains("Gespeichert"):
		_fail("Spielstände: keine Meldung nach dem Speichern (%s)" % bar.speed_line_text())
	await _key(KEY_ESCAPE)
	if menu.is_open() or int(_main.call("speed")) != 3:
		_fail("Spielstände: Escape schließt das Menü nicht oder setzt die Zeit nicht fort")
	_main.call("set_speed", 0)

	sim.advance_days(15)
	await _key(KEY_F5)
	var quick_checksum := sim.get_checksum()
	if not FileAccess.file_exists(SaveSlots.quicksave_path(dir)):
		_fail("Spielstände: F5 legt keinen Schnellspeicher an")
	sim.advance_days(20)
	await _key(KEY_F9)
	if sim.get_checksum() != quick_checksum:
		_fail(
			"Spielstände: F9 lädt nicht den Schnellspeicher (%s statt %s)"
			% [sim.get_checksum(), quick_checksum]
		)

	# Laden aus der Liste; eine fremde Datei erscheint ausgegraut.
	var garbage := FileAccess.open(SaveSlots.file_path(dir, "kaputt"), FileAccess.WRITE)
	garbage.store_string("kein Spielstand")
	garbage.close()
	_main.call("open_game_menu")
	if not menu.entry_names().has("kaputt") or menu.load_entry("kaputt"):
		_fail("Spielstände: fremde Datei fehlt in der Liste oder lässt sich laden")
	if not menu.load_entry("Klicktest") or sim.get_checksum() != saved_checksum or menu.is_open():
		_fail(
			"Spielstände: Laden aus der Liste (%s statt %s)" % [sim.get_checksum(), saved_checksum]
		)

	# Automatischer Spielstand: mit Stufe 5 bis zum nächsten Jahreswechsel.
	var year := int(sim.get_date().get("year", 0))
	var autosave := SaveSlots.autosave_path(dir)
	_main.call("set_speed", 5)
	var deadline := Time.get_ticks_msec() + 15000
	while not FileAccess.file_exists(autosave) and Time.get_ticks_msec() < deadline:
		await process_frame
	_main.call("set_speed", 0)
	var date: Dictionary = sim.get_save_info(autosave).get("date", {})
	if (
		int(date.get("year", 0)) != year + 1
		or int(date.get("month", 0)) != 1
		or int(date.get("day", 0)) != 1
	):
		_fail("Spielstände: kein automatischer Spielstand am 1. Januar %d (%s)" % [year + 1, date])
	_clear_saves(dir)
	print("  Spielstände: Menü, Speichern, Escape, F5, F9, Liste, automatischer Spielstand")


## Neues Spiel 1848 über das Spielmenü, Modus Verwaltung (Taste Ü), Staatswechsel über die
## Ortsinfo (seit M7).
## Startbildschirm (seit M13, vertical_slice.md 3.1): Hauptmenü und Lesezeichen; schließt ihn danach,
## damit die übrigen Prüfungen auf der Karte 1648 laufen.
func _check_start_screen_menu() -> void:
	var start: StartScreen = _main.call("start_screen")
	if start == null or not start.is_open() or start.page() != StartScreen.Page.MENU:
		_fail("Startbildschirm: beim Start nicht offen")
		return
	var text := start.page_text()
	for expected in ["EINIGKEIT", "Neues Spiel", "Fortsetzen", "Laden …", "Geschichtsstunde", "Beenden"]:
		if not text.contains(expected):
			_fail("Startbildschirm: Hauptmenü ohne „%s“" % expected)
	# Tasten erreichen die Karte nicht, solange er offen ist.
	await _key(KEY_3)
	if int(_main.call("speed")) != 0:
		_fail("Startbildschirm: Taste 3 startet die Zeit (%d)" % int(_main.call("speed")))
	start.menu_button("NewGame").emit_signal("pressed")
	await _frames(1)
	text = start.page_text()
	if start.page() != StartScreen.Page.SCENARIO:
		_fail("Startbildschirm: „Neues Spiel“ zeigt nicht die Lesezeichen")
	for expected in ["Völkerfrühling", "Düppel", "Königgrätz", "Sedan", "Weitere", "Technikprototyp"]:
		if not text.contains(expected):
			_fail("Startbildschirm: Lesezeichen ohne „%s“" % expected)
	if start.selected_scenario() != "voelkerfruehling":
		_fail("Startbildschirm: vorgewählt ist %s statt voelkerfruehling" % start.selected_scenario())
	await _key(KEY_ESCAPE)
	if start.page() != StartScreen.Page.MENU:
		_fail("Startbildschirm: Esc führt nicht zurück ins Hauptmenü")
	start.close()
	await _frames(1)
	print("  Startbildschirm: Hauptmenü, Lesezeichen mit 1648 unter „Weitere“, Esc zurück")


## Startbildschirm bis zum Spielbeginn (seit M13): 1848, Bayern, Stufe Meister, offener Verlauf. Läuft
## zuletzt, weil er die Partie ersetzt.
func _check_start_screen_flow() -> void:
	var start: StartScreen = _main.call("start_screen")
	var dir := "user://klick_test_spielstaende"
	_clear_saves(dir)
	_main.set("save_dir", dir)
	_main.call("open_start_screen")
	await _frames(1)
	if not start.is_open():
		_fail("Startbildschirm: open_start_screen öffnet ihn nicht")
		return
	if not start.menu_button("Continue").disabled or not start.menu_button("Load").disabled:
		_fail("Startbildschirm: „Fortsetzen“ oder „Laden“ ohne Spielstände frei")
	start.close()
	_main.call("save_game_as", "Vor dem Start")
	_main.call("open_start_screen")
	await _frames(1)
	if start.menu_button("Continue").disabled:
		_fail("Startbildschirm: „Fortsetzen“ gesperrt, obwohl ein Spielstand da ist")
	_clear_saves(dir)
	start.menu_button("NewGame").emit_signal("pressed")
	start.select_scenario("voelkerfruehling")
	start.confirm_scenario()
	await _frames(2)
	var sim: SimulationClient = _main.call("simulation")
	var text := start.page_text()
	if start.page() != StartScreen.Page.STATE or sim.get_scenario_name() != "Völkerfrühling":
		_fail("Startbildschirm: nach dem Lesezeichen keine Staatswahl (%s)" % sim.get_scenario_name())
		return
	for expected in ["Königreich Preußen  ·  mittel", "Kaisertum Österreich  ·  schwer", "Anderer Staat"]:
		if not text.contains(expected):
			_fail("Startbildschirm: Staatswahl ohne „%s“" % expected)
	if start.selected_state() != "PRU":
		_fail("Startbildschirm: vorgewählt ist %s statt PRU" % start.selected_state())
	if not start.choose_state("BAV"):
		_fail("Startbildschirm: Bayern nicht wählbar")
	start.confirm_state()
	await _frames(1)
	text = start.page_text()
	if start.page() != StartScreen.Page.SETTINGS:
		_fail("Startbildschirm: keine Spieleinstellungen nach der Staatswahl")
		return
	if not text.contains("Geselle") or not text.contains("Historisch"):
		_fail("Startbildschirm: Einstellungen ohne Geselle/Historisch: %s" % text.replace("\n", " | "))
	if not start.choose_difficulty("meister"):
		_fail("Startbildschirm: Stufe Meister fehlt")
	start.set_open_course(true)
	start.start_game()
	await _frames(2)
	# Der Befehl wirkt am ersten Tag; danach sind die Einstellungen fest.
	sim.advance_days(1)
	var settings := sim.get_game_settings()
	var player := str(sim.get_state(sim.get_player_state_index()).get("tag", ""))
	if start.is_open() or player != "BAV" or str(settings.get("difficulty_id", "")) != "meister":
		_fail("Startbildschirm: Partie als %s, Stufe %s, offen %s" % [
			player, settings.get("difficulty_id", ""), start.is_open()
		])
	if not bool(settings.get("open", false)) or bool(settings.get("editable", true)):
		_fail("Startbildschirm: offener Verlauf nach dem ersten Tag %s, änderbar %s" % [
			settings.get("open", false), settings.get("editable", true)
		])
	if sim.set_game_settings("lehrling", false):
		_fail("Startbildschirm: Stufe nach dem ersten Tag noch änderbar")
	print("  Startbildschirm: 1848 als Bayern, Stufe %s, Verlauf offen" % settings.get("difficulty_name", ""))


## Bereichsleiste, Fensterverwaltung und Jahrbuch (seit M13a5a) in der Partie ab 1848 (als Bayern).
func _check_areas() -> void:
	var bar: AreaBar = _main.call("area_bar")
	var windows: WindowManager = _main.call("window_manager")
	var yearbook: YearbookWindow = _main.call("yearbook_window")
	var diplomacy: DiplomacyWindow = _main.call("diplomacy_window")
	var sim: SimulationClient = _main.call("simulation")
	_main.call("_refresh_all")
	var areas := bar.visible_areas()
	for expected in ["haushalt", "gesetze", "diplomatie", "journale", "jahrbuch", "chronik"]:
		if not areas.has(expected):
			_fail("Bereichsleiste ohne „%s“ (%s)" % [expected, ", ".join(areas)])
	bar.button(&"jahrbuch").emit_signal("pressed")
	await _frames(2)
	if not yearbook.is_open() or not bar.button(&"jahrbuch").button_pressed:
		_fail("Bereichsleiste: „Jahrbuch“ öffnet das Jahrbuch nicht")
	var columns := yearbook.column_names()
	if columns.size() != 17 or columns[0] != "Bevölkerung (Mio.)" or not columns.has("Einigkeit"):
		_fail("Jahrbuch: Spalten %s" % ", ".join(columns))
	yearbook.select_column(columns.find("Einigkeit"))
	yearbook.set_compare(0, sim.get_state_index("AUT"))
	await _frames(1)
	var table := yearbook.table_text()
	if yearbook.shown_states().size() != 2 or not table.contains("Österreich") or not table.contains("01.03.1848"):
		_fail("Jahrbuch: Vergleich mit Österreich fehlt (%s)" % table.replace("\n", " / "))
	bar.button(&"diplomatie").emit_signal("pressed")
	await _frames(2)
	if not diplomacy.is_open() or yearbook.is_open() or windows.open_window() != diplomacy:
		_fail("Fensterverwaltung: Diplomatie schließt das Jahrbuch nicht")
	await _key(KEY_ESCAPE)
	if diplomacy.is_open() or windows.open_window() != null:
		_fail("Fensterverwaltung: Escape schließt die Diplomatie nicht")
	# Seit M13a5d öffnet „Chronik“ das Fenster der Chronik (das Panel der linken Spalte ruht).
	var chronicle: AreaWindow = _main.call("area_window", &"chronik")
	bar.button(&"chronik").emit_signal("pressed")
	if not chronicle.is_open():
		_fail("Bereichsleiste: „Chronik“ öffnet die Chronik nicht")
	bar.button(&"chronik").emit_signal("pressed")
	if chronicle.is_open():
		_fail("Bereichsleiste: „Chronik“ schließt die Chronik nicht")
	await _key(KEY_B)
	await _frames(1)
	if not yearbook.is_open():
		_fail("Taste B öffnet das Jahrbuch nicht")
	await _key(KEY_B)
	if yearbook.is_open():
		_fail("Taste B schließt das Jahrbuch nicht")
	print("  Bereiche: %s; Jahrbuch mit %d Spalten, Vergleich, Fensterverwaltung" % [", ".join(areas), columns.size()])
	# Fenster der Bereiche (seit M13a5b): je Bereich ein Stichwort im Inhalt.
	var expected_text := {
		&"kabinett": "FRÜHERE AMTSINHABER", &"gesetze": "VERFASSUNG UND WAHLRECHT", &"gruppen": "Zustimmung",
		&"parlament": "Wahlrecht", &"gesellschaft": "ALTERSAUFBAU", &"wirtschaft": "GÜTER JE MONAT",
	}
	for area: StringName in expected_text:
		if not bar.visible_areas().has(str(area)):
			_fail("Bereichsleiste ohne „%s“" % area)
			continue
		bar.button(area).emit_signal("pressed")
		await _frames(2)
		var window: AreaWindow = _main.call("area_window", area)
		if window == null or not window.is_open() or windows.open_window() != window or not bar.button(area).button_pressed:
			_fail("Bereich %s: Fenster nicht offen" % area)
			continue
		if not window.window_text().contains(str(expected_text[area])):
			_fail("Bereich %s ohne „%s“: %s" % [area, expected_text[area], window.window_text().left(200).replace("\n", " / ")])
	var parliament: ParliamentWindow = _main.call("area_window", &"parlament")
	bar.button(&"parlament").emit_signal("pressed")
	await _frames(1)
	parliament.select_parliament(0)
	await _frames(1)
	if parliament.chosen_parliament() != 0 or not parliament.window_text().contains("Wahlrecht"):
		_fail("Parlament: Auswahl der Nationalversammlung")
	await _key(KEY_ESCAPE)
	if windows.open_window() != null:
		_fail("Fensterverwaltung: Escape schließt das Fenster des Bereichs nicht")
	print("  Fenster der Bereiche: Kabinett, Gesetze, Gruppen, Parlament, Gesellschaft, Wirtschaft")
	await _check_bars()


## Einstellungen, Chronik und Lexikon (seit M13a5d), mit einer eigenen Datei der Einstellungen.
func _check_settings_chronicle_lexicon() -> void:
	var path := "user://klick_test_einstellungen.cfg"
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	_main.call("use_settings_file", path)
	var settings: UserSettings = _main.call("settings")
	var window: SettingsWindow = _main.call("settings_window")
	var menu: GameMenu = _main.call("game_menu")
	_main.call("open_game_menu")
	(menu.find_child("SettingsButton", true, false) as Button).emit_signal("pressed")
	await _frames(1)
	if not window.is_open() or menu.is_open():
		_fail("Einstellungen: Spielmenü öffnet das Fenster nicht")
	window.show_tab(2)
	await _frames(1)
	var rule := window.control("spiel/pause_amt") as CheckBox
	if rule == null:
		_fail("Einstellungen: Pausenregel „Amtswechsel“ fehlt")
	else:
		rule.button_pressed = true
		await _frames(1)
		var saved := ConfigFile.new()
		if not settings.pause_on("pause_amt") or saved.load(path) != OK or not bool(saved.get_value("spiel", "pause_amt", false)):
			_fail("Einstellungen: Pausenregel nicht gespeichert")
	window.show_tab(4)
	if window.capture_key("jahrbuch", KEY_J):
		_fail("Einstellungen: Taste J doppelt vergeben (Journale)")
	if not window.capture_key("jahrbuch", KEY_G) or settings.key_of("jahrbuch") != KEY_G:
		_fail("Einstellungen: Taste G für das Jahrbuch nicht übernommen")
	await _key(KEY_G)
	var yearbook: YearbookWindow = _main.call("yearbook_window")
	if not yearbook.is_open():
		_fail("Einstellungen: neue Taste G öffnet das Jahrbuch nicht")
	_main.call("close_yearbook")
	settings.reset_group("tasten")
	settings.reset_group("spiel")
	settings.apply(_main.get_tree())
	_quiet_hints()
	_main.call("close_settings")
	DirAccess.remove_absolute(ProjectSettings.globalize_path(path))
	# Chronik als Fenster mit Filter.
	var bar: AreaBar = _main.call("area_bar")
	bar.button(&"chronik").emit_signal("pressed")
	await _frames(2)
	var chronicle: ChronicleWindow = _main.call("area_window", &"chronik")
	if not chronicle.is_open() or chronicle.entry_texts().is_empty():
		_fail("Chronik: Fenster leer oder nicht offen")
	chronicle.set_filter(0, true, "")
	await _frames(1)
	for text in chronicle.entry_texts():
		if not text.contains("WENDEPUNKT"):
			_fail("Chronik: Filter „nur Wendepunkte“ zeigt „%s“" % text)
			break
	chronicle.set_filter(0, false, "zzzz-gibt-es-nicht")
	if not chronicle.entry_texts().is_empty():
		_fail("Chronik: Suche ohne Treffer zeigt Einträge")
	chronicle.set_filter(0, false, "")
	# Lexikon über die Leiste und über einen Verweis im Tooltip.
	bar.button(&"lexikon").emit_signal("pressed")
	await _frames(1)
	var lexicon: LexiconWindow = _main.call("lexicon_window")
	if not lexicon.is_open() or chronicle.is_open():
		_fail("Lexikon: Fenster nicht offen oder Chronik nicht geschlossen")
	var layer: TooltipLayer = _main.call("tooltip_layer")
	layer.link_clicked.emit("begriff:akzise")
	await _frames(1)
	if lexicon.current_article() != "akzise" or not lexicon.article_text().contains("Verbrauchssteuer"):
		_fail("Lexikon: Verweis öffnet „%s“" % lexicon.current_article())
	_main.call("close_lexicon_window")
	print("  Einstellungen (Pausenregel, Taste), Chronik mit Filter, Lexikon mit Verweis")


## Obere Leiste, rechte Übersicht, Ticker und verschachtelte Tooltips (seit M13a5c).
func _check_bars() -> void:
	var top: TopBar = _main.call("top_bar")
	var overview: OverviewPanel = _main.call("overview_panel")
	var layer: TooltipLayer = _main.call("tooltip_layer")
	var windows: WindowManager = _main.call("window_manager")
	_main.call("select_location", -1)
	_main.call("_refresh_bars", true)
	await _frames(1)
	if not top.field_text(&"kasse").contains("Taler") or not top.field_text(&"verwaltung").ends_with("Orte"):
		_fail("Obere Leiste: Kasse „%s“, Verwaltung „%s“" % [top.field_text(&"kasse"), top.field_text(&"verwaltung")])
	if top.field_text(&"vermaechtnis").split(" ").size() != 5 or top.field_text(&"legitimitaet") == "–":
		_fail("Obere Leiste: Vermächtnis „%s“, Legitimität „%s“" % [top.field_text(&"vermaechtnis"), top.field_text(&"legitimitaet")])
	var texts_source: BarTooltips = _main.call("bar_tooltips")
	layer.show_for(top.field(&"vermaechtnis"), texts_source.legacy, texts_source.legacy_link)
	await _frames(1)
	if not layer.is_showing(0) or not layer.text_of(0).contains("Einigkeit"):
		_fail("Tooltip Vermächtnis: „%s“" % layer.text_of(0))
	layer.open_link("vermaechtnis:einigkeit")
	await _frames(1)
	if not layer.is_showing(1) or not layer.text_of(1).contains("Grundwert"):
		_fail("Verschachtelter Tooltip Einigkeit: „%s“" % layer.text_of(1))
	layer.show_for(top.field(&"kasse"), texts_source.cash)
	layer.open_link("begriff:akzise")
	await _frames(1)
	if not layer.text_of(0).contains("Kontribution") or not layer.text_of(1).begins_with("Akzise:"):
		_fail("Tooltip Kasse mit Begriff Akzise: „%s“ / „%s“" % [layer.text_of(0).left(80), layer.text_of(1)])
	layer.show_for(top.field(&"legitimitaet"), texts_source.legitimacy)
	if not layer.text_of(0).contains("Unruhe") or not layer.text_of(0).contains("Zustimmung der Gruppen"):
		_fail("Tooltip Legitimität ohne Ursachen der Unruhe: „%s“" % layer.text_of(0).replace("\n", " / "))
	# Seit M14a2: Symbole statt Überschriften, dazu Unruhe, Bevölkerung (mit der Verwaltung) und Heer.
	for key: StringName in [&"kasse", &"legitimitaet", &"unruhe", &"bevoelkerung", &"heer", &"vermaechtnis"]:
		var metric := top.field(key)
		var icon := metric.get_node_or_null("Icon") as TextureRect if metric != null else null
		if metric == null or not metric.is_visible_in_tree() or icon == null or icon.texture == null or top.field_text(key) == "–":
			_fail("Obere Leiste: Kennzahl %s ohne Symbol oder Wert („%s“)" % [key, top.field_text(key)])
	for pair: Array in [[&"unruhe", texts_source.unrest, "Ziel"], [&"bevoelkerung", texts_source.population, "Einwohner"], [&"heer", texts_source.army, "Kosten"]]:
		layer.show_for(top.field(pair[0]), pair[1])
		if not layer.text_of(0).contains(str(pair[2])):
			_fail("Tooltip %s: „%s“" % [pair[0], layer.text_of(0).replace("\n", " / ")])
	if not top.get_global_rect().encloses(top.field(&"vermaechtnis").get_global_rect()) or top.get_global_rect().size.x > _main.get_viewport().get_visible_rect().size.x + 1.0:
		_fail("Obere Leiste breiter als das Bild (%s)" % top.get_global_rect())
	layer.hide_all()
	var texts := overview.entry_texts()
	var journal := -1
	for i in texts.size():
		if texts[i].begins_with("Die deutsche Frage") or texts[i].begins_with("Revolution"):
			journal = i
			break
	if not overview.visible or journal < 0:
		_fail("Übersicht ohne Journal: %s" % ", ".join(texts))
	else:
		overview.entry_button(journal).emit_signal("pressed")
		await _frames(1)
		if windows.open_window() != _main.call("journal_window"):
			_fail("Übersicht: Klick auf das Journal öffnet es nicht")
		_main.call("close_journal")
	var ticker: TickerBar = _main.call("ticker")
	ticker.push([{"date_text": "01.01.1849", "text": "Probe für den Ticker", "turning_point": true, "location_index": -1}] as Array[Dictionary])
	await _frames(1)
	await _check_settings_chronicle_lexicon()
	print("  Leisten: Kasse %s, Vermächtnis %s; Übersicht %d Einträge; Tooltips verschachtelt" % [
		top.field_text(&"kasse"), top.field_text(&"vermaechtnis"), texts.size()
	])


## Urteil der Geschichte (seit M13a4) in der Partie ab 1848: Inhalt, Export der Chronik, Weiterspielen,
## Hauptmenü. Bis 1871 rechnet der Test nicht; er öffnet das Urteil wie das Spielmenü nach dem Ende.
func _check_verdict() -> void:
	var window: VerdictWindow = _main.call("verdict_window")
	var sim: SimulationClient = _main.call("simulation")
	_main.call("open_verdict")
	await _frames(2)
	var text := "\n".join(window.content_texts())
	if not window.is_open() or not text.contains("Vermächtnis") or not text.contains("Die deutsche Frage"):
		_fail("Urteil: nicht offen oder ohne Vermächtnis und Journal (%s)" % text.replace("\n", " | "))
	var path: String = _main.call("export_chronicle")
	if path.is_empty() or not FileAccess.file_exists(path):
		_fail("Urteil: Chronik nicht exportiert (%s)" % path)
	else:
		var html := FileAccess.get_file_as_string(path)
		if not html.contains("<h2>Chronik</h2>") or not html.contains("Vermächtnis"):
			_fail("Urteil: exportierte Chronik ohne Chronik oder Vermächtnis")
		DirAccess.remove_absolute(path)
	window.button("Continue").emit_signal("pressed")
	await _frames(1)
	if window.is_open():
		_fail("Urteil: „Weiterspielen“ schließt es nicht")
	_main.call("open_verdict")
	window.button("Menu").emit_signal("pressed")
	await _frames(1)
	var start: StartScreen = _main.call("start_screen")
	if window.is_open() or not start.is_open():
		_fail("Urteil: „Hauptmenü“ öffnet den Startbildschirm nicht")
	start.close()
	var legacy: String = (_main.call("state_panel") as StatePanel).legacy_text()
	if not legacy.begins_with("Vermächtnis: Einigkeit"):
		_fail("Staatspanel ohne Vermächtnis (%s)" % legacy)
	print("  Urteil der Geschichte: Inhalt, Chronik als HTML, Weiterspielen, Hauptmenü (%s)" % sim.get_scenario_name())


## Zeitung und Epochenrahmen (seit M13b1, vertical_slice.md 5.1 und 5.2) in der Partie ab 1848 (als Bayern):
## eingebettete Schriften im Theme, Rahmen der Fenster, die Zeitung öffnet sich bei einer wichtigen Nachricht
## von selbst und hält die Zeit an, ein Klick legt sie beiseite, das Jahrbuch listet die Zeitungen.
func _check_newspaper() -> void:
	var sim: SimulationClient = _main.call("simulation")
	var paper: NewspaperWindow = _main.call("newspaper_window")
	var theme := ThemeDB.get_project_theme()
	var base := theme.default_font as FontVariation
	var masthead := theme.get_font(&"font", &"MastheadLabel") as FontFile
	if base == null or not (base.base_font as FontFile).resource_path.ends_with("SourceSerif4.ttf") or masthead == null:
		_fail("Schriften: Grundschrift %s, Zeitungskopf %s" % [base, masthead])
	var diplomacy: Control = _main.call("diplomacy_window")
	if not diplomacy.has_meta(&"epoch_frame"):
		_fail("Diplomatiefenster ohne Rahmen der Epoche")
	# Bis eine Zeitung von selbst erscheint (Ereignisse wie im Ereignislabor mit der Standardoption).
	var opened := false
	for _step in 200:
		sim.advance_days(3)
		_main.call("_after_time_passed")
		if paper.is_open():
			opened = true
			break
		_answer_events()
		if paper.is_open(): # _answer_events legt sie sonst beiseite
			opened = true
			break
	if not opened and _newspapers_seen == 0:
		_fail("Keine Zeitung von selbst bis %s" % sim.get_date_text())
		return
	if not opened:
		var pages := sim.get_front_pages(sim.get_player_state_index(), 2)
		if pages.is_empty():
			_fail("Keine Titelseite bis %s" % sim.get_date_text())
			return
		_main.call("open_newspaper", int(pages[-1].get("chronicle_index", -1)))
	await _frames(2)
	var page := paper.page()
	var text := paper.page_text()
	if int(_main.call("speed")) != 0 or str(page.get("paper", "")).is_empty() or str(page.get("headline", "")).is_empty():
		_fail("Zeitung: Tempo %d, Seite %s" % [int(_main.call("speed")), str(page).left(300)])
	if not text.contains("Nr. ") or text.contains("{"):
		_fail("Zeitung: Kopf oder Platzhalter „%s“" % text.left(300))
	var rect := paper.get_global_rect()
	var area := _main.get_viewport().get_visible_rect()
	if not area.encloses(rect):
		_fail("Zeitung ragt aus dem Bild: %s in %s" % [rect, area])
	_main.call("set_speed", 3)
	await _click(rect.get_center())
	await _frames(1)
	if paper.is_open():
		_fail("Ein Klick legt die Zeitung nicht beiseite")
	# Jahrbuch: Liste der Zeitungen, ein Klick öffnet eine.
	_main.call("open_yearbook")
	await _frames(1)
	var yearbook: YearbookWindow = _main.call("yearbook_window")
	yearbook.show_papers(true)
	await _frames(1)
	var entries := yearbook.paper_entries()
	if entries.is_empty():
		_fail("Jahrbuch ohne Zeitungen")
	else:
		var index := int(entries[0].get("chronicle_index", -1))
		(yearbook.find_child("PaperList", true, false).get_child(0).get_child(0) as Button).pressed.emit()
		await _frames(1)
		if not paper.is_open() or int(paper.page().get("chronicle_index", -2)) != index:
			_fail("Jahrbuch: Klick auf „%s“ öffnet die Zeitung nicht" % str(entries[0].get("text", "")))
		_main.call("close_newspaper")
	yearbook.show_papers(false)
	_main.call("close_yearbook")
	print("  Zeitung: %d von selbst erschienen, „%s“ im %s; Jahrbuch mit %d Zeitungen; Schriften und Rahmen der Epoche" % [
		_newspapers_seen + (1 if opened else 0), page.get("headline", ""), page.get("paper", ""), entries.size()
	])


## Musik und Klänge (seit M13b2, vertical_slice.md 5.3) in der Partie ab 1848 (als Bayern): Die Stimmung folgt
## der Lage des Staates, Fenster, Ereignisse und Befehle klingen. Ohne gerenderte Musik nur die Stille.
func _check_audio() -> void:
	var audio: AudioDirector = _main.call("audio_director")
	if audio == null:
		_fail("Kein Klangregisseur")
		return
	if not audio.has_music():
		print("  Musik und Klänge: keine Dateien (tools/klang/klang.py), das Spiel bleibt still")
		return
	var mood := str(_main.call("mood_for_player"))
	_main.call("_update_mood")
	if audio.mood() != mood or not audio.pieces_for(mood).has(audio.current_piece()):
		_fail("Musik: Stimmung %s, gespielt %s (%s)" % [mood, audio.current_piece(), audio.mood()])
	var before := audio.sounds_played.duplicate()
	_main.call("open_yearbook")
	await _frames(1)
	_main.call("close_yearbook")
	var state_panel: StatePanel = _main.call("state_panel")
	state_panel.tax_rate_requested.emit(0, 12)
	await _frames(1)
	for sound in ["papier", "gaensekiel"]:
		if int(audio.sounds_played.get(sound, 0)) <= int(before.get(sound, 0)):
			_fail("Klang %s nicht gespielt (%s)" % [sound, str(audio.sounds_played)])
	if int(audio.sounds_played.get("glocke", 0)) == 0:
		_fail("Kein Ereignis mit Glocke bis hierher (%s)" % str(audio.sounds_played))
	print("  Musik und Klänge: Stimmung %s, „%s“; Klänge %s" % [
		audio.mood(), audio.pieces.get(audio.current_piece(), {}).get("titel", ""), str(audio.sounds_played)
	])


## Hinweise beim ersten Öffnen ruhen im Test (seit M13b3), außer in _check_onboarding; die gesehenen stehen
## in einer eigenen Datei.
func _quiet_hints() -> void:
	(_main.call("settings") as UserSettings).set_value("spiel/hinweise", false)
	var hint: HintCard = _main.call("hint_card")
	if hint != null:
		hint.dismiss()
		hint.use_seen_file("user://klick_test_hinweise.cfg")


## Karte frei (seit M14a1, europa_und_oberflaeche.md 5): Ohne offenes Fenster verdecken Leisten und Panels
## höchstens FREE_MAP_LIMIT des Bildes; die Staatsübersicht ist eine Schublade (Bereich „Staat“, Wappen in der
## oberen Leiste, Esc schließt sie), Haushalt und Zollunion öffnen sie mit.
func _check_free_map() -> void:
	var screen := root.get_visible_rect()
	(_main.call("window_manager") as WindowManager).close_all()
	_main.call("set_state_drawer", false)
	_main.call("select_location", -1)
	await _frames(2)
	var panel: StatePanel = _main.call("state_panel")
	if bool(_main.call("is_state_drawer_open")) or screen.intersects(panel.get_global_rect()):
		_fail("Schublade: geschlossen und doch im Bild %s" % panel.get_global_rect())
	var covered := _ui_coverage()
	if covered > FREE_MAP_LIMIT:
		_fail("Karte frei: Leisten verdecken %.1f %% des Bildes (höchstens %.0f %%)" % [100.0 * covered, 100.0 * FREE_MAP_LIMIT])
	var bar: AreaBar = _main.call("area_bar")
	bar.button(&"staat").emit_signal("pressed")
	await _frames(2)
	var opened := bool(_main.call("is_state_drawer_open")) and screen.encloses(panel.get_global_rect())
	await _key(KEY_ESCAPE)
	await _frames(2)
	var closed := not bool(_main.call("is_state_drawer_open")) and not screen.intersects(panel.get_global_rect())
	var top: TopBar = _main.get("_top_bar")
	top.state_requested.emit()
	await _frames(1)
	var by_coat := bool(_main.call("is_state_drawer_open"))
	_main.call("set_state_drawer", false)
	bar.button(&"haushalt").emit_signal("pressed")
	await _frames(2)
	var budget := bool(_main.call("is_state_drawer_open")) and panel.is_budget_visible()
	_main.call("set_state_drawer", false)
	await _frames(1)
	if not opened or not closed or not by_coat or not budget or panel.is_budget_visible():
		_fail("Schublade: Bereich öffnet %s, Esc schließt %s, Wappen öffnet %s, Haushalt mit Schublade %s" % [
			opened, closed, by_coat, budget
		])
	print("  Karte frei: Leisten verdecken %.1f %% des Bildes; Schublade über Bereich, Wappen, Haushalt, Esc" % (100.0 * covered))


## Anteil des Bildes unter sichtbaren Leisten und Panels (Raster mit 8 Pixeln).
func _ui_coverage() -> float:
	var screen := root.get_visible_rect()
	var controls: Array[Control] = [
		_main.get("_top_bar"), _main.call("area_bar"), _main.get("_left_column"), _main.get("_mode_bar"),
		_main.call("ticker"), _main.get("_play_bar"),
	]
	for child in (_main.get("_right_column") as Control).get_children():
		if child is Control and (child as Control).name != &"Spacer":
			controls.append(child as Control)
	var rects: Array[Rect2] = []
	for control in controls:
		if control != null and control.is_visible_in_tree():
			var cut := control.get_global_rect().intersection(screen)
			if cut.has_area():
				rects.append(cut)
	var covered := 0
	var total := 0
	for y in range(0, int(screen.size.y), 8):
		for x in range(0, int(screen.size.x), 8):
			total += 1
			var point := Vector2(x + 4, y + 4)
			for rect in rects:
				if rect.has_point(point):
					covered += 1
					break
	return float(covered) / maxf(total, 1.0)


## Spieltest 11/2026: Das Panel der Geschichtsstunde verdeckt weder Ortsinfo, Übersicht und Legende noch die
## Staatsübersicht; ein offenes Fenster höchstens am Rand (nur wenn sonst kein Platz ist, dann kompakt).
func _check_tutorial_free(tutorial: TutorialPanel, step_id: String) -> void:
	var rect := tutorial.get_global_rect()
	var panels: Array[Control] = [
		_main.call("location_panel"), _main.call("overview_panel"), _main.call("state_panel"),
		_main.find_child("LegendPanel", true, false) as Control,
	]
	for panel in panels:
		if panel != null and panel.is_visible_in_tree() and rect.intersects(panel.get_global_rect()):
			_fail("Geschichtsstunde „%s“: Panel %s verdeckt %s %s" % [step_id, rect, panel.name, panel.get_global_rect()])
	var window: Control = (_main.call("window_manager") as WindowManager).open_window()
	if window != null and window.is_visible_in_tree():
		var cut := rect.intersection(window.get_global_rect())
		if cut.has_area() and (cut.size.y > 8.0 and cut.size.x > 8.0):
			_fail("Geschichtsstunde „%s“: Panel %s verdeckt das Fenster %s" % [step_id, rect, window.name])


## Onboarding (seit M13b3, vertical_slice.md 5.4; Abnahme b3: Klicktest durch die Geschichtsstunde): Start über
## das Hauptmenü, jeder Schritt mit seiner Aufgabe über die Bedienung, die Zeit nur, wo der Schritt es erlaubt;
## dann ein Hinweis beim ersten Öffnen (und nur dann) und „Was ist passiert?“ nach dem Laden.
func _check_onboarding() -> void:
	var tutorial: TutorialPanel = _main.call("tutorial_panel")
	var start: StartScreen = _main.call("start_screen")
	var dir := "user://klick_test_spielstaende"
	_clear_saves(dir)
	_main.set("save_dir", dir)
	_main.call("open_start_screen")
	await _frames(1)
	var lesson_button := start.menu_button("Lesson")
	if lesson_button == null or lesson_button.disabled:
		_fail("Geschichtsstunde: Knopf im Hauptmenü fehlt oder ist gesperrt")
		return
	lesson_button.emit_signal("pressed")
	await _frames(2)
	var sim: SimulationClient = _main.call("simulation")
	var player := str(sim.get_state(sim.get_player_state_index()).get("tag", ""))
	if not tutorial.is_active() or player != "PRU" or start.is_open() or sim.get_date_text() != "1. März 1848":
		_fail("Geschichtsstunde: aktiv %s, Staat %s, %s" % [tutorial.is_active(), player, sim.get_date_text()])
		return
	var total := tutorial.step_count()
	var walked := PackedStringArray()
	for _guard in total + 2:
		if not tutorial.is_active():
			break
		var step := tutorial.current_step()
		if not step.time:
			_main.call("set_speed", 3)
			if int(_main.call("speed")) != 0:
				_fail("Geschichtsstunde: Zeit läuft im Schritt „%s“" % step.id)
				_main.call("set_speed", 0)
		if step.check != "weiter" and not tutorial.button("Next").disabled:
			_fail("Geschichtsstunde: „Weiter“ vor der Aufgabe im Schritt „%s“" % step.id)
		if not step.target.is_empty() and _main.call("tutorial_target", step.target) == null:
			_fail("Geschichtsstunde: Ziel „%s“ fehlt" % step.target)
		await _do_lesson_step(step, sim)
		await _frames(2)
		_check_tutorial_free(tutorial, step.id)
		if not tutorial.is_done():
			_fail("Geschichtsstunde: Aufgabe „%s“ nicht erkannt (%s)" % [step.id, sim.get_date_text()])
			tutorial.mark_done()
		walked.append(step.id)
		_main.call("set_speed", 0)
		tutorial.button("Next").emit_signal("pressed")
		await _frames(1)
	if tutorial.is_active() or walked.size() != total:
		_fail("Geschichtsstunde: %d von %d Schritten (%s)" % [walked.size(), total, ", ".join(walked)])
	var date := sim.get_date_text()
	# Hinweise beim ersten Öffnen: einmal je Fenster.
	var settings: UserSettings = _main.call("settings")
	var hint: HintCard = _main.call("hint_card")
	hint.reset_seen()
	settings.set_value("spiel/hinweise", true)
	_main.call("open_diplomacy")
	await _frames(1)
	var first := hint.current_key()
	hint.dismiss()
	_main.call("close_diplomacy")
	_main.call("open_diplomacy")
	await _frames(1)
	if first != "DiplomacyWindow" or hint.visible:
		_fail("Hinweis: beim ersten Öffnen „%s“, beim zweiten sichtbar %s" % [first, hint.visible])
	_main.call("close_diplomacy")
	_quiet_hints()
	# „Was ist passiert?“: F9 lädt den Spielstand aus dem Schritt „Speichern“.
	await _key(KEY_F9)
	await _frames(2)
	var recap: RecapWindow = _main.call("recap_window")
	var news := recap.entries()
	if not recap.is_open() or news.is_empty() or news.size() > RecapWindow.LIMIT:
		_fail("Was ist passiert?: offen %s, %d Einträge" % [recap.is_open(), news.size()])
	else:
		recap.close_requested.emit()
	_clear_saves(dir)
	print("  Geschichtsstunde: %d Schritte (%s) bis %s; Hinweis beim ersten Öffnen; „Was ist passiert?“ mit %d Nachrichten, zuerst „%s“" % [
		walked.size(), ", ".join(walked), date, news.size(), news[0].get("headline", "") if not news.is_empty() else ""
	])


## Die Aufgabe eines Schritts über die Bedienung erledigen.
func _do_lesson_step(step: TutorialStep, sim: SimulationClient) -> void:
	var bar: AreaBar = _main.call("area_bar")
	match step.check:
		"kartenmodus":
			(_main.get("_mode_bar") as MapModeBar).mode_selected.emit(MapModes.MAP_DENSITY)
		"ort":
			_main.call("_select_location", sim.get_location_index("berlin_coelln"))
		"haushalt", "kabinett", "zollunion":
			bar.button(StringName("haushalt" if step.check == "haushalt" else step.check)).emit_signal("pressed")
		"steuern":
			(_main.call("state_panel") as StatePanel).tax_rate_requested.emit(1, 6)
		"gesandter":
			bar.button(&"diplomatie").emit_signal("pressed")
			await _frames(1)
			var window: DiplomacyWindow = _main.call("diplomacy_window")
			window.select_state(sim.get_state_index("AUT"))
			await _frames(1)
			(window.find_child("Action_envoy_send", true, false) as Button).emit_signal("pressed")
		"tempo":
			_main.call("set_speed", 5)
		"ereignis", "wahl":
			var tutorial: TutorialPanel = _main.call("tutorial_panel")
			var night: ElectionNight = _main.call("election_night")
			_main.call("set_speed", 5)
			for _frame in 4000:
				if tutorial.is_done():
					break
				if _answer_events() > 0:
					_main.call("set_speed", 5)
				await process_frame
			if night.is_open():
				_main.call("_close_election_night", false)
		"journal":
			await _key(KEY_J)
		"speichern":
			await _key(KEY_F5)


func _check_new_game() -> void:
	var menu: GameMenu = _main.call("game_menu")
	var dir := "user://klick_test_spielstaende"
	_clear_saves(dir)
	_main.set("save_dir", dir)
	_main.call("open_game_menu")
	if not menu.scenario_ids().has("voelkerfruehling"):
		_fail("Neues Spiel: Szenarien im Menü %s" % ", ".join(menu.scenario_ids()))
		return
	menu.save_as("Stand1648")
	# „Starten“ verlangt einen zweiten Klick; der erste beendet die Partie nicht.
	menu.call("_on_new_game_pressed")
	if not menu.is_open() or str(_main.call("simulation").get_scenario().get("id", "")) != "westfaelischer_frieden":
		_fail("Neues Spiel: ein Klick auf „Starten“ beendet die Partie ohne Rückfrage")
	menu.start_new_game("voelkerfruehling")
	await _frames(2)
	var sim: SimulationClient = _main.call("simulation")
	var catalog: LocationCatalog = _main.get("_catalog")
	var cultures: Dictionary = (_main.call("location_panel") as LocationPanel).culture_colors
	if catalog.no_data_count() > 1000 or cultures.size() <= 9:
		_fail(
			"Neues Spiel: Ortskatalog oder Farben vom alten Szenario (%d Orte ohne Daten, %d Kulturen)"
			% [catalog.no_data_count(), cultures.size()]
		)
	if (
		menu.is_open()
		or sim.get_scenario_name() != "Völkerfrühling"
		or int(sim.get_date().get("year", 0)) != 1848
		or str(sim.get_state(sim.get_player_state_index()).get("tag", "")) != "PRU"
	):
		_fail(
			"Neues Spiel: %s, %s, Menü offen %s" % [
				sim.get_scenario_name(), sim.get_date_text(), menu.is_open()
			]
		)
	await _key(KEY_BRACKETLEFT)
	if int(_main.call("map_mode")) != MapModes.MAP_VERWALTUNG:
		_fail("Taste Ü schaltet nicht in den Modus Verwaltung (%d)" % int(_main.call("map_mode")))
	# Taste „+“ (physisch rechts neben Ü): Eisenbahnen mit Linien und Bahnhof in der Ortsinfo.
	await _key(KEY_BRACKETRIGHT)
	var view: MapView = _main.call("map_view")
	var rail := view.railway_layer()
	if int(_main.call("map_mode")) != MapModes.MAP_EISENBAHNEN or rail == null or not rail.visible:
		_fail("Taste + schaltet nicht in den Modus Eisenbahnen mit sichtbaren Strecken")
	elif rail.open_count() < 100:
		_fail("Eisenbahnen 1848: nur %d eröffnete Abschnitte gezeichnet" % rail.open_count())
	var leipzig_info := sim.get_location(sim.get_location_index("leipzig"))
	if not ", ".join(leipzig_info.get("eisenbahnen", PackedStringArray())).contains("Leipzig-Dresdner Eisenbahn"):
		_fail("Ortsinfo Leipzig: keine Leipzig-Dresdner Eisenbahn (%s)" % leipzig_info.get("eisenbahnen"))
	await _key(KEY_BRACKETLEFT)
	if rail != null and rail.visible:
		_fail("Strecken bleiben außerhalb des Modus Eisenbahnen sichtbar")
	var munich := sim.get_location_index("muenchen")
	_main.call("select_location", munich)
	await _frames(1)
	var panel: LocationPanel = _main.call("location_panel")
	var play := panel.find_child("PlayState", true, false) as Button
	if play == null or not play.visible:
		_fail("Ortsinfo München: Knopf „Diesen Staat spielen“ fehlt")
		return
	play.pressed.emit()
	await _frames(1)
	var bavaria := sim.get_state_index("BAV")
	var bar: TopBar = _main.call("top_bar")
	if sim.get_player_state_index() != bavaria or play.visible:
		_fail("Staatswechsel: Spieler %d statt Bayern (%d)" % [sim.get_player_state_index(), bavaria])
	if not bar.speed_line_text().contains("Bayern"):
		_fail("Staatswechsel: keine Meldung (%s)" % bar.speed_line_text())
	# Ein kleiner Spielerstaat bleibt in der politischen Legende sichtbar.
	var luebeck := sim.get_state_index("LUE")
	var listed := LegendData.political_entries(sim, LegendData.MAX_POLITICAL_ENTRIES, luebeck)
	if not listed.any(func(entry: Dictionary) -> bool: return bool(entry.get("player", false))):
		_fail("Politische Legende: Spielerstaat Lübeck fehlt")

	await _check_party_mode(sim)
	# Die Diplomatie vor dem Wahlabend: Ab Juli 1848 ruht die Bundesversammlung (Ereignis
	# bund.uebergabe, seit M11), dann ist kein Antrag möglich.
	await _check_diplomacy_ui(sim)
	await _check_election_night(sim)
	await _check_events_ui(sim)
	await _check_customs_ui(sim)
	await _check_war_ui(menu)
	await _check_war_bookmarks()
	_check_revolution_paths()

	# Ein Spielstand von 1648 lässt sich im Spiel von 1848 aus der Liste laden.
	_main.call("open_game_menu")
	if not menu.load_entry("Stand1648"):
		_fail("Neues Spiel: Spielstand von 1648 im Menü von 1848 gesperrt")
	sim = _main.call("simulation")
	catalog = _main.get("_catalog")
	if sim.get_scenario().get("id", "") != "westfaelischer_frieden" or catalog.no_data_count() < 4000:
		_fail(
			"Zurück nach 1648: %s, %d Orte ohne Daten"
			% [sim.get_scenario_name(), catalog.no_data_count()]
		)
	_clear_saves(dir)
	print("  Neues Spiel 1848, Modus Verwaltung, Staatswechsel, Spielstand von 1648 geladen")


## Krieg (seit M10, Schritt b3): das Lesezeichen Königgrätz im Menü, Heere auf der Karte, Auswahl
## per Klick auf die Marke, Marschbefehl per Rechtsklick, Kartenmodus „Krieg“ (Taste Y), Escape hebt
## die Auswahl auf.
func _check_war_ui(menu: GameMenu) -> void:
	_main.call("open_game_menu")
	for war_scenario in ["dueppel", "koeniggraetz", "sedan"]:
		if not menu.scenario_ids().has(war_scenario):
			_fail("Neues Spiel: Kriegslesezeichen %s fehlt im Menü (%s)" % [
				war_scenario, ", ".join(menu.scenario_ids())
			])
			return
	menu.start_new_game("koeniggraetz")
	await _frames(2)
	var sim: SimulationClient = _main.call("simulation")
	var view: MapView = _main.call("map_view")
	var layer := view.army_layer()
	if sim.get_scenario_name() != "Königgrätz" or layer == null or layer.marker_count() < 10:
		_fail("Königgrätz: %s, %d Heeresmarken" % [
			sim.get_scenario_name(), layer.marker_count() if layer != null else -1
		])
		return
	var player := sim.get_player_state_index()
	# Seit M14a2: Die Bereichsleiste zählt die Kriege des Spielerstaates am Symbol „Militär“.
	_main.call("_refresh_bars", true)
	var bar: AreaBar = _main.call("area_bar")
	if bar.badge(&"militaer") < 1:
		_fail("Königgrätz: kein Zähler am Bereich Militär (%d)" % bar.badge(&"militaer"))
	var army_id := -1
	for army in sim.get_armies(player):
		if str(army.get("name", "")) == "I. Armee":
			army_id = int(army.get("id", -1))
	var prag := sim.get_location_index("prag")
	view.camera.focus_on(view.location_center(sim.get_location_index("goerlitz")), view.camera.overview_zoom * 4.0)
	await _frames(30)
	# Die Meldung „Neues Spiel“ liegt in der Mitte über der Karte; „Verstanden“ schließt sie.
	for button in _main.find_child("NoticeLayer", true, false).find_children("*", "Button", true, false):
		if (button as Button).text == "Verstanden":
			(button as Button).pressed.emit()
	await _frames(2)
	var marker := layer.marker_center(army_id)
	await _click(view.camera.world_to_screen(marker))
	if int(_main.call("selected_army")) != army_id:
		_fail("Klick auf die Marke der I. Armee wählt sie nicht (%d statt %d)" % [
			int(_main.call("selected_army")), army_id
		])
		return
	var target := view.camera.world_to_screen(view.location_center(prag))
	await _right_click(target)
	sim.advance_days(1)
	_main.call("_after_time_passed")
	var marching := sim.get_army(army_id)
	if int(marching.get("destination_index", -1)) != prag:
		_fail("Rechtsklick auf Prag: kein Marschbefehl (%s)" % str(marching))
	await _key(KEY_Z)
	if int(_main.call("map_mode")) != MapModes.MAP_KRIEG or not layer.show_garrisons:
		_fail("Taste Y schaltet nicht in den Modus Krieg mit Garnisonen (%d)" % int(_main.call("map_mode")))
	# Heeresfenster (seit b3c): Stärke, Moral, Versorgung, Befehlshaber, Ziel; Generalstab.
	var army_panel: ArmyPanel = _main.call("army_panel")
	var army_text := army_panel.panel_text()
	if not army_panel.visible or army_panel.army_id() != army_id or not army_text.contains("Prag") \
			or not army_text.contains("Moral") or not army_text.contains("Befehlshaber"):
		_fail("Heeresfenster der I. Armee: sichtbar %s, Text „%s“" % [army_panel.visible, army_text])
	army_panel.staff_button().pressed.emit()
	var pending := army_panel.staff_button().disabled
	sim.advance_days(1)
	_main.call("_after_time_passed")
	if not pending or not bool(sim.get_army(army_id).get("staff", false)) \
			or army_panel.staff_button().text != "Selbst führen":
		_fail("„Dem Generalstab übergeben“: gesperrt %s, Generalstab %s, Knopf „%s“" % [
			pending, sim.get_army(army_id).get("staff", false), army_panel.staff_button().text
		])
	await _key(KEY_ESCAPE)
	if int(_main.call("selected_army")) != -1 or army_panel.visible:
		_fail("Escape hebt die Auswahl des Heeres nicht auf")
	# Staatspanel: Heer unter Waffen, im Krieg keine Abrüstung; „Krieg …“ öffnet das Kriegsfenster.
	var state_panel: StatePanel = _main.call("state_panel")
	var military := state_panel.military_text()
	if not military.contains(" Mann · mobil") or not state_panel.mobilize_button().disabled:
		_fail("Staatspanel im Krieg: „%s“" % military)
	(state_panel.find_child("WarButton", true, false) as Button).pressed.emit()
	await _frames(1)
	var war_window: WarWindow = _main.call("war_window")
	var war_text := war_window.window_text()
	if not war_window.is_open() or not war_text.contains("Kriegspunkte") \
			or not war_window.peace_button(SimulationClient.PEACE_WHITE).text.contains("Österreich"):
		_fail("Kriegsfenster: offen %s, Text „%s“" % [war_window.is_open(), war_text])
	war_window.show_tab(1)
	var row := war_window.army_button(army_id)
	if row == null:
		_fail("Kriegsfenster, Reiter Heere: I. Armee fehlt")
	else:
		row.pressed.emit()
		if int(_main.call("selected_army")) != army_id:
			_fail("Kriegsfenster, Reiter Heere: Klick wählt die I. Armee nicht")
	await _key(KEY_ESCAPE)
	if war_window.is_open():
		_fail("Escape schließt das Kriegsfenster nicht")
	_main.call("select_army", -1)
	# Schlachtbericht: Tage bis zur ersten Schlacht mit einem preußischen Heer.
	var reports: BattleReports = _main.call("battle_reports")
	var days := 0
	while reports.report_count() == 0 and days < 60:
		sim.advance_days(1)
		_main.call("_after_time_passed")
		days += 1
	var report := reports.reports_text()
	if reports.report_count() == 0 or not report.contains(" bei ") or not report.contains("Schlacht bei"):
		_fail("Kein Schlachtbericht in %d Tagen („%s“)" % [days, report])
	# Kriegserklärung aus dem Fenster „Diplomatie“: Rückfrage mit Seiten und Ziel, Krieg am nächsten Tag.
	var enemy := -1
	for state in sim.get_state_count():
		if state != player and not sim.get_declaration_goals(player, state).is_empty():
			enemy = state
			break
	_main.call("open_diplomacy")
	var diplomacy: DiplomacyWindow = _main.call("diplomacy_window")
	diplomacy.select_state(enemy)
	var declare := diplomacy.find_child("DeclareWar", true, false) as Button
	var wars_before := sim.get_wars().size()
	if enemy < 0 or declare == null or declare.disabled:
		_fail("Diplomatie: „Krieg erklären …“ fehlt oder gesperrt (Ziel %d)" % enemy)
	else:
		declare.pressed.emit()
		var question := diplomacy.window_text()
		(diplomacy.find_child("WarConfirm", true, false) as Button).pressed.emit()
		sim.advance_days(1)
		_main.call("_after_time_passed")
		if not question.contains("den Krieg erklären?") or sim.get_wars().size() != wars_before + 1:
			_fail("Kriegserklärung: Rückfrage „%s“, Kriege %d statt %d" % [
				question, sim.get_wars().size(), wars_before + 1
			])
	_main.call("close_diplomacy")
	var enemy_name := str(sim.get_state(enemy).get("short_name", ""))
	var markers := layer.marker_count()
	# Düppel als Dänemark (Review M10): Verliert der Spielerstaat alle Orte, hält die Zeit an und ein
	# Hinweis nennt den Weg zu einem anderen Staat.
	_main.call("start_new_game", "dueppel", "DEN")
	await _frames(1)
	sim = _main.call("simulation")
	var denmark := sim.get_player_state_index()
	var war_days := 0
	while int(sim.get_state(denmark).get("location_count", 1)) > 0 and war_days < 200:
		sim.advance_days(1)
		_main.call("_after_time_passed")
		war_days += 1
	if not bool(_main.call("player_gone_noticed")) or int(_main.call("speed")) != 0:
		_fail("Dänemark ohne Orte nach %d Tagen: kein Hinweis oder die Zeit läuft" % war_days)
	for button in _main.find_child("NoticeLayer", true, false).find_children("*", "Button", true, false):
		if (button as Button).text == "Verstanden":
			(button as Button).pressed.emit()
	await _frames(1)
	print(
		"  Kriegslesezeichen Düppel, Königgrätz, Sedan im Menü; Königgrätz: %d Heeresmarken, I. Armee per Klick gewählt, Marsch nach Prag per Rechtsklick, "
		% markers
		+ "Heeresfenster mit Generalstab, Kriegsfenster, Schlachtbericht nach %d Tagen, Kriegserklärung an %s; "
		% [days, enemy_name]
		+ "Düppel: Dänemark nach %d Tagen ohne Orte, Hinweis" % war_days
	)


## Die drei Kriegslesezeichen durchspielen (Abnahme M10, militaer.md Abschnitt 9): als Preußen das
## stärkste eigene Feldheer per Rechtsklick zur Hauptstadt des Gegners schicken, im Kriegsfenster ein
## Friedensangebot senden, das der Gegner laut Vorab-Prüfung annimmt; am nächsten Tag ist der Krieg aus.
func _check_war_bookmarks() -> void:
	var lines := PackedStringArray()
	for bookmark in ["dueppel", "koeniggraetz", "sedan"]:
		_main.call("start_new_game", bookmark, "PRU")
		await _frames(1)
		for button in _main.find_child("NoticeLayer", true, false).find_children("*", "Button", true, false):
			if (button as Button).text == "Verstanden":
				(button as Button).pressed.emit()
		var sim: SimulationClient = _main.call("simulation")
		var player := sim.get_player_state_index()
		var wars := sim.get_wars()
		if wars.is_empty():
			_fail("%s: kein Krieg zum Start" % bookmark)
			continue
		var war: Dictionary = wars[0]
		var enemies: Variant = war.get("defenders" if int(war.get("player_side", 0)) > 0 else "attackers", [])
		var enemy_capital := int(sim.get_state(int((enemies as Variant)[0])).get("capital_index", -1))
		# Marsch: das stärkste Feldheer mit einem Weg zur feindlichen Hauptstadt.
		_main.call("_select_strongest_army")
		var army: Dictionary = _main.call("selected_army_data")
		var view: MapView = _main.call("map_view")
		view.camera.focus_on(view.location_center(enemy_capital), view.camera.overview_zoom * 2.0)
		await _frames(2)
		await _right_click(view.camera.world_to_screen(view.location_center(enemy_capital)))
		sim.advance_days(1)
		_main.call("_after_time_passed")
		var marching := sim.get_army(int(army.get("id", -1)))
		if army.is_empty() or (marching.get("route", PackedInt32Array()) as PackedInt32Array).is_empty():
			_fail("%s: %s marschiert nicht zur Hauptstadt des Gegners (%s)" % [
				bookmark, str(army.get("name", "–")), str(marching.get("status_text", ""))
			])
		# Frieden: das erste Angebot, das der Gegner annähme.
		_main.call("open_war_window")
		var window: WarWindow = _main.call("war_window")
		window.select_war(int(war.get("id", -1)))
		var sent := ""
		for terms in [SimulationClient.PEACE_OWN_GOALS, SimulationClient.PEACE_WHITE, SimulationClient.PEACE_CAPITULATION]:
			var button := window.peace_button(terms)
			if not button.disabled and button.text.contains("nähme an"):
				sent = button.text
				button.pressed.emit()
				break
		sim.advance_days(1)
		_main.call("_after_time_passed")
		var still := false
		for w in sim.get_wars():
			still = still or int(w.get("id", -1)) == int(war.get("id", -1))
		if sent.is_empty() or still:
			_fail("%s: Frieden nicht geschlossen (Angebot „%s“, Krieg läuft %s)" % [bookmark, sent, still])
		_main.call("close_war_window")
		lines.append("%s %s, %s" % [
			str(sim.get_scenario_name()), str(army.get("name", "–")), sent.get_slice(" – ", 0)
		])
	print("  Kriegslesezeichen durchgespielt (Marsch, Frieden): " + "; ".join(lines))


## Staatsinfo und Parteienmodus (seit M8): „Verfassung und Gruppen …“ im Staatspanel; Rolle im
## Spielmenü, Parteipanel statt Staatspanel, Wahlkampf über den Knopf in der Ortsinfo (Wahl oben,
## kein „Diesen Staat spielen“), Mittel am nächsten Tag weniger, zurück zum Staat über das Parteipanel.
func _check_party_mode(sim: SimulationClient) -> void:
	var currents := sim.get_currents()
	var liberals := -1
	for i in currents.size():
		if str(currents[i].get("id", "")) == "liberale":
			liberals = i
	sim.advance_days(1) # Spenden am Monatsersten
	_main.call("_after_time_passed")
	# Staatsinfo (seit M8): „Verfassung und Gruppen …“ zeigt Verfassung, Wahlrecht und Gruppen.
	var state_panel: StatePanel = _main.call("state_panel")
	var politics := state_panel.find_child("PoliticsButton", true, false) as Button
	if politics == null or not politics.visible:
		_fail("Staatspanel 1848: Knopf „Verfassung und Gruppen“ fehlt")
	else:
		politics.pressed.emit()
		await _frames(1)
		var text := state_panel.politics_text()
		var shown := state_panel.is_politics_visible()
		if not shown or not text.contains("Verfassung:") or not text.contains("Bauernschaft"):
			_fail("Verfassung und Gruppen: offen %s, Text „%s“" % [shown, text])
		(state_panel.find_child("PoliticsPopup", true, false) as PopupPanel).hide()
	var menu: GameMenu = _main.call("game_menu")
	_main.call("open_game_menu")
	menu.select_role(liberals)
	_main.call("close_game_menu", false)
	await _frames(1)
	var party_panel: PartyPanel = _main.call("party_panel")
	var bar: TopBar = _main.call("top_bar")
	if not party_panel.visible or state_panel.visible or sim.get_player_current() != liberals:
		_fail("Parteienmodus: Parteipanel %s, Staatspanel %s, Strömung %d statt %d" % [
			party_panel.visible, state_panel.visible, sim.get_player_current(), liberals
		])
		return
	if not party_panel.facts_text().contains("Taler") or not bar.speed_line_text().contains("Liberale"):
		_fail("Parteienmodus: Angaben „%s“, Meldung „%s“" % [party_panel.facts_text(), bar.speed_line_text()])
	_main.call("select_location", sim.get_location_index("mannheim"))
	await _frames(1)
	var panel: LocationPanel = _main.call("location_panel")
	var rally := panel.find_child("Campaign%d" % SimulationClient.CAMPAIGN_VERSAMMLUNG, true, false) as Button
	var play := panel.find_child("PlayState", true, false) as Button
	var section := panel.find_child("Election", true, false) as Control
	var facts := panel.find_child("Facts", true, false) as Control
	if rally == null or rally.disabled or play.visible or section.get_index() > facts.get_index():
		_fail("Ortsinfo Mannheim im Parteienmodus: Versammlung %s, Staat spielen %s, Wahl oben %s" % [
			rally != null and not rally.disabled, play.visible, section.get_index() < facts.get_index()
		])
		return
	var funds := float(sim.get_party(liberals).get("funds", 0.0))
	# Seit M12 hat die KI für die Liberalen bis zur Übernahme schon Wahlkampf geführt (Monatserster).
	var before := int(sim.get_party(liberals).get("campaign_districts", 0))
	rally.pressed.emit()
	await _frames(1)
	if not bar.speed_line_text().contains("Versammlung"):
		_fail("Wahlkampf: keine Meldung (%s)" % bar.speed_line_text())
	sim.advance_days(1)
	_main.call("_after_time_passed")
	await _frames(1)
	var party := sim.get_party(liberals)
	var districts := int(party.get("campaign_districts", 0))
	if (
		float(party.get("funds", 0.0)) >= funds or districts < maxi(before, 1)
		or not party_panel.facts_text().contains("in %d Wahlkreis" % districts)
	):
		_fail("Wahlkampf am nächsten Tag: %s, Panel „%s“" % [str(party), party_panel.facts_text()])
	# Der Knopf bleibt derselbe, damit ein Klick bei laufender Zeit nicht verloren geht (Review M8).
	if not is_instance_valid(rally) or not rally.is_inside_tree() or not rally.is_visible_in_tree():
		_fail("Wahlkampf: Knopf „Versammlung“ nach dem Tageswechsel neu angelegt oder verborgen")
	var leave := party_panel.find_child("LeaveParty", true, false) as Button
	leave.pressed.emit()
	await _frames(1)
	if (
		party_panel.visible or not state_panel.visible or sim.get_player_current() != -1
		or (panel.find_child("Campaign", true, false) as Control).visible
		or section.get_index() < facts.get_index()
	):
		_fail("Zurück zum Staat: Parteipanel %s, Staatspanel %s, Strömung %d" % [
			party_panel.visible, state_panel.visible, sim.get_player_current()
		])
	print("  Parteienmodus: Rolle im Menü, Versammlung in Mannheim (%s → %s Taler), zurück zum Staat" % [
		Format.integer(roundi(funds)), Format.integer(roundi(float(party.get("funds", 0.0))))
	])


## Zollunion (seit M9, diplomatie.md 3.7): Taste Ö mit Legende, gesperrter Zollregler eines
## Mitglieds, Zollgebiet in der Ortsinfo; Hannover tritt über das Fenster „Zollunion“ mit Rückfrage
## dem Zollverein bei, der Antrag steht am nächsten Tag im Fenster, im Hinweis und in der Chronik und
## wird wieder zurückgezogen.
func _check_customs_ui(sim: SimulationClient) -> void:
	await _key(KEY_SEMICOLON)
	var legend := _texts(_main.get("_legend"))
	if int(_main.call("map_mode")) != MapModes.MAP_ZOLLGEBIETE or not legend.contains("Zollverein (26 Staaten)"):
		_fail("Taste Ö: Modus %d, Legende „%s“" % [int(_main.call("map_mode")), legend.left(160)])
	var state_panel: StatePanel = _main.call("state_panel")
	var sliders := state_panel.find_children("*", "HSlider", true, false)
	var slider := sliders[SimulationClient.TAX_ZOLL] as HSlider if sliders.size() == 3 else null
	if slider == null or slider.editable or roundi(slider.value) != 10:
		_fail("Zollregler eines Zollvereinsmitglieds nicht auf 10 %% gesperrt (%s)" % [
			"fehlt" if slider == null else "%s, frei %s" % [slider.value, slider.editable]
		])
	_main.call("select_location", sim.get_location_index("hamburg"))
	await _frames(1)
	var location: LocationPanel = _main.call("location_panel")
	var zone := location.find_child("Customs", true, false) as Label
	if zone == null or not zone.visible or not zone.text.begins_with("Hamburg (Zoll"):
		_fail("Ortsinfo Hamburg: Zollgebiet „%s“" % (zone.text if zone != null else "–"))
	_main.call("select_location", -1)

	var hanover := sim.get_state_index("HAN")
	_main.call("play_state", hanover)
	await _frames(1)
	var button := state_panel.find_child("CustomsButton", true, false) as Button
	if button == null or not button.visible:
		_fail("Staatspanel Hannover: Knopf „Zollunion …“ fehlt")
		return
	button.pressed.emit()
	await _frames(1)
	var text := state_panel.customs_text()
	if (
		not state_panel.is_customs_visible() or not text.contains("Zollgebiet: Steuerverein (Leitmacht)")
		or not text.contains("[Beitreten …]") or not text.contains("[Austreten …]")
	):
		_fail("Fenster Zollunion (Hannover): offen %s, „%s“" % [state_panel.is_customs_visible(), text])
		return
	var effective := str(sim.get_customs_change(hanover, 0).get("effective_text", ""))
	(state_panel.find_child("CustomsAction0", true, false) as Button).pressed.emit()
	await _frames(1)
	text = state_panel.customs_text()
	var question := "Hannover verlässt zum %s den Steuerverein und tritt dem Zollverein bei." % effective
	if not text.contains(question) or not text.contains("löst sich der Steuerverein dann auf"):
		_fail("Rückfrage zum Beitritt: „%s“" % text)
		return
	(state_panel.find_child("CustomsConfirm", true, false) as Button).pressed.emit()
	await _frames(1)
	var join := state_panel.find_child("CustomsAction0", true, false) as Button
	if not join.disabled or join.text != "beantragt" or not state_panel.customs_text().contains("nächsten Tag"):
		_fail("Nach dem Antrag: Knopf „%s“ (gesperrt %s)" % [join.text, join.disabled])
	sim.advance_days(1)
	_main.call("_after_time_passed")
	state_panel.show_customs()
	await _frames(1)
	text = state_panel.customs_text()
	if text.contains("nächsten Tag hinaus"):
		_fail("Hinweis „geht mit dem nächsten Tag hinaus“ bleibt nach dem Tageswechsel stehen")
	var hint := (state_panel.find_child("TaxHint", true, false) as Label).text
	var entry := false
	for item in sim.get_chronicle(0):
		entry = entry or str(item["text"]).contains(question.trim_suffix("."))
	var request := "Beitritt zum Zollverein beantragt, wirksam zum %s" % effective
	if not text.contains("Antrag: " + request) or not hint.begins_with(request) or not entry:
		_fail("Antrag am nächsten Tag: Fenster „%s“, Hinweis „%s“, Chronik %s" % [text, hint, entry])
		return
	(state_panel.find_child("CustomsWithdraw", true, false) as Button).pressed.emit()
	await _frames(1)
	if not state_panel.customs_text().contains("Den Antrag zurückziehen? Hannover bleibt im Steuerverein."):
		_fail("Rückfrage zur Rücknahme: „%s“" % state_panel.customs_text())
	(state_panel.find_child("CustomsConfirm", true, false) as Button).pressed.emit()
	sim.advance_days(1)
	_main.call("_after_time_passed")
	if not sim.get_state(hanover).get("customs_request", {}).is_empty():
		_fail("Rücknahme: Antrag bleibt bestehen (%s)" % str(sim.get_state(hanover).get("customs_request")))
	(state_panel.find_child("CustomsPopup", true, false) as PopupPanel).hide()
	_main.call("set_map_mode", MapModes.MAP_POLITICAL)
	print("  Zollunion: Taste Ö, Zollregler gesperrt, Hannover beantragt den Beitritt zum %s und zieht ihn zurück" % effective)


## Diplomatie (seit M9, Schritt b3, diplomatie.md Abschnitt 6): Taste # mit Legende; Preußen
## öffnet das Fenster „Diplomatie“ über das Staatspanel, entsendet einen Gesandten nach Bayern
## (Posten „Gesandte“ im Haushalt), fordert von Österreich die Führung im Bund (mit Rückfrage), sieht
## die Krisenleiste über dem Fenster und gibt nach; ein Antrag in der Bundesversammlung; Escape schließt.
func _check_diplomacy_ui(sim: SimulationClient) -> void:
	await _key(KEY_BACKSLASH)
	var legend := _texts(_main.get("_legend"))
	if int(_main.call("map_mode")) != MapModes.MAP_BEZIEHUNGEN or not legend.contains("freundlich ("):
		_fail("Taste #: Modus %d, Legende „%s“" % [int(_main.call("map_mode")), legend.left(160)])
	var prussia := sim.get_state_index("PRU")
	var austria := sim.get_state_index("AUT")
	var bavaria := sim.get_state_index("BAV")
	_main.call("play_state", prussia)
	await _frames(1)
	var state_panel: StatePanel = _main.call("state_panel")
	var window: DiplomacyWindow = _main.call("diplomacy_window")
	var bar: PlayBar = _main.call("play_bar")
	var button := state_panel.find_child("DiplomacyButton", true, false) as Button
	if button == null or not button.visible:
		_fail("Staatspanel Preußen: Knopf „Diplomatie …“ fehlt")
		return
	button.pressed.emit()
	await _frames(2)
	# Seit M12 kann Preußen vor der Übernahme schon Gesandte entsandt haben (KI).
	var envoys := int(sim.get_diplomacy(prussia).get("envoy_count", 0))
	if (
		not window.is_open() or window.current_tab() != 0
		or not window.window_text().contains("Gesandte %d von" % envoys)
	):
		_fail("Fenster Diplomatie: offen %s, „%s“" % [window.is_open(), window.window_text().left(200)])
		return
	# Gesandter nach Bayern.
	(window.find_child("State%d" % bavaria, true, false) as Button).pressed.emit()
	await _frames(1)
	var send := window.find_child("Action_envoy_send", true, false) as Button
	if window.selected_state() != bavaria or send == null or send.disabled:
		_fail("Bayern gewählt: %d, Knopf Gesandter %s" % [window.selected_state(), "fehlt" if send == null else "gesperrt"])
		return
	send.pressed.emit()
	await _frames(1)
	if not window.window_text().contains("Gesandter nach Bayern entsandt"):
		_fail("Gesandter: keine Rückmeldung („%s“)" % window.window_text().right(300))
	sim.advance_days(1)
	_main.call("_after_time_passed")
	window.refresh(true)
	if not window.window_text().contains("unser Gesandter"):
		_fail("Gesandter am nächsten Tag nicht in der Liste („%s“)" % window.window_text().left(300))
	# KI (seit M12, ki.md 9): Absichten Bayerns mit eigenem Gesandten, „Warum?“ an einer Entscheidung der KI,
	# Regierung Preußens in der Staatsinfo.
	window.select_state(bavaria)
	await _frames(1)
	if not window.ai_text().contains("Absichten: Hauptziel"):
		_fail("Absichten mit Gesandtem in Bayern: „%s“" % window.ai_text())
	var deciding := -1
	for state in sim.get_state_count():
		if state != prussia and not sim.get_ai_decisions(state, 1).is_empty() and window.has_state(state):
			deciding = state
			break
	if deciding < 0:
		_fail("Keine Entscheidung der KI bis hierher")
	else:
		window.select_state(deciding)
		await _frames(1)
		var ai := window.ai_text()
		if not ai.contains("Warum?") or not ai.contains("Gründe: "):
			_fail("„Warum?“ an einer Entscheidung der KI: „%s“" % ai)
	state_panel.show_state(sim.get_state(prussia), "")
	var government := state_panel.government_text()
	if not government.contains("König: Friedrich Wilhelm IV.") or not government.contains("Neigungen (Machtpolitiker)"):
		_fail("Staatsinfo Preußen ohne Regierung: „%s“" % government)
	state_panel.show_budget()
	await _frames(1)
	if not state_panel.budget_text().contains("Gesandte"):
		_fail("Haushalt Preußens ohne Posten „Gesandte“")
	state_panel.hide_popups()
	# Forderung an Österreich mit Rückfrage; die Krise steht am nächsten Tag in der Leiste.
	window.select_state(austria)
	(window.find_child("Action_demand_leadership", true, false) as Button).pressed.emit()
	await _frames(1)
	var question := window.find_child("DemandQuestion", true, false) as Control
	if not question.visible or not window.window_text().contains("Führung im Bund fordern? Das löst eine Krise aus"):
		_fail("Rückfrage zur Forderung: „%s“" % window.window_text().right(400))
		return
	(window.find_child("DemandConfirm", true, false) as Button).pressed.emit()
	await _frames(1)
	sim.advance_days(1)
	_main.call("_after_time_passed")
	await _frames(2)
	if not bar.visible or not bar.bar_text().contains("KRISE: Preußen fordert von Österreich: Führung im Bund"):
		_fail("Krisenleiste: sichtbar %s, „%s“" % [bar.visible, bar.bar_text()])
		return
	var modes_top := (_main.get("_mode_bar_area") as Control).get_global_rect().position.y
	if window.get_global_rect().intersects(bar.get_global_rect()) or window.get_global_rect().end.y > modes_top:
		_fail("Fenster %s, Krisenleiste %s und Leiste der Kartenmodi (ab %s) überlappen" % [
			window.get_global_rect(), bar.get_global_rect(), modes_top
		])
	var give_in := bar.find_child("Yield", true, false) as Button
	if give_in == null or not give_in.is_visible_in_tree():
		_fail("Krisenleiste ohne Knopf „Nachgeben“")
		return
	give_in.pressed.emit()
	await _frames(1)
	if not bar.bar_text().contains("Wir geben nach"):
		_fail("Nachgeben: keine Rückmeldung („%s“)" % bar.bar_text())
	sim.advance_days(1)
	_main.call("_after_time_passed")
	await _frames(1)
	if bar.visible or (sim.get_german_question().get("play_results", []) as Array).size() != 1:
		_fail("Nach dem Nachgeben: Leiste sichtbar %s, Ergebnisse %s" % [bar.visible, sim.get_german_question().get("play_results")])
	# Antrag auf eine Bundesreform im Reiter „Bundesversammlung“.
	(window.find_child("Tab1", true, false) as Button).pressed.emit()
	await _frames(1)
	var reform := window.find_child("Motion%d_-1" % SimulationClient.MOTION_REFORM, true, false) as Button
	if window.current_tab() != 1 or reform == null or reform.disabled:
		_fail("Reiter Bundesversammlung: %d, Antrag Bundesreform %s" % [window.current_tab(), "fehlt" if reform == null else "gesperrt"])
		return
	reform.pressed.emit()
	sim.advance_days(1)
	_main.call("_after_time_passed")
	window.refresh(true)
	if not window.window_text().contains("Offener Antrag von Preußen: Bundesreform"):
		_fail("Antrag am nächsten Tag nicht offen („%s“)" % window.window_text().right(400))
	await _key(KEY_ESCAPE)
	if window.is_open():
		_fail("Escape schließt das Fenster Diplomatie nicht")
	_main.call("set_map_mode", MapModes.MAP_POLITICAL)
	print("  Diplomatie: Taste #, Gesandter nach Bayern, Absichten Bayerns, „Warum?“ an einer Entscheidung der KI, Regierung Preußens, Krise um die Führung im Bund mit Nachgeben, Antrag Bundesreform")


## Wahlen 1848 (seit M8): Taste Ä, Wahlkreis in der Ortsinfo, Wahlabend am Wahltag mit Meldungen,
## Hochrechnung = Ergebnis am Ende, Telegramm zeigt den Wahlkreis, „Letzte Wahl“ im Spielmenü.
func _check_election_night(sim: SimulationClient) -> void:
	var night: ElectionNight = _main.call("election_night")
	var panel: LocationPanel = _main.call("location_panel")
	await _key(KEY_APOSTROPHE)
	if int(_main.call("map_mode")) != MapModes.MAP_WAHLEN:
		_fail("Taste Ä schaltet nicht in den Modus Wahlen (%d)" % int(_main.call("map_mode")))
	var mannheim := sim.get_location_index("mannheim")
	var before: Dictionary = sim.get_location(mannheim).get("wahl", {})
	if before.is_empty() or bool(before.get("held", true)):
		_fail("Ortsinfo Mannheim vor der Wahl: %s" % str(before))
	_answer_events()
	_main.call("set_speed", 5)
	for _frame in 3000:
		if night.is_open():
			break
		# Ereignisse halten die Zeit an (seit M11); hier gilt jeweils die erste mögliche Option.
		if _answer_events() > 0:
			_main.call("set_speed", 5)
		await process_frame
	if not night.is_open():
		_fail("Wahlabend öffnet nicht am Wahltag (%s)" % sim.get_date_text())
		return
	var date := sim.get_date()
	var left := _main.get_node("UI/Root/LeftColumn") as Control
	if (
		int(date.get("month", 0)) != 5 or int(date.get("day", 0)) != 2
		or int(_main.call("speed")) != 0 or left.visible
		or int(_main.call("map_mode")) != MapModes.MAP_WAHLEN
	):
		_fail(
			"Wahlabend: %s, Tempo %d, linke Spalte %s, Modus %d" % [
				sim.get_date_text(), int(_main.call("speed")), left.visible, int(_main.call("map_mode"))
			]
		)
	if night.reported_count() != 0 or night.district_count() != 633:
		_fail("Wahlabend: %d von %d gemeldet zu Beginn" % [night.reported_count(), night.district_count()])
	_main.call("select_location", mannheim)
	night.set_speed(1)
	for _frame in 20:
		await process_frame
	var partial := night.reported_count()
	if partial <= 0 or night.is_complete():
		_fail("Wahlabend: keine schrittweisen Meldungen (%d)" % partial)
	# Die Ortsinfo verrät das Ergebnis nicht vor der Meldung (Review M8).
	var mannheim_rank := night.rank_of_district(int(before.get("district_index", -1)))
	var shown := _texts(panel.find_child("Election", true, false))
	if mannheim_rank >= night.reported_count() and not shown.contains("noch nicht gemeldet"):
		_fail("Wahlabend: Ortsinfo Mannheim zeigt vor der Meldung „%s“" % shown)
	var election := sim.get_election(sim.get_election_count() - 1)
	night.finish()
	await _frames(1)
	var tally := night.current_tally()
	var counts: PackedInt32Array = tally.get("counts", PackedInt32Array())
	var projection: PackedFloat64Array = tally.get("projection", PackedFloat64Array())
	var seats: PackedInt32Array = election.get("seats_by_current", PackedInt32Array())
	var same := counts == seats
	for g in projection.size():
		same = same and is_equal_approx(projection[g], float(counts[g]))
	if not night.is_complete() or not same:
		_fail("Wahlabend: Ergebnis %s, Hochrechnung %s, Kern %s" % [counts, projection, seats])
	# Telegramm: der Wahlkreis erscheint in der Ortsinfo mit dem Ergebnis.
	night.district_requested.emit(mannheim)
	await _frames(2)
	var section := panel.find_child("Election", true, false) as Control
	var after: Dictionary = sim.get_location(mannheim).get("wahl", {})
	if section == null or not section.visible or not bool(after.get("held", false)):
		_fail("Ortsinfo Mannheim nach der Wahl ohne Ergebnis (%s)" % str(after))
	night.close_requested.emit()
	await _frames(1)
	_answer_events() # ein Ereignis am Wahltag hält die Zeit weiter an, bis es entschieden ist
	if night.is_open() or not left.visible or int(_main.call("speed")) != 5:
		_fail("Wahlabend schließen: offen %s, linke Spalte %s, Tempo %d" % [
			night.is_open(), left.visible, int(_main.call("speed"))
		])
	_main.call("set_speed", 0)
	# Spielmenü: „Letzte Wahl“ öffnet den Wahlabend mit dem ganzen Ergebnis.
	_main.call("open_game_menu")
	_main.call("open_last_election")
	await _frames(1)
	if not night.is_open() or not night.is_complete():
		_fail("Letzte Wahl: Wahlabend offen %s, vollständig %s" % [night.is_open(), night.is_complete()])
	night.close_requested.emit()
	await _frames(1)
	# Ab dem Fraktionsstichtag zeigt der Wahlabend acht Fraktionen und die Fraktionslosen: Er bleibt
	# im Bild, und die stärkste Fraktion ist keine „fraktionslos“ (Review M8).
	sim.advance_days(155)
	_main.call("open_game_menu")
	_main.call("open_last_election")
	await _frames(2)
	var bottom := night.get_global_rect().end.y
	var height := _main.get_viewport().get_visible_rect().size.y
	var status := _texts(night)
	if bottom > height + 0.5 or not status.contains("stärkste Fraktion") or status.contains("Fraktion: fraktionslos"):
		_fail("Wahlabend mit Fraktionen: unten bei %d von %d, „%s“" % [bottom, height, status])
	night.close_requested.emit()
	await _frames(1)
	print("  Wahlen 1848: Taste Ä, Wahlabend am 1. Mai (%d Meldungen schrittweise), Ergebnis = Hochrechnung" % partial)


## Offene Ereignisse im Ereignisfenster beantworten (seit M11): mit der bevorzugten Option aus
## preferred (Kennung des Ereignisses → Kennung der Option), sonst der Standardoption (größtes
## Gewicht, wie das Ereignislabor), sonst der ersten möglichen; Zahl der gewählten Optionen.
func _answer_events(preferred: Dictionary = {}) -> int:
	var window: EventWindow = _main.call("event_window")
	var answered := 0
	# Seit M13b1 hält auch die Zeitung die Zeit an: ein Klick legt sie beiseite (zählt wie eine Wahl).
	var paper: NewspaperWindow = _main.call("newspaper_window")
	if paper != null and paper.is_open():
		_newspapers_seen += 1
		_main.call("close_newspaper")
		answered += 1
	for _round in 20:
		if not window.is_open():
			break
		var event := window.current_event()
		var wanted := str(preferred.get(str(event.get("id", "")), ""))
		var standard := int(event.get("default_option", -1))
		var target: Button = null
		var target_rank := 0
		for option: Variant in event.get("options", []):
			var index := int((option as Dictionary).get("index", -1))
			var button := window.option_button(index)
			if button == null or button.disabled:
				continue
			var rank := 3 if str((option as Dictionary).get("id", "")) == wanted else (2 if index == standard else 1)
			if rank > target_rank:
				target = button
				target_rank = rank
		var chosen := target != null
		if chosen:
			target.pressed.emit()
			answered += 1
		else:
			_fail("Ereignisfenster ohne mögliche Option: %s" % window.window_text().left(300))
			break
	return answered


## Abnahme M11 (ereignisse.md Abschnitt 10): Als Baden die Märzforderungen mit dem Militär
## beantworten und einen Aufstand erleben; als Preußen die Krone annehmen (Wahlen der Strategie
## „krone“ des Ereignislabors) und den Ausgang „Reich der Paulskirche“ erreichen. Alle Wahlen gehen
## über das Ereignisfenster.
func _check_revolution_paths() -> void:
	_main.call("start_new_game", "voelkerfruehling", "BAD")
	var sim: SimulationClient = _main.call("simulation")
	var baden := sim.get_state_index("BAD")
	var revolt := ""
	for _day in 90:
		sim.advance_days(1)
		_main.call("_check_open_events")
		_answer_events({"maerz.forderungen": "militaer"})
		for entry in sim.get_revolts():
			if int(entry.get("state_index", -1)) == baden:
				revolt = "%s seit %s" % [entry.get("kind_text"), entry.get("since_text")]
		if not revolt.is_empty():
			break
	if revolt.is_empty() or revolt.contains("Ã"):
		_fail("Baden lehnt die Märzforderungen ab: Aufstand „%s“ bis %s" % [revolt, sim.get_date_text()])
	var revolt_shown := str(sim.get_state_events(baden).get("revolt", {}).get("kind_text", ""))
	if not revolt.is_empty() and revolt_shown.is_empty():
		_fail("Aufstand in Baden fehlt in get_state_events")
	_main.call("start_new_game", "voelkerfruehling", "PRU")
	sim = _main.call("simulation")
	var crown := {"pru.kaiserdeputation": "annehmen", "pru.barrikaden": "abziehen", "pru.dreiklassen": "behalten"}
	var outcome := {}
	var answered := 0
	for _day in 500:
		sim.advance_days(1)
		_main.call("_check_open_events")
		answered += _answer_events(crown)
		for journal in sim.get_journals():
			if str(journal.get("id", "")) == "revolution" and bool(journal.get("finished", false)):
				outcome = journal
		if not outcome.is_empty():
			break
	var accepted := false
	for entry in sim.get_event_log(400):
		if str(entry.get("title", "")).contains("Kaiserdeputation") and int(entry.get("state_index", -1)) == sim.get_state_index("PRU"):
			accepted = str(entry.get("option_text", "")).contains("Krone annehmen")
	if str(outcome.get("stage_id", "")) != "reich_paulskirche" or not accepted:
		_fail("Krone angenommen: Ausgang „%s“ am %s (Deputation im Protokoll: %s)" % [
			outcome.get("stage_title", "offen"), sim.get_date_text(), accepted
		])
	print("  Revolution: Baden mit dem Militär → Aufstand (%s); Preußen nimmt die Krone an → %s am %s (%d Wahlen im Fenster)" % [
		revolt, outcome.get("stage_title", "offen"), outcome.get("since_text", "–"), answered
	])


## Ereignisse und Journale (seit M11, Schritt b4): Das Ereignisfenster hält die Zeit an, das Tempo
## bleibt gesperrt, die Wahl schließt es und setzt das Tempo fort; Taste J öffnet die Journale, das
## Spielmenü die bisherigen Ereignisse; Kartenmodus Unruhe (Taste X); die ruhende Bundesversammlung.
func _check_events_ui(sim: SimulationClient) -> void:
	var window: EventWindow = _main.call("event_window")
	var player := sim.get_player_state_index()
	if sim.get_event_player() != player:
		_fail("Ereignisspieler %d statt des Spielerstaates %d" % [sim.get_event_player(), player])
	_answer_events()
	_main.call("set_speed", 5)
	var paper: NewspaperWindow = _main.call("newspaper_window")
	# Seit M14a1 bis zu einem Spieldatum statt einer festen Zahl von Bildern: Ohne Last laufen die Bilder so
	# schnell, dass 3.000 Bilder nur gut ein halbes Spieljahr waren (Test schlug je nach Rechner fehl).
	var limit := ElectionTally.day_number({"year": 1850, "month": 1, "day": 1})
	var night: ElectionNight = _main.call("election_night")
	for _frame in 200000:
		if window.is_open() or ElectionTally.day_number(sim.get_date()) >= limit:
			break
		if paper.is_open(): # seit M13b1: die Zeitung hält die Zeit an
			_newspapers_seen += 1
			_main.call("close_newspaper")
			_main.call("set_speed", 5)
		if night.is_open(): # ein Wahlabend (etwa die Zweite Kammer 1849) hält die Zeit ebenfalls an
			_main.call("_close_election_night", false)
			_main.call("set_speed", 5)
		elif int(_main.call("speed")) == 0: # Pausenregeln (Krieg, Forderung …)
			_main.call("set_speed", 5)
		await process_frame
	if not window.is_open():
		_fail("Kein Ereignis für %s bis %s" % [sim.get_state(player).get("short_name"), sim.get_date_text()])
		_main.call("set_speed", 0)
		return
	var text := window.window_text()
	if int(_main.call("speed")) != 0 or not text.contains("Quelle:") or window.option_button(0) == null:
		_fail("Ereignisfenster: Tempo %d, „%s“" % [int(_main.call("speed")), text.left(300)])
	_main.call("set_speed", 3)
	var bar: TopBar = _main.call("top_bar")
	if int(_main.call("speed")) != 0 or not bar.speed_line_text().contains("Erst das Ereignis entscheiden"):
		_fail("Tempo bei offenem Ereignis: %d, Meldung „%s“" % [int(_main.call("speed")), bar.speed_line_text()])
	var event := window.current_event()
	var index := int(event.get("event_index", -1))
	var answered := _answer_events()
	# Gleich nach der Wahl, vor dem nächsten Bild: Danach läuft die Zeit und das Ereignis ist entschieden.
	var chosen := false
	for open in sim.get_open_events():
		chosen = chosen or (int(open.get("event_index", -1)) == index and int(open.get("chosen_option", -1)) >= 0)
	if window.is_open() or not chosen or int(_main.call("speed")) != 3:
		_fail("Nach der Wahl: Fenster offen %s, gewählt %s, Tempo %d" % [window.is_open(), chosen, int(_main.call("speed"))])
	_main.call("set_speed", 0)
	_answer_events()
	# Journale: Taste J, Reiter je Journal und „Bisherige Ereignisse“, Escape schließt.
	var journal: JournalWindow = _main.call("journal_window")
	await _key(KEY_J)
	var journal_text := journal.window_text()
	if not journal.is_open() or journal.tab_count() < 3 or not journal_text.contains("Stufe „"):
		_fail("Taste J: Journal offen %s, %d Reiter, „%s“" % [journal.is_open(), journal.tab_count(), journal_text.left(300)])
	await _key(KEY_ESCAPE)
	if journal.is_open():
		_fail("Escape schließt das Journalfenster nicht")
	var menu: GameMenu = _main.call("game_menu")
	_main.call("open_game_menu")
	var events_button := menu.find_child("EventsButton", true, false) as Button
	if events_button == null or not events_button.visible:
		_fail("Spielmenü ohne Knopf „Ereignisse“")
	else:
		events_button.pressed.emit()
		await _frames(1)
		if menu.is_open() or not journal.is_open() or not journal.is_event_log_shown() or not journal.window_text().contains(" · "):
			_fail("Spielmenü „Ereignisse“: Menü %s, Journal %s, „%s“" % [menu.is_open(), journal.is_open(), journal.window_text().left(300)])
	_main.call("close_journal")
	# Kartenmodus Unruhe (Taste X) mit Legende.
	await _key(KEY_X)
	var legend := _texts(_main.get("_legend"))
	if int(_main.call("map_mode")) != MapModes.MAP_UNRUHE or not legend.contains("ruhig"):
		_fail("Taste X: Modus %d, Legende „%s“" % [int(_main.call("map_mode")), legend.left(200)])
	# Kartenmodus Bund (seit M13, Taste C): der Deutsche Bund unter Österreich.
	await _key(KEY_C)
	legend = _texts(_main.get("_legend"))
	if int(_main.call("map_mode")) != MapModes.MAP_BUND or not legend.contains("Präsidium: Österreich") or not legend.contains("Deutscher Bund"):
		_fail("Taste C: Modus %d, Legende „%s“" % [int(_main.call("map_mode")), legend.left(200)])
	_main.call("set_map_mode", MapModes.MAP_POLITICAL)
	# Ab Juli 1848 ruht die Bundesversammlung: Das Fenster sagt es, Anträge sind gesperrt.
	if bool(sim.get_diet().get("suspended", false)):
		_main.call("open_diplomacy", 1)
		await _frames(1)
		var diplomacy: DiplomacyWindow = _main.call("diplomacy_window")
		var reform := diplomacy.find_child("Motion%d_-1" % SimulationClient.MOTION_REFORM, true, false) as Button
		if not diplomacy.window_text().contains("Die Bundesversammlung ruht") or reform == null or not reform.disabled:
			_fail("Ruhende Bundesversammlung: „%s“" % diplomacy.window_text().left(400))
		_main.call("close_diplomacy")
	else:
		_fail("Die Bundesversammlung tagt noch am %s" % sim.get_date_text())
	print("  Ereignisse: „%s“ hält die Zeit an (%d Optionen gewählt), Taste J, Spielmenü „Ereignisse“, Taste X" % [
		str(event.get("title", "")), answered
	])


## Texte aller Labels unter node (Tests).
func _texts(node: Node) -> String:
	var parts := PackedStringArray()
	if node == null:
		return ""
	for label in node.find_children("*", "Label", true, false):
		parts.append((label as Label).text)
	return " ".join(parts)


## Taste drücken und loslassen (echtes Eingabeereignis mit physischem Tastencode).
func _key(code: Key) -> void:
	for pressed: bool in [true, false]:
		var event := InputEventKey.new()
		event.physical_keycode = code
		event.keycode = code
		event.pressed = pressed
		Input.parse_input_event(event)
		await process_frame
	await process_frame


func _clear_saves(dir: String) -> void:
	var absolute := SaveSlots.absolute_dir(dir)
	var access := DirAccess.open(absolute)
	if access == null:
		return
	for file in access.get_files():
		access.remove(file)
	DirAccess.remove_absolute(absolute)


## Kein gezeichneter Name liegt unter einem sichtbaren Bedienelement.
func _check_labels_free(labels: MapLabels, situation: String) -> void:
	var origin := labels.get_global_rect().position
	var covered := 0
	var example := ""
	for entry: Dictionary in labels.drawn_labels:
		var rect: Rect2 = entry["rect"]
		rect.position += origin
		for control in labels.covering_controls:
			if control.is_visible_in_tree() and control.get_global_rect().intersects(rect.grow(-1.0)):
				covered += 1
				example = str(entry["text"])
				break
	if covered > 0:
		_fail("Namen: %d Namen unter Panels %s (z. B. %s)" % [covered, situation, example])


func _drawn_with_style(labels: MapLabels, text: String, style: int) -> bool:
	for entry: Dictionary in labels.drawn_labels:
		if str(entry["text"]) == text and int(entry["style"]) == style:
			return true
	return false


func _frames(count: int) -> void:
	for _frame in count:
		await process_frame


## Linksklick an einem Punkt in Viewport-Koordinaten. Ereignisse kommen in
## Fensterkoordinaten an; Godot rechnet sie mit der Streckung (canvas_items) in den
## Viewport um. Headless ist das Fenster 64 × 64 Pixel groß, der Viewport dann
## 1600 × 1600; unter xvfb sind beide 1600 × 900.
func _click(position: Vector2) -> void:
	var in_window := root.get_final_transform() * position
	for pressed: bool in [true, false]:
		var event := InputEventMouseButton.new()
		event.button_index = MOUSE_BUTTON_LEFT
		event.pressed = pressed
		event.position = in_window
		event.global_position = in_window
		if pressed:
			event.button_mask = MOUSE_BUTTON_MASK_LEFT
		Input.parse_input_event(event)
		# Eingaben werden einmal je Bild ausgeliefert.
		await process_frame


## Rechtsklick ohne Ziehen an einem Punkt in Viewport-Koordinaten (seit M10, Marschbefehl).
func _right_click(position: Vector2) -> void:
	var in_window := root.get_final_transform() * position
	for pressed: bool in [true, false]:
		var event := InputEventMouseButton.new()
		event.button_index = MOUSE_BUTTON_RIGHT
		event.pressed = pressed
		event.position = in_window
		event.global_position = in_window
		if pressed:
			event.button_mask = MOUSE_BUTTON_MASK_RIGHT
		Input.parse_input_event(event)
		await process_frame


## true, wenn ein Bedienelement über der Karte den Bildschirmpunkt verdeckt.
func _covered(screen_point: Vector2) -> bool:
	var labels: MapLabels = _main.call("map_labels")
	for control in labels.covering_controls:
		if control.is_visible_in_tree() and control.get_global_rect().grow(2.0).has_point(
			screen_point
		):
			return true
	return false


func _describe(sim_index: int) -> String:
	if sim_index < 0:
		return "keinen Ort"
	var map_index := _map.map_index_of_sim(sim_index)
	if map_index < 0:
		return "Simulationsindex %d (nicht auf der Karte)" % sim_index
	return "„%s“" % _map.map_ids[map_index]


func _fail(message: String) -> void:
	_failure_count += 1
	if _failures.size() < MAX_REPORTED:
		_failures.append(message)
