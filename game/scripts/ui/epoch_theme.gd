class_name EpochTheme
extends RefCounted
## Epochenrahmen III (seit M13b1, vertical_slice.md 5.1): eingebettete Schriften unter der SIL Open Font
## License (assets/schriften, Lizenztexte daneben), Eckornamente, Trenner und Vignetten als eigene SVG
## (assets/ornamente). apply() setzt die Schriften im Theme des Projekts; draw_frame() zeichnet den Rahmen
## eines Fensters (Doppellinie und Ecken) über seinen Hintergrund.

const FONT_DIR := "res://assets/schriften/"
const ORNAMENT_DIR := "res://assets/ornamente/"
## Vignetten der Ereignisse (Feld 'vignette' in den Ereignisdateien).
const VIGNETTES: PackedStringArray = ["revolution", "krieg", "diplomatie", "wirtschaft", "hof"]
## Tinte und Gold der Palette (wie im Theme).
const INK := Color(0.168627, 0.129412, 0.0862745)
const GOLD := Color(0.721569, 0.537255, 0.184314)
const CORNER_SIZE := 22.0

static var _fonts := {}
static var _textures := {}


## Schrift nach Rolle: text, text_kraeftig, kursiv, titel, kopf, fraktur, tabelle.
static func font(role: String) -> Font:
	if _fonts.has(role):
		return _fonts[role]
	var result: Font = null
	match role:
		"text":
			result = _variation("SourceSerif4.ttf", 400, 12)
		"text_kraeftig":
			result = _variation("SourceSerif4.ttf", 600, 12)
		"kursiv":
			result = _variation("SourceSerif4-Italic.ttf", 400, 12)
		"titel":
			result = _variation("PlayfairDisplay.ttf", 700, 0)
		"kopf":
			var v := _variation("PlayfairDisplay.ttf", 600, 0)
			if v != null:
				v.spacing_glyph = 1
			result = v
		"fraktur":
			result = _file("UnifrakturMaguntia.ttf")
		"tabelle":
			result = _variation("SourceCodePro.ttf", 400, 0)
	_fonts[role] = result
	return result


## Schriften ins Theme: Grundschrift, Titel, Kopfzeilen, Zahlen, Tabellen, Zeitungskopf. Fehlt eine Datei,
## bleibt die bisherige Schrift (Systemschrift).
static func apply(theme: Theme) -> void:
	if theme == null or font("text") == null:
		return
	theme.default_font = font("text")
	for type: StringName in [&"TitleLabel", &"NoticeTitle", &"NameLabel", &"HeadlineLabel"]:
		_set_font(theme, type, &"font", font("titel"))
	_set_font(theme, &"HeaderLabel", &"font", font("kopf"))
	for type: StringName in [&"DateLabel", &"ValueLabel"]:
		_set_font(theme, type, &"font", font("text_kraeftig"))
	_set_font(theme, &"CodeLabel", &"font", font("tabelle"))
	_set_font(theme, &"MastheadLabel", &"font", font("fraktur"))
	_set_font(theme, &"SublineLabel", &"font", font("kursiv"))
	_set_font(theme, &"RichTextLabel", &"normal_font", font("text"))
	_set_font(theme, &"RichTextLabel", &"bold_font", font("text_kraeftig"))
	_set_font(theme, &"RichTextLabel", &"italics_font", font("kursiv"))
	_set_font(theme, &"RichTextLabel", &"mono_font", font("tabelle"))
	# Neue Varianten der Zeitung (seit b1).
	for entry: Array in [[&"MastheadLabel", 44], [&"HeadlineLabel", 30], [&"SublineLabel", 17]]:
		if theme.get_type_variation_base(entry[0]) == &"":
			theme.set_type_variation(entry[0], &"Label")
		theme.set_font_size(&"font_size", entry[0], entry[1])
		theme.set_color(&"font_color", entry[0], INK)


## Doppellinie und Eckornamente innerhalb von rect (Koordinaten des Elements).
static func draw_frame(item: CanvasItem, rect: Rect2) -> void:
	if rect.size.x < 4.0 * CORNER_SIZE or rect.size.y < 3.0 * CORNER_SIZE:
		return
	var ink := INK
	ink.a = 0.55
	item.draw_rect(rect.grow(-3.0), ink, false, 1.0)
	ink.a = 0.3
	item.draw_rect(rect.grow(-6.0), ink, false, 1.0)
	var corner := texture("ecke")
	if corner == null:
		return
	var s := Vector2(CORNER_SIZE, CORNER_SIZE)
	var origin := rect.position + Vector2(1.0, 1.0)
	var far := rect.end - Vector2(1.0, 1.0)
	for spec: Array in [
		[origin, Vector2(1, 1)], [Vector2(far.x, origin.y), Vector2(-1, 1)],
		[Vector2(origin.x, far.y), Vector2(1, -1)], [far, Vector2(-1, -1)],
	]:
		item.draw_set_transform(spec[0], 0.0, spec[1])
		item.draw_texture_rect(corner, Rect2(Vector2.ZERO, s), false)
	item.draw_set_transform(Vector2.ZERO, 0.0, Vector2.ONE)


## Zeichnet künftig den Rahmen über den Hintergrund von control (nach seinem eigenen Zeichnen).
static func frame(control: Control) -> void:
	if control == null or control.has_meta(&"epoch_frame"):
		return
	control.set_meta(&"epoch_frame", true)
	control.draw.connect(func() -> void: draw_frame(control, Rect2(Vector2.ZERO, control.size)))


## Textur eines Ornaments (ecke, trenner, vignette_<art>); null, wenn sie fehlt.
static func texture(name: String) -> Texture2D:
	if _textures.has(name):
		return _textures[name]
	var path := ORNAMENT_DIR + name + ".svg"
	var result: Texture2D = load(path) as Texture2D if ResourceLoader.exists(path) else null
	_textures[name] = result
	return result


## Vignette einer Art (leer oder unbekannt: null).
static func vignette(kind: String) -> Texture2D:
	return texture("vignette_" + kind) if VIGNETTES.has(kind) else null


static func _file(file_name: String) -> FontFile:
	var path := FONT_DIR + file_name
	if not ResourceLoader.exists(path):
		push_warning("EINIGKEIT: Schrift fehlt: " + path)
		return null
	var file := load(path) as FontFile
	# Ohne Grafikausgabe ist das Fenster 64 × 64 Pixel groß; die globale Überabtastung (0,04) bläht dann die
	# Zeilenhöhen auf. Tests sollen mit denselben Maßen rechnen wie das Spiel.
	if file != null and DisplayServer.get_name() == "headless":
		file.oversampling = 1.0
	if file != null and file.fallbacks.is_empty():
		var system := SystemFont.new()
		system.font_names = PackedStringArray(["DejaVu Serif", "Noto Serif", "Georgia", "serif"])
		if DisplayServer.get_name() == "headless":
			system.oversampling = 1.0
		file.fallbacks = [system]
	return file


## Variable Schrift mit Stärke (wght) und optischer Größe (opsz, 0: Standard der Datei).
static func _variation(file_name: String, weight: int, optical: int) -> FontVariation:
	var file := _file(file_name)
	if file == null:
		return null
	var v := FontVariation.new()
	v.base_font = file
	var ts := TextServerManager.get_primary_interface()
	var axes := {ts.name_to_tag("wght"): weight}
	if optical > 0:
		axes[ts.name_to_tag("opsz")] = optical
	v.variation_opentype = axes
	return v


static func _set_font(theme: Theme, type: StringName, item: StringName, value: Font) -> void:
	if value != null:
		theme.set_font(item, type, value)
