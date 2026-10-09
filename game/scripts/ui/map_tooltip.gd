class_name MapTooltip
extends PanelContainer
## Kleiner Hinweis am Mauszeiger mit dem Namen des überfahrenen Ortes.

const OFFSET := Vector2(18, 20)
const SCREEN_MARGIN := 8.0

@onready var _title: Label = %Title
@onready var _detail: Label = %Detail


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	top_level = true
	visible = false


func show_text(title: String, detail: String) -> void:
	_title.text = title
	_detail.text = detail
	_detail.visible = not detail.is_empty()
	visible = true
	reset_size()
	_follow_mouse()


func hide_tooltip() -> void:
	visible = false


func _process(_delta: float) -> void:
	if visible:
		_follow_mouse()


func _follow_mouse() -> void:
	var viewport_size := get_viewport_rect().size
	var mouse := get_viewport().get_mouse_position()
	var target := mouse + OFFSET
	if target.x + size.x > viewport_size.x - SCREEN_MARGIN:
		target.x = mouse.x - OFFSET.x - size.x
	if target.y + size.y > viewport_size.y - SCREEN_MARGIN:
		target.y = mouse.y - OFFSET.y - size.y
	position = target.max(Vector2(SCREEN_MARGIN, SCREEN_MARGIN))
