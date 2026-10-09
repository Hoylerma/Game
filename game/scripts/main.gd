extends Node
## Hauptszene: verbindet Simulation (GDExtension), Karte und Oberfläche.
##
## Ablauf: Optionen lesen → Karte laden → Simulation laden → Oberfläche
## füllen. Fehlt die Erweiterung, die Karte oder sind die Daten fehlerhaft,
## erscheint ein Hinweis; das Projekt bleibt bedienbar, soweit es geht.

## Tage je Sekunde für Pause (0) und die Stufen 1–5.
const SPEED_DAYS_PER_SECOND: PackedInt32Array = [0, 2, 5, 15, 40, 120]
## Stufe, mit der die Leertaste nach dem Start fortsetzt.
const DEFAULT_RESUME_SPEED := 2
## Obergrenze je Bild, damit langsame Bilder keine Lawine auslösen.
const MAX_DAYS_PER_FRAME := 30
const SCREENSHOT_WAIT_FRAMES := 20
const BRIDGE_BUILD_COMMANDS := "cmake --preset bridge\ncmake --build --preset bridge"
const SPEED_ACTIONS: Array[StringName] = [
	&"tempo_pause", &"tempo_1", &"tempo_2", &"tempo_3", &"tempo_4", &"tempo_5"
]

var _sim: SimulationClient = null
var _map: MapData = null
var _options: LaunchOptions = null
var _data_dir := ""
var _player := -1
var _speed := 0
var _resume_speed := DEFAULT_RESUME_SPEED
var _day_accumulator := 0.0
var _map_mode := MapModes.MAP_POLITICAL
var _selected := -1
var _chronicle_read := 0
var _legend_month := -1
## Kartenmodus → Zeitpunkt (ms), zu dem eine doppelt belegte Taste gedrückt wurde.
var _mode_key_down_at := {}

@onready var _map_view: MapView = $MapView
@onready var _top_bar: TopBar = %TopBar
@onready var _state_panel: StatePanel = %StatePanel
@onready var _chronicle_panel: ChroniclePanel = %ChroniclePanel
@onready var _location_panel: LocationPanel = %LocationPanel
@onready var _legend: LegendPanel = %LegendPanel
@onready var _mode_bar: MapModeBar = %MapModeBar
@onready var _tooltip: MapTooltip = %MapTooltip
@onready var _notices: NoticeLayer = %NoticeLayer
@onready var _map_labels: MapLabels = %MapLabels
@onready var _left_column: Control = $UI/Root/LeftColumn
@onready var _right_column: Control = $UI/Root/RightColumn
@onready var _mode_bar_area: Control = $UI/Root/ModeBarArea


func _ready() -> void:
	_options = LaunchOptions.from_command_line()
	for problem in _options.problems:
		printerr("EINIGKEIT: ", problem)
	_data_dir = UhrwerkPaths.data_dir()
	print("EINIGKEIT: Datenordner %s (%s)" % [_data_dir, UhrwerkPaths.data_dir_origin()])

	_top_bar.configure_speeds(SPEED_DAYS_PER_SECOND)
	_top_bar.speed_selected.connect(_set_speed)
	_state_panel.tax_rate_requested.connect(_on_tax_rate_requested)
	_chronicle_panel.location_requested.connect(_on_chronicle_location_requested)
	_location_panel.close_requested.connect(_select_location.bind(-1))
	_mode_bar.mode_selected.connect(_set_map_mode)
	_map_view.location_hovered.connect(_on_location_hovered)
	_map_view.location_clicked.connect(_on_location_clicked)
	get_viewport().size_changed.connect(_on_viewport_size_changed)

	_load_map()
	_load_simulation()
	_connect_map_and_simulation()
	_update_owners()
	_update_map_labels()
	if _sim != null:
		_legend_month = int(_sim.get_date().get("month", 0))
	_set_map_mode(MapModes.MAP_POLITICAL)
	_refresh_all()

	# Erst nach dem ersten Layout sind die Panelgrößen bekannt.
	await get_tree().process_frame
	_fit_camera()
	if _options.wants_screenshot():
		_run_screenshot()


func _process(delta: float) -> void:
	if _sim == null or _speed == 0:
		return
	_day_accumulator += delta * SPEED_DAYS_PER_SECOND[_speed]
	var days := floori(_day_accumulator)
	if days < 1:
		return
	_day_accumulator -= days
	_sim.advance_days(mini(days, MAX_DAYS_PER_FRAME))
	_after_time_passed()


