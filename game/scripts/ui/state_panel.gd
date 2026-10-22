class_name StatePanel
extends PanelContainer
## Staatspanel des Spielerstaates mit Reglern für Kontribution, Akzise und Zoll und einem
## Haushaltsfenster (seit M5), seit M9 mit dem Fenster „Zollunion“.
##
## Ein Regler schickt einen Befehl an die Simulation; wirksam wird er erst am nächsten
## simulierten Tag. Bis dahin zeigt das Panel den Wunschwert an. Beim Ziehen geht der Befehl
## erst beim Loslassen hinaus – sonst würde jeder Zwischenschritt als eigener Befehl (und
## Chronikeintrag) ausgeführt.

## Neuer Satz in Prozent für eine Steuerart (SimulationClient.TAX_*) gewünscht.
signal tax_rate_requested(kind: int, percent: int)
## Wechsel der Zollunion gewünscht (seit M9): Index der Union, -1 für den Austritt. Die Union, der
## der Staat schon angehört, zieht einen offenen Antrag zurück.
signal customs_change_requested(customs_union: int)

## Namen der Steuerarten, Index = SimulationClient.TAX_*.
const TAX_NAMES: PackedStringArray = ["Kontribution", "Akzise", "Zoll"]
const TAX_TOOLTIPS: PackedStringArray = [
	"Kontribution: direkte Steuer auf die Einkommen auf dem Land und auf Agrareinkommen in Städten",
	"Akzise: Verbrauchssteuer auf alle Käufe in Städten",
	"Zoll: Abgabe auf eingeführte Waren (nach ihrem Anteil am Angebot des Marktes)",
]
## Kürzester Abstand zwischen zwei Füllungen des offenen Fensters „Verfassung und Gruppen“.
const POLITICS_REFRESH_MS := 1000
const DEFAULT_HINT := "Höhere Steuern füllen die Kasse, verteuern aber Waren und bremsen das Wachstum."
## Kein Ziel gewählt bzw. kein Antrag unterwegs (Zollunion; -1 ist der Austritt).
const NO_CUSTOMS_CHOICE := -2

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
var _politics_filled_at := 0
## Zollunion (seit M9): Knopf und Fenster, nur in Szenarien mit Zollunionen (set_customs_available).
## main.gd liefert die Unionen über customs_source (SimulationClient.get_customs_unions) und die
## Vorschau eines Wechsels über customs_preview (get_customs_change des Spielerstaates; Argument
## Union oder -1).
var customs_source: Callable = Callable()
var customs_preview: Callable = Callable()
var _customs_available := false
var _customs_button: Button = null
var _customs_popup: PopupPanel = null
var _customs_grid: GridContainer = null
var _customs_rows: VBoxContainer = null
var _customs_withdraw: Button = null
var _customs_question_box: VBoxContainer = null
var _customs_question: Label = null
var _customs_note: Label = null
var _customs_choice := NO_CUSTOMS_CHOICE
var _customs_sent := NO_CUSTOMS_CHOICE
var _customs_filled_at := 0
var _tax_rows: Array[HBoxContainer] = []

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
	_build_customs()
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
	# Mitgliedschaften (seit M7; Zollunion seit M9).
	var member := PackedStringArray()
	if bool(state.get("bund", false)):
		member.append("Deutscher Bund")
	var customs_union := str(state.get("customs_union_name", ""))
	if not customs_union.is_empty():
		member.append(customs_union)
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
	# Mitglieder einer Zollunion erheben den Vereinstarif: Zollregler gesperrt (seit M9).
	var locked := bool(state.get("customs_locked", false))
	for kind in _sliders.size():
		_state_rates[kind] = int(rates[kind]) if kind < rates.size() else -1
		var fixed := locked and kind == SimulationClient.TAX_ZOLL
		if fixed:
			_state_rates[kind] = roundi(float(state.get("customs_tariff_percent", 0.0)))
			_pending[kind] = -1
		if _pending[kind] == _state_rates[kind]:
			_pending[kind] = -1
		_sliders[kind].editable = _state_rates[kind] >= 0 and not fixed
		if _dragging != kind:
			_show_rate(kind, _pending[kind] if _pending[kind] >= 0 else _state_rates[kind])
	_set_tax_tooltip(
		SimulationClient.TAX_ZOLL,
		(
			"Zoll: Tarif der Zollunion (%s). Mitglieder erheben keinen eigenen Zoll; sie erhalten "
			% str(state.get("customs_union_name", ""))
			+ "einen Anteil an der Vereinskasse nach Einwohnern."
		) if locked else TAX_TOOLTIPS[SimulationClient.TAX_ZOLL]
	)
	_update_customs(state)
	_update_hint()
	_budget_button.disabled = false
	if _budget_popup.visible:
		_fill_budget()
	_politics_button.visible = bool(state.get("politik", false))
	if _politics_popup.visible:
		if not _politics_button.visible:
			_politics_popup.hide()
		elif Time.get_ticks_msec() - _politics_filled_at >= POLITICS_REFRESH_MS:
			# Anteil und Einfluss rechnen über alle Blöcke des Staates: höchstens einmal je Sekunde.
			_fill_politics()


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
	if _customs_popup != null:
		_customs_button.visible = false
		_customs_popup.hide()
	if _politics_popup != null:
		_politics_button.visible = false
		_politics_popup.hide()


