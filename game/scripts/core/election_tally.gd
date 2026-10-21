class_name ElectionTally
extends RefCounted
## Rechnungen des Wahlabends (seit M8, docs/technik/politik.md Abschnitt 8): Meldereihenfolge,
## gemeldete Sitze, Hochrechnung mit Fehlerband und die Sitzordnung im Halbrund. Arbeitet auf dem
## Wörterbuch von SimulationClient.get_election und kommt ohne GDExtension aus (Einheitentests).

## Breite des Fehlerbands in Standardabweichungen (95 %).
const BAND_SIGMA := 1.96


## Fortlaufende Tagesnummer eines Datums { year, month, day } (gregorianisch, julianische Tageszahl),
## für Abstände zwischen Daten.
static func day_number(date: Dictionary) -> int:
	var month := int(date.get("month", 1))
	var shift := floori((14 - month) / 12.0)
	var year := int(date.get("year", 0)) + 4800 - shift
	month += 12 * shift - 3
	return (
		int(date.get("day", 1)) + floori((153 * month + 2) / 5.0) + 365 * year
		+ floori(year / 4.0) - floori(year / 100.0) + floori(year / 400.0) - 32045
	)


## Indizes in election.districts in der Meldereihenfolge (report_rank, bei Gleichstand der Index).
static func report_order(election: Dictionary) -> PackedInt32Array:
	var districts: Array = election.get("districts", [])
	var order: Array[int] = []
	for i in districts.size():
		order.append(i)
	order.sort_custom(func(a: int, b: int) -> bool:
		var rank_a := int((districts[a] as Dictionary).get("report_rank", 0))
		var rank_b := int((districts[b] as Dictionary).get("report_rank", 0))
		return rank_a < rank_b if rank_a != rank_b else a < b
	)
	return PackedInt32Array(order)


## Gruppe eines Abgeordneten: Strömung oder (by_faction) Fraktion; Fraktionslose zählen in die
## letzte Gruppe (Index faction_count).
static func group_of(election: Dictionary, deputy: int, by_faction: bool, faction_count: int) -> int:
	if by_faction:
		var faction := int((election.get("deputy_faction", PackedInt32Array()) as PackedInt32Array)[deputy])
		return faction if faction >= 0 and faction < faction_count else faction_count
	return int((election.get("deputy_current", PackedInt32Array()) as PackedInt32Array)[deputy])


## Stand nach reported Meldungen (in der Meldereihenfolge). groups ist die Zahl der Gruppen
## (Strömungen oder Fraktionen plus fraktionslos). Ergebnis:
##   counts       gemeldete Sitze je Gruppe (PackedInt32Array)
##   projection   Hochrechnung je Gruppe (PackedFloat64Array): gemeldet plus die offenen Sitze nach
##                den Anteilen der gemeldeten Wahlkreise desselben Staates (sonst aller gemeldeten)
##   band         halbe Breite des Fehlerbands je Gruppe (± 1,96 σ, binomial je offenem Sitz)
##   reported, open_seats, seated (gemeldete Abgeordnete), empty (gemeldete leere Sitze, Boykott),
##   projected_seated (Hochrechnung der besetzten Sitze)
static func tally(election: Dictionary, reported: int, by_faction: bool, groups: int) -> Dictionary:
	var districts: Array = election.get("districts", [])
	var order := report_order(election)
	var reported_count := clampi(reported, 0, districts.size())
	var faction_count := groups - 1 if by_faction else groups
	var is_reported := {}
	for k in reported_count:
		is_reported[int((districts[order[k]] as Dictionary).get("district_index", -1))] = true

	var counts := PackedInt32Array()
	counts.resize(groups)
	var by_state := {} # Staat → { seats, empty, groups: Array (Sitze je Gruppe) }
	var total_seats := 0
	var empty := 0
	for k in reported_count:
		var district := districts[order[k]] as Dictionary
		var state := int(district.get("state_index", -1))
		var entry := _state_entry(by_state, state, groups)
		var seats := int(district.get("seats", 0))
		entry["seats"] = int(entry["seats"]) + seats
		total_seats += seats
		if bool(district.get("boycott", false)):
			entry["empty"] = int(entry["empty"]) + seats
			empty += seats
	var deputy_district: PackedInt32Array = election.get("deputy_district", PackedInt32Array())
	var seated := 0
	for deputy in deputy_district.size():
		if not is_reported.has(deputy_district[deputy]):
			continue
		var group := group_of(election, deputy, by_faction, faction_count)
		if group < 0 or group >= groups:
			continue
		counts[group] += 1
		seated += 1
	# Gemeldete Abgeordnete je Staat für die Anteile.
	var state_of_district := {}
	for district: Variant in districts:
		var d := district as Dictionary
		state_of_district[int(d.get("district_index", -1))] = int(d.get("state_index", -1))
	for deputy in deputy_district.size():
		if not is_reported.has(deputy_district[deputy]):
			continue
		var group := group_of(election, deputy, by_faction, faction_count)
		if group < 0 or group >= groups:
			continue
		var entry := _state_entry(by_state, int(state_of_district.get(deputy_district[deputy], -1)), groups)
		var state_groups: Array = entry["groups"]
		state_groups[group] = int(state_groups[group]) + 1

	var projection := PackedFloat64Array()
	var variance := PackedFloat64Array()
	projection.resize(groups)
	variance.resize(groups)
	for g in groups:
		projection[g] = counts[g]
	var open_seats := 0
	var projected_seated := float(seated)
	for k in range(reported_count, districts.size()):
		var district := districts[order[k]] as Dictionary
		var seats := int(district.get("seats", 0))
		open_seats += seats
		var state := int(district.get("state_index", -1))
		var basis: Dictionary = by_state.get(state, {})
		var basis_seats := int(basis.get("seats", 0))
		var basis_groups: Array = basis.get("groups", [])
		if basis_seats <= 0:
			basis_seats = total_seats
			basis_groups = Array(counts)
		if basis_seats <= 0:
			continue # noch keine Meldung: keine Hochrechnung
		for g in groups:
			var p := float(basis_groups[g]) / float(basis_seats)
			projection[g] += seats * p
			variance[g] += seats * p * (1.0 - p)
			projected_seated += seats * p
	var band := PackedFloat64Array()
	band.resize(groups)
	for g in groups:
		band[g] = BAND_SIGMA * sqrt(variance[g])
	return {
		"counts": counts,
		"projection": projection,
		"band": band,
		"reported": reported_count,
		"open_seats": open_seats,
		"seated": seated,
		"empty": empty,
		"projected_seated": projected_seated,
	}


