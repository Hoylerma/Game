extends "res://scripts/main/main_windows.gd"
## Hauptszene: verbindet Simulation (GDExtension), Karte und Oberfläche.
##
## Seit M13a5d in drei Ebenen: main_core.gd (Laden, Anzeige, Bedienung, Spielstände, Screenshot),
## main_windows.gd (Fenster der Partie) und diese Datei (Ablauf, Bereiche, Einstellungen, Leisten).


# --- Ablauf ------------------------------------------------------------------

var _frame_meter: FrameMeter = null


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
	_top_bar.state_requested.connect(func() -> void: open_area(&"staat"))
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
	_build_audio()
	_build_start_screen()
	_build_onboarding()
	_frame_meter = FrameMeter.new() # Bildrate für die Rückmeldung (seit M13b6)
	add_child(_frame_meter)
	_state_panel.tax_rate_requested.connect(_on_tax_rate_requested)
	# Seit M14a1: Die Staatsübersicht ist eine Schublade, beim Start zu; ihre Fenster öffnen sie.
	_state_panel.popup_opening.connect(
		func() -> void:
			if not _state_drawer_open:
				set_state_drawer(true)
	)
	_apply_state_drawer()
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
		_election_night, _diplomacy_window, _play_bar, _army_panel, _war_window, _messages,
		_event_window, _journal_window, _area_bar, _yearbook_window, _overview,
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
	_update_mood() # Musik von Anfang an (seit M13b2)
	if _wants_start_screen():
		open_start_screen()
	view_ready.emit()
	if _options.smoke_test:
		_run_smoke_test()
	elif _options.wants_screenshot():
		_run_screenshot()
func _process(delta: float) -> void:
	# Geschichtsstunde (seit M13b3): die Aufgabe des Schritts prüfen, auch bei angehaltener Zeit.
	if _tutorial != null and _tutorial.is_active() and not _tutorial.is_done():
		if _tutorial_check(_tutorial.current_step().check):
			_tutorial.mark_done()
	# Freien Platz halten, wenn Ortsinfo oder Fenster auf- und zugehen (seit dem Spieltest 11/2026).
	if _tutorial != null and _tutorial.visible:
		_place_tutorial()
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
	# Escape schließt das zuletzt geöffnete Fenster eines Bereichs (seit M13a5a), hebt dann die Auswahl auf
	# (Heer, Ort) und schließt zuletzt die Schublade der Staatsübersicht (seit M14a1).
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
	if event.is_action_pressed(&"auswahl_aufheben", false, true) and _state_drawer_open and _state_panel.visible:
		set_state_drawer(false)
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
	# Stimmung der Musik einmal im Monat (seit M13b2).
	if month != _mood_month:
		_mood_month = month
		_update_mood()
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
	_place_diplomacy.call_deferred()
	_place_messages.call_deferred()


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
		&"staat":
			if _sim != null and _state_panel.visible:
				toggle_state_drawer()
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
	_area_bar.set_drawer_open(_state_drawer_open and _state_panel.visible)


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
	if not _options.language.is_empty():
		Translation_.use(_options.language) # nur für diesen Start, ohne zu speichern
	_settings.changed.connect(_on_setting_changed)
	# Legende ein- oder ausgeklappt wie zuletzt (seit M14a1).
	_legend.set_collapsed(bool(_settings.get_value("anzeige/legende_zu")))
	_legend.collapsed_changed.connect(
		func(collapsed: bool) -> void:
			_settings.set_value("anzeige/legende_zu", collapsed)
			_settings.save_file()
			_fit_location_panel.call_deferred()
	)
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


# --- Auslieferung (seit M13b5, vertical_slice.md 7) --------------------------------------------


## Rückmeldung (Spielmenü): ZIP in user://rueckmeldungen; der Pfad steht in der Statuszeile.
func write_feedback(note: String) -> Dictionary:
	var frames := _frame_meter.summary() if _frame_meter != null else {}
	var result := FeedbackReport.write(_sim, _player, note, FeedbackReport.DIR, frames)
	var error := str(result.get("error", ""))
	if error.is_empty():
		var text := tr("Rückmeldung gespeichert: {pfad} – bitte diese Datei schicken; es wurde nichts versandt.")
		_game_menu.show_status(text.format({"pfad": result["path"]}))
	else:
		_game_menu.show_status(error, true)
	return result


