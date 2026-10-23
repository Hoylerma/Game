class_name DiplomacyWindow
extends PanelContainer
## Fenster „Diplomatie“ (seit M9, Schritt b3, docs/technik/diplomatie.md Abschnitt 6) mit drei
## Reitern: die Staaten mit Beziehung, Lager, Verträgen und Gesandten und den Handlungen gegenüber dem
## gewählten Staat; die Bundesversammlung mit Halbrund, offenem Antrag, Anträgen und den letzten
## Abstimmungen; „Die deutsche Frage“ als Anzeige ohne Wertung (Abschnitt 4.6).
##
## Das Fenster liest über SimulationClient und meldet Befehle als Signale; main.gd schickt sie an die
## Simulation. Zeilen und Knöpfe werden einmal angelegt und danach nur aktualisiert, damit ein Klick
## bei laufender Zeit nicht verloren geht (Review M8).

signal envoy_requested(target: int, send: bool)
signal treaty_requested(target: int, action: int)
signal demand_requested(target: int, kind: int)
signal motion_requested(kind: int, target: int)
signal close_requested

const WIDTH := 620.0
const LIST_HEIGHT := 250.0
## Kürzeste Staatenliste, wenn das Fenster sonst nicht in die Höhe passt (fit_height).
const MIN_LIST_HEIGHT := 120.0
## Kürzester Abstand zwischen zwei Füllungen bei laufender Zeit.
const REFRESH_MS := 1000
const MAX_RESULTS := 8
## Farben der Lager im Halbrund (Preußen, Österreich, unentschieden).
const CAMP_COLORS: Array[Color] = [Color(0.20, 0.27, 0.45), Color(0.80, 0.62, 0.16), Color(0.66, 0.63, 0.57)]
## Handlungen im Reiter „Staaten“: Schlüssel aus get_diplomacy_actions, Text, Art des Befehls, Wert.
const ACTIONS: Array = [
	["envoy_send", "Gesandten entsenden", "envoy", 1],
	["envoy_recall", "Gesandten abberufen", "envoy", 0],
	["alliance", "Bündnis anbieten", "treaty", SimulationClient.TREATY_ALLIANCE],
	["cancel_alliance", "Bündnis kündigen", "treaty", SimulationClient.TREATY_CANCEL_ALLIANCE],
	["guarantee", "Garantie geben", "treaty", SimulationClient.TREATY_GUARANTEE],
	["cancel_guarantee", "Garantie kündigen", "treaty", SimulationClient.TREATY_CANCEL_GUARANTEE],
	["demand_leadership", "Fordern: Führung im Bund", "demand", SimulationClient.DEMAND_LEADERSHIP],
	["demand_customs_union", "Fordern: Beitritt zur Zollunion", "demand", SimulationClient.DEMAND_CUSTOMS_UNION],
	["demand_break_alliance", "Fordern: Bündnisse lösen", "demand", SimulationClient.DEMAND_BREAK_ALLIANCE],
	["motion_intervention", "Bundesintervention beantragen", "motion", SimulationClient.MOTION_INTERVENTION],
]

var _sim: SimulationClient = null
var _player := -1
var _selected := -1
var _filled_at := 0
var _relations: Array[Dictionary] = []
var _diet: Dictionary = {}
var _tab_buttons: Array[Button] = []
var _tab_pages: Array[Control] = []
var _current_tab := 0
var _summary: Label = null
var _rows: VBoxContainer = null
var _list_scroll: ScrollContainer = null
## Staat → [Zeile (HBoxContainer), Farbfeld, Knopf].
var _row_by_state := {}
var _detail_title: Label = null
var _detail_info: Label = null
var _action_buttons := {}
var _question_box: VBoxContainer = null
var _question: Label = null
var _pending_demand := -1
var _note: Label = null
var _diet_info: Label = null
var _hemicycle: Control = null
var _hemicycle_layout := {}
var _hemicycle_key := Vector3.ZERO
var _motion_label: Label = null
var _motion_buttons: VBoxContainer = null
var _results: VBoxContainer = null
var _question_grid: GridContainer = null
var _question_lists: VBoxContainer = null


func _ready() -> void:
	name = "DiplomacyWindow"
	custom_minimum_size = Vector2(WIDTH, 0)
	visible = false
	mouse_filter = Control.MOUSE_FILTER_STOP
	_build()


