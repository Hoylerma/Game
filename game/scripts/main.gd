extends Node
## Hauptszene: verbindet Simulation (GDExtension), Karte und Oberfläche.
##
## Ablauf: Optionen lesen → Karte laden → Simulation laden → Oberfläche
## füllen. Fehlt die Erweiterung, die Karte oder sind die Daten fehlerhaft,
## erscheint ein Hinweis; das Projekt bleibt bedienbar, soweit es geht.

## Karte, Simulation und Kamera sind bereit (nach dem ersten Layout).
signal view_ready

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
## Rand um die Karte (in Kartenpixeln je 1.024 Pixel Kartenbreite), damit Rahmen und
## Gradzahlen in der Übersicht sichtbar bleiben.
const OVERVIEW_MARGIN := 28.0
## Kartenmodi, deren Legende sich mit der Zeit ändert (monatlich neu aufbauen).
const DYNAMIC_LEGEND_MODES: PackedInt32Array = [
	MapModes.MAP_POLITICAL, MapModes.MAP_DENSITY, MapModes.MAP_CONFESSION,
	MapModes.MAP_DEVASTATION, MapModes.MAP_LEBENSSTANDARD, MapModes.MAP_KULTUR,
	MapModes.MAP_WIRTSCHAFT
]
## Höchstzahl der Kennungen in einer Hinweismeldung.
const MAX_LISTED_IDS := 20

## true, sobald view_ready gesendet wurde.
var view_is_ready := false
## Ordner der Spielstände (seit M6); Tests setzen einen eigenen, bevor sie speichern.
var save_dir := SaveSlots.DEFAULT_DIR
## Automatischer Spielstand am 1. Januar (seit M6).
var autosave_enabled := true

var _sim: SimulationClient = null
var _map: MapData = null
var _catalog := LocationCatalog.new()
## Besitzer je Simulationsindex beim letzten Abgleich (Grenzen, Staatsnamen).
var _owners := PackedInt32Array()
## Markt je Simulationsindex (seit M5, unveränderlich): Grenzen im Kartenmodus „Märkte“.
var _market_indices := PackedInt32Array()
## Unterste Verwaltungseinheit je Simulationsindex (seit M7): Grenzen im Modus „Verwaltung“.
var _admin_indices := PackedInt32Array()
## Szenario, zu dem Ortskatalog und Farben gehören (seit M7).
var _scenario_id := ""
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
## Spielmenü (seit M6) und die Geschwindigkeit, die beim Schließen wieder gilt.
var _game_menu: GameMenu = null
var _speed_before_menu := 0

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
	_top_bar.menu_requested.connect(toggle_game_menu)
	_build_game_menu()
	_state_panel.tax_rate_requested.connect(_on_tax_rate_requested)
	_chronicle_panel.location_requested.connect(_on_chronicle_location_requested)
	_location_panel.close_requested.connect(_select_location.bind(-1))
	_location_panel.play_state_requested.connect(play_state)
	_mode_bar.mode_selected.connect(_set_map_mode)
	_map_view.location_hovered.connect(_on_location_hovered)
	_map_view.location_clicked.connect(_on_location_clicked)
	get_viewport().size_changed.connect(_on_viewport_size_changed)

	_load_map()
	_load_simulation()
	_connect_map_and_simulation()
	_map_labels.camera = _map_view.camera
	_map_labels.location_diameter = _map_view.location_diameter()
	_map_labels.set_covering_controls([
		_top_bar, _state_panel, _chronicle_panel, _location_panel, _legend, _mode_bar
	])
	_update_place_labels()
	_update_owners()
	if _sim != null:
		_legend_month = int(_sim.get_date().get("month", 0))
	_set_map_mode(MapModes.MAP_POLITICAL)
	_refresh_all()

	# Erst nach dem ersten Layout sind die Panelgrößen bekannt.
	await get_tree().process_frame
	_fit_camera()
	_apply_launch_view()
	view_is_ready = true
	view_ready.emit()
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
	# Höchstens bis zum 1. Januar, damit der automatische Spielstand genau den Jahreswechsel trifft;
	# die übrigen Tage kommen im nächsten Bild (verworfen wird nur, was über MAX_DAYS_PER_FRAME geht).
	var capped := mini(days, MAX_DAYS_PER_FRAME)
	var step := mini(capped, SaveSlots.days_until_new_year(_sim.get_date()))
	_day_accumulator += capped - step
	_sim.advance_days(step)
	_after_time_passed()
	var now := _sim.get_date()
	if int(now.get("month", 0)) == 1 and int(now.get("day", 0)) == 1:
		_autosave()


