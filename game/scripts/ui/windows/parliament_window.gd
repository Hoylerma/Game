class_name ParliamentWindow
extends AreaWindow
## Parlament – Plenarsaal (seit M13a5b): ein Parlament zur Wahl (zuerst das des Spielerstaates), seine
## letzte Wahl als Halbrund nach Fraktionen mit Legende, die nächste Wahl und die Abstimmungen.

const HEMICYCLE_HEIGHT := 150.0
const INDEPENDENT_COLOR := Color("9a9184")

var _select: OptionButton = null
## Parlamente in der Reihenfolge der Auswahl (Index in get_parliaments).
var _choices := PackedInt32Array()
var _chosen := -1


func _window_title() -> String:
	return "PARLAMENT"


func _window_name() -> String:
	return "ParliamentWindow"


func _build_head(box: VBoxContainer) -> void:
	_select = OptionButton.new()
	_select.name = "Parliament"
	_select.focus_mode = Control.FOCUS_NONE
	_select.item_selected.connect(
		func(i: int) -> void:
			_chosen = _choices[i] if i >= 0 and i < _choices.size() else -1
			refresh(true)
	)
	box.add_child(_select)


## Parlament wählen (Index in get_parliaments, Tests).
func select_parliament(index: int) -> void:
	_chosen = index
	refresh(true)


func chosen_parliament() -> int:
	return _chosen


func _fill() -> void:
	var parliaments := _sim.get_parliaments()
	if parliaments.is_empty():
		_select.visible = false
		_paragraph("Das Lesezeichen kennt kein Parlament.", true)
		return
	_select.visible = true
	# Zuerst der Landtag des Spielerstaates, dann die übrigen.
	_choices = PackedInt32Array()
	for p in parliaments:
		if int(p.get("state_index", -1)) == _player:
			_choices.append(int(p.get("index", 0)))
	for p in parliaments:
		if not _choices.has(int(p.get("index", 0))):
			_choices.append(int(p.get("index", 0)))
	if not _choices.has(_chosen):
		_chosen = _choices[0]
	_select.clear()
	for i in _choices.size():
		_select.add_item(str(parliaments[_choices[i]].get("name", "")))
		if _choices[i] == _chosen:
			_select.select(i)
	var parliament := parliaments[_chosen]
	_line("Wahlrecht", str(parliament.get("franchise", "")) + (", über Wahlmänner" if bool(parliament.get("indirect", false)) else ""))
	if bool(parliament.get("pending", false)):
		_line("Nächste Wahl", str(parliament.get("election_date_text", "")))
	var election_index := int(parliament.get("election_index", -1))
	if election_index < 0:
		_line("Sitze", Format.integer(int(parliament.get("seats", 0))))
		_paragraph("Noch nicht gewählt.", true)
	else:
		var election := _sim.get_election(election_index)
		_heading("LETZTE WAHL AM %s" % str(election.get("date_text", "")).to_upper())
		var factions: Array = parliament.get("factions", [])
		var seats: PackedInt32Array = election.get("seats_by_faction", PackedInt32Array())
		var groups: Array[Dictionary] = []
		for f in factions.size():
			var count := seats[f] if f < seats.size() else 0
			groups.append({"name": str(factions[f].get("name", "")), "color": factions[f].get("color", INDEPENDENT_COLOR), "seats": count})
		if seats.size() > factions.size():
			groups.append({"name": "fraktionslos", "color": INDEPENDENT_COLOR, "seats": seats[seats.size() - 1]})
		_add_hemicycle(groups)
		var seated := int(election.get("seated", 0))
		for g in groups:
			if int(g.get("seats", 0)) > 0:
				_bar(str(g.get("name", "")), float(g.get("seats", 0)) / maxf(seated, 1.0), g.get("color", INDEPENDENT_COLOR), Format.integer(int(g.get("seats", 0))))
		_line("Besetzt", "%s von %s Sitzen" % [Format.integer(seated), Format.integer(int(parliament.get("seats", 0)))])
	var votes := _sim.get_votes().filter(func(v: Dictionary) -> bool: return int(v.get("parliament_index", -1)) == _chosen)
	if not votes.is_empty():
		_gap()
		_heading("ABSTIMMUNGEN")
		for vote: Dictionary in votes:
			_line(
				"%s (%s)" % [str(vote.get("title", "")), str(vote.get("date_text", ""))],
				"%s: %d zu %d" % ["angenommen" if bool(vote.get("accepted", false)) else "abgelehnt", int(vote.get("yes", 0)), int(vote.get("no", 0))]
			)


## Halbrund: je Sitz ein Punkt in der Farbe der Fraktion, von links nach rechts in der Reihenfolge der Achse.
func _add_hemicycle(groups: Array[Dictionary]) -> void:
	var colors := PackedColorArray()
	for g in groups:
		for i in int(g.get("seats", 0)):
			colors.append(g.get("color", INDEPENDENT_COLOR))
	var view := Control.new()
	view.name = "Hemicycle"
	view.custom_minimum_size = Vector2(0, HEMICYCLE_HEIGHT)
	view.mouse_filter = Control.MOUSE_FILTER_IGNORE
	view.draw.connect(
		func() -> void:
			var size := view.size
			var layout := ElectionTally.hemicycle(colors.size(), Vector2(size.x * 0.5, size.y - 4.0), minf(size.x * 0.5 - 6.0, size.y - 10.0))
			var points: PackedVector2Array = layout.get("points", PackedVector2Array())
			var dot := float(layout.get("dot_size", 4.0)) * 0.5
			for k in mini(points.size(), colors.size()):
				view.draw_circle(points[k], dot, colors[k])
	)
	_content.add_child(view)
