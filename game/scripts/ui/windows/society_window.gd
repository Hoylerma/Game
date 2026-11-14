class_name SocietyWindow
extends AreaWindow
## Gesellschaft (seit M13a5b): die Bevölkerung des Staates über alle Orte – Altersaufbau, Lebensstandard,
## Alphabetisierung, Kulturen, Konfessionen, Berufe und die Bilanz des Vorjahres mit Wanderung und
## Auswanderung.

const LIMIT := 6


func _window_title() -> String:
	return "GESELLSCHAFT"


func _window_name() -> String:
	return "SocietyWindow"


func _fill() -> void:
	var society := _sim.get_state_society(_player)
	if society.is_empty():
		return
	var population := int(society.get("population", 0))
	_line("Einwohner", Format.integer(population))
	_line("Orte, davon Städte", "%s, %s" % [Format.integer(int(society.get("locations", 0))), Format.integer(int(society.get("cities", 0)))])
	_bar("Lebensstandard", float(society.get("living_standard", 0.0)) / 100.0, UiPalette.POSITIVE, Format.number(float(society.get("living_standard", 0.0)), 1), "0 bis 100, nach Einwohnern gemittelt")
	_bar("Alphabetisierung", float(society.get("literacy_percent", 0.0)) / 100.0, UiPalette.GOLD, Format.percent(float(society.get("literacy_percent", 0.0)), 1))
	_gap()
	_heading("ALTERSAUFBAU")
	var total := maxf(float(population), 1.0)
	for age: Array in [["children", "0–14 Jahre"], ["young", "15–29 Jahre"], ["working", "30–64 Jahre"], ["old", "65 Jahre und älter"]]:
		var count := float(society.get(age[0], 0))
		_bar(str(age[1]), count / total, UiPalette.BAR_NEUTRAL, Format.percent(count * 100.0 / total, 1))
	_gap()
	_heading("KULTUREN")
	for entry in sorted_shares(society.get("cultures", {}), LIMIT):
		_bar(str(entry[0]), float(entry[1]) / 100.0, UiPalette.ACCENT, Format.percent(float(entry[1]), 1))
	_gap()
	_heading("KONFESSIONEN")
	for entry in sorted_shares(society.get("confessions", {}), LIMIT):
		_bar(str(entry[0]), float(entry[1]) / 100.0, UiPalette.EDGE, Format.percent(float(entry[1]), 1))
	_gap()
	_heading("BERUFE")
	for row: Dictionary in society.get("professions", []):
		_bar(str(row.get("name", "")), float(row.get("percent", 0.0)) / 100.0, UiPalette.INK_FAINT, Format.integer(int(row.get("people", 0))))
	_gap()
	_heading("VORJAHR")
	var balance: Dictionary = society.get("balance_previous", {})
	for item: Array in [["births", "Geburten"], ["deaths", "Sterbefälle"], ["plague_deaths", "Seuchentote"], ["moved_in", "Zuzug"], ["moved_out", "Wegzug"], ["emigrated", "Auswanderung"]]:
		var value := int(balance.get(item[0], 0))
		_line(str(item[1]), "%s (%s ‰)" % [Format.integer(value), Format.number(value * 1000.0 / total, 1)])
