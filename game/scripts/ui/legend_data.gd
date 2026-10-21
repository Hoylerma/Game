class_name LegendData
extends RefCounted
## Leitet Legenden aus den Kartenfarben der Simulation ab.
##
## Die Farben stammen immer aus get_map_colors, damit Legende und Karte
## übereinstimmen. Verläufe (Dichte, Verwüstung) werden aus den Orten
## geschätzt: Passt eine lineare Abbildung Wert → Farbe, zeigt die Legende die
## exakten Endfarben, sonst die tatsächlich vorkommenden Farben.
## Orte ohne Daten (Alpha 0 in den datenabhängigen Modi) zählen nicht mit; für sie
## gibt es einen eigenen Eintrag mit Schraffur (no_data_entry).

## Höchste Abweichung (je Farbkanal), bis zu der ein linearer Verlauf gilt.
const LINEAR_TOLERANCE := 0.02
## Höchstzahl der Zeilen in der politischen Legende (seit M7).
const MAX_POLITICAL_ENTRIES := 16


## Staaten mit mindestens einem Ort, alphabetisch nach Kurzname; bei mehr als max_entries
## Staaten nur die größten und der Spielerstaat (player), die übrigen in einer Zeile
## „N weitere Staaten“.
static func political_entries(
	sim: SimulationClient, max_entries: int = MAX_POLITICAL_ENTRIES, player: int = -1
) -> Array[Dictionary]:
	var entries: Array[Dictionary] = []
	for index in sim.get_state_count():
		var state := sim.get_state(index)
		var count := int(state.get("location_count", 0))
		if count <= 0:
			continue
		var detail := "%s – %s" % [str(state.get("name", "")), count_text(count)]
		entries.append({
			"color": state.get("color", Color.GRAY),
			"text": str(state.get("short_name", state.get("tag", ""))),
			"detail": detail,
			"count": count,
			"player": index == player,
		})
	# Viele Staaten (1848: 38): nur die größten und der Spielerstaat, der Rest als eine Zeile
	# (seit M7).
	var rest: Array[Dictionary] = []
	if entries.size() > max_entries:
		entries.sort_custom(
			func(a: Dictionary, b: Dictionary) -> bool:
				if a["player"] != b["player"]:
					return a["player"]
				return a["count"] > b["count"]
		)
		rest = entries.slice(max_entries - 1)
		entries = entries.slice(0, max_entries - 1)
	entries.sort_custom(
		func(a: Dictionary, b: Dictionary) -> bool:
			return str(a["text"]).naturalnocasecmp_to(str(b["text"])) < 0
	)
	if not rest.is_empty():
		var names := PackedStringArray()
		for entry in rest:
			names.append(str(entry["text"]))
		entries.append({
			"color": UiPalette.INK_FAINT,
			"text": "%d weitere Staaten" % rest.size(),
			"detail": ", ".join(names),
		})
	return entries


## Eintrag „Keine Daten“ (Schraffur) für count Orte ohne Daten.
static func no_data_entry(count: int) -> Dictionary:
	return {
		"color": UiPalette.PAPER_DEEP,
		"hatch": true,
		"text": "Keine Daten",
		"detail": "%s ohne historische Daten zum Startdatum" % count_text(count),
	}


## "1 Ort" bzw. "1.234 Orte".
static func count_text(count: int) -> String:
	return "1 Ort" if count == 1 else "%s Orte" % GermanFormat.integer(count)