## --rauchtest: Wie uhrwerk-cli lauf ohne Spieler rechnen (keine Ereignisse warten auf eine Wahl), die
## Prüfsumme ausgeben und beenden. Exitcode 1 ohne Simulation oder Karte.
func _run_smoke_test() -> void:
	if _sim == null or not _map_view.has_map():
		printerr("EINIGKEIT: Rauchtest fehlgeschlagen: Simulation oder Karte nicht geladen (Daten: %s)" % _data_dir)
		get_tree().quit(1)
		return
	_sim.set_event_player(-1)
	_sim.advance_days(maxi(_options.days, 0))
	print("EINIGKEIT: Rauchtest %s %s Version %s Prüfsumme %s" % [
		str(_sim.get_scenario().get("id", "")), _sim.get_date_text(), ProjectSettings.get_setting("application/config/version"),
		_sim.get_checksum(),
	])
	get_tree().quit(0)


# --- Onboarding (seit M13b3, vertical_slice.md 5.4) -----------------------------------------

const LESSON_PATH := "res://tutorial/maerz_1848.tres"


func _build_onboarding() -> void:
	var root: Control = $UI/Root
	_recap_window = RecapWindow.new()
	root.add_child(_recap_window)
	root.move_child(_recap_window, _top_bar.get_index())
	_recap_window.close_requested.connect(
		func() -> void:
			_recap_window.close()
			_sync_area_bar()
	)
	_recap_window.resized.connect(_place_diplomacy)
	_windows.add(_recap_window)
	_tutorial_highlight = TutorialHighlight.new()
	root.add_child(_tutorial_highlight)
	_tutorial_highlight.visible = false
	_tutorial_highlight.target = func() -> Control:
		var step := _tutorial.current_step()
		return tutorial_target(step.target) if step != null else null
	_tutorial = TutorialPanel.new()
	root.add_child(_tutorial)
	_tutorial.step_entered.connect(_on_tutorial_step)
	_tutorial.closed.connect(_on_tutorial_closed)
	_tutorial.lexicon_requested.connect(func(article: String) -> void: open_lexicon(article))
	_tutorial.resized.connect(_place_tutorial)
	_hint_card = HintCard.new()
	root.add_child(_hint_card)
	_hint_card.resized.connect(_place_hint)
	_hint_card.disable_requested.connect(
		func() -> void:
			_settings.set_value("spiel/hinweise", false)
			_settings.save_file()
			_top_bar.show_notice("Hinweise abgeschaltet (Einstellungen, Spiel)")
	)
	# Hinweise beim ersten Öffnen: Fenster der Fensterverwaltung und die übrigen Fenster der Partie.
	_windows.window_opened.connect(func(window: Control) -> void: _offer_hint(str(window.name)))
	for window: Control in [_event_window, _newspaper_window, _election_night, _verdict_window]:
		window.visibility_changed.connect(
			func() -> void:
				if window.visible:
					_offer_hint(str(window.name))
		)
	_area_bar.area_requested.connect(
		func(area: StringName) -> void:
			_tutorial_note("bereich:" + str(area))
			if area == &"haushalt":
				_offer_hint("Haushalt")
			elif area == &"zollunion":
				_offer_hint("Zollunion")
	)
	_start_screen.lesson_requested.connect(func() -> void: start_lesson())
	# Handlungen, die Schritte der Geschichtsstunde prüfen.
	_state_panel.tax_rate_requested.connect(func(_k: int, _p: int) -> void: _tutorial_note("steuern"))
	_diplomacy_window.envoy_requested.connect(
		func(_t: int, send: bool) -> void:
			if send:
				_tutorial_note("gesandter")
	)
	_event_window.option_chosen.connect(func(_e: int, _o: int) -> void: _tutorial_note("ereignis"))
	get_viewport().size_changed.connect(_place_tutorial)


## Geschichtsstunde (Tests).
func tutorial_panel() -> TutorialPanel:
	return _tutorial


func hint_card() -> HintCard:
	return _hint_card


func recap_window() -> RecapWindow:
	return _recap_window


