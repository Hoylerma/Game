class_name MapModeBar
extends PanelContainer
## Leiste der Kartenmodi (Tasten Q W E R T Z U I). Die Knöpfe zeigen Kurznamen; Taste und
## Erklärung stehen im Tooltip.

## Kartenmodus gewählt (MapModes).
signal mode_selected(mode: int)

var _buttons: Array[Button] = []

@onready var _row: HBoxContainer = %Buttons


func _ready() -> void:
	var group := ButtonGroup.new()
	group.allow_unpress = false
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
		button.pressed.connect(_on_button_pressed.bind(mode))
		_row.add_child(button)
		_buttons.append(button)


## Aktiven Modus markieren, ohne das Signal auszulösen.
func show_mode(mode: int) -> void:
	for i in _buttons.size():
		_buttons[i].set_pressed_no_signal(i == mode)


func _on_button_pressed(mode: int) -> void:
	mode_selected.emit(mode)
