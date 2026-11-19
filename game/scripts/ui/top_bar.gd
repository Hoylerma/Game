class_name TopBar
extends PanelContainer
## Obere Leiste: Spielerstaat, Kennzahlen, Datum und Geschwindigkeit. Seit M13a5c (vertical_slice.md 4.1)
## nach Kapitel 22: Kasse mit Saldo, Legitimität, Verwaltung und die fünf Dimensionen des Vermächtnisses; je
## Zahl ein verschachtelter Tooltip (TooltipLayer, main.gd verbindet ihn).
##
## Seit M14a2 (europa_und_oberflaeche.md 5): je Kennzahl ein Symbol statt einer Überschrift (der Name steht im
## Tooltip); neu Unruhe, Bevölkerung und Heer, die Verwaltung steht im Tooltip der Bevölkerung. Legitimität,
## Unruhe und Bevölkerung zeigen ihre Veränderung seit dem Monatsanfang mit Pfeil und Farbe.

## Geschwindigkeit gewählt (0 = Pause, 1–5).
signal speed_selected(speed: int)
## Knopf „Menü“ gedrückt (seit M6: Spielmenü mit Speichern und Laden).
signal menu_requested
## Klick auf Wappen oder Staatsnamen: Staatsübersicht öffnen oder schließen (seit M14a1).
signal state_requested

## Dauer einer kurzen Meldung unter dem Datum (z. B. „Schnellgespeichert“).
const NOTICE_SECONDS := 3.0
## Größe der Symbole der Kennzahlen.
const ICON_SIZE := 24.0
## Kleinste gezeigte Veränderung je Kennzahl (darunter kein Pfeil).
const MIN_CHANGE := {&"legitimitaet": 0.05, &"unruhe": 0.5, &"bevoelkerung": 1.0}

## Kennzahlen: Kennung → HBox mit Symbol und Werten (Tooltip-Ziel).
var _metrics := {}
## Veränderung je Kennzahl (Label) und Werte jetzt und am Monatsanfang.
var _changes := {}
var _now := {}
var _baseline := {}
var _baseline_month := -1
var _unrest: Label = null
var _population: Label = null
var _army: Label = null

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
	for control: Control in [_coat, _state_name.get_parent() as Control]:
		control.mouse_filter = Control.MOUSE_FILTER_STOP
		control.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND
		control.tooltip_text = tr("Staatsübersicht öffnen oder schließen")
		control.gui_input.connect(_on_state_input)
	_build_saldo()
	_build_metrics()
	show_unavailable()


func _on_state_input(event: InputEvent) -> void:
	var button := event as InputEventMouseButton
	if button != null and button.pressed and button.button_index == MOUSE_BUTTON_LEFT:
		state_requested.emit()
		accept_event()


## Kennzahlen mit Symbol: die Kästen der Szene ohne Überschrift, dazu Unruhe, Bevölkerung und Heer.
func _build_metrics() -> void:
	var row := _cash.get_parent().get_parent().get_parent() as HBoxContainer # Row/CashBox/CashRow/Cash
	var cash_box := _cash.get_parent().get_parent() as Control
	_wrap_metric(row, &"kasse", cash_box, tr("Kasse und Saldo"))
	var legitimacy_box := _legitimacy.get_parent() as Control
	_wrap_metric(row, &"legitimitaet", legitimacy_box, tr("Legitimität"))
	_unrest = _value_label("Unrest")
	_population = _value_label("Population")
	_army = _value_label("Army")
	var after := _metrics[&"legitimitaet"] as Control
	for entry: Array in [[&"unruhe", _unrest, tr("Unruhe")], [&"bevoelkerung", _population, tr("Bevölkerung und Verwaltung")], [&"heer", _army, tr("Heer")]]:
		var box := VBoxContainer.new()
		box.alignment = BoxContainer.ALIGNMENT_CENTER
		box.add_child(entry[1])
		row.add_child(box)
		row.move_child(box, after.get_index() + 1)
		after = _wrap_metric(row, entry[0], box, entry[2])
	var administration_box := _administration.get_parent() as Control
	administration_box.visible = false
	var legacy_box := _legacy.get_parent() as Control
	_wrap_metric(row, &"vermaechtnis", legacy_box, tr("Vermächtnis"))
	_legacy.add_theme_font_size_override(&"font_size", 15)
	# Nachgiebig (seit M14a2): Abstände kleiner, der Staatsname schrumpft mit Auslassungspunkten, die Zeile
	# unter dem Datum ebenso; so passt die Leiste mit den neuen Kennzahlen auch unter 1.600 Pixeln.
	row.add_theme_constant_override(&"separation", 10)
	var state_box := row.get_node("StateBox") as Control
	state_box.custom_minimum_size.x = 150.0
	state_box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	(row.get_node("Spacer") as Control).size_flags_stretch_ratio = 0.5
	_speed_text.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	_speed_text.clip_text = true
	var date_box := _date.get_parent() as Control
	date_box.custom_minimum_size.x = 195.0
	_speed_text.custom_minimum_size.x = 195.0
	_speed_text.size_flags_horizontal = Control.SIZE_FILL