func _unhandled_input(event: InputEvent) -> void:
	for speed in SPEED_ACTIONS.size():
		if event.is_action_pressed(SPEED_ACTIONS[speed], false, true):
			if speed == 0:
				_toggle_pause()
			else:
				_set_speed(speed)
			get_viewport().set_input_as_handled()
			return
	if event.is_action_pressed(&"auswahl_aufheben", false, true) and _selected >= 0:
		_select_location(-1)
		get_viewport().set_input_as_handled()
		return
	_handle_map_mode_key(event)


# --- Laden -------------------------------------------------------------------


func _load_map() -> void:
	_map = MapData.load_from_dir(UhrwerkPaths.map_dir())
	for warning in _map.warnings:
		printerr("EINIGKEIT: Hinweis zur Karte: ", warning)
	if not _map.is_valid():
		printerr("EINIGKEIT: ", _map.error)
		_notices.add_notice(
			"Karte nicht verfügbar",
			(
				"Die Kartenansicht braucht das Ort-ID-Bild provinzen.png und die Zuordnung "
				+ "definition.csv. Beide erzeugt der Kartengenerator aus den Ortsdaten. "
				+ "Die übrige Oberfläche funktioniert auch ohne Karte."
			),
			_map.error
		)
	_map_view.setup(_map)


func _load_simulation() -> void:
	if not SimulationClient.is_extension_available():
		var library := SimulationClient.expected_library_file()
		printerr(
			"EINIGKEIT: Die GDExtension „uhrwerk_bridge“ ist nicht geladen ",
			"(erwartet game/%s). Bauen im Repository-Hauptordner mit: " % library,
			"cmake --preset bridge && cmake --build --preset bridge"
		)
		_notices.add_notice(
			"Simulationskern nicht geladen",
			(
				"Die GDExtension „uhrwerk_bridge“ fehlt oder konnte nicht geladen werden. "
				+ "Ohne sie läuft keine Simulation; Leisten und Karte bleiben ohne Daten. Baue "
				+ "die Erweiterung im Repository-Hauptordner und starte das Spiel danach neu."
			),
			"Erwartet: game/%s\n\n%s" % [library, BRIDGE_BUILD_COMMANDS]
		)
		return
	var missing := SimulationClient.missing_methods()
	if not missing.is_empty():
		printerr("EINIGKEIT: Der Erweiterung fehlen Methoden: ", ", ".join(missing))
		_notices.add_notice(
			"Erweiterung passt nicht zur Oberfläche",
			(
				"Die geladene GDExtension stellt nicht alle Methoden der Brücken-Schnittstelle "
				+ "bereit (docs/technik/bridge-api.md). Bitte die Erweiterung neu bauen."
			),
			"Fehlende Methoden:\n" + "\n".join(missing) + "\n\n" + BRIDGE_BUILD_COMMANDS
		)
		return
	var sim := SimulationClient.create()
	if sim == null:
		_notices.add_notice(
			"Simulationskern nicht geladen",
			"Die Klasse UhrwerkSimulation konnte nicht erzeugt werden.",
			BRIDGE_BUILD_COMMANDS
		)
		return
	var errors := sim.load_scenario(_data_dir)
	if not errors.is_empty() or not sim.is_loaded():
		printerr("EINIGKEIT: Spieldaten konnten nicht geladen werden:\n", errors)
		_notices.add_notice(
			"Spieldaten konnten nicht geladen werden",
			(
				"Datenordner: %s\nHerkunft: %s\n" % [_data_dir, UhrwerkPaths.data_dir_origin()]
				+ (
					"Einen anderen Ordner wählt die Umgebungsvariable %s."
					% UhrwerkPaths.ENV_DATA_DIR
				)
			),
			errors if not errors.is_empty() else "Unbekannter Fehler beim Laden."
		)
		return
	for warning in sim.get_load_warnings():
		print("EINIGKEIT: Hinweis zu den Daten: ", warning)
	_sim = sim
	_player = _sim.get_player_state_index()
	_state_panel.set_max_tax_rate(_sim.get_max_tax_rate_percent())
	_location_panel.confession_colors = LegendData.category_colors(
		_sim, MapModes.MAP_CONFESSION, "majority_confession"
	)
	print(
		(
			"EINIGKEIT: %s geladen – %s, %d Staaten, %d Orte"
			% [
				_sim.get_scenario_name(),
				_sim.get_date_text(),
				_sim.get_state_count(),
				_sim.get_location_count()
			]
		)
	)


