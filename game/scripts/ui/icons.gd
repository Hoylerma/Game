class_name Icons
extends RefCounted
## Symbole der Oberfläche (seit M14a2, europa_und_oberflaeche.md 5): eigene SVG-Zeichnungen im Stil der
## Ornamente (assets/symbole, Tinte #2b2116, Gold #b88930), zur Laufzeit in Größe und Farbe gerendert und
## zwischengespeichert. Die Dateien sind nicht importiert („keep“), damit sie auch im Paket als SVG-Text
## vorliegen; die Exportvorlagen nehmen sie ausdrücklich mit.

const DIR := "res://assets/symbole/"
## Farbe der Linien in den Dateien; texture() ersetzt sie durch die gewünschte Tinte.
const FILE_INK := "#2b2116"
## Überabtastung: Die Textur ist doppelt so groß wie die Anzeige (scharf bis 200 % Oberfläche).
const OVERSAMPLE := 2.0

static var _cache := {}
static var _sources := {}


## Gibt es das Symbol?
static func exists(icon: StringName) -> bool:
	return not _source(icon).is_empty()


## Alle Symbole (Tests).
static func names() -> PackedStringArray:
	var result := PackedStringArray()
	for file in DirAccess.get_files_at(DIR):
		if file.ends_with(".svg"):
			result.append(file.get_basename())
	result.sort()
	return result


## Symbol als Textur für die Anzeigegröße size (Pixel der Oberfläche) in der Tinte ink; null, wenn es fehlt.
static func texture(icon: StringName, size: float = 20.0, ink: Color = UiPalette.INK) -> Texture2D:
	var key := "%s|%d|%s" % [icon, roundi(size), ink.to_html(false)]
	if _cache.has(key):
		return _cache[key]
	var svg := _source(icon)
	if svg.is_empty():
		return null
	if ink != UiPalette.INK:
		svg = svg.replace(FILE_INK, "#" + ink.to_html(false))
	var image := Image.new()
	if image.load_svg_from_string(svg, size / 24.0 * OVERSAMPLE) != OK:
		return null
	var result := ImageTexture.create_from_image(image)
	_cache[key] = result
	return result


## Knopf mit Symbol statt Text: quadratisch, Name und Erklärung im Tooltip. Schmale Innenabstände, damit
## das Symbol in voller Größe erscheint; gedrückt hell mit Goldrahmen statt der roten Füllung des Themes
## (darauf wäre die Tinte schlecht zu sehen).
static func icon_button(icon: StringName, tooltip: String, size: float = 22.0) -> Button:
	var button := Button.new()
	button.icon = texture(icon, size)
	button.tooltip_text = tooltip
	button.focus_mode = Control.FOCUS_NONE
	button.icon_alignment = HORIZONTAL_ALIGNMENT_CENTER
	button.expand_icon = true
	button.custom_minimum_size = Vector2(size + 12.0, size + 10.0)
	var theme := ThemeDB.get_project_theme()
	for style_name: StringName in [&"normal", &"hover", &"pressed", &"hover_pressed", &"disabled"]:
		var style: StyleBox = null
		if theme != null and theme.has_stylebox(style_name, &"Button"):
			style = theme.get_stylebox(style_name, &"Button").duplicate() as StyleBox
		else:
			style = StyleBoxFlat.new()
		if style_name in [&"pressed", &"hover_pressed"] and style is StyleBoxFlat:
			var flat := style as StyleBoxFlat
			flat.bg_color = UiPalette.PAPER
			flat.border_color = UiPalette.GOLD
			flat.set_border_width_all(2)
		style.content_margin_left = 5.0
		style.content_margin_right = 5.0
		style.content_margin_top = 4.0
		style.content_margin_bottom = 4.0
		button.add_theme_stylebox_override(style_name, style)
	return button


static func _source(icon: StringName) -> String:
	if _sources.has(icon):
		return _sources[icon]
	var path := DIR + str(icon) + ".svg"
	var text := FileAccess.get_file_as_string(path) if FileAccess.file_exists(path) else ""
	_sources[icon] = text
	return text