## Kasten box in eine Zeile mit Symbol und Veränderung legen (an seinem Platz in row).
func _wrap_metric(row: HBoxContainer, key: StringName, box: Control, title: String) -> HBoxContainer:
	var metric := HBoxContainer.new()
	metric.name = "Metric_%s" % key
	metric.add_theme_constant_override(&"separation", 5)
	metric.mouse_filter = Control.MOUSE_FILTER_STOP
	metric.tooltip_text = title
	metric.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	row.add_child(metric)
	row.move_child(metric, box.get_index())
	var icon := TextureRect.new()
	icon.name = "Icon"
	icon.texture = Icons.texture(&"gesellschaft" if key == &"bevoelkerung" else key, ICON_SIZE)
	icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	icon.custom_minimum_size = Vector2(ICON_SIZE, ICON_SIZE)
	icon.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	metric.add_child(icon)
	box.reparent(metric)
	box.custom_minimum_size.x = 0.0
	box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	if box is BoxContainer:
		(box as BoxContainer).alignment = BoxContainer.ALIGNMENT_CENTER
	var caption := box.get_node_or_null("Caption") as Control
	if caption != null:
		caption.visible = false
	if MIN_CHANGE.has(key):
		var change := Label.new()
		change.name = "Change"
		change.add_theme_font_size_override(&"font_size", 13)
		change.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		change.visible = false
		metric.add_child(change)
		_changes[key] = change
	_metrics[key] = metric
	return metric


func _value_label(node_name: String) -> Label:
	var label := Label.new()
	label.name = node_name
	label.theme_type_variation = &"ValueLabel"
	label.text = "–"
	return label


## Neuer Monat (Jahr × 12 + Monat): Die Veränderungen zählen ab den jetzigen Werten.
func begin_month(month_key: int) -> void:
	if month_key == _baseline_month:
		return
	_baseline_month = month_key
	_baseline = _now.duplicate()
	_show_changes()


func _remember(key: StringName, value: float) -> void:
	_now[key] = value
	if not _baseline.has(key):
		_baseline[key] = value


## Pfeil und Betrag der Veränderung seit dem Monatsanfang; mehr Unruhe ist schlecht.
func _show_changes() -> void:
	for key: StringName in _changes:
		var label := _changes[key] as Label
		var change := float(_now.get(key, 0.0)) - float(_baseline.get(key, _now.get(key, 0.0)))
		if absf(change) < float(MIN_CHANGE[key]):
			label.visible = false
			continue
		label.visible = true
		var good := change > 0.0 if key != &"unruhe" else change < 0.0
		var amount := Format.number(absf(change), 1) if key == &"legitimitaet" else Format.integer(roundi(absf(change)))
		label.text = ("▲" if change > 0.0 else "▼") + amount
		label.add_theme_color_override(&"font_color", UiPalette.amount_color(1.0 if good else -1.0))


## Symbol einer Kennzahl in einer anderen Tinte (seit der Rückmeldung 11/2026: die Bevölkerung in der Farbe
## des Urteils „Wie geht es dem Volk?“).
func set_icon_ink(key: StringName, ink: Color) -> void:
	var metric := _metrics.get(key) as Control
	var icon := metric.find_child("Icon", false, false) as TextureRect if metric != null else null
	if icon != null:
		icon.texture = Icons.texture(&"gesellschaft" if key == &"bevoelkerung" else key, ICON_SIZE, ink)
		icon.set_meta(&"ink", ink)


## Tinte des Symbols einer Kennzahl (Tests).
func icon_ink(key: StringName) -> Color:
	var metric := _metrics.get(key) as Control
	var icon := metric.find_child("Icon", false, false) as TextureRect if metric != null else null
	return icon.get_meta(&"ink", UiPalette.INK) if icon != null else UiPalette.INK


