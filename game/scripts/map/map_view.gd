class_name MapView
extends Node2D
## Kartenansicht: ID-Bild mit Kartenshader, Hervorhebung, Auswahl, Kartenrahmen.
##
## Arbeitet nach außen mit Simulationsindizes; der Shader bekommt Kartenindizes.
## Die Nachschlagetexturen sind zweidimensional (MapData.LUT_COLUMNS Spalten), weil
## 4.500 Orte die sichere Texturbreite des Kompatibilitätsrenderers überschreiten.

## Ort unter dem Mauszeiger hat gewechselt (-1 = keiner).
signal location_hovered(sim_index: int)
## Linksklick auf die Karte (-1 = kein Ort an dieser Stelle).
signal location_clicked(sim_index: int)

const MAP_SHADER: Shader = preload("res://shaders/map.gdshader")
## Markierung in der Farbtabelle für Orte der Karte, die die Simulation nicht kennt.
const UNKNOWN_MARKER := Color(0.0, 0.0, 0.0, 0.5)
## Kantenlänge und Startwert der Körnungstextur (Papier).
const GRAIN_SIZE := 256
const GRAIN_SEED := 1648
## Im Kartenmodus „Kreise“ sind Kreisgrenzen mindestens so deutlich.
const KREIS_MODE_BORDER_ALPHA := 0.7
## Farben des Kartenrahmens (Kupferstich).
const FRAME_INK := Color("2b2116")
const FRAME_PAPER := Color("f3ead5")
const FRAME_MARGIN := Color("efe5cd")
const FRAME_LABEL := Color("5b4a36")
## Rahmenmaße in Kartenpixeln bei 1024 Pixeln Kartenbreite (skaliert mit der Karte).
const FRAME_INNER := 3.0
const FRAME_BAND := 7.0
const FRAME_LABEL_SIZE := 22
## Unterteilung des Gradrahmens.
const FRAME_STEP_DEGREES := 0.5

## Hervorhebung beim Überfahren (im Screenshot-Modus aus).
var hover_enabled := true

var _map: MapData = null
var _material: ShaderMaterial = null
var _color_texture: ImageTexture = null
var _info_texture: ImageTexture = null
var _last_colors := PackedColorArray()
var _last_owners := PackedInt32Array()
var _last_kreise := PackedInt32Array()
var _hovered := -1
var _mouse_in_window := true
var _location_diameter := 1.0
var _detail_size := -1.0
var _kreis_mode := false
## Eisenbahnen über der Karte (seit M7), nur im Kartenmodus „Eisenbahnen“ sichtbar.
var _railways: RailwayLayer = null
## Maßstab des Rahmens (1 bei 1024 Pixeln Kartenbreite).
var _frame_unit := 1.0

@onready var camera: MapCamera = $MapCamera
@onready var _sprite: Sprite2D = $MapSprite


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
	_location_diameter = map.typical_location_diameter()
	_frame_unit = maxf(float(map.size.x) / 1024.0, 1.0)

	var lut := MapData.lut_size(map.map_count())
	_color_texture = ImageTexture.create_from_image(
		Image.create_empty(lut.x, lut.y, false, Image.FORMAT_RGBA8)
	)
	_info_texture = ImageTexture.create_from_image(
		Image.create_empty(lut.x, lut.y, false, Image.FORMAT_RGBA8)
	)
	_last_colors = PackedColorArray()
	_last_owners = PackedInt32Array()
	_last_kreise = PackedInt32Array()

	_material = ShaderMaterial.new()
	_material.shader = MAP_SHADER
	_material.set_shader_parameter(&"id_map", map.texture)
	if map.river_texture != null:
		_material.set_shader_parameter(&"river_map", map.river_texture)
	_material.set_shader_parameter(&"color_lut", _color_texture)
	_material.set_shader_parameter(&"info_lut", _info_texture)
	_material.set_shader_parameter(&"grain_map", _make_grain_texture())
	_material.set_shader_parameter(&"lut_columns", MapData.LUT_COLUMNS)
	_material.set_shader_parameter(&"map_size", map.size)
	_material.set_shader_parameter(&"selected_index", -1)
	_material.set_shader_parameter(&"hovered_index", -1)
	_sprite.material = _material
	if _railways == null:
		_railways = RailwayLayer.new()
		_railways.name = "Railways"
		_railways.visible = false
		add_child(_railways)
		move_child(_railways, _sprite.get_index() + 1)
	_detail_size = -1.0
	_update_detail()
	queue_redraw()


