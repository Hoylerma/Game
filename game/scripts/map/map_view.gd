class_name MapView
extends Node2D
## Kartenansicht: ID-Bild mit Kartenshader, Hervorhebung, Auswahl, Kartenrahmen.
##
## Arbeitet mit Simulationsindizes nach außen; der Shader bekommt Kartenindizes.

## Ort unter dem Mauszeiger hat gewechselt (-1 = keiner).
signal location_hovered(sim_index: int)
## Linksklick auf die Karte (-1 = kein Ort an dieser Stelle).
signal location_clicked(sim_index: int)

const MAP_SHADER: Shader = preload("res://shaders/map.gdshader")
const MAX_COAST_POINTS := 128
## Farben des Kartenrahmens (Kupferstich).
const FRAME_INK := Color("2b2116")
const FRAME_PAPER := Color("f3ead5")
const FRAME_MARGIN := Color("efe5cd")
const FRAME_LABEL := Color("5b4a36")
## Rahmenmaße in Kartenpixeln.
const FRAME_INNER := 3.0
const FRAME_BAND := 7.0
const FRAME_LABEL_SIZE := 22
## Unterteilung des Gradrahmens.
const FRAME_STEP_DEGREES := 0.5

## Hervorhebung beim Überfahren (im Screenshot-Modus aus).
var hover_enabled := true

var _map: MapData = null
var _material: ShaderMaterial = null
var _color_image: Image = null
var _color_texture: ImageTexture = null
var _owner_image: Image = null
var _owner_texture: ImageTexture = null
var _hovered := -1
var _mouse_in_window := true

@onready var _sprite: Sprite2D = $MapSprite
@onready var camera: MapCamera = $MapCamera


## Übernimmt die geladene Karte. Ohne gültige Karte bleibt die Ansicht leer.
func setup(map: MapData) -> void:
	_map = map
	if map == null or not map.is_valid():
		_sprite.visible = false
		queue_redraw()
		return
	_sprite.texture = map.texture
	_sprite.centered = false
	_sprite.position = Vector2.ZERO
	_sprite.texture_filter = CanvasItem.TEXTURE_FILTER_NEAREST
	_sprite.visible = true

	var lut_width := maxi(map.map_count(), 1)
	_color_image = Image.create_empty(lut_width, 1, false, Image.FORMAT_RGBA8)
	_color_texture = ImageTexture.create_from_image(_color_image)
	_owner_image = Image.create_empty(lut_width, 1, false, Image.FORMAT_RGBA8)
	_owner_texture = ImageTexture.create_from_image(_owner_image)

	_material = ShaderMaterial.new()
	_material.shader = MAP_SHADER
	_material.set_shader_parameter(&"id_map", map.texture)
	_material.set_shader_parameter(&"color_lut", _color_texture)
	_material.set_shader_parameter(&"owner_lut", _owner_texture)
	_material.set_shader_parameter(&"selected_index", -1)
	_material.set_shader_parameter(&"hovered_index", -1)
	_apply_coast(map)
	_sprite.material = _material
	queue_redraw()


func has_map() -> bool:
	return _map != null and _map.is_valid()


## Weltrechteck der Karte (Kartenpixel = Welteinheiten).
func map_rect() -> Rect2:
	if not has_map():
		return Rect2(Vector2.ZERO, Vector2(1024, 1152))
	return Rect2(_sprite.position, Vector2(_map.size))


## Farben je Simulationsindex (get_map_colors) in die Nachschlagetextur schreiben.
func set_location_colors(colors: PackedColorArray) -> void:
	if not has_map():
		return
	for map_index in _map.map_count():
		var sim_index := _map.map_to_sim[map_index]
		var color := Color(0, 0, 0, 0)
		if sim_index >= 0 and sim_index < colors.size():
			color = colors[sim_index]
			color.a = 1.0
		_color_image.set_pixel(map_index, 0, color)
	_color_texture.update(_color_image)


## Besitzer je Simulationsindex (Staatsindex, -1 = keiner) für die Staatsgrenzen.
func set_location_owners(owners: PackedInt32Array) -> void:
	if not has_map():
		return
	for map_index in _map.map_count():
		var sim_index := _map.map_to_sim[map_index]
		var encoded := 0
		if sim_index >= 0 and sim_index < owners.size() and owners[sim_index] >= 0:
			encoded = owners[sim_index] + 1
		_owner_image.set_pixel(map_index, 0, Color8(encoded & 0xFF, (encoded >> 8) & 0xFF, 0, 255))
	_owner_texture.update(_owner_image)


