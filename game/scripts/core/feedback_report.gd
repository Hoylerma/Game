class_name FeedbackReport
extends RefCounted
## Rückmeldung (seit M13b5, vertical_slice.md 7): legt ein ZIP-Archiv in user://rueckmeldungen an,
## mit Spielstand, Notiz, Angaben zum Rechner und zur Partie, Godot-Protokoll und Einstellungen.
## Es wird nichts versandt; die Spielerin oder der Spieler schickt die Datei selbst (Spieltest, b6).
##
## Inhalt des Archivs:
##   info.txt              Version, System, Grafik, Sprache, Szenario, Datum, Prüfsumme, Bildrate (lesbar)
##   messwerte.json        dieselben Angaben maschinenlesbar für tools/spieltest (seit M13b6)
##   notiz.txt             freier Text (kann leer sein)
##   spielstand.einigkeit  laufende Partie (fehlt ohne Simulation)
##   godot.log             Protokoll dieser Sitzung (fehlt, wenn Godot keins schreibt)
##   einstellungen.cfg     Einstellungen (fehlt ohne Datei)
##
## Seit M13b6 steht statt des Benutzerordners „~“ in info.txt, messwerte.json und godot.log (Pfade
## enthalten oft den Benutzernamen).

const DIR := "user://rueckmeldungen"
const LOG_PATH := "user://logs/godot.log"
## Format von messwerte.json; tools/spieltest prüft es.
const FORMAT := 1


## Archiv schreiben. frames: FrameMeter.summary() oder leer. Ergebnis: { path (absolut), files (Namen im
## Archiv), error (leer bei Erfolg) }.
static func write(
	sim: SimulationClient, player: int, note: String, dir: String = DIR, frames: Dictionary = {}
) -> Dictionary:
	var absolute := ProjectSettings.globalize_path(dir)
	if DirAccess.make_dir_recursive_absolute(absolute) != OK:
		return {"path": "", "files": PackedStringArray(), "error": "Ordner %s lässt sich nicht anlegen." % absolute}
	var stamp := Time.get_datetime_string_from_system(false, true).replace(":", "-").replace(" ", "_")
	var path := absolute.path_join("rueckmeldung_%s.zip" % stamp)
	var zip := ZIPPacker.new()
	if zip.open(path) != OK:
		return {"path": "", "files": PackedStringArray(), "error": "%s lässt sich nicht schreiben." % path}
	var files := PackedStringArray()
	var values := measurements(sim, player, frames)
	_add(zip, files, "info.txt", info_text(values).to_utf8_buffer())
	_add(zip, files, "messwerte.json", (JSON.stringify(values, "  ", false) + "\n").to_utf8_buffer())
	_add(zip, files, "notiz.txt", (note.strip_edges() + "\n").to_utf8_buffer())
	if sim != null:
		var temp := absolute.path_join(".spielstand.tmp")
		if sim.save_game(temp, player).is_empty():
			_add(zip, files, "spielstand.%s" % SaveSlots.EXTENSION, FileAccess.get_file_as_bytes(temp))
		DirAccess.remove_absolute(temp)
	if FileAccess.file_exists(LOG_PATH):
		_add(zip, files, "godot.log", anonymize(FileAccess.get_file_as_string(LOG_PATH)).to_utf8_buffer())
	if FileAccess.file_exists(UserSettings.PATH):
		_add(zip, files, "einstellungen.cfg", FileAccess.get_file_as_bytes(UserSettings.PATH))
	zip.close()
	return {"path": path, "files": files, "error": ""}


