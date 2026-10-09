class_name UhrwerkPaths
extends RefCounted
## Auflösung der Pfade zu den Spieldaten.
##
## Die Spieldaten liegen nicht im Godot-Projekt, sondern im Repository unter
## data/. Statische Hilfsklasse, damit sie auch im --script-Modus (Tests)
## ohne Szenenbaum funktioniert.

## Umgebungsvariable, die den Datenordner überschreibt.
const ENV_DATA_DIR := "UHRWERK_DATEN"
## Standardort relativ zum Projektordner (game/).
const DEFAULT_DATA_DIR := "res://../data"
## Szenario, das der Prototyp lädt.
const DEFAULT_SCENARIO := "westfaelischer_frieden"
## Startwert der Zufallsströme (wie uhrwerk-cli).
const DEFAULT_SEED := 1648


## Absoluter, normalisierter Pfad zum Datenordner.
static func data_dir() -> String:
	var from_env := OS.get_environment(ENV_DATA_DIR).strip_edges()
	if not from_env.is_empty():
		return _normalize(from_env)
	return _normalize(ProjectSettings.globalize_path(DEFAULT_DATA_DIR))


## Ordner mit den Kartendateien (provinzen.png, definition.csv, karte.chron).
static func map_dir() -> String:
	return data_dir().path_join("karte")


static func province_image_path() -> String:
	return map_dir().path_join("provinzen.png")


static func definition_csv_path() -> String:
	return map_dir().path_join("definition.csv")


static func map_config_path() -> String:
	return map_dir().path_join("karte.chron")


## Beschreibt, woher der Datenordner kommt (für Meldungen).
static func data_dir_origin() -> String:
	if not OS.get_environment(ENV_DATA_DIR).strip_edges().is_empty():
		return "Umgebungsvariable %s" % ENV_DATA_DIR
	return "Standard (%s)" % DEFAULT_DATA_DIR


static func _normalize(path: String) -> String:
	var result := path.replace("\\", "/")
	if result.begins_with("res://") or result.begins_with("user://"):
		result = ProjectSettings.globalize_path(result)
	elif result.is_relative_path():
		# Relative Angaben in UHRWERK_DATEN beziehen sich auf den Projektordner.
		result = ProjectSettings.globalize_path("res://").path_join(result)
	result = result.simplify_path()
	if result.length() > 1 and result.ends_with("/"):
		result = result.left(result.length() - 1)
	return result
