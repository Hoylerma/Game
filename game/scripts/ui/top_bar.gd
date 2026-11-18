class_name TopBar
extends PanelContainer
## Obere Leiste: Spielerstaat, Kennzahlen, Datum und Geschwindigkeit. Seit M13a5c (vertical_slice.md 4.1)
## nach Kapitel 22: Kasse mit Saldo, Legitimität, Verwaltung und die fünf Dimensionen des Vermächtnisses; je
## Zahl ein verschachtelter Tooltip (TooltipLayer, main.gd verbindet ihn).

## Geschwindigkeit gewählt (0 = Pause, 1–5).
signal speed_selected(speed: int)
## Knopf „Menü“ gedrückt (seit M6: Spielmenü mit Speichern und Laden).
signal menu_requested

## Dauer einer kurzen Meldung unter dem Datum (z. B. „Schnellgespeichert“).
const NOTICE_SECONDS := 3.0

var _speed_line := ""
var _notice_serial := 0
var _notice_active := false

@onready var _coat: CoatOfArms = %Coat
@onready var _state_name: Label = %StateName
@onready var _state_detail: Label = %StateDetail
@onready var _cash: Label = %Cash
## Saldo neben der Kasse (seit dem Spieltest 11/2026 eigene Farbe: grün oder rot).
var _saldo: Label = null
@onready var _legitimacy: Label = %Legitimacy
@onready var _administration: Label = %Administration
@onready var _legacy: Label = %Legacy
@onready var _date: Label = %Date
@onready var _speed_text: Label = %SpeedText
@onready var _menu_button: Button = %GameMenuButton
@onready var _speed_buttons: Array[Button] = [
	%SpeedPause as Button,
	%Speed1 as Button,
	%Speed2 as Button,
	%Speed3 as Button,
	%Speed4 as Button,
	%Speed5 as Button,
]


func _ready() -> void:
	var group := ButtonGroup.new()
	group.allow_unpress = false
	for speed in _speed_buttons.size():
		var button := _speed_buttons[speed]
		button.button_group = group
		button.toggle_mode = true
		button.focus_mode = Control.FOCUS_NONE
		button.pressed.connect(_on_speed_button_pressed.bind(speed))
	_menu_button.pressed.connect(func() -> void: menu_requested.emit())
	_build_saldo()
	show_unavailable()


## Kasse und Saldo nebeneinander, damit jeder Wert seine Farbe bekommt.
func _build_saldo() -> void:
	var box := _cash.get_parent()
	var row := HBoxContainer.new()
	row.name = "CashRow"
	row.add_theme_constant_override(&"separation", 0)
	row.mouse_filter = Control.MOUSE_FILTER_IGNORE
	box.add_child(row)
	box.move_child(row, _cash.get_index())
	_cash.reparent(row)
	var dot := Label.new()
	dot.text = " · "
	dot.theme_type_variation = &"ValueLabel"
	row.add_child(dot)
	_saldo = Label.new()
	_saldo.name = "Saldo"
	_saldo.theme_type_variation = &"ValueLabel"
	row.add_child(_saldo)


## Kennzahlen des Spielerstaates (Dictionary aus get_state).
func show_state(state: Dictionary) -> void:
	if state.is_empty():
		show_unavailable()
		return
	_coat.shield_color = state.get("color", UiPalette.INK_FAINT)
	_state_name.text = str(state.get("name", "–"))
	_state_detail.text = "%s · %s" % [
		str(state.get("ruler", "–")), Format.government(str(state.get("government", "")))
	]
	var treasury := float(state.get("treasury", 0.0))
	var saldo := float(state.get("income_month", 0.0)) - float(state.get("expenses_month", 0.0))
	_cash.text = Format.money(treasury)
	_cash.add_theme_color_override(&"font_color", UiPalette.amount_color(treasury))
	_saldo.text = Format.signed_number(roundi(saldo))
	_saldo.add_theme_color_override(&"font_color", UiPalette.amount_color(roundi(saldo)))


