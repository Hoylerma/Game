extends Node
## Grundlage der Hauptszene (seit M13a5d aus main.gd ausgelagert): Felder, Laden, Anzeige, Bedienung,
## Spielstände und Screenshot. Darüber liegen main_windows.gd (Fenster der Partie) und main.gd (Ablauf,
## Bereiche, Einstellungen, Leisten); was diese Ebene von oben braucht, steht unten als Platzhalter.
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
	MapModes.MAP_WIRTSCHAFT, MapModes.MAP_EISENBAHNEN, MapModes.MAP_WAHLEN, MapModes.MAP_ZOLLGEBIETE,
	MapModes.MAP_BEZIEHUNGEN, MapModes.MAP_KRIEG, MapModes.MAP_UNRUHE, MapModes.MAP_BUND,
]
## Höchstzahl der Kennungen in einer Hinweismeldung.
const MAX_LISTED_IDS := 20

## true, sobald view_ready gesendet wurde.
var view_is_ready := false
## Ordner der Spielstände (seit M6); Tests setzen einen eigenen, bevor sie speichern.
var save_dir := SaveSlots.DEFAULT_DIR
## Automatischer Spielstand am 1. Januar (seit M6).
var autosave_enabled := true
## Einstellungen (seit M13a5d, vertical_slice.md 4.5); Tests setzen einen eigenen Pfad vor dem Öffnen.
var settings_path := UserSettings.PATH
var _settings: UserSettings = null
var _settings_window: SettingsWindow = null
## Halbjahr des letzten automatischen Spielstands (Jahr × 2 + 0/1).
var _autosave_half := -1
## Startbildschirm beim Start ohne Szenario- und Screenshot-Optionen (seit M13); Tests schalten ihn ab,
## bevor sie die Szene einhängen.
var start_screen_enabled := true

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
var _start_screen: StartScreen = null
var _speed_before_menu := 0
## Wahlabend (seit M8) und der Stand seiner Meldungen für die Karte.
var _election_night: ElectionNight = null
var _elections_seen := 0
var _speed_before_election := 0
## Screenshot ohne --wahlabend: eine Wahl in den simulierten Tagen öffnet keinen Wahlabend (Review M8).
var _quiet_elections := false
## Meldeposition je Ort (Wahlkreis des gezeigten Wahlabends), -1 = keiner.
var _election_ranks := PackedInt32Array()
## Gemeldete Wahlkreise des offenen Wahlabends; -1 = Karte ohne Schraffur offener Wahlkreise.
var _election_reported := -1
## Wahlkreise je Ort für die Grenzen im Kartenmodus „Wahlen“ und ihr Parlament (-2 = nicht gelesen).
var _district_indices := PackedInt32Array()
var _district_parliament := -2
## Wahltage noch nicht abgehaltener Wahlen; die Zeit hält danach an (Wahlabend).
var _pending_election_days := PackedInt32Array()
var _pending_month := -1 # Jahr × 12 + Monat der letzten Abfrage (seit M13a3)
## Parteienmodus (seit M8): gespielte Strömung (-1 = Staat), Parteipanel, Strömungen des Szenarios.
var _player_current := -1
var _party_panel: PartyPanel = null
var _currents: Array[Dictionary] = []
## Diplomatie (seit M9, Schritt b3): Fenster „Diplomatie“ und Krisenleiste über der Karte.
var _diplomacy_window: DiplomacyWindow = null
## Militär (seit M10, Schritt b3): ausgewähltes Heer (-1 keines), Staatsfarben je Index, Heere des
## letzten Tages und die Kontrolle je Ort.
var _selected_army := -1
var _state_colors := PackedColorArray()
var _armies: Array[Dictionary] = []
var _control := PackedInt32Array()
var _tooltip_index := -1
var _play_bar: PlayBar = null
## Heere des Vortags (Schlachtberichte: Stand vor der Schlacht), Heeresfenster in der rechten Spalte,
## Kriegsfenster (seit M10, Schritt b3); Schlachtberichte seit M14a3 als Meldekarten.
var _previous_armies: Array[Dictionary] = []
var _army_panel: ArmyPanel = null
var _war_window: WarWindow = null
## Der Spielerstaat ist untergegangen oder hat die Karte verlassen (Hinweis einmal, Review M10).
var _player_gone_noticed := false
## Ereignisse und Journale (seit M11, Schritt b4, ereignisse.md Abschnitt 9).
var _event_window: EventWindow = null
var _journal_window: JournalWindow = null
## Fensterverwaltung, Bereichsleiste und Jahrbuch (seit M13a5a, vertical_slice.md 4.1–4.3).
var _windows := WindowManager.new()
var _area_bar: AreaBar = null
## Schublade der Staatsübersicht (seit M14a1, europa_und_oberflaeche.md 5): beim Start zu, öffnet über den
## Bereich „Staat“, das Wappen in der oberen Leiste oder ein Fenster der Staatsübersicht.
var _state_drawer_open := false
## Breite der linken Spalte (Schublade), wie in der Szene.
const LEFT_COLUMN_WIDTH := 340.0
var _yearbook_window: YearbookWindow = null
## Fenster der Bereiche (seit M13a5b), nach Bereich.
var _area_windows: Dictionary = {}
var _areas_cache: Dictionary = {}
var _areas_player := -2
## Leisten nach Kapitel 22 (seit M13a5c): rechte Übersicht, verschachtelte Tooltips.
var _overview: OverviewPanel = null
## Meldekarten (seit M14a3, europa_und_oberflaeche.md 5): statt Ticker und Schlachtberichten.
var _messages: MessageStack = null
var _tooltips: TooltipLayer = null
var _bar_tooltips: BarTooltips = null
var _overview_filled_at := -100000
## Lexikon und Abdunkeln hinter einem Wendepunkt (seit M13a5d).
var _lexicon: Lexicon = null
var _lexicon_window: LexiconWindow = null
var _turning_backdrop: ColorRect = null
const OVERVIEW_REFRESH_MS := 500
## Urteil der Geschichte (seit M13a4) und ob es nach dem Ende schon gezeigt wurde.
var _verdict_window: VerdictWindow = null
var _newspaper_window: NewspaperWindow = null # seit M13b1
var _audio: AudioDirector = null # Musik und Klänge, seit M13b2
# Onboarding (seit M13b3): Geschichtsstunde, Hinweise, „Was ist passiert?“.
var _tutorial: TutorialPanel = null
var _tutorial_highlight: TutorialHighlight = null
var _hint_card: HintCard = null
var _recap_window: RecapWindow = null
var _tutorial_flags := {}
var _tutorial_base := {}
var _mood_month := -1 # Monat der letzten Prüfung der Stimmung
var _telegraph_day := -1000000 # Tag des letzten Telegrafenklangs
var _speed_before_newspaper := 0
var _newspaper_day := -1000000 # Tag der letzten Zeitung (höchstens eine im Monat, Gewicht 3 ausgenommen)
var _verdict_shown := false
## Letzter Tag der Partie als Tagesnummer (ElectionTally.day_number), −1 ohne Ende.
var _end_day := -1
## Tempo vor dem Ereignisfenster; nach der letzten Wahl geht es damit weiter.
var _speed_before_event := 0
## Kriege beim letzten Aufbau der Legende im Kartenmodus „Krieg“ (_war_signature).
var _legend_wars := ""
var _legend_wars_chronicle := -1

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


