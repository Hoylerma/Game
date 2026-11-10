extends "res://scripts/main/main_windows.gd"
## Hauptszene: verbindet Simulation (GDExtension), Karte und Oberfläche.
##
## Seit M13a5d in drei Ebenen: main_core.gd (Laden, Anzeige, Bedienung, Spielstände, Screenshot),
## main_windows.gd (Fenster der Partie) und diese Datei (Ablauf, Bereiche, Einstellungen, Leisten).


# --- Ablauf ------------------------------------------------------------------


func _ready() -> void:
	_options = LaunchOptions.from_command_line()
	for problem in _options.problems:
		printerr("EINIGKEIT: ", problem)
	_data_dir = UhrwerkPaths.data_dir()
	print("EINIGKEIT: Datenordner %s (%s)" % [_data_dir, UhrwerkPaths.data_dir_origin()])

	EpochTheme.apply(ThemeDB.get_project_theme()) # Schriften der Epoche (seit M13b1)
	_top_bar.configure_speeds(SPEED_DAYS_PER_SECOND)
	_top_bar.speed_selected.connect(_set_speed)
	_top_bar.menu_requested.connect(toggle_game_menu)
	_build_game_menu()
	_build_election_night()
	_build_party_panel()
	_build_diplomacy()
	_build_war()
	_build_events()
	_build_areas()
	_build_bars()
	_build_settings()
	_build_verdict()
	_build_newspaper()
	_build_start_screen()
	_state_panel.tax_rate_requested.connect(_on_tax_rate_requested)
	_state_panel.politics_source = func() -> Dictionary:
		return _sim.get_state_politics(_player) if _sim != null else {}
	_state_panel.events_source = func() -> Dictionary:
		return _sim.get_state_events(_player) if _sim != null else {}
	_state_panel.persons_source = func() -> Array:
		return _sim.get_state_persons(_player) if _sim != null else []
	_state_panel.intentions_source = func() -> Dictionary:
		return _sim.get_intentions(_player) if _sim != null else {}
	_state_panel.legacy_source = func() -> Dictionary:
		return _sim.get_legacy(_player) if _sim != null else {}
	_state_panel.customs_change_requested.connect(_on_customs_change_requested)
	_state_panel.customs_source = func() -> Array:
		return _sim.get_customs_unions() if _sim != null else []
	_state_panel.customs_preview = func(customs_union: int) -> Dictionary:
		return _sim.get_customs_change(_player, customs_union) if _sim != null else {}
	_chronicle_panel.location_requested.connect(_on_chronicle_location_requested)
	_location_panel.close_requested.connect(_select_location.bind(-1))
	_location_panel.play_state_requested.connect(play_state)
	_location_panel.campaign_requested.connect(_on_campaign_requested)
	_mode_bar.mode_selected.connect(_set_map_mode)
	_map_view.location_hovered.connect(_on_location_hovered)
	_map_view.location_clicked.connect(_on_location_clicked)
	_map_view.location_right_clicked.connect(_on_location_right_clicked)
	_map_view.army_clicked.connect(select_army)
	get_viewport().size_changed.connect(_on_viewport_size_changed)

	_load_map()
	_load_simulation()
	_connect_map_and_simulation()
	_map_labels.camera = _map_view.camera
	_map_labels.location_diameter = _map_view.location_diameter()
	_map_labels.set_covering_controls([
		_top_bar, _state_panel, _party_panel, _chronicle_panel, _location_panel, _legend, _mode_bar,
		_election_night, _diplomacy_window, _play_bar, _army_panel, _war_window, _battle_reports,
		_event_window, _journal_window, _area_bar, _yearbook_window, _overview, _ticker,
	])
	_update_place_labels()
	_update_owners()
	if _sim != null:
		_legend_month = int(_sim.get_date().get("month", 0))
	_set_map_mode(MapModes.MAP_POLITICAL)
	_refresh_all()
	_apply_party_option()
	_sync_event_player()

	# Erst nach dem ersten Layout sind die Panelgrößen bekannt.
	await get_tree().process_frame
	_fit_mode_bar()
	_fit_camera()
	_apply_launch_view()
	view_is_ready = true
	if _wants_start_screen():
		open_start_screen()
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
	# Höchstens bis einschließlich zum nächsten Wahltag: Danach öffnet der Wahlabend (seit M8).
	step = mini(step, _days_through_next_election())
	# Höchstens bis einschließlich zum letzten Tag der Partie: Danach folgt das Urteil (seit M13a4).
	step = mini(step, _days_through_end())
	_day_accumulator += capped - step
	_sim.advance_days(step)
	_after_time_passed()
	_maybe_autosave()
