class_name RailwayLayer
extends Node2D
## Eisenbahnstrecken als Linien über der Karte (seit M7, Kartenmodus „Eisenbahnen“).
##
## Punkte in Weltkoordinaten der Karte, paarweise (Anfang, Ende je Abschnitt). Eröffnete
## Abschnitte als durchgezogene Tuschelinie, spätere gestrichelt und blasser – aber nur, wenn sie in
## den nächsten PLANNED_YEARS Jahren eröffnen (1648 also keine).

## Spätere Abschnitte erscheinen so viele Jahre vor ihrer Eröffnung.
const PLANNED_YEARS := 5
const INK := Color("2b2116")
const PLANNED := Color(0.17, 0.13, 0.09, 0.45)
## Linienbreite als Anteil des typischen Ortsdurchmessers (Kartenpixel).
const WIDTH_FACTOR := 0.09

var _open := PackedVector2Array()
var _planned := PackedVector2Array()
var _width := 1.0


## Abschnitte übernehmen; diameter ist der typische Ortsdurchmesser der Karte.
func set_sections(open: PackedVector2Array, planned: PackedVector2Array, diameter: float) -> void:
	_open = open
	_planned = planned
	_width = maxf(diameter * WIDTH_FACTOR, 0.5)
	queue_redraw()


## Ob die Karte einen Abschnitt aus get_railways zum Datum date ({ year, month, day }) zeichnet.
static func is_shown(section: Dictionary, date: Dictionary) -> bool:
	if bool(section.get("open", false)):
		return true
	var opened: Variant = section.get("opened", {})
	if not opened is Dictionary or (opened as Dictionary).is_empty():
		return false
	return _day_number(opened as Dictionary) <= _day_number(date) + PLANNED_YEARS * 10000


static func _day_number(date: Dictionary) -> int:
	return int(date.get("year", 0)) * 10000 + int(date.get("month", 0)) * 100 + int(date.get("day", 0))


func open_count() -> int:
	return _open.size() / 2


func planned_count() -> int:
	return _planned.size() / 2


func _draw() -> void:
	if _open.size() >= 2:
		draw_multiline(_open, INK, _width, true)
		# Knoten an den Enden, damit die Linien an Bahnhöfen geschlossen wirken.
		for point in _open:
			draw_circle(point, _width * 0.9, INK)
	for i in range(0, _planned.size() - 1, 2):
		draw_dashed_line(_planned[i], _planned[i + 1], PLANNED, _width * 0.7, _width * 3.0, true, true)
