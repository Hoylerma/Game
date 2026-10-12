class_name GameMenu
extends Control
## Spielmenü (seit M6, docs/technik/speicherstaende.md Abschnitt 9): Speichern unter einem Namen,
## Laden aus der Liste der Spielstände, Schnellspeichern und Schnellladen. Überdeckt die Karte;
## solange es offen ist, hält die Hauptszene die Zeit an. Der Aufbau steht im Code, die Hauptszene
## legt das Menü an (main.gd).

signal save_requested(file_name: String)
signal load_requested(path: String)
signal quick_save_requested
signal quick_load_requested
signal close_requested

const PANEL_WIDTH := 680.0
const LIST_HEIGHT := 250.0

var _name_edit: LineEdit = null
var _save_button: Button = null
var _list: VBoxContainer = null
var _empty_label: Label = null
var _load_button: Button = null
var _quick_load_button: Button = null
var _status: Label = null
var _group := ButtonGroup.new()
var _selected_path := ""


func _ready() -> void:
	name = "GameMenu"
	set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_STOP
	visible = false
	_build()


## Öffnet das Menü mit einem Namensvorschlag und der Liste der Spielstände (SaveSlots.list).
func open(default_name: String, saves: Array[Dictionary], quicksave_exists: bool) -> void:
	_name_edit.text = default_name
	show_saves(saves, quicksave_exists)
	_status.text = ""
	visible = true
	_name_edit.grab_focus()
	_name_edit.select_all()


func close() -> void:
	visible = false
	_name_edit.release_focus()


func is_open() -> bool:
	return visible


## Spielstände anzeigen; unpassende ausgegraut mit Grund als Tooltip.
func show_saves(saves: Array[Dictionary], quicksave_exists: bool) -> void:
	for child in _list.get_children():
		_list.remove_child(child)
		child.queue_free()
	_selected_path = ""
	for save in saves:
		_list.add_child(_save_entry(save))
	_empty_label.visible = saves.is_empty()
	_load_button.disabled = true
	_quick_load_button.disabled = not quicksave_exists


## Rückmeldung unter den Knöpfen (Fehler in Rot).
func show_status(text: String, is_error: bool = false) -> void:
	_status.text = text
	_status.add_theme_color_override(
		&"font_color", UiPalette.NEGATIVE if is_error else UiPalette.INK_SOFT
	)


## Namen der Einträge in der Ladeliste (Tests).
func entry_names() -> PackedStringArray:
	var names := PackedStringArray()
	for child in _list.get_children():
		names.append(str(child.get_meta(&"save_name", "")))
	return names


## Eintrag auswählen und laden wie per Klick (Klicktest).
func load_entry(save_name: String) -> bool:
	for child in _list.get_children():
		if str(child.get_meta(&"save_name", "")) == save_name and not (child as Button).disabled:
			(child as Button).button_pressed = true
			_on_load_pressed()
			return true
	return false


## Speichern wie per Knopf mit dem Namen file_name (Klicktest).
func save_as(file_name: String) -> void:
	_name_edit.text = file_name
	_on_save_pressed()


func _input(event: InputEvent) -> void:
	# Escape schließt das Menü, auch wenn das Namensfeld den Fokus hat (es würde die Taste sonst
	# für sich behalten).
	if visible and event.is_action_pressed(&"auswahl_aufheben", false, true):
		close_requested.emit()
		get_viewport().set_input_as_handled()


func _gui_input(event: InputEvent) -> void:
	# Klick neben das Fenster schließt das Menü.
	if event is InputEventMouseButton and event.pressed and event.button_index == MOUSE_BUTTON_LEFT:
		close_requested.emit()
		accept_event()


func _build() -> void:
	var dimmer := ColorRect.new()
	dimmer.color = Color(UiPalette.INK, 0.35)
	dimmer.mouse_filter = Control.MOUSE_FILTER_IGNORE
	dimmer.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(dimmer)

	var center := CenterContainer.new()
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	center.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(center)

	var panel := PanelContainer.new()
	panel.custom_minimum_size = Vector2(PANEL_WIDTH, 0)
	panel.mouse_filter = Control.MOUSE_FILTER_STOP
	center.add_child(panel)

	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 10)
	panel.add_child(box)

	var title := Label.new()
	title.text = "SPIELMENÜ"
	title.theme_type_variation = &"HeaderLabel"
	box.add_child(title)

	box.add_child(_caption("Speichern"))
	var save_row := HBoxContainer.new()
	_name_edit = LineEdit.new()
	_name_edit.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_name_edit.placeholder_text = "Name des Spielstands"
	_name_edit.max_length = SaveSlots.MAX_NAME_LENGTH + 10
	_name_edit.text_submitted.connect(func(_text: String) -> void: _on_save_pressed())
	_style_line_edit(_name_edit)
	save_row.add_child(_name_edit)
	_save_button = _button("Speichern", _on_save_pressed)
	save_row.add_child(_save_button)
	box.add_child(save_row)

	box.add_child(_caption("Laden"))
	var scroll := ScrollContainer.new()
	scroll.custom_minimum_size = Vector2(0, LIST_HEIGHT)
	scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_list = VBoxContainer.new()
	_list.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_list.add_theme_constant_override(&"separation", 2)
	scroll.add_child(_list)
	box.add_child(scroll)
	_empty_label = Label.new()
	_empty_label.text = "Noch keine Spielstände."
	_empty_label.theme_type_variation = &"MutedLabel"
	box.add_child(_empty_label)

	var row := HBoxContainer.new()
	row.add_theme_constant_override(&"separation", 6)
	_load_button = _button("Laden", _on_load_pressed)
	row.add_child(_load_button)
	row.add_child(_button("Schnellspeichern (F5)", func() -> void: quick_save_requested.emit()))
	_quick_load_button = _button("Schnellladen (F9)", func() -> void: quick_load_requested.emit())
	row.add_child(_quick_load_button)
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(spacer)
	row.add_child(_button("Weiterspielen (F10)", func() -> void: close_requested.emit()))
	box.add_child(row)

	_status = Label.new()
	_status.theme_type_variation = &"CaptionLabel"
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_status.custom_minimum_size = Vector2(1, 0)
	box.add_child(_status)