## Noch nicht bestätigte Regler vergessen (nach dem Laden eines Spielstands, seit M6).
func clear_pending() -> void:
	for kind in _pending.size():
		_pending[kind] = -1
	_customs_sent = NO_CUSTOMS_CHOICE
	_customs_choice = NO_CUSTOMS_CHOICE
	if _customs_note != null:
		_customs_note.text = ""


## Szenario mit Zollunionen (seit M9): Knopf „Zollunion …“ zeigen.
func set_customs_available(available: bool) -> void:
	_customs_available = available
	if not available and _customs_popup != null:
		_customs_popup.hide()


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


## Fenster „Zollunion“ neben dem Panel öffnen (seit M9, nur in Szenarien mit Zollunionen).
func show_customs() -> void:
	if _state.is_empty() or not _customs_button.visible:
		return
	_customs_choice = NO_CUSTOMS_CHOICE
	_customs_question_box.visible = false
	if _customs_sent == NO_CUSTOMS_CHOICE:
		_customs_note.text = ""
	_fill_customs()
	var rect := get_global_rect()
	_customs_popup.popup(Rect2i(Vector2i(rect.end.x + 8.0, rect.position.y), Vector2i(380, 0)))


## Haushalts-, Verfassungs- und Zollfenster schließen (Wahlabend, Parteienmodus).
func hide_popups() -> void:
	if _budget_popup != null:
		_budget_popup.hide()
	if _politics_popup != null:
		_politics_popup.hide()
	if _customs_popup != null:
		_customs_popup.hide()


## true, solange das Fenster „Zollunion“ offen ist (für Tests).
func is_customs_visible() -> bool:
	return _customs_popup != null and _customs_popup.visible


## Text des Fensters „Zollunion“ mit Unionen und offener Rückfrage, zeilenweise (für Tests).
func customs_text() -> String:
	var lines := PackedStringArray()
	var children := _customs_grid.get_children()
	for i in range(0, children.size() - 1, 2):
		lines.append("%s: %s" % [(children[i] as Label).text, (children[i + 1] as Label).text])
	for row in _customs_rows.get_children():
		var parts := PackedStringArray()
		for child in row.find_children("*", "", true, false):
			if child is Label:
				parts.append((child as Label).text)
			elif child is Button:
				parts.append("[%s]" % (child as Button).text)
		lines.append(" ".join(parts))
	if _customs_question_box.visible:
		lines.append(_customs_question.text)
	if _customs_note.visible:
		lines.append(_customs_note.text)
	return "\n".join(lines)


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
	row.name = "Tax%d" % kind
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
	_tax_rows.append(row)


func _set_tax_tooltip(kind: int, text: String) -> void:
	var row := _tax_rows[kind]
	if row.tooltip_text == text:
		return
	row.tooltip_text = text
	for child in row.get_children():
		(child as Control).tooltip_text = text


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
	# Beantragter Wechsel der Zollunion (seit M9).
	var request: Dictionary = _state.get("customs_request", {})
	if not request.is_empty():
		_tax_hint.text = _request_text(request) + "."
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
	_politics_filled_at = Time.get_ticks_msec()
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


