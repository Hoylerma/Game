class_name BarTooltips
extends RefCounted
## Texte der verschachtelten Tooltips der oberen Leiste (seit M13a5c, seit a5d aus main.gd ausgelagert):
## Kasse, Legitimität mit den Ursachen der Unruhe, Verwaltung, Vermächtnis und die zweite Stufe je Dimension.
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