func _refresh_map() -> void:
	if _sim == null or not _map.is_valid():
		return
	var colors := _sim.get_map_colors(_map_mode)
	# Wahlabend (seit M8): noch nicht gemeldete Wahlkreise schraffiert (Alpha 0 wie „keine Daten“).
	if _map_mode == MapModes.MAP_WAHLEN and _election_reported >= 0:
		for i in mini(colors.size(), _election_ranks.size()):
			if _election_ranks[i] >= _election_reported:
				colors[i].a = 0.0
	_map_view.set_location_colors(Accessibility.adjust_all(colors))
	_update_war_map()


## Kartenmodus „Krieg“ (seit M10): Grenzen nach der Kontrolle (Fronten), besetzte Orte schraffiert
## in der Farbe des Besitzers; in den übrigen Modi ohne Schraffur.
func _update_war_map() -> void:
	if _map_mode != MapModes.MAP_KRIEG:
		_map_view.set_hatch_colors(PackedColorArray())
		return
	_control = _sim.get_location_control()
	var hatch := PackedColorArray()
	hatch.resize(_control.size())
	for i in _control.size():
		var owner := _owners[i] if i < _owners.size() else -1
		if _control[i] >= 0 and owner >= 0 and _control[i] != owner and owner < _state_colors.size():
			hatch[i] = _state_colors[owner]
		else:
			hatch[i] = Color(0, 0, 0, 0)
	_map_view.set_hatch_colors(hatch)
	_map_view.set_location_info(_control, _border_groups())


## Heere, Schlachten und Belagerungen auf der Karte (seit M10); Garnisonen nur im Modus „Krieg“.
func _update_armies() -> void:
	if _sim == null or not _map.is_valid() or _map_view.army_layer() == null:
		return
	if _state_colors.size() != _sim.get_state_count():
		_state_colors.resize(_sim.get_state_count())
		for i in _state_colors.size():
			_state_colors[i] = _sim.get_state(i).get("color", Color.GRAY)
	_previous_armies = _armies
	_armies = _sim.get_armies()
	_map_view.army_layer().show_garrisons = _map_mode == MapModes.MAP_KRIEG
	_map_view.set_armies(_armies, _state_colors, _player)
	_map_view.set_war_events(_sim.get_battles(), _sim.get_sieges())
	var army := selected_army_data()
	if army.is_empty():
		_selected_army = -1
	_map_view.set_army_selection(
		_selected_army, int(army.get("location_index", -1)), army.get("route", PackedInt32Array())
	)
	_refresh_army_panel(army)


