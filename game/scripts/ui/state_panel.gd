class_name StatePanel
extends PanelContainer
## Staatspanel des Spielerstaates mit Reglern für Kontribution, Akzise und Zoll und einem
## Haushaltsfenster (seit M5).
##
## Ein Regler schickt einen Befehl an die Simulation; wirksam wird er erst am nächsten
## simulierten Tag. Bis dahin zeigt das Panel den Wunschwert an. Beim Ziehen geht der Befehl
## erst beim Loslassen hinaus – sonst würde jeder Zwischenschritt als eigener Befehl (und
## Chronikeintrag) ausgeführt.

## Neuer Satz in Prozent für eine Steuerart (SimulationClient.TAX_*) gewünscht.
signal tax_rate_requested(kind: int, percent: int)

## Namen der Steuerarten, Index = SimulationClient.TAX_*.
const TAX_NAMES: PackedStringArray = ["Kontribution", "Akzise", "Zoll"]
const TAX_TOOLTIPS: PackedStringArray = [
	"Kontribution: direkte Steuer auf die Einkommen auf dem Land und auf Agrareinkommen in Städten",
	"Akzise: Verbrauchssteuer auf alle Käufe in Städten",
	"Zoll: Abgabe auf eingeführte Waren (nach ihrem Anteil am Angebot des Marktes)",
]
const DEFAULT_HINT := "Höhere Steuern füllen die Kasse, verteuern aber Waren und bremsen das Wachstum."

var _state_rates: Array[int] = [-1, -1, -1]
var _pending: Array[int] = [-1, -1, -1]
var _dragging := -1
var _updating := false
var _sliders: Array[HSlider] = []
var _values: Array[Label] = []
var _state := {}
var _budget_popup: PopupPanel = null
var _budget_grid: GridContainer = null
## Verfassung und Interessengruppen (seit M8): Knopf und Fenster; main.gd liefert die Angaben über
## politics_source (SimulationClient.get_state_politics des Spielerstaates).
var politics_source: Callable = Callable()
var _politics_button: Button = null
var _politics_popup: PopupPanel = null
var _politics_grid: GridContainer = null
var _politics_groups: VBoxContainer = null

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
@onready var _debt: Label = %Debt
@onready var _taxes: VBoxContainer = %Taxes
@onready var _tax_hint: Label = %TaxHint
@onready var _budget_button: Button = %BudgetButton


func _ready() -> void:
	for kind in TAX_NAMES.size():
		_add_tax_row(kind)
	_build_budget_popup()
	_budget_button.pressed.connect(show_budget)
	_build_politics()
	show_unavailable()


## Obergrenze der Steuersätze (get_max_tax_rate_percent), gilt für alle drei Arten.
func set_max_tax_rate(max_percent: int) -> void:
	_updating = true
	for slider in _sliders:
		slider.min_value = 0
		slider.max_value = maxi(max_percent, 0)
	_updating = false


## Daten des Staates (get_state) und Name der Hauptstadt anzeigen.
func show_state(state: Dictionary, capital_name: String) -> void:
	if state.is_empty():
		show_unavailable()
		return
	_state = state
	_coat.shield_color = state.get("color", UiPalette.INK_FAINT)
	_name.text = str(state.get("name", "–"))
	_short.text = "%s · %s" % [str(state.get("short_name", "")), str(state.get("tag", ""))]
	# Mitgliedschaften (seit M7).
	var member := PackedStringArray()
	if bool(state.get("bund", false)):
		member.append("Deutscher Bund")
	if bool(state.get("zollverein", false)):
		member.append("Zollverein")
	if not member.is_empty():
		_short.text += " · " + ", ".join(member)
	_ruler.text = str(state.get("ruler", "–"))
	_government.text = GermanFormat.government(str(state.get("government", "")))
	_capital.text = capital_name if not capital_name.is_empty() else "–"
	_locations.text = GermanFormat.integer(int(state.get("location_count", 0)))
	var population := int(state.get("population", 0))
	_population.text = GermanFormat.integer(population)
	_population.mouse_filter = Control.MOUSE_FILTER_PASS
	_population.tooltip_text = _population_tooltip(state, population)
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
	var debt := float(state.get("debt", 0.0))
	_debt.text = GermanFormat.money(debt)
	_debt.add_theme_color_override(
		&"font_color", UiPalette.NEGATIVE if debt > 0.0 else UiPalette.INK
	)
	var rates: Array = state.get("tax_rates_percent", [])
	for kind in _sliders.size():
		_state_rates[kind] = int(rates[kind]) if kind < rates.size() else -1
		if _pending[kind] == _state_rates[kind]:
			_pending[kind] = -1
		_sliders[kind].editable = _state_rates[kind] >= 0
		if _dragging != kind:
			_show_rate(kind, _pending[kind] if _pending[kind] >= 0 else _state_rates[kind])
	_update_hint()
	_budget_button.disabled = false
	if _budget_popup.visible:
		_fill_budget()
	_politics_button.visible = bool(state.get("politik", false))
	if _politics_popup.visible:
		if _politics_button.visible:
			_fill_politics()
		else:
			_politics_popup.hide()


