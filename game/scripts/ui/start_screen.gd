class_name StartScreen
extends Control
## Startbildschirm (seit M13, Schritt a1; docs/technik/vertical_slice.md Abschnitt 3.1).
##
## Hauptmenü mit Neues Spiel, Fortsetzen, Laden, Geschichtsstunde, Einstellungen und Beenden; ein neues
## Spiel führt in drei Schritten: Lesezeichen, Staat, Spieleinstellungen. Der Startbildschirm rechnet
## nichts: main.gd lädt das gewählte Lesezeichen (es liegt dann schon unter dem Bildschirm), liefert die
## Staaten und Einstellungen und startet die Partie.

## Ein Lesezeichen wurde gewählt; main.gd lädt es und ruft show_states auf.
signal scenario_chosen(scenario_id: String)
## Ein Staat wurde gewählt; main.gd ruft show_settings auf.
signal state_chosen(state_tag: String)
## Die Partie beginnt mit diesen Einstellungen (Stufe leer = Standard).
signal game_requested(state_tag: String, difficulty_id: String, open: bool, compass: bool)
signal load_requested(path: String)
signal quit_requested
## Knopf „Einstellungen“ (seit M13a5d).
signal settings_requested
## „Geschichtsstunde“ im Hauptmenü (seit M13b3).
signal lesson_requested
## Sichtbarkeit geändert (main.gd blendet die Leisten des Spiels dahinter aus).
signal shown_changed(open: bool)

enum Page { MENU, SCENARIO, STATE, SETTINGS, LOAD }

const PANEL_WIDTH := 760.0
## Lesezeichen in der Reihenfolge des Menüs; westfaelischer_frieden steht unter „Weitere“.
const SCENARIO_ORDER: PackedStringArray = ["voelkerfruehling", "dueppel", "koeniggraetz", "sedan"]
const PROTOTYPE_SCENARIO := "westfaelischer_frieden"
const SCENARIO_TEXT := {
	"voelkerfruehling": (
		"Der Deutsche Bund im März 1848: In Paris ist die Monarchie gestürzt, in den deutschen Staaten "
		+ "fordern Versammlungen Pressefreiheit, Geschworene und ein deutsches Parlament. Die Partie "
		+ "läuft bis 1871."
	),
	"dueppel": (
		"Februar 1864: Preußen und Österreich führen gemeinsam Krieg gegen Dänemark um Schleswig, "
		+ "Holstein und Lauenburg."
	),
	"koeniggraetz": (
		"Juni 1866: Der Bund hat gegen Preußen mobil gemacht; drei preußische Armeen stehen an der "
		+ "böhmischen Grenze."
	),
	"sedan": (
		"Juli 1870: Frankreich hat dem Norddeutschen Bund den Krieg erklärt; die süddeutschen Staaten "
		+ "stehen an seiner Seite."
	),
	"westfaelischer_frieden": (
		"Technikprototyp: Die Testregion um 1648 mit 60 Orten und ihrer Bevölkerung, ohne Ereignisse "
		+ "und KI."
	),
}
## Empfohlene Startstaaten je Lesezeichen (Entscheidung 3) mit Schwierigkeit und Kurztext.
const RECOMMENDED := {
	"voelkerfruehling": [
		{"tag": "PRU", "difficulty": "mittel", "text": "Zollverein, Revolution in Berlin, Führung im Norden"},
		{
			"tag": "AUT", "difficulty": "schwer",
			"text": "Präsidium im Bund, Völker des Kaiserstaats, Revolution in Wien und Ungarn"
		},
		{"tag": "BAV", "difficulty": "mittel", "text": "größter Mittelstaat, Trias, Lola-Montez-Affäre"},
		{"tag": "SAX", "difficulty": "schwer", "text": "Industrie, Maiaufstand 1849, zwischen den Großmächten"},
	],
}