## Ordnet Karten- und Simulationsindizes zu und meldet Abweichungen.
func _connect_map_and_simulation() -> void:
	if not _map.is_valid():
		return
	var unresolved := _map.bind_simulation(_sim)
	if _sim == null:
		_map_view.set_location_colors(PackedColorArray())
		return
	var absent := PackedStringArray()
	for sim_index in _map.sim_to_map.size():
		if _map.sim_to_map[sim_index] < 0:
			absent.append(str(_sim.get_location(sim_index).get("id", sim_index)))
	if unresolved.is_empty() and absent.is_empty():
		return
	var details := ""
	if not unresolved.is_empty():
		details += "Kartenorte ohne Spieldaten: %s\n" % ", ".join(unresolved)
	if not absent.is_empty():
		details += "Orte ohne Kartenfläche: %s\n" % ", ".join(absent)
	details += "\nKarte neu erzeugen:\n" + MapData.GENERATOR_COMMAND
	printerr("EINIGKEIT: Karte und Spieldaten passen nicht zusammen.\n", details)
	_notices.add_notice(
		"Karte passt nicht zu den Spieldaten",
		"Einige Orte der Karte und der Spieldaten lassen sich nicht zuordnen.",
		details
	)


# --- Anzeige -----------------------------------------------------------------


func _refresh_all() -> void:
	if _sim == null:
		_top_bar.show_unavailable()
		_state_panel.show_unavailable()
		_legend.show_message(MapModes.name_of(_map_mode), "Ohne Simulation liegen keine Daten vor.")
		return
	var state := _sim.get_state(_player)
	_top_bar.show_date(_sim.get_date_text())
	_top_bar.show_speed(_speed, SPEED_DAYS_PER_SECOND[_speed])
	_top_bar.show_state(state)
	var capital_name := ""
	var capital := int(state.get("capital_index", -1))
	if capital >= 0:
		capital_name = str(_sim.get_location(capital).get("name", ""))
	_state_panel.show_state(state, capital_name)
	_refresh_location_panel()
	_refresh_map()
	_pull_chronicle()


func _after_time_passed() -> void:
	_refresh_all()
	var month := int(_sim.get_date().get("month", 0))
	if month != _legend_month:
		_legend_month = month
		_rebuild_legend()
		_update_owners()
	var hovered := _map_view.get_hovered()
	if hovered >= 0:
		_on_location_hovered(hovered)


func _refresh_map() -> void:
	if _sim == null or not _map.is_valid():
		return
	_map_view.set_location_colors(_sim.get_map_colors(_map_mode))


func _update_owners() -> void:
	if _sim == null or not _map.is_valid():
		return
	var owners := PackedInt32Array()
	owners.resize(_sim.get_location_count())
	for index in owners.size():
		owners[index] = int(_sim.get_location(index).get("owner_index", -1))
	_map_view.set_location_owners(owners)


## Ortsnamen für die Karte: Hauptstädte vor Städten vor übrigen Orten.
func _update_map_labels() -> void:
	_map_labels.camera = _map_view.camera
	var entries: Array[Dictionary] = []
	if _sim != null and _map.is_valid():
		var capitals := {}
		for state_index in _sim.get_state_count():
			capitals[int(_sim.get_state(state_index).get("capital_index", -1))] = true
		for index in _sim.get_location_count():
			var center := _map_view.location_center(index)
			if center.x < 0.0:
				continue
			var location := _sim.get_location(index)
			var rank := MapLabels.Rank.PLACE
			if capitals.has(index):
				rank = MapLabels.Rank.CAPITAL
			elif bool(location.get("is_city", false)):
				rank = MapLabels.Rank.TOWN
			entries.append({
				"world": center,
				"text": str(location.get("name", "")),
				"rank": rank,
				"weight": float(location.get("population", 0)),
			})
	_map_labels.set_entries(entries)


func _refresh_location_panel() -> void:
	if _sim == null or _selected < 0:
		_location_panel.visible = false
		return
	var location := _sim.get_location(_selected)
	var owner_state := _sim.get_state(int(location.get("owner_index", -1)))
	_location_panel.show_location(location, owner_state.get("color", UiPalette.INK_FAINT))
	_fit_location_panel.call_deferred()


## Ortsinfo und Legende teilen sich die rechte Spalte; die Ortsinfo weicht aus.
func _fit_location_panel() -> void:
	var separation := float(_right_column.get_theme_constant(&"separation", &"VBoxContainer"))
	var available := _right_column.size.y - _legend.get_combined_minimum_size().y - separation
	_location_panel.fit_height(available)