## Kategorien eines Kartenmodus (Name aus dem Ortsfeld field) mit Anzahl der Orte;
## Orte ohne Daten (Alpha 0) am Ende als ein Eintrag mit Schraffur.
## catalog liefert Kategorien ohne get_location, wenn field "terrain" ist.
static func category_entries(
	sim: SimulationClient, mode: int, field: String, catalog: LocationCatalog = null
) -> Array[Dictionary]:
	var colors := sim.get_map_colors(mode)
	var by_name := {}
	var order: Array[String] = []
	var without_data := 0
	var use_catalog := catalog != null and field == "terrain" and catalog.size() == colors.size()
	for index in mini(colors.size(), sim.get_location_count()):
		if colors[index].a <= 0.0:
			without_data += 1
			continue
		var raw := ""
		if use_catalog:
			raw = catalog.terrains[index]
		else:
			raw = str(sim.get_location(index).get(field, ""))
		var category := GermanFormat.humanize(raw)
		if not by_name.has(category):
			by_name[category] = {"color": colors[index], "count": 0}
			order.append(category)
		by_name[category]["count"] = int(by_name[category]["count"]) + 1
	order.sort_custom(
		func(a: String, b: String) -> bool:
			return int(by_name[a]["count"]) > int(by_name[b]["count"])
	)
	var entries: Array[Dictionary] = []
	for category in order:
		var count := int(by_name[category]["count"])
		entries.append({
			"color": by_name[category]["color"],
			"text": category,
			"detail": count_text(count),
		})
	if without_data > 0:
		entries.append(no_data_entry(without_data))
	return entries


## Name → Farbe der Kategorien eines Kartenmodus (z. B. Konfessionen).
static func category_colors(sim: SimulationClient, mode: int, field: String) -> Dictionary:
	var result := {}
	for entry in category_entries(sim, mode, field):
		if not bool(entry.get("hatch", false)):
			result[entry["text"]] = entry["color"]
	return result


## Farben der Kreise (Palette der Brücke) in der Reihenfolge der Kreisindizes,
## ohne Wiederholungen; Orte ohne Kreis (Grau) zuletzt.
static func kreis_palette(sim: SimulationClient, catalog: LocationCatalog) -> PackedColorArray:
	var colors := sim.get_map_colors(MapModes.MAP_KREISE)
	var first_of_kreis := {}
	var without_kreis := Color(0, 0, 0, 0)
	for index in mini(colors.size(), catalog.kreis_indices.size()):
		var kreis := catalog.kreis_indices[index]
		if kreis < 0:
			without_kreis = colors[index]
		elif not first_of_kreis.has(kreis):
			first_of_kreis[kreis] = colors[index]
	var kreise: Array = first_of_kreis.keys()
	kreise.sort()
	var palette := PackedColorArray()
	for kreis: int in kreise:
		var color: Color = first_of_kreis[kreis]
		if not palette.has(color):
			palette.append(color)
	if without_kreis.a > 0.0 and not palette.has(without_kreis):
		palette.append(without_kreis)
	return palette


## Märkte mit Orten mit Daten (seit M5), Farbe aus dem Marktmodus der Brücke, in der
## Reihenfolge der Marktindizes; market_indices ist get_location_market_indices().
static func market_entries(
	sim: SimulationClient, market_indices: PackedInt32Array
) -> Array[Dictionary]:
	var colors := sim.get_map_colors(MapModes.MAP_MAERKTE)
	var color_of := {}
	for index in mini(colors.size(), market_indices.size()):
		var market := market_indices[index]
		if market >= 0 and colors[index].a > 0.0 and not color_of.has(market):
			color_of[market] = colors[index]
	var entries: Array[Dictionary] = []
	for market in sim.get_market_count():
		var info := sim.get_market(market)
		if not bool(info.get("active", false)) or not color_of.has(market):
			continue
		# Kurzname ohne Klammerzusatz („Kursachsen (Leipzig, Dresden)“ → „Kursachsen“), damit die
		# Legende schmal bleibt; der volle Name steht im Tooltip.
		var full_name := str(info.get("name", ""))
		var short_name := full_name.get_slice(" (", 0)
		var detail := "%s – %s mit Daten" % [
			full_name, count_text(int(info.get("location_count", 0)))
		]
		if bool(info.get("is_port", false)):
			detail += ", Hafen"
		entries.append({"color": color_of[market], "text": short_name, "detail": detail})
	return entries