func _build_customs() -> void:
	_customs_button = Button.new()
	_customs_button.name = "CustomsButton"
	_customs_button.text = "Zollunion …"
	_customs_button.focus_mode = Control.FOCUS_NONE
	_customs_button.tooltip_text = "Zollunionen; Beitritt, Wechsel und Austritt zum nächsten 1. Januar"
	_customs_button.visible = false
	_customs_button.pressed.connect(show_customs)
	var parent := _budget_button.get_parent()
	parent.add_child(_customs_button)
	parent.move_child(_customs_button, _politics_button.get_index() + 1)
	_customs_popup = PopupPanel.new()
	_customs_popup.name = "CustomsPopup"
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 6)
	var header := Label.new()
	header.text = "ZOLLUNION"
	header.theme_type_variation = &"HeaderLabel"
	box.add_child(header)
	_customs_grid = GridContainer.new()
	_customs_grid.columns = 2
	_customs_grid.add_theme_constant_override(&"h_separation", 18)
	box.add_child(_customs_grid)
	var caption := Label.new()
	caption.text = "UNIONEN (MITGLIEDER · TARIF · KASSE JE MONAT)"
	caption.theme_type_variation = &"CaptionLabel"
	box.add_child(caption)
	_customs_rows = VBoxContainer.new()
	_customs_rows.add_theme_constant_override(&"separation", 4)
	box.add_child(_customs_rows)
	_customs_withdraw = Button.new()
	_customs_withdraw.name = "CustomsWithdraw"
	_customs_withdraw.text = "Antrag zurückziehen …"
	_customs_withdraw.focus_mode = Control.FOCUS_NONE
	_customs_withdraw.pressed.connect(func() -> void:
		_ask_customs(int(_state.get("customs_union_index", -1)))
	)
	box.add_child(_customs_withdraw)
	# Rückfrage mit Zeitpunkt (diplomatie.md, Abschnitt 3.7).
	_customs_question_box = VBoxContainer.new()
	_customs_question_box.name = "CustomsQuestion"
	_customs_question_box.visible = false
	_customs_question = Label.new()
	_customs_question.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_customs_question.custom_minimum_size = Vector2(340, 0)
	_customs_question_box.add_child(_customs_question)
	var answers := HBoxContainer.new()
	answers.alignment = BoxContainer.ALIGNMENT_END
	var confirm := Button.new()
	confirm.name = "CustomsConfirm"
	confirm.text = "Antrag stellen"
	confirm.focus_mode = Control.FOCUS_NONE
	confirm.pressed.connect(_confirm_customs)
	var cancel := Button.new()
	cancel.name = "CustomsCancel"
	cancel.text = "Abbrechen"
	cancel.focus_mode = Control.FOCUS_NONE
	cancel.pressed.connect(func() -> void:
		_customs_choice = NO_CUSTOMS_CHOICE
		_customs_question_box.visible = false
	)
	answers.add_child(cancel)
	answers.add_child(confirm)
	_customs_question_box.add_child(answers)
	box.add_child(_customs_question_box)
	_customs_note = Label.new()
	_customs_note.theme_type_variation = &"MutedLabel"
	_customs_note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_customs_note.custom_minimum_size = Vector2(340, 0)
	_customs_note.visible = false
	box.add_child(_customs_note)
	var note := Label.new()
	note.text = (
		"Wechsel werden zum nächsten 1. Januar wirksam. Mitglieder erheben den Tarif der Union; "
		+ "die Vereinskasse wird nach Einwohnern verteilt. Über einen Beitritt entscheidet die "
		+ "Leitmacht; tritt sie aus, löst sich ihre Union auf."
	)
	note.theme_type_variation = &"CaptionLabel"
	note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	note.custom_minimum_size = Vector2(340, 0)
	box.add_child(note)
	_customs_popup.add_child(box)
	add_child(_customs_popup)
	_customs_popup.add_theme_stylebox_override(&"panel", get_theme_stylebox(&"panel"))


func _update_customs(state: Dictionary) -> void:
	_customs_button.visible = _customs_available
	if not _customs_available:
		_customs_popup.hide()
		return
	# Ein gestellter Antrag ist angekommen, sobald die Simulation ihn zeigt (oder, bei einer
	# Rücknahme, keinen mehr).
	if _customs_sent != NO_CUSTOMS_CHOICE:
		var request: Dictionary = state.get("customs_request", {})
		var shown := int(state.get("customs_union_index", -1))
		if not request.is_empty():
			shown = int(request.get("customs_union_index", -1))
		if shown == _customs_sent:
			_customs_sent = NO_CUSTOMS_CHOICE
			_customs_note.text = ""
			_customs_filled_at = 0
	if _customs_popup.visible and Time.get_ticks_msec() - _customs_filled_at >= POLITICS_REFRESH_MS:
		_fill_customs()


