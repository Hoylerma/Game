class_name LegendPanel
extends PanelContainer
## Legende des aktiven Kartenmodus: Farbfelder oder Farbverlauf.

const SWATCH_SIZE := Vector2(16, 12)
## Ab dieser Zahl von Einträgen zweispaltig.
const TWO_COLUMNS_FROM := 7
## Schraffur „keine Daten“ wie im Kartenshader (Grund, Linie).
const HATCH_BASE := Color(0.910, 0.882, 0.812)
const HATCH_LINE := Color(0.42, 0.35, 0.26)

static var _hatch_texture: ImageTexture = null

## Inhalt der aktuellen Anzeige; gleicher Inhalt wird nicht neu aufgebaut.
var _shown: Array = []

@onready var _title: Label = %Title
@onready var _body: VBoxContainer = %Body


## Farbfelder; entries: Dictionaries { "color": Color, "text": String, "detail": String,
## optional "hatch": true für die Schraffur „keine Daten“ }.
func show_swatches(title: String, entries: Array[Dictionary]) -> void:
	if not _begin(["swatches", title, entries]):
		return
	_add_swatch_grid(entries)


func _add_swatch_grid(entries: Array[Dictionary]) -> void:
	var grid := GridContainer.new()
	grid.columns = 2 if entries.size() >= TWO_COLUMNS_FROM else 1
	grid.add_theme_constant_override(&"h_separation", 14)
	grid.add_theme_constant_override(&"v_separation", 3)
	for entry in entries:
		var row := HBoxContainer.new()
		row.add_theme_constant_override(&"separation", 6)
		var swatch: Control = null
		if bool(entry.get("hatch", false)):
			swatch = hatch_swatch()
		else:
			var plain := ColorRect.new()
			plain.color = entry.get("color", Color.GRAY)
			swatch = plain
		swatch.custom_minimum_size = SWATCH_SIZE
		swatch.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		row.add_child(swatch)
		var label := Label.new()
		label.text = str(entry.get("text", ""))
		label.theme_type_variation = &"MutedLabel"
		label.add_theme_color_override(&"font_color", UiPalette.INK)
		# Lange Namen (1848: „Sachsen-Weimar-Eisenach“) kürzen statt die Spalte zu verbreitern;
		# der volle Name steht im Tooltip (seit M7).
		label.custom_minimum_size = Vector2(1, 0)
		label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		row.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		row.add_child(label)
		var detail := str(entry.get("detail", ""))
		if not detail.is_empty():
			row.tooltip_text = detail
			row.mouse_filter = Control.MOUSE_FILTER_PASS
		grid.add_child(row)
	_body.add_child(grid)


## Farbverlauf mit Beschriftungen (links, Mitte, rechts) und Einheit; extra sind
## zusätzliche Farbfelder darunter (z. B. „Keine Daten“, wie bei show_swatches).
func show_gradient(
	title: String,
	gradient: Gradient,
	labels: PackedStringArray,
	unit: String,
	extra: Array[Dictionary] = []
) -> void:
	if not _begin(["gradient", title, gradient.offsets, gradient.colors, labels, unit, extra]):
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
	if not extra.is_empty():
		_add_swatch_grid(extra)


## Farbreihe ohne Einzelnamen (z. B. Kreise) mit Erklärung darunter.
func show_palette(title: String, colors: PackedColorArray, text: String) -> void:
	if not _begin(["palette", title, colors, text]):
		return
	var strip := HBoxContainer.new()
	strip.add_theme_constant_override(&"separation", 2)
	for color in colors:
		var swatch := ColorRect.new()
		swatch.color = color
		swatch.custom_minimum_size = SWATCH_SIZE
		strip.add_child(swatch)
	_body.add_child(strip)
	var label := Label.new()
	label.text = text
	label.theme_type_variation = &"MutedLabel"
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.custom_minimum_size = Vector2(240, 0)
	_body.add_child(label)


## Kleines Feld mit der Schraffur „keine Daten“.
static func hatch_swatch() -> TextureRect:
	if _hatch_texture == null:
		var image := Image.create_empty(int(SWATCH_SIZE.x), int(SWATCH_SIZE.y), false,
			Image.FORMAT_RGBA8)
		for y in image.get_height():
			for x in image.get_width():
				var on_line := (x + y) % 5 == 0
				image.set_pixel(x, y, HATCH_BASE.lerp(HATCH_LINE, 0.5 if on_line else 0.0))
		_hatch_texture = ImageTexture.create_from_image(image)
	var rect := TextureRect.new()
	rect.texture = _hatch_texture
	rect.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	rect.stretch_mode = TextureRect.STRETCH_SCALE
	return rect


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
