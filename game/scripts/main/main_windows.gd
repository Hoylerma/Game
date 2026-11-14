extends "res://scripts/main/main_core.gd"
## Fenster der Partie (seit M13a5d aus main.gd ausgelagert): Diplomatie, Militär, Startbildschirm,
## Parteienmodus, Ereignisse und Journale, Urteil der Geschichte, Wahlabend.


# --- Diplomatie (seit M9, Schritt b3, docs/technik/diplomatie.md Abschnitt 6) ------


func _build_diplomacy() -> void:
	var root: Control = $UI/Root
	_diplomacy_window = DiplomacyWindow.new()
	root.add_child(_diplomacy_window)
	root.move_child(_diplomacy_window, _top_bar.get_index())
	_play_bar = PlayBar.new()
	_play_bar.reasons_source = _ai_reasons
	root.add_child(_play_bar)
	root.move_child(_play_bar, _top_bar.get_index())
	_diplomacy_window.close_requested.connect(close_diplomacy)
	_diplomacy_window.envoy_requested.connect(_on_envoy_requested)
	_diplomacy_window.treaty_requested.connect(_on_treaty_requested)
	_diplomacy_window.demand_requested.connect(_on_demand_requested)
	_diplomacy_window.motion_requested.connect(_on_motion_requested)
	_diplomacy_window.resized.connect(_place_diplomacy)
	_play_bar.manoeuvre_requested.connect(_on_manoeuvre_requested)
	_play_bar.resized.connect(_place_diplomacy)
	_play_bar.visibility_changed.connect(_place_diplomacy)
	_state_panel.diplomacy_requested.connect(toggle_diplomacy)


## Fenster „Diplomatie“ (Zugriff für Tests).
func diplomacy_window() -> DiplomacyWindow:
	return _diplomacy_window


## Krisenleiste (Zugriff für Tests).
func play_bar() -> PlayBar:
	return _play_bar


## Öffnet das Fenster „Diplomatie“ mit dem Reiter tab (0 Staaten, 1 Bundesversammlung,
## 2 Deutsche Frage); nur mit dem Deutschen Bund und nicht im Parteienmodus.
func open_diplomacy(tab: int = 0) -> void:
	if _sim == null or _player_current >= 0 or not bool(_sim.get_diplomacy(_player).get("active", false)):
		return
	_state_panel.hide_popups()
	_windows.opening(_diplomacy_window)
	_diplomacy_window.open(_sim, _player, tab)
	_place_diplomacy()
	_sync_area_bar()


func close_diplomacy() -> void:
	if _diplomacy_window != null:
		_diplomacy_window.close()
		_sync_area_bar()


func toggle_diplomacy() -> void:
	if _diplomacy_window.is_open():
		close_diplomacy()
	else:
		open_diplomacy()


## Krisenleiste mittig unter der oberen Leiste, das Fenster rechts neben der linken Spalte darunter
## und über der Leiste der Kartenmodi (die Staatenliste wird bei Bedarf kürzer).
func _place_diplomacy() -> void:
	if _diplomacy_window == null:
		return
	var top := _left_column.offset_top
	var area := ($UI/Root as Control).size
	var bar_size := _play_bar.get_combined_minimum_size()
	_play_bar.size = bar_size
	_play_bar.position = Vector2(roundf((area.x - bar_size.x) * 0.5), top)
	var left := _left_column.offset_right + 12.0
	# Bereichsleiste (seit M13a5a) senkrecht neben der linken Spalte; die Fenster öffnen rechts daneben.
	if _area_bar != null and _area_bar.visible:
		var area_size := _area_bar.get_combined_minimum_size()
		_area_bar.size = area_size
		_area_bar.position = Vector2(_left_column.offset_right + 8.0, top)
		left = _area_bar.position.x + area_size.x + 8.0
	if _play_bar.visible:
		_play_bar.position.x = maxf(_play_bar.position.x, left)
		top += bar_size.y + 8.0
	# Alle Fenster der Bereiche an derselben Stelle (seit M13a5a: Fensterverwaltung, nie zwei zugleich
	# offen); das Ereignisfenster mittig.
	_windows.place(left, top, _bottom_of_map_area())
	_place_event_window()
	_place_reports()


## Krisenleiste und offenes Fenster neu füllen (nach jedem Tag; das Fenster höchstens einmal je Sekunde).
func _refresh_diplomacy() -> void:
	var plays: Array[Dictionary] = _sim.get_diplomatic_plays()
	_play_bar.show_plays(plays, _player if _player_current < 0 else -1)
	_diplomacy_window.refresh()


func _on_envoy_requested(target: int, send: bool) -> void:
	var name_text := _state_short_name(target)
	if _sim.send_envoy(_player, target, send):
		_diplomacy_window.show_note(
			("Gesandter nach %s entsandt" if send else "Gesandter aus %s abberufen") % name_text
			+ " (ab dem nächsten Tag)."
		)
	else:
		_diplomacy_window.show_note("Nicht möglich: kein Gesandter frei oder keiner in %s." % name_text)


