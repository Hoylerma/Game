class_name ArmyLayer
extends Node2D
## Heere über der Karte (seit M10, Schritt b3; militaer.md Abschnitt 8).
##
## Je Ort eine Reihe von Marken: Fähnchen in der Staatsfarbe mit der Stärke in Tausend, das
## ausgewählte Heer mit Rahmen und seinem Weg, dazu Schlachten (gekreuzte Säbel) und Belagerungen
## (Ring). Die Marken behalten auf dem Bildschirm ihre Größe; Positionen sind Weltkoordinaten der
## Karte. Garnisonen zeigt die Ebene nur auf Wunsch (Kartenmodus „Krieg“).

## Höhe einer Marke in Bildschirmpixeln.
const MARKER_SCREEN := 15.0
const INK := Color("2b2116")
const PAPER := Color("f4ecda")
const SELECTION := Color("8e2b1d")
const ROUTE := Color(0.56, 0.17, 0.11, 0.9)
const PREVIEW := Color(0.17, 0.13, 0.09, 0.7)
const BATTLE := Color("9a2d22")

var show_garrisons := false

## Je Eintrag { id, position, color, soldiers, garrison, staff, own }.
var _markers: Array[Dictionary] = []
## Rechteck je Marke (Weltkoordinaten) für army_at, gleiche Reihenfolge wie _markers.
var _rects: Array[Rect2] = []
var _battles := PackedVector2Array()
var _sieges := PackedVector2Array()
var _route := PackedVector2Array()
var _preview := PackedVector2Array()
var _selected := -1
var _zoom := 1.0


## Kurztext der Stärke: Tausend gerundet („93“), unter tausend „<1“.
static func strength_text(soldiers: int) -> String:
	if soldiers < 1000:
		return "<1"
	return GermanFormat.integer(roundi(soldiers / 1000.0))


## Heere übernehmen: armies wie get_armies, positions je Heer (Weltpunkt seines Ortes),
## colors je Heer (Staatsfarbe), own je Heer (gehört dem Spieler).
func set_armies(armies: Array[Dictionary], positions: PackedVector2Array, colors: PackedColorArray,
		own: Array[bool]) -> void:
	_markers.clear()
	for i in armies.size():
		var army := armies[i]
		if positions[i].x < 0.0:
			continue
		if bool(army.get("garrison", false)) and not show_garrisons:
			continue
		_markers.append({
			"id": int(army.get("id", -1)),
			"position": positions[i],
			"color": colors[i],
			"soldiers": int(army.get("soldiers", 0)),
			"garrison": bool(army.get("garrison", false)),
			"staff": bool(army.get("staff", false)),
			"own": own[i],
		})
	_layout()
	queue_redraw()


## Schlachten und Belagerungen als Weltpunkte.
func set_events(battles: PackedVector2Array, sieges: PackedVector2Array) -> void:
	_battles = battles
	_sieges = sieges
	queue_redraw()


## Ausgewähltes Heer (-1 keines) mit seinem Weg (Weltpunkte ab dem Standort).
func set_selection(army_id: int, route: PackedVector2Array) -> void:
	_selected = army_id
	_route = route
	queue_redraw()


## Vorschau eines Marschbefehls (leer: keine).
func set_preview(route: PackedVector2Array) -> void:
	_preview = route
	queue_redraw()


func set_zoom(zoom: float) -> void:
	_zoom = maxf(zoom, 0.0001)
	_layout()
	queue_redraw()


func marker_count() -> int:
	return _markers.size()


func selected_army() -> int:
	return _selected


## Heer unter einem Weltpunkt (oberste Marke zuerst) oder -1.
func army_at(world_point: Vector2) -> int:
	for i in range(_rects.size() - 1, -1, -1):
		if _rects[i].has_point(world_point):
			return int(_markers[i]["id"])
	return -1


## Weltpunkt der Marke eines Heeres (Mitte) oder Vector2(-1, -1).
func marker_center(army_id: int) -> Vector2:
	for i in _markers.size():
		if int(_markers[i]["id"]) == army_id:
			return _rects[i].get_center()
	return Vector2(-1, -1)


func _unit() -> float:
	return MARKER_SCREEN / _zoom


## Marken je Ort nebeneinander, über dem Ortsmittelpunkt.
func _layout() -> void:
	_rects.clear()
	var unit := _unit()
	var per_place := {}
	for marker in _markers:
		var key: Vector2 = marker["position"]
		var slot := int(per_place.get(key, 0))
		per_place[key] = slot + 1
		var height := unit * (0.7 if bool(marker["garrison"]) else 1.0)
		var width := height * 1.9
		var origin: Vector2 = key + Vector2(-width * 0.5 + slot * (width + unit * 0.15), -height * 1.35)
		_rects.append(Rect2(origin, Vector2(width, height)))


func _draw() -> void:
	var unit := _unit()
	var line := maxf(unit * 0.09, 0.05)
	if _route.size() >= 2:
		for i in _route.size() - 1:
			draw_dashed_line(_route[i], _route[i + 1], ROUTE, line * 2.0, unit * 0.5, true, true)
		draw_circle(_route[_route.size() - 1], unit * 0.22, ROUTE)
	if _preview.size() >= 2:
		for i in _preview.size() - 1:
			draw_dashed_line(_preview[i], _preview[i + 1], PREVIEW, line * 1.5, unit * 0.35, true, true)
	for point in _sieges:
		draw_arc(point, unit * 0.75, 0.0, TAU, 24, INK, line * 2.0, true)
	for point in _battles:
		var r := unit * 0.6
		draw_line(point + Vector2(-r, -r), point + Vector2(r, r), BATTLE, line * 3.0, true)
		draw_line(point + Vector2(-r, r), point + Vector2(r, -r), BATTLE, line * 3.0, true)
	var font := ThemeDB.fallback_font
	for i in _markers.size():
		var marker := _markers[i]
		var rect := _rects[i]
		var color: Color = marker["color"]
		# Fahnenstange zum Ort, Fähnchen mit Rand, Stärke in Tausend.
		var pole_foot: Vector2 = marker["position"]
		draw_line(Vector2(rect.position.x, rect.end.y), pole_foot, INK, line, true)
		draw_rect(rect, color)
		var selected := int(marker["id"]) == _selected
		draw_rect(rect, SELECTION if selected else INK, false, line * (3.0 if selected else 1.0))
		if bool(marker["own"]) and not bool(marker["staff"]):
			draw_rect(rect.grow(-line * 2.5), PAPER, false, line)
		var size := maxi(roundi(rect.size.y * 0.72 * _zoom), 6)
		var text := strength_text(int(marker["soldiers"]))
		var text_color := INK if color.get_luminance() > 0.45 else PAPER
		var width := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, size).x / _zoom
		draw_set_transform(Vector2(rect.get_center().x - width * 0.5, rect.end.y - rect.size.y * 0.22),
			0.0, Vector2.ONE / _zoom)
		draw_string(font, Vector2.ZERO, text, HORIZONTAL_ALIGNMENT_LEFT, -1, size, text_color)
		draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE)