## Öffnet das Fenster für den Spielerstaat.
func open(sim: SimulationClient, player: int, tab: int = 0) -> void:
	_sim = sim
	if player != _player:
		_selected = -1
	_player = player
	_pending_demand = -1
	_question_box.visible = false
	_note.text = ""
	visible = true
	show_tab(tab)
	refresh(true)


func close() -> void:
	visible = false


func is_open() -> bool:
	return visible


## Neu füllen; ohne force höchstens einmal je REFRESH_MS (bei laufender Zeit).
func refresh(force: bool = false) -> void:
	if not visible or _sim == null:
		return
	if not force and Time.get_ticks_msec() - _filled_at < REFRESH_MS:
		return
	_filled_at = Time.get_ticks_msec()
	_fill_states()
	_fill_diet()
	_fill_question()


## Höchsthöhe des Fensters (main.gd): Die Staatenliste wird kürzer, wenn es sonst nicht passt.
func fit_height(max_height: float) -> void:
	_list_scroll.custom_minimum_size.y = LIST_HEIGHT
	var overflow := get_combined_minimum_size().y - max_height
	if overflow > 0.0:
		_list_scroll.custom_minimum_size.y = maxf(MIN_LIST_HEIGHT, LIST_HEIGHT - overflow)


## Reiter zeigen: 0 Staaten, 1 Bundesversammlung, 2 Deutsche Frage.
func show_tab(tab: int) -> void:
	_current_tab = clampi(tab, 0, _tab_pages.size() - 1)
	for i in _tab_pages.size():
		_tab_pages[i].visible = i == _current_tab
		_tab_buttons[i].set_pressed_no_signal(i == _current_tab)


func current_tab() -> int:
	return _current_tab


## Einen Staat in der Liste wählen (auch für Tests).
func select_state(state: int) -> void:
	_selected = state
	_pending_demand = -1
	_question_box.visible = false
	_fill_detail()
	_highlight_rows()


func selected_state() -> int:
	return _selected


## Rückmeldung nach einem Befehl (main.gd).
func show_note(text: String) -> void:
	_note.text = text
	_filled_at = 0


## Text des Fensters, zeilenweise (für Tests): Zusammenfassung, Auswahl, Knöpfe, Bundesversammlung.
func window_text() -> String:
	var lines := PackedStringArray([_summary.text, _detail_title.text, _detail_info.text])
	for key: String in _action_buttons:
		var button: Button = _action_buttons[key]
		lines.append("[%s%s]" % [button.text, " gesperrt" if button.disabled else ""])
	if _question_box.visible:
		lines.append(_question.text)
	lines.append(_note.text)
	lines.append(_diet_info.text)
	lines.append(_motion_label.text)
	for child in _motion_buttons.get_children():
		lines.append("[%s%s]" % [(child as Button).text, " gesperrt" if (child as Button).disabled else ""])
	for child in _results.get_children():
		lines.append((child as Label).text)
	for i in range(0, _question_grid.get_child_count() - 1, 2):
		lines.append("%s: %s" % [
			(_question_grid.get_child(i) as Label).text, (_question_grid.get_child(i + 1) as Label).text
		])
	return "\n".join(lines)


# --- Aufbau ------------------------------------------------------------------


func _build() -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 6)
	add_child(box)
	var head := HBoxContainer.new()
	var header := Label.new()
	header.text = "DIPLOMATIE"
	header.theme_type_variation = &"HeaderLabel"
	header.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	head.add_child(header)
	var close_button := Button.new()
	close_button.name = "Close"
	close_button.text = "×"
	close_button.theme_type_variation = &"FlatButton"
	close_button.focus_mode = Control.FOCUS_NONE
	close_button.tooltip_text = "Fenster schließen"
	close_button.pressed.connect(func() -> void: close_requested.emit())
	head.add_child(close_button)
	box.add_child(head)
	# Reiter als Knöpfe im Stil des Themes (ein TabContainer hätte kein Theme).
	var tabs := HBoxContainer.new()
	tabs.name = "Tabs"
	tabs.add_theme_constant_override(&"separation", 4)
	box.add_child(tabs)
	box.add_child(HSeparator.new())
	var group := ButtonGroup.new()
	group.allow_unpress = false
	for page: Control in [_build_states_tab(), _build_diet_tab(), _build_question_tab()]:
		var button := Button.new()
		button.name = "Tab%d" % _tab_pages.size()
		button.text = page.name
		button.toggle_mode = true
		button.button_group = group
		button.focus_mode = Control.FOCUS_NONE
		button.pressed.connect(show_tab.bind(_tab_pages.size()))
		tabs.add_child(button)
		_tab_buttons.append(button)
		_tab_pages.append(page)
		box.add_child(page)


