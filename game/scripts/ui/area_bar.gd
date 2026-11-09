class_name AreaBar
extends PanelContainer
## Bereiche des Spielerstaates (seit M13, Schritt a5a, Fenster der Bereiche seit a5b; vertical_slice.md 4.1): eine senkrechte Leiste
## kleiner Knöpfe rechts neben der linken Spalte (in der Spalte selbst ist neben dem Staatspanel kein
## Platz); die Fenster der Bereiche öffnen rechts daneben. Ein Klick öffnet das Fenster des Bereichs (main.gd). Bereiche ohne Inhalt im
## Lesezeichen (Zollunion, Diplomatie, Krieg, Journale, Jahrbuch) sind ausgeblendet.

signal area_requested(area: StringName)

## Bereiche in der Reihenfolge der Leiste: Kennung, Aufschrift, Tooltip.
const AREAS: Array[Array] = [
	[&"kabinett", "Kabinett", "Kabinett und Hof: Herrscher, Minister, Neigungen, frühere Amtsinhaber"],
	[&"gesetze", "Gesetze", "Verfassung, Wahlrecht und Gesetze mit den Wünschen der Gruppen"],
	[&"gruppen", "Gruppen", "Interessengruppen: Anteil, Einfluss, Zustimmung, Forderungen"],
	[&"parlament", "Parlament", "Plenarsaal: letzte Wahl nach Fraktionen, nächste Wahl, Abstimmungen"],
	[&"gesellschaft", "Gesellschaft", "Bevölkerung: Alter, Kulturen, Konfessionen, Berufe, Wanderung"],
	[&"wirtschaft", "Wirtschaft", "Güter, Preise, Betriebe, Märkte und Zollgebiet"],
	[&"haushalt", "Haushalt", "Einnahmen und Ausgaben im Einzelnen"],
	[&"zollunion", "Zollunion", "Zollunionen und Wechsel zum 1. Januar"],
	[&"diplomatie", "Diplomatie", "Beziehungen, Bündnisse, Bundesversammlung und deutsche Frage"],
	[&"militaer", "Militär", "Kriege, Heere und Frieden"],
	[&"journale", "Journale", "Revolution und deutsche Frage (J)"],
	[&"jahrbuch", "Jahrbuch", "Statistisches Jahrbuch: Kennzahlen und Vermächtnis je Jahr (B)"],
	[&"chronik", "Chronik", "Chronik der Partie mit Filter und Suche"],
	[&"lexikon", "Lexikon", "Begriffe der Epoche und des Spiels"],
]

var _buttons: Dictionary = {}
var _flow: VBoxContainer = null


func _ready() -> void:
	name = "AreaBar"
	mouse_filter = Control.MOUSE_FILTER_STOP
	_flow = VBoxContainer.new()
	_flow.add_theme_constant_override(&"separation", 4)
	add_child(_flow)
	for area in AREAS:
		var button := Button.new()
		button.name = "Area_%s" % str(area[0])
		button.text = str(area[1])
		button.tooltip_text = str(area[2])
		button.focus_mode = Control.FOCUS_NONE
		button.toggle_mode = true
		button.add_theme_font_size_override(&"font_size", 13)
		var id: StringName = area[0]
		button.pressed.connect(func() -> void: area_requested.emit(id))
		_flow.add_child(button)
		_buttons[id] = button


## Knopf eines Bereichs (Tests).
func button(area: StringName) -> Button:
	return _buttons.get(area) as Button


## Welche Bereiche es gibt (Kennung → bool); fehlende bleiben, wie sie sind.
func set_available(areas: Dictionary) -> void:
	for id: Variant in areas:
		var b := _buttons.get(StringName(str(id))) as Button
		if b != null:
			b.visible = bool(areas[id])


## Der Bereich, dessen Fenster offen ist, erscheint gedrückt (leer: keiner).
func set_active(area: StringName) -> void:
	for id: Variant in _buttons:
		(_buttons[id] as Button).set_pressed_no_signal(id == area)


## Sichtbare Bereiche (Tests).
func visible_areas() -> PackedStringArray:
	var result := PackedStringArray()
	for area in AREAS:
		if (_buttons[area[0]] as Button).visible:
			result.append(str(area[0]))
	return result
