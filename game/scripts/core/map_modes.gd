class_name MapModes
extends RefCounted
## Kartenmodi der Brücke (get_map_colors) mit Namen, Tasten und Aktionen.
## Die Konstanten heißen wie in docs/technik/bridge-api.md (MAP_POLITICAL …).

const MAP_POLITICAL := 0
const MAP_DENSITY := 1
const MAP_CONFESSION := 2
const MAP_TERRAIN := 3
const MAP_DEVASTATION := 4
const COUNT := 5

## Anzeigenamen der Modi (Index = Modus).
const NAMES: PackedStringArray = [
	"Politisch",
	"Bevölkerungsdichte",
	"Konfession",
	"Gelände",
	"Verwüstung",
]

## Kurze Erklärung für Tooltips der Modusleiste.
const DESCRIPTIONS: PackedStringArray = [
	"Besitzer der Orte",
	"Einwohner je Quadratkilometer",
	"Mehrheitskonfession der Orte",
	"Geländetyp der Orte",
	"Kriegsverwüstung der Orte",
]

## Aktionen der Eingabebelegung (project.godot), Index = Modus.
const ACTIONS: Array[StringName] = [
	&"kartenmodus_politisch",
	&"kartenmodus_dichte",
	&"kartenmodus_konfession",
	&"kartenmodus_gelaende",
	&"kartenmodus_verwuestung",
]

## Tastenbezeichnung für die Anzeige, Index = Modus.
const KEY_HINTS: PackedStringArray = ["Q", "W", "E", "R", "T"]

## Obergrenze der Dichteskala der Brücke (≥ 120 Einwohner je km² → dunkelste Farbe).
const DENSITY_SCALE_MAX := 120.0


static func is_valid(mode: int) -> bool:
	return mode >= 0 and mode < COUNT


static func name_of(mode: int) -> String:
	return NAMES[mode] if is_valid(mode) else "–"


## Modus, dessen Aktion das Ereignis auslöst, sonst -1.
static func mode_for_event(event: InputEvent) -> int:
	for mode in COUNT:
		if event.is_action(ACTIONS[mode], true):
			return mode
	return -1
