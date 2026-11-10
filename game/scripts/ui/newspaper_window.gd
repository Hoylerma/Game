class_name NewspaperWindow
extends PanelContainer
## Zeitung (seit M13b1, vertical_slice.md 5.2): Titelseite eines erfundenen Blattes zu einem Eintrag der
## Chronik. Kopf in Fraktur, Ausgabe und Datum, Schlagzeile, Unterzeile, zwei Spalten (Leitartikel und
## Vermischtes). Ein Klick irgendwohin schließt. Die Seite kommt fertig aus dem Kern (get_front_page);
## main.gd öffnet sie bei wichtigen Nachrichten (Gewicht 3, Gewicht 2 mit dem Spielerstaat beteiligt) und
## aus dem Jahrbuch.

signal close_requested

const WIDTH := 760.0
const COLUMN_GAP := 22
## Bezeichnung der Richtung im Kopf.
const LEANINGS := {
	"liberal": "Liberales Blatt", "konservativ": "Konservatives Blatt", "demokratisch": "Demokratisches Blatt",
	"katholisch": "Katholisches Blatt",
}

var _page := {}
var _masthead: Label = null
var _issue: Label = null
var _headline: Label = null
var _subline: Label = null
var _article: Label = null
var _report: Label = null
var _miscellany: VBoxContainer = null
var _hint: Label = null


func _ready() -> void:
	name = "NewspaperWindow"
	custom_minimum_size = Vector2(WIDTH, 0)
	visible = false
	mouse_filter = Control.MOUSE_FILTER_STOP
	_build()
	EpochTheme.frame(self)


func _gui_input(event: InputEvent) -> void:
	var button := event as InputEventMouseButton
	if button != null and button.pressed and button.button_index == MOUSE_BUTTON_LEFT:
		accept_event()
		close_requested.emit()


## Zeigt die Titelseite page (SimulationClient.get_front_page); false bei einer leeren.
func show_page(page: Dictionary) -> bool:
	if page.is_empty():
		return false
	_page = page
	_masthead.text = str(page.get("paper", ""))
	_issue.text = "Nr. %d  ·  %s  ·  %s" % [
		int(page.get("issue", 0)), str(page.get("date_text", "")),
		str(LEANINGS.get(str(page.get("leaning", "")), "Zeitung"))
	]
	_headline.text = str(page.get("headline", ""))
	_subline.text = str(page.get("subline", ""))
	_subline.visible = not _subline.text.is_empty()
	_article.text = str(page.get("text", ""))
	var report := str(page.get("report", ""))
	_report.text = "Drahtbericht: %s" % report if not report.is_empty() and report != _article.text else ""
	_report.visible = not _report.text.is_empty()
	for child in _miscellany.get_children():
		_miscellany.remove_child(child)
		child.queue_free()
	var misc: Array = page.get("miscellany", [])
	if misc.is_empty():
		_miscellany.add_child(_column_text("Aus den übrigen Landesteilen nichts Neues."))
	for entry: Variant in misc:
		var item := _column_text(str(entry))
		item.add_theme_font_size_override(&"font_size", 13)
		_miscellany.add_child(item)
	visible = true
	reset_size()
	return true


func close() -> void:
	visible = false


func is_open() -> bool:
	return visible


## Angezeigte Seite (Tests).
func page() -> Dictionary:
	return _page


## Alle Texte der Seite (Tests), Kopf zuerst.
func page_text() -> String:
	var parts := PackedStringArray([_masthead.text, _issue.text, _headline.text, _subline.text, _article.text, _report.text])
	for child in _miscellany.get_children():
		if child is Label:
			parts.append((child as Label).text)
	return "\n".join(parts)


func _build() -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 4)
	box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_child(box)
	_masthead = _label(&"MastheadLabel", HORIZONTAL_ALIGNMENT_CENTER)
	_masthead.name = "Masthead"
	box.add_child(_masthead)
	_issue = _label(&"CaptionLabel", HORIZONTAL_ALIGNMENT_CENTER)
	box.add_child(_issue)
	box.add_child(_rule())
	_headline = _label(&"HeadlineLabel", HORIZONTAL_ALIGNMENT_CENTER)
	_headline.name = "Headline"
	_headline.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	box.add_child(_headline)
	_subline = _label(&"SublineLabel", HORIZONTAL_ALIGNMENT_CENTER)
	_subline.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	box.add_child(_subline)
	box.add_child(_rule())
	var columns := HBoxContainer.new()
	columns.add_theme_constant_override(&"separation", COLUMN_GAP)
	columns.mouse_filter = Control.MOUSE_FILTER_IGNORE
	box.add_child(columns)
	var left := VBoxContainer.new()
	left.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	left.size_flags_stretch_ratio = 1.2
	left.mouse_filter = Control.MOUSE_FILTER_IGNORE
	columns.add_child(left)
	_article = _column_text("")
	_article.name = "Article"
	left.add_child(_article)
	_report = _column_text("")
	_report.name = "Report"
	_report.theme_type_variation = &"MutedLabel"
	left.add_child(_report)
	var line := VSeparator.new()
	line.mouse_filter = Control.MOUSE_FILTER_IGNORE
	columns.add_child(line)
	var right := VBoxContainer.new()
	right.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	right.mouse_filter = Control.MOUSE_FILTER_IGNORE
	columns.add_child(right)
	var misc_title := _label(&"HeaderLabel", HORIZONTAL_ALIGNMENT_LEFT)
	misc_title.text = "VERMISCHTES"
	right.add_child(misc_title)
	_miscellany = VBoxContainer.new()
	_miscellany.add_theme_constant_override(&"separation", 6)
	_miscellany.mouse_filter = Control.MOUSE_FILTER_IGNORE
	right.add_child(_miscellany)
	box.add_child(_rule())
	_hint = _label(&"CaptionLabel", HORIZONTAL_ALIGNMENT_CENTER)
	_hint.text = "Ein Klick legt die Zeitung beiseite."
	box.add_child(_hint)


func _label(variation: StringName, align: HorizontalAlignment) -> Label:
	var label := Label.new()
	label.theme_type_variation = variation
	label.horizontal_alignment = align
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return label


func _column_text(text: String) -> Label:
	var label := Label.new()
	label.text = text
	label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_LEFT
	label.custom_minimum_size = Vector2(200, 0)
	label.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return label


func _rule() -> TextureRect:
	var rule := TextureRect.new()
	rule.texture = EpochTheme.texture("trenner")
	rule.stretch_mode = TextureRect.STRETCH_KEEP_CENTERED
	rule.custom_minimum_size = Vector2(0, 16)
	rule.mouse_filter = Control.MOUSE_FILTER_IGNORE
	return rule