func show_unavailable() -> void:
	_state = {}
	_coat.shield_color = UiPalette.INK_FAINT
	_name.text = "Kein Spielerstaat"
	_short.text = "–"
	for label: Label in [
		_ruler, _government, _capital, _locations, _population, _treasury, _income, _expenses,
		_balance, _debt
	]:
		label.text = "–"
	for kind in _sliders.size():
		_state_rates[kind] = -1
		_pending[kind] = -1
		_sliders[kind].editable = false
		_values[kind].text = "–"
	_tax_hint.text = "Ohne geladene Simulation nicht verfügbar."
	_budget_button.disabled = true
	if _budget_popup != null:
		_budget_popup.hide()
	if _politics_popup != null:
		_politics_button.visible = false
		_politics_popup.hide()


## Noch nicht bestätigte Regler vergessen (nach dem Laden eines Spielstands, seit M6).
func clear_pending() -> void:
	for kind in _pending.size():
		_pending[kind] = -1


## Ein Befehl wurde abgelehnt: Regler auf den Wert der Simulation zurücksetzen.
func reject_pending(kind: int, message: String) -> void:
	if kind < 0 or kind >= _sliders.size():
		return
	_pending[kind] = -1
	if _state_rates[kind] >= 0:
		_show_rate(kind, _state_rates[kind])
	_tax_hint.text = message


## Haushaltsfenster neben dem Panel öffnen.
func show_budget() -> void:
	if _state.is_empty():
		return
	_fill_budget()
	var rect := get_global_rect()
	_budget_popup.popup(Rect2i(Vector2i(rect.end.x + 8.0, rect.position.y), Vector2i(330, 0)))


## Fenster „Verfassung und Gruppen“ neben dem Panel öffnen (seit M8, nur in Szenarien mit Parlament).
func show_politics() -> void:
	if _state.is_empty() or not _politics_button.visible or not _fill_politics():
		return
	var rect := get_global_rect()
	_politics_popup.popup(Rect2i(Vector2i(rect.end.x + 8.0, rect.position.y), Vector2i(360, 0)))


## true, solange das Fenster „Verfassung und Gruppen“ offen ist (für Tests).
func is_politics_visible() -> bool:
	return _politics_popup != null and _politics_popup.visible


## Text des Fensters „Verfassung und Gruppen“, zeilenweise (für Tests).
func politics_text() -> String:
	var lines := PackedStringArray()
	var children := _politics_grid.get_children()
	for i in range(0, children.size() - 1, 2):
		lines.append("%s: %s" % [(children[i] as Label).text, (children[i + 1] as Label).text])
	for row in _politics_groups.get_children():
		var parts := PackedStringArray()
		for child in row.get_children():
			if child is Label:
				parts.append((child as Label).text)
		lines.append(" ".join(parts))
	return "\n".join(lines)


## true, solange das Haushaltsfenster offen ist (für Tests).
func is_budget_visible() -> bool:
	return _budget_popup != null and _budget_popup.visible


## Text des Haushaltsfensters, zeilenweise „Bezeichnung: Wert“ (für Tests).
func budget_text() -> String:
	var lines := PackedStringArray()
	var children := _budget_grid.get_children()
	for i in range(0, children.size() - 1, 2):
		lines.append("%s: %s" % [(children[i] as Label).text, (children[i + 1] as Label).text])
	return "\n".join(lines)