func _on_treaty_requested(target: int, action: int) -> void:
	var name_text := _state_short_name(target)
	var texts := {
		SimulationClient.TREATY_ALLIANCE: "Bündnis mit %s geschlossen",
		SimulationClient.TREATY_GUARANTEE: "Garantie für %s gegeben",
		SimulationClient.TREATY_CANCEL_ALLIANCE: "Bündnis mit %s gekündigt",
		SimulationClient.TREATY_CANCEL_GUARANTEE: "Garantie für %s gekündigt",
	}
	if _sim.propose_treaty(_player, target, action):
		_diplomacy_window.show_note(str(texts.get(action, "%s")) % name_text + " (ab dem nächsten Tag).")
	else:
		_diplomacy_window.show_note("%s lehnt ab oder der Vertrag ist nicht möglich." % name_text)


func _on_demand_requested(target: int, kind: int) -> void:
	var name_text := _state_short_name(target)
	if _sim.make_demand(_player, target, kind):
		_diplomacy_window.show_note("Forderung an %s gestellt: Die Krise beginnt am nächsten Tag." % name_text)
	else:
		_diplomacy_window.show_note("Die Forderung an %s ist zur Zeit nicht möglich." % name_text)


func _on_motion_requested(kind: int, target: int) -> void:
	var preview := _sim.get_motion_preview(_player, kind, target)
	if _sim.file_motion(_player, kind, target):
		_diplomacy_window.show_note(
			"Antrag gestellt: %s; abgestimmt wird am nächsten Monatsersten." % str(preview.get("text", ""))
		)
	else:
		_diplomacy_window.show_note("Der Antrag ist zur Zeit nicht möglich (schon ein Antrag offen?).")


func _on_manoeuvre_requested(kind: int) -> void:
	var texts := {
		SimulationClient.MANOEUVRE_YIELD: "Wir geben nach",
		SimulationClient.MANOEUVRE_MEDIATE: "Wir bieten Vermittlung an",
		SimulationClient.MANOEUVRE_ESCALATE: "Wir eskalieren am Ende der Stufe",
	}
	if _sim.manoeuvre(_player, kind):
		_play_bar.show_note("%s (ab dem nächsten Tag)." % str(texts.get(kind, "")))
	else:
		_play_bar.show_note("Dieses Manöver ist zur Zeit nicht möglich.")


# --- Militär (seit M10, Schritt b3; docs/technik/militaer.md Abschnitt 8) -----------


func _build_war() -> void:
	var root := $UI/Root as Control
	_army_panel = ArmyPanel.new()
	_right_column.add_child(_army_panel)
	_right_column.move_child(_army_panel, _location_panel.get_index())
	_army_panel.close_requested.connect(select_army.bind(-1))
	_army_panel.staff_requested.connect(_on_staff_requested)
	_army_panel.focus_requested.connect(focus_army)
	_war_window = WarWindow.new()
	_war_window.reasons_source = _ai_reasons
	root.add_child(_war_window)
	root.move_child(_war_window, _top_bar.get_index())
	_war_window.state_name = _state_short_name
	_war_window.location_name = _location_name
	_war_window.close_requested.connect(close_war_window)
	_war_window.peace_requested.connect(_on_peace_requested)
	_war_window.goal_requested.connect(_on_goal_requested)
	_war_window.army_selected.connect(func(army_id: int) -> void:
		select_army(army_id)
		focus_army(army_id)
	)
	_war_window.resized.connect(_place_diplomacy)
	_battle_reports = BattleReports.new()
	root.add_child(_battle_reports)
	root.move_child(_battle_reports, _top_bar.get_index())
	_battle_reports.minimum_size_changed.connect(_place_reports.call_deferred)
	_war_window.visibility_changed.connect(_place_reports.call_deferred)
	_diplomacy_window.visibility_changed.connect(_place_reports.call_deferred)
	_battle_reports.location_requested.connect(func(index: int) -> void: _select_location(index, true))
	_state_panel.mobilization_requested.connect(_on_mobilization_requested)
	_state_panel.war_window_requested.connect(toggle_war_window)
	_diplomacy_window.war_requested.connect(_on_war_requested)


func war_window() -> WarWindow:
	return _war_window


func battle_reports() -> BattleReports:
	return _battle_reports


## Kriegsfenster öffnen (Reiter 0 Kriege, 1 Heere); nur in Szenarien mit Heeren, nicht im
## Parteienmodus. Schließt das Fenster „Diplomatie“ (beide stehen an derselben Stelle).
func open_war_window(tab: int = 0) -> void:
	if _sim == null or _player_current >= 0 or not bool(_sim.get_state(_player).get("military", false)):
		return
	_state_panel.hide_popups()
	_windows.opening(_war_window)
	_war_window.open(_sim, _player, tab)
	_place_diplomacy()
	_sync_area_bar()


func close_war_window() -> void:
	if _war_window != null:
		_war_window.close()
		_sync_area_bar()


func toggle_war_window() -> void:
	if _war_window.is_open():
		close_war_window()
	else:
		open_war_window()


## Karte auf den Standort eines Heeres.
func focus_army(army_id: int) -> void:
	var army := _sim.get_army(army_id) if _sim != null else {}
	var center := _map_view.location_center(int(army.get("location_index", -1)))
	if center.x >= 0.0:
		_map_view.camera.focus_on(center)