## Das Theme kennt kein Eingabefeld: Papierstil wie die Panels statt des dunklen Godot-Standards.
static func _style_line_edit(edit: LineEdit) -> void:
	var normal := StyleBoxFlat.new()
	normal.bg_color = UiPalette.PAPER
	normal.border_color = UiPalette.EDGE
	normal.set_border_width_all(1)
	normal.set_corner_radius_all(3)
	normal.content_margin_left = 8.0
	normal.content_margin_right = 8.0
	normal.content_margin_top = 4.0
	normal.content_margin_bottom = 4.0
	var focus := normal.duplicate() as StyleBoxFlat
	focus.draw_center = false
	focus.border_color = UiPalette.GOLD
	focus.set_border_width_all(2)
	edit.add_theme_stylebox_override(&"normal", normal)
	edit.add_theme_stylebox_override(&"focus", focus)
	edit.add_theme_color_override(&"font_color", UiPalette.INK)
	edit.add_theme_color_override(&"font_selected_color", UiPalette.INK)
	edit.add_theme_color_override(&"font_placeholder_color", UiPalette.INK_FAINT)
	edit.add_theme_color_override(&"caret_color", UiPalette.INK)
	edit.add_theme_color_override(&"selection_color", Color(UiPalette.GOLD, 0.35))


func _caption(text: String) -> Label:
	var label := Label.new()
	label.text = text.to_upper()
	label.theme_type_variation = &"CaptionLabel"
	return label


func _button(text: String, action: Callable) -> Button:
	var button := Button.new()
	button.text = text
	button.focus_mode = Control.FOCUS_NONE
	button.custom_minimum_size = Vector2(0, 30)
	button.pressed.connect(action)
	return button


## Eintrag der Ladeliste: Name, Spielstand (Datum, Staat) und Änderungszeit.
func _save_entry(save: Dictionary) -> Button:
	var info: Dictionary = save.get("info", {})
	var button := Button.new()
	button.toggle_mode = true
	button.button_group = _group
	button.focus_mode = Control.FOCUS_NONE
	button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.theme_type_variation = &"FlatButton"
	button.set_meta(&"save_name", str(save.get("name", "")))
	var detail := str(info.get("reason", "nicht lesbar"))
	if bool(info.get("ok", false)):
		detail = "%s · %s" % [str(info.get("date_text", "")), str(info.get("player_name", ""))]
	button.text = "%s — %s · %s" % [
		str(save.get("name", "")), detail, SaveSlots.format_modified(int(save.get("modified", 0)))
	]
	button.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	button.clip_text = true
	if not bool(info.get("compatible", false)):
		button.disabled = true
		button.tooltip_text = str(info.get("reason", "Spielstand lässt sich nicht laden."))
	else:
		button.tooltip_text = "Spielstand von %s, Prüfsumme %s" % [
			str(info.get("date_text", "")), str(info.get("checksum", ""))
		]
	button.toggled.connect(_on_entry_toggled.bind(str(save.get("path", ""))))
	return button


func _on_entry_toggled(pressed: bool, path: String) -> void:
	if pressed:
		_selected_path = path
		_load_button.disabled = false


func _on_save_pressed() -> void:
	var file_name := SaveSlots.sanitize(_name_edit.text)
	if file_name.is_empty():
		show_status("Bitte einen Namen für den Spielstand eingeben.", true)
		return
	if SaveSlots.is_reserved(file_name):
		show_status(
			"„%s“ ist für Schnellspeicher und automatische Spielstände reserviert." % file_name, true
		)
		return
	save_requested.emit(file_name)


func _on_load_pressed() -> void:
	if not _selected_path.is_empty():
		load_requested.emit(_selected_path)
