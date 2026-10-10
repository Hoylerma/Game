class_name MapLabels
extends Control
## Namen auf der Karte, im Bildschirmraum gezeichnet (scharf bei jedem Zoom).
##
## Drei Ebenen nach Zoomstufe (MapDetail): fern Staatsnamen, mittel Kreisnamen,
## nah Ortsnamen; je Zoom ist genau eine Ebene sichtbar. Jede Ebene liegt in einem
## Weltraster (CELL_SIZE Kartenpixel), damit je Bild nur die Namen der sichtbaren
## Zellen geprüft werden, nicht alle 4.500 Orte. Innerhalb einer Ebene gilt die
## Rangfolge der Einträge (Hauptstädte, Städte, übrige Orte; größere zuerst).
## Überlappungen verhindert ein Belegungsraster im Bildschirmraum: Ein Ortsname,
## der eine schon belegte Zelle berühren würde, weicht eine Zeile nach oben oder
## unten aus; ist auch dort kein Platz, entfällt er (Kreis- und Staatsnamen
## entfallen sofort). Der Name des ausgewählten Ortes (set_pinned) kommt zuerst.

## Darstellungsarten der Namen.
enum Style { PLACE, CITY, CAPITAL, KREIS, STATE }

## Kantenlänge der Weltrasterzellen in Kartenpixeln.
const CELL_SIZE := 256.0
## Kantenlänge der Belegungszellen in Bildschirmpixeln.
const OCCUPANCY_CELL := 8
## Schriftgrößen je Stil (STATE: Mindestgröße, wächst mit der Fläche).
const FONT_SIZES: PackedInt32Array = [13, 14, 15, 13, 13]
const STATE_FONT_MAX := 30
## Schriftgröße eines Staatsnamens: Faktor × Wurzel der Fläche (Pixel) × Zoom.
const STATE_FONT_SCALE := 0.11
const OUTLINE_SIZE := 4
const LABEL_GAP := 2.0
const SERIF_FONTS: PackedStringArray = [
	"Noto Serif", "Source Serif Pro", "Georgia", "Cambria", "DejaVu Serif", "Liberation Serif",
	"Times New Roman", "serif"
]
## Für Kreisnamen: Familien mit echter Kursive zuerst (DejaVu Serif hat keine).
const ITALIC_FONTS: PackedStringArray = [
	"Noto Serif", "Source Serif Pro", "Georgia", "Cambria", "Liberation Serif",
	"Times New Roman", "FreeSerif", "DejaVu Serif", "serif"
]

## Kamera der Kartenansicht; ohne sie wird nichts gezeichnet.
var camera: MapCamera = null
## Bedienelemente über der Karte; unter ihnen erscheinen keine Namen.
var covering_controls: Array[Control] = []
## Typischer Ortsdurchmesser in Kartenpixeln (MapView.location_diameter).
var location_diameter := 36.0

## Zeitbedarf des letzten Zeichnens in Mikrosekunden, Zahl der gezeichneten Namen und
## Zahl der Zeichenvorgänge insgesamt (Leistungstest).
var last_draw_usec := 0
var last_drawn := 0
var draw_count := 0

var _layers: Array[LabelLayer] = []
var _fonts: Array[Font] = []
var _colors: Array[Color] = []
var _occupancy := PackedByteArray()
var _occupancy_size := Vector2i.ZERO
var _last_transform := Transform2D()
var _last_size := Vector2.ZERO
## Name des ausgewählten Ortes: [Weltpunkt, Text, Stil] oder leer.
var _pinned: Array = []


## Namen einer Ebene mit Weltraster und Breitenspeicher.
class LabelLayer:
	extends RefCounted
	var positions := PackedVector2Array()
	var texts := PackedStringArray()
	var styles := PackedByteArray()
	## Fläche in Kartenpixeln (Größe der Staatsnamen).
	var weights := PackedFloat32Array()
	## Gemessene Breiten (−1 = noch nicht gemessen; nicht für STATE).
	var widths := PackedFloat32Array()
	## Weltrasterzelle → Indizes (aufsteigend = Rangfolge).
	var grid := {}

	func size() -> int:
		return texts.size()