## Oberste Verwaltungseinheiten des Staates state_index mit Orten (seit M7), Farbe aus dem Modus
## „Verwaltung“ (die Karte färbt nach der obersten Einheit und zeichnet die Grenzen der untersten);
## admin_indices ist get_location_admin_indices(). Leer, wenn der Staat keine Gliederung hat.
static func admin_entries(
	sim: SimulationClient, admin_indices: PackedInt32Array, state_index: int
) -> Array[Dictionary]:
	var colors := sim.get_map_colors(MapModes.MAP_VERWALTUNG)
	var owners := sim.get_location_owner_indices()
	var top_of := {}
	var color_of := {}
	var count_of := {}
	var lowest_of := {}
	for index in mini(colors.size(), mini(admin_indices.size(), owners.size())):
		var unit := admin_indices[index]
		if unit < 0 or owners[index] != state_index:
			continue
		if not top_of.has(unit):
			var top := unit
			for step in sim.get_admin_unit_count():
				var parent := int(sim.get_admin_unit(top).get("parent_index", -1))
				if parent < 0:
					break
				top = parent
			top_of[unit] = top
		var top_unit: int = top_of[unit]
		if not color_of.has(top_unit):
			color_of[top_unit] = colors[index]
			lowest_of[top_unit] = {}
		count_of[top_unit] = int(count_of.get(top_unit, 0)) + 1
		(lowest_of[top_unit] as Dictionary)[unit] = true
	var tops: Array = color_of.keys()
	tops.sort()
	var entries: Array[Dictionary] = []
	for top_unit: int in tops:
		var info := sim.get_admin_unit(top_unit)
		var parts := (lowest_of[top_unit] as Dictionary).size()
		var detail := "%s (%s) – %s" % [
			str(info.get("name", "")),
			GermanFormat.humanize(str(info.get("kind", ""))),
			count_text(int(count_of[top_unit])),
		]
		if parts > 1:
			detail += ", %d Untereinheiten" % parts
		entries.append(
			{"color": color_of[top_unit], "text": str(info.get("name", "")), "detail": detail}
		)
	return entries


## Legende „Eisenbahnen“ (seit M7): Bahnhöfe (Endpunkte eröffneter Abschnitte), Orte in einem
## Markt mit Bahnfracht zu einem Nachbarmarkt, Orte ohne Bahn; gezählt nach get_rail_access, also
## nach denselben Stufen wie die Farben. Leer, wenn das Szenario keine Strecken hat.
static func railway_entries(sim: SimulationClient) -> Array[Dictionary]:
	var railways := sim.get_railways()
	if railways.is_empty():
		return []
	var date := sim.get_date()
	var open_sections := 0
	var planned_sections := 0
	var first_year := 0
	for line: Variant in railways:
		for section: Variant in (line as Dictionary).get("sections", []):
			var entry := section as Dictionary
			var year := int((entry.get("opened", {}) as Dictionary).get("year", 0))
			if first_year == 0 or year < first_year:
				first_year = year
			if bool(entry.get("open", false)):
				open_sections += 1
			elif RailwayLayer.is_shown(entry, date):
				planned_sections += 1
	var access := sim.get_rail_access()
	var colors := sim.get_map_colors(MapModes.MAP_EISENBAHNEN)
	var counts := [0, 0, 0]
	var samples: Array[Color] = [Color.TRANSPARENT, Color.TRANSPARENT, Color.TRANSPARENT]
	var without_data := 0
	for index in colors.size():
		if colors[index].a <= 0.0:
			without_data += 1
			continue
		var kind := clampi(int(access[index]) if index < access.size() else 0, 0, 2)
		counts[kind] += 1
		samples[kind] = colors[index]
	var texts := ["ohne Bahn", "Markt mit Bahnfracht", "Bahnhof"]
	var station_detail := "%d Abschnitte eröffnet" % open_sections
	if planned_sections > 0:
		station_detail += ", %d in den nächsten %d Jahren (gestrichelt)" % [
			planned_sections, RailwayLayer.PLANNED_YEARS
		]
	var details := [
		"" if open_sections > 0 else "noch keine Eisenbahn (erste Eröffnung %d)" % first_year,
		"handelt mit Nachbarmärkten am selben Netz zur Bahnfracht",
		station_detail,
	]
	var entries: Array[Dictionary] = []
	for kind in [2, 1, 0]:
		if counts[kind] == 0:
			continue
		var detail := count_text(counts[kind])
		if not str(details[kind]).is_empty():
			detail += " – " + str(details[kind])
		entries.append({"color": samples[kind], "text": texts[kind], "detail": detail})
	if without_data > 0:
		entries.append(no_data_entry(without_data))
	return entries