func _unhandled_input(event: InputEvent) -> void:
	if _handle_save_keys(event):
		get_viewport().set_input_as_handled()
		return
	if _game_menu != null and _game_menu.is_open():
		return # Solange das Menü offen ist, ruhen Karte und Zeit (Escape schließt es, GameMenu).
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
	if event.is_action_pressed(&"kamera_uebersicht", false, true):
		_fit_camera()
		get_viewport().set_input_as_handled()
		return
	if event.is_action_pressed(&"kamera_hauptstadt", false, true):
		focus_capital()
		get_viewport().set_input_as_handled()
		return
	_handle_map_mode_key(event)


# --- Zugriff für Tests -----------------------------------------------------------


## Kartenansicht (Klick- und Leistungstest).
func map_view() -> MapView:
	return _map_view


## Ausgewählter Ort (Simulationsindex) oder -1.
func selected_location() -> int:
	return _selected


func simulation() -> SimulationClient:
	return _sim


func map_labels() -> MapLabels:
	return _map_labels


## Geschwindigkeitsstufe 0 (Pause) bis 5.
func set_speed(speed: int) -> void:
	_set_speed(speed)


## Kartenmodus (MapModes) wie über die Modusleiste.
func set_map_mode(mode: int) -> void:
	_set_map_mode(mode)


## Ort auswählen (Simulationsindex, -1 = keiner) wie per Klick.
func select_location(sim_index: int) -> void:
	_select_location(sim_index)


## Staatspanel (Klicktest: Steuerregler und Haushaltsfenster, seit M5).
func state_panel() -> StatePanel:
	return _state_panel


## Ortspanel (Klicktest: Abschnitt Wirtschaft, seit M5).
func location_panel() -> LocationPanel:
	return _location_panel


## Spielmenü (Klicktest, seit M6).
func game_menu() -> GameMenu:
	return _game_menu


## Obere Leiste (Klicktest: Meldungen nach dem Speichern, seit M6).
func top_bar() -> TopBar:
	return _top_bar


## Geschwindigkeitsstufe 0 (Pause) bis 5.
## Aktiver Kartenmodus (Tests, seit M7).
func map_mode() -> int:
	return _map_mode


func speed() -> int:
	return _speed


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
	var scenario_id := _options.scenario_id
	if scenario_id.is_empty():
		scenario_id = UhrwerkPaths.DEFAULT_SCENARIO
	var errors := sim.load_scenario(_data_dir, scenario_id)
	if (not errors.is_empty() or not sim.is_loaded()) and scenario_id != UhrwerkPaths.DEFAULT_SCENARIO:
		# Unbekanntes --szenario: mit dem Standardszenario weiter, damit das Menü erreichbar bleibt.
		printerr("EINIGKEIT: --szenario=%s ließ sich nicht laden:\n" % scenario_id, errors)
		_notices.add_notice(
			"Szenario nicht geladen",
			"--szenario=%s ließ sich nicht laden; es gilt das Standardszenario." % scenario_id,
			errors
		)
		sim = SimulationClient.create()
		errors = sim.load_scenario(_data_dir, UhrwerkPaths.DEFAULT_SCENARIO)
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
	if not _options.state_tag.is_empty():
		var chosen := _sim.get_state_index(_options.state_tag)
		if chosen < 0 or not _sim.set_player_state(chosen):
			printerr("EINIGKEIT: --staat=%s besteht im Szenario nicht." % _options.state_tag)
			_notices.add_notice(
				"Unbekannter Staat",
				"--staat=%s besteht im Szenario nicht; Ihr spielt den vorgesehenen Staat."
				% _options.state_tag,
				""
			)
	_player = _sim.get_player_state_index()
	_read_scenario_constants()
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
		details += "Kartenorte ohne Spieldaten: %s\n" % _short_list(unresolved)
	if not absent.is_empty():
		details += "Orte ohne Kartenfläche: %s\n" % _short_list(absent)
	details += "\nKarte neu erzeugen:\n" + MapData.GENERATOR_COMMAND
	printerr("EINIGKEIT: Karte und Spieldaten passen nicht zusammen.\n", details)
	_notices.add_notice(
		"Karte passt nicht zu den Spieldaten",
		"Einige Orte der Karte und der Spieldaten lassen sich nicht zuordnen.",
		details
	)