## Kennzahlen aus get_state_overview und get_legacy (seit M13a5c; main.gd höchstens zweimal je Sekunde).
func show_overview(overview: Dictionary, legacy: Dictionary) -> void:
	if overview.is_empty():
		return
	if bool(overview.get("events", false)):
		var value := float(overview.get("legitimacy", 0.0))
		_legitimacy.text = Format.signed_number(value, 1)
		_legitimacy.add_theme_color_override(&"font_color", UiPalette.POSITIVE if value > 0.5 else (UiPalette.NEGATIVE if value < -0.5 else UiPalette.INK))
	else:
		_legitimacy.text = "–"
	_administration.text = "%s Orte" % Format.integer(int(overview.get("locations", 0)))
	var parts := PackedStringArray()
	for dimension: Dictionary in legacy.get("dimensions", []):
		parts.append("%s%d" % [str(dimension.get("name", "")).left(1), roundi(float(dimension.get("value", 0.0)))])
	_legacy.text = " ".join(parts) if not parts.is_empty() else "–"


## Felder für die Tooltips (main.gd): kasse, legitimitaet, verwaltung, vermaechtnis.
func field(key: StringName) -> Control:
	match key:
		&"kasse":
			return _cash.get_parent().get_parent() as Control
		&"legitimitaet":
			return _legitimacy.get_parent() as Control
		&"verwaltung":
			return _administration.get_parent() as Control
		&"vermaechtnis":
			return _legacy.get_parent() as Control
	return null


## Texte der Kennzahlen (Tests).
func field_text(key: StringName) -> String:
	match key:
		&"kasse":
			return "%s · %s" % [_cash.text, _saldo.text]
		&"legitimitaet":
			return _legitimacy.text
		&"verwaltung":
			return _administration.text
		&"vermaechtnis":
			return _legacy.text
	return ""


## Leere Anzeige, solange keine Simulation läuft.
func show_unavailable() -> void:
	_coat.shield_color = UiPalette.INK_FAINT
	_state_name.text = "Keine Simulation"
	_state_detail.text = "Spielerstaat unbekannt"
	for label: Label in [_cash, _legitimacy, _administration, _legacy]:
		label.text = "–"
		label.remove_theme_color_override(&"font_color")
	if _saldo != null:
		_saldo.text = "–"
		_saldo.remove_theme_color_override(&"font_color")
	_date.text = "–"
	_set_speed_line("Simulation nicht geladen")
	for button in _speed_buttons:
		button.disabled = true
	_menu_button.disabled = true


## Tooltips der Geschwindigkeitsstufen (Index 0 = Pause).
func configure_speeds(days_per_second: PackedInt32Array) -> void:
	_speed_buttons[0].tooltip_text = "Pause (Leertaste)"
	for speed in range(1, mini(days_per_second.size(), _speed_buttons.size())):
		var days := days_per_second[speed]
		_speed_buttons[speed].tooltip_text = "Stufe %d: %d %s je Sekunde (Taste %d)" % [
			speed, days, "Tag" if days == 1 else "Tage", speed
		]


func show_date(text: String) -> void:
	_date.text = text


## Aktive Geschwindigkeit anzeigen (0 = Pause) und Schaltflächen freigeben.
func show_speed(speed: int, days_per_second: int) -> void:
	for i in _speed_buttons.size():
		_speed_buttons[i].disabled = false
		_speed_buttons[i].set_pressed_no_signal(i == speed)
	_menu_button.disabled = false
	if speed == 0:
		_set_speed_line("Pausiert · Leertaste setzt fort")
	elif days_per_second == 1:
		_set_speed_line("Stufe %d · 1 Tag je Sekunde" % speed)
	else:
		_set_speed_line("Stufe %d · %d Tage je Sekunde" % [speed, days_per_second])


## Kurze Meldung anstelle der Geschwindigkeitszeile, nach NOTICE_SECONDS wieder die Geschwindigkeit
## (seit M6: Speichern, Laden, automatischer Spielstand).
func show_notice(text: String) -> void:
	_notice_serial += 1
	_notice_active = true
	var serial := _notice_serial
	_speed_text.text = text
	get_tree().create_timer(NOTICE_SECONDS).timeout.connect(
		func() -> void:
			if serial == _notice_serial:
				_notice_active = false
				_speed_text.text = _speed_line
	)


## Gerade angezeigte Meldung bzw. Geschwindigkeitszeile (Tests).
func speed_line_text() -> String:
	return _speed_text.text


func _set_speed_line(text: String) -> void:
	_speed_line = text
	# Eine laufende Meldung bleibt stehen, bis ihre Zeit um ist.
	if not _notice_active:
		_speed_text.text = text


func _on_speed_button_pressed(speed: int) -> void:
	speed_selected.emit(speed)
