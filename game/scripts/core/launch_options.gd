class_name LaunchOptions
extends RefCounted
## Kommandozeilenoptionen nach "--" (OS.get_cmdline_user_args()).
##
##   --screenshot=<pfad>    nach dem Laden simulieren, Bild speichern, beenden
##   --tage=<n>             Tage vor dem Screenshot (Standard 60)
##   --kartenmodus=<0..17>  Kartenmodus (0 Politisch … 5 Kreise, 6 Lebensstandard, 7 Kultur,
##                          8 Wirtschaft, 9 Märkte, 10 Verwaltung, 11 Eisenbahnen, 12 Wahlen,
##                          13 Zollgebiete, 14 Beziehungen, 15 Krieg, 16 Unruhe, 17 Bund)
##   --ort=<ort_id>         Ort auswählen und Ortsinfo öffnen
##   --zoom=<faktor>        Vergrößerung gegenüber der Übersicht (1 = ganze Karte);
##                          zentriert auf --ort, sonst auf die Kartenmitte (bis 1: Übersicht)
##   --haushalt             Haushaltsfenster des Spielerstaates öffnen (seit M5)
##   --verfassung           Fenster „Verfassung und Gruppen“ des Spielerstaates öffnen (seit M8)
##   --zollunion            Fenster „Zollunion“ des Spielerstaates öffnen (seit M9)
##   --diplomatie[=<reiter>]  Fenster „Diplomatie“ öffnen, Reiter staaten (Standard), bund oder
##                          frage (seit M9, nur mit dem Deutschen Bund)
##   --krieg[=<reiter>]     Kriegsfenster öffnen, Reiter kriege (Standard) oder heere (seit M10,
##                          nur in Szenarien mit Heeren)
##   --heer                 stärkstes eigenes Feldheer auswählen (Heeresfenster, seit M10)
##   --spielmenue           Spielmenü öffnen (seit M6)
##   --szenario=<id>        Szenario beim Start (seit M7; Standard westfaelischer_frieden)
##   --staat=<TAG>          Spielerstaat beim Start (seit M7; Standard: Spieler des Szenarios)
##   --wahlabend            Wahlabend der letzten Wahl öffnen, ganz gemeldet (seit M8)
##   --partei=<strömung>    Parteienmodus mit dieser Strömung, z. B. liberale (seit M8)
##   --ereignis             Ereignisse des Spielerstaates warten auf die Wahl; das Ereignisfenster
##                          zeigt die offenen (seit M11; ohne rechnen Screenshots ohne Spieler)
##   --journal              Journalfenster öffnen (seit M11)
##   --startbildschirm[=<seite>]  Startbildschirm vor dem Screenshot öffnen, Seite menue (Standard),
##                          lesezeichen oder einstellungen (seit M13; einstellungen: mit --szenario und
##                          dem Spielerstaat des Lesezeichens)

const DEFAULT_SCREENSHOT_DAYS := 60
## Reiter des Fensters „Diplomatie“ für --diplomatie=<reiter>.
const DIPLOMACY_TABS: PackedStringArray = ["staaten", "bund", "frage"]
## Reiter des Kriegsfensters für --krieg=<reiter> (seit M10).
const WAR_TABS: PackedStringArray = ["kriege", "heere"]