var _page := Page.MENU
var _pages: Array[Control] = []
var _continue_button: Button = null
var _load_menu_button: Button = null
var _scenario_list: VBoxContainer = null
var _scenario_group := ButtonGroup.new()
var _scenario_text: Label = null
var _scenario_next: Button = null
var _scenario_ids := PackedStringArray()
var _selected_scenario := ""
var _state_title: Label = null
var _recommended_box: VBoxContainer = null
var _all_states: OptionButton = null
var _all_tags := PackedStringArray()
var _state_group := ButtonGroup.new()
var _state_next: Button = null
var _selected_state := ""
var _settings_title: Label = null
var _difficulty_select: OptionButton = null
var _difficulty_ids := PackedStringArray()
var _difficulty_text: Label = null
var _course_select: OptionButton = null
var _course_text: Label = null
var _compass_toggle: CheckBox = null
var _settings_note: Label = null
var _start_button: Button = null
var _save_list: VBoxContainer = null
var _save_group := ButtonGroup.new()
var _save_load_button: Button = null
var _selected_save := ""
var _newest_save := ""
var _status: Label = null


func _ready() -> void:
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_STOP
	_build()
	visible = false


## Öffnet das Hauptmenü; newest_save ist der jüngste Spielstand (leer: Fortsetzen gesperrt).
func open_menu(newest_save: String = "", has_saves: bool = false) -> void:
	_newest_save = newest_save
	_continue_button.disabled = newest_save.is_empty()
	_continue_button.tooltip_text = (
		tr("Den jüngsten Spielstand laden: {name}").format({"name": newest_save.get_file().get_basename()})
		if not newest_save.is_empty() else tr("Noch kein Spielstand vorhanden")
	)
	_load_menu_button.disabled = not has_saves
	visible = true
	_show_page(Page.MENU)
	shown_changed.emit(true)


func close() -> void:
	if visible:
		visible = false
		shown_changed.emit(false)


func is_open() -> bool:
	return visible


func page() -> int:
	return _page


## Lesezeichen aus SimulationClient.get_scenarios: die vier der Epoche zuerst, 1648 unter „Weitere“.
func show_scenarios(scenarios: Array[Dictionary]) -> void:
	for child in _scenario_list.get_children():
		_scenario_list.remove_child(child)
		child.queue_free()
	_scenario_ids = PackedStringArray()
	var by_id := {}
	for scenario in scenarios:
		by_id[str(scenario.get("id", ""))] = scenario
	var ordered := PackedStringArray()
	for id in SCENARIO_ORDER:
		if by_id.has(id):
			ordered.append(id)
	for id: String in by_id.keys():
		if not ordered.has(id) and id != PROTOTYPE_SCENARIO:
			ordered.append(id)
	for id in ordered:
		_add_scenario_button(by_id[id], false)
	if by_id.has(PROTOTYPE_SCENARIO):
		_scenario_list.add_child(_caption("Weitere"))
		_add_scenario_button(by_id[PROTOTYPE_SCENARIO], true)
	_selected_scenario = ""
	_scenario_text.text = "Ein Lesezeichen wählen."
	_scenario_next.disabled = true
	if not _scenario_ids.is_empty():
		select_scenario(_scenario_ids[0])


## Staaten des geladenen Lesezeichens: je { tag, name, recommended, difficulty, text }; die empfohlenen
## als Knöpfe, alle übrigen in einer Liste.
func show_states(scenario_name: String, states: Array[Dictionary], default_tag: String) -> void:
	_state_title.text = tr("STAAT WÄHLEN – {lesezeichen}").format({"lesezeichen": scenario_name.to_upper()})
	for child in _recommended_box.get_children():
		_recommended_box.remove_child(child)
		child.queue_free()
	_all_states.clear()
	_all_tags = PackedStringArray()
	_all_states.add_item("Anderer Staat …")
	_all_tags.append("")
	for state in states:
		var tag := str(state.get("tag", ""))
		if bool(state.get("recommended", false)):
			var button := Button.new()
			button.name = "State_" + tag
			button.toggle_mode = true
			button.button_group = _state_group
			button.alignment = HORIZONTAL_ALIGNMENT_LEFT
			button.text = "%s  ·  %s" % [str(state.get("name", tag)), _difficulty_word(str(state.get("difficulty", "")))]
			button.tooltip_text = str(state.get("text", ""))
			button.toggled.connect(func(pressed: bool) -> void:
				if pressed:
					_pick_state(tag, false))
			_recommended_box.add_child(button)
			var note := Label.new()
			note.text = str(state.get("text", ""))
			note.theme_type_variation = &"CaptionLabel"
			note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
			_recommended_box.add_child(note)
		else:
			_all_states.add_item(tr("{staat} (nicht abgestimmt)").format({"staat": str(state.get("name", tag))}))
			_all_tags.append(tag)
	_selected_state = ""
	_state_next.disabled = true
	_show_page(Page.STATE)
	if not default_tag.is_empty():
		choose_state(default_tag)


