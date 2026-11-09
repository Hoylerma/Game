class_name TooltipLayer
extends Control
## Verschachtelte Tooltips (seit M13, Schritt a5c; vertical_slice.md 4.3 und Kapitel 03 Abschnitt 6): eine
## eigene Ebene statt Godots Standard. attach() verbindet ein Steuerelement mit einem Text (BBCode, von
## einer Funktion geliefert). Unterstrichene Begriffe ([url=…]) öffnen beim Überfahren einen zweiten
## Tooltip daneben; beide bleiben offen, solange die Maus auf dem Element oder einem der Tooltips steht.
## Verweise „begriff:<kennung>“ erklärt das Glossar, andere liefert der link_provider des Elements.

## Verzögerung bis zum Öffnen und bis zum Schließen.
const OPEN_DELAY := 0.35
const CLOSE_DELAY := 0.3
const MAX_WIDTH := 380.0
const LEVELS := 2

var _panels: Array[PanelContainer] = []
var _labels: Array[RichTextLabel] = []
var _owner_control: Control = null
var _provider: Callable = Callable()
var _link_provider: Callable = Callable()
var _pending: Control = null
var _pending_time := 0.0
var _away := 0.0
var _away_link := 0.0
var _hovered_link := ""


func _ready() -> void:
	name = "TooltipLayer"
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_anchors_preset(Control.PRESET_FULL_RECT)
	for level in LEVELS:
		var panel := PanelContainer.new()
		panel.name = "Tooltip%d" % level
		panel.visible = false
		panel.mouse_filter = Control.MOUSE_FILTER_STOP
		var label := RichTextLabel.new()
		label.bbcode_enabled = true
		label.fit_content = true
		label.scroll_active = false
		label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		label.custom_minimum_size = Vector2(MAX_WIDTH, 0)
		label.mouse_filter = Control.MOUSE_FILTER_PASS
		label.meta_underlined = true
		label.meta_hover_started.connect(_on_meta_hover.bind(level))
		label.meta_hover_ended.connect(func(_meta: Variant) -> void: _hovered_link = "")
		panel.add_child(label)
		add_child(panel)
		_panels.append(panel)
		_labels.append(label)


## Tooltip für ein Element: provider() → BBCode; link_provider(meta) → BBCode für eigene Verweise.
func attach(control: Control, provider: Callable, link_provider: Callable = Callable()) -> void:
	control.tooltip_text = ""
	control.mouse_filter = Control.MOUSE_FILTER_STOP if control.mouse_filter == Control.MOUSE_FILTER_IGNORE else control.mouse_filter
	control.mouse_entered.connect(
		func() -> void:
			_pending = control
			_pending_time = 0.0
			control.set_meta(&"tooltip_provider", provider)
			control.set_meta(&"tooltip_links", link_provider)
	)
	control.mouse_exited.connect(
		func() -> void:
			if _pending == control:
				_pending = null
	)


## Sofort öffnen (Tests, Tastatur).
func show_for(control: Control, provider: Callable, link_provider: Callable = Callable()) -> void:
	_owner_control = control
	_provider = provider
	_link_provider = link_provider
	_show_level(0, str(provider.call()), control.get_global_rect())


func hide_all() -> void:
	for panel in _panels:
		panel.visible = false
	_owner_control = null
	_away = 0.0


func is_showing(level: int = 0) -> bool:
	return level < _panels.size() and _panels[level].visible


## Text eines Tooltips ohne Auszeichnung (Tests).
func text_of(level: int) -> String:
	return _labels[level].get_parsed_text() if level < _labels.size() else ""


## Verweis öffnen wie beim Überfahren (Tests).
func open_link(meta: String) -> void:
	_on_meta_hover(meta, 0)


func _process(delta: float) -> void:
	if _pending != null and _pending != _owner_control:
		_pending_time += delta
		if _pending_time >= OPEN_DELAY and _pending.is_visible_in_tree():
			var provider: Callable = _pending.get_meta(&"tooltip_provider", Callable())
			var links: Callable = _pending.get_meta(&"tooltip_links", Callable())
			if provider.is_valid():
				show_for(_pending, provider, links)
	if _owner_control == null:
		return
	var mouse := get_global_mouse_position()
	var inside := _owner_control.is_visible_in_tree() and _owner_control.get_global_rect().has_point(mouse)
	for panel in _panels:
		inside = inside or (panel.visible and panel.get_global_rect().has_point(mouse))
	if inside:
		_away = 0.0
		# Der zweite Tooltip schließt, wenn die Maus eine Weile weder auf ihm noch auf einem Verweis steht.
		if _panels[1].visible and _hovered_link.is_empty() and not _panels[1].get_global_rect().has_point(mouse):
			_away_link += delta
			if _away_link >= CLOSE_DELAY:
				_panels[1].visible = false
		else:
			_away_link = 0.0
		return
	_away += delta
	if _away >= CLOSE_DELAY:
		hide_all()


func _on_meta_hover(meta: Variant, level: int) -> void:
	var key := str(meta)
	_hovered_link = key
	if level + 1 >= LEVELS:
		return
	var text := ""
	if key.begins_with("begriff:"):
		text = Glossary.text(key.substr(8))
	elif _link_provider.is_valid():
		text = str(_link_provider.call(key))
	if text.is_empty():
		return
	_show_level(level + 1, text, _panels[level].get_global_rect())


func _show_level(level: int, text: String, anchor: Rect2) -> void:
	for l in range(level + 1, LEVELS):
		_panels[l].visible = false
	_labels[level].text = text
	var panel := _panels[level]
	panel.visible = true
	panel.reset_size()
	var size := panel.get_combined_minimum_size()
	var area := get_viewport_rect().size
	var position := Vector2(anchor.position.x, anchor.end.y + 4.0)
	if level > 0:
		position = Vector2(anchor.end.x + 6.0, anchor.position.y)
		if position.x + size.x > area.x:
			position.x = anchor.position.x - size.x - 6.0
	position.x = clampf(position.x, 4.0, maxf(area.x - size.x - 4.0, 4.0))
	position.y = clampf(position.y, 4.0, maxf(area.y - size.y - 4.0, 4.0))
	panel.position = position
	panel.size = size