## Heeresfenster des ausgewählten Heeres (seit M10); ersetzt solange die Ortsinfo.
func _refresh_army_panel(army: Dictionary) -> void:
	if army.is_empty():
		_army_panel.hide_army()
		return
	var location := int(army.get("location_index", -1))
	var destination := int(army.get("destination_index", -1))
	var own := int(army.get("state_index", -1)) == _player and _player_current < 0
	var arrival := ""
	if own and destination >= 0:
		arrival = str(_sim.get_route_preview(int(army.get("id", -1)), destination).get("arrival_text", ""))
	_army_panel.show_army(
		army, _state_short_name(int(army.get("state_index", -1))), _location_name(location),
		_location_name(destination) if destination >= 0 else "", own, arrival
	)


func _location_name(index: int) -> String:
	return _catalog.names[index] if index >= 0 and index < _catalog.names.size() else "–"


## Heer auswählen (-1: keines); die Karte zeigt seinen Weg, die rechte Spalte das Heeresfenster
## statt der Ortsinfo.
func select_army(army_id: int) -> void:
	_selected_army = army_id
	if army_id < 0:
		_map_view.set_route_preview(-1, PackedInt32Array())
	elif _selected >= 0:
		_select_location(-1)
	_update_armies()


func army_panel() -> ArmyPanel:
	return _army_panel


func selected_army() -> int:
	return _selected_army


## Daten des ausgewählten Heeres (wie get_armies) oder leer.
func selected_army_data() -> Dictionary:
	for army in _armies:
		if int(army.get("id", -1)) == _selected_army:
			return army
	return {}


## Kriege und ihre Seiten als Text (Kartenmodus „Krieg“): Die Legende folgt einem neuen Krieg oder
## Frieden am selben Tag statt am Monatsersten (Review M10); in anderen Modi leer. Kriegsbeginn und
## Frieden stehen in der Chronik: Ohne neuen Eintrag bleibt der letzte Stand (get_wars kostet einige
## Millisekunden).
func _war_signature() -> String:
	if _map_mode != MapModes.MAP_KRIEG:
		return ""
	if _sim.get_chronicle_size() == _legend_wars_chronicle and not _legend_wars.is_empty():
		return _legend_wars
	_legend_wars_chronicle = _sim.get_chronicle_size()
	var parts := PackedStringArray()
	for war in _sim.get_wars():
		parts.append("%d:%s:%s" % [int(war.get("id", -1)), str(war.get("attackers", [])), str(war.get("defenders", []))])
	return ";".join(parts)


## Hat der Spielerstaat keine Orte mehr (annektiert im Frieden, seit M10), hält die Zeit an und ein
## Hinweis nennt den Weg zu einem anderen Staat; die Partie läuft danach weiter.
func _check_player_state() -> void:
	if _player_gone_noticed or _player_current >= 0:
		return
	var state := _sim.get_state(_player)
	if state.is_empty() or int(state.get("location_count", 1)) > 0:
		return
	_player_gone_noticed = true
	_set_speed(0)
	close_war_window()
	select_army(-1)
	_notices.add_notice(
		"%s hat keine Orte mehr" % str(state.get("name", "")),
		"Der Staat hat im Frieden alle Orte auf der Karte verloren. Die Partie läuft weiter: Ein Klick "
		+ "auf einen Ort und „Diesen Staat spielen“ in der Ortsinfo übernimmt einen anderen Staat.",
		""
	)


func player_gone_noticed() -> bool:
	return _player_gone_noticed


## Befehligt der Spieler dieses Heer? Eigene Feldheere, nicht im Parteienmodus (Review M10).
func _commands_army(army: Dictionary) -> bool:
	return (
		_player_current < 0 and int(army.get("state_index", -1)) == _player
		and not bool(army.get("garrison", false))
	)


## Rechtsklick: ein eigenes ausgewähltes Feldheer marschiert zum Ort (seit M10).
func _on_location_right_clicked(sim_index: int) -> void:
	var army := selected_army_data()
	if _sim == null or army.is_empty() or sim_index < 0:
		return
	if not _commands_army(army):
		return
	var preview := _sim.get_route_preview(int(army.get("id", -1)), sim_index)
	if _sim.order_march(int(army.get("id", -1)), sim_index):
		_map_view.set_army_selection(
			_selected_army, int(army.get("location_index", -1)), preview.get("path", PackedInt32Array())
		)
		_map_view.set_route_preview(-1, PackedInt32Array())
		_top_bar.show_notice("%s marschiert nach %s: %s Tage, Ankunft %s" % [
			str(army.get("name", "")), _location_name(sim_index),
			Format.number(float(preview.get("days", 0.0)), 1), str(preview.get("arrival_text", "")),
		])
	else:
		_top_bar.show_notice("%s: kein Weg nach %s" % [str(army.get("name", "")), _location_name(sim_index)])