## Kennungen für Meldungen, höchstens MAX_LISTED_IDS.
static func _short_list(ids: PackedStringArray) -> String:
	if ids.size() <= MAX_LISTED_IDS:
		return ", ".join(ids)
	return "%s … und %d weitere" % [
		", ".join(ids.slice(0, MAX_LISTED_IDS)), ids.size() - MAX_LISTED_IDS
	]


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
		if DYNAMIC_LEGEND_MODES.has(_map_mode):
			_rebuild_legend()
		_update_owners()
	var hovered := _map_view.get_hovered()
	if hovered >= 0:
		_on_location_hovered(hovered)


func _refresh_map() -> void:
	if _sim == null or not _map.is_valid():
		return
	_map_view.set_location_colors(_sim.get_map_colors(_map_mode))


## Besitzer und Kreise je Ort (zwei Indexarrays der Brücke) für Grenzen und Staatsnamen;
## Staatsnamen werden nur neu berechnet, wenn sich ein Besitzer geändert hat.
func _update_owners() -> void:
	if _sim == null or not _map.is_valid():
		return
	var owners := _sim.get_location_owner_indices()
	if owners == _owners and not _owners.is_empty():
		return
	_owners = owners
	_map_view.set_location_info(owners, _border_groups())
	_update_state_labels()


## Gruppen für die feinen Grenzlinien: Märkte im Marktmodus, Verwaltungseinheiten im Modus
## „Verwaltung“, sonst Kreise.
func _border_groups() -> PackedInt32Array:
	if _map_mode == MapModes.MAP_MAERKTE and not _market_indices.is_empty():
		return _market_indices
	if _map_mode == MapModes.MAP_VERWALTUNG and not _admin_indices.is_empty():
		return _admin_indices
	return _catalog.kreis_indices


## Rechnungseinheit des Szenarios (seit M7), z. B. „Reichstaler“ 1648, „Taler“ 1848.
func _currency() -> String:
	return str(_sim.get_scenario().get("currency", "Reichstaler")) if _sim != null else "Reichstaler"


## Was sich nur mit dem Szenario ändert (seit M7): Märkte, Verwaltung, Bundesgebiet; bei einem
## anderen Szenario als bisher auch Ortskatalog, Farben der Ortsinfo und Steuergrenze. True, wenn
## das Szenario gewechselt hat.
func _read_scenario_constants() -> bool:
	_market_indices = _sim.get_location_market_indices()
	_admin_indices = _sim.get_location_admin_indices()
	var federation := false
	for state in _sim.get_state_count():
		federation = federation or bool(_sim.get_state(state).get("bund", false))
	_location_panel.show_federation = federation
	_location_panel.player_index = _player
	var scenario_id := str(_sim.get_scenario().get("id", ""))
	if scenario_id == _scenario_id:
		return false
	_scenario_id = scenario_id
	_catalog = LocationCatalog.build(_sim)
	_state_panel.set_max_tax_rate(_sim.get_max_tax_rate_percent())
	_location_panel.confession_colors = LegendData.category_colors(
		_sim, MapModes.MAP_CONFESSION, "majority_confession"
	)
	_location_panel.culture_colors = LegendData.category_colors(
		_sim, MapModes.MAP_KULTUR, "majority_culture"
	)
	return true


## Ob ein Ort des Szenarios zu einer Verwaltungseinheit gehört (1648: keiner).
func _has_admin_units() -> bool:
	for unit in _admin_indices:
		if unit >= 0:
			return true
	return false


## Kartenindex → Gruppe (Besitzer bzw. Kreis) aus einem Array je Simulationsindex.
func _group_of_map(by_sim: PackedInt32Array) -> PackedInt32Array:
	var result := PackedInt32Array()
	result.resize(_map.map_count())
	result.fill(-1)
	for map_index in _map.map_count():
		var sim_index := _map.map_to_sim[map_index]
		if sim_index >= 0 and sim_index < by_sim.size():
			result[map_index] = by_sim[sim_index]
	return result


