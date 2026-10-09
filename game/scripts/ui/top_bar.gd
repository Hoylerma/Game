class_name TopBar
extends PanelContainer
## Obere Leiste: Spielerstaat, Kennzahlen, Datum und Geschwindigkeit.

## Geschwindigkeit gewählt (0 = Pause, 1–5).
signal speed_selected(speed: int)

@onready var _coat: CoatOfArms = %Coat
@onready var _state_name: Label = %StateName
@onready var _state_detail: Label = %StateDetail
@onready var _treasury: Label = %Treasury
@onready var _income: Label = %Income
@onready var _expenses: Label = %Expenses
@onready var _population: Label = %Population
@onready var _date: Label = %Date
@onready var _speed_text: Label = %SpeedText
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
	show_unavailable()


## Kennzahlen des Spielerstaates (Dictionary aus get_state).
func show_state(state: Dictionary) -> void:
	if state.is_empty():
		show_unavailable()
		return
	_coat.shield_color = state.get("color", UiPalette.INK_FAINT)
	_state_name.text = str(state.get("name", "–"))
	_state_detail.text = "%s · %s" % [
		str(state.get("ruler", "–")), GermanFormat.government(str(state.get("government", "")))
	]
	_treasury.text = GermanFormat.money(float(state.get("treasury", 0.0)))
	_treasury.add_theme_color_override(
		&"font_color",
		UiPalette.NEGATIVE if float(state.get("treasury", 0.0)) < 0.0 else UiPalette.INK
	)
	_income.text = GermanFormat.signed_money(float(state.get("income_month", 0.0)))
	_expenses.text = GermanFormat.signed_money(-float(state.get("expenses_month", 0.0)))
	_population.text = GermanFormat.integer(int(state.get("population", 0)))


## Leere Anzeige, solange keine Simulation läuft.
func show_unavailable() -> void:
	_coat.shield_color = UiPalette.INK_FAINT
	_state_name.text = "Keine Simulation"
	_state_detail.text = "Spielerstaat unbekannt"
	for label: Label in [_treasury, _income, _expenses, _population]:
		label.text = "–"
	_date.text = "–"
	_speed_text.text = "Simulation nicht geladen"
	for button in _speed_buttons:
		button.disabled = true


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
	if speed == 0:
		_speed_text.text = "Pausiert · Leertaste setzt fort"
	elif days_per_second == 1:
		_speed_text.text = "Stufe %d · 1 Tag je Sekunde" % speed
	else:
		_speed_text.text = "Stufe %d · %d Tage je Sekunde" % [speed, days_per_second]


func _on_speed_button_pressed(speed: int) -> void:
	speed_selected.emit(speed)