## Startet die Geschichtsstunde „März 1848 in Berlin“: neues Spiel als Preußen, Stufe Lehrling, Kompass an;
## from_step > 0 springt zu einem Schritt (Screenshots). false, wenn das Lesezeichen nicht lädt.
func start_lesson(from_step: int = 0) -> bool:
	var lesson := load(LESSON_PATH) as TutorialLesson
	if lesson == null or not lesson.problems().is_empty():
		_top_bar.show_notice("Die Geschichtsstunde fehlt oder ist fehlerhaft")
		return false
	if _start_screen.is_open():
		_start_screen.close()
	var errors := start_new_game(lesson.scenario, lesson.state, true)
	if not errors.is_empty():
		return false
	if bool(_sim.get_game_settings().get("active", false)):
		_sim.set_game_settings(lesson.difficulty, false)
	_event_window.set_compass(true)
	_hint_card.dismiss()
	focus_capital()
	_set_map_mode(MapModes.MAP_POLITICAL)
	_set_speed(0)
	_tutorial.start(lesson)
	for _i in clampi(from_step, 0, lesson.steps.size() - 1):
		_tutorial.mark_done()
		_tutorial.advance()
	print("EINIGKEIT: Geschichtsstunde „%s“ beginnt" % lesson.title)
	return true


func _on_tutorial_step(step: TutorialStep) -> void:
	_tutorial_flags.clear()
	_tutorial_base = {
		"map_mode": _map_mode, "elections": _sim.get_election_count() if _sim != null else 0,
	}
	if not step.time:
		_set_speed(0)
	# Schritte an der Staatsübersicht öffnen ihre Schublade, der Schritt der Kartenmodi die Legende (seit M14a1).
	if step.target in ["steuern", "staatsuebersicht"] and not _state_drawer_open:
		set_state_drawer(true)
	if step.target == "kartenmodi" and _legend.is_collapsed():
		_legend.set_collapsed(false)
	_tutorial_highlight.visible = not step.target.is_empty()
	_place_tutorial()
	_place_tutorial.call_deferred()


func _on_tutorial_closed(completed: bool) -> void:
	_tutorial_highlight.visible = false
	_tutorial_flags.clear()
	_top_bar.show_notice(
		"Die Geschichtsstunde ist zu Ende – die Partie gehört Ihnen." if completed
		else "Geschichtsstunde beendet; die Partie läuft weiter."
	)


func _tutorial_blocks_time() -> bool:
	if _tutorial == null or not _tutorial.is_active():
		return false
	var step := _tutorial.current_step()
	return step != null and not step.time


## Ist die Aufgabe des Schritts erledigt? (Prüfungen aus TutorialStep.CHECKS)
func _tutorial_check(check: String) -> bool:
	match check:
		"weiter":
			return true
		"kartenmodus":
			return _map_mode != int(_tutorial_base.get("map_mode", _map_mode))
		"ort":
			return _selected >= 0
		"haushalt":
			return _state_panel.is_budget_visible() or _tutorial_flags.has("bereich:haushalt")
		"steuern":
			return _tutorial_flags.has("steuern")
		"kabinett":
			return area_window(&"kabinett").is_open()
		"gesandter":
			return _tutorial_flags.has("gesandter")
		"zollunion":
			return _state_panel.is_customs_visible() or _tutorial_flags.has("bereich:zollunion")
		"diplomatie":
			return _diplomacy_window.is_open()
		"tempo":
			return _speed > 0
		"ereignis":
			return _tutorial_flags.has("ereignis")
		"wahl":
			return _election_night.is_open() or _sim.get_election_count() > int(_tutorial_base.get("elections", 0))
		"journal":
			return _journal_window.is_open()
		"speichern":
			return _tutorial_flags.has("speichern")
	return false


## Element, das ein Schritt hervorhebt (TutorialStep.TARGETS), oder null.
func tutorial_target(target_name: String) -> Control:
	if target_name.begins_with("bereich:"):
		return _area_bar.button(StringName(target_name.trim_prefix("bereich:")))
	match target_name:
		"kartenmodi":
			return _mode_bar
		"staatsuebersicht":
			return _state_panel
		"steuern":
			var row := _state_panel.find_child("Tax0", true, false) as Control
			return row.get_parent() as Control if row != null else _state_panel
		"tempo":
			return _top_bar.find_child("SpeedButtons", true, false) as Control
		"menue":
			return _top_bar.find_child("GameMenuButton", true, false) as Control
		"uebersicht":
			return _overview
	return null


