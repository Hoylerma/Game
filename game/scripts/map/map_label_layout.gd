class_name MapLabelLayout
extends RefCounted
## Beschriftungspunkte für Staaten und Kreise, berechnet aus den Orten der
## definition.csv (Beschriftungspunkt x, y und Pixelzahl je Ort).
##
## Ein Gruppenname (Staat, Kreis) steht im Schwerpunkt der Beschriftungspunkte
## seiner Orte, gewichtet mit deren Pixelzahl. Liegt dieser Schwerpunkt nicht auf
## der Gruppe selbst (z. B. bei einem Staat aus verstreuten Orten), rückt er auf den
## nächstgelegenen Beschriftungspunkt eines Ortes der Gruppe.
##
## Staaten zerfallen oft in getrennte Gebiete (Exklaven). Die Orte eines Staates
## werden deshalb zu Gebieten gebündelt (Beschriftungspunkte höchstens
## CLUSTER_DISTANCE × typischer Ortsdurchmesser auseinander). Beschriftet werden das
## größte Gebiet und jedes weitere, das mindestens EXTRA_AREA_SHARE davon und
## mindestens SUBSTANTIAL_LOCATIONS typische Orte groß ist. Ist schon das größte
## Gebiet kleiner (ein zersplitterter Staat), steht ein Name im Schwerpunkt aller Orte.

const CLUSTER_DISTANCE := 2.5
const EXTRA_AREA_SHARE := 0.35
const SUBSTANTIAL_LOCATIONS := 8.0


## Kartenindizes je Gruppe; group_of_map: Kartenindex → Gruppe (−1 = keine).
## Nur Orte mit Beschriftungspunkt (Pixelzahl > 0) zählen.
static func members_by_group(
	group_of_map: PackedInt32Array, map: MapData, group_count: int
) -> Array[PackedInt32Array]:
	var result: Array[PackedInt32Array] = []
	result.resize(maxi(group_count, 0))
	for map_index in mini(group_of_map.size(), map.label_points.size()):
		var group := group_of_map[map_index]
		if group < 0 or group >= group_count or map.label_points[map_index].x < 0.0:
			continue
		result[group].append(map_index)
	return result


## Schwerpunkt der Beschriftungspunkte von members, gewichtet mit der Pixelzahl.
static func weighted_center(members: PackedInt32Array, map: MapData) -> Vector2:
	var sum := Vector2.ZERO
	var weight := 0.0
	for map_index in members:
		var pixels := float(maxi(map.pixel_counts[map_index], 1))
		sum += map.label_points[map_index] * pixels
		weight += pixels
	if weight <= 0.0:
		return Vector2(-1, -1)
	return sum / weight


## Summe der Pixel von members.
static func pixel_sum(members: PackedInt32Array, map: MapData) -> int:
	var total := 0
	for map_index in members:
		total += map.pixel_counts[map_index]
	return total


## Beschriftungspunkt einer Gruppe: gewichteter Schwerpunkt, falls er auf einem Ort
## der Gruppe liegt, sonst der nächstgelegene Beschriftungspunkt eines Mitglieds.
static func anchor(
	members: PackedInt32Array, map: MapData, group_of_map: PackedInt32Array, group: int
) -> Vector2:
	var center := weighted_center(members, map)
	if center.x < 0.0:
		return center
	var at_center := map.map_index_at_pixel(Vector2i(floori(center.x), floori(center.y)))
	if at_center >= 0 and at_center < group_of_map.size() and group_of_map[at_center] == group:
		return center
	var best := center
	var best_distance := INF
	for map_index in members:
		var distance := map.label_points[map_index].distance_squared_to(center)
		if distance < best_distance:
			best_distance = distance
			best = map.label_points[map_index]
	return best


