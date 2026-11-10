class_name VerdictWindow
extends PanelContainer
## Urteil der Geschichte (seit M13, Schritt a4; vertical_slice.md 3.5): am Ende der Partie der Ausgang
## der Journale mit Wertung und historischem Vergleich, die fünf Dimensionen des Vermächtnisses mit
## ihrem Verlauf seit dem Start und einem Satz, die Wendepunkte, die Kriege und Herrscher und Minister
## im Amt. Knöpfe: Weiterspielen (ohne neue Ereignisse), Chronik exportieren (HTML), Hauptmenü.
## main.gd legt das Fenster an, öffnet es am Ende von selbst und aus dem Spielmenü.

signal continue_requested
signal menu_requested
signal export_requested

const WIDTH := 720.0
const CONTENT_HEIGHT := 520.0
## Wertungen der Ausgänge (Chronik-Skript `wertung`), wie im Journalfenster.
const RATINGS := {"gelungen": "gelungen", "teilweise": "teilweise gelungen", "gescheitert": "gescheitert"}
## Höchstens so viele Wendepunkte und Kriege.
const LIST_LIMIT := 14
const SPARK_SIZE := Vector2(140, 28)

var _sim: SimulationClient = null
var _player := -1
var _scroll: ScrollContainer = null
var _content: VBoxContainer = null
var _title: Label = null
var _subtitle: Label = null
var _export_note: Label = null
var _continue_button: Button = null


func _ready() -> void:
	name = "VerdictWindow"
	custom_minimum_size = Vector2(WIDTH, 0)
	visible = false
	mouse_filter = Control.MOUSE_FILTER_STOP
	_build()
	EpochTheme.frame(self)


func open(sim: SimulationClient, player: int) -> void:
	_sim = sim
	_player = player
	visible = true
	_export_note.text = ""
	_fill()


func close() -> void:
	visible = false


func is_open() -> bool:
	return visible


## Hinweis nach dem Export (Pfad oder Fehler).
func show_export_note(text: String) -> void:
	_export_note.text = text


## Inhalt (Tests): Texte aller Zeilen.
func content_texts() -> PackedStringArray:
	var texts := PackedStringArray()
	for child in _content.get_children():
		if child is Label:
			texts.append((child as Label).text)
	return texts


## Knöpfe (Tests): "Continue", "Export", "Menu".
func button(button_name: String) -> Button:
	return find_child(button_name, true, false) as Button


func _build() -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 6)
	add_child(box)
	var caption := Label.new()
	caption.text = "URTEIL DER GESCHICHTE"
	caption.theme_type_variation = &"CaptionLabel"
	box.add_child(caption)
	_title = Label.new()
	_title.theme_type_variation = &"HeaderLabel"
	_title.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_title.custom_minimum_size = Vector2(1, 0)
	box.add_child(_title)
	_subtitle = Label.new()
	_subtitle.theme_type_variation = &"MutedLabel"
	box.add_child(_subtitle)
	_scroll = ScrollContainer.new()
	_scroll.custom_minimum_size = Vector2(0, CONTENT_HEIGHT)
	_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	box.add_child(_scroll)
	_content = VBoxContainer.new()
	_content.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_content.add_theme_constant_override(&"separation", 4)
	_scroll.add_child(_content)
	_export_note = Label.new()
	_export_note.theme_type_variation = &"MutedLabel"
	_export_note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_export_note.custom_minimum_size = Vector2(1, 0)
	box.add_child(_export_note)
	var buttons := HBoxContainer.new()
	buttons.alignment = BoxContainer.ALIGNMENT_END
	buttons.add_theme_constant_override(&"separation", 8)
	box.add_child(buttons)
	var export_button := _make_button("Export", "Chronik exportieren", export_requested)
	buttons.add_child(export_button)
	buttons.add_child(_make_button("Menu", "Hauptmenü", menu_requested))
	_continue_button = _make_button("Continue", "Weiterspielen", continue_requested)
	buttons.add_child(_continue_button)


func _make_button(button_name: String, text: String, signal_to_emit: Signal) -> Button:
	var b := Button.new()
	b.name = button_name
	b.text = text
	b.focus_mode = Control.FOCUS_NONE
	b.pressed.connect(func() -> void: signal_to_emit.emit())
	return b


func _fill() -> void:
	for child in _content.get_children():
		_content.remove_child(child)
		child.queue_free()
	if _sim == null:
		return
	var verdict := _sim.get_verdict()
	var state := _sim.get_state(_player) if _player >= 0 else {}
	_title.text = str(state.get("name", _sim.get_scenario_name()))
	_subtitle.text = "%s · vom %s bis zum %s" % [
		_sim.get_scenario_name(), str(verdict.get("start_date_text", "")),
		str(verdict.get("end_date_text", _sim.get_date_text())),
	]
	_fill_journals()
	_fill_legacy()
	_fill_persons()
	_fill_list(
		"Wendepunkte", verdict.get("turning_points", []),
		func(e: Dictionary) -> String:
			var who := str(e.get("state_name", ""))
			return "%s – %s%s%s" % [
				str(e.get("date_text", "")), str(e.get("title", "")), (" (%s)" % who) if not who.is_empty() else "",
				(": " + str(e.get("option_text", ""))) if not str(e.get("option_text", "")).is_empty() else "",
			]
	)
	_fill_list(
		"Kriege", verdict.get("wars", []),
		func(e: Dictionary) -> String:
			var winner := str(e.get("winner_name", ""))
			return "%s gegen %s, %s bis %s: %s" % [
				str(e.get("attacker_name", "")), str(e.get("defender_name", "")), str(e.get("started_text", "")),
				str(e.get("ended_text", "")), ("Sieg " + winner) if not winner.is_empty() else "ohne Sieger",
			]
	)


