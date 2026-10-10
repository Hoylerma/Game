class_name MapData
extends RefCounted
## Kartendaten des Kernraums: Ort-ID-Bild (provinzen.png), Flussebene (fluesse.png)
## und Zuordnung Kartenindex → Ort mit Beschriftungspunkt und Fläche (definition.csv).
##
## provinzen.png: R + G × 256 = Kartenindex + 1 (0 = kein Ort), B = Flächenart
## (0 Kernraum-Land, 1 Meer, 2 Binnengewässer, 3 Ausland).
## Der Kartenindex ist unabhängig vom Simulationsindex; beide werden über die
## Ort-Kennung (get_location_index) verbunden. Format: chronik-skript.md, Abschnitt 6.

## Flächenarten im Blaukanal von provinzen.png.
enum Area { CORE_LAND = 0, SEA = 1, INLAND_WATER = 2, FOREIGN = 3 }

## Spalten der definition.csv (Fassung M3), in dieser Reihenfolge.
const CSV_COLUMNS: PackedStringArray = [
	"kartenindex", "ort", "name", "x", "y", "pixel", "flaeche_km2"
]
## Kopfzeile der definition.csv vor M3 (nur Kartenindex, Kennung, Name).
const CSV_HEADER_V1 := "kartenindex;ort;name"
const GENERATOR_COMMAND := "python3 tools/kartengenerator/kartengenerator.py"
## Spalten der Nachschlagetexturen. 4.500 Orte passen nicht sicher in eine Zeile
## (Texturbreite im Kompatibilitätsrenderer), daher zweidimensional.
const LUT_COLUMNS := 256
## Anzeigenamen der Flächenarten (Index = Area).
const AREA_NAMES: PackedStringArray = ["Kernraum", "Meer", "Binnengewässer", "Ausland"]

## Bild für die Klick-Erkennung (bleibt im Speicher).
var image: Image = null
## Textur für die Darstellung (Filter NEAREST setzt der Kartenshader).
var texture: ImageTexture = null
## Flussebene mit Mipmaps (Deckung 0..255) oder null, wenn fluesse.png fehlt.
var river_texture: ImageTexture = null
var size := Vector2i.ZERO
## Kartenindex → Ort-Kennung laut CSV ("" = Lücke in der Nummerierung).
var map_ids := PackedStringArray()
## Kartenindex → Anzeigename laut CSV.
var map_names := PackedStringArray()
## Kartenindex → Beschriftungspunkt (Pixelmitte) in Bildkoordinaten,
## Vector2(-1, -1) für einen Ort ohne Pixel.
var label_points := PackedVector2Array()
## Kartenindex → Anzahl Pixel des Ortes.
var pixel_counts := PackedInt32Array()
## Kartenindex → Fläche in km² laut Kartengenerator.
var areas_km2 := PackedInt32Array()
## Kartenindex → Simulationsindex (-1 = nicht auflösbar).
var map_to_sim := PackedInt32Array()
## Simulationsindex → Kartenindex (-1 = Ort nicht auf der Karte).
var sim_to_map := PackedInt32Array()
## Optionale Kartenbeschreibung (karte.chron).
var config: MapConfig = null
## Fataler Fehler (deutsch); leer, wenn die Karte benutzbar ist.
var error := ""
## Nicht fatale Hinweise (deutsch).
var warnings := PackedStringArray()


## Lädt provinzen.png, fluesse.png, definition.csv und (optional) karte.chron.
static func load_from_dir(map_dir: String) -> MapData:
	var data := MapData.new()
	var image_path := map_dir.path_join("provinzen.png")
	var csv_path := map_dir.path_join("definition.csv")
	for path: String in [image_path, csv_path]:
		if not FileAccess.file_exists(path):
			data.error = (
				"Die Kartendatei fehlt: %s\nErzeugen mit (im Repository-Hauptordner):\n    %s"
				% [path, GENERATOR_COMMAND]
			)
			return data
	var loaded := Image.load_from_file(image_path)
	if loaded == null or loaded.is_empty():
		data.error = "Das Kartenbild %s konnte nicht gelesen werden." % image_path
		return data
	if loaded.get_format() != Image.FORMAT_RGB8:
		loaded.convert(Image.FORMAT_RGB8)
	data.image = loaded
	data.size = loaded.get_size()
	data.texture = ImageTexture.create_from_image(loaded)
	var csv_error := data._read_definition(csv_path)
	if not csv_error.is_empty():
		data.error = csv_error
		return data
	data.config = MapConfig.load_from_file(map_dir.path_join("karte.chron"))
	if data.config != null:
		var configured := Vector2i(data.config.image_width, data.config.image_height)
		if configured != data.size:
			data.warnings.append(
				(
					"provinzen.png ist %d×%d Pixel, karte.chron nennt %d×%d; Karte neu erzeugen."
					% [data.size.x, data.size.y, configured.x, configured.y]
				)
			)
	data._load_rivers(map_dir.path_join("fluesse.png"))
	return data