## Spieleinstellungen aus SimulationClient.get_game_settings; ohne KI (1648) nur der Hinweis.
func show_settings(state_name: String, settings: Dictionary) -> void:
	_settings_title.text = tr("SPIELEINSTELLUNGEN – {staat}").format({"staat": state_name.to_upper()})
	_difficulty_select.clear()
	_difficulty_ids = PackedStringArray()
	var active := bool(settings.get("active", false))
	var chosen := 0
	for level: Dictionary in settings.get("difficulties", []):
		_difficulty_select.add_item(str(level.get("name", "")))
		_difficulty_select.set_item_tooltip(_difficulty_select.item_count - 1, str(level.get("text", "")))
		_difficulty_ids.append(str(level.get("id", "")))
		if str(level.get("id", "")) == str(settings.get("difficulty_id", "")):
			chosen = _difficulty_ids.size() - 1
	_difficulty_select.disabled = not active or _difficulty_ids.is_empty()
	_course_select.disabled = not active
	_course_select.select(1 if bool(settings.get("open", false)) else 0)
	_update_course_text()
	if not _difficulty_ids.is_empty():
		_difficulty_select.select(chosen)
	_update_difficulty_text()
	_settings_note.text = (
		"" if active
		else "In diesem Lesezeichen handeln die übrigen Staaten nicht selbst; Stufe und Verlauf gelten nicht."
	)
	_settings_note.visible = not active
	_show_page(Page.SETTINGS)


## Spielstände aus SaveSlots.list (neueste zuerst).
func show_saves(saves: Array[Dictionary]) -> void:
	for child in _save_list.get_children():
		_save_list.remove_child(child)
		child.queue_free()
	_selected_save = ""
	_save_load_button.disabled = true
	var sorted := saves.duplicate()
	sorted.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return int(a.get("modified", 0)) > int(b.get("modified", 0)))
	for save: Dictionary in sorted:
		var info: Dictionary = save.get("info", {})
		var button := Button.new()
		button.toggle_mode = true
		button.button_group = _save_group
		button.alignment = HORIZONTAL_ALIGNMENT_LEFT
		button.text = "%s – %s, %s" % [
			str(save.get("name", "")), str(info.get("player_name", "")), str(info.get("date_text", ""))
		]
		button.tooltip_text = tr("Gespeichert {zeit}").format({"zeit": SaveSlots.format_modified(int(save.get("modified", 0)))})
		if not bool(info.get("ok", true)):
			button.disabled = true
			button.tooltip_text = tr("Nicht lesbar: {grund}").format({"grund": str(info.get("reason", ""))})
		var path := str(save.get("path", ""))
		button.toggled.connect(func(pressed: bool) -> void:
			if pressed:
				_selected_save = path
				_save_load_button.disabled = false)
		_save_list.add_child(button)
	if sorted.is_empty():
		_save_list.add_child(_caption("Noch keine Spielstände."))
	_show_page(Page.LOAD)


func show_status(text: String, is_error: bool = false) -> void:
	_status.text = text
	_status.modulate = UiPalette.NEGATIVE if is_error else Color.WHITE
	_status.visible = not text.is_empty()


# --- Bedienung (auch für Tests) ----------------------------------------------------


func select_scenario(id: String) -> bool:
	for button in _scenario_list.get_children():
		if button is Button and button.name == "Scenario_" + id:
			(button as Button).button_pressed = true
			return true
	return false


func confirm_scenario() -> void:
	if not _selected_scenario.is_empty():
		show_status(tr("Lade {lesezeichen} …").format({"lesezeichen": _selected_scenario}))
		scenario_chosen.emit(_selected_scenario)


func choose_state(tag: String) -> bool:
	for child in _recommended_box.get_children():
		if child is Button and child.name == "State_" + tag:
			(child as Button).button_pressed = true
			return true
	var index := _all_tags.find(tag)
	if index > 0:
		_all_states.select(index)
		_pick_state(tag, true)
		return true
	return false


