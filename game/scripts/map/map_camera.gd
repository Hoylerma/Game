class_name MapCamera
extends Camera2D
## Kamera der Kartenansicht.
##
## Ziehen mit rechter oder mittlerer Maustaste, Bewegen mit WASD und den
## Pfeiltasten, Zoomen mit dem Mausrad (zum Mauszeiger hin, mit Grenzen).
## W ist zugleich die Taste des Kartenmodus „Bevölkerungsdichte“: Kurz
## getippt schaltet sie den Modus um (siehe main.gd), gehalten bewegt sie
## die Kamera. Tasten, die keinem Kartenmodus gehören, bewegen sofort.

## Ab dieser Haltedauer gilt eine doppelt belegte Taste als Kamerabewegung.
const TAP_HOLD_MS := 180
const PAN_ACTIONS: Array[StringName] = [
	&"kamera_links", &"kamera_rechts", &"kamera_hoch", &"kamera_runter"
]
const PAN_DIRECTIONS: Array[Vector2] = [Vector2.LEFT, Vector2.RIGHT, Vector2.UP, Vector2.DOWN]
## Tastaturbewegung in Bildschirmpunkten je Sekunde.
const KEY_PAN_SPEED := 820.0
const ZOOM_STEP := 1.18
const MAX_ZOOM := 8.0
## Wie weit über den Kartenausschnitt hinaus herausgezoomt werden darf.
const MIN_ZOOM_FACTOR := 0.55
const ZOOM_SMOOTHING := 16.0

## Weltbereich der Karte; die Kamera bleibt über diesem Rechteck.
var map_rect := Rect2(Vector2.ZERO, Vector2(1024, 1152))
var min_zoom := 0.2

var _target_zoom := 1.0
var _zoom_anchor_screen := Vector2.ZERO
var _zoom_anchor_world := Vector2.ZERO
var _dragging := false
## Gedrückte Bewegungstasten: Tastencode → [Richtung, Zeitpunkt in ms, doppelt belegt].
var _held_keys := {}


func _ready() -> void:
	anchor_mode = Camera2D.ANCHOR_MODE_DRAG_CENTER
	_target_zoom = zoom.x


## true, wenn das Ereignis eine Kamerabewegungs-Aktion auslöst.
static func is_pan_event(event: InputEvent) -> bool:
	for action in PAN_ACTIONS:
		if event.is_action(action, true):
			return true
	return false


## Richtet die Kamera so aus, dass rect vollständig in screen_area
## (Bildschirmkoordinaten) passt, und legt daraus die Zoomgrenzen fest.
func fit_to(rect: Rect2, screen_area: Rect2) -> void:
	map_rect = rect
	if rect.size.x <= 0.0 or rect.size.y <= 0.0 or screen_area.size.x <= 0.0:
		return
	var fit := minf(screen_area.size.x / rect.size.x, screen_area.size.y / rect.size.y) * 0.96
	min_zoom = minf(fit * MIN_ZOOM_FACTOR, MAX_ZOOM)
	_set_zoom_now(fit)
	var screen_center := _viewport_size() * 0.5
	position = rect.get_center() - (screen_area.get_center() - screen_center) / fit
	_clamp_position()


## Zentriert die Kamera auf einen Weltpunkt (optional mit neuem Zoom).
func focus_on(world_point: Vector2, new_zoom: float = -1.0) -> void:
	if new_zoom > 0.0:
		_set_zoom_now(clampf(new_zoom, min_zoom, MAX_ZOOM))
	position = world_point
	_clamp_position()


## Weltkoordinate eines Punktes in Bildschirmkoordinaten des Viewports.
func screen_to_world(screen_point: Vector2) -> Vector2:
	return position + (screen_point - _viewport_size() * 0.5) / zoom.x


func reset_input_state() -> void:
	_held_keys.clear()
	_dragging = false


func _notification(what: int) -> void:
	if what == NOTIFICATION_APPLICATION_FOCUS_OUT or what == NOTIFICATION_WM_WINDOW_FOCUS_OUT:
		reset_input_state()


