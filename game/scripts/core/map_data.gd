class_name MapData
extends RefCounted
## Karten-ID-Bild (provinzen.png) und Zuordnung Kartenindex → Ort (definition.csv).
##
## Jeder Pixel kodiert Kartenindex + 1 als R + G × 256 (0 = kein Ort).
## Der Kartenindex ist unabhängig vom Simulationsindex; beide werden über die
## Ort-Kennung (get_location_index) verbunden. Format: chronik-skript.md, Abschnitt 6.

const CSV_HEADER := "kartenindex;ort;name"
const GENERATOR_COMMAND := "python3 tools/kartengenerator/kartengenerator.py"
## Abtastschritt (Pixel) für Schwerpunkte und Flächenschätzung.
const SAMPLE_STEP := 2

## Bild für die Klick-Erkennung (bleibt im Speicher).
var image: Image = null
## Textur für die Darstellung (Filter NEAREST setzt die Kartenansicht).
var texture: ImageTexture = null
var size := Vector2i.ZERO
## Kartenindex → Ort-Kennung laut CSV ("" = Lücke in der Nummerierung).
var map_ids := PackedStringArray()
## Kartenindex → Anzeigename laut CSV.
var map_names := PackedStringArray()
## Kartenindex → Simulationsindex (-1 = nicht auflösbar).
var map_to_sim := PackedInt32Array()
## Simulationsindex → Kartenindex (-1 = Ort nicht auf der Karte).
var sim_to_map := PackedInt32Array()
## Kartenindex → Schwerpunkt in Bildkoordinaten (Vector2(-1, -1), wenn ohne Fläche).
var centers := PackedVector2Array()
## Kartenindex → Anzahl abgetasteter Pixel (Flächenschätzung).
var sample_counts := PackedInt32Array()
## Optionale Kartenbeschreibung (karte.chron).
var config: MapConfig = null
## Fataler Fehler (deutsch); leer, wenn die Karte benutzbar ist.
var error := ""
## Nicht fatale Hinweise (deutsch).
var warnings := PackedStringArray()


## Lädt provinzen.png, definition.csv und (optional) karte.chron aus dem Kartenordner.
static func load_from_dir(map_dir: String) -> MapData:
	var data := MapData.new()
	var image_path := map_dir.path_join("provinzen.png")
	var csv_path := map_dir.path_join("definition.csv")
	if not FileAccess.file_exists(image_path):
		data.error = (
			"Die Karte fehlt: %s\nErzeugen mit (im Repository-Hauptordner):\n    %s"
			% [image_path, GENERATOR_COMMAND]
		)
		return data
	if not FileAccess.file_exists(csv_path):
		data.error = (
			"Die Kartenzuordnung fehlt: %s\nErzeugen mit (im Repository-Hauptordner):\n    %s"
			% [csv_path, GENERATOR_COMMAND]
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
	data._compute_centers()
	data.config = MapConfig.load_from_file(map_dir.path_join("karte.chron"))
	return data


func is_valid() -> bool:
	return error.is_empty() and image != null


## Anzahl der Kartenindizes (Breite der Farbtabelle).
func map_count() -> int:
	return map_ids.size()


## Kartenindex am Pixel oder -1 (kein Ort, außerhalb des Bildes).
func map_index_at_pixel(pixel: Vector2i) -> int:
	if image == null or pixel.x < 0 or pixel.y < 0 or pixel.x >= size.x or pixel.y >= size.y:
		return -1
	var color := image.get_pixel(pixel.x, pixel.y)
	var index := color.r8 + color.g8 * 256 - 1
	return index if index < map_count() else -1


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


## Schwerpunkt eines Ortes in Bildkoordinaten oder Vector2(-1, -1).
func center_of_sim(sim_index: int) -> Vector2:
	if sim_index < 0 or sim_index >= sim_to_map.size():
		return Vector2(-1, -1)
	var map_index := sim_to_map[sim_index]
	if map_index < 0 or map_index >= centers.size():
		return Vector2(-1, -1)
	return centers[map_index]


func _read_definition(csv_path: String) -> String:
	var file := FileAccess.open(csv_path, FileAccess.READ)
	if file == null:
		return "Die Kartenzuordnung %s konnte nicht geöffnet werden." % csv_path
	var by_index := {}
	var highest := -1
	var line_number := 0
	while not file.eof_reached():
		var line := file.get_line().strip_edges()
		line_number += 1
		if line_number == 1:
			line = line.trim_prefix("﻿")
			if line.to_lower() != CSV_HEADER:
				warnings.append(
					"definition.csv: unerwartete Kopfzeile „%s“ (erwartet „%s“)."
					% [line, CSV_HEADER]
				)
			continue
		if line.is_empty():
			continue
		var parts := line.split(";")
		if parts.size() < 2 or not parts[0].strip_edges().is_valid_int():
			warnings.append("definition.csv:%d: Zeile wird übersprungen." % line_number)
			continue
		var index := parts[0].strip_edges().to_int()
		if index < 0 or index > 65534:
			warnings.append("definition.csv:%d: Kartenindex %d ungültig." % [line_number, index])
			continue
		if by_index.has(index):
			warnings.append("definition.csv:%d: Kartenindex %d doppelt." % [line_number, index])
			continue
		var display_name := ";".join(parts.slice(2)).strip_edges() if parts.size() > 2 else ""
		by_index[index] = [parts[1].strip_edges(), display_name]
		highest = maxi(highest, index)
	if by_index.is_empty():
		return "Die Kartenzuordnung %s enthält keine Einträge." % csv_path
	map_ids.resize(highest + 1)
	map_names.resize(highest + 1)
	for index: int in by_index:
		var entry: Array = by_index[index]
		map_ids[index] = str(entry[0])
		map_names[index] = str(entry[1])
	map_to_sim.resize(highest + 1)
	map_to_sim.fill(-1)
	return ""


## Schwerpunkte und Flächen aller Kartenindizes (auf jedem SAMPLE_STEP-ten Pixel).
func _compute_centers() -> void:
	var count := map_count()
	var sums := PackedVector2Array()
	sums.resize(count)
	sample_counts.resize(count)
	sample_counts.fill(0)
	var bytes := image.get_data()
	var width := size.x
	for y in range(0, size.y, SAMPLE_STEP):
		var row := y * width * 3
		for x in range(0, width, SAMPLE_STEP):
			var offset := row + x * 3
			var index := bytes[offset] + bytes[offset + 1] * 256 - 1
			if index >= 0 and index < count:
				sums[index] += Vector2(x, y)
				sample_counts[index] += 1
	centers.resize(count)
	for index in count:
		var samples := sample_counts[index]
		if samples > 0:
			centers[index] = sums[index] / samples + Vector2(0.5, 0.5)
		else:
			centers[index] = Vector2(-1, -1)