func _build_states_tab() -> Control:
	var tab := VBoxContainer.new()
	tab.name = "Staaten"
	tab.add_theme_constant_override(&"separation", 6)
	_summary = _label(&"MutedLabel", true)
	tab.add_child(_summary)
	var scroll := ScrollContainer.new()
	scroll.custom_minimum_size = Vector2(0, LIST_HEIGHT)
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_list_scroll = scroll
	_rows = VBoxContainer.new()
	_rows.name = "StateRows"
	_rows.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_rows.add_theme_constant_override(&"separation", 1)
	scroll.add_child(_rows)
	tab.add_child(scroll)
	tab.add_child(HSeparator.new())
	_detail_title = _label(&"NameLabel", true)
	tab.add_child(_detail_title)
	_detail_info = _label(&"MutedLabel", true)
	tab.add_child(_detail_info)
	var grid := GridContainer.new()
	grid.name = "Actions"
	grid.columns = 2
	grid.add_theme_constant_override(&"h_separation", 6)
	grid.add_theme_constant_override(&"v_separation", 4)
	for action: Array in ACTIONS:
		var button := Button.new()
		button.name = "Action_%s" % str(action[0])
		button.text = str(action[1])
		button.focus_mode = Control.FOCUS_NONE
		button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		button.clip_text = true
		button.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		button.pressed.connect(_on_action.bind(action))
		grid.add_child(button)
		_action_buttons[str(action[0])] = button
	tab.add_child(grid)
	_question_box = VBoxContainer.new()
	_question_box.name = "DemandQuestion"
	_question_box.visible = false
	_question = _label(&"Label", true)
	_question_box.add_child(_question)
	var answers := HBoxContainer.new()
	answers.alignment = BoxContainer.ALIGNMENT_END
	var cancel := Button.new()
	cancel.name = "DemandCancel"
	cancel.text = "Abbrechen"
	cancel.focus_mode = Control.FOCUS_NONE
	cancel.pressed.connect(func() -> void:
		_pending_demand = -1
		_question_box.visible = false
	)
	answers.add_child(cancel)
	var confirm := Button.new()
	confirm.name = "DemandConfirm"
	confirm.text = "Forderung stellen"
	confirm.focus_mode = Control.FOCUS_NONE
	confirm.pressed.connect(_confirm_demand)
	answers.add_child(confirm)
	_question_box.add_child(answers)
	tab.add_child(_question_box)
	_note = _label(&"MutedLabel", true)
	tab.add_child(_note)
	return tab


func _build_diet_tab() -> Control:
	var tab := VBoxContainer.new()
	tab.name = "Bundesversammlung"
	tab.add_theme_constant_override(&"separation", 6)
	_diet_info = _label(&"MutedLabel", true)
	tab.add_child(_diet_info)
	_hemicycle = Control.new()
	_hemicycle.name = "Hemicycle"
	_hemicycle.custom_minimum_size = Vector2(0, 150)
	_hemicycle.draw.connect(_draw_hemicycle)
	tab.add_child(_hemicycle)
	var legend := HBoxContainer.new()
	legend.alignment = BoxContainer.ALIGNMENT_CENTER
	for i in 3:
		var swatch := ColorRect.new()
		swatch.color = CAMP_COLORS[i]
		swatch.custom_minimum_size = Vector2(12, 12)
		swatch.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		legend.add_child(swatch)
		var caption := _label(&"MutedLabel", false)
		caption.text = ["Lager Preußen", "Lager Österreich", "unentschieden"][i]
		legend.add_child(caption)
	tab.add_child(legend)
	_motion_label = _label(&"Label", true)
	tab.add_child(_motion_label)
	_motion_buttons = VBoxContainer.new()
	_motion_buttons.name = "Motions"
	_motion_buttons.add_theme_constant_override(&"separation", 4)
	tab.add_child(_motion_buttons)
	var caption := _label(&"CaptionLabel", false)
	caption.text = "LETZTE ABSTIMMUNGEN"
	tab.add_child(caption)
	_results = VBoxContainer.new()
	_results.add_theme_constant_override(&"separation", 2)
	tab.add_child(_results)
	return tab


