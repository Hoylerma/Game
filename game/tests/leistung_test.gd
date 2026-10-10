extends SceneTree
## Leistungstest der Kartenansicht: Bildzeiten beim Schwenken und Zoomen.
##
## Startet die Hauptszene mit den echten Daten und bewegt die Karte über echte
## Eingabeereignisse (Ziehen mit der rechten Maustaste, Mausrad). Je Abschnitt
## werden die Abstände zwischen zwei Bildern gemessen (V-Sync aus, keine
## Bildratenbremse) und als Mittel, Median, 95-%-Wert und Höchstwert ausgegeben,
## dazu die Zeit für die Namen (MapLabels) und die Renderzeit des Viewports.
##
## Braucht eine Grafikausgabe (ohne Bildschirm z. B. xvfb-run, Software-Renderer
## llvmpipe). Aufruf aus dem Repository-Hauptordner, nach einem Import:
##   xvfb-run -a -s "-screen 0 1600x900x24" godot --path game \
##     --rendering-driver opengl3 --audio-driver Dummy --script res://tests/leistung_test.gd
## Optionen nach "--": --bilder=<n> Bilder je Abschnitt (Standard 150).
## Ergebnis: Tabelle und "LEISTUNG OK" (Exitcode 0); ohne Karte oder Simulation
## "LEISTUNG FEHLGESCHLAGEN" (Exitcode 1). Grenzwerte prüft der Test nicht, weil
## die Zahlen stark vom Rechner abhängen.

const MAIN_SCENE := "res://scenes/main.tscn"
const DEFAULT_FRAMES := 150
## Schwenkgeschwindigkeit in Bildschirmpixeln je Bild.
const PAN_PIXELS_PER_FRAME := 9.0
## Bilder zwischen zwei Mausradschritten.
const WHEEL_EVERY := 5

var _main: Node = null
var _frames := DEFAULT_FRAMES
var _results: Array[Dictionary] = []


func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--bilder=") and arg.trim_prefix("--bilder=").is_valid_int():
			_frames = maxi(arg.trim_prefix("--bilder=").to_int(), 10)
	_run()


func _run() -> void:
	print("EINIGKEIT – Leistungstest der Kartenansicht")
	if DisplayServer.get_name() == "headless":
		printerr("LEISTUNG FEHLGESCHLAGEN: braucht eine Grafikausgabe (z. B. xvfb-run).")
		quit(1)
		return
	DisplayServer.window_set_vsync_mode(DisplayServer.VSYNC_DISABLED)
	Engine.max_fps = 0
	var started := Time.get_ticks_usec()
	_main = (load(MAIN_SCENE) as PackedScene).instantiate()
	root.add_child(_main)
	if not bool(_main.get("view_is_ready")):
		await _main.view_ready
	var load_ms := (Time.get_ticks_usec() - started) / 1000.0
	var map_view: MapView = _main.call("map_view")
	if not map_view.has_map() or _main.call("simulation") == null:
		printerr("LEISTUNG FEHLGESCHLAGEN: Karte oder Simulation nicht geladen.")
		quit(1)
		return
	map_view.hover_enabled = false
	var camera := map_view.camera
	var viewport_rid := root.get_viewport_rid()
	RenderingServer.viewport_set_measure_render_time(viewport_rid, true)
	print("Renderer: %s, %s" % [
		RenderingServer.get_video_adapter_name(), RenderingServer.get_video_adapter_vendor()
	])
	print("Fenster %s, Karte %s, %d Orte; Laden bis zur ersten Ansicht %.0f ms" % [
		str(root.size), str(map_view.map_data().size), map_view.map_data().map_count(), load_ms
	])
	for _i in 10:
		await process_frame

	var diameter := map_view.location_diameter()
	var medium := MapDetail.zoom_for(MapDetail.LEVEL_SIZES[1], diameter)
	var near := MapDetail.zoom_for(MapDetail.LEVEL_SIZES[2], diameter)
	var center := map_view.map_rect().get_center()

	await _measure("Ruhe, Übersicht", camera, func(_frame: int) -> void: pass)
	await _measure("Schwenken, Übersicht", camera, _pan_step.bind(camera))
	camera.focus_on(center, medium)
	await _measure("Schwenken, mittel", camera, _pan_step.bind(camera))
	camera.focus_on(center, near)
	await _measure("Schwenken, nah", camera, _pan_step.bind(camera))
	camera.focus_on(center, camera.overview_zoom)
	await _measure("Zoomfahrt (Mausrad)", camera, _wheel_step.bind(camera))
	camera.focus_on(center, medium)
	_main.call("set_speed", 5)
	await _measure("Simulation 120 Tage/s, mittel", camera, func(_frame: int) -> void: pass)
	_main.call("set_speed", 0)
	_main.call("set_map_mode", MapModes.MAP_KREISE)
	await _measure("Kartenmodus Kreise, Schwenken mittel", camera, _pan_step.bind(camera))

	print("")
	print("%-38s %7s %7s %7s %7s %7s %8s %8s %6s" % [
		"Abschnitt", "Mittel", "Median", "95 %", "Max", "Bilder/s", "Namen", "Render", "Namen#"
	])
	for result in _results:
		print("%-38s %7.2f %7.2f %7.2f %7.2f %7.1f %8.2f %8.2f %6d" % [
			result["name"], result["mean"], result["median"], result["p95"], result["max"],
			result["fps"], result["labels_ms"], result["render_ms"], result["labels"]
		])
	print("(Zeiten in ms je Bild. „Namen“: Zeichnen der Beschriftung (GDScript), gemittelt über")
	print(" alle Bilder. „Render“: CPU- plus GPU-Renderzeit des Viewports laut RenderingServer;")
	print(" mit llvmpipe nur ein grober Anhalt. „Namen#“: gezeichnete Namen im letzten Bild.)")
	print("LEISTUNG OK")
	quit(0)