func _pull_chronicle() -> void:
	var count := _sim.get_chronicle_size()
	if count < _chronicle_read:
		_chronicle_panel.clear()
		_chronicle_read = 0
	if count > _chronicle_read:
		_chronicle_panel.add_entries(_sim.get_chronicle(_chronicle_read))
		_chronicle_read = count


func _rebuild_legend() -> void:
	var title := MapModes.name_of(_map_mode)
	if _sim == null:
		_legend.show_message(title, "Ohne Simulation liegen keine Daten vor.")
		return
	match _map_mode:
		MapModes.MAP_POLITICAL:
			_legend.show_swatches(title, LegendData.political_entries(_sim))
		MapModes.MAP_DENSITY:
			var scale_max := MapModes.DENSITY_SCALE_MAX
			var density_labels := PackedStringArray(
				[
					"0",
					GermanFormat.number(scale_max * 0.5),
					"≥ " + GermanFormat.number(scale_max),
				]
			)
			_legend.show_gradient(
				title,
				LegendData.gradient_for(_sim, MapModes.MAP_DENSITY, "density", scale_max),
				density_labels,
				"Einwohner je km²"
			)
		MapModes.MAP_CONFESSION:
			var confessions := LegendData.category_entries(
				_sim, MapModes.MAP_CONFESSION, "majority_confession"
			)
			_legend.show_swatches(title, confessions)
		MapModes.MAP_TERRAIN:
			var terrains := LegendData.category_entries(_sim, MapModes.MAP_TERRAIN, "terrain")
			_legend.show_swatches(title, terrains)
		MapModes.MAP_DEVASTATION:
			var devastation_labels := PackedStringArray(
				[GermanFormat.percent(0), GermanFormat.percent(50), GermanFormat.percent(100)]
			)
			_legend.show_gradient(
				title,
				LegendData.gradient_for(
					_sim, MapModes.MAP_DEVASTATION, "devastation_percent", 100.0
				),
				devastation_labels,
				"Kriegsschäden der Orte"
			)
	if _selected >= 0:
		_fit_location_panel.call_deferred()


func _fit_camera() -> void:
	var viewport_rect := get_viewport().get_visible_rect()
	var left := _left_column.get_global_rect().end.x + 8.0
	var right := _right_column.get_global_rect().position.x - 8.0
	var top := _top_bar.get_global_rect().end.y + 8.0
	var bottom := _mode_bar_area.get_global_rect().position.y - 4.0
	var area := Rect2(left, top, right - left, bottom - top)
	if area.size.x < 200.0 or area.size.y < 200.0:
		area = viewport_rect
	# Rahmen und Gradzahlen sollen sichtbar sein.
	_map_view.camera.fit_to(_map_view.map_rect().grow(34.0), area)
	_map_view.camera.map_rect = _map_view.map_rect()


# --- Bedienung ---------------------------------------------------------------


func _set_speed(speed: int) -> void:
	if _sim == null:
		_top_bar.show_unavailable()
		return
	_speed = clampi(speed, 0, SPEED_DAYS_PER_SECOND.size() - 1)
	if _speed > 0:
		_resume_speed = _speed
	else:
		_day_accumulator = 0.0
	_top_bar.show_speed(_speed, SPEED_DAYS_PER_SECOND[_speed])


func _toggle_pause() -> void:
	_set_speed(0 if _speed > 0 else _resume_speed)


func _set_map_mode(mode: int) -> void:
	if not MapModes.is_valid(mode):
		return
	_map_mode = mode
	_mode_bar.show_mode(mode)
	_refresh_map()
	_rebuild_legend()
	var hovered := _map_view.get_hovered()
	if hovered >= 0:
		_on_location_hovered(hovered)


## Kartenmodus-Tasten; doppelt belegte Tasten (W) schalten nur bei kurzem Tippen.
func _handle_map_mode_key(event: InputEvent) -> void:
	var mode := MapModes.mode_for_event(event)
	if mode < 0:
		return
	if not MapCamera.is_pan_event(event):
		if event.is_pressed() and not event.is_echo():
			_set_map_mode(mode)
			get_viewport().set_input_as_handled()
		return
	if event.is_pressed() and not event.is_echo():
		_mode_key_down_at[mode] = Time.get_ticks_msec()
	elif event.is_released() and _mode_key_down_at.has(mode):
		var held := Time.get_ticks_msec() - int(_mode_key_down_at[mode])
		_mode_key_down_at.erase(mode)
		if held < MapCamera.TAP_HOLD_MS:
			_set_map_mode(mode)