func _heading(text: String) -> void:
	var label := Label.new()
	label.text = text
	label.theme_type_variation = &"HeaderLabel"
	_content.add_child(label)


func _paragraph(text: String, muted: bool = false) -> void:
	var label := Label.new()
	label.text = text
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.custom_minimum_size = Vector2(WIDTH - 40.0, 0)
	if muted:
		label.theme_type_variation = &"MutedLabel"
	_content.add_child(label)


func _fill_journals() -> void:
	for journal in _sim.get_journals():
		if not bool(journal.get("started", false)):
			continue
		_heading(str(journal.get("title", "")))
		if bool(journal.get("finished", false)):
			var rating := str(journal.get("rating", ""))
			_paragraph(
				"%s (%s, %s)" % [
					str(journal.get("stage_title", "")), RATINGS.get(rating, rating), str(journal.get("since_text", "")),
				]
			)
		else:
			_paragraph("offen: %s" % str(journal.get("stage_title", "")))
		var text := str(journal.get("stage_text", ""))
		if not text.is_empty():
			_paragraph(text, true)
		var historical := str(journal.get("historical", ""))
		if not historical.is_empty():
			_paragraph(historical, true)


func _fill_legacy() -> void:
	var legacy := _sim.get_legacy(_player)
	var dimensions: Array = legacy.get("dimensions", [])
	if dimensions.is_empty():
		return
	_heading("Vermächtnis")
	var yearbook := _sim.get_yearbook(_player)
	var first: int = int(yearbook.get("measure_count", 0))
	var rows: Array = yearbook.get("rows", [])
	for d in dimensions.size():
		var dimension: Dictionary = dimensions[d]
		var line := HBoxContainer.new()
		line.add_theme_constant_override(&"separation", 8)
		var label := Label.new()
		label.text = "%s %d – %s" % [
			str(dimension.get("name", "")), roundi(float(dimension.get("value", 0.0))),
			str(dimension.get("sentence", "")),
		]
		label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		label.custom_minimum_size = Vector2(1, 0)
		label.tooltip_text = _factor_text(dimension)
		label.mouse_filter = Control.MOUSE_FILTER_PASS
		line.add_child(label)
		var series := PackedFloat64Array()
		for row: Dictionary in rows:
			var values: PackedFloat64Array = row.get("values", PackedFloat64Array())
			if first + d < values.size():
				series.append(values[first + d])
		line.add_child(_spark(series))
		_content.add_child(line)


## Tooltip: Grundwert und die wirksamen Faktoren mit Beitrag.
static func _factor_text(dimension: Dictionary) -> String:
	var lines := PackedStringArray(["Grundwert %+d" % roundi(float(dimension.get("base", 0.0)))])
	for factor: Dictionary in dimension.get("factors", []):
		var value := float(factor.get("value", 0.0))
		if bool(factor.get("active", false)) and absf(value) >= 0.5:
			lines.append("%s %+d" % [str(factor.get("name", "")), roundi(value)])
	return "\n".join(lines)


## Verlauf einer Dimension (0–100) als Linie.
func _spark(series: PackedFloat64Array) -> Control:
	var spark := Control.new()
	spark.custom_minimum_size = SPARK_SIZE
	spark.mouse_filter = Control.MOUSE_FILTER_IGNORE
	spark.draw.connect(
		func() -> void:
			spark.draw_rect(Rect2(Vector2.ZERO, SPARK_SIZE), UiPalette.PAPER_INSET)
			if series.size() < 2:
				return
			var points := PackedVector2Array()
			for i in series.size():
				var x := SPARK_SIZE.x * float(i) / float(series.size() - 1)
				var y := SPARK_SIZE.y * (1.0 - clampf(series[i], 0.0, 100.0) / 100.0)
				points.append(Vector2(x, y))
			spark.draw_polyline(points, UiPalette.ACCENT, 1.5, true)
	)
	return spark


func _fill_persons() -> void:
	var persons := _sim.get_state_persons(_player) if _player >= 0 else ([] as Array[Dictionary])
	if persons.is_empty():
		return
	_heading("Herrscher und Minister")
	for person in persons:
		_paragraph(
			"%s: %s (seit %s)" % [
				str(person.get("title", "")), str(person.get("name", "")), str(person.get("since_text", "")),
			]
		)


func _fill_list(heading: String, entries: Array, line_of: Callable) -> void:
	if entries.is_empty():
		return
	_heading(heading)
	for i in mini(entries.size(), LIST_LIMIT):
		_paragraph(str(line_of.call(entries[i])))
	if entries.size() > LIST_LIMIT:
		_paragraph("… und %d weitere" % (entries.size() - LIST_LIMIT), true)
