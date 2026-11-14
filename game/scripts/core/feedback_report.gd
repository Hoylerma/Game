class_name FeedbackReport
extends RefCounted
## Rückmeldung (seit M13b5, vertical_slice.md 7): legt ein ZIP-Archiv in user://rueckmeldungen an,
## mit Spielstand, Notiz, Angaben zum Rechner und zur Partie, Godot-Protokoll und Einstellungen.
## Es wird nichts versandt; die Spielerin oder der Spieler schickt die Datei selbst (Spieltest, b6).
##
## Inhalt des Archivs:
##   info.txt              Version, System, Grafik, Sprache, Szenario, Datum, Prüfsumme
##   notiz.txt             freier Text (kann leer sein)
##   spielstand.einigkeit  laufende Partie (fehlt ohne Simulation)
##   godot.log             Protokoll dieser Sitzung (fehlt, wenn Godot keins schreibt)
##   einstellungen.cfg     Einstellungen (fehlt ohne Datei)

const DIR := "user://rueckmeldungen"
const LOG_PATH := "user://logs/godot.log"


## Archiv schreiben. Ergebnis: { path (absolut), files (Namen im Archiv), error (leer bei Erfolg) }.
static func write(sim: SimulationClient, player: int, note: String, dir: String = DIR) -> Dictionary:
	var absolute := ProjectSettings.globalize_path(dir)
	if DirAccess.make_dir_recursive_absolute(absolute) != OK:
		return {"path": "", "files": PackedStringArray(), "error": "Ordner %s lässt sich nicht anlegen." % absolute}
	var stamp := Time.get_datetime_string_from_system(false, true).replace(":", "-").replace(" ", "_")
	var path := absolute.path_join("rueckmeldung_%s.zip" % stamp)
	var zip := ZIPPacker.new()
	if zip.open(path) != OK:
		return {"path": "", "files": PackedStringArray(), "error": "%s lässt sich nicht schreiben." % path}
	var files := PackedStringArray()
	_add(zip, files, "info.txt", info_text(sim, player).to_utf8_buffer())
	_add(zip, files, "notiz.txt", (note.strip_edges() + "\n").to_utf8_buffer())
	if sim != null:
		var temp := absolute.path_join(".spielstand.tmp")
		if sim.save_game(temp, player).is_empty():
			_add(zip, files, "spielstand.%s" % SaveSlots.EXTENSION, FileAccess.get_file_as_bytes(temp))
		DirAccess.remove_absolute(temp)
	for entry in [[LOG_PATH, "godot.log"], [UserSettings.PATH, "einstellungen.cfg"]]:
		if FileAccess.file_exists(entry[0]):
			_add(zip, files, entry[1], FileAccess.get_file_as_bytes(entry[0]))
	zip.close()
	return {"path": path, "files": files, "error": ""}


## Angaben zu Programm, Rechner und Partie, eine je Zeile („Schlüssel: Wert“).
static func info_text(sim: SimulationClient, player: int) -> String:
	var lines := PackedStringArray()
	lines.append("EINIGKEIT Rückmeldung")
	lines.append("Version: %s" % ProjectSettings.get_setting("application/config/version", ""))
	lines.append("Godot: %s" % Engine.get_version_info().get("string", ""))
	lines.append("System: %s %s" % [OS.get_name(), OS.get_version()])
	lines.append("Prozessor: %s (%d Kerne)" % [OS.get_processor_name(), OS.get_processor_count()])
	lines.append("Grafik: %s" % " / ".join(OS.get_video_adapter_driver_info()) if DisplayServer.get_name() != "headless" else "Grafik: keine (headless)")
	lines.append("Bildschirm: %s" % str(DisplayServer.screen_get_size()))
	lines.append("Sprache: %s (System %s)" % [TranslationServer.get_locale(), OS.get_locale()])
	lines.append("Zeit: %s" % Time.get_datetime_string_from_system(false, true))
	lines.append("Daten: %s (%s)" % [UhrwerkPaths.data_dir(), UhrwerkPaths.data_dir_origin()])
	if sim != null:
		lines.append("Szenario: %s" % str(sim.get_scenario().get("id", "")))
		lines.append("Spieldatum: %s" % sim.get_date_text())
		lines.append("Spieler: %s" % (str(sim.get_state(player).get("tag", "")) if player >= 0 else "keiner"))
		lines.append("Prüfsumme: %s" % sim.get_checksum())
	return "\n".join(lines) + "\n"


static func _add(zip: ZIPPacker, files: PackedStringArray, file_name: String, bytes: PackedByteArray) -> void:
	zip.start_file(file_name)
	zip.write_file(bytes)
	zip.close_file()
	files.append(file_name)