func confirm_state() -> void:
	if not _selected_state.is_empty():
		state_chosen.emit(_selected_state)


## Wählt die Stufe nach Kennung (Tests); false, wenn es sie nicht gibt.
func choose_difficulty(id: String) -> bool:
	var index := _difficulty_ids.find(id)
	if index < 0:
		return false
	_difficulty_select.select(index)
	_update_difficulty_text()
	return true


func set_open_course(open: bool) -> void:
	_course_select.select(1 if open else 0)
	_update_course_text()


func start_game() -> void:
	var difficulty := ""
	if not _difficulty_select.disabled and _difficulty_select.selected >= 0:
		difficulty = _difficulty_ids[_difficulty_select.selected]
	game_requested.emit(
		_selected_state, difficulty, not _course_select.disabled and _course_select.selected == 1,
		_compass_toggle.button_pressed
	)


func go_back() -> void:
	match _page:
		Page.SCENARIO, Page.LOAD:
			_show_page(Page.MENU)
		Page.STATE:
			_show_page(Page.SCENARIO)
		Page.SETTINGS:
			_show_page(Page.STATE)


func selected_scenario() -> String:
	return _selected_scenario


func selected_state() -> String:
	return _selected_state


## Knopf des Hauptmenüs nach Name (NewGame, Continue, Load, Lesson, Settings, Quit; Tests).
func menu_button(node_name: String) -> Button:
	return _pages[Page.MENU].get_node_or_null(node_name) as Button


## Sichtbare Texte der aktuellen Seite (Tests).
func page_text() -> String:
	var parts := PackedStringArray()
	_collect_text(_pages[_page], parts)
	return "\n".join(parts)


func _gui_input(event: InputEvent) -> void:
	if event.is_action_pressed(&"auswahl_aufheben", false, true) and _page != Page.MENU:
		go_back()
		accept_event()


func _unhandled_input(event: InputEvent) -> void:
	if visible and event.is_action_pressed(&"auswahl_aufheben", false, true) and _page != Page.MENU:
		go_back()
		get_viewport().set_input_as_handled()


# --- Aufbau --------------------------------------------------------------------


func _build() -> void:
	var dimmer := ColorRect.new()
	dimmer.color = Color(UiPalette.INK, 0.8)  # das Spiel dahinter nur schwach sichtbar
	dimmer.mouse_filter = Control.MOUSE_FILTER_IGNORE
	dimmer.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(dimmer)
	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	center.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(center)
	var panel := PanelContainer.new()
	panel.custom_minimum_size = Vector2(PANEL_WIDTH, 0)
	center.add_child(panel)
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 10)
	panel.add_child(box)
	_pages = [_build_menu(), _build_scenarios(), _build_states(), _build_settings(), _build_load()]
	for page_box in _pages:
		box.add_child(page_box)
	_status = Label.new()
	_status.name = "Status"
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_status.visible = false
	box.add_child(_status)


func _build_menu() -> VBoxContainer:
	var page_box := _page_box("Menu")
	var title := Label.new()
	title.text = "EINIGKEIT"
	title.theme_type_variation = &"HeaderLabel"
	title.add_theme_font_size_override(&"font_size", 56)
	title.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	page_box.add_child(title)
	var subtitle := Label.new()
	subtitle.text = "Deutsche Geschichte 1848–1871 · Spielbarer Vertical Slice"
	subtitle.theme_type_variation = &"CaptionLabel"
	subtitle.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	page_box.add_child(subtitle)
	page_box.add_child(HSeparator.new())
	page_box.add_child(_menu_button("Neues Spiel", "NewGame", func() -> void: _show_page(Page.SCENARIO)))
	_continue_button = _menu_button("Fortsetzen", "Continue", func() -> void:
		if not _newest_save.is_empty():
			load_requested.emit(_newest_save))
	page_box.add_child(_continue_button)
	_load_menu_button = _menu_button("Laden …", "Load", func() -> void: _show_page(Page.LOAD))
	page_box.add_child(_load_menu_button)
	var lesson := _menu_button("Geschichtsstunde", "Lesson", func() -> void: lesson_requested.emit())
	lesson.tooltip_text = "„März 1848 in Berlin“: als Preußen in vierzehn Schritten durch das Spiel"
	page_box.add_child(lesson)
	var settings := _menu_button("Einstellungen", "Settings", func() -> void: settings_requested.emit())
	settings.tooltip_text = "Bild, Ton, Spiel, Barrierefreiheit, Tasten und Sprache"
	page_box.add_child(settings)
	page_box.add_child(_menu_button("Beenden", "Quit", func() -> void: quit_requested.emit()))
	return page_box


