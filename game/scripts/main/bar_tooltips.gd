class_name BarTooltips
extends RefCounted
## Texte der verschachtelten Tooltips der oberen Leiste (seit M13a5c, seit a5d aus main.gd ausgelagert):
## Kasse, Legitimität mit den Ursachen der Unruhe, Verwaltung, Vermächtnis und die zweite Stufe je Dimension;
## seit M14a2 auch Unruhe, Bevölkerung (mit der Verwaltung) und Heer.
## Simulation und Spielerstaat liefert main.gd über Funktionen, damit die Texte immer aktuell sind.

var sim_source: Callable = Callable()
var player_source: Callable = Callable()
var location_name: Callable = Callable()


func sim() -> SimulationClient:
	return sim_source.call() as SimulationClient


func player() -> int:
	return int(player_source.call())


func cash() -> String:
	var state := sim().get_state(player())
	var taxes: Array = state.get("taxes_month", [])
	var names := [Glossary.link("kontribution", "Kontribution"), Glossary.link("akzise", "Akzise"), Glossary.link("zoll", "Zoll")]
	var lines := PackedStringArray(["[b]Kasse des Vormonats[/b]", "Einnahmen:"])
	var income := 0.0
	for k in mini(taxes.size(), names.size()):
		income += float(taxes[k])
		lines.append("  %s %s" % [names[k], Format.signed_money(float(taxes[k]))])
	var hinterland := float(state.get("hinterland_month", 0.0))
	if hinterland > 0.0:
		income += hinterland
		lines.append("  Hinterland %s" % Format.signed_money(hinterland))
	lines.append("Ausgaben:")
	var expenses := 0.0
	for item: Array in [["Hof", "court_month"], ["Verwaltung", "administration_month"], ["Heer", "army_month"], ["Zinsen", "interest_month"], ["Bauten", "building_month"], ["Gesandte", "envoy_cost_month"]]:
		var amount := float(state.get(str(item[1]), 0.0))
		if amount > 0.0:
			expenses += amount
			lines.append("  %s %s" % [item[0], Format.signed_money(-amount)])
	lines.append("%s %s" % [Glossary.link("saldo", "Saldo"), Format.signed_money(income - expenses)])
	lines.append("Schatz %s · %s %s" % [Format.money(float(state.get("treasury", 0.0))), Glossary.link("schulden", "Schulden"), Format.money(float(state.get("debt", 0.0)))])
	return "\n".join(lines)


func legitimacy() -> String:
	var events := sim().get_state_events(player())
	if events.is_empty():
		return "Ohne Ereignisse gibt es weder Zustimmung noch Unruhe."
	var lines := PackedStringArray(["[b]%s[/b] (Zustimmung der Gruppen im Mittel)" % Glossary.link("legitimitaet", "Legitimität")])
	for entry: Dictionary in events.get("approvals", []):
		lines.append("  %s %s" % [str(entry.get("name", "")), Format.signed_number(roundi(float(entry.get("value", 0.0))))])
	lines.append("%s %d, Ziel %d:" % [Glossary.link("unruhe", "Unruhe"), roundi(float(events.get("unrest", 0.0))), roundi(float(events.get("unrest_target", 0.0)))])
	for cause: Dictionary in events.get("unrest_causes", []):
		var value := float(cause.get("value", 0.0))
		if absf(value) >= 0.5:
			lines.append("  %s %s" % [str(cause.get("name", "")), Format.signed_number(roundi(value))])
	return "\n".join(lines)


func administration() -> String:
	var overview := sim().get_state_overview(player())
	var state := sim().get_state(player())
	return "[b]Verwaltung[/b]\n%s Orte in %s %s\n%s Einwohner\nHauptstadt: %s" % [
		Format.integer(int(overview.get("locations", 0))), Format.integer(int(overview.get("kreise", 0))),
		Glossary.link("kreis", "Kreisen"), Format.integer(int(state.get("population", 0))),
		location_name.call(int(state.get("capital_index", -1))),
	]


