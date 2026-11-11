class_name AudioDirector
extends Node
## Musik und Bedienklänge (seit M13b2, vertical_slice.md 5.3). Die Stücke und Klänge rendert
## tools/klang/klang.py nach audio/musik und audio/klaenge (nicht versioniert); fehlen sie, bleibt das Spiel
## still. Die Musik folgt einer Stimmung (frieden, krise, krieg, revolution): Ein Wechsel blendet über,
## zwischen zwei Stücken liegt eine Pause. Klänge spielen auf dem Bus „Klaenge“, die Musik auf „Musik“;
## die Lautstärken setzen die Einstellungen (UserSettings.apply).

signal piece_started(piece_id: String)

const MUSIC_DIR := "res://audio/musik/"
const SOUND_DIR := "res://audio/klaenge/"
const LIST_PATH := "res://audio/musik/stuecke.cfg"
const MOODS: PackedStringArray = ["frieden", "krise", "krieg", "revolution"]
const SOUNDS: PackedStringArray = ["gaensekiel", "siegel", "papier", "glocke", "trommel", "telegraf"]
const MUSIC_BUS := &"Musik"
const SOUND_BUS := &"Klaenge"
## Überblendung bei einem Wechsel der Stimmung (Sekunden) und Pause zwischen zwei Stücken.
const FADE_SECONDS := 3.0
const PAUSE_MIN := 6.0
const PAUSE_MAX := 16.0
const SILENT_DB := -60.0
## Höchstens so viele Klänge zugleich.
const SOUND_VOICES := 4

## Stücke: Kennung → { titel, komponist, werk, stimmung, dauer }.
var pieces := {}
## Gespielte Klänge je Kennung (Tests).
var sounds_played := {}
var _mood := ""
var _current := ""
var _last_by_mood := {}
var _players: Array[AudioStreamPlayer] = []
var _active := 0
var _fade := 0.0 # > 0: läuft; blendet _players[1 − _active] aus und _players[_active] ein
var _pause := -1.0
var _sounds: Array[AudioStreamPlayer] = []
var _next_sound := 0
var _streams := {}
var _rng := RandomNumberGenerator.new()


## Busse „Musik“ und „Klaenge“ anlegen (beide auf Master), wenn es sie noch nicht gibt.
static func ensure_buses() -> void:
	for bus in [MUSIC_BUS, SOUND_BUS]:
		if AudioServer.get_bus_index(bus) < 0:
			AudioServer.add_bus()
			var index := AudioServer.bus_count - 1
			AudioServer.set_bus_name(index, bus)
			AudioServer.set_bus_send(index, &"Master")


func _ready() -> void:
	name = "AudioDirector"
	process_mode = Node.PROCESS_MODE_ALWAYS
	ensure_buses()
	_rng.randomize()
	for i in 2:
		var p := AudioStreamPlayer.new()
		p.bus = MUSIC_BUS
		p.volume_db = SILENT_DB
		p.finished.connect(_on_finished.bind(i))
		add_child(p)
		_players.append(p)
	for i in SOUND_VOICES:
		var s := AudioStreamPlayer.new()
		s.bus = SOUND_BUS
		add_child(s)
		_sounds.append(s)
	_load_list()


## Gibt es Musik (stuecke.cfg und mindestens eine Datei)?
func has_music() -> bool:
	return not pieces.is_empty()


func mood() -> String:
	return _mood


func current_piece() -> String:
	return _current


## Stücke einer Stimmung, nach Kennung geordnet.
func pieces_for(mood_id: String) -> PackedStringArray:
	var result := PackedStringArray()
	for id: String in pieces:
		if str((pieces[id] as Dictionary).get("stimmung", "")) == mood_id:
			result.append(id)
	result.sort()
	return result


## Stimmung setzen; ein Wechsel blendet zu einem Stück der neuen Stimmung über.
func set_mood(mood_id: String) -> void:
	if not MOODS.has(mood_id) or mood_id == _mood:
		return
	_mood = mood_id
	if not has_music():
		return
	_start_piece(_choose(mood_id), true)


