class_name TickerBar
extends PanelContainer
## Nachrichtenticker (seit M13, Schritt a5c; vertical_slice.md 4.1): die jüngsten Einträge der Chronik in
## einer Zeile über der Leiste der Kartenmodi, Wendepunkte zuerst und in Rot. Neue Einträge rücken nach;
## nach SHOW_SECONDS folgt der nächste. Ein Klick fährt den Ort an. Seit M14a1 nur sichtbar, solange er
## eine Meldung zeigt: HIDE_SECONDS nach der letzten verschwindet er.

signal location_requested(location_index: int)

const SHOW_SECONDS := 4.0
const HIDE_SECONDS := 12.0
const QUEUE_LIMIT := 12

var _queue: Array[Dictionary] = []
var _current: Dictionary = {}
var _shown_for := 0.0
var _label: Button = null


func _init() -> void:
	name = "TickerBar"
	mouse_filter = Control.MOUSE_FILTER_STOP
	_label = Button.new()
	_label.name = "Text"
	_label.flat = true
	_label.alignment = HORIZONTAL_ALIGNMENT_LEFT
	_label.clip_text = true
	_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	_label.focus_mode = Control.FOCUS_NONE
	_label.add_theme_font_size_override(&"font_size", 13)
	_label.pressed.connect(
		func() -> void:
			var location := int(_current.get("location_index", -1))
			if location >= 0:
				location_requested.emit(location)
	)
	add_child(_label)
	_label.text = "–"
	visible = false


## Neue Einträge der Chronik ({ date_text, text, location_index, turning_point }): Wendepunkte vorn.
func push(entries: Array[Dictionary]) -> void:
	for entry in entries:
		if bool(entry.get("turning_point", false)):
			_queue.push_front(entry)
		else:
			_queue.append(entry)
	while _queue.size() > QUEUE_LIMIT:
		var dropped := false
		for i in range(_queue.size() - 1, -1, -1):
			if not bool(_queue[i].get("turning_point", false)):
				_queue.remove_at(i)
				dropped = true
				break
		if not dropped:
			_queue.pop_back()
	if _current.is_empty() and not _queue.is_empty():
		_next()


func clear() -> void:
	_queue.clear()
	_current = {}
	_label.text = "–"
	visible = false


## Zeigt er gerade eine Meldung (sonst ist er ausgeblendet)?
func has_message() -> bool:
	return not _current.is_empty() and (not _queue.is_empty() or _shown_for < HIDE_SECONDS)


## Gezeigter Text (Tests).
func shown_text() -> String:
	return _label.text


func _process(delta: float) -> void:
	if _current.is_empty():
		return
	_shown_for += delta
	if _shown_for >= SHOW_SECONDS and not _queue.is_empty():
		_next()
	elif _queue.is_empty() and _shown_for >= HIDE_SECONDS and visible:
		visible = false


func _next() -> void:
	_current = _queue.pop_front()
	_shown_for = 0.0
	visible = true
	var turning := bool(_current.get("turning_point", false))
	_label.text = "%s  %s%s" % [str(_current.get("date_text", "")), "WENDEPUNKT · " if turning else "", str(_current.get("text", ""))]
	_label.tooltip_text = str(_current.get("text", ""))
	if turning:
		_label.add_theme_color_override(&"font_color", UiPalette.ACCENT)
	else:
		_label.remove_theme_color_override(&"font_color")