## Orts- und Kreisnamen (ändern sich während der Partie nicht).
## Rangfolge der Ortsnamen: Hauptstädte, Hauptorte der Kreise (ein Kreis heißt nach
## seinem größten Ort), übrige Städte, übrige Orte; bei Gleichstand der größere.
func _update_place_labels() -> void:
	var places: Array[Dictionary] = []
	var kreise: Array[Dictionary] = []
	if _sim != null and _map.is_valid():
		var capitals := _capitals()
		for map_index in _map.map_count():
			var sim_index := _map.map_to_sim[map_index]
			var point := _map.label_points[map_index]
			if sim_index < 0 or point.x < 0.0:
				continue
			var style := _place_style(sim_index, capitals)
			var rank := 0.0
			var is_seat := _catalog.kreis_name_of(sim_index) == _catalog.names[sim_index]
			if style == MapLabels.Style.CAPITAL:
				rank = 4.0
			elif style == MapLabels.Style.CITY:
				rank = 3.0 if is_seat else 2.0
			elif is_seat:
				rank = 1.0
			places.append({
				"world": point,
				"text": _map.map_names[map_index],
				"style": style,
				"rank": rank * 10000000.0 + _map.pixel_counts[map_index],
			})
		var kreis_of_map := _group_of_map(_catalog.kreis_indices)
		for label in MapLabelLayout.kreis_labels(_map, kreis_of_map, _catalog.kreis_names.size()):
			var kreis := int(label["group"])
			kreise.append({
				"world": label["point"],
				"text": _catalog.kreis_names[kreis],
				"style": MapLabels.Style.KREIS,
				"rank": float(label["pixels"]),
			})
	_map_labels.set_layer(MapDetail.Tier.PLACE, places)
	_map_labels.set_layer(MapDetail.Tier.KREIS, kreise)


## Simulationsindizes der Hauptstädte (als Menge).
func _capitals() -> Dictionary:
	var capitals := {}
	if _sim != null:
		for state_index in _sim.get_state_count():
			capitals[int(_sim.get_state(state_index).get("capital_index", -1))] = true
	return capitals


## Stil eines Ortsnamens: Hauptstadt, Stadt oder Ort (auch für den angehefteten Namen).
func _place_style(sim_index: int, capitals: Dictionary) -> MapLabels.Style:
	if capitals.has(sim_index):
		return MapLabels.Style.CAPITAL
	if _catalog.is_city[sim_index] != 0:
		return MapLabels.Style.CITY
	return MapLabels.Style.PLACE


## Staatsnamen (fern): je Staat im pixelgewichteten Schwerpunkt seiner Gebiete.
func _update_state_labels() -> void:
	var states: Array[Dictionary] = []
	if _sim != null and _map.is_valid():
		var owner_of_map := _group_of_map(_owners)
		var labels := MapLabelLayout.state_labels(
			_map, owner_of_map, _sim.get_state_count(), _map_view.location_diameter()
		)
		for label in labels:
			var state := _sim.get_state(int(label["group"]))
			var text := str(state.get("short_name", ""))
			if text.is_empty():
				text = str(state.get("name", ""))
			states.append({
				"world": label["point"],
				"text": text.to_upper(),
				"style": MapLabels.Style.STATE,
				"rank": float(label["pixels"]),
				"weight": float(label["pixels"]),
			})
	_map_labels.set_layer(MapDetail.Tier.STATE, states)