func has_map() -> bool:
	return _map != null and _map.is_valid()


func map_data() -> MapData:
	return _map


## Weltrechteck der Karte (Kartenpixel = Welteinheiten).
func map_rect() -> Rect2:
	if not has_map():
		return Rect2(Vector2.ZERO, Vector2(1024, 1152))
	return Rect2(_sprite.position, Vector2(_map.size))


## Typischer Ortsdurchmesser in Kartenpixeln (Grundlage der Detailstufen).
func location_diameter() -> float:
	return _location_diameter


## Bildschirmgröße eines typischen Ortes beim aktuellen Zoom (MapDetail).
func detail_size() -> float:
	return MapDetail.screen_size(camera.zoom.x, _location_diameter)


## Farben je Simulationsindex (get_map_colors) in die Farbtabelle schreiben.
## Alpha 0 (keine Daten) bleibt erhalten; der Shader schraffiert diese Orte.
func set_location_colors(colors: PackedColorArray) -> void:
	if not has_map() or (colors == _last_colors and not _last_colors.is_empty()):
		return
	_last_colors = colors
	var lut := MapData.lut_size(_map.map_count())
	var table := PackedColorArray()
	table.resize(lut.x * lut.y)
	for map_index in _map.map_count():
		var sim_index := _map.map_to_sim[map_index]
		if sim_index >= 0 and sim_index < colors.size():
			table[map_index] = colors[sim_index]
		else:
			table[map_index] = UNKNOWN_MARKER
	var image := Image.create_from_data(
		lut.x, lut.y, false, Image.FORMAT_RGBAF, table.to_byte_array()
	)
	image.convert(Image.FORMAT_RGBA8)
	_color_texture.update(image)


## Besitzer (Staatsindex) und Kreis (Kreisindex) je Simulationsindex, -1 = keiner;
## Grundlage der Staats- und Kreisgrenzen.
func set_location_info(owners: PackedInt32Array, kreise: PackedInt32Array) -> void:
	if not has_map():
		return
	if owners == _last_owners and kreise == _last_kreise and not _last_owners.is_empty():
		return
	_last_owners = owners
	_last_kreise = kreise
	var lut := MapData.lut_size(_map.map_count())
	var bytes := PackedByteArray()
	bytes.resize(lut.x * lut.y * 4)
	for map_index in _map.map_count():
		var sim_index := _map.map_to_sim[map_index]
		var owner := 0
		var kreis := 0
		if sim_index >= 0:
			if sim_index < owners.size() and owners[sim_index] >= 0:
				owner = owners[sim_index] + 1
			if sim_index < kreise.size() and kreise[sim_index] >= 0:
				kreis = kreise[sim_index] + 1
		var offset := map_index * 4
		bytes[offset] = owner & 0xFF
		bytes[offset + 1] = (owner >> 8) & 0xFF
		bytes[offset + 2] = kreis & 0xFF
		bytes[offset + 3] = (kreis >> 8) & 0xFF
	_info_texture.update(Image.create_from_data(lut.x, lut.y, false, Image.FORMAT_RGBA8, bytes))


## Im Kartenmodus „Kreise“ bleiben Kreisgrenzen auch beim Herauszoomen sichtbar.
## Eisenbahnabschnitte als Paare von Simulationsindizes (seit M7); open je Abschnitt.
func set_railways(sections: PackedInt32Array, open: Array[bool]) -> void:
	if _railways == null:
		return
	var open_points := PackedVector2Array()
	var planned_points := PackedVector2Array()
	for i in range(0, sections.size() - 1, 2):
		var a := location_center(sections[i])
		var b := location_center(sections[i + 1])
		if a.x < 0.0 or b.x < 0.0:
			continue
		# Gepackte Arrays sind Werttypen: getrennt anhängen statt über eine Variable.
		if open[i / 2]:
			open_points.append_array([a, b])
		else:
			planned_points.append_array([a, b])
	_railways.set_sections(open_points, planned_points, _location_diameter)


