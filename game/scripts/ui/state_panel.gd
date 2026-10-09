class_name StatePanel
extends PanelContainer
## Staatspanel des Spielerstaates mit Steuersatz-Regler.
##
## Der Regler schickt einen Befehl an die Simulation; wirksam wird er erst am
## nächsten simulierten Tag. Bis dahin zeigt das Panel den Wunschwert an.
## Beim Ziehen geht der Befehl erst beim Loslassen hinaus – sonst würde jeder
## Zwischenschritt als eigener Befehl (und Chronikeintrag) ausgeführt.

## Neuer Steuersatz in Prozent gewünscht.
signal tax_rate_requested(percent: int)

var _state_tax := -1
var _pending_tax := -1
var _updating := false
var _dragging := false

@onready var _coat: CoatOfArms = %Coat
@onready var _name: Label = %StateName
@onready var _short: Label = %StateShort
@onready var _ruler: Label = %Ruler
@onready var _government: Label = %Government
@onready var _capital: Label = %Capital
@onready var _locations: Label = %Locations
@onready var _population: Label = %Population
@onready var _treasury: Label = %Treasury
@onready var _income: Label = %Income
@onready var _expenses: Label = %Expenses
@onready var _balance: Label = %Balance
@onready var _tax_value: Label = %TaxValue
@onready var _tax_slider: HSlider = %TaxSlider
@onready var _tax_min: Label = %TaxMin
@onready var _tax_max: Label = %TaxMax
@onready var _tax_hint: Label = %TaxHint


func _ready() -> void:
	_tax_slider.focus_mode = Control.FOCUS_NONE
	_tax_slider.step = 1.0
	_tax_slider.value_changed.connect(_on_tax_slider_changed)
	_tax_slider.drag_started.connect(_on_tax_drag_started)
	_tax_slider.drag_ended.connect(_on_tax_drag_ended)
	show_unavailable()


## Obergrenze des Steuersatzes (get_max_tax_rate_percent).
func set_max_tax_rate(max_percent: int) -> void:
	_updating = true
	_tax_slider.min_value = 0
	_tax_slider.max_value = maxi(max_percent, 0)
	_tax_slider.tick_count = 0
	_updating = false
	_tax_min.text = GermanFormat.percent(0)
	_tax_max.text = GermanFormat.percent(max_percent)


## Daten des Staates (get_state) und Name der Hauptstadt anzeigen.
func show_state(state: Dictionary, capital_name: String) -> void:
	if state.is_empty():
		show_unavailable()
		return
	_coat.shield_color = state.get("color", UiPalette.INK_FAINT)
	_name.text = str(state.get("name", "–"))
	_short.text = "%s · %s" % [str(state.get("short_name", "")), str(state.get("tag", ""))]
	_ruler.text = str(state.get("ruler", "–"))
	_government.text = GermanFormat.government(str(state.get("government", "")))
	_capital.text = capital_name if not capital_name.is_empty() else "–"
	_locations.text = GermanFormat.integer(int(state.get("location_count", 0)))
	_population.text = GermanFormat.integer(int(state.get("population", 0)))
	var treasury := float(state.get("treasury", 0.0))
	_treasury.text = GermanFormat.money(treasury)
	_treasury.add_theme_color_override(
		&"font_color", UiPalette.NEGATIVE if treasury < 0.0 else UiPalette.INK
	)
	var income := float(state.get("income_month", 0.0))
	var expenses := float(state.get("expenses_month", 0.0))
	_income.text = GermanFormat.signed_money(income)
	_expenses.text = GermanFormat.signed_money(-expenses)
	var balance := income - expenses
	_balance.text = GermanFormat.signed_money(balance)
	_balance.add_theme_color_override(
		&"font_color", UiPalette.NEGATIVE if balance < 0.0 else UiPalette.POSITIVE
	)
	_state_tax = int(state.get("tax_rate_percent", 0))
	if _pending_tax == _state_tax:
		_pending_tax = -1
	_tax_slider.editable = true
	if not _dragging:
		_show_tax(_pending_tax if _pending_tax >= 0 else _state_tax)


func show_unavailable() -> void:
	_coat.shield_color = UiPalette.INK_FAINT
	_name.text = "Kein Spielerstaat"
	_short.text = "–"
	for label: Label in [
		_ruler, _government, _capital, _locations, _population, _treasury, _income, _expenses,
		_balance
	]:
		label.text = "–"
	_state_tax = -1
	_pending_tax = -1
	_tax_slider.editable = false
	_tax_value.text = "–"
	_tax_hint.text = "Ohne geladene Simulation nicht verfügbar."


## Ein Befehl wurde abgelehnt: Regler auf den Wert der Simulation zurücksetzen.
func reject_pending(message: String) -> void:
	_pending_tax = -1
	if _state_tax >= 0:
		_show_tax(_state_tax)
	_tax_hint.text = message


func _show_tax(percent: int) -> void:
	_updating = true
	_tax_slider.value = percent
	_updating = false
	_tax_value.text = GermanFormat.percent(percent)
	if _pending_tax >= 0 and _pending_tax != _state_tax:
		_tax_hint.text = "Neuer Satz gilt ab dem nächsten Tag (bisher %s)." % GermanFormat.percent(
			_state_tax
		)
	else:
		_tax_hint.text = "Höhere Steuern füllen die Kasse, bremsen aber das Wachstum."


func _on_tax_slider_changed(value: float) -> void:
	if _updating:
		return
	if _dragging:
		_tax_value.text = GermanFormat.percent(roundi(value))
		return
	_request_tax(roundi(value))


func _on_tax_drag_started() -> void:
	_dragging = true


func _on_tax_drag_ended(value_changed: bool) -> void:
	_dragging = false
	if value_changed and _tax_slider.editable:
		_request_tax(roundi(_tax_slider.value))


func _request_tax(percent: int) -> void:
	_pending_tax = percent
	_show_tax(percent)
	tax_rate_requested.emit(percent)