func _refresh_location_panel() -> void:
	if _sim == null or _selected < 0:
		_location_panel.visible = false
		return
	var location := _sim.get_location(_selected)
	var owner_state := _sim.get_state(int(location.get("owner_index", -1)))
	_location_panel.show_location(location, owner_state.get("color", UiPalette.PAPER_DEEP))
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
	var no_data: Array[Dictionary] = []
	if _catalog.no_data_count() > 0:
		no_data.append(LegendData.no_data_entry(_catalog.no_data_count()))
	match _map_mode:
		MapModes.MAP_POLITICAL:
			var states := LegendData.political_entries(
				_sim, LegendData.MAX_POLITICAL_ENTRIES, _player
			)
			states.append_array(no_data)
			_legend.show_swatches(title, states)
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
				"Einwohner je km²",
				no_data
			)
		MapModes.MAP_CONFESSION:
			var confessions := LegendData.category_entries(
				_sim, MapModes.MAP_CONFESSION, "majority_confession"
			)
			_legend.show_swatches(title, confessions)
		MapModes.MAP_TERRAIN:
			var terrains := LegendData.category_entries(
				_sim, MapModes.MAP_TERRAIN, "terrain", _catalog
			)
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
				"Kriegsschäden der Orte",
				no_data
			)
		MapModes.MAP_LEBENSSTANDARD:
			var living_labels := PackedStringArray(["0", "50", "100"])
			_legend.show_gradient(
				title,
				LegendData.gradient_for(
					_sim, MapModes.MAP_LEBENSSTANDARD, "living_standard", 100.0
				),
				living_labels,
				"Mittlerer Lebensstandard der Einwohner",
				no_data
			)
		MapModes.MAP_KULTUR:
			var cultures := LegendData.category_entries(
				_sim, MapModes.MAP_KULTUR, "majority_culture"
			)
			_legend.show_swatches(title, cultures)
		MapModes.MAP_KREISE:
			# Nur Kreise mit Orten und Orte mit Kreis (ein leerer Kreis ist nur eine Warnung).
			var kreis_count := _catalog.kreise_with_places()
			var text := (
				(
					"%s Kreise fassen %s zusammen. Die Farben wiederholen sich; Grenzlinien "
					+ "trennen die Kreise, ihre Namen erscheinen beim Hineinzoomen."
				)
				% [
					GermanFormat.integer(kreis_count),
					LegendData.count_text(_catalog.places_in_kreisen())
				]
			)
			if kreis_count == 0:
				text = "Die Daten enthalten keine Kreise (history/kreise fehlt oder ist leer)."
			_legend.show_palette(title, LegendData.kreis_palette(_sim, _catalog), text)
		MapModes.MAP_WIRTSCHAFT:
			var economy_max := MapModes.ECONOMY_SCALE_MAX
			var economy_labels := PackedStringArray(
				[
					"0",
					GermanFormat.number(economy_max * 0.5),
					"≥ " + GermanFormat.number(economy_max),
				]
			)
			_legend.show_gradient(
				title,
				LegendData.gradient_for(
					_sim, MapModes.MAP_WIRTSCHAFT, "production_per_capita", economy_max
				),
				economy_labels,
				"%s je Einwohner und Jahr" % _currency(),
				no_data
			)
		MapModes.MAP_MAERKTE:
			var markets := LegendData.market_entries(_sim, _market_indices)
			_legend.show_swatches(title, markets)
		MapModes.MAP_VERWALTUNG:
			var units := LegendData.admin_entries(_sim, _admin_indices, _player)
			if units.is_empty():
				_legend.show_message(
					title,
					(
						"Euer Staat hat keine Verwaltungsgliederung in den Daten. Staaten ohne "
						+ "Gliederung erscheinen in ihrer aufgehellten Farbe."
					)
					if _has_admin_units()
					else "Dieses Szenario enthält keine Verwaltungseinheiten."
				)
			else:
				_legend.show_swatches(title, units)
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
	var map_rect := _map_view.map_rect()
	var margin := OVERVIEW_MARGIN * maxf(map_rect.size.x / 1024.0, 1.0)
	_map_view.camera.fit_to(map_rect.grow(margin), area)
	_map_view.camera.map_rect = map_rect


## --zoom: Vergrößerung gegenüber der Übersicht, zentriert auf --ort oder die Kartenmitte;
## bis zum Faktor 1 bleibt die Bildmitte der Übersicht.
func _apply_launch_view() -> void:
	if _options.zoom_factor <= 0.0:
		return
	var camera := _map_view.camera
	var center := _map_view.map_rect().get_center()
	if _options.zoom_factor <= 1.0:
		center = camera.overview_center
	elif not _options.location_id.is_empty() and _sim != null:
		var point := _map_view.location_center(_sim.get_location_index(_options.location_id))
		if point.x >= 0.0:
			center = point
	camera.focus_on(center, camera.overview_zoom * _options.zoom_factor)


