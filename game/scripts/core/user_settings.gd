class_name UserSettings
extends RefCounted
## Einstellungen (seit M13, Schritt a5d; vertical_slice.md 4.5) in user://einstellungen.cfg: Bild, Ton,
## Spiel (Autosave, Pausenregeln, Kompass, Hinweise), Barrierefreiheit, Tasten und Sprache. get/set nach
## „gruppe/schluessel“; apply() wirkt auf Fenster, Ton, Theme und Tastenbelegung.

signal changed(key: String)

const PATH := "user://einstellungen.cfg"
## Pausenregeln (Abschnitt 4.4): Schlüssel unter „spiel/“ und Beschriftung.
const PAUSE_RULES: Array[Array] = [
	["pause_krieg", "Kriegserklärung an oder durch den Spielerstaat"],
	["pause_forderung", "Forderung an den Spielerstaat"],
	["pause_wahl", "Wahlergebnis (Wahlabend)"],
	["pause_amt", "Amtswechsel im eigenen Staat"],
	["pause_bund", "Gründung eines Bundes"],
	["pause_frieden", "Frieden"],
]
## Belegbare Aktionen (Abschnitt „Tasten“) mit Beschriftung.
const ACTIONS: Array[Array] = [
	["tempo_pause", "Pause"], ["tempo_1", "Tempo 1"], ["tempo_2", "Tempo 2"], ["tempo_3", "Tempo 3"],
	["tempo_4", "Tempo 4"], ["tempo_5", "Tempo 5"], ["kamera_hoch", "Karte hoch"], ["kamera_runter", "Karte runter"],
	["kamera_links", "Karte links"], ["kamera_rechts", "Karte rechts"], ["kamera_uebersicht", "Übersicht"],
	["kamera_hauptstadt", "Hauptstadt"], ["journal", "Journale"], ["jahrbuch", "Jahrbuch"],
	["kartenmodus_politisch", "Kartenmodus Politisch"], ["kartenmodus_wahlen", "Kartenmodus Wahlen"],
	["kartenmodus_krieg", "Kartenmodus Krieg"], ["kartenmodus_bund", "Kartenmodus Bund"],
	["spielmenue", "Spielmenü"], ["schnellspeichern", "Schnellspeichern"], ["schnellladen", "Schnellladen"],
]
const AUTOSAVE := ["aus", "jaehrlich", "halbjaehrlich"]
const FPS_LIMITS: PackedInt32Array = [0, 30, 60, 120]

const DEFAULTS := {
	"bild/vollbild": false,
	"bild/oberflaeche": 100,
	"bild/bildrate": 0,
	"ton/gesamt": 80,
	"ton/musik": 70,
	"ton/klaenge": 80,
	"ton/musik_aus": false,
	"spiel/autosave": "jaehrlich",
	"spiel/kompass": true,
	"spiel/hinweise": true,
	"spiel/pause_krieg": true,
	"spiel/pause_forderung": true,
	"spiel/pause_wahl": true,
	"spiel/pause_amt": false,
	"spiel/pause_bund": true,
	"spiel/pause_frieden": true,
	"zugang/farbsehschwaeche": false,
	"zugang/kontrast": false,
	"zugang/animationen_reduziert": false,
	"zugang/schrift": 100,
	"sprache/sprache": "de",
}

var path := PATH
var _values := {}
## Tastenbelegung: Aktion → physischer Tastencode (nur geänderte).
var _keys := {}
## Ursprüngliche Belegung je Aktion (zum Zurücksetzen) und Schriftgrößen des Themes.
var _default_keys := {}
var _base_font_sizes := {}
var _base_colors := {}


func _init(file_path: String = PATH) -> void:
	path = file_path
	_values = DEFAULTS.duplicate()
	for action in ACTIONS:
		var key := _first_key(StringName(action[0]))
		if key != KEY_NONE:
			_default_keys[action[0]] = key


func get_value(key: String) -> Variant:
	return _values.get(key, DEFAULTS.get(key))


func set_value(key: String, value: Variant) -> void:
	if not DEFAULTS.has(key) or _values.get(key) == value:
		return
	_values[key] = value
	changed.emit(key)


func reset_group(group: String) -> void:
	for key: String in DEFAULTS:
		if key.begins_with(group + "/"):
			set_value(key, DEFAULTS[key])
	if group == "tasten":
		_keys.clear()
		changed.emit("tasten/")


## Pausenregel an? (Schlüssel aus PAUSE_RULES, z. B. "pause_krieg").
func pause_on(rule: String) -> bool:
	return bool(get_value("spiel/" + rule))