## Schlachtberichte unten rechts über der Karte, links der rechten Spalte und über der Leiste der
## Kartenmodi; neben einem offenen Fenster (Krieg, Diplomatie) so schmal wie nötig.
func _place_reports() -> void:
	if _battle_reports == null:
		return
	var right := _right_column.get_global_rect().position.x - 12.0
	var left := _left_column.offset_right + 12.0
	for window: Control in _windows.windows():
		if window != null and window.visible:
			left = maxf(left, window.position.x + window.size.x + 8.0)
	var width := clampf(right - left, BattleReports.MIN_WIDTH, BattleReports.MAX_WIDTH)
	_battle_reports.custom_minimum_size.x = width
	var size := _battle_reports.get_combined_minimum_size()
	_battle_reports.size = size
	_battle_reports.position = Vector2(right - size.x, _mode_bar_area.position.y - size.y - 8.0)


## Stärkstes eigenes Feldheer auswählen (Screenshot mit --heer).
func _select_strongest_army() -> void:
	var best := {}
	for army in _sim.get_armies(_player):
		if not bool(army.get("garrison", false)) and int(army.get("soldiers", 0)) > int(best.get("soldiers", -1)):
			best = army
	if not best.is_empty():
		select_army(int(best.get("id", -1)))


func _on_staff_requested(army_id: int, on: bool) -> void:
	if not _sim.set_army_staff(army_id, on):
		_army_panel.clear_pending()
		_top_bar.show_notice("Das Heer steht schon so")
	_update_armies()


func _on_mobilization_requested(mobilize: bool) -> void:
	if _sim.order_mobilization(_player, mobilize):
		_top_bar.show_notice(("Mobilmachung befohlen" if mobilize else "Abrüstung befohlen") + " (ab dem nächsten Tag)")
	else:
		_state_panel.reject_mobilization(
			"Mobilmachung nicht möglich." if mobilize else "Abrüstung nicht möglich (im Krieg oder schon im Frieden)."
		)
	_refresh_all()


func _on_war_requested(target: int, goal: int) -> void:
	var name_text := _state_short_name(target)
	if _sim.declare_war(_player, target, goal):
		_diplomacy_window.show_note("Kriegserklärung an %s: Der Krieg beginnt am nächsten Tag." % name_text)
	else:
		_diplomacy_window.show_note("Eine Kriegserklärung an %s ist zur Zeit nicht möglich." % name_text)


func _on_peace_requested(war_id: int, terms: int) -> void:
	var preview := _sim.get_peace_preview(_player, war_id, terms)
	if _sim.offer_peace(_player, war_id, terms):
		_war_window.show_note("Friedensangebot gesandt: %s Antwort am nächsten Tag." % str(preview.get("text", "")))
	else:
		_war_window.show_note("Dieses Friedensangebot ist zur Zeit nicht möglich.")


func _on_goal_requested(war_id: int, code: int) -> void:
	if _sim.add_war_goal(_player, war_id, code):
		_war_window.show_note("Neues Kriegsziel gestellt (ab dem nächsten Tag).")
	else:
		_war_window.show_note("Dieses Kriegsziel ist nicht möglich.")


func _state_short_name(state: int) -> String:
	return str(_sim.get_state(state).get("short_name", "")) if _sim != null else ""


# --- Startbildschirm (seit M13, vertical_slice.md 3.1) ------------------------------


func _build_start_screen() -> void:
	_start_screen = StartScreen.new()
	_start_screen.name = "StartScreen"
	var root := _state_panel.get_parent().get_parent()  # UI/Root
	root.add_child(_start_screen)
	root.move_child(_start_screen, _notices.get_index())  # Hinweise bleiben darüber lesbar
	_start_screen.scenario_chosen.connect(_on_start_scenario)
	_start_screen.state_chosen.connect(_on_start_state)
	_start_screen.game_requested.connect(_on_start_game)
	_start_screen.load_requested.connect(_on_start_load)
	_start_screen.quit_requested.connect(func() -> void: get_tree().quit())
	# Hinter dem Startbildschirm nur die Karte: Leisten und Panels der Partie ruhen.
	_start_screen.shown_changed.connect(func(open: bool) -> void:
		for control: Control in [_top_bar, _left_column, _right_column, _mode_bar_area, _map_labels, _area_bar, _ticker]:
			if control != null:
				control.visible = not open)
	_start_screen.settings_requested.connect(open_settings)


func _wants_start_screen() -> bool:
	return (
		start_screen_enabled and _sim != null and not _options.wants_screenshot()
		and _options.scenario_id.is_empty() and _options.state_tag.is_empty() and _options.party_id.is_empty()
	)


## Startbildschirm (Tests).
func start_screen() -> StartScreen:
	return _start_screen


## Öffnet das Hauptmenü des Startbildschirms; die Zeit hält an.
func open_start_screen() -> void:
	if _sim == null:
		return
	close_game_menu(false)
	_set_speed(0)
	_tooltip.hide_tooltip()
	var saves := SaveSlots.list(_sim, save_dir)
	var newest := ""
	var newest_time := -1
	for save in saves:
		# Auch Spielstände anderer Lesezeichen: das Laden bringt ihr Szenario mit.
		if int(save.get("modified", 0)) > newest_time:
			newest_time = int(save.get("modified", 0))
			newest = str(save.get("path", ""))
	_start_screen.show_scenarios(_sim.get_scenarios(_data_dir))
	_start_screen.show_saves(saves)
	_start_screen.open_menu(newest, not saves.is_empty())