## Angaben zu Programm, Rechner, Sitzung und Partie (Schlüssel wie in messwerte.json).
static func measurements(sim: SimulationClient, player: int, frames: Dictionary = {}) -> Dictionary:
	var headless := DisplayServer.get_name() == "headless"
	var values := {
		"format": FORMAT,
		"version": str(ProjectSettings.get_setting("application/config/version", "")),
		"godot": str(Engine.get_version_info().get("string", "")),
		"system": "%s %s" % [OS.get_name(), OS.get_version()],
		"prozessor": "%s (%d Kerne)" % [OS.get_processor_name(), OS.get_processor_count()],
		"grafik": "keine (headless)" if headless else " / ".join(OS.get_video_adapter_driver_info()),
		"grafikkarte": "" if headless else RenderingServer.get_video_adapter_name(),
		"bildschirm": _size_text(DisplayServer.screen_get_size()),
		"fenster": "" if headless else _size_text(DisplayServer.window_get_size()),
		"vollbild": not headless and DisplayServer.window_get_mode() >= DisplayServer.WINDOW_MODE_FULLSCREEN,
		"bildratenlimit": Engine.max_fps,
		"sprache": TranslationServer.get_locale(),
		"systemsprache": OS.get_locale(),
		"zeit": Time.get_datetime_string_from_system(false, true),
		"daten": anonymize("%s (%s)" % [UhrwerkPaths.data_dir(), UhrwerkPaths.data_dir_origin()]),
		"bildrate_median": snappedf(float(frames.get("median", 0.0)), 0.1),
		"bildrate_p5": snappedf(float(frames.get("p5", 0.0)), 0.1),
		"bildrate_min": snappedf(float(frames.get("min", 0.0)), 0.1),
		"bildrate_sekunden": int(frames.get("seconds", 0)),
		"sitzung_sekunden": int(frames.get("session_seconds", 0)),
	}
	if sim != null:
		var settings := sim.get_game_settings()
		var verdict := sim.get_verdict()
		values.merge({
			"szenario": str(sim.get_scenario().get("id", "")),
			"spieldatum": sim.get_date_text(),
			"spieltage": sim.get_days_elapsed(),
			"spieler": str(sim.get_state(player).get("tag", "")) if player >= 0 else "",
			"stufe": str(settings.get("difficulty_id", "")),
			"verlauf": "offen" if bool(settings.get("open", false)) else "historisch",
			"partie_beendet": bool(verdict.get("over", false)),
			"pruefsumme": sim.get_checksum(),
		})
	return values


## Lesbare Fassung der Messwerte, eine Angabe je Zeile („Schlüssel: Wert“).
static func info_text(values: Dictionary) -> String:
	var lines := PackedStringArray(["EINIGKEIT Rückmeldung"])
	var labels := [
		["version", "Version"], ["godot", "Godot"], ["system", "System"], ["prozessor", "Prozessor"],
		["grafik", "Grafik"], ["grafikkarte", "Grafikkarte"], ["bildschirm", "Bildschirm"], ["fenster", "Fenster"],
		["sprache", "Sprache"], ["zeit", "Zeit"], ["daten", "Daten"], ["szenario", "Szenario"],
		["spieldatum", "Spieldatum"], ["spieler", "Spieler"], ["stufe", "Stufe"], ["verlauf", "Verlauf"],
		["pruefsumme", "Prüfsumme"],
	]
	for pair: Array in labels:
		if not str(values.get(pair[0], "")).is_empty():
			lines.append("%s: %s" % [pair[1], values[pair[0]]])
	lines.append("Vollbild: %s, Bildratenlimit: %s" % [
		"ja" if values.get("vollbild", false) else "nein", values.get("bildratenlimit", 0)
	])
	lines.append("Bildrate: Median %s, 5 %% %s, Minimum %s (über %d s)" % [
		values.get("bildrate_median", 0), values.get("bildrate_p5", 0), values.get("bildrate_min", 0),
		values.get("bildrate_sekunden", 0),
	])
	lines.append("Sitzung: %d min" % (int(values.get("sitzung_sekunden", 0)) / 60))
	if values.has("partie_beendet"):
		lines.append("Partie beendet: %s" % ("ja" if values["partie_beendet"] else "nein"))
	return "\n".join(lines) + "\n"


## Benutzerordner durch „~“ ersetzen (Linux und macOS HOME, Windows USERPROFILE; beide Schreibweisen der
## Trennzeichen).
static func anonymize(text: String) -> String:
	var result := text
	for variable in ["USERPROFILE", "HOME"]:
		var home := OS.get_environment(variable).strip_edges()
		if home.length() < 3:
			continue
		for form in [home, home.replace("\\", "/"), home.replace("/", "\\")]:
			result = result.replace(form, "~")
	return result


static func _size_text(size: Vector2i) -> String:
	return "%dx%d" % [size.x, size.y]


static func _add(zip: ZIPPacker, files: PackedStringArray, file_name: String, bytes: PackedByteArray) -> void:
	zip.start_file(file_name)
	zip.write_file(bytes)
	zip.close_file()
	files.append(file_name)
