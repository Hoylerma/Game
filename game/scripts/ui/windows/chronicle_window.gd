class_name ChronicleWindow
extends AreaWindow
## Chronik (seit M13a5d; vertical_slice.md 4.2): alle Einträge der Partie, neueste zuerst, mit Filter nach
## Staat (alle, eigener, Welt) und Art (alle, nur Wendepunkte) und einer Suche; ein Klick fährt den Ort an.
## Ersetzt das Chronikpanel der linken Spalte.

signal location_requested(location_index: int)

const CHRONICLE_WIDTH := 760.0
const LIMIT := 250
const SCOPES: PackedStringArray = ["alle Staaten", "eigener Staat", "Welt"]

var _scope_select: OptionButton = null
var _turning: CheckBox = null
var _search: LineEdit = null
var _count_seen := -1


func _window_title() -> String:
	return "CHRONIK"


func _window_name() -> String:
	return "ChronicleWindow"


func _ready() -> void:
	super._ready()
	custom_minimum_size = Vector2(CHRONICLE_WIDTH, 0)


func _build_head(box: VBoxContainer) -> void:
	var row := HBoxContainer.new()
	row.add_theme_constant_override(&"separation", 8)
	_scope_select = OptionButton.new()
	_scope_select.name = "Scope"
	_scope_select.focus_mode = Control.FOCUS_NONE
	for scope in SCOPES:
		_scope_select.add_item(scope)
	_scope_select.item_selected.connect(func(_i: int) -> void: _changed())
	row.add_child(_scope_select)
	_turning = CheckBox.new()
	_turning.name = "TurningOnly"
	_turning.text = "nur Wendepunkte"
	_turning.focus_mode = Control.FOCUS_NONE
	_turning.toggled.connect(func(_on: bool) -> void: _changed())
	row.add_child(_turning)
	_search = LineEdit.new()
	_search.name = "Search"
	_search.placeholder_text = "Suchen …"
	_search.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_search.text_changed.connect(func(_t: String) -> void: _changed())
	row.add_child(_search)
	box.add_child(row)


## Filter setzen (Tests): scope 0 alle, 1 eigener Staat, 2 Welt.
func set_filter(scope: int, turning_only: bool, query: String) -> void:
	_scope_select.select(scope)
	_turning.set_pressed_no_signal(turning_only)
	_search.text = query
	_changed()


func _changed() -> void:
	_count_seen = -1
	refresh(true)


## Neu nur, wenn die Chronik gewachsen ist (sie wird lang).
func refresh(force: bool = false) -> void:
	if not visible or _sim == null:
		return
	var count := _sim.get_chronicle_size()
	if not force and count == _count_seen:
		return
	_count_seen = count
	super.refresh(true)


func _fill() -> void:
	var entries := _sim.get_chronicle(0)
	var scope := _scope_select.selected
	var turning_only := _turning.button_pressed
	var needle := _search.text.strip_edges().to_lower()
	var shown := 0
	var matching := 0
	for i in range(entries.size() - 1, -1, -1):
		var entry := entries[i]
		var who := int(entry.get("state_index", -1))
		if (scope == 1 and who != _player) or (scope == 2 and who >= 0):
			continue
		var turning := bool(entry.get("turning_point", false))
		if turning_only and not turning:
			continue
		var text := str(entry.get("text", ""))
		if not needle.is_empty() and not text.to_lower().contains(needle):
			continue
		matching += 1
		if shown >= LIMIT:
			continue
		shown += 1
		var b := Button.new()
		b.flat = true
		b.alignment = HORIZONTAL_ALIGNMENT_LEFT
		b.focus_mode = Control.FOCUS_NONE
		b.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		b.custom_minimum_size = Vector2(CHRONICLE_WIDTH - 60.0, 0)
		b.text = "%s  %s%s" % [str(entry.get("date_text", "")), "WENDEPUNKT · " if turning else "", text]
		var color := UiPalette.ACCENT if turning else UiPalette.INK
		for key: StringName in [&"font_color", &"font_hover_color", &"font_pressed_color", &"font_focus_color"]:
			b.add_theme_color_override(key, color)
		var location := int(entry.get("location_index", -1))
		if location >= 0:
			b.pressed.connect(func() -> void: location_requested.emit(location))
			b.tooltip_text = "Ort anfahren"
		else:
			b.mouse_default_cursor_shape = Control.CURSOR_ARROW
		_content.add_child(b)
	_title.text = "CHRONIK (%s von %s Einträgen)" % [GermanFormat.integer(matching), GermanFormat.integer(entries.size())]
	if matching > shown:
		_paragraph("… %s weitere; der Filter grenzt ein." % GermanFormat.integer(matching - shown), true)
	if matching == 0:
		_paragraph("Keine Einträge zu diesem Filter.", true)


## Texte der gezeigten Einträge (Tests).
func entry_texts() -> PackedStringArray:
	var result := PackedStringArray()
	for child in _content.get_children():
		if child is Button:
			result.append((child as Button).text)
	return result