## Für Screenshots (--startbildschirm): 0 Hauptmenü, 1 Lesezeichen, 2 Einstellungen des geladenen
## Lesezeichens mit seinem Spielerstaat.
func _open_start_page(page: int) -> void:
	open_start_screen()
	if page == 1:
		_start_screen.menu_button("NewGame").emit_signal("pressed")
	elif page == 2:
		_start_screen.show_settings(str(_sim.get_state(_player).get("name", "")), _sim.get_game_settings())


func _on_start_scenario(scenario_id: String) -> void:
	var errors := start_new_game(scenario_id, "", true)
	if not errors.is_empty():
		_start_screen.show_status("Das Lesezeichen ließ sich nicht laden: " + errors, true)
		return
	var scenario := _sim.get_scenario()
	var playable: PackedStringArray = scenario.get("playable", PackedStringArray())
	var recommended: Array = StartScreen.RECOMMENDED.get(scenario_id, [])
	var states: Array[Dictionary] = []
	var listed := PackedStringArray()
	for entry: Dictionary in recommended:
		var index := _sim.get_state_index(str(entry.get("tag", "")))
		if index >= 0:
			states.append(_start_state_entry(index, true, str(entry.get("difficulty", "")), str(entry.get("text", ""))))
			listed.append(str(entry.get("tag", "")))
	if recommended.is_empty():
		for tag in playable:
			var index := _sim.get_state_index(tag)
			if index >= 0:
				states.append(_start_state_entry(index, true, "", "Hervorgehobener Staat des Lesezeichens"))
				listed.append(tag)
	var others: Array[Dictionary] = []
	for index in _sim.get_state_count():
		var state := _sim.get_state(index)
		if int(state.get("location_count", 0)) > 0 and not listed.has(str(state.get("tag", ""))):
			others.append(_start_state_entry(index, false, "", ""))
	others.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return str(a.get("name", "")).naturalnocasecmp_to(str(b.get("name", ""))) < 0)
	states.append_array(others)
	_start_screen.show_states(str(scenario.get("name", "")), states, str(scenario.get("player_tag", "")))


func _start_state_entry(index: int, recommended: bool, difficulty: String, text: String) -> Dictionary:
	var state := _sim.get_state(index)
	return {
		"tag": str(state.get("tag", "")), "name": str(state.get("name", "")), "recommended": recommended,
		"difficulty": difficulty, "text": text,
	}


func _on_start_state(state_tag: String) -> void:
	var index := _sim.get_state_index(state_tag)
	if index < 0:
		return
	_start_screen.show_settings(str(_sim.get_state(index).get("name", state_tag)), _sim.get_game_settings())


## Beginnt die Partie: Spielerstaat, Spieleinstellungen (Befehl am ersten Tag), Kompass.
func _on_start_game(state_tag: String, difficulty_id: String, open: bool, compass: bool) -> void:
	var index := _sim.get_state_index(state_tag)
	if index < 0:
		return
	if index != _player:
		play_state(index)
	if bool(_sim.get_game_settings().get("active", false)):
		_sim.set_game_settings(difficulty_id, open)
	_event_window.set_compass(compass)
	_start_screen.close()
	focus_capital()
	# Der Befehl wirkt beim ersten Schritt; der Name der Stufe kommt aus der Auswahl.
	var settings := _sim.get_game_settings()
	var level := str(settings.get("difficulty_name", ""))
	for entry: Dictionary in settings.get("difficulties", []):
		if str(entry.get("id", "")) == difficulty_id:
			level = str(entry.get("name", ""))
	_top_bar.show_notice(
		"Neues Spiel: %s, %s%s" % [
			str(_sim.get_state(_player).get("name", "")), _sim.get_date_text(),
			(", Stufe %s" % level) if bool(settings.get("active", false)) else ""
		]
	)
	print(
		"EINIGKEIT: Partie beginnt als %s, Stufe %s, Verlauf %s" % [
			state_tag, difficulty_id if not difficulty_id.is_empty() else "Standard",
			"offen" if open else "historisch"
		]
	)


func _on_start_load(path: String) -> void:
	if load_game_file(path):
		_start_screen.close()
	else:
		_start_screen.show_status("Der Spielstand ließ sich nicht laden.", true)


# --- Parteienmodus (seit M8) ------------------------------------------------------


func _build_party_panel() -> void:
	_party_panel = PartyPanel.new()
	_left_column.add_child(_party_panel)
	_left_column.move_child(_party_panel, _state_panel.get_index())
	_party_panel.leave_requested.connect(set_party.bind(-1))


## Parteipanel (Tests).
func party_panel() -> PartyPanel:
	return _party_panel


## --partei=<strömung> (seit M8): Parteienmodus beim Start.
func _apply_party_option() -> void:
	if _sim == null or _options.party_id.is_empty():
		return
	if _sim.get_parliaments().is_empty():
		printerr("EINIGKEIT: --partei=%s: Dieses Szenario hat kein Parlament." % _options.party_id)
		return
	for i in _currents.size():
		if str(_currents[i].get("id", "")) == _options.party_id:
			set_party(i)
			return
	printerr("EINIGKEIT: --partei=%s: unbekannte Strömung in diesem Szenario." % _options.party_id)


