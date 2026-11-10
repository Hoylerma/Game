class_name EventWindow
extends PanelContainer
## Ereignisfenster (seit M11, Schritt b4; ereignisse.md Abschnitt 9): offene Ereignisse des Spielerstaates
## nacheinander mit Datum, Titel, Text, Quelle und den Optionen samt ihren Folgen. Der historische
## Kompass markiert die Option, die historisch gewählt wurde (abschaltbar). Wendepunkte tragen eine
## eigene Kopfzeile. main.gd hält die Zeit an, solange das Fenster offen ist. Seit M13b1 zeigt eine
## Vignette die Art des Ereignisses (Revolution, Krieg, Diplomatie, Wirtschaft, Hof).
##
## Wie die übrigen Fenster liest es nichts selbst: main.gd übergibt die Ereignisse aus
## SimulationClient.get_open_events und erhält die Wahl als Signal.

signal option_chosen(event_index: int, option_index: int)

const WIDTH := 580.0
## Breite eines Wendepunkts (seit M13a5d).
const TURNING_POINT_WIDTH := 760.0
const TEXT_HEIGHT := 190.0
const COMPASS_MARK := "◆"
const TURNING_POINT_COLOR := Color("8e2b1d")

## Historischer Kompass: markiert die historische Option (seit M11, abschaltbar).
var compass := true

var _events: Array[Dictionary] = []
var _header: Label = null
var _counter: Label = null
var _title: Label = null
var _meta: Label = null
var _text: Label = null
var _options: VBoxContainer = null
var _source: Label = null
var _compass_toggle: CheckBox = null
var _vignette: TextureRect = null # Bild nach Art des Ereignisses (seit M13b1)


func _ready() -> void:
	name = "EventWindow"
	custom_minimum_size = Vector2(WIDTH, 0)
	visible = false
	mouse_filter = Control.MOUSE_FILTER_STOP
	_build()
	EpochTheme.frame(self)


## Zeigt das erste der offenen Ereignisse; leer schließt das Fenster.
func show_events(events: Array[Dictionary]) -> void:
	_events = events
	if _events.is_empty():
		close()
		return
	visible = true
	_fill()


func close() -> void:
	visible = false
	_events = []


func is_open() -> bool:
	return visible


## Das angezeigte Ereignis (leer, wenn keins).
## Ob gerade ein Wendepunkt zu entscheiden ist (seit M13a5d).
func is_turning_point() -> bool:
	return visible and bool(current_event().get("turning_point", false))


func current_event() -> Dictionary:
	return _events[0] if not _events.is_empty() else {}


## Knopf einer Option des angezeigten Ereignisses oder null (für Tests).
func option_button(option_index: int) -> Button:
	return _options.find_child("Option%d" % option_index, true, false) as Button


## Text des Fensters, zeilenweise (für Tests).
func window_text() -> String:
	var lines := PackedStringArray([_header.text, _counter.text, _title.text, _meta.text, _text.text])
	for child in _options.get_children():
		for item in child.get_children():
			if item is Button:
				lines.append("[%s%s]" % [(item as Button).text, " gesperrt" if (item as Button).disabled else ""])
			elif item is Label:
				lines.append((item as Label).text)
	lines.append(_source.text)
	return "\n".join(lines)


## Seit M13a5d: der Kompass gehört zu den Einstellungen (main.gd speichert ihn).
signal compass_changed(on: bool)


func set_compass(on: bool) -> void:
	var changed := compass != on
	compass = on
	_compass_toggle.set_pressed_no_signal(on)
	if visible:
		_fill()
	if changed:
		compass_changed.emit(on)


# --- Aufbau ------------------------------------------------------------------