func _build_question_tab() -> Control:
	var tab := VBoxContainer.new()
	tab.name = "Deutsche Frage"
	tab.add_theme_constant_override(&"separation", 6)
	_question_grid = GridContainer.new()
	_question_grid.columns = 2
	_question_grid.add_theme_constant_override(&"h_separation", 18)
	tab.add_child(_question_grid)
	_question_lists = VBoxContainer.new()
	_question_lists.add_theme_constant_override(&"separation", 2)
	tab.add_child(_question_lists)
	var note := _label(&"CaptionLabel", true)
	note.text = (
		"Die Lage, nach der später die Ausgänge der deutschen Frage bewertet werden (kleindeutsch, "
		+ "großdeutsch, Bund, Scheitern …); hier noch ohne Wertung. Lager: Neigung eines Bundesstaates zu "
		+ "Preußen oder Österreich (Unterschied der Beziehungen über 20)."
	)
	tab.add_child(note)
	return tab


func _label(variation: StringName, wrap: bool) -> Label:
	var label := Label.new()
	if variation != &"Label":
		label.theme_type_variation = variation
	if wrap:
		label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		label.custom_minimum_size = Vector2(WIDTH - 40.0, 0)
	return label


# --- Reiter „Staaten“ --------------------------------------------------------


func _fill_states() -> void:
	_relations = _sim.get_relations(_player)
	var overview := _sim.get_diplomacy(_player)
	var player_state := _sim.get_state(_player)
	_summary.text = "%s: Gesandte %s von %s (Kosten %s je Monat, je Gesandtem %s der Einnahmen)." % [
		str(player_state.get("short_name", "")),
		GermanFormat.integer(int(overview.get("envoy_count", 0))),
		GermanFormat.integer(int(overview.get("envoy_capacity", 0))),
		GermanFormat.money(float(overview.get("envoy_cost_month", 0.0))),
		GermanFormat.percent(float(overview.get("envoy_cost_percent", 0.0)), 1),
	]
	if bool(overview.get("mobilized", false)):
		_summary.text += " Das Heer ist mobilgemacht (Kosten × 1,5)."
	var sorted := _relations.duplicate()
	sorted.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return float(a.get("relation", 0.0)) > float(b.get("relation", 0.0))
	)
	# Zeilen eines anderen Spielstands (andere Staaten) bleiben unsichtbar.
	for state: int in _row_by_state:
		((_row_by_state[state] as Array)[0] as Control).visible = false
	for i in sorted.size():
		var entry: Dictionary = sorted[i]
		var state := int(entry.get("index", -1))
		if not _row_by_state.has(state):
			_make_row(state)
		var parts: Array = _row_by_state[state]
		(parts[0] as Control).visible = true
		_rows.move_child(parts[0] as Control, i)
		(parts[1] as ColorRect).color = LegendData.relation_color(float(entry.get("relation", 0.0)))
		(parts[2] as Button).text = "%s   %s%s" % [
			str(entry.get("short_name", "")),
			GermanFormat.signed_number(roundi(float(entry.get("relation", 0.0)))),
			_markers(entry),
		]
	if (_selected < 0 or _entry_of(_selected).is_empty()) and not sorted.is_empty():
		_selected = int(sorted[0].get("index", -1))
	_fill_detail()
	_highlight_rows()


## Zeile: Farbfeld der Beziehung (wie der Kartenmodus „Beziehungen“) und ein Knopf mit Name, Wert
## und Merkmalen.
func _make_row(state: int) -> void:
	var row := HBoxContainer.new()
	row.add_theme_constant_override(&"separation", 6)
	var swatch := ColorRect.new()
	swatch.custom_minimum_size = LegendPanel.SWATCH_SIZE
	swatch.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	row.add_child(swatch)
	var button := Button.new()
	button.name = "State%d" % state
	button.theme_type_variation = &"FlatButton"
	button.toggle_mode = true
	button.focus_mode = Control.FOCUS_NONE
	button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	button.clip_text = true
	button.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	button.pressed.connect(select_state.bind(state))
	row.add_child(button)
	_rows.add_child(row)
	_row_by_state[state] = [row, swatch, button]