## Gespielte Strömung (-1 = Staat).
func player_current() -> int:
	return _player_current


## Rolle wechseln: Strömung current im Parteienmodus, -1 = Staat (Spielmenü, Parteipanel, --partei).
func set_party(current: int) -> void:
	if _sim == null or not _sim.set_player_current(current):
		return
	_player_current = _sim.get_player_current()
	_apply_role()
	if _player_current >= 0:
		_top_bar.show_notice("Ihr spielt jetzt die Partei %s" % str(_currents[_player_current].get("name", "")))
	else:
		_top_bar.show_notice("Ihr spielt wieder %s" % str(_sim.get_state(_player).get("name", "")))


## Staats- oder Parteipanel in der linken Spalte, Wahlkampf in der Ortsinfo.
func _apply_role() -> void:
	var party := _player_current >= 0
	_party_panel.visible = party
	_state_panel.visible = not party
	if _area_bar != null:
		_area_bar.visible = not party
	if party:
		_state_panel.hide_popups()
		_windows.close_all()
		_sync_area_bar()
	_location_panel.set_party_mode(party)
	if party:
		_party_panel.show_party(_sim.get_party(_player_current))
	_refresh_location_panel()
	_sync_event_player()


func _on_campaign_requested(district: int, action: int) -> void:
	if _sim == null or _player_current < 0:
		return
	var campaign := _sim.get_campaign(district, _player_current)
	var actions: Array = campaign.get("actions", [])
	var name_text := str((actions[action] as Dictionary).get("name", "")) if action < actions.size() else ""
	if _sim.campaign(_player_current, district, action):
		_top_bar.show_notice(
			"%s im Wahlkreis %s angesetzt (ab dem nächsten Tag)" % [
				name_text, str(campaign.get("district_name", ""))
			]
		)
	else:
		_top_bar.show_notice("%s nicht möglich: Mittel reichen nicht oder die Wahl ist vorbei" % name_text)
	_refresh_location_panel()


# --- Ereignisse und Journale (seit M11, Schritt b4, ereignisse.md Abschnitt 9) ---------


func _build_events() -> void:
	var root: Control = $UI/Root
	_journal_window = JournalWindow.new()
	root.add_child(_journal_window)
	root.move_child(_journal_window, _top_bar.get_index())
	_journal_window.close_requested.connect(close_journal)
	_journal_window.resized.connect(_place_diplomacy)
	_state_panel.journal_requested.connect(toggle_journal)
	# Das Ereignisfenster liegt über allem außer dem Spielmenü (wie der Wahlabend).
	_event_window = EventWindow.new()
	$UI.add_child(_event_window)
	$UI.move_child(_event_window, _game_menu.get_index())
	_event_window.option_chosen.connect(_on_event_option_chosen)
	# Umbrochene Texte kennen ihre Höhe erst nach dem Layout: bei jeder Änderung neu einpassen.
	_event_window.minimum_size_changed.connect(_place_event_window)


## Ereignisfenster (Zugriff für Tests).
func event_window() -> EventWindow:
	return _event_window


## Journalfenster (Zugriff für Tests).
func journal_window() -> JournalWindow:
	return _journal_window


## Öffnet das Journalfenster (Reiter tab, -1: der zuletzt gezeigte); nur in Partien mit Ereignissen.
func open_journal(tab: int = -1) -> void:
	if not _events_available():
		return
	_state_panel.hide_popups()
	_windows.opening(_journal_window)
	_journal_window.open(_sim, tab)
	_place_diplomacy()
	_sync_area_bar()


## Spielmenü „Ereignisse“: schließt das Menü (das Tempo von vorher gilt) und zeigt die bisherigen
## Ereignisse im Journalfenster.
func open_event_log() -> void:
	close_game_menu()
	open_journal()
	if _journal_window.is_open():
		_journal_window.show_event_log()


## Ob die Partie Ereignisse kennt (Journale oder schon eingetretene Ereignisse).
func _events_available() -> bool:
	return _sim != null and (not _sim.get_journals().is_empty() or not _sim.get_event_log(1).is_empty())


func close_journal() -> void:
	if _journal_window != null:
		_journal_window.close()
		_sync_area_bar()


func toggle_journal() -> void:
	if _journal_window.is_open():
		close_journal()
	else:
		open_journal()


## Welcher Staat auf seine Ereignisse wartet (Befehl Spielerstaat, ereignisse.md 3.6, ki.md 4.1): der
## Spielerstaat, im Parteienmodus keiner. Screenshots rechnen wie die Kommandozeile ohne Spieler,
## außer mit --ereignis.
func _sync_event_player() -> void:
	if _sim == null:
		return
	var wanted := _player if _player_current < 0 else -1
	if (_options.wants_screenshot() and not _options.show_events) or _options.smoke_test:
		wanted = -1
	if _sim.get_event_player() != wanted:
		_sim.set_event_player(wanted)