func set_railways_visible(enabled: bool) -> void:
	if _railways != null:
		_railways.visible = enabled


func railway_layer() -> RailwayLayer:
	return _railways


func set_kreis_mode(enabled: bool) -> void:
	_kreis_mode = enabled
	_detail_size = -1.0
	_update_detail()


func set_selected(sim_index: int) -> void:
	if _material != null:
		_material.set_shader_parameter(&"selected_index", _map_index_of(sim_index))


func get_hovered() -> int:
	return _hovered


## Weltposition des Beschriftungspunkts eines Ortes oder Vector2(-1, -1).
func location_center(sim_index: int) -> Vector2:
	if not has_map():
		return Vector2(-1, -1)
	var point := _map.label_point_of_sim(sim_index)
	if point.x < 0.0:
		return point
	return _sprite.position + point


## Auswahlfunktion der Karte: Ort (Simulationsindex) unter einem Punkt in
## Bildschirmkoordinaten des Viewports oder -1. Klick und Hervorhebung nutzen sie.
func location_at_screen(screen_point: Vector2) -> int:
	if not has_map():
		return -1
	return location_at_world(camera.screen_to_world(screen_point))


## Ort (Simulationsindex) an einem Weltpunkt oder -1.
func location_at_world(world: Vector2) -> int:
	if not has_map():
		return -1
	var local := world - _sprite.position
	return _map.sim_index_at_pixel(Vector2i(floori(local.x), floori(local.y)))


## Flächenart (MapData.Area) an einem Weltpunkt oder -1 außerhalb der Karte.
func area_at_world(world: Vector2) -> int:
	if not has_map():
		return -1
	var local := world - _sprite.position
	return _map.area_at_pixel(Vector2i(floori(local.x), floori(local.y)))


func _notification(what: int) -> void:
	if what == NOTIFICATION_WM_MOUSE_EXIT:
		_mouse_in_window = false
	elif what == NOTIFICATION_WM_MOUSE_ENTER:
		_mouse_in_window = true


func _process(_delta: float) -> void:
	if not has_map():
		return
	_update_detail()
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
	location_clicked.emit(location_at_screen(button.position))
	get_viewport().set_input_as_handled()


## Zoomabhängige Deckkraft der Kreis- und Ortsgrenzen (MapDetail).
func _update_detail() -> void:
	if _material == null:
		return
	var size := detail_size()
	if is_equal_approx(size, _detail_size):
		return
	_detail_size = size
	var kreis_alpha := MapDetail.kreis_border_alpha(size)
	if _kreis_mode:
		kreis_alpha = maxf(kreis_alpha, KREIS_MODE_BORDER_ALPHA)
	_material.set_shader_parameter(&"kreis_border_alpha", kreis_alpha)
	_material.set_shader_parameter(
		&"location_border_alpha", MapDetail.location_border_alpha(size)
	)


func _sim_index_under_mouse() -> int:
	var viewport := get_viewport()
	if viewport.gui_get_hovered_control() != null:
		return -1
	var mouse := viewport.get_mouse_position()
	if not viewport.get_visible_rect().has_point(mouse):
		return -1
	return location_at_screen(mouse)


func _map_index_of(sim_index: int) -> int:
	if not has_map():
		return -1
	return _map.map_index_of_sim(sim_index)


## Feine, wiederholbare Körnung für den Papierton (fester Startwert).
static func _make_grain_texture() -> ImageTexture:
	var random := RandomNumberGenerator.new()
	random.seed = GRAIN_SEED
	var bytes := PackedByteArray()
	bytes.resize(GRAIN_SIZE * GRAIN_SIZE)
	for i in bytes.size():
		bytes[i] = random.randi_range(0, 255)
	var image := Image.create_from_data(GRAIN_SIZE, GRAIN_SIZE, false, Image.FORMAT_L8, bytes)
	return ImageTexture.create_from_image(image)


