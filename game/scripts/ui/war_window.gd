class_name WarWindow
extends PanelContainer
## Kriegsfenster (seit M10, Schritt b3; militaer.md Abschnitte 5 und 8) mit zwei Reitern: die
## laufenden Kriege mit Seiten, Zielen, Kriegspunkten, Kriegsmüdigkeit und Verlusten, für die
## Hauptpartei Friedensangebote mit Vorab-Prüfung und weitere Kriegsziele; die eigenen Heere mit
## Stärke, Moral und Lage (ein Klick wählt das Heer auf der Karte).
##
## Wie das Fenster „Diplomatie“ liest es über SimulationClient und meldet Befehle als Signale; Zeilen
## und Knöpfe werden einmal angelegt und danach nur aktualisiert (Review M8).

signal peace_requested(war_id: int, terms: int)
signal goal_requested(war_id: int, code: int)
signal army_selected(army_id: int)
signal close_requested

const WIDTH := 600.0
const LIST_HEIGHT := 110.0
## Höhe einer Zeile der Kriegsliste (FlatButton mit Abstand).
const ROW_HEIGHT := 31.0
const ARMY_LIST_HEIGHT := 300.0
const REFRESH_MS := 1000
## Friedensangebote: Art (SimulationClient.PEACE_*), Text des Knopfes.
const PEACE_TERMS: Array = [
	[SimulationClient.PEACE_WHITE, "Weißer Frieden"],
	[SimulationClient.PEACE_OWN_GOALS, "Unsere Ziele fordern"],
	[SimulationClient.PEACE_CAPITULATION, "Kapitulation anbieten"],
]
const SIDE_COLORS: Array[Color] = [Color("8e2b1d"), Color("20304f")]

## Name eines Staates (Kurzname) und eines Ortes, von main.gd gesetzt.
var state_name: Callable = func(index: int) -> String: return str(index)
var location_name: Callable = func(index: int) -> String: return str(index)

var _sim: SimulationClient = null
var _player := -1
var _selected := -1
var _filled_at := 0
var _wars: Array[Dictionary] = []
var _tab_buttons: Array[Button] = []
var _tab_pages: Array[Control] = []
var _current_tab := 0
var _summary: Label = null
var _war_rows: VBoxContainer = null
var _war_scroll: ScrollContainer = null
var _war_title: Label = null
var _war_sides: GridContainer = null
var _war_facts: Label = null
var _score_bars: Array[ProgressBar] = []
var _peace_box: VBoxContainer = null
var _peace_buttons: Array[Button] = []
var _goal_box: HBoxContainer = null
var _goal_choice: OptionButton = null
var _goal_button: Button = null
## Stand der Zielauswahl (_fill_goal_options); leer: neu füllen.
var _goal_key := ""
var _note: Label = null
## Begründung einer Kriegserklärung der KI (seit M12, ki.md 9): main.gd liefert sie über reasons_source
## (Staat, Ziel, SimulationClient.AI_* → Gründe oder "").
var reasons_source: Callable = Callable()
var _army_summary: Label = null
var _army_rows: VBoxContainer = null


func _ready() -> void:
	name = "WarWindow"
	custom_minimum_size = Vector2(WIDTH, 0)
	visible = false
	mouse_filter = Control.MOUSE_FILTER_STOP
	_build()


## Öffnet das Fenster für den Spielerstaat; tab 0 Kriege, 1 Heere.
func open(sim: SimulationClient, player: int, tab: int = 0) -> void:
	_sim = sim
	if player != _player:
		_selected = -1
	_player = player
	_note.text = ""
	_goal_key = ""
	visible = true
	show_tab(tab)
	refresh(true)


func close() -> void:
	visible = false


func is_open() -> bool:
	return visible


func refresh(force: bool = false) -> void:
	if not visible or _sim == null:
		return
	if not force and Time.get_ticks_msec() - _filled_at < REFRESH_MS:
		return
	_filled_at = Time.get_ticks_msec()
	_fill_wars()
	_fill_armies()


func show_tab(tab: int) -> void:
	_current_tab = clampi(tab, 0, _tab_pages.size() - 1)
	for i in _tab_pages.size():
		_tab_pages[i].visible = i == _current_tab
		_tab_buttons[i].set_pressed_no_signal(i == _current_tab)


func current_tab() -> int:
	return _current_tab


func select_war(war_id: int) -> void:
	_selected = war_id
	_fill_detail()


func selected_war() -> int:
	return _selected


func show_note(text: String) -> void:
	_note.text = text
	_filled_at = 0