func _highlight_rows() -> void:
	for state: int in _row_by_state:
		((_row_by_state[state] as Array)[2] as Button).set_pressed_no_signal(state == _selected)


func _markers(entry: Dictionary) -> String:
	var parts := _marker_list(entry)
	return "   · " + " · ".join(parts) if not parts.is_empty() else ""


## Verträge, Gesandte, Gegnerschaft, Krise und Lager eines Staates (Liste und Auswahl).
func _marker_list(entry: Dictionary) -> PackedStringArray:
	var parts := PackedStringArray()
	if bool(entry.get("allied", false)):
		parts.append("Bündnis")
	if bool(entry.get("guarantees", false)):
		parts.append("von uns garantiert")
	if bool(entry.get("guaranteed", false)):
		parts.append("garantiert uns")
	if bool(entry.get("envoy", false)):
		parts.append("unser Gesandter")
	if bool(entry.get("envoy_from", false)):
		parts.append("Gesandter bei uns")
	if bool(entry.get("rival", false)):
		parts.append("Gegner")
	if bool(entry.get("in_play", false)):
		parts.append("in einer Krise")
	if bool(entry.get("federal", false)):
		parts.append("Lager %s" % str(entry.get("camp_text", "")))
	return parts


func _entry_of(state: int) -> Dictionary:
	for entry in _relations:
		if int(entry.get("index", -1)) == state:
			return entry
	return {}


func _fill_detail() -> void:
	var entry := _entry_of(_selected)
	if entry.is_empty():
		_detail_title.text = "Kein Staat gewählt"
		_detail_info.text = ""
		for key: String in _action_buttons:
			(_action_buttons[key] as Button).disabled = true
		return
	var actions := _sim.get_diplomacy_actions(_player, _selected)
	var overview := _sim.get_diplomacy(_player)
	_detail_title.text = "%s – Beziehung %s (Zielwert %s)" % [
		str(entry.get("name", "")),
		GermanFormat.signed_number(roundi(float(entry.get("relation", 0.0)))),
		GermanFormat.signed_number(roundi(float(entry.get("target", 0.0)))),
	]
	var facts := PackedStringArray()
	var union_name := str(entry.get("customs_union_name", ""))
	facts.append("Zollunion: %s" % (union_name if not union_name.is_empty() else "keine"))
	if int(entry.get("plenum_votes", 0)) > 0:
		facts.append("Stimmen im Plenum: %s" % GermanFormat.integer(int(entry.get("plenum_votes", 0))))
	facts.append_array(_marker_list(entry))
	_detail_info.text = " · ".join(facts) + "\n" + (
		"Bei einer Krise auf unserer Seite: %s; auf der Gegenseite: %s (Soldaten %s gegen %s)." % [
			_names_text(actions.get("own_side", PackedStringArray())),
			_names_text(actions.get("other_side", PackedStringArray())),
			GermanFormat.integer(int(actions.get("own_strength", 0))),
			GermanFormat.integer(int(actions.get("other_strength", 0))),
		]
	)
	for action: Array in ACTIONS:
		var key := str(action[0])
		var button: Button = _action_buttons[key]
		var decision := str(actions.get(key, "ungueltig"))
		button.disabled = decision != "angenommen"
		button.tooltip_text = _action_tooltip(key, decision, entry, overview, actions)
		button.text = str(action[1])
		if key == "demand_customs_union" and not str(actions.get("customs_union_name", "")).is_empty():
			button.text = "Fordern: Beitritt zum %s" % str(actions.get("customs_union_name", ""))
		if decision == "abgelehnt":
			button.text += " (lehnt ab)"


func _names_text(names: PackedStringArray) -> String:
	if names.is_empty():
		return "niemand"
	if names.size() > 6:
		return "%s und %s weitere" % [", ".join(names.slice(0, 5)), GermanFormat.integer(names.size() - 5)]
	return ", ".join(names)