## Farben des Kartenmodus „Wahlen“ wie in der Brücke (seit M8): außerhalb des Wahlgebiets, Boykott.
const ELECTION_OUTSIDE := Color(0.90, 0.88, 0.82)
const ELECTION_BOYCOTT := Color(0.76, 0.79, 0.84)


## Titel der Legende im Kartenmodus „Wahlen“ mit dem Jahr der gezeigten Wahl (seit M8).
static func election_title(sim: SimulationClient) -> String:
	var parliament := sim.get_shown_parliament()
	var parliaments := sim.get_parliaments()
	if parliament < 0 or parliament >= parliaments.size():
		return MapModes.name_of(MapModes.MAP_WAHLEN)
	var date: Dictionary = parliaments[parliament].get("election_date", {})
	return "Wahlen %d" % int(date.get("year", 0))


## Einträge im Kartenmodus „Wahlen“ (seit M8): vor der Wahl die Wahlkreise, danach die Sitze je
## Strömung und der Boykott; dazu die Orte außerhalb des Wahlgebiets. Leer ohne Parlament.
static func election_entries(sim: SimulationClient, revealing: bool = false) -> Array[Dictionary]:
	var entries: Array[Dictionary] = []
	var parliament_index := sim.get_shown_parliament()
	var parliaments := sim.get_parliaments()
	if parliament_index < 0 or parliament_index >= parliaments.size():
		return entries
	var parliament := parliaments[parliament_index]
	var colors := sim.get_map_colors(MapModes.MAP_WAHLEN)
	var districts := sim.get_location_electoral_district_indices(parliament_index)
	var outside := 0
	var sample := Color.TRANSPARENT
	for i in mini(colors.size(), districts.size()):
		if colors[i].a <= 0.0:
			continue
		if districts[i] < 0:
			outside += 1
		elif sample.a <= 0.0:
			sample = colors[i]
	var election_index := int(parliament.get("election_index", -1))
	var parliament_name := str(parliament.get("name", ""))
	if election_index < 0:
		entries.append({
			"color": sample if sample.a > 0.0 else UiPalette.PAPER_DEEP,
			"text": "Wahlkreise",
			"detail": "%s: %s Wahlkreise, %s Sitze; Wahl am %s" % [
				parliament_name,
				GermanFormat.integer(int(parliament.get("district_count", 0))),
				GermanFormat.integer(int(parliament.get("seats", 0))),
				str(parliament.get("election_date_text", "")),
			],
		})
	elif revealing:
		# Wahlabend läuft (Review M8): keine Sitze vor der letzten Meldung.
		entries.append({
			"color": sample if sample.a > 0.0 else UiPalette.PAPER_DEEP,
			"text": "gemeldet",
			"detail": "Farbe der siegreichen Strömung; die Sitze folgen am Ende des Wahlabends",
		})
		entries.append({
			"color": UiPalette.PAPER_DEEP,
			"hatch": true,
			"text": "noch nicht gemeldet",
			"detail": parliament_name,
		})
	else:
		var election := sim.get_election(election_index)
		var currents := sim.get_currents()
		var seats: PackedInt32Array = election.get("seats_by_current", PackedInt32Array())
		for c in mini(currents.size(), seats.size()):
			if seats[c] <= 0:
				continue
			entries.append({
				"color": currents[c].get("color", UiPalette.BAR_NEUTRAL),
				"text": str(currents[c].get("short_name", currents[c].get("name", ""))),
				"detail": "%s: %s Sitze (%s); hell bei knappem Sieg" % [
					str(currents[c].get("name", "")), GermanFormat.integer(seats[c]), parliament_name
				],
			})
		var empty := int(election.get("boycott_seats", 0))
		if empty > 0:
			entries.append({
				"color": ELECTION_BOYCOTT,
				"text": "Boykott",
				"detail": "%s Sitze leer" % GermanFormat.integer(empty),
			})
	if outside > 0:
		entries.append({
			"color": ELECTION_OUTSIDE,
			"text": "nicht im Wahlgebiet",
			"detail": count_text(outside),
		})
	return entries


