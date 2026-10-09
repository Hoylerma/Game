class_name LegendPanel
extends PanelContainer
## Legende des aktiven Kartenmodus: Farbfelder oder Farbverlauf.

const SWATCH_SIZE := Vector2(16, 12)
## Ab dieser Zahl von Einträgen zweispaltig.
const TWO_COLUMNS_FROM := 7

## Inhalt der aktuellen Anzeige; gleicher Inhalt wird nicht neu aufgebaut.
var _shown: Array = []

@onready var _title: Label = %Title
@onready var _body: VBoxContainer = %Body


## Farbfelder; entries: Dictionaries { "color": Color, "text": String, "detail": String }.
func show_swatches(title: String, entries: Array[Dictionary]) -> void:
	if not _begin(["swatches", title, entries]):
		return
	var grid := GridContainer.new()
	grid.columns = 2 if entries.size() >= TWO_COLUMNS_FROM else 1
	grid.add_theme_constant_override(&"h_separation", 14)
	grid.add_theme_constant_override(&"v_separation", 3)
	for entry in entries:
		var row := HBoxContainer.new()
		row.add_theme_constant_override(&"separation", 6)
		var swatch := ColorRect.new()
		swatch.color = entry.get("color", Color.GRAY)
		swatch.custom_minimum_size = SWATCH_SIZE
		swatch.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		row.add_child(swatch)
		var label := Label.new()
		label.text = str(entry.get("text", ""))
		label.theme_type_variation = &"MutedLabel"
		label.add_theme_color_override(&"font_color", UiPalette.INK)
		row.add_child(label)
		var detail := str(entry.get("detail", ""))
		if not detail.is_empty():
			row.tooltip_text = detail
			row.mouse_filter = Control.MOUSE_FILTER_PASS
		grid.add_child(row)
	_body.add_child(grid)


## Farbverlauf mit Beschriftungen (links, Mitte, rechts) und Einheit.
func show_gradient(
	title: String, gradient: Gradient, labels: PackedStringArray, unit: String
) -> void:
	if not _begin(["gradient", title, gradient.offsets, gradient.colors, labels, unit]):
		return
	var texture := GradientTexture2D.new()
	texture.gradient = gradient
	texture.width = 256
	texture.height = 1
	var bar := TextureRect.new()
	bar.texture = texture
	bar.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	bar.stretch_mode = TextureRect.STRETCH_SCALE
	bar.custom_minimum_size = Vector2(240, 14)
	var frame := PanelContainer.new()
	frame.add_theme_stylebox_override(&"panel", _frame_style())
	frame.add_child(bar)
	_body.add_child(frame)
	var scale_row := HBoxContainer.new()
	for i in labels.size():
		var label := Label.new()
		label.text = labels[i]
		label.theme_type_variation = &"CaptionLabel"
		label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		if i == labels.size() - 1 and labels.size() > 1:
			label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		elif i > 0:
			label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
		scale_row.add_child(label)
	_body.add_child(scale_row)
	if not unit.is_empty():
		var unit_label := Label.new()
		unit_label.text = unit
		unit_label.theme_type_variation = &"MutedLabel"
		_body.add_child(unit_label)


## Nur ein Hinweistext (z. B. ohne Simulation).
func show_message(title: String, text: String) -> void:
	if not _begin(["message", title, text]):
		return
	var label := Label.new()
	label.text = text
	label.theme_type_variation = &"MutedLabel"
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.custom_minimum_size = Vector2(240, 0)
	_body.add_child(label)


## Leert die Legende für neuen Inhalt; false, wenn genau dieser Inhalt schon angezeigt wird.
func _begin(content: Array) -> bool:
	if content == _shown:
		return false
	_shown = content
	_title.text = str(content[1])
	for child in _body.get_children():
		_body.remove_child(child)
		child.queue_free()
	return true


static func _frame_style() -> StyleBoxFlat:
	var style := StyleBoxFlat.new()
	style.bg_color = UiPalette.PAPER
	style.border_color = UiPalette.INK_SOFT
	style.set_border_width_all(1)
	style.content_margin_left = 1
	style.content_margin_right = 1
	style.content_margin_top = 1
	style.content_margin_bottom = 1
	return style