func _add_tax_row(kind: int) -> void:
	var row := HBoxContainer.new()
	row.tooltip_text = TAX_TOOLTIPS[kind]
	row.mouse_filter = Control.MOUSE_FILTER_PASS
	var caption := Label.new()
	caption.text = TAX_NAMES[kind]
	caption.theme_type_variation = &"MutedLabel"
	caption.custom_minimum_size = Vector2(96, 0)
	caption.mouse_filter = Control.MOUSE_FILTER_PASS
	caption.tooltip_text = TAX_TOOLTIPS[kind]
	var slider := HSlider.new()
	slider.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	slider.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	slider.custom_minimum_size = Vector2(0, 22)
	slider.focus_mode = Control.FOCUS_NONE
	slider.step = 1.0
	slider.max_value = 50.0
	slider.editable = false
	slider.tooltip_text = TAX_TOOLTIPS[kind]
	slider.value_changed.connect(_on_slider_changed.bind(kind))
	slider.drag_started.connect(_on_drag_started.bind(kind))
	slider.drag_ended.connect(_on_drag_ended.bind(kind))
	var value := Label.new()
	value.text = "–"
	value.theme_type_variation = &"ValueLabel"
	value.custom_minimum_size = Vector2(46, 0)
	value.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	row.add_child(caption)
	row.add_child(slider)
	row.add_child(value)
	_taxes.add_child(row)
	_sliders.append(slider)
	_values.append(value)


func _show_rate(kind: int, percent: int) -> void:
	_updating = true
	_sliders[kind].value = percent
	_updating = false
	_values[kind].text = GermanFormat.percent(percent) if percent >= 0 else "–"


func _update_hint() -> void:
	for kind in _sliders.size():
		if _pending[kind] >= 0 and _pending[kind] != _state_rates[kind]:
			_tax_hint.text = (
				"Neue %s gilt ab dem nächsten Tag (bisher %s)."
				% [TAX_NAMES[kind], GermanFormat.percent(_state_rates[kind])]
			)
			return
	_tax_hint.text = DEFAULT_HINT


func _on_slider_changed(value: float, kind: int) -> void:
	if _updating:
		return
	if _dragging == kind:
		_values[kind].text = GermanFormat.percent(roundi(value))
		return
	_request(kind, roundi(value))


func _on_drag_started(kind: int) -> void:
	_dragging = kind


func _on_drag_ended(value_changed: bool, kind: int) -> void:
	_dragging = -1
	if value_changed and _sliders[kind].editable:
		_request(kind, roundi(_sliders[kind].value))


func _request(kind: int, percent: int) -> void:
	_pending[kind] = percent
	_show_rate(kind, percent)
	_update_hint()
	tax_rate_requested.emit(kind, percent)


func _build_budget_popup() -> void:
	_budget_popup = PopupPanel.new()
	_budget_popup.name = "BudgetPopup"
	var box := VBoxContainer.new()
	var header := Label.new()
	header.text = "HAUSHALT"
	header.theme_type_variation = &"HeaderLabel"
	box.add_child(header)
	_budget_grid = GridContainer.new()
	_budget_grid.columns = 2
	_budget_grid.add_theme_constant_override(&"h_separation", 18)
	box.add_child(_budget_grid)
	var note := Label.new()
	note.text = "Beträge je Monat (Rechentag des Staates)."
	note.theme_type_variation = &"CaptionLabel"
	box.add_child(note)
	_budget_popup.add_child(box)
	add_child(_budget_popup)
	# Ein Popup ist ein eigenes Fenster und übernimmt den Panel-Stil des Themes nicht: Stil
	# des Staatspanels verwenden.
	_budget_popup.add_theme_stylebox_override(&"panel", get_theme_stylebox(&"panel"))


