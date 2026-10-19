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

var _main: Node = null
var _view: MapView = null
var _map: MapData = null
var _sprite: Sprite2D = null
var _failures := PackedStringArray()
var _failure_count := 0


func _initialize() -> void:
	_run()


func _run() -> void:
	print("EINIGKEIT – Klicktest der Kartenansicht")
	_main = (load(MAIN_SCENE) as PackedScene).instantiate()
	root.add_child(_main)
	if not bool(_main.get("view_is_ready")):
		await _main.view_ready
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
	await _check_election_night(sim)

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


## Parteienmodus (seit M8): Rolle im Spielmenü, Parteipanel statt Staatspanel, Wahlkampf über den
## Knopf in der Ortsinfo (Wahl oben, kein „Diesen Staat spielen“), Mittel am nächsten Tag weniger,
## zurück zum Staat über das Parteipanel.
func _check_party_mode(sim: SimulationClient) -> void:
	var currents := sim.get_currents()
	var liberals := -1
	for i in currents.size():
		if str(currents[i].get("id", "")) == "liberale":
			liberals = i
	sim.advance_days(1) # Spenden am Monatsersten
	_main.call("_after_time_passed")
	var menu: GameMenu = _main.call("game_menu")
	_main.call("open_game_menu")
	menu.select_role(liberals)
	_main.call("close_game_menu", false)
	await _frames(1)
	var party_panel: PartyPanel = _main.call("party_panel")
	var state_panel: StatePanel = _main.call("state_panel")
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
	rally.pressed.emit()
	await _frames(1)
	if not bar.speed_line_text().contains("Versammlung"):
		_fail("Wahlkampf: keine Meldung (%s)" % bar.speed_line_text())
	sim.advance_days(1)
	_main.call("_after_time_passed")
	await _frames(1)
	var party := sim.get_party(liberals)
	if float(party.get("funds", 0.0)) >= funds or not party_panel.facts_text().contains("in 1 Wahlkreis"):
		_fail("Wahlkampf am nächsten Tag: %s, Panel „%s“" % [str(party), party_panel.facts_text()])
	var leave := party_panel.find_child("LeaveParty", true, false) as Button
	leave.pressed.emit()
	await _frames(1)
	if (
		party_panel.visible or not state_panel.visible or sim.get_player_current() != -1
		or panel.find_child("Campaign", true, false) != null or section.get_index() < facts.get_index()
	):
		_fail("Zurück zum Staat: Parteipanel %s, Staatspanel %s, Strömung %d" % [
			party_panel.visible, state_panel.visible, sim.get_player_current()
		])
	print("  Parteienmodus: Rolle im Menü, Versammlung in Mannheim (%s → %s Taler), zurück zum Staat" % [
		GermanFormat.integer(roundi(funds)), GermanFormat.integer(roundi(float(party.get("funds", 0.0))))
	])


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
	_main.call("set_speed", 5)
	for _frame in 3000:
		if night.is_open():
			break
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
	night.set_speed(1)
	for _frame in 20:
		await process_frame
	var partial := night.reported_count()
	if partial <= 0 or night.is_complete():
		_fail("Wahlabend: keine schrittweisen Meldungen (%d)" % partial)
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
	print("  Wahlen 1848: Taste Ä, Wahlabend am 1. Mai (%d Meldungen schrittweise), Ergebnis = Hochrechnung" % partial)


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