func is_valid() -> bool:
	return error.is_empty() and image != null


## Anzahl der Kartenindizes.
func map_count() -> int:
	return map_ids.size()


## Kartenindex am Pixel oder -1 (kein Ort, außerhalb des Bildes).
func map_index_at_pixel(pixel: Vector2i) -> int:
	if image == null or pixel.x < 0 or pixel.y < 0 or pixel.x >= size.x or pixel.y >= size.y:
		return -1
	var color := image.get_pixel(pixel.x, pixel.y)
	var index := color.r8 + color.g8 * 256 - 1
	return index if index < map_count() else -1


## Flächenart am Pixel (Area) oder -1 außerhalb des Bildes.
func area_at_pixel(pixel: Vector2i) -> int:
	if image == null or pixel.x < 0 or pixel.y < 0 or pixel.x >= size.x or pixel.y >= size.y:
		return -1
	return image.get_pixel(pixel.x, pixel.y).b8


## Simulationsindex am Pixel oder -1.
func sim_index_at_pixel(pixel: Vector2i) -> int:
	var map_index := map_index_at_pixel(pixel)
	if map_index < 0 or map_index >= map_to_sim.size():
		return -1
	return map_to_sim[map_index]


## Verbindet Karten- und Simulationsindizes. Liefert die Kennungen der CSV,
## die die Simulation nicht kennt.
func bind_simulation(sim: SimulationClient) -> PackedStringArray:
	var unresolved := PackedStringArray()
	var location_count := sim.get_location_count() if sim != null else 0
	map_to_sim.resize(map_count())
	map_to_sim.fill(-1)
	sim_to_map.resize(location_count)
	sim_to_map.fill(-1)
	for map_index in map_count():
		var id := map_ids[map_index]
		if id.is_empty() or sim == null:
			continue
		var sim_index := sim.get_location_index(id)
		if sim_index < 0 or sim_index >= location_count:
			unresolved.append(id)
			continue
		map_to_sim[map_index] = sim_index
		sim_to_map[sim_index] = map_index
	return unresolved


## Kartenindex eines Simulationsindex oder -1.
func map_index_of_sim(sim_index: int) -> int:
	if sim_index < 0 or sim_index >= sim_to_map.size():
		return -1
	return sim_to_map[sim_index]


## Beschriftungspunkt eines Ortes (Simulationsindex) in Bildkoordinaten oder Vector2(-1, -1).
func label_point_of_sim(sim_index: int) -> Vector2:
	var map_index := map_index_of_sim(sim_index)
	if map_index < 0 or map_index >= label_points.size():
		return Vector2(-1, -1)
	return label_points[map_index]


## Typischer Durchmesser eines Ortes in Bildpixeln (Wurzel des Medians der Pixelzahlen).
## Grundlage der Zoomstufen (MapDetail); 1, wenn keine Fläche bekannt ist.
func typical_location_diameter() -> float:
	var counts: Array[int] = []
	for count in pixel_counts:
		if count > 0:
			counts.append(count)
	if counts.is_empty():
		return 1.0
	counts.sort()
	return sqrt(float(counts[counts.size() / 2]))


## Größe der Nachschlagetexturen für count Einträge (LUT_COLUMNS Spalten).
static func lut_size(count: int) -> Vector2i:
	return Vector2i(LUT_COLUMNS, maxi(ceili(float(maxi(count, 1)) / LUT_COLUMNS), 1))


## Texel eines Kartenindex in den Nachschlagetexturen.
static func lut_coords(map_index: int) -> Vector2i:
	return Vector2i(map_index % LUT_COLUMNS, map_index / LUT_COLUMNS)