## Kamera auf die Hauptstadt des Spielerstaates (mittlere Zoomstufe).
func focus_capital() -> void:
	if _sim == null:
		return
	var capital := int(_sim.get_state(_player).get("capital_index", -1))
	var point := _map_view.location_center(capital)
	if point.x < 0.0:
		return
	var zoom := MapDetail.zoom_for(MapDetail.LEVEL_SIZES[1], _map_view.location_diameter())
	_map_view.camera.focus_on(point, zoom)


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
	_map_view.set_kreis_mode(
		mode == MapModes.MAP_KREISE or mode == MapModes.MAP_MAERKTE or mode == MapModes.MAP_VERWALTUNG
	)
	if not _owners.is_empty():
		_map_view.set_location_info(_owners, _border_groups())
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
	var pinned_point := _map_view.location_center(sim_index)
	if sim_index >= 0 and sim_index < _catalog.size() and pinned_point.x >= 0.0:
		var style := _place_style(sim_index, _capitals())
		_map_labels.set_pinned(pinned_point, _catalog.names[sim_index], style)
	else:
		_map_labels.set_pinned(Vector2.ZERO, "", 0)
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


## Zweite Zeile des Tooltips: Besitzer und der Wert des Kartenmodus; für Orte ohne
## Daten „Keine Daten“ mit Kreis und Fläche (bzw. Gelände).
func _tooltip_detail(location: Dictionary) -> String:
	var kreis_name := str(location.get("kreis_name", ""))
	var kreis_text := "Kreis %s" % kreis_name if not kreis_name.is_empty() else "ohne Kreis"
	var terrain := GermanFormat.humanize(str(location.get("terrain", "")))
	var first := str(location.get("owner_name", ""))
	var second := kreis_text
	if not bool(location.get("has_data", true)):
		first = "Keine Daten"
		second = GermanFormat.area(float(location.get("area_km2", 0.0)))
	match _map_mode:
		MapModes.MAP_KREISE:
			return kreis_text
		MapModes.MAP_TERRAIN:
			second = terrain
		MapModes.MAP_DENSITY when bool(location.get("has_data", true)):
			second = GermanFormat.density(float(location.get("density", 0.0)))
		MapModes.MAP_CONFESSION when bool(location.get("has_data", true)):
			second = GermanFormat.humanize(str(location.get("majority_confession", "")))
		MapModes.MAP_DEVASTATION when bool(location.get("has_data", true)):
			second = "%s verwüstet" % GermanFormat.percent(
				int(location.get("devastation_percent", 0))
			)
		MapModes.MAP_WIRTSCHAFT when bool(location.get("has_data", true)):
			second = "%s %s je Einwohner und Jahr" % [
				GermanFormat.number(float(location.get("production_per_capita", 0.0)), 1), _currency()
			]
		MapModes.MAP_MAERKTE:
			var market := str(location.get("market_name", ""))
			second = "Markt %s" % market if not market.is_empty() else "ohne Markt"
		MapModes.MAP_VERWALTUNG when bool(location.get("has_data", true)):
			var chain := PackedStringArray()
			for unit: Variant in location.get("verwaltung", []):
				if unit is Dictionary:
					chain.insert(0, str((unit as Dictionary).get("name", "")))
			second = " · ".join(chain) if not chain.is_empty() else "ohne Gliederung"
		_ when not bool(location.get("has_data", true)):
			second = "%s · %s" % [kreis_text, second]
	return "%s · %s" % [first, second]


func _on_tax_rate_requested(kind: int, percent: int) -> void:
	if _sim == null:
		return
	if not _sim.set_tax_percent(_player, kind, percent):
		_state_panel.reject_pending(
			kind, "Der Satz %s wurde nicht angenommen." % GermanFormat.percent(percent)
		)


# --- Spielstände (seit M6, docs/technik/speicherstaende.md Abschnitt 9) ----------


func _build_game_menu() -> void:
	_game_menu = GameMenu.new()
	$UI.add_child(_game_menu)
	_game_menu.save_requested.connect(save_game_as)
	_game_menu.load_requested.connect(load_game_file)
	_game_menu.quick_save_requested.connect(quick_save)
	_game_menu.quick_load_requested.connect(quick_load)
	_game_menu.close_requested.connect(close_game_menu)
	_game_menu.new_game_requested.connect(func(id: String) -> void: start_new_game(id))


