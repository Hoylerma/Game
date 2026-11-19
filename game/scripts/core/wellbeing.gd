class_name Wellbeing
extends RefCounted
## „Wie geht es meiner Bevölkerung?“ (seit der Rückmeldung aus dem Spiel, 11/2026): ein Urteil in Worten aus
## den Werten je Schicht (SimulationClient.get_state_society: strata, hungry, worst_locations) und der
## Unruhe, mit Gründen und Hinweisen, was hilft. Für den Tooltip der Bevölkerung und das Fenster
## Gesellschaft; ohne Simulation prüfbar.

enum Level { GOOD, STRAINED, BAD }

const STRATA := ["Unterschicht", "Mittelschicht", "Oberschicht"]
const TITLES := ["Dem Volk geht es gut", "Die Lage ist angespannt", "Dem Volk geht es schlecht"]

## Schwellen: Anteil der Menschen in Not (Prozent), Lebensstandard der Unterschicht, Grundnahrung der
## Unterschicht (Prozent), Unruhe.
const NEED_STRAINED := 5.0
const NEED_BAD := 25.0
const LOWER_LIVING_STRAINED := 45.0
const LOWER_LIVING_BAD := 35.0
const LOWER_FOOD_STRAINED := 85.0
const UNREST_STRAINED := 10.0
const UNREST_BAD := 30.0


## Urteil: { level, title, causes, tips } aus society (get_state_society) und der Unruhe (0–100).
static func assess(society: Dictionary, unrest: float) -> Dictionary:
	var population := maxf(float(society.get("population", 0)), 1.0)
	var need := float(society.get("hungry", 0))
	var need_share := need * 100.0 / population
	var strata: Array = society.get("strata", [])
	var lower: Dictionary = strata[0] if not strata.is_empty() else {}
	var lower_living := float(lower.get("living_standard", society.get("living_standard", 50.0)))
	var lower_food := float(lower.get("food_percent", 100.0))
	var causes := PackedStringArray()
	var tips := PackedStringArray()
	var level := Level.GOOD
	if need_share >= NEED_STRAINED:
		level = maxi(level, Level.BAD if need_share >= NEED_BAD else Level.STRAINED)
		causes.append("%s Menschen sind in Not: Ihnen fehlt mehr als ein Viertel der Grundnahrung, es sterben mehr." % Format.compact(need))
	if lower_food < LOWER_FOOD_STRAINED:
		level = maxi(level, Level.STRAINED)
		causes.append("Die Unterschicht deckt nur %s ihrer Grundnahrung." % Format.percent(lower_food, 0))
	if need_share >= NEED_STRAINED or lower_food < LOWER_FOOD_STRAINED:
		tips.append("Akzise senken: Sie verteuert Brot, Fleisch und Bier (Staatsübersicht, Steuern).")
	if lower_living < LOWER_LIVING_STRAINED:
		level = maxi(level, Level.BAD if lower_living < LOWER_LIVING_BAD else Level.STRAINED)
		causes.append("Die Unterschicht lebt kärglich (Lebensstandard %s von 100)." % Format.number(lower_living, 0))
		tips.append("Kontribution senken: Den Leuten bleibt mehr vom Einkommen (Staatsübersicht, Steuern).")
	if unrest >= UNREST_STRAINED:
		level = maxi(level, Level.BAD if unrest >= UNREST_BAD else Level.STRAINED)
		causes.append(
			("Unruhe %s: Ein Aufstand droht." if unrest >= UNREST_BAD else "Unruhe %s: Die Stimmung kippt.") % Format.integer(roundi(unrest))
		)
		tips.append("Die Gründe der Unruhe stehen im Tooltip der Unruhe; Reformen und Zugeständnisse senken sie.")
	if causes.is_empty():
		causes.append("Alle Schichten sind satt, niemand ist in Not.")
	return {"level": level, "title": TITLES[level], "causes": causes, "tips": tips}


## Farbe eines Urteils.
static func color_of(level: int) -> Color:
	match level:
		Level.GOOD:
			return Accessibility.adjust(UiPalette.POSITIVE)
		Level.STRAINED:
			return UiPalette.GOLD
	return UiPalette.NEGATIVE


## Farbe eines Lebensstandards (0–100) bzw. einer Deckung (Prozent) für Tooltip und Fenster.
static func living_color(value: float) -> Color:
	if value >= 55.0:
		return Accessibility.adjust(UiPalette.POSITIVE)
	if value >= LOWER_LIVING_STRAINED:
		return UiPalette.INK
	if value >= LOWER_LIVING_BAD:
		return UiPalette.GOLD
	return UiPalette.NEGATIVE


static func food_color(percent: float) -> Color:
	if percent >= 95.0:
		return Accessibility.adjust(UiPalette.POSITIVE)
	if percent >= LOWER_FOOD_STRAINED:
		return UiPalette.INK
	if percent >= 75.0:
		return UiPalette.GOLD
	return UiPalette.NEGATIVE


## Tooltip „Bevölkerung“ (BBCode): Urteil, Schichten, Gründe, was hilft, die schwächsten Orte.
static func tooltip(society: Dictionary, unrest: float) -> String:
	var verdict := assess(society, unrest)
	var lines := PackedStringArray([
		"[b]Bevölkerung[/b] %s Einwohner" % Format.integer(int(society.get("population", 0))),
		"[color=#%s][b]● %s[/b][/color]" % [color_of(int(verdict["level"])).to_html(false), str(verdict["title"])],
		"%s %s von 100 (Mittel aller Einwohner) · %s: %s" % [
			Glossary.link("lebensstandard", "Lebensstandard"), Format.number(float(society.get("living_standard", 0.0)), 1),
			Glossary.link("not", "in Not"), Format.integer(int(society.get("hungry", 0))),
		],
		"",
		"[b]Je Schicht[/b] – Anteil · Lebensstandard · Grundnahrung · Einkommen im Jahr",
	])
	var strata: Array = society.get("strata", [])
	for row: Dictionary in strata:
		var stratum := int(row.get("stratum", 0))
		var living := float(row.get("living_standard", 0.0))
		var food := float(row.get("food_percent", 0.0))
		lines.append("  %s %s · [color=#%s]%s[/color] · [color=#%s]%s[/color] · %s" % [
			STRATA[clampi(stratum, 0, 2)], Format.percent(float(row.get("percent", 0.0)), 0),
			living_color(living).to_html(false), Format.number(living, 0),
			food_color(food).to_html(false), Format.percent(food, 0),
			Format.money(float(row.get("income_year", 0.0))),
		])
	lines.append("")
	lines.append("[b]Warum[/b]")
	for cause in verdict["causes"] as PackedStringArray:
		lines.append("  • " + cause)
	var tips := verdict["tips"] as PackedStringArray
	if not tips.is_empty():
		lines.append("[b]Was hilft[/b]")
		for tip in tips:
			lines.append("  • " + tip)
	var worst: Array = society.get("worst_locations", [])
	if not worst.is_empty():
		var names := PackedStringArray()
		for place: Dictionary in worst:
			names.append("%s %s" % [str(place.get("name", "")), Format.number(float(place.get("living_standard", 0.0)), 0)])
		lines.append("Am schlechtesten lebt man in: %s (Kartenmodus Lebensstandard)" % ", ".join(names))
	return "\n".join(lines)
