class_name HintCard
extends PanelContainer
## Hinweis beim ersten Öffnen eines Fensters (seit M13b3, vertical_slice.md 5.4): zwei, drei Sätze aus
## game/tutorial/hinweise.cfg (Abschnitt = Knotenname des Fensters). Welche schon gezeigt wurden, steht in
## user://hinweise.cfg; die Einstellung „Hinweise“ schaltet sie ab. Ein Klick auf „Verstanden“ oder nach
## einer halben Minute verschwindet die Karte.

signal disable_requested

const TEXTS_PATH := "res://tutorial/hinweise.cfg"
const SEEN_PATH := "user://hinweise.cfg"
const WIDTH := 420.0
const SECONDS := 30.0

var seen_path := SEEN_PATH
var _texts := ConfigFile.new()
var _seen := ConfigFile.new()
var _title: Label = null
var _body: Label = null
var _left := 0.0
var _key := ""


func _ready() -> void:
	name = "HintCard"
	custom_minimum_size = Vector2(WIDTH, 0)
	visible = false
	mouse_filter = Control.MOUSE_FILTER_STOP
	_texts.load(TEXTS_PATH)
	_seen.load(seen_path)
	_build()


## Abschnitte mit Hinweistext (Tests).
func keys() -> PackedStringArray:
	return _texts.get_sections()


func has_hint(key: String) -> bool:
	return _texts.has_section(key)


func was_seen(key: String) -> bool:
	return bool(_seen.get_value("gesehen", key, false))


func current_key() -> String:
	return _key if visible else ""


## Zeigt den Hinweis zu key, wenn es ihn gibt und er noch nicht gesehen ist; true, wenn gezeigt.
func offer(key: String) -> bool:
	if not has_hint(key) or was_seen(key):
		return false
	_seen.set_value("gesehen", key, true)
	_seen.save(seen_path)
	_key = key
	_title.text = tr("Hinweis: %s") % tr(str(_texts.get_value(key, "titel", key)))
	_body.text = tr(str(_texts.get_value(key, "text", "")))
	_left = SECONDS
	visible = true
	reset_size()
	return true


## Alle Hinweise wieder zeigen (Einstellungen, Tests).
func reset_seen() -> void:
	_seen = ConfigFile.new()
	_seen.save(seen_path)


func use_seen_file(path: String) -> void:
	seen_path = path
	_seen = ConfigFile.new()
	_seen.load(path)


func dismiss() -> void:
	visible = false
	_key = ""


func _process(delta: float) -> void:
	if not visible:
		return
	_left -= delta
	if _left <= 0.0:
		dismiss()


func _build() -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 4)
	add_child(box)
	_title = Label.new()
	_title.theme_type_variation = &"HeaderLabel"
	box.add_child(_title)
	_body = Label.new()
	_body.name = "Body"
	_body.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_body.custom_minimum_size = Vector2(WIDTH - 30.0, 0)
	box.add_child(_body)
	var buttons := HBoxContainer.new()
	buttons.alignment = BoxContainer.ALIGNMENT_END
	var off := Button.new()
	off.name = "Off"
	off.flat = true
	off.text = tr("Keine Hinweise mehr")
	off.focus_mode = Control.FOCUS_NONE
	off.pressed.connect(func() -> void:
		dismiss()
		disable_requested.emit())
	buttons.add_child(off)
	var ok := Button.new()
	ok.name = "Ok"
	ok.text = tr("Verstanden")
	ok.focus_mode = Control.FOCUS_NONE
	ok.pressed.connect(dismiss)
	buttons.add_child(ok)
	box.add_child(buttons)
