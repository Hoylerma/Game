class_name MapModes
extends RefCounted
## Kartenmodi der Brücke (get_map_colors) mit Namen, Tasten und Aktionen.
## Die Konstanten heißen wie in docs/technik/bridge-api.md (MAP_POLITICAL …).

const MAP_POLITICAL := 0
const MAP_DENSITY := 1
const MAP_CONFESSION := 2
const MAP_TERRAIN := 3
const MAP_DEVASTATION := 4
const MAP_KREISE := 5
const MAP_LEBENSSTANDARD := 6
const MAP_KULTUR := 7
const MAP_WIRTSCHAFT := 8
const MAP_MAERKTE := 9
const MAP_VERWALTUNG := 10
const COUNT := 11

## Anzeigenamen der Modi (Index = Modus).
const NAMES: PackedStringArray = [
	"Politisch",
	"Bevölkerungsdichte",
	"Konfession",
	"Gelände",
	"Verwüstung",
	"Kreise",
	"Lebensstandard",
	"Kultur",
	"Wirtschaftsleistung",
	"Märkte",
	"Verwaltung",
]

## Kurznamen für die Knöpfe der Modusleiste (Index = Modus); seit M4 kurz gehalten,
## damit die Leiste zwischen die Spalten passt.
const SHORT_NAMES: PackedStringArray = [
	"Politisch",
	"Dichte",
	"Konfession",
	"Gelände",
	"Verwüstung",
	"Kreise",
	"Lebensstandard",
	"Kultur",
	"Wirtschaft",
	"Märkte",
	"Verwaltung",
]

## Kurze Erklärung für Tooltips der Modusleiste.
const DESCRIPTIONS: PackedStringArray = [
	"Besitzer der Orte",
	"Einwohner je Quadratkilometer",
	"Mehrheitskonfession der Orte",
	"Geländetyp der Orte",
	"Kriegsverwüstung der Orte",
	"Kreise (je 4–12 Orte)",
	"Mittlerer Lebensstandard der Einwohner (0–100)",
	"Mehrheitskultur (Landsmannschaft) der Orte",
	"Wirtschaftsleistung je Einwohner und Jahr (in der Währung des Szenarios)",
	"Märkte: Orte, die gemeinsam handeln und Preise bilden",
	"Verwaltungseinheiten der Staaten (Provinzen, Kronländer, Kreise); seit 1848",
]

## Aktionen der Eingabebelegung (project.godot), Index = Modus.
const ACTIONS: Array[StringName] = [
	&"kartenmodus_politisch",
	&"kartenmodus_dichte",
	&"kartenmodus_konfession",
	&"kartenmodus_gelaende",
	&"kartenmodus_verwuestung",
	&"kartenmodus_kreise",
	&"kartenmodus_lebensstandard",
	&"kartenmodus_kultur",
	&"kartenmodus_wirtschaft",
	&"kartenmodus_maerkte",
	&"kartenmodus_verwaltung",
]

## Tastenbezeichnung für die Anzeige, Index = Modus. Die Belegung nutzt physische
## Tasten: „Z“ ist die Taste rechts neben T (auf englischen Tastaturen „Y“), „Ü“ die rechts
## neben P (englisch „[“).
const KEY_HINTS: PackedStringArray = ["Q", "W", "E", "R", "T", "Z", "U", "I", "O", "P", "Ü"]

## Modi, deren Farben von der Simulation abhängen; dort erhalten Orte ohne Daten
## Alpha 0 (Schraffur „keine Daten“, bridge-api.md). Im Marktmodus gilt das nur für
## Märkte ohne Orte mit Daten.
const DATA_MODES: PackedInt32Array = [
	MAP_POLITICAL,
	MAP_DENSITY,
	MAP_CONFESSION,
	MAP_DEVASTATION,
	MAP_LEBENSSTANDARD,
	MAP_KULTUR,
	MAP_WIRTSCHAFT,
	MAP_MAERKTE,
	MAP_VERWALTUNG,
]

## Obergrenze der Dichteskala der Brücke (≥ 120 Einwohner je km² → dunkelste Farbe).
const DENSITY_SCALE_MAX := 120.0
## Obergrenze der Wirtschaftsskala der Brücke (≥ 16 Reichstaler je Einwohner und Jahr).
const ECONOMY_SCALE_MAX := 16.0


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
