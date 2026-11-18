class_name MapModeBar
extends PanelContainer
## Leiste der Kartenmodi (Tasten Q W E R T Z U I O P Ü + Ä Ö #). Die Knöpfe zeigen Kurznamen; Taste und
## Erklärung stehen im Tooltip. Seit M5 schmalere Knöpfe ohne Titel „Karte“, seit M7 mit zwölf
## Modi noch schmaler, seit M8 mit dreizehn in kleinerer Schrift, seit M9 mit fünfzehn noch kleiner,
## damit die Leiste bei 1.600 Pixeln Fensterbreite zwischen die Spalten passt. Seit M14a2 eine Reihe Symbole
## in vier Gruppen (Politik, Gesellschaft, Wirtschaft, Land und Krieg); Name, Erklärung und Taste im Tooltip.

## Kartenmodus gewählt (MapModes).
signal mode_selected(mode: int)

## Abstand zwischen den Knöpfen (seit M9 mit fünfzehn Modi).
const BUTTON_SEPARATION := 1

## Gruppen der Leiste in der Reihenfolge der Anzeige (Kartenmodi aus MapModes). Bewusst untypisiert: Eine
## Konstante vom Typ Array[PackedInt32Array] aus Array-Literalen liefert in Godot 4.4 leere Gruppen (size() 0).
const GROUPS: Array = [
	[MapModes.MAP_POLITICAL, MapModes.MAP_BUND, MapModes.MAP_BEZIEHUNGEN, MapModes.MAP_WAHLEN,
		MapModes.MAP_VERWALTUNG, MapModes.MAP_KREISE],
	[MapModes.MAP_DENSITY, MapModes.MAP_CONFESSION, MapModes.MAP_KULTUR, MapModes.MAP_LEBENSSTANDARD,
		MapModes.MAP_UNRUHE],
	[MapModes.MAP_WIRTSCHAFT, MapModes.MAP_MAERKTE, MapModes.MAP_ZOLLGEBIETE, MapModes.MAP_EISENBAHNEN],
	[MapModes.MAP_TERRAIN, MapModes.MAP_DEVASTATION, MapModes.MAP_KRIEG],
]
## Symbol je Kartenmodus (Index wie MapModes).
const ICONS: Array[StringName] = [
	&"politisch", &"dichte", &"konfession", &"gelaende", &"verwuestung", &"kreise", &"lebensstandard",
	&"kultur", &"wirtschaft", &"maerkte", &"verwaltung", &"bahn", &"wahlen", &"zollunion", &"beziehung",
	&"militaer", &"unruhe", &"bund",
]
const ICON_SIZE := 22.0

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
	_buttons.resize(MapModes.COUNT)
	for g in GROUPS.size():
		if g > 0:
			var gap := VSeparator.new()
			gap.custom_minimum_size = Vector2(10, 0)
			first.add_child(gap)
		for mode in GROUPS[g]:
			var tooltip := "%s: %s (Taste %s)" % [
				MapModes.NAMES[mode], MapModes.DESCRIPTIONS[mode], MapModes.KEY_HINTS[mode]
			]
			var button := Icons.icon_button(ICONS[mode], tooltip, ICON_SIZE)
			button.name = "Mode_%d" % mode
			button.toggle_mode = true
			button.button_group = group
			button.pressed.connect(_on_button_pressed.bind(mode))
			first.add_child(button)
			_buttons[mode] = button


func _add_row() -> HBoxContainer:
	var row := HBoxContainer.new()
	row.add_theme_constant_override(&"separation", BUTTON_SEPARATION)
	row.alignment = BoxContainer.ALIGNMENT_CENTER
	_box.add_child(row)
	_rows.append(row)
	return row


## Seit M14a2 passen die Symbole immer in eine Reihe; rows bleibt für die Hauptszene erhalten.
func set_rows(_rows_wanted: int) -> void:
	_row_count = 1


## Knopf eines Kartenmodus (Tests).
func button(mode: int) -> Button:
	return _buttons[mode] if mode >= 0 and mode < _buttons.size() else null


func row_count() -> int:
	return _row_count


## Aktiven Modus markieren, ohne das Signal auszulösen.
func show_mode(mode: int) -> void:
	for i in _buttons.size():
		_buttons[i].set_pressed_no_signal(i == mode)


func _on_button_pressed(mode: int) -> void:
	mode_selected.emit(mode)
