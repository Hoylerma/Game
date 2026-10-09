@tool
class_name CoatOfArms
extends Control
## Schlichter Wappenschild in der Farbe eines Staates (Platzhalter für echte Wappen).

@export var shield_color := Color("8a7558"):
	set(value):
		shield_color = value
		queue_redraw()

@export var outline_color := Color("2b2116"):
	set(value):
		outline_color = value
		queue_redraw()


func _draw() -> void:
	var w := size.x
	var h := size.y
	if w <= 2.0 or h <= 2.0:
		return
	# Halbrunder Schild: gerade Oberkante, Seiten laufen zur Spitze zusammen.
	var points := PackedVector2Array()
	var inset := 1.0
	points.append(Vector2(inset, inset))
	points.append(Vector2(w - inset, inset))
	points.append(Vector2(w - inset, h * 0.45))
	var steps := 10
	for i in range(1, steps + 1):
		var t := float(i) / steps
		var x := lerpf(w - inset, w * 0.5, t)
		var y := lerpf(h * 0.45, h - inset, sin(t * PI * 0.5))
		points.append(Vector2(x, y))
	for i in range(1, steps):
		var t := float(i) / steps
		var x := lerpf(w * 0.5, inset, t)
		var y := lerpf(h - inset, h * 0.45, 1.0 - cos(t * PI * 0.5))
		points.append(Vector2(x, y))
	points.append(Vector2(inset, h * 0.45))
	draw_colored_polygon(points, shield_color)
	# Lichtkante oben links für etwas Prägung.
	var highlight := PackedVector2Array([
		Vector2(inset + 2.0, inset + 2.0),
		Vector2(w * 0.5, inset + 2.0),
		Vector2(inset + 2.0, h * 0.5),
	])
	draw_colored_polygon(highlight, Color(1, 1, 1, 0.16))
	var outline := points.duplicate()
	outline.append(points[0])
	draw_polyline(outline, outline_color, 1.5, true)