## Offene, noch nicht gewählte Ereignisse des Spielers: Das Fenster zeigt das erste und hält die
## Zeit an; sind alle gewählt, schließt es und die Zeit läuft mit dem Tempo von vorher weiter.
func _check_open_events() -> void:
	if _sim == null:
		return
	var open: Array[Dictionary] = []
	for event in _sim.get_open_events():
		if int(event.get("chosen_option", -1)) < 0 and int(event.get("state_index", -1)) == _player:
			open.append(event)
	if open.is_empty():
		_close_event_window(true)
		return
	if not _event_window.is_open():
		_speed_before_event = _speed
		_set_speed(0)
		_tooltip.hide_tooltip()
	_event_window.show_events(open)
	_place_event_window()
	_place_event_window.call_deferred()


func _on_event_option_chosen(event_index: int, option_index: int) -> void:
	var event := _event_window.current_event()
	if not _sim.choose_event_option(event_index, option_index):
		_top_bar.show_notice("Diese Wahl ist nicht möglich")
		return
	for option: Variant in event.get("options", []):
		if int((option as Dictionary).get("index", -1)) == option_index:
			_top_bar.show_notice(
				"%s: %s (ab dem nächsten Tag)" % [str(event.get("title", "")), str((option as Dictionary).get("text", ""))]
			)
	_check_open_events()


## Schließt das Ereignisfenster; resume setzt die Geschwindigkeit von vorher fort.
func _close_event_window(resume: bool) -> void:
	if _event_window == null or not _event_window.is_open():
		return
	_event_window.close()
	if _turning_backdrop != null:
		_turning_backdrop.visible = false
	if resume:
		_set_speed(_speed_before_event)


## Ereignisfenster mittig über der Karte.
func _place_event_window() -> void:
	if _event_window == null:
		return
	if _turning_backdrop != null:
		_turning_backdrop.visible = _event_window.is_turning_point()
	var area := get_viewport().get_visible_rect().size
	var size := _event_window.get_combined_minimum_size()
	_event_window.size = size
	_event_window.position = Vector2(
		roundf((area.x - size.x) * 0.5), maxf(_left_column.offset_top, roundf((area.y - size.y) * 0.5))
	)


# --- Urteil der Geschichte (seit M13a4, vertical_slice.md 3.5) ------------------------


func _build_verdict() -> void:
	_verdict_window = VerdictWindow.new()
	$UI.add_child(_verdict_window)
	# Unter dem Spielmenü wie der Wahlabend.
	$UI.move_child(_verdict_window, _game_menu.get_index())
	_verdict_window.continue_requested.connect(close_verdict)
	_verdict_window.menu_requested.connect(
		func() -> void:
			close_verdict()
			open_start_screen()
	)
	_verdict_window.export_requested.connect(export_chronicle)
	_verdict_window.minimum_size_changed.connect(_place_verdict)


## Urteil der Geschichte (Zugriff für Tests).
func verdict_window() -> VerdictWindow:
	return _verdict_window


## Öffnet das Urteil der Geschichte; die Zeit hält an.
func open_verdict() -> void:
	if _sim == null:
		return
	_set_speed(0)
	_tooltip.hide_tooltip()
	_state_panel.hide_popups()
	_windows.close_all()
	_sync_area_bar()
	_verdict_window.open(_sim, _player)
	_place_verdict()


## Weiterspielen: schließt das Urteil, die Zeit bleibt angehalten.
func close_verdict() -> void:
	_verdict_window.close()


## Schreibt die Chronik als HTML-Datei und nennt den Pfad im Urteil.
func export_chronicle() -> String:
	var path := ChronicleExport.write(_sim)
	if path.is_empty():
		_verdict_window.show_export_note("Die Chronik konnte nicht geschrieben werden.")
	else:
		_verdict_window.show_export_note("Chronik gespeichert: %s" % ProjectSettings.globalize_path(path))
	return path


func _place_verdict() -> void:
	if _verdict_window == null or not _verdict_window.is_open():
		return
	var area := get_viewport().get_visible_rect().size
	_verdict_window.size = _verdict_window.get_combined_minimum_size()
	_verdict_window.position = Vector2(
		roundf((area.x - _verdict_window.size.x) * 0.5), maxf(8.0, roundf((area.y - _verdict_window.size.y) * 0.5))
	)


# --- Zeitung (seit M13b1, vertical_slice.md 5.2) ------------------------------------


func _build_newspaper() -> void:
	_newspaper_window = NewspaperWindow.new()
	$UI.add_child(_newspaper_window)
	# Unter dem Spielmenü wie das Urteil.
	$UI.move_child(_newspaper_window, _game_menu.get_index())
	_newspaper_window.close_requested.connect(close_newspaper)
	_newspaper_window.minimum_size_changed.connect(_place_newspaper)


## Zeitungsfenster (Zugriff für Tests).
func newspaper_window() -> NewspaperWindow:
	return _newspaper_window


## Öffnet die Titelseite zum Chronikeintrag chronicle_index; die Zeit hält an, bis ein Klick sie schließt.
## false ohne Titelseite.
func open_newspaper(chronicle_index: int) -> bool:
	if _sim == null:
		return false
	var page := _sim.get_front_page(chronicle_index, _player)
	if page.is_empty():
		return false
	if not _newspaper_window.is_open():
		_speed_before_newspaper = _speed
	_set_speed(0)
	_tooltip.hide_tooltip()
	_newspaper_window.show_page(page)
	_place_newspaper()
	_place_newspaper.call_deferred()
	return true