func _draw() -> void:
	if not has_map():
		return
	var unit := _frame_unit
	var rect := map_rect()
	var inner := FRAME_INNER * unit
	var band := FRAME_BAND * unit
	var outer := rect.grow(inner + band)
	# Von außen nach innen: Papierrand, Zierlinie, Gradrahmen, Innenlinie. Nur Ringe,
	# keine Flächen unter der Karte: Beim Hineinzoomen bedecken sonst mehrere
	# bildschirmfüllende Rechtecke einander (teuer bei Software-Rendering).
	_draw_ring(outer.grow(48.0 * unit), outer.grow(7.0 * unit), FRAME_MARGIN)
	_draw_ring(outer.grow(7.0 * unit), outer.grow(6.0 * unit), FRAME_INK)
	_draw_ring(outer.grow(6.0 * unit), outer, FRAME_MARGIN)
	_draw_ring(outer, outer.grow(-unit), FRAME_INK)
	_draw_ring(outer.grow(-unit), rect.grow(inner), FRAME_PAPER)
	if _map.config != null:
		_draw_degree_band(rect)
	_draw_ring(rect.grow(inner), rect.grow(inner - unit), FRAME_INK)
	_draw_ring(rect.grow(inner - unit), rect, FRAME_PAPER)
	if _map.config != null:
		_draw_degree_labels(rect)


## Fläche zwischen outer und inner (inner liegt ganz in outer).
func _draw_ring(outer: Rect2, inner: Rect2, color: Color) -> void:
	draw_rect(Rect2(outer.position, Vector2(outer.size.x, inner.position.y - outer.position.y)),
		color)
	draw_rect(Rect2(Vector2(outer.position.x, inner.end.y),
		Vector2(outer.size.x, outer.end.y - inner.end.y)), color)
	draw_rect(Rect2(Vector2(outer.position.x, inner.position.y),
		Vector2(inner.position.x - outer.position.x, inner.size.y)), color)
	draw_rect(Rect2(Vector2(inner.end.x, inner.position.y),
		Vector2(outer.end.x - inner.end.x, inner.size.y)), color)


## Abwechselnd dunkle Felder je halbem Grad entlang des Rahmens.
func _draw_degree_band(rect: Rect2) -> void:
	var config := _map.config
	var unit := _frame_unit
	var pixel_scale := Vector2(rect.size.x / config.image_width, rect.size.y / config.image_height)
	var thickness := (FRAME_BAND - 2.0) * unit
	var top := rect.position.y - (FRAME_INNER + FRAME_BAND - 1.0) * unit
	var bottom := rect.end.y + (FRAME_INNER + 1.0) * unit
	var left := rect.position.x - (FRAME_INNER + FRAME_BAND - 1.0) * unit
	var right := rect.end.x + (FRAME_INNER + 1.0) * unit
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
	var unit := _frame_unit
	var pixel_scale := Vector2(rect.size.x / config.image_width, rect.size.y / config.image_height)
	# Höchstens ein Drittel des Gradabstands, damit sich die Zahlen nicht berühren.
	var degree_width := (
		config.geo_to_pixel(config.longitude_min + 1.0, 0.0).x
		- config.geo_to_pixel(config.longitude_min, 0.0).x
	) * pixel_scale.x
	var font_size := maxi(roundi(minf(FRAME_LABEL_SIZE * unit, degree_width / 3.2)), 8)
	var offset := (FRAME_INNER + FRAME_BAND + 12.0) * unit
	for degree in range(ceili(config.longitude_min), floori(config.longitude_max) + 1):
		var x := rect.position.x + config.geo_to_pixel(degree, 0.0).x * pixel_scale.x
		var text := "%d° O" % degree
		var width := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x
		draw_string(font, Vector2(x - width * 0.5, rect.position.y - offset), text,
			HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, FRAME_LABEL)
		draw_string(font, Vector2(x - width * 0.5, rect.end.y + offset + font_size * 0.8),
			text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, FRAME_LABEL)
	for degree in range(ceili(config.latitude_min), floori(config.latitude_max) + 1):
		var y := rect.position.y + config.geo_to_pixel(0.0, degree).y * pixel_scale.y
		var text := "%d° N" % degree
		var width := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x
		draw_string(font, Vector2(rect.position.x - offset - width, y + font_size * 0.35),
			text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, FRAME_LABEL)
		draw_string(font, Vector2(rect.end.x + offset, y + font_size * 0.35), text,
			HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, FRAME_LABEL)