## Misst _frames Bilder; step(frame) bewegt vor jedem Bild die Karte.
func _measure(title: String, camera: MapCamera, step: Callable) -> void:
	var labels: MapLabels = _main.call("map_labels")
	var viewport_rid := root.get_viewport_rid()
	await process_frame
	var times := PackedFloat64Array()
	var label_total := 0.0
	var render_total := 0.0
	var label_draws := labels.draw_count
	var last := Time.get_ticks_usec()
	for frame in _frames:
		step.call(frame)
		await process_frame
		var now := Time.get_ticks_usec()
		times.append((now - last) / 1000.0)
		last = now
		# Namen werden nur nach einer Kamerabewegung neu gezeichnet.
		if labels.draw_count != label_draws:
			label_draws = labels.draw_count
			label_total += labels.last_draw_usec / 1000.0
		render_total += RenderingServer.viewport_get_measured_render_time_cpu(viewport_rid)
		render_total += RenderingServer.viewport_get_measured_render_time_gpu(viewport_rid)
	_release_buttons()
	var sorted := times.duplicate()
	sorted.sort()
	var total := 0.0
	for value in times:
		total += value
	var mean := total / times.size()
	_results.append({
		"name": title,
		"mean": mean,
		"median": sorted[sorted.size() / 2],
		"p95": sorted[mini(int(sorted.size() * 0.95), sorted.size() - 1)],
		"max": sorted[sorted.size() - 1],
		"fps": 1000.0 / mean if mean > 0.0 else 0.0,
		"labels_ms": label_total / times.size(),
		"render_ms": render_total / times.size(),
		"labels": labels.last_drawn,
	})
	print("  %s: %.2f ms im Mittel (Zoom %.3f, %s)" % [
		title, mean, camera.zoom.x,
		MapDetail.level_name(MapDetail.screen_size(camera.zoom.x, labels.location_diameter))
	])


## Ziehen mit der rechten Maustaste: Richtung wechselt alle 60 Bilder.
func _pan_step(frame: int, camera: MapCamera) -> void:
	var middle := root.get_visible_rect().size * 0.5
	if frame == 0:
		_press(MOUSE_BUTTON_RIGHT, middle, true)
	var phase := (frame / 60) % 4
	var directions: Array[Vector2] = [
		Vector2(1, 0.4), Vector2(-0.4, 1), Vector2(-1, -0.4), Vector2(0.4, -1)
	]
	var direction := directions[phase]
	var motion := InputEventMouseMotion.new()
	motion.position = middle
	motion.global_position = middle
	motion.relative = direction * PAN_PIXELS_PER_FRAME
	motion.button_mask = MOUSE_BUTTON_MASK_RIGHT
	Input.parse_input_event(motion)
	if camera == null:
		return


## Mausrad an wechselnden Punkten: 24 Schritte hinein, dann hinaus.
func _wheel_step(frame: int, camera: MapCamera) -> void:
	if frame % WHEEL_EVERY != 0 or camera == null:
		return
	var steps := frame / WHEEL_EVERY
	var size := root.get_visible_rect().size
	var point := Vector2(size.x * (0.4 + 0.2 * sin(steps)), size.y * (0.4 + 0.2 * cos(steps)))
	var wheel := MOUSE_BUTTON_WHEEL_UP if (steps / 12) % 2 == 0 else MOUSE_BUTTON_WHEEL_DOWN
	_press(wheel, point, true)
	_press(wheel, point, false)


func _press(button: MouseButton, position: Vector2, pressed: bool) -> void:
	var event := InputEventMouseButton.new()
	event.button_index = button
	event.pressed = pressed
	event.position = position
	event.global_position = position
	if pressed and button == MOUSE_BUTTON_RIGHT:
		event.button_mask = MOUSE_BUTTON_MASK_RIGHT
	Input.parse_input_event(event)


func _release_buttons() -> void:
	_press(MOUSE_BUTTON_RIGHT, root.get_visible_rect().size * 0.5, false)