func set_selected(sim_index: int) -> void:
	if _material != null:
		_material.set_shader_parameter(&"selected_index", _map_index_of(sim_index))


func get_hovered() -> int:
	return _hovered


## Weltposition des Schwerpunkts eines Ortes oder Vector2(-1, -1).
func location_center(sim_index: int) -> Vector2:
	if not has_map():
		return Vector2(-1, -1)
	var center := _map.center_of_sim(sim_index)
	if center.x < 0.0:
		return center
	return _sprite.position + center


func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_MOUSE_EXIT:
		_mouse_in_window = false
	elif what == NOTIFICATION_WM_MOUSE_ENTER:
		_mouse_in_window = true


func _process(_delta: float) -> void:
	if not has_map():
		return
	var sim_index := -1
	if hover_enabled and _mouse_in_window:
		sim_index = _sim_index_under_mouse()
	if sim_index != _hovered:
		_hovered = sim_index
		_material.set_shader_parameter(&"hovered_index", _map_index_of(sim_index))
		location_hovered.emit(sim_index)


func _unhandled_input(event: InputEvent) -> void:
	var button := event as InputEventMouseButton
	if button == null or button.button_index != MOUSE_BUTTON_LEFT or not button.pressed:
		return
	if not has_map():
		return
	var world := camera.screen_to_world(button.position)
	location_clicked.emit(_sim_index_at_world(world))
	get_viewport().set_input_as_handled()


func _sim_index_under_mouse() -> int:
	var viewport := get_viewport()
	if viewport.gui_get_hovered_control() != null:
		return -1
	var mouse := viewport.get_mouse_position()
	if not viewport.get_visible_rect().has_point(mouse):
		return -1
	return _sim_index_at_world(camera.screen_to_world(mouse))


func _sim_index_at_world(world: Vector2) -> int:
	var local := world - _sprite.position
	return _map.sim_index_at_pixel(Vector2i(floori(local.x), floori(local.y)))


func _map_index_of(sim_index: int) -> int:
	if not has_map() or sim_index < 0 or sim_index >= _map.sim_to_map.size():
		return -1
	return _map.sim_to_map[sim_index]


## Übergibt dem Shader das Meerespolygon: die Küstenlinie (West → Ost), an
## ihren Enden senkrecht zum oberen Bildrand geschlossen. Wie im
## Kartengenerator ist Meer nur, was innerhalb des Längenbereichs der Küste
## nördlich von ihr liegt. Ohne karte.chron gilt jedes Gebiet ohne Ort als Meer.
func _apply_coast(map: MapData) -> void:
	var polygon := PackedVector2Array()
	if map.config != null and map.config.coast.size() >= 2:
		var coast := map.config.coast_in_pixels(map.size)
		if coast[0].x > coast[coast.size() - 1].x:
			coast.reverse()
		polygon.append_array(coast)
		polygon.append(Vector2(coast[coast.size() - 1].x, -1.0))
		polygon.append(Vector2(coast[0].x, -1.0))
	if polygon.size() > MAX_COAST_POINTS:
		printerr(
			(
				"EINIGKEIT: Küstenlinie hat mehr als %d Punkte (mit zwei Eckpunkten) und wird ignoriert."
				% MAX_COAST_POINTS
			)
		)
		polygon.clear()
	var padded := polygon.duplicate()
	padded.resize(MAX_COAST_POINTS)
	_material.set_shader_parameter(&"coast_points", padded)
	_material.set_shader_parameter(&"coast_point_count", polygon.size())


func _draw() -> void:
	if not has_map():
		return
	var rect := map_rect()
	var outer := rect.grow(FRAME_INNER + FRAME_BAND)
	# Von außen nach innen: Papierrand, Zierlinie, Gradrahmen, Innenlinie.
	draw_rect(outer.grow(48.0), FRAME_MARGIN)
	draw_rect(outer.grow(7.0), FRAME_INK)
	draw_rect(outer.grow(6.0), FRAME_MARGIN)
	draw_rect(outer, FRAME_INK)
	draw_rect(outer.grow(-1.0), FRAME_PAPER)
	if _map.config != null:
		_draw_degree_band(rect)
	draw_rect(rect.grow(FRAME_INNER), FRAME_INK)
	draw_rect(rect.grow(FRAME_INNER - 1.0), FRAME_PAPER)
	if _map.config != null:
		_draw_degree_labels(rect)