func _build_scenarios() -> VBoxContainer:
	var page_box := _page_box("Scenarios")
	page_box.add_child(_header("NEUES SPIEL – LESEZEICHEN"))
	_scenario_list = VBoxContainer.new()
	_scenario_list.add_theme_constant_override(&"separation", 4)
	page_box.add_child(_scenario_list)
	_scenario_text = Label.new()
	_scenario_text.name = "ScenarioText"
	_scenario_text.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_scenario_text.custom_minimum_size = Vector2(0, 66)
	page_box.add_child(_scenario_text)
	_scenario_next = _button("Weiter: Staat wählen", confirm_scenario)
	page_box.add_child(_nav_row(_scenario_next))
	return page_box


func _build_states() -> VBoxContainer:
	var page_box := _page_box("States")
	_state_title = _header("STAAT WÄHLEN")
	page_box.add_child(_state_title)
	page_box.add_child(_caption("Empfohlene Staaten"))
	_recommended_box = VBoxContainer.new()
	_recommended_box.add_theme_constant_override(&"separation", 2)
	page_box.add_child(_recommended_box)
	page_box.add_child(_caption("Alle Staaten"))
	_all_states = OptionButton.new()
	_all_states.name = "AllStates"
	_all_states.fit_to_longest_item = false
	_all_states.focus_mode = Control.FOCUS_NONE
	_all_states.item_selected.connect(func(index: int) -> void:
		if index > 0:
			_pick_state(_all_tags[index], true))
	page_box.add_child(_all_states)
	_state_next = _button("Weiter: Spieleinstellungen", confirm_state)
	page_box.add_child(_nav_row(_state_next))
	return page_box


func _build_settings() -> VBoxContainer:
	var page_box := _page_box("Settings")
	_settings_title = _header("SPIELEINSTELLUNGEN")
	page_box.add_child(_settings_title)
	page_box.add_child(_caption("Schwierigkeit"))
	_difficulty_select = OptionButton.new()
	_difficulty_select.name = "Difficulty"
	_difficulty_select.focus_mode = Control.FOCUS_NONE
	_difficulty_select.item_selected.connect(func(_index: int) -> void: _update_difficulty_text())
	page_box.add_child(_difficulty_select)
	_difficulty_text = Label.new()
	_difficulty_text.theme_type_variation = &"CaptionLabel"
	_difficulty_text.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	page_box.add_child(_difficulty_text)
	page_box.add_child(_caption("Geschichtsverlauf"))
	_course_select = OptionButton.new()
	_course_select.name = "Course"
	_course_select.focus_mode = Control.FOCUS_NONE
	_course_select.add_item("Historisch")
	_course_select.add_item("Offen")
	_course_select.item_selected.connect(func(_index: int) -> void: _update_course_text())
	page_box.add_child(_course_select)
	# Erklärung unter der Auswahl (seit M13b4: umbrechend, damit längere Übersetzungen Platz haben).
	_course_text = Label.new()
	_course_text.name = "CourseText"
	_course_text.theme_type_variation = &"CaptionLabel"
	_course_text.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	page_box.add_child(_course_text)
	_compass_toggle = CheckBox.new()
	_compass_toggle.name = "Compass"
	_compass_toggle.text = "Historischer Kompass in Ereignissen (◆ zeigt die historische Wahl)"
	_compass_toggle.button_pressed = true
	_compass_toggle.focus_mode = Control.FOCUS_NONE
	page_box.add_child(_compass_toggle)
	_settings_note = Label.new()
	_settings_note.theme_type_variation = &"CaptionLabel"
	_settings_note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	page_box.add_child(_settings_note)
	_start_button = _button("Spiel beginnen", start_game)
	_start_button.name = "StartGame"
	page_box.add_child(_nav_row(_start_button))
	return page_box


