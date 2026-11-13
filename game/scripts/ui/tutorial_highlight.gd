class_name TutorialHighlight
extends Control
## Hervorhebung der Geschichtsstunde (seit M13b3): ein goldener Rahmen um das Element, das ein Schritt
## nennt; pulsiert, außer mit reduzierten Animationen. target liefert das Element (oder null).

var target: Callable = Callable()
var _phase := 0.0


func _ready() -> void:
	name = "TutorialHighlight"
	set_anchors_preset(Control.PRESET_FULL_RECT)
	mouse_filter = Control.MOUSE_FILTER_IGNORE


func _process(delta: float) -> void:
	_phase = fmod(_phase + delta * 2.4, TAU)
	queue_redraw()


## Rahmen im Koordinatensystem dieses Elements, leer ohne Ziel.
func target_rect() -> Rect2:
	if not target.is_valid():
		return Rect2()
	var control := target.call() as Control
	if control == null or not control.is_visible_in_tree():
		return Rect2()
	var rect := control.get_global_rect()
	return Rect2(rect.position - global_position, rect.size).grow(4.0)


func _draw() -> void:
	var rect := target_rect()
	if rect.size == Vector2.ZERO:
		return
	var alpha := 0.85 if Accessibility.reduced_motion else 0.55 + 0.4 * sin(_phase)
	var gold := EpochTheme.GOLD
	gold.a = alpha
	draw_rect(rect, gold, false, 3.0)
	gold.a = alpha * 0.35
	draw_rect(rect.grow(4.0), gold, false, 2.0)