## Besitzer und Kreise je Ort (zwei Indexarrays der Brücke) für Grenzen und Staatsnamen;
## Staatsnamen werden nur neu berechnet, wenn sich ein Besitzer geändert hat.
func _update_owners(force: bool = false) -> void:
	if _sim == null or not _map.is_valid():
		return
	var owners := _sim.get_location_owner_indices()
	if owners == _owners and not _owners.is_empty() and not force:
		return
	_owners = owners
	if _map_mode != MapModes.MAP_KRIEG:
		_map_view.set_location_info(owners, _border_groups())
	_update_state_labels()


## Eisenbahnen auf der Karte (seit M7): eröffnete Abschnitte und die der nächsten Jahre
## (RailwayLayer.is_shown), sichtbar nur im Modus „Eisenbahnen“.
func _update_railways() -> void:
	var shown := _sim != null and _map.is_valid() and _map_mode == MapModes.MAP_EISENBAHNEN
	_map_view.set_railways_visible(shown)
	if not shown:
		return
	var date := _sim.get_date()
	var sections := PackedInt32Array()
	var open: Array[bool] = []
	for line: Variant in _sim.get_railways():
		for section: Variant in (line as Dictionary).get("sections", []):
			var entry := section as Dictionary
			if not RailwayLayer.is_shown(entry, date):
				continue
			sections.append(int(entry.get("from", -1)))
			sections.append(int(entry.get("to", -1)))
			open.append(bool(entry.get("open", false)))
	_map_view.set_railways(sections, open)


## Gruppen für die feinen Grenzlinien: Märkte in den Modi „Märkte“ und „Eisenbahnen“,
## Verwaltungseinheiten im Modus „Verwaltung“, sonst Kreise.
func _border_groups() -> PackedInt32Array:
	var market_mode := _map_mode == MapModes.MAP_MAERKTE or _map_mode == MapModes.MAP_EISENBAHNEN
	if market_mode and not _market_indices.is_empty():
		return _market_indices
	if _map_mode == MapModes.MAP_VERWALTUNG and not _admin_indices.is_empty():
		return _admin_indices
	if _map_mode == MapModes.MAP_WAHLEN:
		var districts := _election_district_indices()
		if not districts.is_empty():
			return districts
	return _catalog.kreis_indices


## Wahlkreise je Ort des Parlaments, das der Kartenmodus „Wahlen“ zeigt (seit M8); leer ohne.
func _election_district_indices() -> PackedInt32Array:
	if _sim == null:
		return PackedInt32Array()
	var parliament := _sim.get_shown_parliament()
	if parliament != _district_parliament:
		_district_parliament = parliament
		_district_indices = (
			_sim.get_location_electoral_district_indices(parliament) if parliament >= 0
			else PackedInt32Array()
		)
	return _district_indices


## Rechnungseinheit des Szenarios (seit M7), z. B. „Reichstaler“ 1648, „Taler“ 1848.
func _currency() -> String:
	return str(_sim.get_scenario().get("currency", "Reichstaler")) if _sim != null else "Reichstaler"


## Was sich nur mit dem Szenario ändert (seit M7): Märkte, Verwaltung, Bundesgebiet; bei einem
## anderen Szenario als bisher auch Ortskatalog, Farben der Ortsinfo und Steuergrenze. True, wenn
## das Szenario gewechselt hat.
func _read_scenario_constants() -> bool:
	# Militär (seit M10): Farben und Auswahl gehören zum Szenario.
	_state_colors = PackedColorArray()
	_selected_army = -1
	_armies.clear()
	_market_indices = _sim.get_location_market_indices()
	_admin_indices = _sim.get_location_admin_indices()
	var federation := false
	for state in _sim.get_state_count():
		federation = federation or bool(_sim.get_state(state).get("bund", false))
	_location_panel.show_federation = federation
	# Zollunionen (seit M9): Zeile „Zollgebiet“ und Knopf „Zollunion …“.
	var customs := not _sim.get_customs_unions().is_empty()
	_location_panel.show_customs = customs
	_state_panel.set_customs_available(customs)
	# Diplomatie (seit M9, Schritt b3): Knopf „Diplomatie …“ und Krisenleiste nur mit dem Bund.
	_state_panel.set_diplomacy_available(bool(_sim.get_diplomacy(_player).get("active", false)))
	# Journale (seit M11, Schritt b4): Knopf „Journale …“ nur in Szenarien mit Journalen.
	_state_panel.set_journals_available(not _sim.get_journals().is_empty())
	_location_panel.player_index = _player
	_areas_player = -2 # Bereiche des neuen Lesezeichens (seit M13a5b)
	# Ende der Partie (seit M13a4): Wer nach dem Ende lädt, spielt weiter, ohne Urteil beim Laden.
	var verdict := _sim.get_verdict()
	_end_day = -1
	if bool(verdict.get("has_end", false)) and bool(verdict.get("active", false)):
		_end_day = ElectionTally.day_number(verdict.get("end_date", {}))
	_verdict_shown = _sim.is_game_over()
	if _verdict_window != null:
		_verdict_window.close()
	if _newspaper_window != null:
		_newspaper_window.close()
	_newspaper_day = -1000000
	# Wahlen (seit M8): Was schon gewählt ist, öffnet keinen Wahlabend mehr.
	_elections_seen = _sim.get_election_count()
	_currents = _sim.get_currents()
	_player_current = _sim.get_player_current()
	_district_parliament = -2
	_update_pending_elections()
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
	# Wahlabend (Review M8): Ein Wahlkreis, der noch nicht gemeldet hat, verrät sein Ergebnis nicht.
	var election: Dictionary = location.get("wahl", {})
	if (
		not election.is_empty() and _election_reported >= 0
		and _election_night.rank_of_district(int(election.get("district_index", -1))) >= _election_reported
	):
		election["unreported"] = true
	_location_panel.show_location(location, owner_state.get("color", UiPalette.PAPER_DEEP))
	# Parteienmodus (seit M8): Stimmungsbericht und Wahlkampf im Wahlkreis der nächsten Wahl.
	var district := int(location.get("campaign_district", -1))
	if _player_current >= 0 and district >= 0:
		_location_panel.show_campaign(
			district,
			_sim.get_campaign(district, _player_current),
			_currents,
			str(_currents[_player_current].get("short_name", "")) if _player_current < _currents.size() else ""
		)
	else:
		_location_panel.hide_campaign()
	_fit_location_panel.call_deferred()


