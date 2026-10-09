class_name MapLabels
extends Control
## Ortsnamen auf der Karte, im Bildschirmraum gezeichnet (scharf bei jedem Zoom).
##
## Reihenfolge nach Rang (Hauptstädte, Städte, übrige Orte); ein Name entfällt,
## wenn er einen bereits gesetzten überdecken würde. Je weiter hineingezoomt
## wird, desto mehr Ränge erscheinen.

enum Rank { PLACE, TOWN, CAPITAL }

## Mindestzoom je Rang (Index = Rank).
const MIN_ZOOM: PackedFloat32Array = [1.35, 0.85, 0.0]
const FONT_SIZES: PackedInt32Array = [13, 14, 15]
const OUTLINE_SIZE := 4
const LABEL_GAP := 3.0
const SERIF_FONTS: PackedStringArray = [
	"Noto Serif", "Source Serif Pro", "Georgia", "Cambria", "DejaVu Serif", "Liberation Serif",
	"Times New Roman", "serif"
]

var camera: Camera2D = null
## Einträge: { "world": Vector2, "text": String, "rank": Rank, "weight": float }.
var _entries: Array[Dictionary] = []
var _regular: SystemFont = null
var _bold: SystemFont = null
var _last_transform := Transform2D()


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	resized.connect(queue_redraw)
	_regular = SystemFont.new()
	_regular.font_names = SERIF_FONTS
	_bold = SystemFont.new()
	_bold.font_names = SERIF_FONTS
	_bold.font_weight = 700


## Setzt die Namen; entries wie oben, Reihenfolge beliebig.
func set_entries(entries: Array[Dictionary]) -> void:
	_entries = entries.duplicate()
	_entries.sort_custom(
		func(a: Dictionary, b: Dictionary) -> bool:
			if int(a["rank"]) != int(b["rank"]):
				return int(a["rank"]) > int(b["rank"])
			return float(a["weight"]) > float(b["weight"])
	)
	queue_redraw()


func _process(_delta: float) -> void:
	if camera == null:
		return
	var current := Transform2D(0.0, camera.zoom, 0.0, camera.position)
	if current != _last_transform:
		_last_transform = current
		queue_redraw()


func _draw() -> void:
	if camera == null or _entries.is_empty():
		return
	var zoom := camera.zoom.x
	var half := get_viewport_rect().size * 0.5
	var placed: Array[Rect2] = []
	for entry in _entries:
		var rank := int(entry["rank"])
		if zoom < MIN_ZOOM[rank]:
			continue
		var font: Font = _bold if rank == Rank.CAPITAL else _regular
		var font_size := FONT_SIZES[rank]
		var text := str(entry["text"])
		var text_size := font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size)
		var world: Vector2 = entry["world"]
		var anchor := (world - camera.position) * zoom + half
		var rect := Rect2(anchor - text_size * 0.5, text_size).grow(LABEL_GAP)
		if not get_rect().intersects(rect):
			continue
		var blocked := false
		for other in placed:
			if other.intersects(rect):
				blocked = true
				break
		if blocked:
			continue
		placed.append(rect)
		var baseline := Vector2(
			anchor.x - text_size.x * 0.5, anchor.y + font.get_ascent(font_size) - text_size.y * 0.5
		)
		draw_string_outline(font, baseline, text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size,
			OUTLINE_SIZE, Color(UiPalette.PAPER, 0.85))
		draw_string(font, baseline, text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, UiPalette.INK)