func _build_politics() -> void:
	_politics_button = Button.new()
	_politics_button.name = "PoliticsButton"
	_politics_button.text = "Verfassung und Gruppen …"
	_politics_button.focus_mode = Control.FOCUS_NONE
	_politics_button.tooltip_text = "Verfassung, Wahlrecht des Landtags und Interessengruppen des Staates"
	_politics_button.visible = false
	_politics_button.pressed.connect(show_politics)
	var parent := _budget_button.get_parent()
	parent.add_child(_politics_button)
	parent.move_child(_politics_button, _budget_button.get_index() + 1)
	_politics_popup = PopupPanel.new()
	_politics_popup.name = "PoliticsPopup"
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 6)
	var header := Label.new()
	header.text = "VERFASSUNG UND GRUPPEN"
	header.theme_type_variation = &"HeaderLabel"
	box.add_child(header)
	_politics_grid = GridContainer.new()
	_politics_grid.columns = 2
	_politics_grid.add_theme_constant_override(&"h_separation", 18)
	box.add_child(_politics_grid)
	var caption := Label.new()
	caption.text = "INTERESSENGRUPPEN (ANTEIL · EINFLUSS)"
	caption.theme_type_variation = &"CaptionLabel"
	box.add_child(caption)
	_politics_groups = VBoxContainer.new()
	_politics_groups.add_theme_constant_override(&"separation", 2)
	box.add_child(_politics_groups)
	var note := Label.new()
	note.text = (
		"Anteil: Mitglieder unter den Einwohnern ab 15 Jahren. Einfluss: Anteil der Mitglieder am "
		+ "Einkommen. Ein Block kann mehreren Gruppen angehören."
	)
	note.theme_type_variation = &"CaptionLabel"
	note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	note.custom_minimum_size = Vector2(320, 0)
	box.add_child(note)
	_politics_popup.add_child(box)
	add_child(_politics_popup)
	_politics_popup.add_theme_stylebox_override(&"panel", get_theme_stylebox(&"panel"))


## Füllt das Fenster aus politics_source; false ohne Angaben.
func _fill_politics() -> bool:
	var politics: Dictionary = politics_source.call() if politics_source.is_valid() else {}
	if politics.is_empty():
		return false
	for child in _politics_grid.get_children():
		_politics_grid.remove_child(child)
		child.queue_free()
	for child in _politics_groups.get_children():
		_politics_groups.remove_child(child)
		child.queue_free()
	_politics_line("Verfassung", str(politics.get("constitution", "–")))
	var franchise := str(politics.get("franchise", "–"))
	var threshold := float(politics.get("census_threshold", 0.0))
	if threshold > 0.0:
		franchise += " (ab %s Steuern im Jahr)" % GermanFormat.money(threshold)
	_politics_line("Wahlrecht (Landtag)", franchise)
	if bool(politics.get("elected", false)):
		var system := "absolute Mehrheit, Stichwahl" if bool(politics.get("runoff", true)) else "relative Mehrheit"
		if bool(politics.get("indirect", false)):
			system += ", über Wahlmänner"
		_politics_line("Wahlsystem", system)
	var groups: Array = politics.get("interest_groups", [])
	groups.sort_custom(func(a: Variant, b: Variant) -> bool:
		return float((a as Dictionary).get("influence_percent", 0.0)) > float(
			(b as Dictionary).get("influence_percent", 0.0)
		)
	)
	for entry: Variant in groups:
		var group := entry as Dictionary
		var row := HBoxContainer.new()
		row.add_theme_constant_override(&"separation", 8)
		var swatch := ColorRect.new()
		swatch.color = group.get("color", UiPalette.BAR_NEUTRAL)
		swatch.custom_minimum_size = Vector2(12, 12)
		swatch.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		row.add_child(swatch)
		var label := Label.new()
		label.text = str(group.get("name", ""))
		label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		row.add_child(label)
		var values := Label.new()
		values.text = "%s · %s" % [
			GermanFormat.percent(float(group.get("share_percent", 0.0)), 1),
			GermanFormat.percent(float(group.get("influence_percent", 0.0)), 1),
		]
		values.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		row.add_child(values)
		_politics_groups.add_child(row)
	return true


func _politics_line(caption: String, value: String) -> void:
	var left := Label.new()
	left.text = caption
	left.theme_type_variation = &"MutedLabel"
	var right := Label.new()
	right.text = value
	right.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_politics_grid.add_child(left)
	_politics_grid.add_child(right)