func _action_tooltip(
	key: String, decision: String, entry: Dictionary, overview: Dictionary, actions: Dictionary
) -> String:
	var state_name := str(entry.get("short_name", ""))
	match key:
		"envoy_send":
			return (
				"Ein Gesandter verbessert die Beziehung um 5 je Monat (bis 60 über den Zielwert) und kostet "
				+ "%s der Monatseinnahmen. Frei: %s von %s." % [
					GermanFormat.percent(float(overview.get("envoy_cost_percent", 0.0)), 1),
					GermanFormat.integer(
						int(overview.get("envoy_capacity", 0)) - int(overview.get("envoy_count", 0))
					),
					GermanFormat.integer(int(overview.get("envoy_capacity", 0))),
				]
			)
		"alliance":
			if decision == "abgelehnt":
				return "%s lehnt ab: ein Bündnis erst ab Beziehung %s und nicht mit dem Verbündeten eines Gegners." % [
					state_name, GermanFormat.signed_number(float(overview.get("alliance_min", 100.0)))
				]
			return "Bündnis (gegenseitig): Zielwert +40; Verbündete stehen in Krisen auf derselben Seite."
		"guarantee":
			if decision == "abgelehnt":
				return "%s lehnt ab: eine Garantie erst ab Beziehung %s." % [
					state_name, GermanFormat.signed_number(float(overview.get("guarantee_min", 50.0)))
				]
			return "Garantie: Wir schützen %s; Zielwert +20, in Krisen auf seiner Seite." % state_name
		"demand_leadership":
			return "Krise um die Führung im Bund: drei Stufen zu 30 Tagen; die schwächere Seite gibt nach."
		"demand_customs_union":
			return (
				"Krise: %s soll unserer Zollunion beitreten; beim Kompromiss zahlt es ein halbes Monatseinkommen."
				% state_name
			)
		"demand_break_alliance":
			return "Krise: %s soll seine Bündnisse mit unseren Gegnern kündigen." % state_name
		"motion_intervention":
			return "Antrag in der Bundesversammlung (Engerer Rat, Mehrheit); abgestimmt am nächsten Monatsersten."
	return "" if decision == "angenommen" else "Zur Zeit nicht möglich."


func _on_action(action: Array) -> void:
	if _selected < 0:
		return
	var kind := str(action[2])
	var value := int(action[3])
	match kind:
		"envoy":
			envoy_requested.emit(_selected, value == 1)
		"treaty":
			treaty_requested.emit(_selected, value)
		"motion":
			motion_requested.emit(value, _selected)
		"demand":
			# Eine Forderung löst eine Krise aus: Rückfrage.
			_pending_demand = value
			var entry := _entry_of(_selected)
			var actions := _sim.get_diplomacy_actions(_player, _selected)
			_question.text = (
				"%s fordern? Das löst eine Krise aus: Protest, Ultimatum und Mobilmachung zu je 30 Tagen. "
				% str((_action_buttons[str(action[0])] as Button).text).trim_prefix("Fordern: ")
				+ "Auf unserer Seite: %s (%s Soldaten); bei %s: %s (%s Soldaten)." % [
					_names_text(actions.get("own_side", PackedStringArray())),
					GermanFormat.integer(int(actions.get("own_strength", 0))),
					str(entry.get("short_name", "")),
					_names_text(actions.get("other_side", PackedStringArray())),
					GermanFormat.integer(int(actions.get("other_strength", 0))),
				]
			)
			_question_box.visible = true


func _confirm_demand() -> void:
	if _pending_demand < 0 or _selected < 0:
		return
	demand_requested.emit(_selected, _pending_demand)
	_pending_demand = -1
	_question_box.visible = false


# --- Reiter „Bundesversammlung“ ----------------------------------------------