## Text des Fensters, zeilenweise (für Tests).
func window_text() -> String:
	var lines := PackedStringArray([_summary.text, _war_title.text])
	for i in range(0, _war_sides.get_child_count() - 1, 2):
		lines.append("%s | %s" % [
			(_war_sides.get_child(i) as Label).text, (_war_sides.get_child(i + 1) as Label).text
		])
	lines.append(_war_facts.text)
	if _peace_box.visible:
		for button in _peace_buttons:
			lines.append("[%s%s]" % [button.text, " gesperrt" if button.disabled else ""])
	if _goal_box.visible:
		lines.append("Ziele zur Wahl: %d" % _goal_choice.item_count)
	lines.append(_note.text)
	lines.append(_army_summary.text)
	for child in _army_rows.get_children():
		if (child as Button).visible:
			lines.append((child as Button).text)
	return "\n".join(lines)


## Knopf eines Friedensangebots (PEACE_*), für Tests.
func peace_button(terms: int) -> Button:
	return _peace_buttons[terms]


## Knopf des Heeres in der Liste oder null (für Tests).
func army_button(army_id: int) -> Button:
	for child in _army_rows.get_children():
		if (child as Button).visible and int(child.get_meta(&"army", -1)) == army_id:
			return child as Button
	return null


# --- Aufbau ------------------------------------------------------------------


func _build() -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 6)
	add_child(box)
	var head := HBoxContainer.new()
	var header := Label.new()
	header.text = "KRIEG"
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
	var tabs := HBoxContainer.new()
	tabs.name = "Tabs"
	tabs.add_theme_constant_override(&"separation", 4)
	box.add_child(tabs)
	box.add_child(HSeparator.new())
	var group := ButtonGroup.new()
	group.allow_unpress = false
	for page: Control in [_build_wars_tab(), _build_armies_tab()]:
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


func _build_wars_tab() -> Control:
	var tab := VBoxContainer.new()
	tab.name = "Kriege"
	tab.add_theme_constant_override(&"separation", 6)
	_summary = _label(&"MutedLabel", true)
	tab.add_child(_summary)
	var scroll := ScrollContainer.new()
	scroll.custom_minimum_size = Vector2(0, LIST_HEIGHT)
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_war_scroll = scroll
	_war_rows = VBoxContainer.new()
	_war_rows.name = "WarRows"
	_war_rows.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_war_rows.add_theme_constant_override(&"separation", 1)
	scroll.add_child(_war_rows)
	tab.add_child(scroll)
	tab.add_child(HSeparator.new())
	_war_title = _label(&"NameLabel", true)
	tab.add_child(_war_title)
	_war_sides = GridContainer.new()
	_war_sides.name = "Sides"
	_war_sides.columns = 2
	_war_sides.add_theme_constant_override(&"h_separation", 16)
	_war_sides.add_theme_constant_override(&"v_separation", 2)
	tab.add_child(_war_sides)
	var bars := HBoxContainer.new()
	bars.add_theme_constant_override(&"separation", 16)
	for side in 2:
		var bar := ProgressBar.new()
		bar.name = "Score%d" % side
		bar.min_value = 0.0
		bar.max_value = 100.0
		bar.show_percentage = false
		bar.custom_minimum_size = Vector2(0, 8)
		bar.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		bar.tooltip_text = "Kriegspunkte gemessen an den Kosten der eigenen Ziele"
		var fill := StyleBoxFlat.new()
		fill.bg_color = SIDE_COLORS[side]
		bar.add_theme_stylebox_override(&"fill", fill)
		bars.add_child(bar)
		_score_bars.append(bar)
	tab.add_child(bars)
	_war_facts = _label(&"MutedLabel", true)
	tab.add_child(_war_facts)
	_peace_box = VBoxContainer.new()
	_peace_box.name = "Peace"
	_peace_box.add_theme_constant_override(&"separation", 4)
	var caption := _label(&"CaptionLabel", false)
	caption.text = "FRIEDENSANGEBOT"
	_peace_box.add_child(caption)
	for entry: Array in PEACE_TERMS:
		var button := Button.new()
		button.name = "Peace%d" % int(entry[0])
		button.focus_mode = Control.FOCUS_NONE
		button.alignment = HORIZONTAL_ALIGNMENT_LEFT
		button.clip_text = true
		button.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		button.pressed.connect(_on_peace.bind(int(entry[0])))
		_peace_box.add_child(button)
		_peace_buttons.append(button)
	tab.add_child(_peace_box)
	_goal_box = HBoxContainer.new()
	_goal_box.name = "Goals"
	_goal_box.add_theme_constant_override(&"separation", 6)
	_goal_choice = OptionButton.new()
	_goal_choice.name = "GoalChoice"
	_goal_choice.focus_mode = Control.FOCUS_NONE
	_goal_choice.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_goal_choice.clip_text = true
	_goal_choice.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	_goal_box.add_child(_goal_choice)
	_goal_button = Button.new()
	_goal_button.name = "AddGoal"
	_goal_button.text = "Kriegsziel stellen"
	_goal_button.focus_mode = Control.FOCUS_NONE
	_goal_button.tooltip_text = "Ein weiteres Ziel erhöht die Kosten: Frieden zu unseren Bedingungen wird teurer."
	_goal_button.pressed.connect(_on_goal)
	_goal_box.add_child(_goal_button)
	tab.add_child(_goal_box)
	_note = _label(&"MutedLabel", true)
	tab.add_child(_note)
	return tab