func _load_rivers(path: String) -> void:
	if not FileAccess.file_exists(path):
		warnings.append(
			"Die Flussebene fehlt (%s); die Karte zeigt keine Flüsse. Erzeugen mit %s"
			% [path, GENERATOR_COMMAND]
		)
		return
	var rivers := Image.load_from_file(path)
	if rivers == null or rivers.is_empty():
		warnings.append("Die Flussebene %s konnte nicht gelesen werden." % path)
		return
	if rivers.get_size() != size:
		warnings.append(
			(
				"fluesse.png ist %d×%d Pixel, provinzen.png %d×%d; Flüsse werden nicht gezeigt."
				% [rivers.get_width(), rivers.get_height(), size.x, size.y]
			)
		)
		return
	if rivers.get_format() != Image.FORMAT_L8:
		rivers.convert(Image.FORMAT_L8)
	# Mipmaps: Beim Herauszoomen mittelt die Grafikkarte die Deckung, statt zu flimmern.
	rivers.generate_mipmaps()
	river_texture = ImageTexture.create_from_image(rivers)


func _read_definition(csv_path: String) -> String:
	var file := FileAccess.open(csv_path, FileAccess.READ)
	if file == null:
		return "Die Kartenzuordnung %s konnte nicht geöffnet werden." % csv_path
	var header := file.get_line().strip_edges().trim_prefix("﻿").to_lower()
	if header == CSV_HEADER_V1:
		return (
			(
				"Die Kartenzuordnung %s hat das Format vor M3 (ohne Beschriftungspunkt und "
				+ "Fläche).\nKarte neu erzeugen (im Repository-Hauptordner):\n    %s"
			)
			% [csv_path, GENERATOR_COMMAND]
		)
	if header != ";".join(CSV_COLUMNS):
		return (
			"Die Kartenzuordnung %s hat die Kopfzeile „%s“, erwartet „%s“."
			% [csv_path, header, ";".join(CSV_COLUMNS)]
		)
	var rows := {}
	var highest := -1
	var line_number := 1
	while not file.eof_reached():
		var line := file.get_line().strip_edges()
		line_number += 1
		if line.is_empty():
			continue
		var parts := line.split(";")
		if parts.size() != CSV_COLUMNS.size():
			warnings.append(
				"definition.csv:%d: %d statt %d Spalten, Zeile übersprungen."
				% [line_number, parts.size(), CSV_COLUMNS.size()]
			)
			continue
		var numbers := PackedInt32Array()
		var numeric := true
		for column: int in [0, 3, 4, 5, 6]:
			var text := parts[column].strip_edges()
			numeric = numeric and text.is_valid_int()
			numbers.append(text.to_int())
		if not numeric:
			warnings.append("definition.csv:%d: keine Ganzzahl, Zeile übersprungen." % line_number)
			continue
		var index := numbers[0]
		if index < 0 or index > 65534:
			warnings.append("definition.csv:%d: Kartenindex %d ungültig." % [line_number, index])
			continue
		if rows.has(index):
			warnings.append("definition.csv:%d: Kartenindex %d doppelt." % [line_number, index])
			continue
		rows[index] = [parts[1].strip_edges(), parts[2].strip_edges(), numbers]
		highest = maxi(highest, index)
	if rows.is_empty():
		return "Die Kartenzuordnung %s enthält keine Einträge." % csv_path
	var count := highest + 1
	map_ids.resize(count)
	map_names.resize(count)
	label_points.resize(count)
	label_points.fill(Vector2(-1, -1))
	pixel_counts.resize(count)
	pixel_counts.fill(0)
	areas_km2.resize(count)
	areas_km2.fill(0)
	for index: int in rows:
		var row: Array = rows[index]
		var numbers: PackedInt32Array = row[2]
		map_ids[index] = str(row[0])
		map_names[index] = str(row[1])
		var x := numbers[1]
		var y := numbers[2]
		if x >= 0 and y >= 0 and x < size.x and y < size.y and numbers[3] > 0:
			label_points[index] = Vector2(x + 0.5, y + 0.5)
		elif numbers[3] > 0:
			warnings.append(
				"definition.csv: Beschriftungspunkt von %s (%d, %d) liegt außerhalb des Bildes."
				% [row[0], x, y]
			)
		pixel_counts[index] = maxi(numbers[3], 0)
		areas_km2[index] = maxi(numbers[4], 0)
	map_to_sim.resize(count)
	map_to_sim.fill(-1)
	return ""
