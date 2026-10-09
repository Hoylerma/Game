class_name LegendData
extends RefCounted
## Leitet Legenden aus den Kartenfarben der Simulation ab.
##
## Die Farben stammen immer aus get_map_colors, damit Legende und Karte
## übereinstimmen. Verläufe (Dichte, Verwüstung) werden aus den Orten
## geschätzt: Passt eine lineare Abbildung Wert → Farbe, zeigt die Legende die
## exakten Endfarben, sonst die tatsächlich vorkommenden Farben.

## Höchste Abweichung (je Farbkanal), bis zu der ein linearer Verlauf gilt.
const LINEAR_TOLERANCE := 0.02


## Staaten mit mindestens einem Ort, alphabetisch nach Kurzname.
static func political_entries(sim: SimulationClient) -> Array[Dictionary]:
	var entries: Array[Dictionary] = []
	for index in sim.get_state_count():
		var state := sim.get_state(index)
		var count := int(state.get("location_count", 0))
		if count <= 0:
			continue
		var detail := "%s – %s" % [
			str(state.get("name", "")), "1 Ort" if count == 1 else "%d Orte" % count
		]
		entries.append({
			"color": state.get("color", Color.GRAY),
			"text": str(state.get("short_name", state.get("tag", ""))),
			"detail": detail,
		})
	entries.sort_custom(
		func(a: Dictionary, b: Dictionary) -> bool:
			return str(a["text"]).naturalnocasecmp_to(str(b["text"])) < 0
	)
	return entries


## Kategorien eines Kartenmodus (Name aus dem Ortsfeld field) mit Anzahl der Orte.
static func category_entries(sim: SimulationClient, mode: int, field: String) -> Array[Dictionary]:
	var colors := sim.get_map_colors(mode)
	var by_name := {}
	var order: Array[String] = []
	for index in mini(colors.size(), sim.get_location_count()):
		var category := GermanFormat.humanize(str(sim.get_location(index).get(field, "")))
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
			"detail": "1 Ort" if count == 1 else "%d Orte" % count,
		})
	return entries


## Name → Farbe der Kategorien eines Kartenmodus (z. B. Konfessionen).
static func category_colors(sim: SimulationClient, mode: int, field: String) -> Dictionary:
	var result := {}
	for entry in category_entries(sim, mode, field):
		result[entry["text"]] = entry["color"]
	return result


## Verlauf für einen Kartenmodus mit Zahlenwerten im Ortsfeld field (0..scale_max).
static func gradient_for(
	sim: SimulationClient, mode: int, field: String, scale_max: float
) -> Gradient:
	var colors := sim.get_map_colors(mode)
	var values := PackedFloat32Array()
	var samples := PackedColorArray()
	for index in mini(colors.size(), sim.get_location_count()):
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
