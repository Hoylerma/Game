class_name YearbookWindow
extends PanelContainer
## Statistisches Jahrbuch (seit M13, Schritt a5a; vertical_slice.md 4.2 und 4.3): eine Kennzahl oder
## Dimension des Vermächtnisses je Stichtag als Liniendiagramm und Tabelle, für den Spielerstaat und bis zu
## drei Staaten zum Vergleich. Die Werte rechnet der Kern (Jahresreihen im Zustand, get_yearbook); das
## Fenster zeigt sie nur. main.gd öffnet und schließt es über die Fensterverwaltung.

signal close_requested

const WIDTH := 600.0
const CHART_HEIGHT := 190.0
const CONTENT_HEIGHT := 200.0
const MIN_CONTENT_HEIGHT := 80.0
const COMPARE_SLOTS := 3
## Nachkommastellen je Spalte (Kennzahlen des Kerns, dann die Dimensionen mit 0).
const DECIMALS: PackedInt32Array = [2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 0, 0]

var _sim: SimulationClient = null
var _player := -1
var _column := 0
var _compare: PackedInt32Array = [-1, -1, -1]
## Je gezeigtem Staat: { index, name, color, rows }.
var _series: Array[Dictionary] = []
var _columns := PackedStringArray()
var _measure: OptionButton = null
var _compare_buttons: Array[OptionButton] = []
var _chart: Control = null
var _scroll: ScrollContainer = null
var _table: GridContainer = null
var _empty: Label = null
var _range: Label = null
## Staaten der Vergleichsauswahl in der Reihenfolge der Einträge (Index des Staates).
var _choices := PackedInt32Array()


func _ready() -> void:
	name = "YearbookWindow"
	custom_minimum_size = Vector2(WIDTH, 0)
	visible = false
	mouse_filter = Control.MOUSE_FILTER_STOP
	_build()


func open(sim: SimulationClient, player: int) -> void:
	_sim = sim
	if player != _player:
		_compare = PackedInt32Array([-1, -1, -1])
	_player = player
	visible = true
	refresh()


func close() -> void:
	visible = false


func is_open() -> bool:
	return visible


func fit_height(max_height: float) -> void:
	_scroll.custom_minimum_size.y = CONTENT_HEIGHT
	var overflow := get_combined_minimum_size().y - max_height
	if overflow > 0.0:
		_scroll.custom_minimum_size.y = maxf(MIN_CONTENT_HEIGHT, CONTENT_HEIGHT - overflow)


## Spalte wählen (Index in columns, Tests).
func select_column(column: int) -> void:
	_column = clampi(column, 0, maxi(_columns.size() - 1, 0))
	_measure.select(_column)
	_fill()


## Vergleichsstaat in Platz slot (−1 = keiner, Tests).
func set_compare(slot: int, state_index: int) -> void:
	if slot < 0 or slot >= COMPARE_SLOTS:
		return
	_compare[slot] = state_index
	refresh()


func column_names() -> PackedStringArray:
	return _columns


## Tabelle als Text, zeilenweise mit „|“ getrennt (Tests).
func table_text() -> String:
	var lines := PackedStringArray()
	var cells := PackedStringArray()
	for i in _table.get_child_count():
		cells.append((_table.get_child(i) as Label).text)
		if cells.size() == _table.columns:
			lines.append(" | ".join(cells))
			cells = PackedStringArray()
	return "\n".join(lines)


## Gezeigte Staaten (Tests).
func shown_states() -> PackedInt32Array:
	var result := PackedInt32Array()
	for s in _series:
		result.append(int(s.get("index", -1)))
	return result


func refresh() -> void:
	if not visible or _sim == null:
		return
	_series.clear()
	var states := PackedInt32Array([_player])
	for c in _compare:
		if c >= 0 and not states.has(c):
			states.append(c)
	for s in states:
		var yearbook := _sim.get_yearbook(s)
		var state := _sim.get_state(s)
		if s == _player:
			_columns = yearbook.get("columns", PackedStringArray())
		_series.append({
			"index": s, "name": str(state.get("short_name", state.get("name", ""))),
			"color": state.get("color", UiPalette.INK), "rows": yearbook.get("rows", []),
		})
	_fill_measures()
	_fill_compare()
	_fill()