## Veränderung einer Kennzahl seit dem Monatsanfang (Tests).
func change_of(key: StringName) -> float:
	return float(_now.get(key, 0.0)) - float(_baseline.get(key, _now.get(key, 0.0)))


## Kasse und Saldo nebeneinander, damit jeder Wert seine Farbe bekommt.
func _build_saldo() -> void:
	var box := _cash.get_parent()
	var row := HBoxContainer.new()
	row.name = "CashRow"
	row.add_theme_constant_override(&"separation", 5)
	row.mouse_filter = Control.MOUSE_FILTER_IGNORE
	box.add_child(row)
	box.move_child(row, _cash.get_index())
	_cash.reparent(row)
	# Seit M14a2 kleiner direkt neben der Kasse, wie die Veränderungen der übrigen Kennzahlen.
	_saldo = Label.new()
	_saldo.name = "Saldo"
	_saldo.add_theme_font_size_override(&"font_size", 14)
	_saldo.size_flags_vertical = Control.SIZE_SHRINK_CENTER
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
	var population := float(state.get("population", 0))
	_population.text = Format.compact(population)
	_remember(&"bevoelkerung", population)
	var military := bool(state.get("military", false))
	var soldiers := int(state.get("soldiers_under_arms", state.get("soldiers", 0))) if military else int(state.get("soldiers", 0))
	var phase := int(state.get("mobilization", 0)) if military else 0
	# Mobilmachung als Goldfarbe; die Phase steht im Tooltip (BarTooltips.army).
	_army.text = Format.integer(soldiers)
	_army.add_theme_color_override(&"font_color", UiPalette.GOLD if phase != 0 else UiPalette.INK)
	_show_changes()


## Kennzahlen aus get_state_overview und get_legacy (seit M13a5c; main.gd höchstens zweimal je Sekunde).
func show_overview(overview: Dictionary, legacy: Dictionary) -> void:
	if overview.is_empty():
		return
	if bool(overview.get("events", false)):
		var value := float(overview.get("legitimacy", 0.0))
		_legitimacy.text = Format.signed_number(value, 1)
		_legitimacy.add_theme_color_override(&"font_color", UiPalette.amount_color(value if absf(value) > 0.5 else 0.0))
		_remember(&"legitimitaet", value)
		var unrest := float(overview.get("unrest", 0.0))
		_unrest.text = Format.integer(roundi(unrest))
		_unrest.add_theme_color_override(&"font_color", UiPalette.NEGATIVE if unrest >= 30.0 else (UiPalette.GOLD if unrest >= 10.0 else UiPalette.INK))
		_remember(&"unruhe", unrest)
	else:
		_legitimacy.text = "–"
		_unrest.text = "–"
	_administration.text = "%s Orte" % Format.integer(int(overview.get("locations", 0)))
	var parts := PackedStringArray()
	for dimension: Dictionary in legacy.get("dimensions", []):
		parts.append("%s%d" % [str(dimension.get("name", "")).left(1), roundi(float(dimension.get("value", 0.0)))])
	_legacy.text = " ".join(parts) if not parts.is_empty() else "–"
	_show_changes()


## Felder für die Tooltips (main.gd): kasse, legitimitaet, unruhe, bevoelkerung (mit der Verwaltung, seit
## M14a2; „verwaltung“ gilt weiter), heer, vermaechtnis.
func field(key: StringName) -> Control:
	if key == &"verwaltung":
		key = &"bevoelkerung"
	return _metrics.get(key) as Control


## Texte der Kennzahlen (Tests).
func field_text(key: StringName) -> String:
	match key:
		&"kasse":
			return "%s · %s" % [_cash.text, _saldo.text]
		&"legitimitaet":
			return _legitimacy.text
		&"verwaltung":
			return _administration.text
		&"unruhe":
			return _unrest.text
		&"bevoelkerung":
			return _population.text
		&"heer":
			return _army.text
		&"vermaechtnis":
			return _legacy.text
	return ""


## Leere Anzeige, solange keine Simulation läuft.
func show_unavailable() -> void:
	_coat.shield_color = UiPalette.INK_FAINT
	_state_name.text = "Keine Simulation"
	_state_detail.text = "Spielerstaat unbekannt"
	for label: Label in [_cash, _legitimacy, _administration, _legacy, _unrest, _population, _army]:
		if label != null:
			label.text = "–"
			label.remove_theme_color_override(&"font_color")
	_now.clear()
	_baseline.clear()
	_baseline_month = -1
	for change: Label in _changes.values():
		change.visible = false
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
