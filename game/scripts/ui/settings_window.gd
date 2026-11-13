class_name SettingsWindow
extends PanelContainer
## Fenster Einstellungen (seit M13, Schritt a5d; vertical_slice.md 4.5): Reiter Bild, Ton, Spiel,
## Barrierefreiheit, Tasten und Sprache. Jede Änderung wirkt sofort und wird gespeichert (UserSettings).
## Startbildschirm und Spielmenü öffnen es; main.gd legt es an.

signal close_requested

const WIDTH := 620.0
const TABS: PackedStringArray = ["Bild", "Ton", "Spiel", "Barrierefreiheit", "Tasten", "Sprache"]
const GROUPS: PackedStringArray = ["bild", "ton", "spiel", "zugang", "tasten", "sprache"]

var _settings: UserSettings = null
var _tab := 0
var _tab_row: HBoxContainer = null
var _page: VBoxContainer = null
var _listening := ""
var _status: Label = null


func _ready() -> void:
	name = "SettingsWindow"
	custom_minimum_size = Vector2(WIDTH, 0)
	visible = false
	mouse_filter = Control.MOUSE_FILTER_STOP
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 8)
	add_child(box)
	var head := HBoxContainer.new()
	var title := Label.new()
	title.text = "EINSTELLUNGEN"
	title.theme_type_variation = &"HeaderLabel"
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	head.add_child(title)
	var reset := Button.new()
	reset.name = "Reset"
	reset.text = "Zurücksetzen"
	reset.tooltip_text = "Die Einstellungen dieses Reiters auf die Vorgaben"
	reset.focus_mode = Control.FOCUS_NONE
	reset.pressed.connect(
		func() -> void:
			_settings.reset_group(GROUPS[_tab])
			_commit()
			_fill()
	)
	head.add_child(reset)
	var close_button := Button.new()
	close_button.name = "Close"
	close_button.text = "Schließen"
	close_button.focus_mode = Control.FOCUS_NONE
	close_button.pressed.connect(func() -> void: close_requested.emit())
	head.add_child(close_button)
	box.add_child(head)
	_tab_row = HBoxContainer.new()
	_tab_row.add_theme_constant_override(&"separation", 4)
	for i in TABS.size():
		var b := Button.new()
		b.name = "Tab%d" % i
		b.text = TABS[i]
		b.toggle_mode = true
		b.focus_mode = Control.FOCUS_NONE
		b.pressed.connect(func() -> void: show_tab(i))
		_tab_row.add_child(b)
	box.add_child(_tab_row)
	_page = VBoxContainer.new()
	_page.add_theme_constant_override(&"separation", 6)
	_page.custom_minimum_size = Vector2(0, 360)
	box.add_child(_page)
	_status = Label.new()
	_status.theme_type_variation = &"MutedLabel"
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_status.custom_minimum_size = Vector2(WIDTH - 40.0, 0)
	box.add_child(_status)


func open(settings: UserSettings) -> void:
	_settings = settings
	visible = true
	_status.text = ""
	show_tab(_tab)


func close() -> void:
	visible = false
	_listening = ""


func is_open() -> bool:
	return visible


func show_tab(tab: int) -> void:
	_tab = clampi(tab, 0, TABS.size() - 1)
	_listening = ""
	for i in _tab_row.get_child_count():
		(_tab_row.get_child(i) as Button).set_pressed_no_signal(i == _tab)
	_fill()


func current_tab() -> int:
	return _tab


## Steuerelement einer Einstellung (Tests): CheckBox, HSlider oder OptionButton nach Schlüssel.
func control(key: String) -> Control:
	return _page.find_child(key.replace("/", "_"), true, false) as Control


## Wartet auf eine neue Taste für die Aktion (Tests rufen capture_key direkt).
func listening_action() -> String:
	return _listening


func _input(event: InputEvent) -> void:
	if not visible or _listening.is_empty():
		return
	if event is InputEventKey and event.pressed and not event.echo:
		get_viewport().set_input_as_handled()
		capture_key(_listening, int((event as InputEventKey).physical_keycode))


## Neue Taste für eine Aktion; eine schon belegte Taste wird abgelehnt.
func capture_key(action: String, physical_keycode: int) -> bool:
	_listening = ""
	if physical_keycode == KEY_ESCAPE:
		_status.text = "Escape ist für das Schließen reserviert."
		_fill()
		return false
	var other := _settings.action_with_key(physical_keycode, action)
	if not other.is_empty():
		_status.text = tr("Die Taste {taste} gehört schon zu „{aktion}“.").format({"taste": OS.get_keycode_string(physical_keycode), "aktion": tr(_action_label(other))})
		_fill()
		return false
	_settings.set_key(action, physical_keycode)
	_commit()
	_status.text = "%s: %s" % [tr(_action_label(action)), OS.get_keycode_string(physical_keycode)]
	_fill()
	return true


func _commit() -> void:
	_settings.apply(get_tree())
	_settings.save_file()