# --- Aufbau ------------------------------------------------------------------


func _build() -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 6)
	add_child(box)
	var head := HBoxContainer.new()
	var header := Label.new()
	header.text = "STATISTISCHES JAHRBUCH"
	header.theme_type_variation = &"HeaderLabel"
	header.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	head.add_child(header)
	var close_button := Button.new()
	close_button.text = "Schließen"
	close_button.focus_mode = Control.FOCUS_NONE
	close_button.pressed.connect(func() -> void: close_requested.emit())
	head.add_child(close_button)
	box.add_child(head)

	var controls := HBoxContainer.new()
	controls.add_theme_constant_override(&"separation", 6)
	_measure = OptionButton.new()
	_measure.name = "Measure"
	_measure.focus_mode = Control.FOCUS_NONE
	_measure.item_selected.connect(
		func(i: int) -> void:
			_column = i
			_fill()
	)
	controls.add_child(_measure)
	var versus := Label.new()
	versus.text = "Vergleich:"
	versus.theme_type_variation = &"MutedLabel"
	controls.add_child(versus)
	for slot in COMPARE_SLOTS:
		var b := OptionButton.new()
		b.name = "Compare%d" % slot
		b.focus_mode = Control.FOCUS_NONE
		b.fit_to_longest_item = false
		b.custom_minimum_size = Vector2(96, 0)
		b.item_selected.connect(
			func(i: int) -> void:
				_compare[slot] = _choices[i - 1] if i > 0 else -1
				refresh()
		)
		controls.add_child(b)
		_compare_buttons.append(b)
	box.add_child(controls)

	_empty = Label.new()
	_empty.text = "Das Jahrbuch füllt sich mit den Jahresreihen (Start, jeder 1. Januar)."
	_empty.theme_type_variation = &"MutedLabel"
	_empty.visible = false
	box.add_child(_empty)
	_chart = Control.new()
	_chart.name = "Chart"
	_chart.custom_minimum_size = Vector2(0, CHART_HEIGHT)
	_chart.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_chart.draw.connect(_draw_chart)
	box.add_child(_chart)
	_range = Label.new()
	_range.theme_type_variation = &"CaptionLabel"
	box.add_child(_range)
	_scroll = ScrollContainer.new()
	_scroll.custom_minimum_size = Vector2(0, CONTENT_HEIGHT)
	_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_table = GridContainer.new()
	_table.add_theme_constant_override(&"h_separation", 16)
	_scroll.add_child(_table)
	box.add_child(_scroll)


func _fill_measures() -> void:
	_measure.clear()
	for name_text in _columns:
		_measure.add_item(name_text)
	_column = clampi(_column, 0, maxi(_columns.size() - 1, 0))
	if not _columns.is_empty():
		_measure.select(_column)


func _fill_compare() -> void:
	_choices = PackedInt32Array()
	var names: Array[Array] = []
	for s in _sim.get_state_count():
		if s == _player:
			continue
		var state := _sim.get_state(s)
		if int(state.get("location_count", 0)) <= 0 and not _compare.has(s):
			continue
		names.append([str(state.get("short_name", state.get("name", ""))), s])
	names.sort_custom(func(a: Array, b: Array) -> bool: return str(a[0]) < str(b[0]))
	for entry in names:
		_choices.append(int(entry[1]))
	for slot in COMPARE_SLOTS:
		var b := _compare_buttons[slot]
		b.clear()
		b.add_item("–")
		var selected := 0
		for i in names.size():
			b.add_item(str(names[i][0]))
			if int(names[i][1]) == _compare[slot]:
				selected = i + 1
		b.select(selected)


func _values(series: Dictionary) -> PackedFloat64Array:
	var result := PackedFloat64Array()
	for row: Dictionary in series.get("rows", []):
		var values: PackedFloat64Array = row.get("values", PackedFloat64Array())
		result.append(values[_column] if _column < values.size() else 0.0)
	return result


func _years() -> PackedInt32Array:
	var result := PackedInt32Array()
	if _series.is_empty():
		return result
	for row: Dictionary in _series[0].get("rows", []):
		result.append(int(row.get("year", 0)))
	return result