## Platz der Geschichtsstunde (seit dem Spieltest 11/2026; vorher fest rechts oben, wo die Ortsinfo
## aufgeht): die erste freie Stelle über der Karte – unten rechts, oben rechts, rechts oben in der Spalte,
## unten links, oben links –, an der das Panel weder Ortsinfo, Übersicht und Legende noch die
## Staatsübersicht, offene Fenster oder das Ziel des Schritts verdeckt. Ist keine Stelle frei, erscheint
## es kompakt (Titel und Aufgabe); sonst an der Stelle mit der kleinsten Überdeckung. Von Hand
## verschoben bleibt es, wo es ist (nur im Bild gehalten).
func _place_tutorial() -> void:
	if _tutorial == null or not _tutorial.visible:
		return
	var view := get_viewport().get_visible_rect()
	if _tutorial.moved_by_user:
		_tutorial.size = _tutorial.get_combined_minimum_size()
		_tutorial.position = _tutorial.position.clamp(view.position, (view.end - _tutorial.size).max(view.position))
		return
	var occupied := tutorial_occupied_rects()
	var best_position := Vector2.ZERO
	var best_compact := true
	var best_overlap := INF
	for compact: bool in [false, true]:
		var size := _tutorial.size_for(compact)
		for candidate in _tutorial_candidates(size):
			var overlap := _overlap_area(Rect2(candidate, size), occupied)
			if overlap < best_overlap:
				best_overlap = overlap
				best_position = candidate
				best_compact = compact
			if overlap <= 0.0:
				break
		if best_overlap <= 0.0:
			break
	if _tutorial.is_compact() != best_compact:
		_tutorial.set_compact(best_compact)
	_tutorial.size = _tutorial.size_for(best_compact)
	_tutorial.position = best_position.round()


## Flächen, die die Geschichtsstunde nicht verdecken darf (auch für den Klicktest).
func tutorial_occupied_rects() -> Array[Rect2]:
	var result: Array[Rect2] = []
	var controls: Array[Control] = [
		_area_bar, _windows.open_window(), _event_window, _newspaper_window, _play_bar, _messages
	]
	for column: Control in [_left_column, _right_column]:
		for child in column.get_children():
			if child is Control and (child as Control).name != &"Spacer":
				controls.append(child as Control)
	var step := _tutorial.current_step() if _tutorial != null else null
	if step != null:
		var target := tutorial_target(step.target)
		if target != null and target != _map_view:
			controls.append(target)
	for control in controls:
		if control != null and control.is_visible_in_tree():
			result.append(control.get_global_rect())
	return result


## Mögliche Plätze über der Karte, in der Reihenfolge des Vorzugs.
func _tutorial_candidates(size: Vector2) -> Array[Vector2]:
	var view := get_viewport().get_visible_rect()
	var top := _left_column.offset_top
	if _play_bar != null and _play_bar.visible:
		top = _play_bar.get_global_rect().end.y + 8.0
	var left := _map_area_left()
	var right := _right_column.get_global_rect().position.x - 8.0
	var bottom := _bottom_of_map_area()
	return [
		Vector2(right - size.x, bottom - size.y),
		Vector2(right - size.x, top),
		Vector2(view.end.x - size.x - 12.0, top),
		Vector2(left, bottom - size.y),
		Vector2(left, top),
	]


static func _overlap_area(rect: Rect2, occupied: Array[Rect2]) -> float:
	var total := 0.0
	for other in occupied:
		var cut := rect.intersection(other.grow(4.0))
		total += cut.get_area() if cut.has_area() else 0.0
	return total


## Hinweis unten rechts über der Legende.
func _place_hint() -> void:
	if _hint_card == null or not _hint_card.visible:
		return
	var area := get_viewport().get_visible_rect().size
	_hint_card.size = _hint_card.get_combined_minimum_size()
	_hint_card.position = Vector2(area.x - _hint_card.size.x - 12.0, area.y - _hint_card.size.y - 120.0)


## Hinweis beim ersten Öffnen, wenn die Einstellung an ist und keine Geschichtsstunde läuft.
func _offer_hint(key: String) -> void:
	if _hint_card == null or _settings == null or not bool(_settings.get_value("spiel/hinweise")):
		return
	if _tutorial.is_active() or _options.wants_screenshot():
		return
	if _hint_card.offer(key):
		_place_hint()
		_place_hint.call_deferred()