func _init() -> void:
	for _tier in MapDetail.Tier.size():
		_layers.append(LabelLayer.new())


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	resized.connect(queue_redraw)
	var regular := SystemFont.new()
	regular.font_names = SERIF_FONTS
	var semibold := SystemFont.new()
	semibold.font_names = SERIF_FONTS
	semibold.font_weight = 600
	var bold := SystemFont.new()
	bold.font_names = SERIF_FONTS
	bold.font_weight = 700
	var italic_base := SystemFont.new()
	italic_base.font_names = ITALIC_FONTS
	italic_base.font_italic = true
	var italic := FontVariation.new()
	italic.base_font = italic_base
	italic.spacing_glyph = 1
	var spaced := FontVariation.new()
	spaced.base_font = bold
	spaced.spacing_glyph = 3
	_fonts.assign([regular, semibold, bold, italic, spaced])
	_colors.assign([
		UiPalette.INK, UiPalette.INK, UiPalette.INK, UiPalette.INK_SOFT, Color("4a2a1c")
	])


## Setzt die Namen einer Ebene (MapDetail.Tier). entries: Dictionaries
## { "world": Vector2, "text": String, "style": Style, "rank": float, "weight": float };
## höherer rank zuerst, weight = Fläche in Kartenpixeln.
func set_layer(tier: int, entries: Array[Dictionary]) -> void:
	var sorted := entries.duplicate()
	sorted.sort_custom(
		func(a: Dictionary, b: Dictionary) -> bool:
			if float(a["rank"]) != float(b["rank"]):
				return float(a["rank"]) > float(b["rank"])
			return str(a["text"]) < str(b["text"])
	)
	var layer := LabelLayer.new()
	for entry: Dictionary in sorted:
		var world: Vector2 = entry["world"]
		var index := layer.size()
		layer.positions.append(world)
		layer.texts.append(str(entry["text"]))
		layer.styles.append(int(entry["style"]))
		layer.weights.append(float(entry.get("weight", 0.0)))
		layer.widths.append(-1.0)
		var key := Vector2i((world / CELL_SIZE).floor())
		var bucket: PackedInt32Array = layer.grid.get(key, PackedInt32Array())
		bucket.append(index)
		layer.grid[key] = bucket
	_layers[tier] = layer
	queue_redraw()


## Name des ausgewählten Ortes, der in der Ortsebene immer zuerst gesetzt wird;
## leerer Text hebt ihn auf.
func set_pinned(world: Vector2, text: String, style: int) -> void:
	_pinned = [] if text.is_empty() else [world, text, style]
	queue_redraw()


## Anzahl der Namen einer Ebene.
func layer_size(tier: int) -> int:
	return _layers[tier].size()


func _process(_delta: float) -> void:
	if camera == null:
		return
	var current := Transform2D(0.0, camera.zoom, 0.0, camera.position)
	if current != _last_transform:
		_last_transform = current
		queue_redraw()


func _draw() -> void:
	var started := Time.get_ticks_usec()
	last_drawn = 0
	draw_count += 1
	if camera == null or _fonts.is_empty():
		return
	var detail := MapDetail.screen_size(camera.zoom.x, location_diameter)
	_reset_occupancy()
	var tiers: Array[int] = []
	for tier in MapDetail.Tier.size():
		if MapDetail.tier_alpha(tier, detail) >= 0.02 and _layers[tier].size() > 0:
			tiers.append(tier)
	# Die deutlichere Ebene zuerst: Sie bekommt die freien Plätze.
	tiers.sort_custom(
		func(a: int, b: int) -> bool:
			return MapDetail.tier_alpha(a, detail) > MapDetail.tier_alpha(b, detail)
	)
	for tier in tiers:
		var alpha := MapDetail.tier_alpha(tier, detail)
		if tier == MapDetail.Tier.PLACE and not _pinned.is_empty():
			_place(_pinned[0], str(_pinned[1]), int(_pinned[2]), -1.0, alpha, true)
		_draw_layer(_layers[tier], alpha)
	last_draw_usec = Time.get_ticks_usec() - started