## Ziehen: Bewegung und Loslassen kommen hier an, auch wenn der Mauszeiger
## dabei über ein Panel fährt (sonst bliebe die Kamera „angeklebt“).
func _input(event: InputEvent) -> void:
	if not _dragging:
		return
	var motion := event as InputEventMouseMotion
	if motion != null:
		if (motion.button_mask & (MOUSE_BUTTON_MASK_RIGHT | MOUSE_BUTTON_MASK_MIDDLE)) == 0:
			_dragging = false
			return
		_move_by(-motion.relative / zoom.x)
		get_viewport().set_input_as_handled()
		return
	var button := event as InputEventMouseButton
	if button != null and not button.pressed and _is_drag_button(button.button_index):
		_dragging = false
		get_viewport().set_input_as_handled()


func _unhandled_input(event: InputEvent) -> void:
	var button := event as InputEventMouseButton
	if button != null:
		_handle_mouse_button(button)
		return
	var key := event as InputEventKey
	if key != null:
		_handle_key(key)


func _handle_mouse_button(button: InputEventMouseButton) -> void:
	if _is_drag_button(button.button_index):
		if button.pressed:
			_dragging = true
			get_viewport().set_input_as_handled()
		return
	if not button.pressed:
		return
	if button.button_index == MOUSE_BUTTON_WHEEL_UP:
		_zoom_at(button.position, _target_zoom * ZOOM_STEP)
		get_viewport().set_input_as_handled()
	elif button.button_index == MOUSE_BUTTON_WHEEL_DOWN:
		_zoom_at(button.position, _target_zoom / ZOOM_STEP)
		get_viewport().set_input_as_handled()


static func _is_drag_button(index: MouseButton) -> bool:
	return index == MOUSE_BUTTON_RIGHT or index == MOUSE_BUTTON_MIDDLE


func _handle_key(key: InputEventKey) -> void:
	var direction := Vector2.ZERO
	for i in PAN_ACTIONS.size():
		if key.is_action(PAN_ACTIONS[i], true):
			direction += PAN_DIRECTIONS[i]
	if direction == Vector2.ZERO:
		return
	var code := key.physical_keycode if key.physical_keycode != KEY_NONE else key.keycode
	if key.pressed:
		if not _held_keys.has(code):
			_held_keys[code] = [direction, Time.get_ticks_msec(), MapModes.mode_for_event(key) >= 0]
	else:
		_held_keys.erase(code)


func _process(delta: float) -> void:
	_update_keyboard_pan(delta)
	_update_zoom(delta)


func _update_keyboard_pan(delta: float) -> void:
	if _held_keys.is_empty():
		return
	var now := Time.get_ticks_msec()
	var direction := Vector2.ZERO
	for code: int in _held_keys:
		var entry: Array = _held_keys[code]
		var shared: bool = entry[2]
		var since: int = entry[1]
		if shared and now - since < TAP_HOLD_MS:
			continue
		var entry_direction: Vector2 = entry[0]
		direction += entry_direction
	if direction == Vector2.ZERO:
		return
	_move_by(direction.normalized() * KEY_PAN_SPEED * delta / zoom.x)


func _update_zoom(delta: float) -> void:
	if is_equal_approx(zoom.x, _target_zoom):
		return
	var weight := 1.0 - exp(-ZOOM_SMOOTHING * delta)
	var new_zoom := lerpf(zoom.x, _target_zoom, weight)
	if absf(new_zoom - _target_zoom) < _target_zoom * 0.002:
		new_zoom = _target_zoom
	zoom = Vector2(new_zoom, new_zoom)
	# Der Weltpunkt unter dem Mauszeiger bleibt an seiner Stelle.
	position = _zoom_anchor_world - (_zoom_anchor_screen - _viewport_size() * 0.5) / new_zoom
	_clamp_position()


func _zoom_at(screen_point: Vector2, requested_zoom: float) -> void:
	_zoom_anchor_screen = screen_point
	_zoom_anchor_world = screen_to_world(screen_point)
	_target_zoom = clampf(requested_zoom, min_zoom, MAX_ZOOM)


## Verschiebt die Kamera; ein laufender Zoom bewegt sich mit.
func _move_by(world_delta: Vector2) -> void:
	position += world_delta
	_zoom_anchor_world += world_delta
	_clamp_position()


func _set_zoom_now(value: float) -> void:
	_target_zoom = value
	zoom = Vector2(value, value)


func _clamp_position() -> void:
	if map_rect.size == Vector2.ZERO:
		return
	position = position.clamp(map_rect.position, map_rect.end)


func _viewport_size() -> Vector2:
	return get_viewport_rect().size