func _fill_diet() -> void:
	_diet = _sim.get_diet()
	_hemicycle.queue_redraw()
	for child in _results.get_children():
		_results.remove_child(child)
		child.queue_free()
	if _diet.is_empty():
		_diet_info.text = "In diesem Szenario tagt keine Bundesversammlung."
		_motion_label.text = ""
		for child in _motion_buttons.get_children():
			(child as Button).visible = false
		return
	_diet_info.text = "%s in %s, Präsidialmacht %s. Plenum: %s Stimmen; Engerer Rat: %s Stimmen." % [
		str(_diet.get("name", "")),
		str(_diet.get("seat_name", "–")),
		str(_diet.get("presidency_name", "–")),
		GermanFormat.integer(int(_diet.get("plenum_total", 0))),
		GermanFormat.integer((_diet.get("council", []) as Array).size()),
	]
	var motion: Dictionary = _diet.get("motion", {})
	if motion.is_empty():
		_motion_label.text = "Kein Antrag offen. Je Monat stimmt die Versammlung über höchstens einen ab."
	else:
		_motion_label.text = "Offener Antrag von %s: %s – abgestimmt am %s (%s)." % [
			str(motion.get("applicant_name", "")),
			str(motion.get("text", "")),
			str(motion.get("vote_text", "")),
			str(motion.get("majority_text", "")),
		]
	var motions: Array = [
		[SimulationClient.MOTION_REFORM, -1],
		[SimulationClient.MOTION_PRESIDENCY, -1],
	]
	# Zolleinigung nur für die eigene Zollunion.
	var own_union := int(_sim.get_state(_player).get("customs_union_index", -1))
	if own_union >= 0:
		motions.append([SimulationClient.MOTION_CUSTOMS, own_union])
	while _motion_buttons.get_child_count() < motions.size():
		var button := Button.new()
		button.focus_mode = Control.FOCUS_NONE
		button.alignment = HORIZONTAL_ALIGNMENT_LEFT
		button.clip_text = true
		button.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		button.pressed.connect(_on_motion.bind(_motion_buttons.get_child_count()))
		_motion_buttons.add_child(button)
	for i in _motion_buttons.get_child_count():
		var button := _motion_buttons.get_child(i) as Button
		button.visible = i < motions.size()
		if not button.visible:
			continue
		var kind := int(motions[i][0])
		var target := int(motions[i][1])
		button.name = "Motion%d_%d" % [kind, target]
		button.set_meta(&"motion", [kind, target])
		var preview := _sim.get_motion_preview(_player, kind, target)
		var decision := str(preview.get("decision", "ungueltig"))
		button.disabled = decision != "angenommen"
		var text := "%s beantragen" % str(preview.get("text", ""))
		if decision == "angenommen":
			var council := bool(preview.get("council_body", false))
			text += " · Prognose %s von %s Stimmen" % [
				GermanFormat.integer(int(preview.get("council_yes" if council else "plenum_yes", 0))),
				GermanFormat.integer(int(preview.get("council_total" if council else "plenum_total", 0))),
			]
		button.text = text
		button.tooltip_text = (
			"Nötig: %s. Gegner des Antrags: %s. Ein Staat stimmt mit Ja, wenn er uns näher steht als dem Gegner."
			% [str(preview.get("majority_text", "")), str(preview.get("opponent_name", "–"))]
			if decision == "angenommen" else "Zur Zeit nicht möglich (offener Antrag oder unpassendes Ziel)."
		)
	var results: Array = _diet.get("results", [])
	for k in mini(results.size(), MAX_RESULTS):
		var r: Dictionary = results[results.size() - 1 - k]
		var council := bool(r.get("council_body", false))
		var label := _label(&"MutedLabel", true)
		label.text = "%s · %s: %s – %s (%s von %s)" % [
			str(r.get("date_text", "")),
			str(r.get("applicant_name", "")),
			str(r.get("text", "")),
			"angenommen" if bool(r.get("accepted", false)) else "abgelehnt",
			GermanFormat.integer(int(r.get("council_yes" if council else "plenum_yes", 0))),
			GermanFormat.integer(int(r.get("council_total" if council else "plenum_total", 0))),
		]
		_results.add_child(label)
	if results.is_empty():
		var none := _label(&"MutedLabel", true)
		none.text = "Noch keine Abstimmung."
		_results.add_child(none)


func _on_motion(index: int) -> void:
	var button := _motion_buttons.get_child(index) as Button
	var motion: Array = button.get_meta(&"motion", [-1, -1])
	motion_requested.emit(int(motion[0]), int(motion[1]))