static func _state_entry(by_state: Dictionary, state: int, groups: int) -> Dictionary:
	if not by_state.has(state):
		var counts: Array = []
		counts.resize(groups)
		counts.fill(0)
		by_state[state] = {"seats": 0, "empty": 0, "groups": counts}
	return by_state[state]


## Abgeordnete von links nach rechts (Position aufsteigend, bei Gleichstand der Index).
static func seat_order(election: Dictionary) -> PackedInt32Array:
	var positions: PackedFloat32Array = election.get("deputy_position", PackedFloat32Array())
	var order: Array[int] = []
	for i in positions.size():
		order.append(i)
	order.sort_custom(func(a: int, b: int) -> bool:
		return positions[a] < positions[b] if positions[a] != positions[b] else a < b
	)
	return PackedInt32Array(order)


## Sitzplätze eines Halbrunds mit count Sitzen um center (unten Mitte), Radius radius: Reihen von
## innen nach außen, Sitze je Reihe im Verhältnis zum Umfang; Ergebnis von links nach rechts
## (Winkel absteigend), bei gleichem Winkel innen zuerst. Der Sitzabstand ist dot_size.
static func hemicycle(count: int, center: Vector2, radius: float) -> Dictionary:
	var points := PackedVector2Array()
	if count <= 0 or radius <= 0.0:
		return {"points": points, "dot_size": 0.0}
	var rows := clampi(roundi(sqrt(count / 9.0)), 1, 14)
	var inner := radius * 0.38
	var spacing := (radius - inner) / rows
	var radii := PackedFloat64Array()
	var total := 0.0
	for r in rows:
		var row_radius := inner + spacing * (r + 0.5)
		radii.append(row_radius)
		total += row_radius
	var per_row := PackedInt32Array()
	var assigned := 0
	for r in rows:
		var n := roundi(count * radii[r] / total) if r < rows - 1 else count - assigned
		n = maxi(n, 0)
		per_row.append(n)
		assigned += n
	var seats: Array[Vector3] = [] # x, y, Winkel
	for r in rows:
		var n := per_row[r]
		for i in n:
			var angle := PI * 0.5 if n == 1 else PI * (1.0 - float(i) / float(n - 1))
			var point := center + Vector2(cos(angle), -sin(angle)) * radii[r]
			seats.append(Vector3(point.x, point.y, angle + r * 1e-6))
	seats.sort_custom(func(a: Vector3, b: Vector3) -> bool: return a.z > b.z)
	for seat in seats:
		points.append(Vector2(seat.x, seat.y))
	var row_seats := float(per_row[rows - 1]) if per_row[rows - 1] > 0 else 1.0
	var dot := minf(spacing * 0.8, PI * radii[rows - 1] / row_seats * 0.8)
	return {"points": points, "dot_size": dot}


## Meldung eines Wahlkreises als Telegramm, z. B. „Mannheim (Baden): Demokraten 61 %“; mit dem
## Kurznamen der Strömung (short_name), damit der Anteil auch in schmalen Spalten sichtbar bleibt.
static func telegram(district: Dictionary, currents: Array[Dictionary]) -> String:
	var place := str(district.get("name", "–"))
	var state := str(district.get("state_name", ""))
	if not state.is_empty():
		place += " (%s)" % state
	if bool(district.get("boycott", false)):
		return "%s: Boykott, kein Abgeordneter" % place
	var winner := int(district.get("winner", -1))
	if winner < 0 or winner >= currents.size():
		return "%s: ohne Ergebnis" % place
	var text := "%s: %s %s" % [
		place,
		str(currents[winner].get("short_name", currents[winner].get("name", "–"))),
		GermanFormat.percent(roundi(float(district.get("winner_share", 0.0)) * 100.0)),
	]
	if bool(district.get("runoff", false)):
		text += " nach Stichwahl"
	var seats := int(district.get("seats", 1))
	if seats > 1:
		text += ", %d Sitze" % seats
	return text