## Ortsinfo und Legende teilen sich die rechte Spalte; die Ortsinfo weicht aus.
func _fit_location_panel() -> void:
	# Höhe aus der Verankerung, nicht aus size.y (die wächst mit dem Inhalt und ginge sonst in die
	# nächste Rechnung ein); zwei Abstände zwischen Ortsinfo, Platzhalter und Legende (Review M8).
	var separation := float(_right_column.get_theme_constant(&"separation", &"VBoxContainer"))
	var height := _right_column.get_parent_area_size().y - _right_column.offset_top + _right_column.offset_bottom
	var available := height - _legend.get_combined_minimum_size().y - 2.0 * separation
	_location_panel.fit_height(available)


func _pull_chronicle() -> void:
	var count := _sim.get_chronicle_size()
	if count < _chronicle_read:
		_chronicle_panel.clear()
		_chronicle_read = 0
		if _messages != null:
			_messages.clear()
	if count > _chronicle_read:
		var entries := _sim.get_chronicle(_chronicle_read)
		_chronicle_panel.add_entries(entries)
		if _messages != null:
			_push_messages(entries, _chronicle_read)
		# Ein neues Spiel liest die ganze Chronik: nur Schlachten aus dem laufenden Spiel melden.
		if _chronicle_read > 0:
			_report_battles(entries)
			_report_threats(entries)
			_pause_for_entries(entries)
			_check_newspapers(_chronicle_read, count)
			_sounds_for_entries(entries)
		_chronicle_read = count


## Meldekarten (seit M14a3; vorher Ticker seit M13a5c): Wendepunkte, Einträge des Spielerstaates und der
## Welt; beim Laden nur die letzten drei davon. Schlachten mit eigenen Heeren meldet _report_battles. Die
## Art kommt aus dem Anlass der Zeitung; Krieg und Frieden des Spielerstaates bleiben liegen.
func _push_messages(entries: Array[Dictionary], first_index: int) -> void:
	var own := str(_sim.get_state(_player).get("short_name", "")) if _player >= 0 else ""
	var relevant: Array[Dictionary] = []
	for i in entries.size():
		var entry := entries[i]
		var who := int(entry.get("state_index", -1))
		if not (bool(entry.get("turning_point", false)) or (who == _player and _player >= 0) or who < 0):
			continue
		if first_index > 0 and _is_own_battle(entry):
			continue
		var item := entry.duplicate()
		item["occasion"] = str(_sim.get_front_page(first_index + i, _player).get("occasion", ""))
		var kind := MessageStack.kind_for(str(item["occasion"]), str(entry.get("text", "")))
		item["important"] = (
			_player_current < 0 and not own.is_empty() and (kind == &"krieg" or kind == &"vertrag")
			and str(entry.get("text", "")).contains(own) and str(item["occasion"]) != "mobil"
			and not str(entry.get("text", "")).contains(" mobil")
		)
		relevant.append(item)
	_messages.push(relevant if first_index > 0 else relevant.slice(maxi(relevant.size() - 3, 0)))


