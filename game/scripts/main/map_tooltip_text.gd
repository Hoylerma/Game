class_name MapTooltipText
extends RefCounted
## Zweite Zeile des Kartentooltips je Kartenmodus (seit M13a5d aus main.gd ausgelagert). main.gd setzt die
## Werte, die es zwischenspeichert, und fragt detail() für den Ort unter der Maus.

var sim: SimulationClient = null
var player := -1
var mode := 0
var currency := "Reichstaler"
## Ort unter der Maus (Simulationsindex), Kontrolle je Ort und die Heere (Kartenmodus „Krieg“).
var index := -1
var control := PackedInt32Array()
var armies: Array[Dictionary] = []
## short_name(staat) → Kurzname; unreported(wahlkreis) → true, solange der Wahlabend ihn noch nicht meldet.
var short_name: Callable = Callable()
var unreported: Callable = Callable()


## Zweite Zeile des Tooltips: Besitzer und der Wert des Kartenmodus; für Orte ohne
## Daten „Keine Daten“ mit Kreis und Fläche (bzw. Gelände).
func detail(location: Dictionary) -> String:
	var kreis_name := str(location.get("kreis_name", ""))
	var kreis_text := "Kreis %s" % kreis_name if not kreis_name.is_empty() else "ohne Kreis"
	var terrain := Format.humanize(str(location.get("terrain", "")))
	var first := str(location.get("owner_name", ""))
	var second := kreis_text
	if not bool(location.get("has_data", true)):
		first = "Keine Daten"
		second = Format.area(float(location.get("area_km2", 0.0)))
	match mode:
		MapModes.MAP_KREISE:
			return kreis_text
		MapModes.MAP_TERRAIN:
			second = terrain
		MapModes.MAP_DENSITY when bool(location.get("has_data", true)):
			second = Format.density(float(location.get("density", 0.0)))
		MapModes.MAP_CONFESSION when bool(location.get("has_data", true)):
			second = Format.humanize(str(location.get("majority_confession", "")))
		MapModes.MAP_DEVASTATION when bool(location.get("has_data", true)):
			second = "%s verwüstet" % Format.percent(
				int(location.get("devastation_percent", 0))
			)
		MapModes.MAP_WIRTSCHAFT when bool(location.get("has_data", true)):
			second = "%s %s je Einwohner und Jahr" % [
				Format.number(float(location.get("production_per_capita", 0.0)), 1), currency
			]
		MapModes.MAP_MAERKTE:
			var market := str(location.get("market_name", ""))
			second = "Markt %s" % market if not market.is_empty() else "ohne Markt"
		MapModes.MAP_VERWALTUNG when bool(location.get("has_data", true)):
			var chain := PackedStringArray()
			for unit: Variant in location.get("verwaltung", []):
				if unit is Dictionary:
					chain.insert(0, str((unit as Dictionary).get("name", "")))
			second = " · ".join(chain) if not chain.is_empty() else "ohne Gliederung"
		MapModes.MAP_EISENBAHNEN when bool(location.get("has_data", true)):
			var lines: PackedStringArray = location.get("eisenbahnen", PackedStringArray())
			second = "Bahnhof: %s" % ", ".join(lines) if not lines.is_empty() else "ohne Bahnhof"
		MapModes.MAP_WAHLEN when bool(location.get("has_data", true)):
			second = election(location)
		MapModes.MAP_ZOLLGEBIETE when bool(location.get("has_data", true)):
			second = "Zollgebiet %s, Zoll %s" % [
				str(location.get("customs_zone_name", "–")),
				Format.percent(float(location.get("customs_tariff_percent", 0.0)))
			]
		MapModes.MAP_BEZIEHUNGEN when bool(location.get("has_data", true)):
			second = relation(int(location.get("owner_index", -1)))
		MapModes.MAP_KRIEG when bool(location.get("has_data", true)):
			second = war(index, int(location.get("owner_index", -1)))
		MapModes.MAP_UNRUHE when bool(location.get("has_data", true)):
			second = unrest(int(location.get("owner_index", -1)))
		_ when not bool(location.get("has_data", true)):
			second = "%s · %s" % [kreis_text, second]
	return "%s · %s" % [first, second]


## Wer den Ort hält und welche Heere dort stehen, im Kartenmodus „Krieg“ (seit M10).
func war(sim_index: int, owner: int) -> String:
	var parts := PackedStringArray()
	var holder := control[sim_index] if sim_index >= 0 and sim_index < control.size() else owner
	if holder >= 0 and holder != owner:
		parts.append("besetzt von %s" % short_name.call(holder))
	else:
		parts.append("gehalten")
	var soldiers := 0
	var count := 0
	for army in armies:
		if int(army.get("location_index", -1)) == sim_index:
			soldiers += int(army.get("soldiers", 0))
			count += 1
	if count > 0:
		parts.append("%s Heer%s mit %s Soldaten" % [
			Format.integer(count), "" if count == 1 else "e", Format.integer(soldiers)
		])
	return ", ".join(parts)


## Beziehung des Besitzers zum Spielerstaat im Tooltip (seit M9).
## Unruhe des Besitzers mit Zielwert und laufendem Aufstand, im Kartenmodus „Unruhe“ (seit M11).
func unrest(owner: int) -> String:
	var events := sim.get_state_events(owner)
	if events.is_empty():
		return "keine Unruhe (vor 1848)"
	var text := "Unruhe %d (Ziel %d)" % [
		roundi(float(events.get("unrest", 0.0))), roundi(float(events.get("unrest_target", 0.0)))
	]
	var revolt: Dictionary = events.get("revolt", {})
	if not revolt.is_empty():
		text += ", %s seit %s" % [str(revolt.get("kind_text", "Aufstand")), str(revolt.get("since_text", ""))]
	return text


func relation(owner: int) -> String:
	if owner == player:
		return "Spielerstaat"
	for entry in sim.get_relations(player):
		if int(entry.get("index", -1)) == owner:
			var text := "Beziehung %s (Ziel %s)" % [
				Format.signed_number(roundi(float(entry.get("relation", 0.0)))),
				Format.signed_number(roundi(float(entry.get("target", 0.0)))),
			]
			if bool(entry.get("allied", false)):
				text += ", Bündnis"
			if bool(entry.get("rival", false)):
				text += ", Gegner"
			return text
	return "keine Beziehung"


## Wahlkreis und Ergebnis im Tooltip (seit M8); während des Wahlabends „noch offen“.
func election(location: Dictionary) -> String:
	var election: Dictionary = location.get("wahl", {})
	if election.is_empty():
		return "außerhalb des Wahlgebiets"
	var text := "Wahlkreis %s" % str(election.get("district_name", ""))
	var district := int(election.get("district_index", -1))
	if unreported.call(district):
		return text + " · noch nicht gemeldet"
	if not bool(election.get("held", false)):
		return text + " · Wahl am %s" % str(election.get("election_date_text", ""))
	if bool(election.get("boycott", false)):
		return text + " · Boykott"
	var winner := str(election.get("winner_short_name", election.get("winner_name", "")))
	var shares: Dictionary = election.get("shares", {})
	text += " · %s %s" % [winner, Format.percent(roundi(float(shares.get(winner, 0.0))))]
	if bool(election.get("runoff", false)):
		text += " im ersten Wahlgang, Sieg in der Stichwahl"
	return text