func _fill() -> void:
	for child in _page.get_children():
		_page.remove_child(child)
		child.queue_free()
	if _settings == null:
		return
	match GROUPS[_tab]:
		"bild":
			_check("bild/vollbild", "Vollbild")
			_slider("bild/oberflaeche", "Größe der Oberfläche", 75, 200, 5, "%")
			_options("bild/bildrate", "Bildrate begrenzen", ["unbegrenzt", "30 Bilder/s", "60 Bilder/s", "120 Bilder/s"], Array(UserSettings.FPS_LIMITS))
		"ton":
			_slider("ton/gesamt", "Lautstärke gesamt", 0, 100, 5, "%")
			_slider("ton/musik", "Musik", 0, 100, 5, "%")
			_slider("ton/klaenge", "Klänge", 0, 100, 5, "%")
			_check("ton/musik_aus", "Musik aus")
			_note("Musik: gemeinfreie Werke der Zeit (Beethoven, Haydn, Silcher, Schumann, Rouget de Lisle), mit einem eigenen Synthesizer gespielt; sie folgt der Lage des Staates (Frieden, Krise, Krieg, Revolution).")
		"spiel":
			_options("spiel/autosave", "Automatischer Spielstand", ["aus", "jährlich (1. Januar)", "halbjährlich (1. Januar, 1. Juli)"], UserSettings.AUTOSAVE)
			_check("spiel/kompass", "Historischer Kompass im Ereignisfenster")
			_check("spiel/hinweise", "Hinweise beim ersten Öffnen eines Fensters")
			_check("spiel/rueckblick", "„Was ist passiert?“ nach dem Laden eines Spielstands")
			_heading("AUTOMATISCHE PAUSE")
			_note("Ein Ereignis des Spielerstaates hält die Zeit immer an.")
			for rule: Array in UserSettings.PAUSE_RULES:
				_check("spiel/" + str(rule[0]), str(rule[1]))
		"zugang":
			_check("zugang/farbsehschwaeche", "Farben für Rot-Grün-Schwäche (Grün wird Blau)")
			_check("zugang/kontrast", "Hoher Kontrast der Schrift")
			_check("zugang/animationen_reduziert", "Reduzierte Animationen (Karte springt, Wahlabend sofort)")
			_slider("zugang/schrift", "Schriftgröße", 100, 150, 10, "%")
			_note("Schraffur für Besetzung und Krieg gilt immer.")
		"tasten":
			for action: Array in UserSettings.ACTIONS:
				_key_row(str(action[0]), str(action[1]))
		"sprache":
			_options("sprache/sprache", "Sprache", ["Deutsch", "English"], ["de", "en"])
			_note("Englisch: Startbildschirm, Spielmenü und Einstellungen; das Übrige bleibt vorerst deutsch.")


func _row(caption: String) -> HBoxContainer:
	var row := HBoxContainer.new()
	row.add_theme_constant_override(&"separation", 8)
	var label := Label.new()
	label.text = caption
	label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	row.add_child(label)
	_page.add_child(row)
	return row


func _heading(text: String) -> void:
	var label := Label.new()
	label.text = text
	label.theme_type_variation = &"CaptionLabel"
	_page.add_child(label)


func _note(text: String) -> void:
	var label := Label.new()
	label.text = text
	label.theme_type_variation = &"MutedLabel"
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.custom_minimum_size = Vector2(WIDTH - 40.0, 0)
	_page.add_child(label)


func _check(key: String, caption: String) -> void:
	var box := CheckBox.new()
	box.name = key.replace("/", "_")
	box.text = caption
	box.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART # längere Übersetzungen umbrechen (seit M13b4)
	box.focus_mode = Control.FOCUS_NONE
	box.button_pressed = bool(_settings.get_value(key))
	box.toggled.connect(
		func(on: bool) -> void:
			_settings.set_value(key, on)
			_commit()
	)
	_page.add_child(box)


func _slider(key: String, caption: String, low: int, high: int, step: int, unit: String) -> void:
	var row := _row(caption)
	var slider := HSlider.new()
	slider.name = key.replace("/", "_")
	slider.min_value = low
	slider.max_value = high
	slider.step = step
	slider.value = float(_settings.get_value(key))
	slider.custom_minimum_size = Vector2(200, 0)
	slider.focus_mode = Control.FOCUS_NONE
	var value := Label.new()
	value.text = "%d %s" % [int(slider.value), unit]
	value.custom_minimum_size = Vector2(56, 0)
	value.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	slider.value_changed.connect(func(v: float) -> void: value.text = "%d %s" % [int(v), unit])
	slider.drag_ended.connect(
		func(_changed: bool) -> void:
			_settings.set_value(key, int(slider.value))
			_commit()
	)
	row.add_child(slider)
	row.add_child(value)


func _options(key: String, caption: String, labels: Array, values: Array) -> void:
	var row := _row(caption)
	var select := OptionButton.new()
	select.name = key.replace("/", "_")
	select.focus_mode = Control.FOCUS_NONE
	for i in labels.size():
		select.add_item(str(labels[i]))
		if values[i] == _settings.get_value(key):
			select.select(i)
	select.item_selected.connect(
		func(i: int) -> void:
			_settings.set_value(key, values[i])
			_commit()
	)
	row.add_child(select)


func _key_row(action: String, caption: String) -> void:
	var row := _row(caption)
	var b := Button.new()
	b.name = "Key_" + action
	b.focus_mode = Control.FOCUS_NONE
	b.custom_minimum_size = Vector2(150, 0)
	b.text = "Taste drücken …" if _listening == action else OS.get_keycode_string(_settings.key_of(action))
	b.pressed.connect(
		func() -> void:
			_listening = action
			_status.text = tr("Neue Taste für „{aktion}“ drücken (Escape bricht ab).").format({"aktion": tr(caption)})
			_fill()
	)
	row.add_child(b)


func _action_label(action: String) -> String:
	for entry: Array in UserSettings.ACTIONS:
		if str(entry[0]) == action:
			return str(entry[1])
	return action
