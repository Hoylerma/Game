class_name JournalWindow
extends PanelContainer
## Journalfenster (seit M11, Schritt b4; ereignisse.md Abschnitt 9): je Journal ein Reiter mit der
## laufenden Stufe (seit wann, Text, Ziele mit Fortschritt), dem Weg bisher und den möglichen
## Ausgängen. Ein beendetes Journal zeigt seinen Ausgang mit Wertung. Der letzte Reiter listet die
## bisherigen Ereignisse mit der gewählten Option (Spielmenü „Ereignisse“), Wendepunkte hervorgehoben.
##
## Wie die übrigen Fenster liest es über SimulationClient; main.gd öffnet und schließt es.

signal close_requested

const WIDTH := 560.0
const CONTENT_HEIGHT := 380.0
## Kürzester Inhalt, wenn das Fenster sonst nicht in die Höhe passt (fit_height).
const MIN_CONTENT_HEIGHT := 140.0
## Kürzester Abstand zwischen zwei Füllungen bei laufender Zeit.
const REFRESH_MS := 1000
const DONE_MARK := "✓"
const OPEN_MARK := "○"
## Wertungen der Ausgänge (Chronik-Skript `wertung`).
const RATINGS := {"gelungen": "gelungen", "teilweise": "teilweise gelungen", "gescheitert": "gescheitert"}
## Einträge im Reiter „Bisherige Ereignisse“ (neueste zuerst).
const LOG_LIMIT := 80
const LOG_TAB_TITLE := "Bisherige Ereignisse"

var _sim: SimulationClient = null
var _journals: Array[Dictionary] = []
var _log: Array[Dictionary] = []
var _current := 0
var _filled_at := 0
var _tabs: HBoxContainer = null
var _scroll: ScrollContainer = null
var _journal_page: VBoxContainer = null
var _log_page: VBoxContainer = null
var _title: Label = null
var _stage: Label = null
var _stage_text: Label = null
var _goals: VBoxContainer = null
var _history: Label = null
var _outcomes: VBoxContainer = null
var _empty: Label = null


func _ready() -> void:
	name = "JournalWindow"
	custom_minimum_size = Vector2(WIDTH, 0)
	visible = false
	mouse_filter = Control.MOUSE_FILTER_STOP
	_build()


func open(sim: SimulationClient, tab: int = -1) -> void:
	_sim = sim
	visible = true
	refresh(true)
	if tab >= 0:
		show_tab(tab)


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
	_journals = _sim.get_journals()
	_log = _sim.get_event_log(LOG_LIMIT)
	_current = clampi(_current, 0, _journals.size())
	_fill_tabs()
	_fill()


## Höchsthöhe des Fensters (main.gd): Der Inhalt wird kürzer, wenn es sonst nicht passt.
func fit_height(max_height: float) -> void:
	_scroll.custom_minimum_size.y = CONTENT_HEIGHT
	var overflow := get_combined_minimum_size().y - max_height
	if overflow > 0.0:
		_scroll.custom_minimum_size.y = maxf(MIN_CONTENT_HEIGHT, CONTENT_HEIGHT - overflow)


## Reiter zeigen: die Journale in ihrer Reihenfolge, danach die bisherigen Ereignisse.
func show_tab(tab: int) -> void:
	_current = clampi(tab, 0, _journals.size())
	_fill_tabs()
	_fill()


## Reiter „Bisherige Ereignisse“ zeigen.
func show_event_log() -> void:
	show_tab(_journals.size())


func is_event_log_shown() -> bool:
	return _current == _journals.size()


func current_tab() -> int:
	return _current


## Zahl der Reiter (Journale und „Bisherige Ereignisse“).
func tab_count() -> int:
	return _journals.size() + 1


