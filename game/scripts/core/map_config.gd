class_name MapConfig
extends RefCounted
## Liest data/karte/karte.chron (Ausschnitt, Projektion, Bildgröße).
##
## Die Datei ist optional für die Oberfläche: Mit ihr zeichnet die Karte den
## Gradrahmen und rechnet Längen-/Breitengrade in Bildpixel um. Land, Meer,
## Seen und Ausland stehen seit M3 im Blaukanal von provinzen.png (MapData).
## Format: docs/technik/chronik-skript.md, Abschnitt 6.1.
## Der Leser versteht nur die flache Struktur dieses einen Blocks.

var image_width := 0
var image_height := 0
var longitude_min := 0.0
var longitude_max := 0.0
var latitude_min := 0.0
var latitude_max := 0.0
var max_radius_km := 0.0
## Bezugsbreite der Plattkarte (Grad Nord); 0, wenn die Datei sie nicht nennt.
var reference_latitude := 0.0


## Liest die Datei; null, wenn sie fehlt oder unbrauchbar ist.
static func load_from_file(path: String) -> MapConfig:
	if not FileAccess.file_exists(path):
		return null
	return from_text(FileAccess.get_file_as_string(path))


## Liest den Inhalt einer karte.chron; null, wenn er unbrauchbar ist.
static func from_text(text: String) -> MapConfig:
	if text.is_empty():
		return null
	var values := _parse_block(_tokenize(text), "karte")
	if values.is_empty():
		return null
	var config := MapConfig.new()
	config.image_width = int(_number(values, "bild_breite"))
	config.image_height = int(_number(values, "bild_hoehe"))
	config.longitude_min = _number(values, "laenge_min")
	config.longitude_max = _number(values, "laenge_max")
	config.latitude_min = _number(values, "breite_min")
	config.latitude_max = _number(values, "breite_max")
	config.max_radius_km = _number(values, "max_radius_km")
	config.reference_latitude = _number(values, "bezugsbreite")
	if not config.is_valid():
		return null
	return config


func is_valid() -> bool:
	return (
		image_width > 0
		and image_height > 0
		and longitude_max > longitude_min
		and latitude_max > latitude_min
	)


## Bildkoordinate (Pixel) eines geografischen Punktes (lineare Abbildung des Rechtecks).
func geo_to_pixel(longitude: float, latitude: float) -> Vector2:
	var x := (longitude - longitude_min) / (longitude_max - longitude_min) * image_width
	var y := (latitude_max - latitude) / (latitude_max - latitude_min) * image_height
	return Vector2(x, y)


## Geografischer Punkt (x = Länge Ost, y = Breite Nord) einer Bildkoordinate.
func pixel_to_geo(pixel: Vector2) -> Vector2:
	var longitude := longitude_min + pixel.x / image_width * (longitude_max - longitude_min)
	var latitude := latitude_max - pixel.y / image_height * (latitude_max - latitude_min)
	return Vector2(longitude, latitude)


static func _number(values: Dictionary, key: String) -> float:
	var value: Variant = values.get(key, 0.0)
	if value is float:
		return value
	return 0.0


## Zerlegt den Text in Wörter, Zeichenketten und die Zeichen { } =.
static func _tokenize(text: String) -> PackedStringArray:
	var tokens := PackedStringArray()
	var i := 0
	var length := text.length()
	while i < length:
		var c := text[i]
		if c == "#":
			while i < length and text[i] != "\n":
				i += 1
		elif c == " " or c == "\t" or c == "\r" or c == "\n" or c == "﻿":
			i += 1
		elif c == "{" or c == "}" or c == "=":
			tokens.append(c)
			i += 1
		elif c == "\"":
			var end := text.find("\"", i + 1)
			if end < 0:
				end = length
			tokens.append(text.substr(i, end - i + 1))
			i = end + 1
		else:
			var start := i
			while i < length and not " \t\r\n{}=<>!#\"".contains(text[i]):
				i += 1
			if i == start:
				i += 1
			else:
				tokens.append(text.substr(start, i - start))
	return tokens


## Sucht "name {" bzw. "name = {" auf oberster Ebene und liest die Einträge
## key = zahl und key = { zahl zahl … }.
static func _parse_block(tokens: PackedStringArray, block_name: String) -> Dictionary:
	var values := {}
	var i := 0
	while i < tokens.size():
		if tokens[i] == block_name:
			var j := i + 1
			if j < tokens.size() and tokens[j] == "=":
				j += 1
			if j < tokens.size() and tokens[j] == "{":
				_read_entries(tokens, j + 1, values)
				return values
		i += 1
	return values


static func _read_entries(tokens: PackedStringArray, start: int, values: Dictionary) -> void:
	var i := start
	while i < tokens.size() and tokens[i] != "}":
		var key := tokens[i]
		if i + 2 < tokens.size() and tokens[i + 1] == "=":
			if tokens[i + 2] == "{":
				var numbers := PackedFloat64Array()
				var j := i + 3
				while j < tokens.size() and tokens[j] != "}":
					if tokens[j].is_valid_float():
						numbers.append(tokens[j].to_float())
					j += 1
				values[key] = numbers
				i = j + 1
				continue
			if tokens[i + 2].is_valid_float():
				values[key] = tokens[i + 2].to_float()
			i += 3
			continue
		i += 1