var screenshot_path := ""
var days := DEFAULT_SCREENSHOT_DAYS
var map_mode := -1
var location_id := ""
## Vergrößerung gegenüber der Übersicht; 0 = Übersicht ohne Zentrierung.
var zoom_factor := 0.0
## Haushaltsfenster vor dem Screenshot öffnen (seit M5).
var show_budget := false
## Fenster „Verfassung und Gruppen“ vor dem Screenshot öffnen (seit M8).
var show_politics := false
## Fenster „Zollunion“ vor dem Screenshot öffnen (seit M9).
var show_customs := false
## Reiter des Fensters „Diplomatie“ vor dem Screenshot (seit M9): 0 Staaten, 1 Bundesversammlung,
## 2 Deutsche Frage; -1 = geschlossen.
var diplomacy_tab := -1
## Reiter des Kriegsfensters vor dem Screenshot (seit M10): 0 Kriege, 1 Heere; -1 = geschlossen.
var war_tab := -1
## Stärkstes eigenes Feldheer vor dem Screenshot auswählen (seit M10).
var select_army := false
var show_game_menu := false
## Wahlabend der letzten Wahl vor dem Screenshot öffnen (seit M8).
var show_election_night := false
## Ereignisse des Spielerstaates warten auf die Wahl, das Ereignisfenster zeigt sie (seit M11).
var show_events := false
## Journalfenster vor dem Screenshot öffnen (seit M11).
var show_journal := false
## Startbildschirm vor dem Screenshot (seit M13): 0 Hauptmenü, 1 Lesezeichen, 2 Einstellungen; -1 = keiner.
var start_page := -1
## Seiten für --startbildschirm=<seite>.
const START_PAGES: PackedStringArray = ["menue", "lesezeichen", "einstellungen"]
## Strömung im Parteienmodus beim Start (seit M8); leer = Staat.
var party_id := ""
## Szenario und Spielerstaat beim Start (seit M7); leer = Standard.
var scenario_id := ""
var state_tag := ""
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
			"--verfassung":
				if value.is_empty():
					options.show_politics = true
				else:
					options.problems.append("--verfassung erwartet keinen Wert, nicht „%s“." % value)
			"--zollunion":
				if value.is_empty():
					options.show_customs = true
				else:
					options.problems.append("--zollunion erwartet keinen Wert, nicht „%s“." % value)
			"--diplomatie":
				var tab := DIPLOMACY_TABS.find(value if not value.is_empty() else "staaten")
				if tab >= 0:
					options.diplomacy_tab = tab
				else:
					options.problems.append(
						"--diplomatie erwartet staaten, bund oder frage, nicht „%s“." % value
					)
			"--krieg":
				var war := WAR_TABS.find(value if not value.is_empty() else "kriege")
				if war >= 0:
					options.war_tab = war
				else:
					options.problems.append("--krieg erwartet kriege oder heere, nicht „%s“." % value)
			"--heer":
				if value.is_empty():
					options.select_army = true
				else:
					options.problems.append("--heer erwartet keinen Wert, nicht „%s“." % value)
			"--spielmenue":
				if value.is_empty():
					options.show_game_menu = true
				else:
					options.problems.append("--spielmenue erwartet keinen Wert, nicht „%s“." % value)
			"--partei":
				if value.is_empty():
					options.problems.append("--partei braucht eine Strömung (--partei=liberale).")
				else:
					options.party_id = value
			"--wahlabend":
				if value.is_empty():
					options.show_election_night = true
				else:
					options.problems.append("--wahlabend erwartet keinen Wert, nicht „%s“." % value)
			"--ereignis":
				if value.is_empty():
					options.show_events = true
				else:
					options.problems.append("--ereignis erwartet keinen Wert, nicht „%s“." % value)
			"--journal":
				if value.is_empty():
					options.show_journal = true
				else:
					options.problems.append("--journal erwartet keinen Wert, nicht „%s“." % value)
			"--startbildschirm":
				var page := 0 if value.is_empty() else START_PAGES.find(value)
				if page >= 0:
					options.start_page = page
				else:
					options.problems.append(
						"--startbildschirm erwartet menue, lesezeichen oder einstellungen, nicht „%s“." % value
					)
			"--szenario":
				if value.is_empty():
					options.problems.append(
						"--szenario braucht eine Kennung (--szenario=voelkerfruehling)."
					)
				else:
					options.scenario_id = value
			"--staat":
				if value.is_empty():
					options.problems.append("--staat braucht einen Staatstag (--staat=BAV).")
				else:
					options.state_tag = value.to_upper()
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