## Unruhe mit Ziel und Ursachen (seit M14a2).
func unrest() -> String:
	var events := sim().get_state_events(player())
	if events.is_empty():
		return "Ohne Ereignisse gibt es keine Unruhe."
	var lines := PackedStringArray([
		"[b]%s[/b] %d, Ziel %d" % [Glossary.link("unruhe", "Unruhe"), roundi(float(events.get("unrest", 0.0))), roundi(float(events.get("unrest_target", 0.0)))],
		"Ab 30 droht ein Aufstand; die Unruhe wandert jeden Monat ein Stück zu ihrem Ziel.",
	])
	for cause: Dictionary in events.get("unrest_causes", []):
		var value := float(cause.get("value", 0.0))
		if absf(value) >= 0.5:
			lines.append("  %s %s" % [str(cause.get("name", "")), Format.signed_number(roundi(value))])
	if not (events.get("revolt", {}) as Dictionary).is_empty():
		lines.append("[b]Aufstand im Land![/b]")
	return "\n".join(lines)


## Bevölkerung mit der Verwaltung (seit M14a2; die Verwaltung stand vorher als eigene Zahl in der Leiste).
## Seit der Rückmeldung 11/2026 mit dem Urteil „Wie geht es dem Volk?“ (Wellbeing).
func population() -> String:
	var society := sim().get_state_society(player())
	var unrest := float(sim().get_state_events(player()).get("unrest", 0.0))
	return "%s\n\n%s" % [Wellbeing.tooltip(society, unrest), administration()]


## Heer: Soldaten, Friedensstärke, Mobilmachung, Kosten (seit M14a2).
func army() -> String:
	var state := sim().get_state(player())
	var lines := PackedStringArray(["[b]Heer[/b]"])
	if bool(state.get("military", false)):
		lines.append("%s Mann unter Waffen (Friedensstärke %s)" % [
			Format.integer(int(state.get("soldiers_under_arms", 0))), Format.integer(int(state.get("peace_strength", 0)))
		])
		lines.append("Lage: %s%s" % [str(state.get("mobilization_text", "")), " · im Krieg" if bool(state.get("in_war", false)) else ""])
	else:
		lines.append("%s Mann (Promille der Einwohner)" % Format.integer(int(state.get("soldiers", 0))))
	lines.append("Kosten %s im Monat" % Format.money(float(state.get("army_month", 0.0))))
	lines.append("Mobilmachung und Krieg: Staatsübersicht und Bereich „Militär“.")
	return "\n".join(lines)


func legacy() -> String:
	var legacy := sim().get_legacy(player())
	var dimensions: Array = legacy.get("dimensions", [])
	if dimensions.is_empty():
		return "Dieses Lesezeichen kennt kein Vermächtnis."
	var lines := PackedStringArray(["[b]%s[/b] – Faktoren beim Überfahren einer Dimension" % Glossary.link("vermaechtnis", "Vermächtnis")])
	for dimension: Dictionary in dimensions:
		lines.append("[url=vermaechtnis:%s]%s[/url] %d – %s" % [
			str(dimension.get("id", "")), str(dimension.get("name", "")), roundi(float(dimension.get("value", 0.0))),
			str(dimension.get("sentence", "")),
		])
	return "\n".join(lines)


## Zweiter Tooltip einer Dimension: Grundwert und Faktoren.
func legacy_link(meta: String) -> String:
	if not meta.begins_with("vermaechtnis:"):
		return ""
	var id := meta.substr(13)
	for dimension: Dictionary in sim().get_legacy(player()).get("dimensions", []):
		if str(dimension.get("id", "")) == id:
			var lines := PackedStringArray([
				"[b]%s %d[/b]" % [str(dimension.get("name", "")), roundi(float(dimension.get("value", 0.0)))],
				Glossary.text(id), "Grundwert %+d" % roundi(float(dimension.get("base", 0.0))),
			])
			for factor: Dictionary in dimension.get("factors", []):
				var value := float(factor.get("value", 0.0))
				if bool(factor.get("active", false)) and absf(value) >= 0.5:
					lines.append("  %s %+d" % [str(factor.get("name", "")), roundi(value)])
			return "\n".join(lines)
	return ""