func _draw_layer(layer: LabelLayer, alpha: float) -> void:
	var zoom := camera.zoom.x
	# Namen, deren Punkt knapp außerhalb liegt, ragen noch ins Bild.
	var world_rect := camera.visible_world_rect().grow(160.0 / zoom)
	var first := Vector2i((world_rect.position / CELL_SIZE).floor())
	var last := Vector2i((world_rect.end / CELL_SIZE).floor())
	var candidates := PackedInt32Array()
	for cell_y in range(first.y, last.y + 1):
		for cell_x in range(first.x, last.x + 1):
			var key := Vector2i(cell_x, cell_y)
			if layer.grid.has(key):
				candidates.append_array(layer.grid[key])
	candidates.sort()
	var pinned_world := Vector2(-1, -1)
	if not _pinned.is_empty():
		pinned_world = _pinned[0]
	for index in candidates:
		# Der ausgewählte Ort steht schon (set_pinned).
		if layer.positions[index] == pinned_world and layer.texts[index] == str(_pinned[1]):
			continue
		var style := layer.styles[index]
		var width := _place(
			layer.positions[index], layer.texts[index], style, layer.widths[index], alpha,
			style <= Style.CAPITAL, layer.weights[index]
		)
		if style != Style.STATE:
			layer.widths[index] = width


## Setzt einen Namen, wenn Platz ist (Ortsnamen auch eine Zeile höher oder tiefer).
## width < 0: noch nicht gemessen. Liefert die gemessene Breite.
func _place(
	world: Vector2,
	text: String,
	style: int,
	width: float,
	alpha: float,
	may_shift: bool,
	weight: float = 0.0
) -> float:
	var zoom := camera.zoom.x
	var font: Font = _fonts[style]
	var font_size := FONT_SIZES[style]
	if style == Style.STATE:
		font_size = clampi(
			roundi(STATE_FONT_SCALE * sqrt(weight) * zoom), FONT_SIZES[style], STATE_FONT_MAX
		)
		width = -1.0
	if width < 0.0:
		width = font.get_string_size(text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x
	var height := font.get_height(font_size)
	var anchor := (world - camera.position) * zoom + get_viewport_rect().size * 0.5
	var rect := Rect2(anchor - Vector2(width, height) * 0.5, Vector2(width, height))
	if not get_rect().intersects(rect):
		return width
	var shifts := PackedFloat32Array([0.0])
	if may_shift:
		shifts.append_array([-height, height])
	for shift in shifts:
		var candidate := Rect2(rect.position + Vector2(0.0, shift), rect.size)
		if not _claim(candidate.grow(LABEL_GAP)):
			continue
		var baseline := candidate.position + Vector2(0.0, font.get_ascent(font_size))
		draw_string_outline(font, baseline, text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size,
			OUTLINE_SIZE, Color(UiPalette.PAPER, 0.85 * alpha))
		draw_string(font, baseline, text, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size,
			Color(_colors[style], _colors[style].a * alpha))
		last_drawn += 1
		break
	return width


func _reset_occupancy() -> void:
	var view := get_rect().size
	if view != _last_size:
		_last_size = view
		_occupancy_size = Vector2i(
			ceili(view.x / OCCUPANCY_CELL) + 1, ceili(view.y / OCCUPANCY_CELL) + 1
		)
		_occupancy.resize(_occupancy_size.x * _occupancy_size.y)
	_occupancy.fill(0)
	# Unter Panels und Leisten bleibt die Karte ohne Namen.
	var origin := get_global_rect().position
	for control in covering_controls:
		if is_instance_valid(control) and control.is_visible_in_tree():
			var covered := control.get_global_rect()
			covered.position -= origin
			_claim(covered, true)


## Belegt die Zellen von rect, wenn alle frei sind (oder force); sonst false.
func _claim(rect: Rect2, force: bool = false) -> bool:
	var x0 := clampi(floori(rect.position.x / OCCUPANCY_CELL), 0, _occupancy_size.x - 1)
	var y0 := clampi(floori(rect.position.y / OCCUPANCY_CELL), 0, _occupancy_size.y - 1)
	var x1 := clampi(floori(rect.end.x / OCCUPANCY_CELL), 0, _occupancy_size.x - 1)
	var y1 := clampi(floori(rect.end.y / OCCUPANCY_CELL), 0, _occupancy_size.y - 1)
	if not force:
		for y in range(y0, y1 + 1):
			var row := y * _occupancy_size.x
			for x in range(x0, x1 + 1):
				if _occupancy[row + x] != 0:
					return false
	for y in range(y0, y1 + 1):
		var row := y * _occupancy_size.x
		for x in range(x0, x1 + 1):
			_occupancy[row + x] = 1
	return true