## Bündelt members (Kartenindizes) zu Gebieten: Einzelverkettung der
## Beschriftungspunkte mit höchstem Abstand distance, über ein Raster in O(n).
static func clusters(
	members: PackedInt32Array, map: MapData, distance: float
) -> Array[PackedInt32Array]:
	var count := members.size()
	var parent := PackedInt32Array()
	parent.resize(count)
	for i in count:
		parent[i] = i
	var cell_size := maxf(distance, 1.0)
	var cells := {}
	for i in count:
		var key := Vector2i((map.label_points[members[i]] / cell_size).floor())
		var bucket: PackedInt32Array = cells.get(key, PackedInt32Array())
		bucket.append(i)
		cells[key] = bucket
	var limit := distance * distance
	for i in count:
		var point := map.label_points[members[i]]
		var key := Vector2i((point / cell_size).floor())
		for dy in range(-1, 2):
			for dx in range(-1, 2):
				var neighbour_key := key + Vector2i(dx, dy)
				if not cells.has(neighbour_key):
					continue
				var bucket: PackedInt32Array = cells[neighbour_key]
				for j in bucket:
					if j <= i:
						continue
					if point.distance_squared_to(map.label_points[members[j]]) <= limit:
						_union(parent, i, j)
	var by_root := {}
	var order: Array[int] = []
	for i in count:
		var root := _find(parent, i)
		if not by_root.has(root):
			by_root[root] = PackedInt32Array()
			order.append(root)
		var group: PackedInt32Array = by_root[root]
		group.append(members[i])
		by_root[root] = group
	var result: Array[PackedInt32Array] = []
	for root in order:
		result.append(by_root[root])
	return result


## Namen der Kreise: { "group", "point", "pixels" } je Kreis mit Orten.
static func kreis_labels(
	map: MapData, kreis_of_map: PackedInt32Array, kreis_count: int
) -> Array[Dictionary]:
	var result: Array[Dictionary] = []
	var members := members_by_group(kreis_of_map, map, kreis_count)
	for kreis in members.size():
		if members[kreis].is_empty():
			continue
		result.append({
			"group": kreis,
			"point": anchor(members[kreis], map, kreis_of_map, kreis),
			"pixels": pixel_sum(members[kreis], map),
		})
	return result


## Namen der Staaten: { "group", "point", "pixels" } je beschriftetem Gebiet.
## diameter = typischer Ortsdurchmesser in Pixeln (MapData.typical_location_diameter).
static func state_labels(
	map: MapData, owner_of_map: PackedInt32Array, state_count: int, diameter: float
) -> Array[Dictionary]:
	var result: Array[Dictionary] = []
	var members := members_by_group(owner_of_map, map, state_count)
	var substantial := SUBSTANTIAL_LOCATIONS * diameter * diameter
	for state in members.size():
		if members[state].is_empty():
			continue
		var areas := clusters(members[state], map, CLUSTER_DISTANCE * diameter)
		var sizes := PackedInt32Array()
		var largest := 0
		for area in areas:
			sizes.append(pixel_sum(area, map))
			largest = maxi(largest, sizes[sizes.size() - 1])
		if largest < substantial:
			result.append({
				"group": state,
				"point": anchor(members[state], map, owner_of_map, state),
				"pixels": pixel_sum(members[state], map),
			})
			continue
		for i in areas.size():
			if sizes[i] == largest or (
				sizes[i] >= largest * EXTRA_AREA_SHARE and sizes[i] >= substantial
			):
				result.append({
					"group": state,
					"point": anchor(areas[i], map, owner_of_map, state),
					"pixels": sizes[i],
				})
	return result


static func _find(parent: PackedInt32Array, i: int) -> int:
	var root := i
	while parent[root] != root:
		root = parent[root]
	while parent[i] != root:
		var next := parent[i]
		parent[i] = root
		i = next
	return root


static func _union(parent: PackedInt32Array, a: int, b: int) -> void:
	var root_a := _find(parent, a)
	var root_b := _find(parent, b)
	if root_a != root_b:
		parent[maxi(root_a, root_b)] = mini(root_a, root_b)