## Gründe der letzten Entscheidung der KI von state gegen target der Art kind (SimulationClient.AI_*,
## seit M12): Text wie „Agenda Österreich +30, Stärke gegen Österreich +12“, sonst "".
func _ai_reasons(state: int, target: int, kind: int) -> String:
	if _sim == null:
		return ""
	for decision in _sim.get_ai_decisions(state, 20):
		if int(decision.get("kind", -1)) == kind and int(decision.get("target_index", -1)) == target:
			return AiReasons.of(decision)
	return ""


## Kriegserklärungen und Forderungen der KI an den Spielerstaat als Hinweis mit den Gründen (seit M12,
## ki.md Abschnitt 9). Die Chronik schreibt „X erklärt Y den Krieg.“ und „X fordert von Y …“.
func _report_threats(entries: Array[Dictionary]) -> void:
	if _player_current >= 0 or _player < 0:
		return
	var own := str(_sim.get_state(_player).get("short_name", ""))
	if own.is_empty():
		return
	for entry in entries:
		var text := str(entry.get("text", ""))
		var state := int(entry.get("state_index", -1))
		if state < 0 or state == _player:
			continue
		var kind := -1
		if text.contains(" erklärt %s den Krieg" % own):
			kind = SimulationClient.AI_DECLARE_WAR
		elif text.contains(" fordert von %s " % own):
			kind = SimulationClient.AI_DEMAND
		if kind < 0:
			continue
		var reasons := _ai_reasons(state, _player, kind)
		# Nur der erste Satz; Kurznamen wie „Reuß ä. L.“ enthalten selbst Punkte.
		var first := text
		var cut := text.find(" (Protest)")
		if cut < 0:
			cut = text.find(" den Krieg.")
			if cut >= 0:
				cut += " den Krieg".length()
		if cut >= 0:
			first = text.substr(0, cut)
		_top_bar.show_notice(first + (" – Gründe: " + reasons if not reasons.is_empty() else ""))


## Schlachtberichte (seit M10): Schlachten, in denen ein eigenes Heer stand (am Vortag oder heute am
## Ort). Die Chronik nennt den Staat des stärksten Siegers; steht er gegen uns im Krieg, ist es eine
## Niederlage.
func _report_battles(entries: Array[Dictionary]) -> void:
	if _player_current >= 0:
		return
	for entry in entries:
		if not _is_own_battle(entry):
			continue
		var location := int(entry.get("location_index", -1))
		var victor := int(entry.get("state_index", -1))
		var victory := victor == _player or not _is_enemy(victor)
		if _messages != null:
			_messages.add_report(
				"%s bei %s" % ["Sieg" if victory else "Niederlage", _location_name(location)],
				str(entry.get("text", "")), location, victory, str(entry.get("date_text", ""))
			)


## Schlacht, in der ein eigenes Heer stand (am Vortag oder heute am Ort)?
func _is_own_battle(entry: Dictionary) -> bool:
	if _player_current >= 0 or _player < 0 or not str(entry.get("text", "")).begins_with("Schlacht bei "):
		return false
	var location := int(entry.get("location_index", -1))
	for army in _previous_armies + _armies:
		if int(army.get("state_index", -1)) == _player and int(army.get("location_index", -1)) == location:
			return true
	return false


## Steht state in einem Krieg auf der Gegenseite des Spielerstaates?
func _is_enemy(state: int) -> bool:
	for war in _sim.get_wars():
		var side := int(war.get("player_side", 0))
		if side == 0:
			continue
		var enemies: Variant = war.get("defenders" if side > 0 else "attackers", [])
		for index: int in enemies:
			if index == state:
				return true
	return false


func _rebuild_legend() -> void:
	LegendView.show(
		_legend, _sim, _map_mode, _catalog, _player, _currency() if _sim != null else "", _market_indices,
		_admin_indices, _has_admin_units() if _sim != null else false, _election_reported >= 0
	)
	if _selected >= 0:
		_fit_location_panel.call_deferred()


func _fit_camera() -> void:
	var viewport_rect := get_viewport().get_visible_rect()
	var left := _map_area_left()
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
	if _speed > 0 and _tutorial_blocks_time():
		# Geschichtsstunde (seit M13b3): Die Zeit läuft nur, wenn der Schritt es verlangt.
		_speed = 0
		_top_bar.show_notice("Die Geschichtsstunde wartet: erst die Aufgabe des Schritts")
	if _speed > 0 and _event_window != null and _event_window.is_open():
		# Die Zeit steht, bis das Ereignis entschieden ist (ereignisse.md 3.6); danach dieses Tempo.
		_speed_before_event = _speed
		_speed = 0
		_top_bar.show_notice("Erst das Ereignis entscheiden")
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
		mode in [
			MapModes.MAP_KREISE, MapModes.MAP_MAERKTE, MapModes.MAP_VERWALTUNG, MapModes.MAP_EISENBAHNEN,
			MapModes.MAP_WAHLEN
		]
	)
	if not _owners.is_empty():
		_map_view.set_location_info(_owners, _border_groups())
	_refresh_map()
	_update_armies()
	_update_railways()
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
	if sim_index >= 0 and _selected_army >= 0:
		select_army(-1)
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
	_fit_mode_bar.call_deferred()
	_place_diplomacy.call_deferred()
	_place_messages.call_deferred()


