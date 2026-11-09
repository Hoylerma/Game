class_name GroupsWindow
extends AreaWindow
## Interessengruppen (seit M13a5b): Anteil an den Einwohnern, Einfluss (Anteil am Einkommen), Zustimmung zur
## Regierung (−10 bis +10) und Forderungen: Gesetzesoptionen, die eine Gruppe wünscht und die nicht gelten,
## und geltende, die sie ablehnt.


func _window_title() -> String:
	return "INTERESSENGRUPPEN"


func _window_name() -> String:
	return "GroupsWindow"


func _fill() -> void:
	var politics := _sim.get_state_politics(_player)
	var groups: Array = politics.get("interest_groups", [])
	if groups.is_empty():
		_paragraph("Das Lesezeichen kennt keine Interessengruppen.", true)
		return
	var approval := {}
	for entry: Dictionary in _sim.get_state_events(_player).get("approvals", []):
		approval[str(entry.get("name", ""))] = float(entry.get("value", 0.0))
	# Forderungen je Gruppe aus den Wünschen an die Gesetze.
	var demands := {}
	for law in _sim.get_state_laws(_player):
		for option: Dictionary in law.get("options", []):
			for wish: Dictionary in option.get("wishes", []):
				var value := float(wish.get("value", 0.0))
				var current := bool(option.get("current", false))
				var text := ""
				if value > 0.0 and not current:
					text = "fordert %s (%s)" % [str(option.get("name", "")), GermanFormat.signed_number(value)]
				elif value < 0.0 and current:
					text = "lehnt %s ab (%s)" % [str(option.get("name", "")), GermanFormat.signed_number(value)]
				if not text.is_empty():
					var name := str(wish.get("group", ""))
					if not demands.has(name):
						demands[name] = PackedStringArray()
					(demands[name] as PackedStringArray).append(text)
	for entry: Variant in groups:
		var group := entry as Dictionary
		var name := str(group.get("name", ""))
		var color: Color = group.get("color", UiPalette.BAR_NEUTRAL)
		_heading(name.to_upper())
		_bar("Anteil", float(group.get("share_percent", 0.0)) / 100.0, color, GermanFormat.percent(float(group.get("share_percent", 0.0)), 1), "Mitglieder unter den Einwohnern ab 15 Jahren")
		_bar("Einfluss", float(group.get("influence_percent", 0.0)) / 100.0, color, GermanFormat.percent(float(group.get("influence_percent", 0.0)), 1), "Anteil der Mitglieder am Einkommen")
		if approval.has(name):
			var value := float(approval[name])
			_signed_bar("Zustimmung", value, 10.0, GermanFormat.signed_number(roundi(value)), "Zustimmung zur Regierung, −10 bis +10")
		for text: String in demands.get(name, PackedStringArray()):
			_paragraph(text, true)