## Bedienklang abspielen (Kennung aus SOUNDS); false, wenn es ihn nicht gibt.
func play_sound(sound_id: String) -> bool:
	var stream := _stream(SOUND_DIR, sound_id)
	if stream == null:
		return false
	var player := _sounds[_next_sound]
	_next_sound = (_next_sound + 1) % _sounds.size()
	player.stream = stream
	if player.is_inside_tree():
		player.play()
	sounds_played[sound_id] = int(sounds_played.get(sound_id, 0)) + 1
	return true


func _process(delta: float) -> void:
	if _fade > 0.0:
		_fade = maxf(_fade - delta, 0.0)
		var t := 1.0 - _fade / FADE_SECONDS
		_players[_active].volume_db = linear_to_db(maxf(t, 0.001))
		var old := _players[1 - _active]
		old.volume_db = linear_to_db(maxf(1.0 - t, 0.001))
		if _fade == 0.0:
			old.stop()
			old.volume_db = SILENT_DB
	if _pause > 0.0:
		_pause -= delta
		if _pause <= 0.0:
			_pause = -1.0
			_start_piece(_choose(_mood), false)


func _on_finished(index: int) -> void:
	if index != _active or _mood.is_empty():
		return
	_current = ""
	_pause = _rng.randf_range(PAUSE_MIN, PAUSE_MAX)


## Ein anderes Stück der Stimmung als das zuletzt gespielte (bei nur einem: dasselbe).
func _choose(mood_id: String) -> String:
	var list := pieces_for(mood_id)
	if list.is_empty():
		return ""
	var last := list.find(str(_last_by_mood.get(mood_id, "")))
	if list.size() > 1 and last >= 0:
		list.remove_at(last)
	return list[_rng.randi_range(0, list.size() - 1)]


func _start_piece(id: String, crossfade: bool) -> void:
	_pause = -1.0
	var stream := _stream(MUSIC_DIR, id) if not id.is_empty() else null
	if stream == null:
		# Keine Musik dieser Stimmung: das laufende Stück ausblenden.
		if _players[_active].playing:
			_active = 1 - _active
			_players[_active].stop()
			_fade = FADE_SECONDS
		_current = ""
		return
	var was_playing := _players[_active].playing
	_active = 1 - _active
	var player := _players[_active]
	player.stream = stream
	player.volume_db = SILENT_DB if (crossfade and was_playing) else 0.0
	if player.is_inside_tree():
		player.play()
	_fade = FADE_SECONDS if (crossfade and was_playing) else 0.0
	if _fade == 0.0:
		_players[1 - _active].stop()
	_current = id
	_last_by_mood[_mood] = id
	piece_started.emit(id)


func _load_list() -> void:
	var config := ConfigFile.new()
	if not FileAccess.file_exists(LIST_PATH) or config.load(LIST_PATH) != OK:
		print("EINIGKEIT: keine Musik (tools/klang/klang.py erzeugt sie nach game/audio)")
		return
	for id in config.get_sections():
		if _stream(MUSIC_DIR, id) == null:
			continue
		var entry := {}
		for key in config.get_section_keys(id):
			entry[key] = config.get_value(id, key)
		pieces[id] = entry


## Datei laden: importiert über den ResourceLoader, sonst (frisch erzeugt, noch nicht importiert)
## direkt als WAV.
func _stream(dir: String, id: String) -> AudioStream:
	var path := dir + id + ".wav"
	if _streams.has(path):
		return _streams[path]
	var stream: AudioStream = null
	if ResourceLoader.exists(path):
		stream = load(path) as AudioStream
	elif FileAccess.file_exists(path):
		stream = AudioStreamWAV.load_from_file(ProjectSettings.globalize_path(path))
	_streams[path] = stream
	return stream