## F10, F5 und F9; true, wenn das Ereignis eine dieser Tasten war.
func _handle_save_keys(event: InputEvent) -> bool:
	if event.is_action_pressed(&"spielmenue", false, true):
		toggle_game_menu()
		return true
	if event.is_action_pressed(&"schnellspeichern", false, true):
		quick_save()
		return true
	if event.is_action_pressed(&"schnellladen", false, true):
		quick_load()
		return true
	return false


## Spielmenü öffnen oder schließen (Knopf „Menü“, F10).
func toggle_game_menu() -> void:
	if _game_menu.is_open():
		close_game_menu()
	else:
		open_game_menu()


## Öffnet das Spielmenü und hält die Zeit an.
func open_game_menu() -> void:
	if _sim == null or _game_menu.is_open():
		return
	_speed_before_menu = _speed
	_set_speed(0)
	_tooltip.hide_tooltip()
	var state := _sim.get_state(_player)
	var state_name := str(state.get("short_name", ""))
	if state_name.is_empty():
		state_name = str(state.get("name", ""))
	_game_menu.show_scenarios(_sim.get_scenarios(_data_dir), str(_sim.get_scenario().get("id", "")))
	_game_menu.open(
		SaveSlots.default_name(state_name, _sim.get_date()),
		SaveSlots.list(_sim, save_dir),
		FileAccess.file_exists(SaveSlots.quicksave_path(save_dir))
	)


## Schließt das Spielmenü; resume setzt die Geschwindigkeit von vorher fort.
func close_game_menu(resume: bool = true) -> void:
	if not _game_menu.is_open():
		return
	_game_menu.close()
	if resume:
		_set_speed(_speed_before_menu)


## Speichert unter file_name (ohne Endung) im Ordner der Spielstände; leer bei Erfolg.
func save_game_as(file_name: String) -> String:
	if _sim == null:
		return "Keine Simulation geladen."
	var error := _sim.save_game(SaveSlots.file_path(save_dir, file_name), _player)
	_report_save(error, "Gespeichert als „%s“" % file_name)
	return error


## F5: Schnellspeicher überschreiben.
func quick_save() -> void:
	if _sim != null:
		_report_save(_sim.save_game(SaveSlots.quicksave_path(save_dir), _player), "Schnellgespeichert")


## F9: Schnellspeicher laden.
func quick_load() -> void:
	if _sim == null:
		return
	if not FileAccess.file_exists(SaveSlots.quicksave_path(save_dir)):
		_report_load_error("Noch kein Schnellspeicher vorhanden (F5 legt ihn an).")
		return
	load_game_file(SaveSlots.quicksave_path(save_dir))


## Lädt einen Spielstand (absoluter Pfad); die laufende Partie bleibt bei einem Fehler erhalten.
## Danach ist die Zeit angehalten und das Menü geschlossen.
func load_game_file(path: String) -> bool:
	if _sim == null:
		return false
	var error := _sim.load_game(_data_dir, path)
	if not error.is_empty():
		_report_load_error(error)
		return false
	_after_load()
	close_game_menu(false)
	_set_speed(0)
	_top_bar.show_notice("Geladen: %s (%s)" % [path.get_file().get_basename(), _sim.get_date_text()])
	print("EINIGKEIT: Spielstand geladen: %s, Prüfsumme %s" % [path, _sim.get_checksum()])
	return true


## Nach dem Laden: Spieler, Besitz, Namen, Chronik, Legende und Panels neu aufbauen. Ein
## Spielstand kann ein anderes Szenario mitbringen; dann wird die Karte neu zugeordnet (new_client:
## immer, weil ein neuer Simulationsclient gebunden werden muss).
func _after_load(new_client: bool = false) -> void:
	_player = _sim.get_player_state_index()
	if _read_scenario_constants() or new_client:
		_connect_map_and_simulation()
	_day_accumulator = 0.0
	_select_location(-1)
	_state_panel.clear_pending()
	_owners = PackedInt32Array()
	_update_place_labels()
	_update_owners()
	_chronicle_panel.clear()
	_chronicle_read = 0
	_legend_month = int(_sim.get_date().get("month", 0))
	_rebuild_legend()
	_refresh_all()