func _on_location_clicked(sim_index: int) -> void:
	_select_location(sim_index)


func _on_chronicle_location_requested(sim_index: int) -> void:
	_select_location(sim_index, true)


func _on_location_hovered(sim_index: int) -> void:
	if _sim == null or sim_index < 0:
		_tooltip.hide_tooltip()
		_map_view.set_route_preview(-1, PackedInt32Array())
		return
	var location := _sim.get_location(sim_index)
	_tooltip_index = sim_index
	var detail := _tooltip_detail(location)
	var march := _march_preview(sim_index)
	if not march.is_empty():
		detail += "\n" + march
	_tooltip.show_text(str(location.get("name", "–")), detail)


## Mit einem eigenen ausgewählten Feldheer: Weg zum Ort auf der Karte und als Text (seit M10).
func _march_preview(sim_index: int) -> String:
	var army := selected_army_data()
	if army.is_empty() or not _commands_army(army):
		_map_view.set_route_preview(-1, PackedInt32Array())
		return ""
	var preview := _sim.get_route_preview(int(army.get("id", -1)), sim_index)
	if not bool(preview.get("ok", false)):
		_map_view.set_route_preview(-1, PackedInt32Array())
		return "%s: kein Weg" % str(army.get("name", ""))
	_map_view.set_route_preview(int(army.get("location_index", -1)), preview.get("path", PackedInt32Array()))
	return "Rechtsklick: %s marschiert hierher, %s Tage (Ankunft %s)" % [
		str(army.get("name", "")), Format.number(float(preview.get("days", 0.0)), 1),
		str(preview.get("arrival_text", "")),
	]


## Zweite Zeile des Tooltips (seit M13a5d in MapTooltipText).
func _tooltip_detail(location: Dictionary) -> String:
	var text := MapTooltipText.new()
	text.sim = _sim
	text.player = _player
	text.mode = _map_mode
	text.currency = _currency()
	text.index = _tooltip_index
	text.control = _control
	text.armies = _armies
	text.short_name = _state_short_name
	text.unreported = func(district: int) -> bool:
		return _election_reported >= 0 and _election_night.rank_of_district(district) >= _election_reported
	return text.detail(location)


func _on_tax_rate_requested(kind: int, percent: int) -> void:
	if _sim == null:
		return
	if not _sim.set_tax_percent(_player, kind, percent):
		_state_panel.reject_pending(
			kind, "Der Satz %s wurde nicht angenommen." % Format.percent(percent)
		)


## Beitritt, Wechsel, Austritt oder Rücknahme (seit M9); wirksam wird der Befehl am nächsten Tag.
func _on_customs_change_requested(customs_union: int) -> void:
	if _sim == null:
		return
	if not _sim.request_customs_union(_player, customs_union):
		_state_panel.reject_customs("Der Antrag wurde nicht angenommen.")


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
	_game_menu.last_election_requested.connect(open_last_election)
	_game_menu.settings_requested.connect(
		func() -> void:
			close_game_menu(false)
			open_settings()
	)
	_game_menu.verdict_requested.connect(
		func() -> void:
			close_game_menu(false)
			open_verdict()
	)
	_game_menu.events_requested.connect(open_event_log)
	_game_menu.role_selected.connect(set_party)
	_game_menu.feedback_requested.connect(func(note: String) -> void: write_feedback(note))


# --- Screenshot ----------------------------------------------------------------


## --screenshot: simulieren, Zustand herstellen, warten, Bild speichern, beenden.
func _run_screenshot() -> void:
	_map_view.hover_enabled = false
	_tooltip.hide_tooltip()
	_quiet_elections = not _options.show_election_night
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
	if _options.show_politics:
		_state_panel.show_politics()
	if _options.show_customs:
		_state_panel.show_customs()
	if _options.diplomacy_tab >= 0:
		open_diplomacy(_options.diplomacy_tab)
	if _options.war_tab >= 0:
		open_war_window(_options.war_tab)
	if _options.select_army:
		_select_strongest_army()
	if _options.show_journal:
		open_journal()
	if _options.show_yearbook:
		open_yearbook()
	if _options.show_recap:
		_after_loading_save()
	if _options.lesson_step > 0:
		start_lesson(_options.lesson_step - 1)
	if _options.show_newspaper:
		var pages := _sim.get_front_pages(_player, 2)
		for i in range(pages.size() - 1, -1, -1):
			if int(pages[i].get("weight", 0)) >= 3 or bool(pages[i].get("involved", false)):
				open_newspaper(int(pages[i].get("chronicle_index", -1)))
				break
	if not _options.area.is_empty():
		open_area(StringName(_options.area))
	if _options.settings_tab >= 0:
		open_settings()
		_settings_window.show_tab(_options.settings_tab)
	if _options.show_game_menu:
		open_game_menu()
	if _options.show_election_night:
		open_last_election()
	if _options.start_page >= 0:
		_open_start_page(_options.start_page)

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