## Füllt das Fenster „Zollunion“ aus customs_source und customs_preview.
func _fill_customs() -> void:
	_customs_filled_at = Time.get_ticks_msec()
	var unions: Array = customs_source.call() if customs_source.is_valid() else []
	var state := _state
	var current := int(state.get("customs_union_index", -1))
	var request: Dictionary = state.get("customs_request", {})
	var requested := (
		int(request.get("customs_union_index", -1)) if not request.is_empty() else NO_CUSTOMS_CHOICE
	)
	for child in _customs_grid.get_children():
		_customs_grid.remove_child(child)
		child.queue_free()
	var tariff := GermanFormat.percent(roundi(float(state.get("customs_tariff_percent", 0.0))))
	if current >= 0 and current < unions.size():
		var union: Dictionary = unions[current]
		var leader := str(union.get("leader_name", ""))
		var leading := bool(state.get("customs_leader", false))
		_customs_line(
			"Zollgebiet",
			"%s (%s)" % [
				str(union.get("name", "")), "Leitmacht" if leading else "Leitmacht " + leader
			]
		)
		_customs_line("Zoll", "%s, Tarif der Union" % tariff)
		_customs_line(
			"Anteil an der Vereinskasse",
			GermanFormat.signed_money(float(state.get("customs_income", 0.0))) + " je Monat"
		)
	else:
		_customs_line("Zollgebiet", "eigenes")
		_customs_line("Zoll", "%s, eigener Satz" % tariff)
	if not request.is_empty():
		_customs_line("Antrag", _request_text(request))
	while _customs_rows.get_child_count() < unions.size():
		_add_customs_row(_customs_rows.get_child_count())
	for i in _customs_rows.get_child_count():
		_customs_rows.get_child(i).visible = i < unions.size()
	for u in unions.size():
		_fill_customs_row(u, unions[u], current, requested)
	_customs_withdraw.visible = not request.is_empty()
	_customs_withdraw.disabled = _customs_sent != NO_CUSTOMS_CHOICE
	if _customs_sent != NO_CUSTOMS_CHOICE:
		_customs_note.text = "Der Antrag geht mit dem nächsten Tag hinaus."
	_customs_note.visible = not _customs_note.text.is_empty()


func _add_customs_row(index: int) -> void:
	var row := HBoxContainer.new()
	row.name = "Union%d" % index
	row.add_theme_constant_override(&"separation", 8)
	var swatch := ColorRect.new()
	swatch.name = "Swatch"
	swatch.custom_minimum_size = Vector2(12, 12)
	swatch.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	row.add_child(swatch)
	var texts := VBoxContainer.new()
	texts.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	texts.add_theme_constant_override(&"separation", 0)
	var name_label := Label.new()
	name_label.name = "Name"
	texts.add_child(name_label)
	var values := Label.new()
	values.name = "Values"
	values.theme_type_variation = &"MutedLabel"
	texts.add_child(values)
	row.add_child(texts)
	var action := Button.new()
	action.name = "CustomsAction%d" % index
	action.focus_mode = Control.FOCUS_NONE
	action.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	action.pressed.connect(func() -> void: _ask_customs(int(action.get_meta(&"target", -1))))
	row.add_child(action)
	_customs_rows.add_child(row)


func _fill_customs_row(index: int, union: Dictionary, current: int, requested: int) -> void:
	var row := _customs_rows.get_child(index)
	(row.get_node("Swatch") as ColorRect).color = union.get("color", UiPalette.BAR_NEUTRAL)
	var leader := str(union.get("leader_name", ""))
	(row.find_child("Name", true, false) as Label).text = "%s (Leitmacht %s)" % [
		str(union.get("name", "")), leader
	]
	var dissolved := bool(union.get("dissolved", false))
	(row.find_child("Values", true, false) as Label).text = (
		"aufgelöst" if dissolved
		else "%s · %s · %s" % [
			GermanFormat.integer(int(union.get("member_count", 0))),
			GermanFormat.percent(roundi(float(union.get("tariff_percent", 0.0)))),
			GermanFormat.money(float(union.get("revenue_month", 0.0))),
		]
	)
	var action := row.find_child("CustomsAction%d" % index, true, false) as Button
	var target := -1 if index == current else index
	action.set_meta(&"target", target)
	var preview: Dictionary = customs_preview.call(target) if customs_preview.is_valid() else {}
	var decision := str(preview.get("decision", "ungueltig"))
	action.text = "Austreten …" if target < 0 else "Beitreten …"
	action.tooltip_text = ""
	if target == requested or target == _customs_sent:
		action.text = "beantragt"
	elif decision == "abgelehnt":
		action.text = "%s lehnt ab" % leader
		action.tooltip_text = "%s lässt %s nicht in den %s." % [
			leader, str(_state.get("short_name", "")), str(union.get("name", ""))
		]
	elif dissolved and decision != "angenommen":
		action.text = "aufgelöst"
		action.tooltip_text = "Einer aufgelösten Union kann nur ihre frühere Leitmacht wieder beitreten."
	action.disabled = (
		decision != "angenommen" or target == requested or _customs_sent != NO_CUSTOMS_CHOICE
	)


