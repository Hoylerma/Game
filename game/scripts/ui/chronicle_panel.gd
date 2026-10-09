class_name ChroniclePanel
extends PanelContainer
## Chronik der Partie, neueste Einträge oben.
##
## Ein Klick auf einen Eintrag mit Ortsbezug wählt den Ort auf der Karte aus.

## Eintrag mit Ortsbezug angeklickt.
signal location_requested(sim_index: int)

## Höchstzahl angezeigter Einträge (ältere fallen unten heraus).
const MAX_VISIBLE_ENTRIES := 250

var _total := 0

@onready var _entries: VBoxContainer = %Entries
@onready var _placeholder: Label = %Placeholder
@onready var _count: Label = %Count
@onready var _scroll: ScrollContainer = %Scroll


func _ready() -> void:
	clear()


func clear() -> void:
	for child in _entries.get_children():
		_entries.remove_child(child)
		child.queue_free()
	_total = 0
	_update_header()


## Neue Einträge (in zeitlicher Reihenfolge) oben einfügen.
func add_entries(entries: Array[Dictionary]) -> void:
	for entry in entries:
		var item := _create_item(entry)
		_entries.add_child(item)
		_entries.move_child(item, 0)
		_total += 1
	while _entries.get_child_count() > MAX_VISIBLE_ENTRIES:
		var oldest := _entries.get_child(_entries.get_child_count() - 1)
		_entries.remove_child(oldest)
		oldest.queue_free()
	if not entries.is_empty():
		_scroll.scroll_vertical = 0
	_update_header()


func _update_header() -> void:
	_placeholder.visible = _total == 0
	if _total == 0:
		_count.text = ""
	elif _total == 1:
		_count.text = "1 Eintrag"
	else:
		_count.text = "%s Einträge" % GermanFormat.integer(_total)


func _create_item(entry: Dictionary) -> Control:
	var location := int(entry.get("location_index", -1))
	var item := PanelContainer.new()
	item.theme_type_variation = &"SectionPanel"
	item.mouse_filter = Control.MOUSE_FILTER_STOP
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 1)
	box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	item.add_child(box)
	var date := Label.new()
	date.theme_type_variation = &"CaptionLabel"
	date.text = str(entry.get("date_text", ""))
	date.mouse_filter = Control.MOUSE_FILTER_IGNORE
	box.add_child(date)
	var text := Label.new()
	text.text = str(entry.get("text", ""))
	text.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	text.custom_minimum_size = Vector2(1, 0)
	text.mouse_filter = Control.MOUSE_FILTER_IGNORE
	box.add_child(text)
	if location >= 0:
		item.tooltip_text = "Klicken, um den Ort auf der Karte zu zeigen"
		item.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
		item.gui_input.connect(_on_item_input.bind(location))
	return item


func _on_item_input(event: InputEvent, location: int) -> void:
	var button := event as InputEventMouseButton
	if button != null and button.pressed and button.button_index == MOUSE_BUTTON_LEFT:
		location_requested.emit(location)
		accept_event()
