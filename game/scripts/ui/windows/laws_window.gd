class_name LawsWindow
extends AreaWindow
## Gesetze (seit M13a5b): Verfassung und Wahlrecht des Staates, die Gesetzesgruppen mit allen Optionen, der
## geltenden markiert, ihre Wirkung auf die Unruhe und die Wünsche der Interessengruppen; dazu Unruhe mit
## Zielwert. Gesetze ändern Ereignisse (ereignisse.md 4.1); eine Gesetzgebung durch den Spieler gibt es noch
## nicht.


func _window_title() -> String:
	return "GESETZE"


func _window_name() -> String:
	return "LawsWindow"


func _fill() -> void:
	var politics := _sim.get_state_politics(_player)
	if not politics.is_empty():
		_heading("VERFASSUNG UND WAHLRECHT")
		_line("Verfassung", str(politics.get("constitution", "–")))
		_line("Wahlrecht des Landtags", str(politics.get("franchise", "–")))
		if bool(politics.get("elected", false)):
			_line(
				"Wahlsystem",
				("Stichwahl" if bool(politics.get("runoff", false)) else "relative Mehrheit")
				+ (", über Wahlmänner" if bool(politics.get("indirect", false)) else "")
			)
		_gap()
	var events := _sim.get_state_events(_player)
	if not events.is_empty():
		_line(
			"Unruhe",
			"%d (Ziel %d)" % [roundi(float(events.get("unrest", 0.0))), roundi(float(events.get("unrest_target", 0.0)))],
			"Unruhe 0 bis 100; sie nähert sich jeden Monat dem Zielwert aus Not, Teuerung, Zensur, Verfassung und Zustimmung"
		)
		_gap()
	var laws := _sim.get_state_laws(_player)
	if laws.is_empty():
		_paragraph("Das Lesezeichen kennt keine Gesetzesgruppen.", true)
		return
	for group in laws:
		_heading(str(group.get("name", "")).to_upper())
		for option: Dictionary in group.get("options", []):
			var current := bool(option.get("current", false))
			var unrest := float(option.get("unrest", 0.0))
			_line(
				("● " if current else "○ ") + str(option.get("name", "")) + (" (in Kraft)" if current else ""),
				"Unruhe %s" % GermanFormat.signed_number(unrest) if unrest != 0.0 else "",
				"Beitrag zum Zielwert der Unruhe",
				UiPalette.NEGATIVE if unrest > 0.0 else Color.TRANSPARENT
			)
			var pro := PackedStringArray()
			var contra := PackedStringArray()
			for wish: Dictionary in option.get("wishes", []):
				var value := float(wish.get("value", 0.0))
				var text := "%s %s" % [str(wish.get("group", "")), GermanFormat.signed_number(value)]
				if value > 0.0:
					pro.append(text)
				elif value < 0.0:
					contra.append(text)
			if not pro.is_empty():
				_paragraph("    dafür: " + ", ".join(pro), true)
			if not contra.is_empty():
				_paragraph("    dagegen: " + ", ".join(contra), true)
	_gap()
	_paragraph("Gesetze ändern sich durch Ereignisse und ihre Optionen.", true)