func _unhandled_input(event: InputEvent) -> void:
	if _handle_save_keys(event):
		get_viewport().set_input_as_handled()
		return
	if _game_menu != null and _game_menu.is_open():
		return # Solange das Menü offen ist, ruhen Karte und Zeit (Escape schließt es, GameMenu).
	if _start_screen != null and _start_screen.is_open():
		return # Startbildschirm (seit M13): Karte und Zeit ruhen.
	for speed in SPEED_ACTIONS.size():
		if event.is_action_pressed(SPEED_ACTIONS[speed], false, true):
			if speed == 0:
				_toggle_pause()
			else:
				_set_speed(speed)
			get_viewport().set_input_as_handled()
			return
	if event.is_action_pressed(&"journal", false, true):
		toggle_journal()
		get_viewport().set_input_as_handled()
		return
	if event.is_action_pressed(&"jahrbuch", false, true):
		toggle_yearbook()
		get_viewport().set_input_as_handled()
		return
	# Escape schließt das zuletzt geöffnete Fenster eines Bereichs (seit M13a5a).
	if event.is_action_pressed(&"auswahl_aufheben", false, true) and _windows.close_top():
		_sync_area_bar()
		get_viewport().set_input_as_handled()
		return
	if event.is_action_pressed(&"auswahl_aufheben", false, true) and _selected_army >= 0:
		select_army(-1)
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
	_update_area_bar()
	_refresh_bars()
	if _player_current >= 0:
		_party_panel.show_party(_sim.get_party(_player_current))
	_refresh_diplomacy()
	_refresh_location_panel()
	_refresh_map()
	_update_armies()
	_war_window.refresh()
	_journal_window.refresh()
	for window: Variant in _area_windows.values():
		(window as AreaWindow).refresh()
	if _yearbook_window.is_open() and int(_sim.get_date().get("day", 0)) == 2:
		_yearbook_window.refresh() # die Jahresreihe des 1. Januar ist da
	_pull_chronicle()
func _after_time_passed() -> void:
	# Befehle des Vortags sind ausgeführt oder verworfen (Generalstab, seit M10).
	_army_panel.clear_pending()
	if _sim.get_election_count() > _elections_seen:
		_elections_seen = _sim.get_election_count()
		_update_pending_elections()
		_district_parliament = -2
		_update_owners(true)
		if not _quiet_elections and (_settings == null or _settings.pause_on("pause_wahl")):
			_open_election_night(_elections_seen - 1, _options.wants_screenshot() or Accessibility.reduced_motion)
		elif not _quiet_elections:
			_top_bar.show_notice("Wahl: %s (Spielmenü „Letzte Wahl“)" % str(_sim.get_election(_elections_seen - 1).get("parliament_name", "")))
	# Ende der Partie (seit M13a4): einmal das Urteil der Geschichte, die Zeit hält an.
	if not _verdict_shown and _end_day >= 0 and _sim.is_game_over():
		_verdict_shown = true
		open_verdict()
	else:
		# Ein Bund kann gegründet werden; seine erste Wahl liegt Monate später (seit M13a3).
		var now := _sim.get_date()
		if int(now.get("year", 0)) * 12 + int(now.get("month", 0)) != _pending_month:
			_update_pending_elections()
	# Besitzer jeden Tag (Review M10: ein Frieden tritt Orte mitten im Monat ab); Grenzen und
	# Staatsnamen ändern sich nur, wenn sich ein Besitzer geändert hat.
	_update_owners()
	_refresh_all()
	_check_player_state()
	var month := int(_sim.get_date().get("month", 0))
	var wars := _war_signature()
	if month != _legend_month or wars != _legend_wars:
		_legend_month = month
		_legend_wars = wars
		if DYNAMIC_LEGEND_MODES.has(_map_mode):
			_rebuild_legend()
	# Eröffnungen fallen nicht auf Monatserste; die Linien folgen den Farben jeden Tag.
	if _map_mode == MapModes.MAP_EISENBAHNEN:
		_update_railways()
	var hovered := _map_view.get_hovered()
	if hovered >= 0:
		_on_location_hovered(hovered)
	_check_open_events()
