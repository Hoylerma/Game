class_name AreaWindow
extends PanelContainer
## Gemeinsame Grundlage der Fenster eines Bereichs (seit M13, Schritt a5b; vertical_slice.md 4.2): Kopf mit
## Titel und „Schließen“, darunter ein rollbarer Inhalt, den die Unterklasse in _fill() aus SimulationClient
## füllt. open/close/is_open/fit_height passen zur Fensterverwaltung (WindowManager); bei laufender Zeit
## füllt refresh() höchstens einmal je Sekunde neu.

signal close_requested

const WIDTH := 580.0
const CONTENT_HEIGHT := 440.0
const MIN_CONTENT_HEIGHT := 120.0
const REFRESH_MS := 1000
const BAR_WIDTH := 140.0

var _sim: SimulationClient = null
var _player := -1
var _filled_at := 0
var _title: Label = null
var _scroll: ScrollContainer = null
var _content: VBoxContainer = null


## Titel in Großbuchstaben (Unterklasse).
func _window_title() -> String:
	return ""


## Knotenname (Unterklasse, für Tests).
func _window_name() -> String:
	return "AreaWindow"


## Inhalt füllen (Unterklasse); _sim und _player sind gesetzt.
func _fill() -> void:
	pass


func _ready() -> void:
	name = _window_name()
	custom_minimum_size = Vector2(WIDTH, 0)
	visible = false
	mouse_filter = Control.MOUSE_FILTER_STOP
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 6)
	add_child(box)
	var head := HBoxContainer.new()
	_title = Label.new()
	_title.text = _window_title()
	_title.theme_type_variation = &"HeaderLabel"
	_title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	head.add_child(_title)
	var close_button := Button.new()
	close_button.name = "Close"
	close_button.text = "Schließen"
	close_button.focus_mode = Control.FOCUS_NONE
	close_button.pressed.connect(func() -> void: close_requested.emit())
	head.add_child(close_button)
	box.add_child(head)
	_build_head(box)
	_scroll = ScrollContainer.new()
	_scroll.custom_minimum_size = Vector2(0, CONTENT_HEIGHT)
	_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_content = VBoxContainer.new()
	_content.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_content.add_theme_constant_override(&"separation", 3)
	_scroll.add_child(_content)
	box.add_child(_scroll)


## Bedienelemente über dem Inhalt (Unterklasse, z. B. eine Auswahl).
func _build_head(_box: VBoxContainer) -> void:
	pass


func open(sim: SimulationClient, player: int) -> void:
	_sim = sim
	_player = player
	visible = true
	refresh(true)


## Staat, den das Fenster zeigt.
func player_index() -> int:
	return _player


func close() -> void:
	visible = false


func is_open() -> bool:
	return visible


func fit_height(max_height: float) -> void:
	_scroll.custom_minimum_size.y = CONTENT_HEIGHT
	var overflow := get_combined_minimum_size().y - max_height
	if overflow > 0.0:
		_scroll.custom_minimum_size.y = maxf(MIN_CONTENT_HEIGHT, CONTENT_HEIGHT - overflow)


func refresh(force: bool = false) -> void:
	if not visible or _sim == null:
		return
	if not force and Time.get_ticks_msec() - _filled_at < REFRESH_MS:
		return
	_filled_at = Time.get_ticks_msec()
	for child in _content.get_children():
		_content.remove_child(child)
		child.queue_free()
	_fill()


## Alle Texte des Inhalts, zeilenweise (Tests).
func window_text() -> String:
	var lines := PackedStringArray()
	_collect(_content, lines)
	return "\n".join(lines)


func _collect(node: Node, lines: PackedStringArray) -> void:
	for child in node.get_children():
		if child is Label:
			lines.append((child as Label).text)
		elif child is HBoxContainer:
			var parts := PackedStringArray()
			for part in child.get_children():
				if part is Label:
					parts.append((part as Label).text)
				else:
					_collect(part, lines)
			if not parts.is_empty():
				lines.append(" ".join(parts))
		else:
			_collect(child, lines)


# --- Bausteine -----------------------------------------------------------------


func _heading(text: String) -> Label:
	var label := Label.new()
	label.text = text
	label.theme_type_variation = &"CaptionLabel"
	_content.add_child(label)
	return label


func _paragraph(text: String, muted: bool = false) -> Label:
	var label := Label.new()
	label.text = text
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.custom_minimum_size = Vector2(WIDTH - 48.0, 0)
	if muted:
		label.theme_type_variation = &"MutedLabel"
	_content.add_child(label)
	return label