func _build() -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 6)
	add_child(box)
	var head := HBoxContainer.new()
	_header = Label.new()
	_header.name = "Header"
	_header.theme_type_variation = &"HeaderLabel"
	_header.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	head.add_child(_header)
	_counter = _label(&"MutedLabel", false)
	_counter.name = "Counter"
	head.add_child(_counter)
	box.add_child(head)
	# Vignette links neben Titel und Datum (seit M13b1, vertical_slice.md 5.1).
	var title_row := HBoxContainer.new()
	title_row.add_theme_constant_override(&"separation", 12)
	_vignette = TextureRect.new()
	_vignette.name = "Vignette"
	_vignette.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	_vignette.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	_vignette.custom_minimum_size = Vector2(72, 48)
	_vignette.mouse_filter = Control.MOUSE_FILTER_IGNORE
	title_row.add_child(_vignette)
	var titles := VBoxContainer.new()
	titles.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	titles.alignment = BoxContainer.ALIGNMENT_CENTER
	_title = _label(&"NameLabel", true)
	_title.name = "Title"
	titles.add_child(_title)
	_meta = _label(&"MutedLabel", true)
	_meta.name = "Meta"
	titles.add_child(_meta)
	title_row.add_child(titles)
	box.add_child(title_row)
	var scroll := ScrollContainer.new()
	scroll.custom_minimum_size = Vector2(0, TEXT_HEIGHT)
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_text = _label(&"", true)
	_text.name = "Text"
	_text.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	scroll.add_child(_text)
	box.add_child(scroll)
	box.add_child(HSeparator.new())
	_options = VBoxContainer.new()
	_options.name = "Options"
	_options.add_theme_constant_override(&"separation", 8)
	box.add_child(_options)
	box.add_child(HSeparator.new())
	var foot := HBoxContainer.new()
	_source = _label(&"CaptionLabel", true)
	_source.name = "Source"
	_source.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	foot.add_child(_source)
	_compass_toggle = CheckBox.new()
	_compass_toggle.name = "Compass"
	_compass_toggle.text = "Kompass"
	_compass_toggle.focus_mode = Control.FOCUS_NONE
	_compass_toggle.button_pressed = compass
	_compass_toggle.tooltip_text = "Historischer Kompass: %s markiert die historische Wahl" % COMPASS_MARK
	_compass_toggle.toggled.connect(set_compass)
	foot.add_child(_compass_toggle)
	box.add_child(foot)


func _fill() -> void:
	var event := current_event()
	var turning := bool(event.get("turning_point", false))
	_header.text = "WENDEPUNKT" if turning else "EREIGNIS"
	if turning:
		_header.add_theme_color_override(&"font_color", TURNING_POINT_COLOR)
	else:
		_header.remove_theme_color_override(&"font_color")
	# Seit M13a5d: Wendepunkte auf einem großen Bildschirm (breiter, größerer Titel; main.gd dunkelt ab).
	custom_minimum_size.x = TURNING_POINT_WIDTH if turning else WIDTH
	if turning:
		_title.add_theme_font_size_override(&"font_size", 28)
		_text.custom_minimum_size.x = TURNING_POINT_WIDTH - 40.0
		_title.custom_minimum_size.x = TURNING_POINT_WIDTH - 180.0
	else:
		_title.remove_theme_font_size_override(&"font_size")
		_text.custom_minimum_size.x = WIDTH - 40.0
		_title.custom_minimum_size.x = WIDTH - 140.0
	_counter.text = "1 von %d" % _events.size() if _events.size() > 1 else ""
	_vignette.texture = EpochTheme.vignette(str(event.get("vignette", "")))
	_vignette.visible = _vignette.texture != null
	_vignette.custom_minimum_size = Vector2(108, 72) if turning else Vector2(72, 48)
	_title.text = str(event.get("title", ""))
	var meta := PackedStringArray()
	var date := str(event.get("date_text", ""))
	if not date.is_empty():
		meta.append(date)
	var state := str(event.get("state_name", ""))
	if not state.is_empty():
		meta.append(state)
	meta.append(str(event.get("kind_text", "")))
	_meta.text = " · ".join(meta)
	_text.text = str(event.get("text", ""))
	var source := str(event.get("source", ""))
	_source.text = "Quelle: %s" % source if not source.is_empty() else ""
	for child in _options.get_children():
		_options.remove_child(child)
		child.queue_free()
	for entry: Variant in event.get("options", []):
		var option := entry as Dictionary
		var index := int(option.get("index", 0))
		var row := VBoxContainer.new()
		row.add_theme_constant_override(&"separation", 2)
		var button := Button.new()
		button.name = "Option%d" % index
		var marked := compass and bool(option.get("historical", false))
		button.text = "%s %s" % [COMPASS_MARK, option.get("text", "")] if marked else str(option.get("text", ""))
		button.alignment = HORIZONTAL_ALIGNMENT_LEFT
		button.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		button.focus_mode = Control.FOCUS_NONE
		button.disabled = not bool(option.get("available", true))
		button.tooltip_text = (
			"Die historische Wahl" if marked
			else ("Zurzeit nicht möglich" if button.disabled else "")
		)
		button.pressed.connect(_on_option.bind(int(event.get("event_index", -1)), index))
		row.add_child(button)
		var effects: PackedStringArray = option.get("effects", PackedStringArray())
		var consequences := _label(&"MutedLabel", true)
		consequences.text = " · ".join(effects) if not effects.is_empty() else "ohne unmittelbare Folgen"
		row.add_child(consequences)
		_options.add_child(row)


func _on_option(event_index: int, option_index: int) -> void:
	option_chosen.emit(event_index, option_index)


func _label(variation: StringName, wrap: bool) -> Label:
	var label := Label.new()
	if variation != &"":
		label.theme_type_variation = variation
	if wrap:
		label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	return label