# --- Bereiche und Jahrbuch (seit M13a5a, vertical_slice.md 4.1–4.3) ---------------------


func _build_areas() -> void:
	_area_bar = AreaBar.new()
	var bar_root: Control = $UI/Root
	bar_root.add_child(_area_bar)
	bar_root.move_child(_area_bar, _top_bar.get_index())
	_area_bar.resized.connect(_place_diplomacy)
	_area_bar.area_requested.connect(open_area)
	_state_panel.set_area_bar(true)
	_yearbook_window = YearbookWindow.new()
	var root: Control = $UI/Root
	root.add_child(_yearbook_window)
	root.move_child(_yearbook_window, _top_bar.get_index())
	_yearbook_window.close_requested.connect(close_yearbook)
	_yearbook_window.newspaper_requested.connect(func(index: int) -> void: open_newspaper(index))
	_yearbook_window.resized.connect(_place_diplomacy)
	for window: Control in [_diplomacy_window, _war_window, _journal_window, _yearbook_window]:
		_windows.add(window)
	# Fenster der Bereiche (seit M13a5b).
	for entry: Array in [
		[&"kabinett", CabinetWindow.new()], [&"gesetze", LawsWindow.new()], [&"gruppen", GroupsWindow.new()],
		[&"parlament", ParliamentWindow.new()], [&"gesellschaft", SocietyWindow.new()],
		[&"wirtschaft", EconomyWindow.new()], [&"chronik", ChronicleWindow.new()],
	]:
		var window: AreaWindow = entry[1]
		root.add_child(window)
		root.move_child(window, _top_bar.get_index())
		window.close_requested.connect(
			func() -> void:
				window.close()
				_sync_area_bar()
		)
		window.resized.connect(_place_diplomacy)
		_windows.add(window)
		_area_windows[entry[0]] = window
	(_area_windows[&"chronik"] as ChronicleWindow).location_requested.connect(
		func(location: int) -> void: _select_location(location, true)
	)
	# Lexikon (seit M13a5d): Artikel aus data/lexikon.
	_lexicon = Lexicon.load_dir(UhrwerkPaths.data_dir().path_join("lexikon"))
	for problem in _lexicon.problems:
		push_warning("EINIGKEIT: Lexikon: " + problem)
	_lexicon_window = LexiconWindow.new()
	root.add_child(_lexicon_window)
	root.move_child(_lexicon_window, _top_bar.get_index())
	_lexicon_window.close_requested.connect(
		func() -> void:
			_lexicon_window.close()
			_sync_area_bar()
	)
	_lexicon_window.resized.connect(_place_diplomacy)
	_windows.add(_lexicon_window)


## Bereichsleiste (Zugriff für Tests).
func area_bar() -> AreaBar:
	return _area_bar


## Jahrbuch (Zugriff für Tests).
func yearbook_window() -> YearbookWindow:
	return _yearbook_window


## Fensterverwaltung (Zugriff für Tests).
func window_manager() -> WindowManager:
	return _windows


## Leiste der Kartenmodi in zwei Reihen, wenn eine nicht zwischen die Spalten passt (seit M13a5a); der
## Bereich darüber wächst mit, damit Fenster und Schlachtberichte über ihr enden.
func _fit_mode_bar() -> void:
	# Die Breite aus den Abständen: Der CenterContainer wächst mit einer zu breiten Leiste mit.
	var root_width := ($UI/Root as Control).size.x
	var available := root_width - _mode_bar_area.offset_left + _mode_bar_area.offset_right
	_mode_bar.set_rows(1)
	if _mode_bar.get_combined_minimum_size().x > available:
		_mode_bar.set_rows(2)
	var height := _mode_bar.get_combined_minimum_size().y
	_mode_bar_area.offset_top = -12.0 - maxf(height, 52.0)
	_place_ticker.call_deferred()
	_place_diplomacy.call_deferred()
	_place_reports.call_deferred()