func _decimals() -> int:
	return DECIMALS[_column] if _column < DECIMALS.size() else 0


func _fill() -> void:
	for child in _table.get_children():
		_table.remove_child(child)
		child.queue_free()
	var years := _years()
	_empty.visible = years.is_empty()
	_table.columns = _series.size() + 1
	_add_cell("Stichtag", true)
	for s in _series:
		_add_cell(str(s.get("name", "")), true, s.get("color", UiPalette.INK))
	var rows: Array = _series[0].get("rows", []) if not _series.is_empty() else []
	for r in rows.size():
		_add_cell(str((rows[r] as Dictionary).get("date_text", "")))
		for s in _series:
			var values := _values(s)
			_add_cell(GermanFormat.number(values[r], _decimals()) if r < values.size() else "–")
	var low := INF
	var high := -INF
	for s in _series:
		for v in _values(s):
			low = minf(low, v)
			high = maxf(high, v)
	_range.text = (
		"%s: %s bis %s" % [
			_columns[_column] if _column < _columns.size() else "", GermanFormat.number(low, _decimals()),
			GermanFormat.number(high, _decimals()),
		]
		if low <= high else ""
	)
	_chart.queue_redraw()


func _add_cell(text: String, header: bool = false, color: Color = Color.TRANSPARENT) -> void:
	var label := Label.new()
	label.text = text
	if header:
		label.theme_type_variation = &"CaptionLabel"
	if color.a > 0.0:
		label.add_theme_color_override(&"font_color", color.darkened(0.25))
	var first_column := _table.get_child_count() % maxi(_table.columns, 1) == 0
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT if first_column else HORIZONTAL_ALIGNMENT_RIGHT
	_table.add_child(label)


func _draw_chart() -> void:
	var size := _chart.size
	_chart.draw_rect(Rect2(Vector2.ZERO, size), UiPalette.PAPER_INSET)
	var low := INF
	var high := -INF
	for s in _series:
		for v in _values(s):
			low = minf(low, v)
			high = maxf(high, v)
	if low > high:
		return
	# Die Null gehört in den Bereich, sonst wirken kleine Änderungen groß.
	low = minf(low, 0.0)
	high = maxf(high, 0.0)
	if is_equal_approx(low, high):
		high = low + 1.0
	var font := get_theme_default_font()
	var font_size := 11
	var label_width := 54.0
	var margin := Vector2(6, 8)
	var area := Rect2(
		Vector2(margin.x + label_width, margin.y), size - Vector2(margin.x * 2.0 + label_width, margin.y * 2.0 + 14.0)
	)
	var zero_y := area.position.y + area.size.y * (1.0 - (0.0 - low) / (high - low))
	_chart.draw_line(Vector2(area.position.x, zero_y), Vector2(area.end.x, zero_y), UiPalette.EDGE, 1.0)
	for v: float in [high, low]:
		var y := area.position.y + area.size.y * (1.0 - (v - low) / (high - low))
		_chart.draw_string(
			font, Vector2(margin.x, y + 4.0), GermanFormat.number(v, _decimals()), HORIZONTAL_ALIGNMENT_RIGHT,
			label_width - 6.0, font_size, UiPalette.INK_SOFT
		)
	var years := _years()
	if not years.is_empty():
		var baseline := area.end.y + 12.0
		_chart.draw_string(font, Vector2(area.position.x, baseline), str(years[0]), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, UiPalette.INK_SOFT)
		_chart.draw_string(
			font, Vector2(area.end.x - 40.0, baseline), str(years[years.size() - 1]), HORIZONTAL_ALIGNMENT_RIGHT, 40.0,
			font_size, UiPalette.INK_SOFT
		)
	for s in _series:
		var values := _values(s)
		if values.size() < 2:
			continue
		var points := PackedVector2Array()
		for i in values.size():
			var x := area.position.x + area.size.x * float(i) / float(values.size() - 1)
			var y := area.position.y + area.size.y * (1.0 - (values[i] - low) / (high - low))
			points.append(Vector2(x, y))
		var color: Color = s.get("color", UiPalette.INK)
		_chart.draw_polyline(points, color.darkened(0.15), 2.0 if int(s.get("index", -1)) == _player else 1.5, true)
