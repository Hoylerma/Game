class_name RecapWindow
extends AreaWindow
## „Was ist passiert?“ (seit M13b3, vertical_slice.md 5.4): nach dem Laden eines Spielstands die wichtigsten
## Nachrichten des letzten Jahres vor seinem Datum, höchstens zehn, neueste zuerst. Wichtig ist, was die
## Zeitung auf die Titelseite brächte (Gewicht 3, Gewicht 2 mit dem Spielerstaat beteiligt); ohne Zeitung
## (1648) die Wendepunkte, Kriegserklärungen, Frieden und Wahlen der Chronik.

const LIMIT := 10
const DAYS := 365

var _entries: Array[Dictionary] = []


func _window_title() -> String:
	return "WAS IST PASSIERT?"


func _window_name() -> String:
	return "RecapWindow"


## Einträge (Tests und main.gd): je { date_text, headline, subline }.
func entries() -> Array[Dictionary]:
	return _entries


## Wichtige Nachrichten des letzten Jahres vor dem Datum der Partie.
static func collect(sim: SimulationClient, player: int) -> Array[Dictionary]:
	var result: Array[Dictionary] = []
	if sim == null:
		return result
	var today := ElectionTally.day_number(sim.get_date())
	var pages := sim.get_front_pages(player, 2)
	if not pages.is_empty():
		for i in range(pages.size() - 1, -1, -1):
			var page := pages[i]
			if int(page.get("weight", 0)) < 3 and not bool(page.get("involved", false)):
				continue
			var day := _day(str(page.get("date_short", "")))
			if today - day > DAYS:
				break
			result.append({
				"date_text": str(page.get("date_text", "")), "headline": str(page.get("headline", "")),
				"subline": str(page.get("subline", "")),
			})
			if result.size() >= LIMIT:
				break
		return result
	# Ohne Zeitung: die Chronik selbst.
	var count := sim.get_chronicle_size()
	var chronicle := sim.get_chronicle(maxi(count - 2000, 0))
	for i in range(chronicle.size() - 1, -1, -1):
		var entry := chronicle[i]
		var text := str(entry.get("text", ""))
		if today - _day(str(entry.get("date_text", ""))) > DAYS:
			break
		var important := (
			bool(entry.get("turning_point", false)) or text.contains(" den Krieg") or text.begins_with("Frieden zwischen")
			or text.begins_with("Wahltag") or text.begins_with("Gründung:")
		)
		if important:
			result.append({"date_text": str(entry.get("date_text", "")), "headline": text, "subline": ""})
			if result.size() >= LIMIT:
				break
	return result


static func _day(date_short: String) -> int:
	var parts := date_short.split(".")
	if parts.size() != 3:
		return -1000000
	return ElectionTally.day_number({"day": parts[0].to_int(), "month": parts[1].to_int(), "year": parts[2].to_int()})


func _fill() -> void:
	_entries = collect(_sim, _player)
	if _entries.is_empty():
		_paragraph("Im letzten Jahr ist nichts Bewegendes geschehen.", true)
		return
	_paragraph("Die wichtigsten Nachrichten des letzten Jahres bis %s:" % _sim.get_date_text(), true)
	for entry in _entries:
		_gap()
		_paragraph(str(entry.get("date_text", "")), true)
		var headline := _paragraph(str(entry.get("headline", "")))
		headline.theme_type_variation = &"ValueLabel"
		if not str(entry.get("subline", "")).is_empty():
			_paragraph(str(entry.get("subline", "")))