## Nach dem Laden eines Spielstands: „Was ist passiert?“ mit den wichtigsten Nachrichten des letzten Jahres.
func _after_loading_save() -> void:
	if _settings != null and not bool(_settings.get_value("spiel/rueckblick")):
		return
	if RecapWindow.collect(_sim, _player).is_empty():
		return
	_windows.opening(_recap_window)
	_recap_window.open(_sim, _player)
	_place_diplomacy()
	_sync_area_bar()


# --- Musik und Klänge (seit M13b2, vertical_slice.md 5.3) --------------------------------

## Journalstufen der Revolution, in denen die Musik „revolution“ spielt (journale/revolution.chron).
const REVOLUTION_STAGES: PackedStringArray = [
	"maerz", "vorparlament", "nationalversammlung", "verfassungswerk", "kaiserkrone", "kampagne",
]


func _build_audio() -> void:
	_audio = AudioDirector.new()
	add_child(_audio)
	_audio.piece_started.connect(
		func(id: String) -> void:
			var piece: Dictionary = _audio.pieces.get(id, {})
			print("EINIGKEIT: Musik %s (%s, %s)" % [piece.get("titel", id), piece.get("komponist", ""), _audio.mood()])
	)
	# Befehle: Gänsekiel; Verträge und Frieden: Siegel; Fenster, Zeitung: Papier; Ereignis: Glocke.
	for sig: Signal in [
		_state_panel.tax_rate_requested, _state_panel.customs_change_requested, _state_panel.mobilization_requested,
		_diplomacy_window.envoy_requested, _diplomacy_window.demand_requested, _diplomacy_window.motion_requested,
		_diplomacy_window.war_requested, _play_bar.manoeuvre_requested, _war_window.goal_requested,
		_location_panel.campaign_requested, _event_window.option_chosen, _army_panel.staff_requested,
	]:
		_sound_on(sig, "gaensekiel")
	for sig: Signal in [_diplomacy_window.treaty_requested, _war_window.peace_requested]:
		_sound_on(sig, "siegel")
	_windows.window_opened.connect(_play_sound.bind("papier").unbind(1))
	for window: Control in [_newspaper_window, _event_window]:
		var sound := "papier" if window == _newspaper_window else "glocke"
		window.visibility_changed.connect(
			func() -> void:
				if window.visible:
					_play_sound(sound)
		)


## Klang zu einem Signal; dessen Argumente fallen weg.
func _sound_on(sig: Signal, sound: String) -> void:
	var argc := 0
	for info: Dictionary in sig.get_object().get_signal_list():
		if StringName(info.get("name", "")) == sig.get_name():
			argc = (info.get("args", []) as Array).size()
	sig.connect(_play_sound.bind(sound).unbind(argc))


## Klangregisseur (Zugriff für Tests).
func audio_director() -> AudioDirector:
	return _audio


## Stimmung aus der Lage des Spielerstaates: Krieg vor Revolution (Aufstand im eigenen Staat) vor Krise
## (Journal der Revolution in einer revolutionären Stufe, Stufe 2 oder 3 eines Spiels mit dem Spielerstaat)
## vor Frieden. Bis zum Spieltest 11/2026 zählte das Journal als Revolution.
func _update_mood() -> void:
	if _audio == null or _sim == null:
		return
	_audio.set_mood(mood_for_player())


func mood_for_player() -> String:
	if _player < 0:
		return "frieden"
	for war in _sim.get_wars():
		if int(war.get("player_side", 0)) != 0:
			return "krieg"
	if not (_sim.get_state_events(_player).get("revolt", {}) as Dictionary).is_empty():
		return "revolution"
	# Seit dem Spieltest 11/2026: Läuft die Revolution nur im Journal (ohne Aufstand im eigenen Staat), ist
	# die Stimmung „krise“ – vorher spielte das ganze erste Jahr nur die Marseillaise.
	for journal in _sim.get_journals():
		if (
			str(journal.get("id", "")) == "revolution" and bool(journal.get("started", false))
			and not bool(journal.get("finished", false)) and REVOLUTION_STAGES.has(str(journal.get("stage_id", "")))
		):
			return "krise"
	for play in _sim.get_diplomatic_plays():
		var involved := int(play.get("demander_index", -1)) == _player or int(play.get("target_index", -1)) == _player
		if involved and int(play.get("stage", 1)) >= 2:
			return "krise"
	return "frieden"