func _build_armies_tab() -> Control:
	var tab := VBoxContainer.new()
	tab.name = "Heere"
	tab.add_theme_constant_override(&"separation", 6)
	_army_summary = _label(&"MutedLabel", true)
	tab.add_child(_army_summary)
	var scroll := ScrollContainer.new()
	scroll.custom_minimum_size = Vector2(0, ARMY_LIST_HEIGHT)
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_army_rows = VBoxContainer.new()
	_army_rows.name = "ArmyRows"
	_army_rows.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_army_rows.add_theme_constant_override(&"separation", 1)
	scroll.add_child(_army_rows)
	tab.add_child(scroll)
	var note := _label(&"CaptionLabel", true)
	note.text = (
		"Ein Klick wählt das Heer auf der Karte; ein Rechtsklick auf einen Ort gibt dann den "
		+ "Marschbefehl. Heere des Generalstabs (G) handeln selbst."
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


# --- Reiter „Kriege“ ---------------------------------------------------------


func _fill_wars() -> void:
	_wars = _sim.get_wars()
	var own := 0
	for war in _wars:
		if int(war.get("player_side", 0)) != 0:
			own += 1
	if _wars.is_empty():
		_summary.text = "Kein Krieg. Eine Kriegserklärung geht über das Fenster „Diplomatie“ (Reiter Staaten)."
	else:
		_summary.text = "%s Krieg%s, %s mit uns." % [
			GermanFormat.integer(_wars.size()), "" if _wars.size() == 1 else "e", GermanFormat.integer(own)
		]
	while _war_rows.get_child_count() < _wars.size():
		var button := Button.new()
		button.theme_type_variation = &"FlatButton"
		button.toggle_mode = true
		button.focus_mode = Control.FOCUS_NONE
		button.alignment = HORIZONTAL_ALIGNMENT_LEFT
		button.clip_text = true
		button.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		button.pressed.connect(_on_war_row.bind(_war_rows.get_child_count()))
		_war_rows.add_child(button)
	for i in _war_rows.get_child_count():
		var button := _war_rows.get_child(i) as Button
		button.visible = i < _wars.size()
		if not button.visible:
			continue
		var war := _wars[i]
		button.name = "War%d" % int(war.get("id", -1))
		button.set_meta(&"war", int(war.get("id", -1)))
		var side := int(war.get("player_side", 0))
		button.text = "%s · seit %s%s" % [
			str(war.get("name", "")), str(war.get("started_text", "")),
			" · wir als Angreifer" if side > 0 else " · wir als Verteidiger" if side < 0 else "",
		]
	# Die Liste ist so hoch wie ihre Zeilen, höchstens LIST_HEIGHT.
	_war_scroll.visible = not _wars.is_empty()
	_war_scroll.custom_minimum_size.y = minf(LIST_HEIGHT, ROW_HEIGHT * _wars.size())
	if _war_of(_selected).is_empty():
		_selected = -1
		# Zuerst ein eigener Krieg.
		for war in _wars:
			if int(war.get("player_side", 0)) != 0:
				_selected = int(war.get("id", -1))
				break
		if _selected < 0 and not _wars.is_empty():
			_selected = int(_wars[0].get("id", -1))
	_fill_detail()


func _on_war_row(index: int) -> void:
	var button := _war_rows.get_child(index) as Button
	select_war(int(button.get_meta(&"war", -1)))


func _war_of(war_id: int) -> Dictionary:
	for war in _wars:
		if int(war.get("id", -1)) == war_id:
			return war
	return {}


func _names(indices: Variant) -> String:
	var names := PackedStringArray()
	for index: int in indices:
		names.append(str(state_name.call(index)))
	return ", ".join(names) if not names.is_empty() else "–"


func _fill_detail() -> void:
	for child in _war_rows.get_children():
		(child as Button).set_pressed_no_signal(int(child.get_meta(&"war", -2)) == _selected)
	for child in _war_sides.get_children():
		_war_sides.remove_child(child)
		child.queue_free()
	var war := _war_of(_selected)
	var has_war := not war.is_empty()
	for bar in _score_bars:
		bar.visible = has_war
	if not has_war:
		_war_title.text = "Kein Krieg gewählt"
		_war_facts.text = ""
		_peace_box.visible = false
		_goal_box.visible = false
		return
	var side := int(war.get("player_side", 0))
	_war_title.text = "%s (seit %s)" % [str(war.get("name", "")), str(war.get("started_text", ""))]
	var attackers: Array = war.get("attackers", [])
	var defenders: Array = war.get("defenders", [])
	var reasons := ""
	if reasons_source.is_valid() and not attackers.is_empty() and not defenders.is_empty():
		reasons = str(reasons_source.call(int(attackers[0]), int(defenders[0]), SimulationClient.AI_DECLARE_WAR))
	_war_title.mouse_filter = Control.MOUSE_FILTER_PASS
	_war_title.tooltip_text = "Gründe der Kriegserklärung: " + reasons if not reasons.is_empty() else ""
	_side_label("ANGREIFER" + (" (WIR)" if side > 0 else ""), true)
	_side_label("VERTEIDIGER" + (" (WIR)" if side < 0 else ""), true)
	_side_label(_names(war.get("attackers", [])), false)
	_side_label(_names(war.get("defenders", [])), false)
	# Ziele je Seite (Array statt PackedStringArray: append auf ein Element ginge an eine Kopie).
	var goals: Array = [[], []]
	for goal: Dictionary in war.get("goals", []):
		(goals[0 if bool(goal.get("attacker", false)) else 1] as Array).append(
			"%s (%s)" % [str(goal.get("text", "")), GermanFormat.integer(roundi(float(goal.get("cost", 0.0))))]
		)
	for k in 2:
		var texts := PackedStringArray(goals[k] as Array)
		_side_label("Ziele: " + ("; ".join(texts) if not texts.is_empty() else "keine"), false)
	for k in 2:
		var prefix := "attacker" if k == 0 else "defender"
		var score := float(war.get(prefix + "_score", 0.0))
		var cost := float(war.get(prefix + "_cost", 0.0))
		_side_label("Kriegspunkte %s von %s · Müdigkeit %s" % [
			GermanFormat.integer(roundi(score)), GermanFormat.integer(roundi(cost)),
			GermanFormat.integer(roundi(float(war.get(prefix + "_weariness", 0.0)))),
		], false)
		_score_bars[k].value = 100.0 * clampf(score / cost, 0.0, 1.0) if cost > 0.0 else (100.0 if score > 0.0 else 0.0)
	for k in 2:
		_side_label("Verluste %s" % GermanFormat.integer(int(war.get(("attacker" if k == 0 else "defender") + "_losses", 0))), false)
	_war_facts.text = (
		"Schlachten: %s. Eine Seite setzt ihre Ziele durch, wenn ihre Kriegspunkte deren Kosten erreichen "
		+ "oder die Müdigkeit des Gegners so hoch steigt; Müdigkeit wächst mit Verlusten, Besetzung und Dauer (bis 100)."
	) % GermanFormat.integer(int(war.get("battles", 0)))
	_peace_box.visible = side != 0
	var leads := bool(war.get("player_leads", false))
	if side != 0:
		for k in PEACE_TERMS.size():
			var terms := int(PEACE_TERMS[k][0])
			var button := _peace_buttons[k]
			var preview := _sim.get_peace_preview(_player, _selected, terms) if leads else {}
			var valid := bool(preview.get("valid", false))
			button.disabled = not valid
			button.text = str(PEACE_TERMS[k][1])
			if valid:
				button.text += " – " + str(preview.get("text", ""))
			button.tooltip_text = (
				"Nur die Hauptpartei einer Seite schließt Frieden." if not leads
				else _peace_tooltip(terms) if valid else "Zur Zeit nicht möglich (keine Ziele?)."
			)
	_goal_box.visible = leads
	if leads:
		_fill_goal_options(war)


func _side_label(text: String, caption: bool) -> void:
	var label := Label.new()
	label.theme_type_variation = &"CaptionLabel" if caption else &"MutedLabel"
	label.text = text
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.custom_minimum_size = Vector2((WIDTH - 56.0) * 0.5, 0)
	label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_war_sides.add_child(label)


func _peace_tooltip(terms: int) -> String:
	match terms:
		SimulationClient.PEACE_WHITE:
			return "Frieden ohne Gewinne und Verluste; besetzte Orte gehen zurück."
		SimulationClient.PEACE_OWN_GOALS:
			return "Frieden zu unseren Zielen; der Gegner nimmt an, wenn unsere Punkte oder seine Müdigkeit die Kosten decken."
		SimulationClient.PEACE_CAPITULATION:
			return "Wir geben dem Gegner seine Ziele."
	return ""


## Weitere Kriegsziele zur Wahl. Neu gefüllt nur, wenn sich Krieg, seine Ziele oder der Monat (Kosten
## nach der Bevölkerung) ändern: Die Liste kostet bei vielen Zielen einige Millisekunden, und ein
## Neufüllen schlösse eine gerade offene Auswahl (Review M10).
func _fill_goal_options(war: Dictionary) -> void:
	var key := "%d:%d:%d:%d" % [
		_selected, (war.get("goals", []) as Array).size(), int(_sim.get_date().get("month", 0)), _player
	]
	if key == _goal_key:
		return
	_goal_key = key
	var options := _sim.get_war_goal_options(_player, _selected)
	var keep := _goal_choice.get_item_id(_goal_choice.selected) if _goal_choice.selected >= 0 else -1
	_goal_choice.clear()
	for option: Dictionary in options:
		var code := int(option.get("code", 0))
		_goal_choice.add_item(
			"%s (Kosten %s)" % [str(option.get("text", "")), GermanFormat.integer(roundi(float(option.get("cost", 0.0))))],
			code
		)
	if _goal_choice.item_count > 0:
		var index := _goal_choice.get_item_index(keep)
		_goal_choice.select(index if index >= 0 else 0)
	_goal_choice.disabled = _goal_choice.item_count == 0
	_goal_button.disabled = _goal_choice.item_count == 0
	if _goal_choice.item_count == 0:
		_goal_choice.text = "keine weiteren Ziele"


func _on_peace(terms: int) -> void:
	if _selected >= 0:
		peace_requested.emit(_selected, terms)


func _on_goal() -> void:
	if _selected >= 0 and _goal_choice.selected >= 0:
		goal_requested.emit(_selected, _goal_choice.get_item_id(_goal_choice.selected))


# --- Reiter „Heere“ ----------------------------------------------------------


func _fill_armies() -> void:
	var armies := _sim.get_armies(_player)
	var state := _sim.get_state(_player)
	if not bool(state.get("military", false)):
		_army_summary.text = "In diesem Szenario gibt es keine Heere."
	else:
		_army_summary.text = "%s Soldaten unter Waffen (Friedensstärke %s) · %s." % [
			GermanFormat.integer(int(state.get("soldiers_under_arms", 0))),
			GermanFormat.integer(int(state.get("peace_strength", 0))),
			str(state.get("mobilization_text", "")),
		]
	# Feldheere zuerst, dann Garnisonen, je nach Stärke.
	armies.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		var ga := bool(a.get("garrison", false))
		var gb := bool(b.get("garrison", false))
		if ga != gb:
			return not ga
		return int(a.get("soldiers", 0)) > int(b.get("soldiers", 0))
	)
	while _army_rows.get_child_count() < armies.size():
		var button := Button.new()
		button.theme_type_variation = &"FlatButton"
		button.focus_mode = Control.FOCUS_NONE
		button.alignment = HORIZONTAL_ALIGNMENT_LEFT
		button.clip_text = true
		button.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		button.pressed.connect(_on_army_row.bind(_army_rows.get_child_count()))
		_army_rows.add_child(button)
	for i in _army_rows.get_child_count():
		var button := _army_rows.get_child(i) as Button
		button.visible = i < armies.size()
		if not button.visible:
			continue
		var army := armies[i]
		button.set_meta(&"army", int(army.get("id", -1)))
		var where := str(location_name.call(int(army.get("location_index", -1))))
		var destination := int(army.get("destination_index", -1))
		if destination >= 0:
			where += " → " + str(location_name.call(destination))
		button.text = "%s%s · %s Mann · Moral %s · %s · %s%s" % [
			"(G) " if bool(army.get("staff", false)) else "",
			str(army.get("name", "")),
			GermanFormat.integer(int(army.get("soldiers", 0))),
			GermanFormat.percent(float(army.get("morale", 0.0)) * 100.0),
			str(army.get("status_text", "")),
			where,
			"" if bool(army.get("supplied", true)) else " · abgeschnitten",
		]


func _on_army_row(index: int) -> void:
	var button := _army_rows.get_child(index) as Button
	army_selected.emit(int(button.get_meta(&"army", -1)))
