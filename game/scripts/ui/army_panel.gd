class_name ArmyPanel
extends PanelContainer
## Heeresfenster (seit M10, Schritt b3; militaer.md Abschnitt 8): das ausgewählte Heer mit Stärke,
## Moral, Versorgung, Befehlshaber, Standort und Weg; für eigene Feldheere der Knopf „Dem Generalstab
## übergeben“ bzw. „Selbst führen“. Steht in der rechten Spalte an der Stelle der Ortsinfo.
##
## Das Panel liest nichts selbst: main.gd übergibt das Heer (get_army) und die Namen; Befehle gehen
## als Signal hinaus und wirken am nächsten Tag.

## Schließen-Schaltfläche gedrückt.
signal close_requested
## Heer dem Generalstab übergeben (on) oder zurücknehmen.
signal staff_requested(army_id: int, on: bool)
## Standort des Heeres anfahren.
signal focus_requested(army_id: int)

const WIDTH := 360.0

var _army_id := -1
## Generalstab: gewünschter Wert bis zum nächsten Tag (-1: kein Befehl unterwegs).
var _staff_pending := -1
var _title: Label = null
var _subtitle: Label = null
var _grid: GridContainer = null
var _morale_bar: ProgressBar = null
var _staff_button: Button = null
var _focus_button: Button = null
var _hint: Label = null


func _ready() -> void:
	name = "ArmyPanel"
	custom_minimum_size = Vector2(WIDTH, 0)
	visible = false
	mouse_filter = Control.MOUSE_FILTER_STOP
	_build()


## Heer anzeigen: army wie get_army, state_name Kurzname des Staates, location_name Standort,
## destination_name Ziel (leer ohne Weg), own (gehört dem Spieler), arrival_text Ankunft am Ziel.
func show_army(army: Dictionary, state_name: String, location_name: String, destination_name: String,
		own: bool, arrival_text: String = "") -> void:
	if army.is_empty():
		hide_army()
		return
	var id := int(army.get("id", -1))
	if id != _army_id:
		_staff_pending = -1
	_army_id = id
	var staff := bool(army.get("staff", false))
	if _staff_pending >= 0 and staff == (_staff_pending == 1):
		_staff_pending = -1
	var garrison := bool(army.get("garrison", false))
	_title.text = str(army.get("name", "–"))
	_subtitle.text = "%s · %s%s" % [
		state_name, str(army.get("status_text", "")), " · Generalstab" if staff else ""
	]
	for child in _grid.get_children():
		_grid.remove_child(child)
		child.queue_free()
	var commander := str(army.get("commander", ""))
	_line("Befehlshaber", "%s (Führung %s)" % [
		commander if not commander.is_empty() else "ohne Namen",
		GermanFormat.number(float(army.get("leadership", 1.0)), 2),
	])
	_line("Stärke", "%s Mann, %s Geschütze" % [
		GermanFormat.integer(int(army.get("soldiers", 0))), GermanFormat.integer(int(army.get("guns", 0)))
	])
	var morale := float(army.get("morale", 0.0))
	_line("Moral", GermanFormat.percent(morale * 100.0))
	_morale_bar.value = morale * 100.0
	var supplied := bool(army.get("supplied", true))
	_line("Versorgung", "versorgt" if supplied else "abgeschnitten (Verluste und Moral sinken)", not supplied)
	_line("Standort", location_name if not location_name.is_empty() else "–")
	if not destination_name.is_empty():
		var leg := "auf der Bahn" if bool(army.get("on_rail", false)) else "zu Fuß"
		var text := "%s (%s, noch %s Tage im Abschnitt)" % [
			destination_name, leg, GermanFormat.number(float(army.get("days_left", 0.0)), 1)
		]
		if not arrival_text.is_empty():
			text += ", Ankunft %s" % arrival_text
		_line("Ziel", text)
	_staff_button.visible = own and not garrison
	var wanted := staff if _staff_pending < 0 else _staff_pending == 1
	_staff_button.text = "Selbst führen" if wanted else "Dem Generalstab übergeben"
	_staff_button.disabled = _staff_pending >= 0
	_staff_button.tooltip_text = (
		"Der Generalstab führt das Heer selbst: sammeln, angreifen, belagern, zurückweichen. "
		+ "Ein eigener Marschbefehl nimmt es ihm wieder ab."
		if not wanted else "Das Heer folgt nur noch Euren Marschbefehlen."
	)
	if _staff_pending >= 0:
		_hint.text = "Befehl erteilt; wirksam ab dem nächsten Tag."
	elif garrison:
		_hint.text = "Garnison: bleibt in ihrer Festung und verteidigt sie."
	elif own:
		_hint.text = "Rechtsklick auf einen Ort: Marschbefehl (der Tooltip nennt Weg und Ankunft)."
	else:
		_hint.text = "Fremdes Heer."
	visible = true


