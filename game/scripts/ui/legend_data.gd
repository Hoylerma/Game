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
## Staaten nur die größten, die übrigen in einer Zeile „N weitere Staaten“.
static func political_entries(
	sim: SimulationClient, max_entries: int = MAX_POLITICAL_ENTRIES
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
		})
	# Viele Staaten (1848: 38): nur die größten, der Rest als eine Zeile (seit M7).
	var rest: Array[Dictionary] = []
	if entries.size() > max_entries:
		entries.sort_custom(func(a: Dictionary, b: Dictionary) -> bool: return a["count"] > b["count"])
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


## Verwaltungseinheiten des Staates state_index mit Orten (seit M7), Farbe aus dem Modus
## „Verwaltung“; admin_indices ist get_location_admin_indices(). Leer, wenn der Staat keine
## Gliederung hat.
static func admin_entries(
	sim: SimulationClient, admin_indices: PackedInt32Array, state_index: int
) -> Array[Dictionary]:
	var colors := sim.get_map_colors(MapModes.MAP_VERWALTUNG)
	var owners := sim.get_location_owner_indices()
	var color_of := {}
	var count_of := {}
	for index in mini(colors.size(), mini(admin_indices.size(), owners.size())):
		var unit := admin_indices[index]
		if unit < 0 or owners[index] != state_index:
			continue
		if not color_of.has(unit):
			color_of[unit] = colors[index]
		count_of[unit] = int(count_of.get(unit, 0)) + 1
	var units: Array = color_of.keys()
	units.sort()
	var entries: Array[Dictionary] = []
	for unit: int in units:
		var info := sim.get_admin_unit(unit)
		var detail := "%s (%s) – %s" % [
			str(info.get("name", "")),
			GermanFormat.humanize(str(info.get("kind", ""))),
			count_text(int(count_of[unit])),
		]
		entries.append({"color": color_of[unit], "text": str(info.get("name", "")), "detail": detail})
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