func _build_load() -> VBoxContainer:
	var page_box := _page_box("LoadPage")
	page_box.add_child(_header("SPIELSTAND LADEN"))
	var scroll := ScrollContainer.new()
	scroll.custom_minimum_size = Vector2(0, 280)
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	page_box.add_child(scroll)
	_save_list = VBoxContainer.new()
	_save_list.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(_save_list)
	_save_load_button = _button("Laden", func() -> void:
		if not _selected_save.is_empty():
			load_requested.emit(_selected_save))
	page_box.add_child(_nav_row(_save_load_button))
	return page_box


func _page_box(node_name: String) -> VBoxContainer:
	var page_box := VBoxContainer.new()
	page_box.name = node_name
	page_box.add_theme_constant_override(&"separation", 8)
	return page_box


func _nav_row(primary: Button) -> HBoxContainer:
	var row := HBoxContainer.new()
	row.add_theme_constant_override(&"separation", 8)
	row.add_child(_button("Zurück", go_back))
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(spacer)
	row.add_child(primary)
	return row


func _header(text: String) -> Label:
	var label := Label.new()
	label.text = text
	label.theme_type_variation = &"HeaderLabel"
	return label


func _caption(text: String) -> Label:
	var label := Label.new()
	label.text = text
	label.theme_type_variation = &"CaptionLabel"
	return label


func _button(text: String, action: Callable) -> Button:
	var button := Button.new()
	button.text = text
	button.focus_mode = Control.FOCUS_NONE
	button.pressed.connect(action)
	return button


func _menu_button(text: String, node_name: String, action: Callable) -> Button:
	var button := _button(text, action)
	button.name = node_name
	button.custom_minimum_size = Vector2(0, 40)
	return button


func _add_scenario_button(scenario: Dictionary, prototype: bool) -> void:
	var id := str(scenario.get("id", ""))
	var button := Button.new()
	button.name = "Scenario_" + id
	button.toggle_mode = true
	button.button_group = _scenario_group
	button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.text = "%s  ·  %s%s" % [
		str(scenario.get("name", id)), str(scenario.get("date_text", "")),
		"  ·  " + tr("Technikprototyp (Testregion)") if prototype else ""
	]
	button.toggled.connect(func(pressed: bool) -> void:
		if pressed:
			_selected_scenario = id
			_scenario_text.text = str(SCENARIO_TEXT.get(id, ""))
			_scenario_next.disabled = false)
	_scenario_list.add_child(button)
	_scenario_ids.append(id)


## Erklärung des gewählten Geschichtsverlaufs.
func _update_course_text() -> void:
	_course_text.text = (
		"Die Staaten verfolgen ihre Ziele in den historischen Zeitfenstern." if _course_select.selected != 1
		else "Ziele ohne Zeitfenster und mit halbem Gewicht; eine freiere Geschichte."
	)


## Schwierigkeit eines empfohlenen Staates, übersetzt (seit M13b4).
func _difficulty_word(word: String) -> String:
	match word:
		"leicht":
			return tr("leicht")
		"mittel":
			return tr("mittel")
		"schwer":
			return tr("schwer")
	return word


func _pick_state(tag: String, from_list: bool) -> void:
	_selected_state = tag
	_state_next.disabled = tag.is_empty()
	if from_list:
		var pressed := _state_group.get_pressed_button()
		if pressed != null:
			pressed.set_pressed_no_signal(false)
	else:
		_all_states.select(0)


func _update_difficulty_text() -> void:
	var index := _difficulty_select.selected
	_difficulty_text.text = (
		_difficulty_select.get_item_tooltip(index) if index >= 0 and index < _difficulty_select.item_count
		else ""
	)


func _show_page(target: Page) -> void:
	_page = target
	for i in _pages.size():
		_pages[i].visible = i == target
	show_status("")


func _collect_text(node: Node, parts: PackedStringArray) -> void:
	if node is Control and not (node as Control).visible:
		return
	if node is Label:
		parts.append((node as Label).text)
	elif node is Button:
		parts.append((node as Button).text)
	elif node is OptionButton:
		parts.append((node as OptionButton).text)
	for child in node.get_children():
		_collect_text(child, parts)
