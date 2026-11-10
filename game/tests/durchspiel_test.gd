extends SceneTree
## Durchspieltest des Vertical Slice (seit M13a6, vertical_slice.md Abschnitt 12, Schritt a6).
##
## Spielt die Partie 1848–1871 je Staat vollständig über die Oberfläche durch:
## 1. Start über den Startbildschirm (Neues Spiel → Völkerfrühling → Staat → Spielbeginn).
## 2. In Schritten von SCHRITT_TAGE Tagen: Zeit vorrücken, die Hauptszene wie nach einem
##    Spielbild aktualisieren (_after_time_passed), offene Ereignisse im Ereignisfenster mit der
##    Standardoption beantworten, einen Wahlabend schließen.
## 3. Einmal im Jahr jedes Fenster der Bereichsleiste öffnen und wieder schließen.
## 4. Alle SPEICHER_JAHRE Jahre speichern, den Spielstand laden und die Prüfsumme vergleichen.
## 5. Am Ende: Das Urteil der Geschichte öffnet sich von selbst, die Chronik lässt sich als HTML
##    schreiben, „Weiterspielen“ schließt das Urteil.
## Jeder Fehler der Oberfläche (SCRIPT ERROR im Protokoll) zählt in der CI als Fehlschlag; der Test
## selbst prüft die Abläufe oben.
##
## Aufruf aus dem Repository-Hauptordner, nach einem Import (ohne Grafikausgabe möglich):
##   godot --headless --path game --script res://tests/durchspiel_test.gd
## Optionen nach "--": --staaten=PRU,AUT (Standard: PRU, AUT, BAV, SAX).
## Ergebnis: je Staat eine Zeile und "DURCHSPIEL OK" (Exitcode 0), sonst "DURCHSPIEL
## FEHLGESCHLAGEN" mit den Abweichungen (Exitcode 1).

const MAIN_SCENE := "res://scenes/main.tscn"
const SCENARIO := "voelkerfruehling"
const DEFAULT_STATES: PackedStringArray = ["PRU", "AUT", "BAV", "SAX"]
const SCHRITT_TAGE := 10
const SPEICHER_JAHRE := 5
const SAVE_DIR := "user://durchspiel_test"
## Sicherung gegen eine Partie ohne Ende: 1848–1871 sind gut 23 Jahre.
const MAX_TAGE := 26 * 365

var _main: Node = null
var _states := DEFAULT_STATES
var _failures := PackedStringArray()


func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--staaten="):
			_states = arg.trim_prefix("--staaten=").to_upper().split(",", false)
	_run()


func _run() -> void:
	print("EINIGKEIT – Durchspieltest 1848–1871 (%s)" % ", ".join(_states))
	_main = (load(MAIN_SCENE) as PackedScene).instantiate()
	root.add_child(_main)
	if not bool(_main.get("view_is_ready")):
		await _main.view_ready
	if _main.call("simulation") == null:
		printerr("DURCHSPIEL FEHLGESCHLAGEN: Simulation nicht geladen.")
		quit(1)
		return
	_main.set("save_dir", SAVE_DIR)
	for tag in _states:
		await _play(tag)
	_clear_saves()
	if _failures.is_empty():
		print("DURCHSPIEL OK %d/%d" % [_states.size(), _states.size()])
		quit(0)
	else:
		printerr("DURCHSPIEL FEHLGESCHLAGEN (%d):" % _failures.size())
		for failure in _failures:
			printerr("  " + failure)
		quit(1)


## Eine Partie als tag vom Startbildschirm bis zum Urteil.
func _play(tag: String) -> void:
	var started := Time.get_ticks_msec()
	if not await _start(tag):
		return
	var sim: SimulationClient = _main.call("simulation")
	var player := sim.get_player_state_index()
	var verdict: VerdictWindow = _main.call("verdict_window")
	var answered := 0
	var elections := 0
	var windows := 0
	var saves := 0
	var last_year := int(sim.get_date().get("year", 0))
	var days := 0
	while not sim.is_game_over() and days < MAX_TAGE:
		sim.advance_days(SCHRITT_TAGE)
		days += SCHRITT_TAGE
		_main.call("_after_time_passed")
		answered += _answer_events(tag)
		var night: ElectionNight = _main.call("election_night")
		if night.is_open():
			elections += 1
			_main.call("_close_election_night", false)
		if sim.is_game_over():
			break
		var year := int(sim.get_date().get("year", 0))
		if year != last_year:
			last_year = year
			windows += await _open_all_windows(tag)
			if year % SPEICHER_JAHRE == 0:
				saves += 1
				if not _save_and_load(tag, year):
					return
				sim = _main.call("simulation")
				player = sim.get_player_state_index()
		await process_frame
	if not sim.is_game_over():
		_fail(tag, "kein Ende der Partie bis %s" % sim.get_date_text())
		return
	await process_frame
	if not verdict.is_open():
		_fail(tag, "Urteil der Geschichte öffnet sich am Ende nicht (%s)" % sim.get_date_text())
		_main.call("open_verdict")
	var text := "\n".join(verdict.content_texts())
	if not text.contains("Vermächtnis"):
		_fail(tag, "Urteil ohne Vermächtnis: %s" % text.left(200).replace("\n", " | "))
	var path: String = _main.call("export_chronicle")
	if path.is_empty() or not FileAccess.file_exists(path):
		_fail(tag, "Chronik nicht exportiert (%s)" % path)
	else:
		DirAccess.remove_absolute(path)
	verdict.button("Continue").emit_signal("pressed")
	await process_frame
	if verdict.is_open():
		_fail(tag, "„Weiterspielen“ schließt das Urteil nicht")
	var legacy := PackedStringArray()
	for dimension: Variant in sim.get_legacy(player).get("dimensions", []):
		var d := dimension as Dictionary
		legacy.append("%s %.0f" % [str(d.get("name", "")), float(d.get("value", 0.0))])
	var outcome := "offen"
	for journal in sim.get_journals():
		if str(journal.get("id", "")) == "deutsche_frage" or str(journal.get("title", "")).contains("deutsche Frage"):
			outcome = str(journal.get("stage_title", outcome))
	var state := sim.get_state(player)
	print("  %s: Ende %s nach %.1f s; %d Ereignisse beantwortet, %d Wahlabende, %d Fenster, %d Spielstände; " % [
		tag, sim.get_date_text(), (Time.get_ticks_msec() - started) / 1000.0, answered, elections, windows, saves
	] + "%d Orte; deutsche Frage: %s; Vermächtnis: %s" % [
		int(state.get("location_count", 0)), outcome, ", ".join(legacy)
	])


