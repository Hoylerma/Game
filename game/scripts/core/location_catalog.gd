class_name LocationCatalog
extends RefCounted
## Unveränderliche Angaben aller Orte, einmal nach dem Laden gelesen.
##
## Name, Stadt, Gelände, Kreis und „hat Daten“ ändern sich während einer Partie
## nicht (bridge-api.md: Kartenfelder wie in den Daten; datierte Blöcke wirken nur
## bis zum Startdatum). Beschriftung, Legenden und Tooltips lesen sie hier, statt
## je Bild Tausende get_location-Aufrufe zu machen. Index = Simulationsindex.

var names := PackedStringArray()
var terrains := PackedStringArray()
var is_city := PackedByteArray()
var has_data := PackedByteArray()
var kreis_indices := PackedInt32Array()
## Anzeigenamen der Kreise (Index = Kreisindex).
var kreis_names := PackedStringArray()
## Anzahl der Orte mit Daten.
var data_count := 0


## Liest alle Orte und Kreise der Simulation (ein get_location je Ort).
static func build(sim: SimulationClient) -> LocationCatalog:
	var catalog := LocationCatalog.new()
	if sim == null:
		return catalog
	var count := sim.get_location_count()
	catalog.names.resize(count)
	catalog.terrains.resize(count)
	catalog.is_city.resize(count)
	catalog.has_data.resize(count)
	for index in count:
		var location := sim.get_location(index)
		catalog.names[index] = str(location.get("name", ""))
		catalog.terrains[index] = str(location.get("terrain", ""))
		catalog.is_city[index] = 1 if bool(location.get("is_city", false)) else 0
		var with_data := bool(location.get("has_data", true))
		catalog.has_data[index] = 1 if with_data else 0
		if with_data:
			catalog.data_count += 1
	catalog.kreis_indices = sim.get_location_kreis_indices()
	if catalog.kreis_indices.size() != count:
		catalog.kreis_indices.resize(count)
		catalog.kreis_indices.fill(-1)
	var kreis_count := sim.get_kreis_count()
	catalog.kreis_names.resize(kreis_count)
	for kreis in kreis_count:
		catalog.kreis_names[kreis] = str(sim.get_kreis(kreis).get("name", ""))
	return catalog


func size() -> int:
	return names.size()


## Anzahl der Orte ohne Daten.
func no_data_count() -> int:
	return size() - data_count


## Name des Kreises eines Ortes oder "".
func kreis_name_of(sim_index: int) -> String:
	if sim_index < 0 or sim_index >= kreis_indices.size():
		return ""
	var kreis := kreis_indices[sim_index]
	if kreis < 0 or kreis >= kreis_names.size():
		return ""
	return kreis_names[kreis]