func _fill_budget() -> void:
	for child in _budget_grid.get_children():
		_budget_grid.remove_child(child)
		child.queue_free()
	var state := _state
	var rates: Array = state.get("tax_rates_percent", [])
	var taxes: Array = state.get("taxes_month", [])
	var income := 0.0
	_budget_line("Einnahmen", "", true)
	for kind in TAX_NAMES.size():
		var amount := float(taxes[kind]) if kind < taxes.size() else 0.0
		income += amount
		var rate := int(rates[kind]) if kind < rates.size() else 0
		_budget_line(
			"%s (%s)" % [TAX_NAMES[kind], GermanFormat.percent(rate)],
			GermanFormat.signed_money(amount)
		)
	_budget_line("Summe", GermanFormat.signed_money(income))
	var soldiers := int(state.get("soldiers", 0))
	var interest_percent := float(state.get("interest_percent", 0.0))
	var items: Array = [
		["Hof", "court_month"],
		["Verwaltung", "administration_month"],
		["Heer (%s Soldaten)" % GermanFormat.integer(soldiers), "army_month"],
		["Zinsen (%s)" % GermanFormat.percent(interest_percent, 1), "interest_month"],
		["Bauten (Landesausbau)", "building_month"],
	]
	var expenses := 0.0
	_budget_line("Ausgaben", "", true)
	for item: Array in items:
		var amount := float(state.get(str(item[1]), 0.0))
		expenses += amount
		_budget_line(str(item[0]), GermanFormat.signed_money(-amount))
	_budget_line("Summe", GermanFormat.signed_money(-expenses))
	_budget_line("Stand", "", true)
	_budget_line("Saldo", GermanFormat.signed_money(income - expenses))
	_budget_line("Schatz", GermanFormat.money(float(state.get("treasury", 0.0))))
	_budget_line("Schulden", GermanFormat.money(float(state.get("debt", 0.0))))
	_budget_line(
		"Jahreseinnahmen (gleitend)", GermanFormat.money(float(state.get("yearly_income", 0.0)))
	)
	var bankruptcies := int(state.get("bankruptcies", 0))
	var bankrupt_text := GermanFormat.integer(bankruptcies)
	var premium := int(state.get("premium_months", 0))
	if premium > 0:
		bankrupt_text += " (Zinsaufschlag noch %s Monate)" % GermanFormat.integer(premium)
	_budget_line("Staatsbankrotte", bankrupt_text)


func _budget_line(caption: String, value: String, header: bool = false) -> void:
	var left := Label.new()
	left.text = caption
	left.theme_type_variation = &"HeaderLabel" if header else &"MutedLabel"
	var right := Label.new()
	right.text = value
	right.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_budget_grid.add_child(left)
	_budget_grid.add_child(right)


## Tooltip zur Bevölkerung: Lebensstandard und Bilanz des Vorjahres (seit M4).
static func _population_tooltip(state: Dictionary, population: int) -> String:
	var lines := PackedStringArray()
	lines.append(
		"Lebensstandard: %s von 100"
		% GermanFormat.number(float(state.get("living_standard", 0.0)))
	)
	var balance: Dictionary = state.get("balance_previous", {})
	var births := int(balance.get("births", 0))
	if births == 0 or population <= 0:
		lines.append("Bilanz des Vorjahres: noch kein volles Jahr gerechnet.")
		return "\n".join(lines)
	var natural := births - int(balance.get("deaths", 0)) - int(balance.get("plague_deaths", 0))
	var migration := int(balance.get("moved_in", 0)) - int(balance.get("moved_out", 0))
	var emigrated := int(balance.get("emigrated", 0))
	var change := natural + migration - emigrated
	lines.append("Vorjahr: %s Einwohner (%s)" % [
		GermanFormat.signed_number(change),
		GermanFormat.signed_number(100.0 * change / population, 1) + " %"
	])
	lines.append("  Geburten minus Sterbefälle: %s" % GermanFormat.signed_number(natural))
	lines.append("  Wanderung zwischen Orten: %s" % GermanFormat.signed_number(migration))
	lines.append("  Auswanderung: %s" % GermanFormat.signed_number(-emigrated))
	return "\n".join(lines)