## Halbrund des Plenums: je Stimme ein Platz in der Farbe des Lagers ihres Staates, von links
## (Preußen) nach rechts (Österreich).
func _draw_hemicycle() -> void:
	var plenum: Array = _diet.get("plenum", [])
	var seats: Array[int] = []
	for camp_order: int in [1, 0, -1]:
		for entry: Dictionary in plenum:
			if int(entry.get("camp", 0)) == camp_order:
				for v in int(entry.get("votes", 0)):
					seats.append(camp_order)
	var size := _hemicycle.size
	var key := Vector3(size.x, size.y, seats.size())
	if key != _hemicycle_key:
		_hemicycle_key = key
		var radius := minf(size.x * 0.5 - 6.0, size.y - 10.0)
		_hemicycle_layout = ElectionTally.hemicycle(seats.size(), Vector2(size.x * 0.5, size.y - 4.0), radius)
	var points: PackedVector2Array = _hemicycle_layout.get("points", PackedVector2Array())
	var dot := float(_hemicycle_layout.get("dot_size", 4.0)) * 0.5
	for k in mini(points.size(), seats.size()):
		var camp := seats[k]
		_hemicycle.draw_circle(points[k], dot, CAMP_COLORS[0 if camp > 0 else 1 if camp < 0 else 2])


# --- Reiter „Deutsche Frage“ -------------------------------------------------


func _fill_question() -> void:
	for child in _question_grid.get_children():
		_question_grid.remove_child(child)
		child.queue_free()
	for child in _question_lists.get_children():
		_question_lists.remove_child(child)
		child.queue_free()
	var q := _sim.get_german_question()
	if q.is_empty():
		_question_line("Lage", "ohne Deutschen Bund keine deutsche Frage")
		return
	_question_line(
		"Bevölkerung des Bundes", "%s Einwohner" % GermanFormat.integer(int(q.get("federal_population", 0)))
	)
	if not str(q.get("customs_union_name", "")).is_empty():
		_question_line(
			"Im %s" % str(q.get("customs_union_name", "")),
			GermanFormat.percent(float(q.get("customs_union_percent", 0.0)), 1)
		)
	_question_line("Lager Preußen", "%s (%s Staaten)" % [
		GermanFormat.percent(float(q.get("prussia_percent", 0.0)), 1), int(q.get("prussia_states", 0))
	])
	_question_line("Lager Österreich", "%s (%s Staaten)" % [
		GermanFormat.percent(float(q.get("austria_percent", 0.0)), 1), int(q.get("austria_states", 0))
	])
	_question_line("Unentschieden", "%s (%s Staaten)" % [
		GermanFormat.percent(float(q.get("undecided_percent", 0.0)), 1), int(q.get("undecided_states", 0))
	])
	_question_line("Bundesreform", "%s Anträge, %s angenommen" % [
		GermanFormat.integer(int(q.get("reform_attempts", 0))), GermanFormat.integer(int(q.get("reform_accepted", 0)))
	])
	_question_line("Führung im Bund", "Preußen %s, Österreich %s, Kompromisse %s" % [
		GermanFormat.integer(int(q.get("leadership_prussia", 0))),
		GermanFormat.integer(int(q.get("leadership_austria", 0))),
		GermanFormat.integer(int(q.get("leadership_compromises", 0))),
	])
	var caption := _label(&"CaptionLabel", false)
	caption.text = "BÜNDNISSE UND GARANTIEN"
	_question_lists.add_child(caption)
	var treaties: Array = q.get("treaties", [])
	for t: Dictionary in treaties:
		var label := _label(&"MutedLabel", true)
		label.text = (
			"%s und %s: Bündnis seit %s" if bool(t.get("alliance", false)) else "%s garantiert %s seit %s"
		) % [str(t.get("from_name", "")), str(t.get("to_name", "")), str(t.get("since_text", ""))]
		_question_lists.add_child(label)
	if treaties.is_empty():
		var none := _label(&"MutedLabel", true)
		none.text = "Keine Verträge unter den Bundesstaaten (außer dem Bund selbst)."
		_question_lists.add_child(none)
	var plays: Array = q.get("play_results", [])
	if not plays.is_empty():
		var caption_plays := _label(&"CaptionLabel", false)
		caption_plays.text = "BEENDETE KRISEN"
		_question_lists.add_child(caption_plays)
		for p: Dictionary in plays:
			var label := _label(&"MutedLabel", true)
			label.text = "%s · %s gegen %s: %s – %s" % [
				str(p.get("date_text", "")), str(p.get("demander_name", "")), str(p.get("target_name", "")),
				str(p.get("text", "")), str(p.get("outcome_text", "")),
			]
			_question_lists.add_child(label)


func _question_line(caption: String, value: String) -> void:
	var left := _label(&"MutedLabel", false)
	left.text = caption
	var right := Label.new()
	right.text = value
	right.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_question_grid.add_child(left)
	_question_grid.add_child(right)