## Zeile „Beschriftung … Wert“, Wert rechtsbündig.
func _line(caption: String, value: String, tooltip: String = "", color: Color = Color.TRANSPARENT) -> HBoxContainer:
	var row := HBoxContainer.new()
	row.add_theme_constant_override(&"separation", 8)
	var left := Label.new()
	left.text = caption
	left.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	left.clip_text = true
	left.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	row.add_child(left)
	var right := Label.new()
	right.text = value
	right.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	if color.a > 0.0:
		right.add_theme_color_override(&"font_color", color)
	row.add_child(right)
	if not tooltip.is_empty():
		row.tooltip_text = tooltip
		left.mouse_filter = Control.MOUSE_FILTER_PASS
		right.mouse_filter = Control.MOUSE_FILTER_PASS
		row.mouse_filter = Control.MOUSE_FILTER_PASS
	_content.add_child(row)
	return row


## Zeile mit Farbfeld, Name, Balken (Anteil 0 … 1) und Wert.
func _bar(caption: String, fraction: float, color: Color, value: String, tooltip: String = "") -> HBoxContainer:
	var row := HBoxContainer.new()
	row.add_theme_constant_override(&"separation", 8)
	var swatch := ColorRect.new()
	swatch.color = color
	swatch.custom_minimum_size = Vector2(10, 10)
	swatch.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	row.add_child(swatch)
	var left := Label.new()
	left.text = caption
	left.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	left.clip_text = true
	left.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	row.add_child(left)
	var bar := Control.new()
	bar.custom_minimum_size = Vector2(BAR_WIDTH, 10)
	bar.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	bar.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var share := clampf(fraction, 0.0, 1.0)
	bar.draw.connect(
		func() -> void:
			bar.draw_rect(Rect2(Vector2.ZERO, bar.size), UiPalette.PAPER_INSET)
			bar.draw_rect(Rect2(Vector2.ZERO, Vector2(bar.size.x * share, bar.size.y)), color)
	)
	row.add_child(bar)
	var right := Label.new()
	right.text = value
	right.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	right.custom_minimum_size = Vector2(70, 0)
	row.add_child(right)
	if not tooltip.is_empty():
		row.tooltip_text = tooltip
		row.mouse_filter = Control.MOUSE_FILTER_PASS
	_content.add_child(row)
	return row


## Zeile mit Balken von der Mitte aus für Werte mit Vorzeichen (−max_abs … +max_abs).
func _signed_bar(caption: String, value: float, max_abs: float, value_text: String, tooltip: String = "") -> HBoxContainer:
	var row := _bar(caption, 0.0, UiPalette.POSITIVE if value >= 0.0 else UiPalette.NEGATIVE, value_text, tooltip)
	var bar := row.get_child(2) as Control
	for connection in bar.draw.get_connections():
		bar.draw.disconnect(connection["callable"])
	var share := clampf(value / maxf(max_abs, 0.001), -1.0, 1.0)
	var color := UiPalette.POSITIVE if value >= 0.0 else UiPalette.NEGATIVE
	bar.draw.connect(
		func() -> void:
			var half := bar.size.x * 0.5
			bar.draw_rect(Rect2(Vector2.ZERO, bar.size), UiPalette.PAPER_INSET)
			var width := half * absf(share)
			var x := half if share >= 0.0 else half - width
			bar.draw_rect(Rect2(Vector2(x, 0), Vector2(width, bar.size.y)), color)
			bar.draw_line(Vector2(half, -1), Vector2(half, bar.size.y + 1), UiPalette.EDGE, 1.0)
	)
	return row


func _gap() -> void:
	var spacer := Control.new()
	spacer.custom_minimum_size = Vector2(0, 6)
	_content.add_child(spacer)


## Anteile (Name → Prozent) absteigend, höchstens limit Einträge, der Rest als „übrige“.
static func sorted_shares(shares: Dictionary, limit: int) -> Array[Array]:
	var entries: Array[Array] = []
	for key: Variant in shares:
		entries.append([str(key), float(shares[key])])
	entries.sort_custom(func(a: Array, b: Array) -> bool: return float(a[1]) > float(b[1]))
	if entries.size() > limit:
		var rest := 0.0
		for i in range(limit, entries.size()):
			rest += float(entries[i][1])
		entries.resize(limit)
		entries.append(["übrige", rest])
	return entries