## Fenster eines Bereichs (Tests): kabinett, gesetze, gruppen, parlament, gesellschaft, wirtschaft.
func area_window(area: StringName) -> AreaWindow:
	return _area_windows.get(area) as AreaWindow


## Öffnet oder schließt das Fenster eines Bereichs (seit M13a5b).
func toggle_area_window(area: StringName) -> void:
	var window := _area_windows.get(area) as AreaWindow
	if window == null or _sim == null:
		return
	if window.is_open():
		window.close()
	else:
		_state_panel.hide_popups()
		_windows.opening(window)
		window.open(_sim, _player)
		_place_diplomacy()
	_sync_area_bar()


## Klick in der Bereichsleiste: Fenster oder Ansicht des Bereichs (ein zweiter Klick schließt es).
func open_area(area: StringName) -> void:
	if _area_windows.has(area):
		toggle_area_window(area)
		return
	match area:
		&"haushalt":
			_state_panel.show_budget()
		&"zollunion":
			_state_panel.show_customs()
		&"diplomatie":
			toggle_diplomacy()
		&"militaer":
			toggle_war_window()
		&"journale":
			toggle_journal()
		&"jahrbuch":
			toggle_yearbook()
		&"lexikon":
			if _lexicon_window.is_open():
				_lexicon_window.close()
			else:
				open_lexicon()
	_sync_area_bar()


## Welche Bereiche es im Lesezeichen und für den Spielerstaat gibt.
func _update_area_bar() -> void:
	var areas := _state_panel.available_areas()
	areas.merge(_scenario_areas(), true)
	areas[&"chronik"] = true
	areas[&"lexikon"] = _lexicon != null and not _lexicon.articles.is_empty()
	_area_bar.set_available(areas)
	_area_bar.visible = _state_panel.visible
	# Ein offenes Fenster folgt dem Spielerstaat (Ortsinfo „Diesen Staat spielen“).
	for window: Variant in _area_windows.values():
		var w := window as AreaWindow
		if w.is_open() and w.player_index() != _player:
			w.open(_sim, _player)
	_sync_area_bar()


## Bereiche, die vom Lesezeichen und vom Spielerstaat abhängen; neu nur mit einem anderen Spielerstaat.
func _scenario_areas() -> Dictionary:
	if _sim == null:
		return {}
	if _areas_player == _player:
		return _areas_cache
	_areas_player = _player
	var politics := _sim.get_state_politics(_player)
	_areas_cache = {
		&"kabinett": not _sim.get_state_persons(_player).is_empty(),
		&"gesetze": not _sim.get_state_laws(_player).is_empty() or not politics.is_empty(),
		&"gruppen": not (politics.get("interest_groups", []) as Array).is_empty(),
		&"parlament": not _sim.get_parliaments().is_empty(),
		&"gesellschaft": true,
		&"wirtschaft": true,
		&"jahrbuch": bool(_sim.get_legacy(_player).get("active", false)),
	}
	return _areas_cache


## Gedrückt erscheint der Bereich mit offenem Fenster.
func _sync_area_bar() -> void:
	if _area_bar == null:
		return
	var open := _windows.open_window()
	var area := &""
	if open == _diplomacy_window:
		area = &"diplomatie"
	elif open == _war_window:
		area = &"militaer"
	elif open == _journal_window:
		area = &"journale"
	elif open == _yearbook_window:
		area = &"jahrbuch"
	else:
		for key: Variant in _area_windows:
			if _area_windows[key] == open:
				area = key
	_area_bar.set_active(area)
	if open == _lexicon_window:
		_area_bar.set_active(&"lexikon")


func open_yearbook() -> void:
	if _sim == null or not bool(_sim.get_legacy(_player).get("active", false)):
		return
	_state_panel.hide_popups()
	_windows.opening(_yearbook_window)
	_yearbook_window.open(_sim, _player)
	_place_diplomacy()
	_sync_area_bar()


func close_yearbook() -> void:
	if _yearbook_window != null:
		_yearbook_window.close()
		_sync_area_bar()


func toggle_yearbook() -> void:
	if _yearbook_window.is_open():
		close_yearbook()
	else:
		open_yearbook()


# --- Einstellungen und Pausenregeln (seit M13a5d, vertical_slice.md 4.4 und 4.5) ---------