## Verlauf für einen Kartenmodus mit Zahlenwerten im Ortsfeld field (0..scale_max).
static func gradient_for(
	sim: SimulationClient, mode: int, field: String, scale_max: float
) -> Gradient:
	var colors := sim.get_map_colors(mode)
	var values := PackedFloat32Array()
	var samples := PackedColorArray()
	for index in mini(colors.size(), sim.get_location_count()):
		if colors[index].a <= 0.0:
			continue
		values.append(float(sim.get_location(index).get(field, 0.0)))
		samples.append(colors[index])
	return gradient_from_samples(values, samples, scale_max)


## Baut einen Verlauf 0..scale_max aus Wert-Farbe-Paaren.
static func gradient_from_samples(
	values: PackedFloat32Array, samples: PackedColorArray, scale_max: float
) -> Gradient:
	var gradient := Gradient.new()
	if values.is_empty() or scale_max <= 0.0:
		gradient.offsets = PackedFloat32Array([0.0])
		gradient.colors = PackedColorArray([UiPalette.PAPER_DEEP])
		return gradient
	var linear := _fit_linear(values, samples, scale_max)
	if not linear.is_empty():
		gradient.offsets = PackedFloat32Array([0.0, 1.0])
		gradient.colors = PackedColorArray([linear[0], linear[1]])
		return gradient
	# Kein linearer Zusammenhang: vorkommende Farben nach Wert sortiert.
	var order := range(values.size())
	order.sort_custom(func(a: int, b: int) -> bool: return values[a] < values[b])
	var offsets := PackedFloat32Array()
	var stops := PackedColorArray()
	for i: int in order:
		var offset := clampf(values[i] / scale_max, 0.0, 1.0)
		if not offsets.is_empty() and offset - offsets[offsets.size() - 1] < 0.01:
			continue
		offsets.append(offset)
		stops.append(samples[i])
	gradient.offsets = offsets
	gradient.colors = stops
	return gradient


## Lineare Ausgleichsrechnung je Kanal über die nicht gekappten Werte.
## Liefert [Farbe bei 0, Farbe bei scale_max] oder ein leeres Array.
static func _fit_linear(
	values: PackedFloat32Array, samples: PackedColorArray, scale_max: float
) -> Array[Color]:
	var xs := PackedFloat32Array()
	var points := PackedColorArray()
	for i in values.size():
		if values[i] >= 0.0 and values[i] < scale_max:
			xs.append(values[i] / scale_max)
			points.append(samples[i])
	if xs.size() < 2:
		return []
	var mean_x := 0.0
	for x in xs:
		mean_x += x
	mean_x /= xs.size()
	var variance := 0.0
	for x in xs:
		variance += (x - mean_x) * (x - mean_x)
	if variance < 0.0001:
		return []
	var start := Color(0, 0, 0, 1)
	var finish := Color(0, 0, 0, 1)
	for channel in 3:
		var mean_y := 0.0
		for c in points:
			mean_y += c[channel]
		mean_y /= points.size()
		var covariance := 0.0
		for i in xs.size():
			covariance += (xs[i] - mean_x) * (points[i][channel] - mean_y)
		var slope := covariance / variance
		var intercept := mean_y - slope * mean_x
		for i in xs.size():
			if absf(intercept + slope * xs[i] - points[i][channel]) > LINEAR_TOLERANCE:
				return []
		start[channel] = clampf(intercept, 0.0, 1.0)
		finish[channel] = clampf(intercept + slope, 0.0, 1.0)
	return [start, finish]