## Neues Spiel über den Startbildschirm; false bei einem Fehler.
func _start(tag: String) -> bool:
	var start: StartScreen = _main.call("start_screen")
	_main.call("open_start_screen")
	await process_frame
	if not start.is_open():
		_fail(tag, "Startbildschirm öffnet sich nicht")
		return false
	start.menu_button("NewGame").emit_signal("pressed")
	start.select_scenario(SCENARIO)
	start.confirm_scenario()
	await process_frame
	if not start.choose_state(tag):
		_fail(tag, "Staat auf dem Startbildschirm nicht wählbar")
		start.close()
		return false
	start.confirm_state()
	await process_frame
	start.start_game()
	await process_frame
	var sim: SimulationClient = _main.call("simulation")
	var player := str(sim.get_state(sim.get_player_state_index()).get("tag", ""))
	if start.is_open() or player != tag or sim.get_scenario_name() != "Völkerfrühling":
		_fail(tag, "Partie als %s in „%s“ (Startbildschirm offen: %s)" % [player, sim.get_scenario_name(), start.is_open()])
		return false
	return true


## Offene Ereignisse des Spielers mit der Standardoption beantworten (sonst der ersten möglichen).
func _answer_events(tag: String) -> int:
	var window: EventWindow = _main.call("event_window")
	var answered := 0
	for _round in 30:
		if not window.is_open():
			break
		var event := window.current_event()
		var standard := int(event.get("default_option", -1))
		var target: Button = null
		for option: Variant in event.get("options", []):
			var index := int((option as Dictionary).get("index", -1))
			var button := window.option_button(index)
			if button == null or button.disabled:
				continue
			if target == null or index == standard:
				target = button
		if target == null:
			_fail(tag, "Ereignisfenster ohne mögliche Option: %s" % window.window_text().left(200))
			break
		target.pressed.emit()
		answered += 1
	if window.is_open():
		_fail(tag, "Ereignisfenster bleibt nach 30 Wahlen offen (%s)" % str(window.current_event().get("id", "")))
	return answered


## Jedes Fenster der Bereichsleiste öffnen und schließen; Zahl der geöffneten Fenster.
func _open_all_windows(tag: String) -> int:
	var bar: AreaBar = _main.call("area_bar")
	var windows: WindowManager = _main.call("window_manager")
	var count := 0
	for area in bar.visible_areas():
		var button := bar.button(StringName(area))
		if button == null or button.disabled:
			continue
		button.emit_signal("pressed")
		await process_frame
		var open := windows.open_window()
		if open != null:
			count += 1
			if not open.is_visible_in_tree():
				_fail(tag, "Bereich „%s“: Fenster offen, aber unsichtbar" % area)
		windows.close_all()
		(_main.call("state_panel") as StatePanel).hide_popups()
		_main.call("_sync_area_bar")
	return count


## Speichern, Laden, Prüfsumme vergleichen; false, wenn die Partie nicht weiterlaufen kann.
func _save_and_load(tag: String, year: int) -> bool:
	var sim: SimulationClient = _main.call("simulation")
	var before := sim.get_checksum()
	var date := sim.get_date_text()
	var name := "durchspiel_%s_%d" % [tag, year]
	var error: String = _main.call("save_game_as", name)
	if not error.is_empty():
		_fail(tag, "Speichern am %s: %s" % [date, error])
		return false
	if not bool(_main.call("load_game_file", SaveSlots.file_path(SAVE_DIR, name))):
		_fail(tag, "Laden des Spielstands vom %s misslingt" % date)
		return false
	sim = _main.call("simulation")
	if sim.get_checksum() != before or sim.get_date_text() != date:
		_fail(tag, "Spielstand vom %s: Prüfsumme %s statt %s (Datum %s)" % [date, sim.get_checksum(), before, sim.get_date_text()])
	DirAccess.remove_absolute(SaveSlots.file_path(SAVE_DIR, name))
	return true


func _fail(tag: String, message: String) -> void:
	_failures.append("%s: %s" % [tag, message])
	printerr("  FEHLER %s: %s" % [tag, message])


func _clear_saves() -> void:
	var absolute := SaveSlots.absolute_dir(SAVE_DIR)
	var access := DirAccess.open(absolute)
	if access == null:
		return
	for file in access.get_files():
		access.remove(file)
	DirAccess.remove_absolute(absolute)