func load_file() -> bool:
	var file := ConfigFile.new()
	if file.load(path) != OK:
		return false
	for key: String in DEFAULTS:
		var parts := key.split("/")
		if file.has_section_key(parts[0], parts[1]):
			var value: Variant = file.get_value(parts[0], parts[1])
			if typeof(value) == typeof(DEFAULTS[key]) or (DEFAULTS[key] is int and value is float):
				_values[key] = value if not (DEFAULTS[key] is int) else int(value)
	_keys.clear()
	if file.has_section("tasten"):
		for action: String in file.get_section_keys("tasten"):
			if _default_keys.has(action):
				_keys[action] = int(file.get_value("tasten", action))
	return true


func save_file() -> bool:
	var file := ConfigFile.new()
	for key: String in _values:
		var parts := key.split("/")
		file.set_value(parts[0], parts[1], _values[key])
	for action: String in _keys:
		file.set_value("tasten", action, _keys[action])
	return file.save(path) == OK


## Taste einer Aktion (physischer Code) und neue Belegung.
func key_of(action: String) -> int:
	return int(_keys.get(action, _default_keys.get(action, KEY_NONE)))


func set_key(action: String, physical_keycode: int) -> void:
	if not _default_keys.has(action):
		return
	if physical_keycode == int(_default_keys[action]):
		_keys.erase(action)
	else:
		_keys[action] = physical_keycode
	changed.emit("tasten/" + action)


## Aktion, die diese Taste schon belegt (leer: keine), außer except.
func action_with_key(physical_keycode: int, except: String = "") -> String:
	for action: Variant in _default_keys:
		if str(action) != except and key_of(str(action)) == physical_keycode:
			return str(action)
	return ""


## Wirkung auf Fenster, Ton, Theme und Eingabe. tree darf null sein (Tests).
func apply(tree: SceneTree) -> void:
	Accessibility.color_blind = bool(get_value("zugang/farbsehschwaeche"))
	Accessibility.high_contrast = bool(get_value("zugang/kontrast"))
	Accessibility.reduced_motion = bool(get_value("zugang/animationen_reduziert"))
	Engine.max_fps = int(get_value("bild/bildrate"))
	var master := AudioServer.get_bus_index(&"Master")
	if master >= 0:
		AudioServer.set_bus_volume_db(master, linear_to_db(maxf(float(get_value("ton/gesamt")) / 100.0, 0.0001)))
	_apply_keys()
	_apply_theme(ThemeDB.get_project_theme())
	if tree == null or DisplayServer.get_name() == "headless":
		return
	tree.root.content_scale_factor = clampf(float(get_value("bild/oberflaeche")) / 100.0, 0.75, 2.0)
	var mode := DisplayServer.WINDOW_MODE_FULLSCREEN if bool(get_value("bild/vollbild")) else DisplayServer.WINDOW_MODE_WINDOWED
	if DisplayServer.window_get_mode() != mode:
		DisplayServer.window_set_mode(mode)


func _apply_keys() -> void:
	for action: Variant in _default_keys:
		var name := StringName(str(action))
		if not InputMap.has_action(name):
			continue
		var wanted := key_of(str(action))
		for event in InputMap.action_get_events(name):
			if event is InputEventKey:
				(event as InputEventKey).physical_keycode = wanted
				break


func _first_key(action: StringName) -> int:
	if not InputMap.has_action(action):
		return KEY_NONE
	for event in InputMap.action_get_events(action):
		if event is InputEventKey:
			return int((event as InputEventKey).physical_keycode)
	return KEY_NONE


## Schriftgröße (Prozent) und hoher Kontrast im Theme des Projekts.
func _apply_theme(theme: Theme) -> void:
	if theme == null:
		return
	var scale := clampf(float(get_value("zugang/schrift")) / 100.0, 1.0, 1.5)
	if _base_font_sizes.is_empty():
		_base_font_sizes["__default"] = theme.default_font_size
		for type in theme.get_font_size_type_list():
			for name in theme.get_font_size_list(type):
				_base_font_sizes["%s/%s" % [type, name]] = theme.get_font_size(name, type)
	theme.default_font_size = roundi(int(_base_font_sizes["__default"]) * scale)
	for key: String in _base_font_sizes:
		if key == "__default":
			continue
		var parts := key.split("/")
		theme.set_font_size(parts[1], parts[0], roundi(int(_base_font_sizes[key]) * scale))
	# Hoher Kontrast: Schrift tief schwarz, gedämpfte Schrift dunkler.
	if _base_colors.is_empty():
		for type in theme.get_color_type_list():
			for name in theme.get_color_list(type):
				if String(name).begins_with("font"):
					_base_colors["%s/%s" % [type, name]] = theme.get_color(name, type)
	for key: String in _base_colors:
		var parts := key.split("/")
		var base: Color = _base_colors[key]
		theme.set_color(parts[1], parts[0], base.darkened(0.6) if Accessibility.high_contrast else base)
