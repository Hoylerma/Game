class_name BattleReports
extends VBoxContainer
## Schlachtberichte (seit M10, Schritt b3; militaer.md Abschnitt 8): eine Meldung je Schlacht mit
## eigenen Heeren, unten rechts über der Karte (main.gd setzt Breite und Lage). Titel „Sieg bei …“
## bzw. „Niederlage bei …“, der Bericht der Chronik und die Knöpfe „Zeigen“ (Ort anfahren) und ×.
## Eine Meldung verschwindet nach SHOW_MS von selbst; mehr als MAX_REPORTS verdrängen die älteste.

## „Zeigen“ gedrückt: Ort der Schlacht auswählen und anfahren.
signal location_requested(location_index: int)

const MAX_REPORTS := 3
const SHOW_MS := 12000
## Breite der Meldungen (main.gd passt sie dem Platz neben offenen Fenstern an).
const MAX_WIDTH := 360.0
const MIN_WIDTH := 220.0


func _init() -> void:
	name = "BattleReports"
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_theme_constant_override(&"separation", 6)
	alignment = BoxContainer.ALIGNMENT_END
	visible = false


func _process(_delta: float) -> void:
	var now := Time.get_ticks_msec()
	for card in get_children():
		if now - int(card.get_meta(&"shown_at", now)) > SHOW_MS:
			_remove(card)


## Meldung hinzufügen; victory färbt den Titel.
func add_report(title: String, text: String, location_index: int, victory: bool) -> void:
	while get_child_count() >= MAX_REPORTS:
		_remove(get_child(0))
	var card := PanelContainer.new()
	card.theme_type_variation = &"NoticePanel"
	card.mouse_filter = Control.MOUSE_FILTER_STOP
	card.set_meta(&"shown_at", Time.get_ticks_msec())
	card.set_meta(&"location", location_index)
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 4)
	card.add_child(box)
	var head := HBoxContainer.new()
	var title_label := Label.new()
	title_label.name = "Title"
	title_label.theme_type_variation = &"NameLabel"
	title_label.text = title
	title_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	title_label.clip_text = true
	title_label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	title_label.custom_minimum_size = Vector2(1, 0)
	title_label.add_theme_color_override(&"font_color", UiPalette.POSITIVE if victory else UiPalette.NEGATIVE)
	head.add_child(title_label)
	var close := Button.new()
	close.name = "Close"
	close.text = "×"
	close.theme_type_variation = &"FlatButton"
	close.focus_mode = Control.FOCUS_NONE
	close.tooltip_text = "Meldung schließen"
	close.pressed.connect(_remove.bind(card))
	head.add_child(close)
	box.add_child(head)
	var body := Label.new()
	body.name = "Body"
	body.text = text
	body.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	body.custom_minimum_size = Vector2(1, 0)
	box.add_child(body)
	if location_index >= 0:
		var foot := HBoxContainer.new()
		foot.alignment = BoxContainer.ALIGNMENT_END
		var show := Button.new()
		show.name = "Show"
		show.text = "Zeigen"
		show.theme_type_variation = &"FlatButton"
		show.focus_mode = Control.FOCUS_NONE
		show.tooltip_text = "Ort der Schlacht anfahren"
		show.pressed.connect(func() -> void: location_requested.emit(location_index))
		foot.add_child(show)
		box.add_child(foot)
	add_child(card)
	visible = true


func clear() -> void:
	for card in get_children():
		_remove(card)


func report_count() -> int:
	var count := 0
	for card in get_children():
		if not card.is_queued_for_deletion():
			count += 1
	return count


## Titel und Text aller Meldungen, zeilenweise (für Tests).
func reports_text() -> String:
	var lines := PackedStringArray()
	for card in get_children():
		if card.is_queued_for_deletion():
			continue
		lines.append(str((card.find_child("Title", true, false) as Label).text))
		lines.append(str((card.find_child("Body", true, false) as Label).text))
	return "\n".join(lines)


func _remove(card: Node) -> void:
	if card.get_parent() == self:
		remove_child(card)
	card.queue_free()
	visible = get_child_count() > 0
