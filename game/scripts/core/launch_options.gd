class_name LaunchOptions
extends RefCounted
## Kommandozeilenoptionen nach "--" (OS.get_cmdline_user_args()).
##
##   --screenshot=<pfad>    nach dem Laden simulieren, Bild speichern, beenden
##   --tage=<n>             Tage vor dem Screenshot (Standard 60)
##   --kartenmodus=<0..9>   Kartenmodus (0 Politisch … 5 Kreise, 6 Lebensstandard, 7 Kultur,
##                          8 Wirtschaft, 9 Märkte)
##   --ort=<ort_id>         Ort auswählen und Ortsinfo öffnen
##   --zoom=<faktor>        Vergrößerung gegenüber der Übersicht (1 = ganze Karte);
##                          zentriert auf --ort, sonst auf die Kartenmitte (bis 1: Übersicht)
##   --haushalt             Haushaltsfenster des Spielerstaates öffnen (seit M5)
##   --spielmenue           Spielmenü öffnen (seit M6)

const DEFAULT_SCREENSHOT_DAYS := 60

var screenshot_path := ""
var days := DEFAULT_SCREENSHOT_DAYS
var map_mode := -1
var location_id := ""
## Vergrößerung gegenüber der Übersicht; 0 = Übersicht ohne Zentrierung.
var zoom_factor := 0.0
## Haushaltsfenster vor dem Screenshot öffnen (seit M5).
var show_budget := false
var show_game_menu := false
## Unbekannte oder fehlerhafte Angaben (deutsche Meldungen).
var problems := PackedStringArray()


static func from_command_line() -> LaunchOptions:
	return parse(OS.get_cmdline_user_args())


static func parse(args: PackedStringArray) -> LaunchOptions:
	var options := LaunchOptions.new()
	for arg in args:
		var key := arg
		var value := ""
		var separator := arg.find("=")
		if separator >= 0:
			key = arg.left(separator)
			value = arg.substr(separator + 1)
		match key:
			"--screenshot":
				if value.is_empty():
					options.problems.append(
						"--screenshot braucht einen Pfad (--screenshot=/pfad/bild.png)."
					)
				else:
					options.screenshot_path = value
			"--tage":
				if value.is_valid_int() and value.to_int() >= 0:
					options.days = value.to_int()
				else:
					options.problems.append(
						"--tage erwartet eine nicht negative Ganzzahl, nicht „%s“." % value
					)
			"--kartenmodus":
				if value.is_valid_int() and MapModes.is_valid(value.to_int()):
					options.map_mode = value.to_int()
				else:
					options.problems.append(
						"--kartenmodus erwartet 0 bis %d, nicht „%s“."
						% [MapModes.COUNT - 1, value]
					)
			"--zoom":
				if value.is_valid_float() and value.to_float() > 0.0:
					options.zoom_factor = value.to_float()
				else:
					options.problems.append(
						"--zoom erwartet eine positive Zahl (z. B. --zoom=4), nicht „%s“." % value
					)
			"--haushalt":
				if value.is_empty():
					options.show_budget = true
				else:
					options.problems.append("--haushalt erwartet keinen Wert, nicht „%s“." % value)
			"--spielmenue":
				if value.is_empty():
					options.show_game_menu = true
				else:
					options.problems.append("--spielmenue erwartet keinen Wert, nicht „%s“." % value)
			"--ort":
				if value.is_empty():
					options.problems.append("--ort braucht eine Ort-Kennung (--ort=berlin_coelln).")
				else:
					options.location_id = value
			_:
				options.problems.append("Unbekannte Option „%s“ wird ignoriert." % arg)
	return options


func wants_screenshot() -> bool:
	return not screenshot_path.is_empty()


## Absoluter Pfad des Screenshots. Relative Angaben beziehen sich auf das
## Verzeichnis, aus dem Godot gestartet wurde ($PWD), sonst auf den Projektordner
## (Godot wechselt beim Start mit --path in den Projektordner).
func resolved_screenshot_path() -> String:
	var path := screenshot_path.replace("\\", "/")
	if path.is_relative_path():
		var start_dir := OS.get_environment("PWD")
		if start_dir.is_empty():
			start_dir = ProjectSettings.globalize_path("res://")
		path = start_dir.path_join(path)
	return path.simplify_path()