func _build_settings() -> void:
	_settings = UserSettings.new(settings_path)
	_settings.load_file()
	if _options.color_blind:
		_settings.set_value("zugang/farbsehschwaeche", true) # nur für diesen Start, ohne zu speichern
	_settings.apply(get_tree())
	_settings.changed.connect(_on_setting_changed)
	_settings_window = SettingsWindow.new()
	$UI.add_child(_settings_window)
	_settings_window.close_requested.connect(close_settings)
	_settings_window.minimum_size_changed.connect(_place_settings)
	_event_window.set_compass(bool(_settings.get_value("spiel/kompass")))
	_event_window.compass_changed.connect(
		func(on: bool) -> void:
			_settings.set_value("spiel/kompass", on)
			_settings.save_file()
	)


## Einstellungen (Zugriff für Tests).
func settings() -> UserSettings:
	return _settings


func settings_window() -> SettingsWindow:
	return _settings_window


## Einstellungen aus einer anderen Datei (Tests: nicht die des Benutzers verändern).
func use_settings_file(file_path: String) -> void:
	settings_path = file_path
	_settings.path = file_path
	_settings.load_file()
	_settings.apply(get_tree())


## Öffnet das Fenster Einstellungen über allem; die Zeit hält an.
func open_settings() -> void:
	_set_speed(0)
	_tooltips.hide_all()
	_settings_window.open(_settings)
	$UI.move_child(_settings_window, _tooltips.get_index())
	_place_settings()


func close_settings() -> void:
	_settings_window.close()


func _place_settings() -> void:
	if _settings_window == null or not _settings_window.is_open():
		return
	var area := get_viewport().get_visible_rect().size
	_settings_window.size = _settings_window.get_combined_minimum_size()
	_settings_window.position = ((area - _settings_window.size) * 0.5).round()


func _on_setting_changed(key: String) -> void:
	if key == "spiel/kompass":
		_event_window.set_compass(bool(_settings.get_value(key)))
	if key.begins_with("zugang/farbsehschwaeche"):
		_refresh_map()
		_rebuild_legend()
	if key.begins_with("zugang/") or key.begins_with("bild/"):
		_place_diplomacy.call_deferred()


## Automatische Pause nach den Regeln (Abschnitt 4.4) aus den neuen Einträgen der Chronik: Krieg, Forderung
## an den Spielerstaat, Frieden, Amtswechsel im eigenen Staat, Bundesgründung. Ereignisse des Spielers und
## Wahlen halten die Zeit auf ihrem eigenen Weg an.
func _pause_for_entries(entries: Array[Dictionary]) -> void:
	if _settings == null or _player_current >= 0 or _player < 0 or _speed == 0:
		return
	var own := str(_sim.get_state(_player).get("short_name", ""))
	if own.is_empty():
		return
	var reason := PauseRules.reason(entries, own, _player, _settings)
	if not reason.is_empty():
		_set_speed(0)
		_top_bar.show_notice("Pause: " + reason)


# --- Leisten und Tooltips (seit M13a5c, vertical_slice.md 4.1 und 4.3) -------------------