func hide_army() -> void:
	_army_id = -1
	_staff_pending = -1
	visible = false


## Nach einem simulierten Tag: Der Befehl ist ausgeführt oder verworfen (main.gd).
func clear_pending() -> void:
	_staff_pending = -1


func army_id() -> int:
	return _army_id if visible else -1


## Text des Panels, zeilenweise (für Tests).
func panel_text() -> String:
	var lines := PackedStringArray([_title.text, _subtitle.text])
	for i in range(0, _grid.get_child_count() - 1, 2):
		lines.append("%s: %s" % [(_grid.get_child(i) as Label).text, (_grid.get_child(i + 1) as Label).text])
	if _staff_button.visible:
		lines.append("[%s%s]" % [_staff_button.text, " gesperrt" if _staff_button.disabled else ""])
	lines.append(_hint.text)
	return "\n".join(lines)


## Knopf „Dem Generalstab übergeben“ bzw. „Selbst führen“ (für Tests).
func staff_button() -> Button:
	return _staff_button


func _build() -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 6)
	add_child(box)
	var head := HBoxContainer.new()
	var header := Label.new()
	header.text = "HEER"
	header.theme_type_variation = &"HeaderLabel"
	header.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	head.add_child(header)
	var close_button := Button.new()
	close_button.name = "Close"
	close_button.text = "×"
	close_button.theme_type_variation = &"FlatButton"
	close_button.focus_mode = Control.FOCUS_NONE
	close_button.tooltip_text = "Schließen (Esc)"
	close_button.pressed.connect(func() -> void: close_requested.emit())
	head.add_child(close_button)
	box.add_child(head)
	_title = Label.new()
	_title.theme_type_variation = &"NameLabel"
	_title.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_title.custom_minimum_size = Vector2(WIDTH - 40.0, 0)
	box.add_child(_title)
	_subtitle = Label.new()
	_subtitle.theme_type_variation = &"MutedLabel"
	_subtitle.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_subtitle.custom_minimum_size = Vector2(WIDTH - 40.0, 0)
	box.add_child(_subtitle)
	box.add_child(HSeparator.new())
	_grid = GridContainer.new()
	_grid.name = "Facts"
	_grid.columns = 2
	_grid.add_theme_constant_override(&"h_separation", 12)
	box.add_child(_grid)
	_morale_bar = ProgressBar.new()
	_morale_bar.name = "Morale"
	_morale_bar.min_value = 0.0
	_morale_bar.max_value = 100.0
	_morale_bar.show_percentage = false
	_morale_bar.custom_minimum_size = Vector2(0, 6)
	_morale_bar.tooltip_text = "Moral: sinkt mit Verlusten und ohne Versorgung, steigt nach Siegen und in Ruhe"
	box.add_child(_morale_bar)
	var buttons := HBoxContainer.new()
	buttons.add_theme_constant_override(&"separation", 6)
	_staff_button = Button.new()
	_staff_button.name = "Staff"
	_staff_button.focus_mode = Control.FOCUS_NONE
	_staff_button.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_staff_button.pressed.connect(_on_staff_pressed)
	buttons.add_child(_staff_button)
	_focus_button = Button.new()
	_focus_button.name = "Focus"
	_focus_button.text = "Zeigen"
	_focus_button.focus_mode = Control.FOCUS_NONE
	_focus_button.tooltip_text = "Karte auf den Standort des Heeres"
	_focus_button.pressed.connect(func() -> void: focus_requested.emit(_army_id))
	buttons.add_child(_focus_button)
	box.add_child(buttons)
	_hint = Label.new()
	_hint.theme_type_variation = &"CaptionLabel"
	_hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_hint.custom_minimum_size = Vector2(WIDTH - 40.0, 0)
	box.add_child(_hint)


func _line(caption: String, value: String, warning: bool = false) -> void:
	var left := Label.new()
	left.theme_type_variation = &"MutedLabel"
	left.text = caption
	left.size_flags_vertical = Control.SIZE_SHRINK_BEGIN
	var right := Label.new()
	right.text = value
	right.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	right.custom_minimum_size = Vector2(1, 0)
	if warning:
		right.add_theme_color_override(&"font_color", UiPalette.NEGATIVE)
	_grid.add_child(left)
	_grid.add_child(right)


func _on_staff_pressed() -> void:
	if _army_id < 0 or _staff_pending >= 0:
		return
	var on := _staff_button.text != "Selbst führen"
	_staff_pending = 1 if on else 0
	staff_requested.emit(_army_id, on)