## Legt die Zeitung beiseite; die Zeit läuft mit dem Tempo von vorher weiter.
func close_newspaper() -> void:
	if not _newspaper_window.is_open():
		return
	_newspaper_window.close()
	_set_speed(_speed_before_newspaper)


## Neue Chronikeinträge from … to−1: die wichtigste Titelseite (Gewicht 3, sonst Gewicht 2 mit dem
## Spielerstaat beteiligt, höchstens eine je 30 Tage) öffnet die Zeitung, solange die Pausenregel an ist
## und kein Ereignis, Wahlabend oder Urteil offen ist.
func _check_newspapers(from: int, to: int) -> void:
	if _settings == null or not _settings.pause_on("pause_zeitung") or _player < 0:
		return
	if _event_window.is_open() or _election_night.is_open() or _verdict_window.is_open():
		return
	var today := ElectionTally.day_number(_sim.get_date())
	var best := -1
	var best_weight := 0
	for index in range(from, to):
		var page := _sim.get_front_page(index, _player)
		var weight := int(page.get("weight", 0))
		if weight < 2 or (weight == 2 and not bool(page.get("involved", false))):
			continue
		if weight == 2 and today - _newspaper_day < 30:
			continue
		if weight >= best_weight:
			best = index
			best_weight = weight
	if best >= 0 and open_newspaper(best):
		_newspaper_day = today


func _place_newspaper() -> void:
	if _newspaper_window == null or not _newspaper_window.is_open():
		return
	var area := get_viewport().get_visible_rect().size
	_newspaper_window.size = _newspaper_window.get_combined_minimum_size()
	_newspaper_window.position = Vector2(
		roundf((area.x - _newspaper_window.size.x) * 0.5), maxf(8.0, roundf((area.y - _newspaper_window.size.y) * 0.5))
	)


# --- Wahlabend (seit M8) ----------------------------------------------------------


func _build_election_night() -> void:
	_election_night = ElectionNight.new()
	$UI.add_child(_election_night)
	# Unter dem Spielmenü, damit es den Wahlabend überdeckt.
	$UI.move_child(_election_night, _game_menu.get_index())
	_election_night.progress_changed.connect(_on_election_progress)
	_election_night.close_requested.connect(_close_election_night)
	_election_night.district_requested.connect(
		func(location: int) -> void: _select_location(location, true)
	)


## Wahlabend (Zugriff für Tests).
func election_night() -> ElectionNight:
	return _election_night


## Öffnet den Wahlabend der letzten Wahl mit dem ganzen Ergebnis (Spielmenü „Letzte Wahl“).
func open_last_election() -> void:
	if _sim == null or _sim.get_election_count() == 0:
		return
	# Nach dem Schließen gilt das Tempo von vor dem Menü (Review M8).
	var resume := _speed_before_menu if _game_menu.is_open() else _speed
	var was_open := _election_night.is_open()
	close_game_menu(false)
	_open_election_night(_sim.get_election_count() - 1, true)
	if not was_open:
		_speed_before_election = resume


## Öffnet den Wahlabend der Wahl index; die Zeit hält an, die Karte zeigt die Wahlen.
func _open_election_night(index: int, complete: bool) -> void:
	var election := _sim.get_election(index)
	if election.is_empty():
		return
	var parliament_index := int(election.get("parliament_index", -1))
	var parliaments := _sim.get_parliaments()
	if parliament_index < 0 or parliament_index >= parliaments.size():
		return
	var parliament := parliaments[parliament_index]
	if not _election_night.is_open():
		_speed_before_election = _speed
	_set_speed(0)
	var by_faction := (
		ElectionTally.day_number(_sim.get_date())
		>= ElectionTally.day_number(parliament.get("faction_date", {}))
	)
	# Meldeposition je Ort für die Schraffur offener Wahlkreise.
	var by_location := _sim.get_location_electoral_district_indices(parliament_index)
	_election_ranks = PackedInt32Array()
	_election_ranks.resize(by_location.size())
	_election_ranks.fill(-1)
	_election_night.open(election, _sim.get_currents(), parliament, by_faction, complete)
	for i in by_location.size():
		if by_location[i] >= 0:
			_election_ranks[i] = _election_night.rank_of_district(by_location[i])
	_left_column.visible = false
	_state_panel.hide_popups()
	close_diplomacy()
	close_war_window()
	_tooltip.hide_tooltip()
	_set_map_mode(MapModes.MAP_WAHLEN)
	_on_election_progress(_election_night.reported_count())


func _on_election_progress(reported: int) -> void:
	var was_revealing := _election_reported >= 0
	_election_reported = reported if not _election_night.is_complete() else -1
	if was_revealing != (_election_reported >= 0):
		_rebuild_legend()
	_refresh_location_panel()
	_refresh_map()
	var hovered := _map_view.get_hovered()
	if hovered >= 0:
		_on_location_hovered(hovered)


## Schließt den Wahlabend; resume setzt die Geschwindigkeit von vorher fort.
func _close_election_night(resume: bool = true) -> void:
	_election_night.close()
	_election_reported = -1
	_left_column.visible = true
	_refresh_map()
	_rebuild_legend()
	if resume:
		_set_speed(_speed_before_election)


