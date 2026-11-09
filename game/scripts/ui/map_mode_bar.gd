class_name MapModeBar
extends PanelContainer
## Leiste der Kartenmodi (Tasten Q W E R T Z U I O P Ü + Ä Ö #). Die Knöpfe zeigen Kurznamen; Taste und
## Erklärung stehen im Tooltip. Seit M5 schmalere Knöpfe ohne Titel „Karte“, seit M7 mit zwölf
## Modi noch schmaler, seit M8 mit dreizehn in kleinerer Schrift, seit M9 mit fünfzehn noch kleiner,
## damit die Leiste bei 1.600 Pixeln Fensterbreite zwischen die Spalten passt.

## Kartenmodus gewählt (MapModes).
signal mode_selected(mode: int)

## Innenabstand links und rechts je Knopf (Theme: 10).
const BUTTON_MARGIN := 2.0
## Abstand zwischen den Knöpfen (seit M9 mit fünfzehn Modi).
const BUTTON_SEPARATION := 1
## Schriftgröße der Knöpfe (Theme: 14; seit M9 mit fünfzehn Modi 12).
const BUTTON_FONT_SIZE := 12
const BUTTON_STYLES: Array[StringName] = [
	&"normal", &"hover", &"pressed", &"hover_pressed", &"disabled"
]

var _buttons: Array[Button] = []
var _rows: Array[HBoxContainer] = []
## Seit M13a5a: mit achtzehn Modi passt eine Reihe bei 1.600 Pixeln nicht mehr zwischen die Spalten; dann
## brechen die Knöpfe in zwei Reihen um (set_rows, main.gd).
var _row_count := 1

@onready var _box: VBoxContainer = %Buttons


func _ready() -> void:
	var group := ButtonGroup.new()
	group.allow_unpress = false
	var first := _add_row()
	for mode in MapModes.COUNT:
		var button := Button.new()
		button.text = MapModes.SHORT_NAMES[mode]
		button.tooltip_text = "%s: %s (Taste %s)" % [
			MapModes.NAMES[mode], MapModes.DESCRIPTIONS[mode], MapModes.KEY_HINTS[mode]
		]
		button.toggle_mode = true
		button.button_group = group
		button.focus_mode = Control.FOCUS_NONE
		button.custom_minimum_size = Vector2(0, 32)
		button.add_theme_font_size_override(&"font_size", BUTTON_FONT_SIZE)
		button.pressed.connect(_on_button_pressed.bind(mode))
		first.add_child(button)
		_compact(button)
		_buttons.append(button)


func _add_row() -> HBoxContainer:
	var row := HBoxContainer.new()
	row.add_theme_constant_override(&"separation", BUTTON_SEPARATION)
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	_box.add_child(row)
	_rows.append(row)
	return row


## Knöpfe auf rows Reihen verteilen (1 oder 2), gleich viele je Reihe.
func set_rows(rows: int) -> void:
	rows = clampi(rows, 1, 2)
	if rows == _row_count:
		return
	_row_count = rows
	while _rows.size() < rows:
		_add_row()
	var per_row := ceili(float(_buttons.size()) / float(rows))
	for i in _buttons.size():
		var target := _rows[mini(floori(float(i) / float(per_row)), rows - 1)]
		if _buttons[i].get_parent() != target:
			_buttons[i].reparent(target)
	for r in _rows.size():
		_rows[r].visible = r < rows


func row_count() -> int:
	return _row_count


## Aktiven Modus markieren, ohne das Signal auszulösen.
func show_mode(mode: int) -> void:
	for i in _buttons.size():
		_buttons[i].set_pressed_no_signal(i == mode)


func _on_button_pressed(mode: int) -> void:
	mode_selected.emit(mode)


## Schmalere Knöpfe: Stile des Themes mit kleinerem Innenabstand links und rechts.
static func _compact(button: Button) -> void:
	for style_name in BUTTON_STYLES:
		var style := button.get_theme_stylebox(style_name)
		if style == null:
			continue
		var compact := style.duplicate() as StyleBox
		compact.content_margin_left = BUTTON_MARGIN
		compact.content_margin_right = BUTTON_MARGIN
		button.add_theme_stylebox_override(style_name, compact)
