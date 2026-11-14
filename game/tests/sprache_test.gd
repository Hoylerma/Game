extends SceneTree
## Sprachtest (seit M13b4, vertical_slice.md 6): Startbildschirm (alle Seiten), Spielmenü und Einstellungen
## (alle Reiter) mit dem Pseudo-Locale – jeder übersetzte Text 40 % länger und mit Akzenten – und auf
## Englisch.
##
## Geprüft wird, dass keine Beschriftung abgeschnitten ist: Jedes Fenster liegt ganz im Bild, und jede
## sichtbare Beschriftung (Label, Button, CheckBox, OptionButton) ohne Zeilenumbruch hat Platz für ihren
## angezeigten Text. Absichtlich gekürzte Einträge (Auslassungspunkte, etwa lange Namen von Spielständen)
## zählen nicht. Auf Englisch müssen die Texte der drei Bildschirme übersetzt sein (Stichproben).
##
## Aufruf aus dem Repository-Hauptordner, nach einem Import (ohne Grafikausgabe möglich):
##   godot --headless --path game --script res://tests/sprache_test.gd
## Ergebnis: "SPRACHE OK" (Exitcode 0), sonst "SPRACHE FEHLGESCHLAGEN" mit den Fundstellen.

const MAIN_SCENE := "res://scenes/main.tscn"

var _main: Node = null
var _failures := PackedStringArray()
var _checked := 0


func _initialize() -> void:
	_run()


func _run() -> void:
	print("EINIGKEIT – Sprachtest (Pseudo-Locale und Englisch)")
	_main = (load(MAIN_SCENE) as PackedScene).instantiate()
	root.add_child(_main)
	if not bool(_main.get("view_is_ready")):
		await _main.view_ready
	for language in ["pseudo", "en"]:
		Translation_.use(language)
		await _check_screens(language)
	Translation_.use("de")
	await _check_english_samples()
	if _failures.is_empty():
		print("SPRACHE OK (%d Beschriftungen geprüft)" % _checked)
		quit(0)
	else:
		printerr("SPRACHE FEHLGESCHLAGEN (%d):" % _failures.size())
		for failure in _failures:
			printerr("  - " + failure)
		quit(1)


func _check_screens(language: String) -> void:
	var start: StartScreen = _main.call("start_screen")
	_main.call("open_start_screen")
	await _frames(2)
	_check_tree(start, "%s Startbildschirm Menü" % language)
	start.menu_button("NewGame").emit_signal("pressed")
	await _frames(2)
	_check_tree(start, "%s Startbildschirm Lesezeichen" % language)
	start.select_scenario("voelkerfruehling")
	start.confirm_scenario()
	await _frames(3)
	_check_tree(start, "%s Startbildschirm Staat" % language)
	start.choose_state("PRU")
	start.confirm_state()
	await _frames(2)
	_check_tree(start, "%s Startbildschirm Einstellungen" % language)
	start.close()
	_main.call("open_game_menu")
	await _frames(2)
	_check_tree(_main.call("game_menu"), "%s Spielmenü" % language)
	_main.call("close_game_menu")
	_main.call("open_settings")
	var settings: SettingsWindow = _main.call("settings_window")
	for tab in SettingsWindow.GROUPS.size():
		settings.show_tab(tab)
		await _frames(2)
		_check_tree(settings, "%s Einstellungen %s" % [language, SettingsWindow.GROUPS[tab]])
	_main.call("close_settings")


## Fenster im Bild, Beschriftungen ohne Zeilenumbruch mit genug Platz.
func _check_tree(window: Control, where: String) -> void:
	var area := window.get_viewport().get_visible_rect()
	var panel := _panel_of(window)
	if not area.grow(0.5).encloses(panel.get_global_rect()):
		_failures.append("%s: Fenster %s ragt aus dem Bild %s" % [where, panel.get_global_rect(), area.size])
	for node in window.find_children("*", "Control", true, false):
		var control := node as Control
		if not control.is_visible_in_tree():
			continue
		var text := ""
		var wraps := false
		var trims := false
		if control is Label:
			var label := control as Label
			text = label.text
			wraps = label.autowrap_mode != TextServer.AUTOWRAP_OFF
			trims = label.text_overrun_behavior != TextServer.OVERRUN_NO_TRIMMING and label.text_overrun_behavior != TextServer.OVERRUN_TRIM_CHAR
		elif control is Button:
			var button := control as Button
			text = button.text
			wraps = button.autowrap_mode != TextServer.AUTOWRAP_OFF
			trims = button.text_overrun_behavior == TextServer.OVERRUN_TRIM_ELLIPSIS
		else:
			continue
		if text.strip_edges().is_empty() or wraps or trims:
			continue
		_checked += 1
		var shown := control.atr(text)
		var font := control.get_theme_font(&"font")
		var font_size := control.get_theme_font_size(&"font_size")
		var needed := font.get_string_size(shown, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size).x
		var room := control.size.x
		if control is Button:
			var style := control.get_theme_stylebox(&"normal")
			room -= style.get_margin(SIDE_LEFT) + style.get_margin(SIDE_RIGHT)
			if control is OptionButton:
				room -= control.get_theme_icon(&"arrow").get_width() + control.get_theme_constant(&"arrow_margin")
			elif control is CheckBox:
				room -= control.get_theme_icon(&"checked").get_width() + control.get_theme_constant(&"h_separation")
		if needed > room + 1.0:
			_failures.append("%s: „%s“ braucht %d px, hat %d px (%s)" % [where, shown.left(60), needed, room, control.get_path()])


func _panel_of(window: Control) -> Control:
	# Der Startbildschirm deckt das ganze Bild; geprüft wird sein Panel.
	for child in window.get_children():
		if child is PanelContainer and (child as Control).visible:
			return child
	return window


## Auf Englisch: Stichproben aus den drei Bildschirmen.
func _check_english_samples() -> void:
	Translation_.use("en")
	var start: StartScreen = _main.call("start_screen")
	_main.call("open_start_screen")
	await _frames(1)
	var samples := {
		start.menu_button("NewGame").atr(start.menu_button("NewGame").text): "New game",
		start.menu_button("Lesson").atr(start.menu_button("Lesson").text): "History lesson",
		tr("Spielbare Staaten: {staaten}").format({"staaten": "PRU"}): "Playable states: PRU",
		tr("Die Taste {taste} gehört schon zu „{aktion}“.").format({"taste": "J", "aktion": tr("Journale")}): "The key J is already used for “Journals”.",
		Format.number(1234.5, 1): "1,234.5",
		Format.date(18, 3, 1848): "18 March 1848",
	}
	for got: String in samples:
		_checked += 1
		if got != samples[got]:
			_failures.append("Englisch: „%s“ statt „%s“" % [got, samples[got]])
	start.close()
	Translation_.use("de")
	if tr("Neues Spiel") != "Neues Spiel" or Format.number(1234.5, 1) != "1.234,5":
		_failures.append("Zurück auf Deutsch: „%s“, %s" % [tr("Neues Spiel"), Format.number(1234.5, 1)])


func _frames(count: int) -> void:
	for _i in count:
		await process_frame