## Abwechselnd dunkle Felder je halbem Grad entlang des Rahmens.
func _draw_degree_band(rect: Rect2) -> void:
	var config := _map.config
	var pixel_scale := Vector2(rect.size.x / config.image_width, rect.size.y / config.image_height)
	var thickness := FRAME_BAND - 2.0
	var top := rect.position.y - FRAME_INNER - FRAME_BAND + 1.0
	var bottom := rect.end.y + FRAME_INNER + 1.0
	var left := rect.position.x - FRAME_INNER - FRAME_BAND + 1.0
	var right := rect.end.x + FRAME_INNER + 1.0
	var lon := floorf(config.longitude_min / FRAME_STEP_DEGREES) * FRAME_STEP_DEGREES
	while lon < config.longitude_max:
		var start := maxf(lon, config.longitude_min)
		var finish := minf(lon + FRAME_STEP_DEGREES, config.longitude_max)
		if roundi(lon / FRAME_STEP_DEGREES) % 2 == 0:
			var x0 := rect.position.x + config.geo_to_pixel(start, 0.0).x * pixel_scale.x
			var x1 := rect.position.x + config.geo_to_pixel(finish, 0.0).x * pixel_scale.x
			draw_rect(Rect2(x0, top, x1 - x0, thickness), FRAME_INK)
			draw_rect(Rect2(x0, bottom, x1 - x0, thickness), FRAME_INK)
		lon += FRAME_STEP_DEGREES
	var lat := floorf(config.latitude_min / FRAME_STEP_DEGREES) * FRAME_STEP_DEGREES
	while lat < config.latitude_max:
		var start := maxf(lat, config.latitude_min)
		var finish := minf(lat + FRAME_STEP_DEGREES, config.latitude_max)
		if roundi(lat / FRAME_STEP_DEGREES) % 2 == 0:
			var y0 := rect.position.y + config.geo_to_pixel(0.0, finish).y * pixel_scale.y
			var y1 := rect.position.y + config.geo_to_pixel(0.0, start).y * pixel_scale.y
			draw_rect(Rect2(left, y0, thickness, y1 - y0), FRAME_INK)
			draw_rect(Rect2(right, y0, thickness, y1 - y0), FRAME_INK)
		lat += FRAME_STEP_DEGREES


## Gradzahlen an den vollen Graden.
func _draw_degree_labels(rect: Rect2) -> void:
	var config := _map.config
	var font := ThemeDB.fallback_font
	var pixel_scale := Vector2(rect.size.x / config.image_width, rect.size.y / config.image_height)
	var offset := FRAME_INNER + FRAME_BAND + 12.0
	for degree in range(ceili(config.longitude_min), floori(config.longitude_max) + 1):
		var x := rect.position.x + config.geo_to_pixel(degree, 0.0).x * pixel_scale.x
		var text := "%d° O" % degree
		var width := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, FRAME_LABEL_SIZE).x
		draw_string(font, Vector2(x - width * 0.5, rect.position.y - offset), text,
			HORIZONTAL_ALIGNMENT_LEFT, -1, FRAME_LABEL_SIZE, FRAME_LABEL)
		draw_string(font, Vector2(x - width * 0.5, rect.end.y + offset + FRAME_LABEL_SIZE * 0.8),
			text, HORIZONTAL_ALIGNMENT_LEFT, -1, FRAME_LABEL_SIZE, FRAME_LABEL)
	for degree in range(ceili(config.latitude_min), floori(config.latitude_max) + 1):
		var y := rect.position.y + config.geo_to_pixel(0.0, degree).y * pixel_scale.y
		var text := "%d° N" % degree
		var width := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, FRAME_LABEL_SIZE).x
		draw_string(font, Vector2(rect.position.x - offset - width, y + FRAME_LABEL_SIZE * 0.35),
			text, HORIZONTAL_ALIGNMENT_LEFT, -1, FRAME_LABEL_SIZE, FRAME_LABEL)
		draw_string(font, Vector2(rect.end.x + offset, y + FRAME_LABEL_SIZE * 0.35), text,
			HORIZONTAL_ALIGNMENT_LEFT, -1, FRAME_LABEL_SIZE, FRAME_LABEL)