func _customs_line(caption: String, value: String) -> void:
	var left := Label.new()
	left.text = caption
	left.theme_type_variation = &"MutedLabel"
	var right := Label.new()
	right.text = value
	right.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_customs_grid.add_child(left)
	_customs_grid.add_child(right)


## Rückfrage zu einem Wechsel (Ziel: Union oder -1; die eigene Union zieht den Antrag zurück).
func _ask_customs(target: int) -> void:
	var preview: Dictionary = customs_preview.call(target) if customs_preview.is_valid() else {}
	var decision := str(preview.get("decision", "ungueltig"))
	if decision != "angenommen" and decision != "zurueckgezogen":
		return
	_customs_choice = target
	_customs_question.text = _question_text(preview, decision, target)
	_customs_question_box.visible = true
	_customs_note.text = ""
	_customs_note.visible = false


func _question_text(preview: Dictionary, decision: String, target: int) -> String:
	var state_name := str(_state.get("short_name", ""))
	var when := str(preview.get("effective_text", ""))
	var current := str(preview.get("current_name", ""))
	var joined := str(preview.get("target_name", ""))
	if decision == "zurueckgezogen":
		return "Den Antrag zurückziehen? %s bleibt %s." % [
			state_name, "im " + current if not current.is_empty() else "ein eigenes Zollgebiet"
		]
	var text := ""
	if joined.is_empty():
		text = "%s tritt zum %s aus dem %s aus." % [state_name, when, current]
	elif current.is_empty():
		text = "%s tritt zum %s dem %s bei." % [state_name, when, joined]
	else:
		text = "%s verlässt zum %s den %s und tritt dem %s bei." % [state_name, when, current, joined]
	if bool(preview.get("dissolves", false)):
		text += " Ohne seine Leitmacht löst sich der %s dann auf; seine Mitglieder werden eigene Zollgebiete." % current
	if joined.is_empty():
		text += " Danach erhebt %s wieder einen eigenen Zoll." % state_name
	else:
		var unions: Array = customs_source.call() if customs_source.is_valid() else []
		var tariff := float((unions[target] as Dictionary).get("tariff_percent", 0.0)) if target < unions.size() else 0.0
		text += " Danach gilt der Tarif der Union (%s), und %s erhält einen Anteil an der Vereinskasse." % [
			GermanFormat.percent(roundi(tariff)), state_name
		]
	return text


func _confirm_customs() -> void:
	if _customs_choice == NO_CUSTOMS_CHOICE:
		return
	_customs_sent = _customs_choice
	_customs_choice = NO_CUSTOMS_CHOICE
	_customs_question_box.visible = false
	customs_change_requested.emit(_customs_sent)
	if _customs_popup.visible:
		_fill_customs()


## Ein Antrag wurde nicht angenommen (main.gd): Knöpfe wieder frei, Meldung im Fenster.
func reject_customs(message: String) -> void:
	_customs_sent = NO_CUSTOMS_CHOICE
	_customs_note.text = message
	_customs_note.visible = true
	if _customs_popup.visible:
		_fill_customs()


## „Beitritt zum Zollverein beantragt, wirksam zum 1. Januar 1849“ (customs_request aus get_state).
func _request_text(request: Dictionary) -> String:
	var joined := str(request.get("customs_union_name", ""))
	var when := str(request.get("effective_text", ""))
	if joined.is_empty():
		var left := str(_state.get("customs_union_name", ""))
		return "Austritt aus dem %s beantragt, wirksam zum %s" % [left, when]
	return "Beitritt zum %s beantragt, wirksam zum %s" % [joined, when]


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
		var caption := "%s (%s)" % [TAX_NAMES[kind], GermanFormat.percent(rate)]
		# Mitglieder einer Zollunion erhalten ihren Anteil an der Vereinskasse (seit M9).
		if kind == SimulationClient.TAX_ZOLL and bool(state.get("customs_locked", false)):
			caption = "Zoll (Vereinskasse, Tarif %s)" % GermanFormat.percent(
				roundi(float(state.get("customs_tariff_percent", 0.0)))
			)
		_budget_line(caption, GermanFormat.signed_money(amount))
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
