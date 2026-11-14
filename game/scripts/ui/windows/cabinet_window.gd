class_name CabinetWindow
extends AreaWindow
## Kabinett und Hof (seit M13a5b): Herrscher und leitender Minister mit Werten, Eigenschaften und Amtszeit,
## die Neigungen des Staates mit seiner Staatspersönlichkeit und die früheren Amtsinhaber der Partie.

const VALUE_NAMES := {
	"staatskunst": "Staatskunst", "diplomatie": "Diplomatie", "kriegskunst": "Kriegskunst", "wirkung": "Wirkung",
}


func _window_title() -> String:
	return "KABINETT UND HOF"


func _window_name() -> String:
	return "CabinetWindow"


func _fill() -> void:
	var persons := _sim.get_state_persons(_player)
	if persons.is_empty():
		_paragraph("Ohne Personen im Amt (das Lesezeichen kennt keine Persönlichkeiten).", true)
		return
	for person in persons:
		_heading("%s: %s" % [str(person.get("title", "")).to_upper(), str(person.get("name", ""))])
		_line("Im Amt seit", str(person.get("since_text", "")))
		var values: Dictionary = person.get("values", {})
		for key: String in VALUE_NAMES:
			_bar(VALUE_NAMES[key], float(values.get(key, 0)) / 10.0, UiPalette.GOLD, "%d von 10" % int(values.get(key, 0)))
		var traits := PackedStringArray(person.get("traits", []))
		_line("Eigenschaften", ", ".join(traits) if not traits.is_empty() else "keine")
		_line("Anteil an den Neigungen", Format.percent(float(person.get("weight", 0.0)) * 100.0))
		var source := str(person.get("source", ""))
		if not source.is_empty():
			_paragraph("Quelle: %s" % source, true)
		_gap()
	var intentions := _sim.get_intentions(_player)
	if not intentions.is_empty():
		var personality := str(intentions.get("personality", ""))
		_heading("NEIGUNGEN DES STAATES" + (" (%s)" % personality if not personality.is_empty() else ""))
		var keys: Array = intentions.get("inclination_keys", [])
		var names: Array = intentions.get("inclination_names", [])
		var inclinations: Dictionary = intentions.get("inclinations", {})
		for k in keys.size():
			var value := float(inclinations.get(keys[k], 0.0))
			_signed_bar(
				str(names[k]), value, 2.0, Format.signed_number(value, 1),
				"−2 bis +2: Staatspersönlichkeit und Eigenschaften im Amt"
			)
		_gap()
	var history := _sim.get_office_history(_player)
	_heading("FRÜHERE AMTSINHABER")
	if history.is_empty():
		_paragraph("In dieser Partie hat noch niemand das Amt verlassen.", true)
	for term in history:
		_line(
			"%s: %s" % [str(term.get("title", "")), str(term.get("name", ""))],
			"%s – %s" % [str(term.get("from_text", "")), str(term.get("to_text", ""))]
		)
