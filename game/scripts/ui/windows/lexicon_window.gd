class_name LexiconWindow
extends PanelContainer
## Lexikon (seit M13a5d; vertical_slice.md 4.2): Artikel aus data/lexikon mit Suche; Verweise im Artikel und
## in den verschachtelten Tooltips öffnen andere Artikel. Liegt in der Fensterverwaltung.

signal close_requested

const WIDTH := 760.0
const HEIGHT := 440.0

var _lexicon: Lexicon = null
var _current := ""
var _search: LineEdit = null
var _list: ItemList = null
var _article: RichTextLabel = null
var _ids := PackedStringArray()


func _ready() -> void:
	name = "LexiconWindow"
	custom_minimum_size = Vector2(WIDTH, 0)
	visible = false
	mouse_filter = Control.MOUSE_FILTER_STOP
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 6)
	add_child(box)
	var head := HBoxContainer.new()
	var title := Label.new()
	title.text = "LEXIKON"
	title.theme_type_variation = &"HeaderLabel"
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	head.add_child(title)
	var close_button := Button.new()
	close_button.text = "Schließen"
	close_button.focus_mode = Control.FOCUS_NONE
	close_button.pressed.connect(func() -> void: close_requested.emit())
	head.add_child(close_button)
	box.add_child(head)
	_search = LineEdit.new()
	_search.name = "Search"
	_search.placeholder_text = "Begriff suchen …"
	_search.text_changed.connect(func(_t: String) -> void: _fill_list())
	box.add_child(_search)
	var split := HBoxContainer.new()
	split.add_theme_constant_override(&"separation", 10)
	_list = ItemList.new()
	_list.name = "Articles"
	_list.custom_minimum_size = Vector2(220, HEIGHT)
	_list.item_selected.connect(func(i: int) -> void: show_article(_ids[i]))
	# Farben des Papiers statt Godots dunkler Vorgabe.
	var list_style := StyleBoxFlat.new()
	list_style.bg_color = UiPalette.PAPER_INSET
	list_style.border_color = UiPalette.EDGE
	list_style.set_border_width_all(1)
	list_style.set_content_margin_all(4)
	_list.add_theme_stylebox_override(&"panel", list_style)
	_list.add_theme_stylebox_override(&"focus", StyleBoxEmpty.new())
	var selected := StyleBoxFlat.new()
	selected.bg_color = UiPalette.PAPER_DEEP
	_list.add_theme_stylebox_override(&"selected", selected)
	_list.add_theme_stylebox_override(&"selected_focus", selected)
	_list.add_theme_color_override(&"font_color", UiPalette.INK)
	_list.add_theme_color_override(&"font_selected_color", UiPalette.ACCENT)
	_list.add_theme_color_override(&"font_hovered_color", UiPalette.INK)
	split.add_child(_list)
	_article = RichTextLabel.new()
	_article.name = "Article"
	_article.bbcode_enabled = true
	_article.custom_minimum_size = Vector2(WIDTH - 260.0, HEIGHT)
	_article.meta_underlined = true
	_article.add_theme_color_override(&"default_color", UiPalette.INK)
	_article.meta_clicked.connect(
		func(meta: Variant) -> void:
			var key := str(meta)
			if key.begins_with("begriff:"):
				show_article(key.substr(8))
	)
	split.add_child(_article)
	box.add_child(split)


func open(lexicon: Lexicon, id: String = "") -> void:
	_lexicon = lexicon
	visible = true
	_fill_list()
	if not id.is_empty() and _lexicon.has(id):
		show_article(id)
	elif _current.is_empty() and not _ids.is_empty():
		show_article(_ids[0])
	else:
		show_article(_current)


func close() -> void:
	visible = false


func is_open() -> bool:
	return visible


func fit_height(max_height: float) -> void:
	var overflow := get_combined_minimum_size().y - max_height
	var height := HEIGHT - maxf(overflow, 0.0)
	_list.custom_minimum_size.y = maxf(160.0, height)
	_article.custom_minimum_size.y = maxf(160.0, height)


func show_article(id: String) -> void:
	if _lexicon == null or not _lexicon.has(id):
		return
	_current = id
	_article.text = _lexicon.bbcode(id)
	var index := _ids.find(id)
	if index >= 0:
		_list.select(index)


func current_article() -> String:
	return _current


## Artikeltext ohne Auszeichnung (Tests).
func article_text() -> String:
	return _article.get_parsed_text()


func _fill_list() -> void:
	_list.clear()
	if _lexicon == null:
		return
	_ids = _lexicon.ids(_search.text)
	for id in _ids:
		_list.add_item(_lexicon.title(id))
	var index := _ids.find(_current)
	if index >= 0:
		_list.select(index)
