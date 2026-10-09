class_name NoticeLayer
extends CenterContainer
## Hinweise in der Bildmitte (fehlende Erweiterung, Ladefehler, fehlende Karte).

const CARD_WIDTH := 620.0
const MAX_DETAIL_LINES := 14

@onready var _cards: VBoxContainer = %Cards


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE


## Fügt einen Hinweis hinzu. details erscheint in Festbreitenschrift (Befehle, Pfade).
func add_notice(
	title: String, body: String, details: String = "", closable: bool = true
) -> Control:
	var card := PanelContainer.new()
	card.theme_type_variation = &"NoticePanel"
	card.custom_minimum_size = Vector2(CARD_WIDTH, 0)
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 10)
	card.add_child(box)

	var title_label := Label.new()
	title_label.theme_type_variation = &"NoticeTitle"
	title_label.text = title
	title_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	title_label.custom_minimum_size = Vector2(1, 0)
	box.add_child(title_label)

	var body_label := Label.new()
	body_label.text = body
	body_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	body_label.custom_minimum_size = Vector2(1, 0)
	box.add_child(body_label)

	if not details.is_empty():
		var lines := details.split("\n")
		if lines.size() > MAX_DETAIL_LINES:
			var omitted := lines.size() - MAX_DETAIL_LINES
			lines = lines.slice(0, MAX_DETAIL_LINES)
			lines.append("… und %d weitere Zeilen (siehe Konsolenausgabe)" % omitted)
		var code_panel := PanelContainer.new()
		code_panel.theme_type_variation = &"CodePanel"
		var code := Label.new()
		code.theme_type_variation = &"CodeLabel"
		code.text = "\n".join(lines)
		code.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		code.custom_minimum_size = Vector2(1, 0)
		code_panel.add_child(code)
		box.add_child(code_panel)

	if closable:
		var row := HBoxContainer.new()
		row.alignment = BoxContainer.ALIGNMENT_END
		var close := Button.new()
		close.text = "Verstanden"
		close.focus_mode = Control.FOCUS_NONE
		close.custom_minimum_size = Vector2(120, 30)
		close.pressed.connect(card.queue_free)
		row.add_child(close)
		box.add_child(row)

	_cards.add_child(card)
	return card


func has_notices() -> bool:
	return _cards.get_child_count() > 0