func _select_location(sim_index: int, focus_camera: bool = false) -> void:
	if _sim == null or sim_index < 0 or sim_index >= _sim.get_location_count():
		sim_index = -1
	_selected = sim_index
	_map_view.set_selected(sim_index)
	_refresh_location_panel()
	if focus_camera and sim_index >= 0:
		var center := _map_view.location_center(sim_index)
		if center.x >= 0.0:
			_map_view.camera.focus_on(center)


func _on_viewport_size_changed() -> void:
	if _selected >= 0:
		_fit_location_panel.call_deferred()


func _on_location_clicked(sim_index: int) -> void:
	_select_location(sim_index)


func _on_chronicle_location_requested(sim_index: int) -> void:
	_select_location(sim_index, true)


func _on_location_hovered(sim_index: int) -> void:
	if _sim == null or sim_index < 0:
		_tooltip.hide_tooltip()
		return
	var location := _sim.get_location(sim_index)
	_tooltip.show_text(str(location.get("name", "–")), _tooltip_detail(location))


func _tooltip_detail(location: Dictionary) -> String:
	var owner_name := str(location.get("owner_name", ""))
	match _map_mode:
		MapModes.MAP_DENSITY:
			var density := float(location.get("density", 0.0))
			return "%s · %s" % [owner_name, GermanFormat.density(density)]
		MapModes.MAP_CONFESSION:
			return "%s · %s" % [
				owner_name, GermanFormat.humanize(str(location.get("majority_confession", "")))
			]
		MapModes.MAP_TERRAIN:
			var terrain := str(location.get("terrain", ""))
			return "%s · %s" % [owner_name, GermanFormat.humanize(terrain)]
		MapModes.MAP_DEVASTATION:
			return "%s · %s verwüstet" % [
				owner_name, GermanFormat.percent(int(location.get("devastation_percent", 0)))
			]
	return owner_name


func _on_tax_rate_requested(percent: int) -> void:
	if _sim == null:
		return
	if not _sim.set_tax_rate_percent(_player, percent):
		_state_panel.reject_pending(
			"Der Steuersatz %s wurde nicht angenommen." % GermanFormat.percent(percent)
		)


# --- Screenshot ----------------------------------------------------------------


## --screenshot: simulieren, Zustand herstellen, warten, Bild speichern, beenden.
func _run_screenshot() -> void:
	_map_view.hover_enabled = false
	_tooltip.hide_tooltip()
	if _sim != null and _options.days > 0:
		_sim.advance_days(_options.days)
	if MapModes.is_valid(_options.map_mode):
		_set_map_mode(_options.map_mode)
	if not _options.location_id.is_empty():
		var index := _sim.get_location_index(_options.location_id) if _sim != null else -1
		if index >= 0:
			_select_location(index)
		else:
			var unknown := _options.location_id
			printerr("EINIGKEIT: Ort „%s“ ist unbekannt und wird nicht ausgewählt." % unknown)
	if _sim != null:
		_after_time_passed()

	if DisplayServer.get_name() == "headless":
		# Ohne Grafikausgabe gibt es kein Bild (und kein frame_post_draw).
		printerr(
			"EINIGKEIT: Screenshots brauchen eine Grafikausgabe; im Headless-Modus gibt es ",
			"kein Bild. Ohne Bildschirm z. B. mit xvfb-run starten."
		)
		get_tree().quit(1)
		return

	for _frame in SCREENSHOT_WAIT_FRAMES:
		await get_tree().process_frame
	await RenderingServer.frame_post_draw

	var path := _options.resolved_screenshot_path()
	var exit_code := _save_screenshot(path)
	if exit_code == 0 and (_sim == null or not _map.is_valid()):
		printerr("EINIGKEIT: Screenshot zeigt einen Fehlerzustand (Simulation oder Karte fehlt).")
		exit_code = 1
	get_tree().quit(exit_code)


func _save_screenshot(path: String) -> int:
	var image := get_viewport().get_texture().get_image()
	if image == null or image.is_empty():
		printerr("EINIGKEIT: Das Bild des Viewports ist leer, kein Screenshot gespeichert.")
		return 1
	DirAccess.make_dir_recursive_absolute(path.get_base_dir())
	var error := image.save_png(path)
	if error != OK:
		printerr("EINIGKEIT: Screenshot konnte nicht gespeichert werden (%s): %s" % [
			error_string(error), path
		])
		return 1
	var summary := "ohne Simulation"
	if _sim != null:
		summary = "%s, Prüfsumme %s" % [_sim.get_date_text(), _sim.get_checksum()]
	print("EINIGKEIT: Screenshot gespeichert: %s (%dx%d, %s)" % [
		path, image.get_width(), image.get_height(), summary
	])
	return 0
