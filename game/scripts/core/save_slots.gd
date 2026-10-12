class_name SaveSlots
extends RefCounted
## Spielstände im Benutzerordner (seit M6, docs/technik/speicherstaende.md Abschnitt 9):
## Dateinamen, Liste für das Spielmenü, Schnellspeicher und Rotation der automatischen
## Spielstände. Statische Hilfsklasse; die Dateien schreibt und liest die Brücke.

## Ordner der Spielstände (Godot-Pfad; die Brücke bekommt den absoluten Pfad).
const DEFAULT_DIR := "user://spielstaende"
const EXTENSION := "einigkeit"
## Schnellspeichern (F5) und Schnellladen (F9).
const QUICKSAVE := "schnellspeicher"
## Automatische Spielstände am 1. Januar: autosave_1 (neuester) bis autosave_<AUTOSAVE_KEEP>.
const AUTOSAVE_PREFIX := "autosave_"
const AUTOSAVE_KEEP := 3
const MAX_NAME_LENGTH := 60
## Zeichen, die in Dateinamen auf einem der Systeme nicht erlaubt sind.
const FORBIDDEN := "/\\:*?\"<>|"


## Absoluter Pfad des Ordners.
static func absolute_dir(dir: String = DEFAULT_DIR) -> String:
	return ProjectSettings.globalize_path(dir)


## Absoluter Pfad eines Spielstands (Name ohne Endung).
static func file_path(dir: String, file_name: String) -> String:
	return absolute_dir(dir).path_join(file_name + "." + EXTENSION)


static func quicksave_path(dir: String = DEFAULT_DIR) -> String:
	return file_path(dir, QUICKSAVE)


static func autosave_path(dir: String = DEFAULT_DIR) -> String:
	return file_path(dir, AUTOSAVE_PREFIX + "1")


## Dateiname aus einer Eingabe: verbotene Zeichen und Steuerzeichen werden zu „_“, Endung,
## Leerraum und Punkte am Rand entfallen, höchstens MAX_NAME_LENGTH Zeichen. Leer, wenn nichts
## übrig bleibt.
static func sanitize(input: String) -> String:
	var result := ""
	for i in input.length():
		var c := input[i]
		result += "_" if c.unicode_at(0) < 32 or FORBIDDEN.contains(c) else c
	result = result.strip_edges()
	if result.to_lower().ends_with("." + EXTENSION):
		result = result.left(-(EXTENSION.length() + 1))
	result = result.strip_edges()
	while result.begins_with(".") or result.ends_with("."):
		result = result.trim_prefix(".").trim_suffix(".").strip_edges()
	if result.length() > MAX_NAME_LENGTH:
		result = result.left(MAX_NAME_LENGTH).strip_edges()
	return result


## Vorschlag für den Namen eines Spielstands: Staat und Datum, z. B. „Brandenburg 1650-03-12“.
static func default_name(state_name: String, date: Dictionary) -> String:
	var text := "%04d-%02d-%02d" % [
		int(date.get("year", 0)), int(date.get("month", 0)), int(date.get("day", 0))
	]
	if not state_name.is_empty():
		text = state_name + " " + text
	return sanitize(text)


static func is_leap_year(year: int) -> bool:
	return (year % 4 == 0 and year % 100 != 0) or year % 400 == 0


## Tage bis zum nächsten 1. Januar (am 31. Dezember 1): so viele Tage darf die Oberfläche
## höchstens weiterschalten, um den Jahreswechsel für den automatischen Spielstand zu treffen.
static func days_until_new_year(date: Dictionary) -> int:
	var year := int(date.get("year", 1))
	var month := clampi(int(date.get("month", 1)), 1, 12)
	var lengths: PackedInt32Array = [
		31, 29 if is_leap_year(year) else 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
	]
	var days := lengths[month - 1] - int(date.get("day", 1)) + 1
	for m in range(month, 12):
		days += lengths[m]
	return maxi(days, 1)


## Spielstände im Ordner, zuletzt geänderte zuerst:
## { path, name, modified (Unix-Zeit), info (get_save_info der Brücke, ohne Simulation leer) }.
static func list(sim: SimulationClient, dir: String = DEFAULT_DIR) -> Array[Dictionary]:
	var result: Array[Dictionary] = []
	var absolute := absolute_dir(dir)
	var access := DirAccess.open(absolute)
	if access == null:
		return result
	for file in access.get_files():
		if file.get_extension() != EXTENSION:
			continue
		var path := absolute.path_join(file)
		result.append({
			"path": path,
			"name": file.get_basename(),
			"modified": FileAccess.get_modified_time(path),
			"info": sim.get_save_info(path) if sim != null else {},
		})
	result.sort_custom(
		func(a: Dictionary, b: Dictionary) -> bool:
			if int(a["modified"]) != int(b["modified"]):
				return int(a["modified"]) > int(b["modified"])
			return str(a["name"]) < str(b["name"])
	)
	return result


## Vor einem automatischen Spielstand: autosave_<keep> entfällt, die übrigen rücken eine Stelle
## weiter, danach ist autosave_1 frei. Legt den Ordner an.
static func rotate_autosaves(dir: String = DEFAULT_DIR, keep: int = AUTOSAVE_KEEP) -> void:
	DirAccess.make_dir_recursive_absolute(absolute_dir(dir))
	var oldest := file_path(dir, AUTOSAVE_PREFIX + str(keep))
	if FileAccess.file_exists(oldest):
		DirAccess.remove_absolute(oldest)
	for n in range(keep - 1, 0, -1):
		var from := file_path(dir, AUTOSAVE_PREFIX + str(n))
		if FileAccess.file_exists(from):
			DirAccess.rename_absolute(from, file_path(dir, AUTOSAVE_PREFIX + str(n + 1)))


## Änderungszeit in Ortszeit, z. B. „12.10.2026, 09:05“.
static func format_modified(unix_time: int) -> String:
	var bias_minutes := int(Time.get_time_zone_from_system().get("bias", 0))
	var t := Time.get_datetime_dict_from_unix_time(unix_time + bias_minutes * 60)
	return "%02d.%02d.%04d, %02d:%02d" % [
		int(t["day"]), int(t["month"]), int(t["year"]), int(t["hour"]), int(t["minute"])
	]