## Neues Spiel (seit M7): lädt das Szenario frisch mit seinem Spielerstaat; die laufende Partie
## bleibt bei einem Fehler erhalten. Leer bei Erfolg, sonst die Fehler.
func start_new_game(scenario_id: String, state_tag: String = "") -> String:
	if _sim == null:
		return "Keine Simulation geladen."
	var sim := SimulationClient.create()
	var errors := sim.load_scenario(_data_dir, scenario_id) if sim != null else "Kein Simulationskern."
	if not errors.is_empty():
		_report_load_error(errors)
		return errors
	_sim = sim
	if not state_tag.is_empty() and _sim.get_state_index(state_tag) >= 0:
		_sim.set_player_state(_sim.get_state_index(state_tag))
	_after_load(true)
	close_game_menu(false)
	_set_speed(0)
	var scenario := _sim.get_scenario()
	var playable := PackedStringArray()
	for tag: String in scenario.get("playable", PackedStringArray()):
		var index := _sim.get_state_index(tag)
		if index >= 0:
			playable.append(str(_sim.get_state(index).get("short_name", tag)))
	_top_bar.show_notice(
		"Neues Spiel: %s (%s)" % [str(scenario.get("name", "")), _sim.get_date_text()]
	)
	if not playable.is_empty():
		_notices.add_notice(
			"Neues Spiel: %s" % str(scenario.get("name", "")),
			(
				"Hervorgehobene Staaten: %s. Jeden anderen Staat übernimmt der Knopf "
				% ", ".join(playable)
				+ "„Diesen Staat spielen“ in der Ortsinfo."
			),
			""
		)
	print("EINIGKEIT: Neues Spiel %s, Spieler %s" % [scenario_id, _sim.get_state(_player).get("tag", "")])
	return ""


## Den Staat state_index als Spielerstaat übernehmen (seit M7, Knopf in der Ortsinfo).
func play_state(state_index: int) -> void:
	if _sim == null or not _sim.set_player_state(state_index):
		return
	_player = _sim.get_player_state_index()
	_location_panel.player_index = _player
	_state_panel.clear_pending()
	_refresh_all()
	_rebuild_legend()
	_top_bar.show_notice("Ihr spielt jetzt %s" % str(_sim.get_state(_player).get("name", "")))


## Automatischer Spielstand am 1. Januar: die letzten SaveSlots.AUTOSAVE_KEEP bleiben.
func _autosave() -> void:
	if not autosave_enabled or _sim == null:
		return
	SaveSlots.rotate_autosaves(save_dir)
	var error := _sim.save_game(SaveSlots.autosave_path(save_dir), _player)
	if error.is_empty():
		_top_bar.show_notice("Automatisch gespeichert (%s)" % _sim.get_date_text())
	else:
		printerr("EINIGKEIT: Automatischer Spielstand fehlgeschlagen: ", error)
		_top_bar.show_notice("Automatischer Spielstand fehlgeschlagen")


func _report_save(error: String, success: String) -> void:
	if error.is_empty():
		_top_bar.show_notice(success)
		if _game_menu.is_open():
			_game_menu.show_status(success + ".")
			_game_menu.show_saves(
				SaveSlots.list(_sim, save_dir),
				FileAccess.file_exists(SaveSlots.quicksave_path(save_dir))
			)
		return
	printerr("EINIGKEIT: Speichern fehlgeschlagen: ", error)
	if _game_menu.is_open():
		_game_menu.show_status("Speichern fehlgeschlagen: " + error, true)
	else:
		_notices.add_notice("Speichern fehlgeschlagen", "Der Spielstand wurde nicht geschrieben.", error)


func _report_load_error(error: String) -> void:
	printerr("EINIGKEIT: Laden fehlgeschlagen: ", error)
	if _game_menu.is_open():
		_game_menu.show_status("Laden fehlgeschlagen: " + error, true)
	else:
		_notices.add_notice(
			"Spielstand nicht geladen", "Die laufende Partie geht unverändert weiter.", error
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
	if _options.show_budget:
		_state_panel.show_budget()
	if _options.show_game_menu:
		open_game_menu()

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