## Text des Fensters, zeilenweise (für Tests).
func window_text() -> String:
	var lines := PackedStringArray()
	for button in _tabs.get_children():
		lines.append("[%s]" % (button as Button).text)
	lines.append_array([_empty.text, _title.text, _stage.text, _stage_text.text])
	for label in _goals.get_children():
		lines.append((label as Label).text)
	lines.append(_history.text)
	for label in _outcomes.get_children():
		lines.append((label as Label).text)
	if _log_page.visible:
		for label in _log_page.get_children():
			lines.append((label as Label).text)
	return "\n".join(lines)


# --- Aufbau ------------------------------------------------------------------


func _build() -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 6)
	add_child(box)
	var head := HBoxContainer.new()
	var header := Label.new()
	header.text = "JOURNALE"
	header.theme_type_variation = &"HeaderLabel"
	header.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	head.add_child(header)
	var close_button := Button.new()
	close_button.name = "Close"
	close_button.text = "×"
	close_button.theme_type_variation = &"FlatButton"
	close_button.focus_mode = Control.FOCUS_NONE
	close_button.tooltip_text = "Schließen (Escape)"
	close_button.pressed.connect(func() -> void: close_requested.emit())
	head.add_child(close_button)
	box.add_child(head)
	_tabs = HBoxContainer.new()
	_tabs.name = "Tabs"
	box.add_child(_tabs)
	_empty = _label(&"MutedLabel")
	box.add_child(_empty)
	_scroll = ScrollContainer.new()
	_scroll.custom_minimum_size = Vector2(0, CONTENT_HEIGHT)
	_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	box.add_child(_scroll)
	var pages := VBoxContainer.new()
	pages.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_scroll.add_child(pages)
	_log_page = VBoxContainer.new()
	_log_page.name = "EventLog"
	_log_page.add_theme_constant_override(&"separation", 4)
	pages.add_child(_log_page)
	var page := VBoxContainer.new()
	page.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	page.add_theme_constant_override(&"separation", 6)
	pages.add_child(page)
	_journal_page = page
	_title = _label(&"NameLabel")
	_title.name = "Title"
	page.add_child(_title)
	_stage = _label(&"MutedLabel")
	_stage.name = "Stage"
	page.add_child(_stage)
	_stage_text = _label(&"")
	page.add_child(_stage_text)
	page.add_child(_caption("ZIELE DER STUFE"))
	_goals = VBoxContainer.new()
	_goals.name = "Goals"
	page.add_child(_goals)
	page.add_child(_caption("WEG BISHER"))
	_history = _label(&"MutedLabel")
	_history.name = "History"
	page.add_child(_history)
	page.add_child(_caption("MÖGLICHE AUSGÄNGE"))
	_outcomes = VBoxContainer.new()
	_outcomes.name = "Outcomes"
	_outcomes.add_theme_constant_override(&"separation", 4)
	page.add_child(_outcomes)


func _fill_tabs() -> void:
	while _tabs.get_child_count() < _journals.size() + 1:
		var button := Button.new()
		button.toggle_mode = true
		button.focus_mode = Control.FOCUS_NONE
		button.pressed.connect(show_tab.bind(_tabs.get_child_count()))
		_tabs.add_child(button)
	for i in _tabs.get_child_count():
		var button := _tabs.get_child(i) as Button
		button.visible = i <= _journals.size()
		if button.visible:
			button.name = "Tab%d" % i
			button.text = str(_journals[i].get("title", "")) if i < _journals.size() else LOG_TAB_TITLE
			button.set_pressed_no_signal(i == _current)


