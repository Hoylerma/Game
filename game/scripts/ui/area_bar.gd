class_name AreaBar
extends PanelContainer
## Bereiche des Spielerstaates (seit M13, Schritt a5a, Fenster der Bereiche seit a5b; vertical_slice.md 4.1): eine senkrechte Leiste
## kleiner Knöpfe am linken Rand (seit M14a1; vorher rechts neben der Staatsübersicht); die Schublade der
## Staatsübersicht („Staat“) und die Fenster der Bereiche öffnen rechts daneben. Ein Klick öffnet das Fenster des Bereichs (main.gd). Bereiche ohne Inhalt im
## Lesezeichen (Zollunion, Diplomatie, Krieg, Journale, Jahrbuch) sind ausgeblendet.

signal area_requested(area: StringName)

## Seit M14a2 Symbole statt Aufschriften (Name und Erklärung im Tooltip) und Zähler, wo etwas zu tun ist.

## Bereiche in der Reihenfolge der Leiste: Kennung, Aufschrift, Tooltip.
const AREAS: Array[Array] = [
	[&"staat", "Staat", "Staatsübersicht: Herrscher, Kasse, Steuern, Heer (auch ein Klick aufs Wappen oben links)"],
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

## Größe der Symbole (Pixel der Oberfläche).
const ICON_SIZE := 22.0

var _buttons: Dictionary = {}
var _badges: Dictionary = {}
var _flow: VBoxContainer = null


func _ready() -> void:
	name = "AreaBar"
	mouse_filter = Control.MOUSE_FILTER_STOP
	_flow = VBoxContainer.new()
	_flow.add_theme_constant_override(&"separation", 4)
	add_child(_flow)
	for area in AREAS:
		var id: StringName = area[0]
		var button := Icons.icon_button(id, "%s – %s" % [tr(str(area[1])), tr(str(area[2]))], ICON_SIZE)
		button.name = "Area_%s" % str(area[0])
		button.toggle_mode = true
		button.pressed.connect(func() -> void: area_requested.emit(id))
		_flow.add_child(button)
		_buttons[id] = button
		_badges[id] = _make_badge(button)


## Knopf eines Bereichs (Tests).
func button(area: StringName) -> Button:
	return _buttons.get(area) as Button


## Welche Bereiche es gibt (Kennung → bool); fehlende bleiben, wie sie sind.
func set_available(areas: Dictionary) -> void:
	for id: Variant in areas:
		var b := _buttons.get(StringName(str(id))) as Button
		if b != null:
			b.visible = bool(areas[id])


## Der Bereich, dessen Fenster offen ist, erscheint gedrückt (leer: keiner); „Staat“ zeigt die Schublade
## der Staatsübersicht (set_drawer_open), die neben einem Fenster offen sein kann.
func set_active(area: StringName) -> void:
	for id: Variant in _buttons:
		if id != &"staat":
			(_buttons[id] as Button).set_pressed_no_signal(id == area)


func set_drawer_open(open: bool) -> void:
	(_buttons[&"staat"] as Button).set_pressed_no_signal(open)


## Zähler an einem Bereich (etwa laufende Kriege); 0 blendet ihn aus.
func set_badge(area: StringName, count: int) -> void:
	var badge := _badges.get(area) as PanelContainer
	if badge == null:
		return
	badge.visible = count > 0
	(badge.get_child(0) as Label).text = str(count) if count < 10 else "9+"


## Gezeigter Zähler eines Bereichs (Tests): 0 ohne Zähler.
func badge(area: StringName) -> int:
	var b := _badges.get(area) as PanelContainer
	if b == null or not b.visible:
		return 0
	var text := (b.get_child(0) as Label).text
	return 10 if text == "9+" else int(text)


## Kleiner Zähler oben rechts am Knopf, Gold auf Tinte.
static func _make_badge(button: Button) -> PanelContainer:
	var badge := PanelContainer.new()
	badge.name = "Badge"
	badge.mouse_filter = Control.MOUSE_FILTER_IGNORE
	var style := StyleBoxFlat.new()
	style.bg_color = UiPalette.ACCENT
	style.set_corner_radius_all(7)
	style.content_margin_left = 4
	style.content_margin_right = 4
	style.content_margin_top = 0
	style.content_margin_bottom = 0
	badge.add_theme_stylebox_override(&"panel", style)
	var label := Label.new()
	label.add_theme_font_size_override(&"font_size", 11)
	label.add_theme_color_override(&"font_color", UiPalette.PAPER)
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	badge.add_child(label)
	button.add_child(badge)
	badge.set_anchors_and_offsets_preset(Control.PRESET_TOP_RIGHT)
	badge.position = Vector2(button.custom_minimum_size.x - 14.0, -3.0)
	badge.visible = false
	return badge


## Sichtbare Bereiche (Tests).
func visible_areas() -> PackedStringArray:
	var result := PackedStringArray()
	for area in AREAS:
		if (_buttons[area[0]] as Button).visible:
			result.append(str(area[0]))
	return result