# --- Platzhalter für die oberen Ebenen ---------------------------------------------


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func _after_time_passed() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func _fit_mode_bar() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func _open_start_page(page: int) -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func _pause_for_entries(entries: Array[Dictionary]) -> void:
	pass


## Platzhalter; main_windows.gd überschreibt ihn (Zeitung, seit M13b1).
func _check_newspapers(_from: int, _to: int) -> void:
	pass


## Bedienklang (seit M13b2); ohne Klangregisseur still.
func _play_sound(sound_id: String) -> void:
	if _audio != null:
		_audio.play_sound(sound_id)


## Platzhalter; main.gd überschreibt ihn (Geschichtsstunde, seit M13b3): hält ein Schritt die Zeit an?
func _tutorial_blocks_time() -> bool:
	return false


## Geschichtsstunde: eine Handlung ist geschehen (z. B. "speichern").
func _tutorial_note(flag: String) -> void:
	_tutorial_flags[flag] = true


## Platzhalter; main.gd überschreibt ihn (Geschichtsstunde, seit M13b3).
func start_lesson(_from_step: int = 0) -> bool:
	return false


## Platzhalter; main.gd überschreibt ihn („Was ist passiert?“ nach dem Laden, seit M13b3).
func _after_loading_save() -> void:
	pass


## Platzhalter; main.gd überschreibt ihn (Stimmung der Musik, seit M13b2).
func _update_mood() -> void:
	pass


## Platzhalter; main.gd überschreibt ihn (Klänge zu neuen Chronikeinträgen, seit M13b2).
func _sounds_for_entries(_entries: Array[Dictionary]) -> void:
	pass


## Platzhalter; main_windows.gd überschreibt ihn (Zeitung, seit M13b1).
func open_newspaper(_chronicle_index: int) -> bool:
	return false


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func _place_diplomacy() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func _place_messages() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func _select_strongest_army() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func _state_short_name(state: int) -> String:
	return ""


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func _update_pending_elections() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func close_game_menu(resume: bool = true) -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func close_war_window() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func load_game_file(path: String) -> bool:
	return false


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func open_area(area: StringName) -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func open_diplomacy(tab: int = 0) -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func open_event_log() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func open_game_menu() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func open_journal(tab: int = -1) -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func open_last_election() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func open_settings() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func open_verdict() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func open_war_window(tab: int = 0) -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func open_yearbook() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func quick_load() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func quick_save() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func save_game_as(file_name: String) -> String:
	return ""


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func write_feedback(note: String) -> Dictionary:
	return {}


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func set_party(current: int) -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func start_new_game(scenario_id: String, state_tag: String = "", quiet: bool = false) -> String:
	return ""


# --- Schublade der Staatsübersicht (seit M14a1, europa_und_oberflaeche.md 5) ------------------


## Linker Rand der freien Karte: rechts der Bereichsleiste und, wenn offen, der Schublade.
func _map_area_left() -> float:
	var left := 12.0
	if _area_bar != null and _area_bar.visible:
		left = _area_bar.get_global_rect().end.x + 8.0
	if _left_column.visible and _left_column_shown():
		left = maxf(left, _left_column.get_global_rect().end.x + 8.0)
	return left


## Steht die linke Spalte im Bild (offene Schublade oder Parteipanel)?
func _left_column_shown() -> bool:
	return _state_drawer_open or not _state_panel.visible


func is_state_drawer_open() -> bool:
	return _state_drawer_open


## Öffnet oder schließt die Schublade; im Parteienmodus steht dort immer das Parteipanel.
func set_state_drawer(open: bool) -> void:
	_state_drawer_open = open
	if not open:
		_state_panel.hide_popups()
	_apply_state_drawer()


func toggle_state_drawer() -> void:
	set_state_drawer(not _state_drawer_open)


## Lage der linken Spalte aus Schublade und Rolle; danach alles neu anordnen. Geschlossen liegt die Spalte
## links außerhalb des Bildes, bleibt aber angeordnet: Eine unsichtbare Spalte ordnet Godot nicht an, und
## ihre umbrochenen Texte wären beim ersten Öffnen für ein Bild riesig hoch.
func _apply_state_drawer() -> void:
	if _left_column_hidden_by_overlay():
		_left_column.visible = false
	else:
		_left_column.visible = true
	if _area_bar != null:
		_area_bar.set_drawer_open(_state_drawer_open and _state_panel.visible)
	_place_diplomacy()
	_place_messages()


## Platzhalter; die Hauptszene (main_windows.gd) überschreibt ihn: Startbildschirm oder Wahlabend offen.
func _left_column_hidden_by_overlay() -> bool:
	return false