## Neue Chronikeinträge: Trommel bei einer Kriegserklärung mit dem Spielerstaat, Siegel bei seinem Frieden
## oder Bündnis, sonst ab 1849 höchstens einmal die Woche der Telegraf für eine Meldung, die ihn betrifft.
func _sounds_for_entries(entries: Array[Dictionary]) -> void:
	if _audio == null or _player < 0 or entries.is_empty():
		return
	var own := str(_sim.get_state(_player).get("short_name", ""))
	if own.is_empty():
		return
	var news := false
	for entry in entries:
		var text := str(entry.get("text", ""))
		if not text.contains(own):
			news = news or int(entry.get("state_index", -1)) < 0
			continue
		if text.contains(" den Krieg") and text.contains(" erklärt "):
			_play_sound("trommel")
			return
		if text.begins_with("Frieden zwischen") or text.contains(" schließen ein Bündnis"):
			_play_sound("siegel")
			return
		news = true
	var today := ElectionTally.day_number(_sim.get_date())
	if news and int(_sim.get_date().get("year", 0)) >= 1849 and today - _telegraph_day >= 7:
		_telegraph_day = today
		_play_sound("telegraf")


# --- Leisten und Tooltips (seit M13a5c, vertical_slice.md 4.1 und 4.3) -------------------


func _build_bars() -> void:
	var root: Control = $UI/Root
	_overview = OverviewPanel.new()
	_right_column.add_child(_overview)
	_right_column.move_child(_overview, _location_panel.get_index() + 1)
	_overview.entry_requested.connect(_on_overview_entry)
	# Die Chronik ist über die Bereichsleiste erreichbar; das Neueste zeigen die Meldekarten (seit M14a3).
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
	_tooltips.attach(_top_bar.field(&"unruhe"), _bar_tooltips.unrest)
	_tooltips.attach(_top_bar.field(&"bevoelkerung"), _bar_tooltips.population)
	_tooltips.attach(_top_bar.field(&"heer"), _bar_tooltips.army)
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


## Rechte Übersicht und Tooltips (Tests).
func overview_panel() -> OverviewPanel:
	return _overview


func tooltip_layer() -> TooltipLayer:
	return _tooltips


func bar_tooltips() -> BarTooltips:
	return _bar_tooltips


## Untere Kante für Fenster: über der Leiste der Kartenmodi (bis M14a3 über dem Ticker).
func _bottom_of_map_area() -> float:
	return _mode_bar_area.position.y - 8.0


## Obere Leiste und rechte Übersicht höchstens zweimal je Sekunde.
func _refresh_bars(force: bool = false) -> void:
	if _sim == null or _overview == null:
		return
	if not force and Time.get_ticks_msec() - _overview_filled_at < OVERVIEW_REFRESH_MS:
		return
	_overview_filled_at = Time.get_ticks_msec()
	var date := _sim.get_date()
	_top_bar.begin_month(int(date.get("year", 0)) * 12 + int(date.get("month", 1)))
	_top_bar.show_overview(_sim.get_state_overview(_player), _sim.get_legacy(_player))
	_update_area_badges()
	if _player_current >= 0:
		_overview.show_entries([] as Array[Dictionary])
		return
	_overview.show_entries(OverviewPanel.collect(_sim, _player, str(_sim.get_state(_player).get("tag", ""))))
	if _location_panel.visible:
		_overview.visible = false


## Zähler der Bereichsleiste (seit M14a2): Kriege des Spielerstaates, Krisen, an denen er beteiligt ist.
func _update_area_badges() -> void:
	if _area_bar == null or _sim == null:
		return
	var wars := 0
	for war in _sim.get_wars():
		if int(war.get("player_side", 0)) != 0:
			wars += 1
	var crises := 0
	for play in _sim.get_diplomatic_plays():
		if int(play.get("demander_index", -1)) == _player or int(play.get("target_index", -1)) == _player:
			crises += 1
	_area_bar.set_badge(&"militaer", wars)
	_area_bar.set_badge(&"diplomatie", crises)


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