func _build_bars() -> void:
	var root: Control = $UI/Root
	_overview = OverviewPanel.new()
	_right_column.add_child(_overview)
	_right_column.move_child(_overview, _location_panel.get_index() + 1)
	_overview.entry_requested.connect(_on_overview_entry)
	_ticker = TickerBar.new()
	root.add_child(_ticker)
	root.move_child(_ticker, _top_bar.get_index())
	_ticker.location_requested.connect(func(location: int) -> void: _select_location(location, true))
	_ticker.resized.connect(_place_ticker)
	# Die Chronik ist über die Bereichsleiste erreichbar; der Ticker zeigt das Neueste.
	_chronicle_panel.visible = false
	_tooltips = TooltipLayer.new()
	$UI.add_child(_tooltips)
	_tooltips.lexicon = _lexicon
	_tooltips.link_clicked.connect(
		func(meta: String) -> void:
			if meta.begins_with("begriff:") and _lexicon.has(meta.substr(8)):
				_tooltips.hide_all()
				open_lexicon(meta.substr(8))
	)
	# Wendepunkte (seit M13a5d): die Karte dunkelt hinter dem großen Ereignisfenster ab.
	_turning_backdrop = ColorRect.new()
	_turning_backdrop.name = "TurningPointBackdrop"
	_turning_backdrop.color = Color(0.1, 0.07, 0.04, 0.45)
	_turning_backdrop.set_anchors_preset(Control.PRESET_FULL_RECT)
	_turning_backdrop.mouse_filter = Control.MOUSE_FILTER_STOP
	_turning_backdrop.visible = false
	$UI.add_child(_turning_backdrop)
	$UI.move_child(_turning_backdrop, _event_window.get_index())
	_bar_tooltips = BarTooltips.new()
	_bar_tooltips.sim_source = func() -> SimulationClient: return _sim
	_bar_tooltips.player_source = func() -> int: return _player
	_bar_tooltips.location_name = _location_name
	_tooltips.attach(_top_bar.field(&"kasse"), _bar_tooltips.cash)
	_tooltips.attach(_top_bar.field(&"legitimitaet"), _bar_tooltips.legitimacy)
	_tooltips.attach(_top_bar.field(&"verwaltung"), _bar_tooltips.administration)
	_tooltips.attach(_top_bar.field(&"vermaechtnis"), _bar_tooltips.legacy, _bar_tooltips.legacy_link)


## Öffnet das Lexikon, optional bei einem Artikel (Kennung aus data/lexikon).
func open_lexicon(id: String = "") -> void:
	if _lexicon == null or _lexicon.articles.is_empty():
		return
	_state_panel.hide_popups()
	_windows.opening(_lexicon_window)
	_lexicon_window.open(_lexicon, id)
	_place_diplomacy()
	_sync_area_bar()


func lexicon_window() -> LexiconWindow:
	return _lexicon_window


func close_lexicon_window() -> void:
	_lexicon_window.close()
	_sync_area_bar()


## Rechte Übersicht, Ticker und Tooltips (Tests).
func overview_panel() -> OverviewPanel:
	return _overview


func ticker() -> TickerBar:
	return _ticker


func tooltip_layer() -> TooltipLayer:
	return _tooltips


func bar_tooltips() -> BarTooltips:
	return _bar_tooltips


## Untere Kante für Fenster: über dem Ticker (sonst über der Leiste der Kartenmodi).
func _bottom_of_map_area() -> float:
	if _ticker != null and _ticker.visible:
		return _ticker.position.y - 8.0
	return _mode_bar_area.position.y - 8.0


## Ticker über der Leiste der Kartenmodi, so breit wie der Platz zwischen den Spalten.
func _place_ticker() -> void:
	if _ticker == null:
		return
	var left := _left_column.offset_right + 12.0
	var right := _right_column.get_global_rect().position.x - 12.0
	var height := _ticker.get_combined_minimum_size().y
	_ticker.size = Vector2(maxf(right - left, 100.0), height)
	_ticker.position = Vector2(left, _mode_bar_area.get_global_rect().position.y - height - 4.0)
	_place_diplomacy.call_deferred()


## Obere Leiste und rechte Übersicht höchstens zweimal je Sekunde.
func _refresh_bars(force: bool = false) -> void:
	if _sim == null or _overview == null:
		return
	if not force and Time.get_ticks_msec() - _overview_filled_at < OVERVIEW_REFRESH_MS:
		return
	_overview_filled_at = Time.get_ticks_msec()
	_top_bar.show_overview(_sim.get_state_overview(_player), _sim.get_legacy(_player))
	if _player_current >= 0:
		_overview.show_entries([] as Array[Dictionary])
		return
	_overview.show_entries(OverviewPanel.collect(_sim, _player, str(_sim.get_state(_player).get("tag", ""))))
	if _location_panel.visible:
		_overview.visible = false


func _on_overview_entry(kind: StringName, value: int) -> void:
	match kind:
		&"event":
			_check_open_events()
		&"crisis":
			open_diplomacy(0)
		&"war":
			open_war_window(0)
		&"journal":
			open_journal(value)
		&"election":
			if not (_area_windows[&"parlament"] as AreaWindow).is_open():
				toggle_area_window(&"parlament")
			(_area_windows[&"parlament"] as ParliamentWindow).select_parliament(value)
		&"army":
			select_army(value)
			focus_army(value)