func _fill() -> void:
	var log_shown := is_event_log_shown()
	_journal_page.visible = not log_shown
	_log_page.visible = log_shown
	_empty.visible = log_shown and _log.is_empty()
	_empty.text = "Noch kein Ereignis eingetreten." if _empty.visible else ""
	for child in _goals.get_children() + _outcomes.get_children() + _log_page.get_children():
		child.get_parent().remove_child(child)
		child.queue_free()
	if log_shown:
		_title.text = ""
		_stage.text = ""
		_stage_text.text = ""
		_history.text = ""
		_fill_log()
		return
	var journal := _journals[_current]
	_title.text = str(journal.get("title", ""))
	var end_text := str(journal.get("end_text", ""))
	var started := bool(journal.get("started", false))
	var finished := bool(journal.get("finished", false))
	if not started:
		_stage.text = "Noch nicht begonnen."
		_stage_text.text = str(journal.get("text", ""))
	elif finished:
		_stage.text = "Ausgang seit %s: %s – %s" % [
			str(journal.get("since_text", "")),
			str(journal.get("stage_title", "")),
			_rating_text(str(journal.get("rating", ""))),
		]
		_stage_text.text = str(journal.get("stage_text", ""))
	else:
		_stage.text = "Stufe „%s“ seit %s%s" % [
			str(journal.get("stage_title", "")),
			str(journal.get("since_text", "")),
			" · entschieden bis %s" % end_text if not end_text.is_empty() else "",
		]
		_stage_text.text = str(journal.get("stage_text", ""))
	var goals: Array = journal.get("goals", [])
	for entry: Variant in goals:
		var goal := entry as Dictionary
		var label := _label(&"")
		var mark := DONE_MARK if bool(goal.get("done", false)) else OPEN_MARK
		label.text = "%s %s" % [mark, str(goal.get("text", ""))]
		if bool(goal.get("counting", false)):
			label.text += " (%d von %d)" % [int(goal.get("count", 0)), int(goal.get("target", 0))]
		_goals.add_child(label)
	if goals.is_empty():
		var none := _label(&"MutedLabel")
		none.text = "Keine Ziele." if started else "–"
		_goals.add_child(none)
	var steps := PackedStringArray()
	for entry: Variant in journal.get("history", []):
		var step := entry as Dictionary
		steps.append("%s: %s" % [str(step.get("date_text", "")), str(step.get("title", ""))])
	_history.text = "\n".join(steps) if not steps.is_empty() else "–"
	for entry: Variant in journal.get("outcomes", []):
		var outcome := entry as Dictionary
		var label := _label(&"")
		var reached := finished and str(outcome.get("id", "")) == str(journal.get("stage_id", ""))
		label.text = "%s%s (%s)%s" % [
			"► " if reached else "",
			str(outcome.get("title", "")),
			_rating_text(str(outcome.get("rating", ""))),
			" · ohne anderen Ausgang am Ende" if bool(outcome.get("fallback", false)) else "",
		]
		label.tooltip_text = str(outcome.get("text", ""))
		label.mouse_filter = Control.MOUSE_FILTER_PASS
		if not reached and finished:
			label.theme_type_variation = &"MutedLabel"
		_outcomes.add_child(label)


func _fill_log() -> void:
	for entry in _log:
		var label := _label(&"")
		var state := str(entry.get("state_name", ""))
		var option := str(entry.get("option_text", ""))
		label.text = "%s · %s%s%s%s" % [
			str(entry.get("date_text", "")),
			"%s: " % state if not state.is_empty() else "",
			str(entry.get("title", "")),
			" – %s" % option if not option.is_empty() else "",
			" %s" % EventWindow.COMPASS_MARK if bool(entry.get("historical", false)) else "",
		]
		if bool(entry.get("turning_point", false)):
			label.add_theme_color_override(&"font_color", EventWindow.TURNING_POINT_COLOR)
			label.tooltip_text = "Wendepunkt"
			label.mouse_filter = Control.MOUSE_FILTER_PASS
		_log_page.add_child(label)


static func _rating_text(rating: String) -> String:
	return str(RATINGS.get(rating, rating))


func _caption(text: String) -> Label:
	var label := _label(&"CaptionLabel")
	label.text = text
	return label


func _label(variation: StringName) -> Label:
	var label := Label.new()
	if variation != &"":
		label.theme_type_variation = variation
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	return label