## Ausstehende Wahltage der Parlamente (als Tagesnummern, ElectionTally.day_number); seit M13a3 auch
## weitere Wahlen und die Wahlen eines Bundes (pending), deshalb einmal im Monat neu.
func _update_pending_elections() -> void:
	_pending_election_days = PackedInt32Array()
	if _sim == null:
		return
	var today := _sim.get_date()
	_pending_month = int(today.get("year", 0)) * 12 + int(today.get("month", 0))
	for parliament in _sim.get_parliaments():
		if bool(parliament.get("pending", false)):
			_pending_election_days.append(ElectionTally.day_number(parliament.get("election_date", {})))


## Tage bis einschließlich zum letzten Tag der Partie (mindestens 1), solange das Urteil aussteht.
func _days_through_end() -> int:
	if _end_day < 0 or _verdict_shown:
		return MAX_DAYS_PER_FRAME
	var today := ElectionTally.day_number(_sim.get_date())
	return maxi(_end_day - today + 1, 1)


## Tage bis einschließlich zum nächsten Wahltag (mindestens 1), sonst eine große Zahl.
func _days_through_next_election() -> int:
	var today := ElectionTally.day_number(_sim.get_date())
	var result := MAX_DAYS_PER_FRAME
	for day in _pending_election_days:
		if day >= today:
			result = mini(result, day - today + 1)
	return maxi(result, 1)


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
	_game_menu.show_last_election(_sim.get_election_count() > 0)
	_game_menu.show_events(_events_available())
	_game_menu.show_verdict(_sim.is_game_over())
	# Parteienmodus nur mit Parlament (Review M8: 1648 hat Strömungen, aber keine Wahl).
	var roles: Array[Dictionary] = []
	if not _sim.get_parliaments().is_empty():
		roles = _currents
	_game_menu.show_roles(state_name, roles, _player_current)
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
	_after_loading_save() # „Was ist passiert?“ (seit M13b3)
	print("EINIGKEIT: Spielstand geladen: %s, Prüfsumme %s" % [path, _sim.get_checksum()])
	return true


## Nach dem Laden: Spieler, Besitz, Namen, Chronik, Legende und Panels neu aufbauen. Ein
## Spielstand kann ein anderes Szenario mitbringen; dann wird die Karte neu zugeordnet (new_client:
## immer, weil ein neuer Simulationsclient gebunden werden muss).
func _after_load(new_client: bool = false) -> void:
	if _election_night.is_open():
		_close_election_night(false)
	close_diplomacy()
	close_war_window()
	close_journal()
	_close_event_window(false)
	select_army(-1)
	_battle_reports.clear()
	_previous_armies.clear()
	_player_gone_noticed = false
	_player = _sim.get_player_state_index()
	if _read_scenario_constants() or new_client:
		_connect_map_and_simulation()
	_apply_role()  # nach den Konstanten: _currents und _player_current gelten für den Spielstand
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
	_update_railways()
	# Ein Spielstand kann offene Ereignisse mitbringen (die Zeit steht nach dem Laden ohnehin).
	_check_open_events()
	_mood_month = -1
	_update_mood() # seit M13b2


## Neues Spiel (seit M7): lädt das Szenario frisch mit seinem Spielerstaat; die laufende Partie
## bleibt bei einem Fehler erhalten. Leer bei Erfolg, sonst die Fehler.
func start_new_game(scenario_id: String, state_tag: String = "", quiet: bool = false) -> String:
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
	if not playable.is_empty() and not quiet:
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
	close_diplomacy()
	close_war_window()
	_close_event_window(false)
	select_army(-1)
	_battle_reports.clear()
	_player_gone_noticed = false
	_sync_event_player()
	_refresh_all()
	_rebuild_legend()
	_top_bar.show_notice("Ihr spielt jetzt %s" % str(_sim.get_state(_player).get("name", "")))


## Automatischer Spielstand am 1. Januar: die letzten SaveSlots.AUTOSAVE_KEEP bleiben.
## Automatischer Spielstand nach der Einstellung (seit M13a5d): aus, zum 1. Januar oder auch zum 1. Juli.
func _maybe_autosave() -> void:
	var now := _sim.get_date()
	var year := int(now.get("year", 0))
	var half := year * 2 + (1 if int(now.get("month", 0)) >= 7 else 0)
	if _autosave_half < 0:
		_autosave_half = half
		return
	var mode := str(_settings.get_value("spiel/autosave")) if _settings != null else "jaehrlich"
	var due := false
	if mode == "jaehrlich":
		due = int(_autosave_half / 2) != year
	elif mode == "halbjaehrlich":
		due = _autosave_half != half
	if half != _autosave_half and (mode == "aus" or due):
		_autosave_half = half
		if due:
			_autosave()


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
		_tutorial_note("speichern")
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


# --- Platzhalter für die oberste Ebene -------------------------------------------


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func _bottom_of_map_area() -> float:
	return 0.0


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func _refresh_all() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func _sync_area_bar() -> void:
	pass


## Platzhalter; die Hauptszene (main.gd oder main_windows.gd) überschreibt ihn.
func settings() -> UserSettings:
	return null
