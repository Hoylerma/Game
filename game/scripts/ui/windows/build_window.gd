class_name BuildWindow
extends AreaWindow
## Bauen (seit M15b3, docs/technik/bauen_und_anleitung.md Abschnitt 3.1): zuerst die besten Vorhaben mit einem
## Knopf „Bauen“, dann das Bauamt mit den laufenden Aufträgen (Fortschritt, Abbrechen), zuletzt die
## Landesteile des Staates. Ein Klick auf einen Landesteil zeigt seine Gebäude mit Stufe, Wirkung der nächsten
## Stufe und dem Grund, wenn sie nicht geht. Gebaut wird in Landesteilen (Provinz, Kronland, sonst Kreis),
## damit niemand 1.400 Orte einzeln bedenken muss.

## Landesteil auf der Karte zeigen (erster Ort).
signal location_requested(location_index: int)
## Ein Auftrag wurde gegeben oder abgebrochen (Meldung in der oberen Leiste).
signal order_changed(text: String)

const PROPOSALS := 5

## Gezeigter Landesteil, -1 für die Übersicht.
var _region := -1


func _window_title() -> String:
	return "BAUEN"


func _window_name() -> String:
	return "BuildWindow"


## Landesteil zeigen (-1: Übersicht).
func show_region(region: int) -> void:
	_region = region
	refresh(true)


func shown_region() -> int:
	return _region


func _fill() -> void:
	if not bool(_sim.get_building_info().get("active", false)):
		_paragraph("In diesem Lesezeichen wird nicht gebaut.", true)
		return
	if _region >= 0:
		_fill_region()
		return
	_paragraph(
		"Gebaut wird in Landesteilen. Ein Auftrag kostet in Monatsraten aus der Staatskasse und danach Unterhalt; "
		+ "die Vorschläge zeigen, was er den Leuten und dem Staat im Jahr bringt.",
		true
	)
	_gap()
	_heading("VORSCHLÄGE")
	var proposals := _sim.get_build_proposals(_player, PROPOSALS)
	if proposals.is_empty():
		_paragraph("Zurzeit lohnt sich kein Vorhaben, oder das Bauamt ist voll.", true)
	for proposal: Dictionary in proposals:
		_proposal_row(proposal)
	_gap()
	var office := _sim.get_build_office(_player)
	_heading("BAUAMT – %d VON %d AUFTRÄGEN" % [int(office.get("orders", 0)), int(office.get("capacity", 0))])
	var orders := _sim.get_build_orders(_player)
	if orders.is_empty():
		_paragraph("Keine laufenden Bauten.", true)
	for order: Dictionary in orders:
		_order_row(order)
	_line(
		"Raten im Monat",
		Format.money(float(office.get("monthly_rates", 0.0))),
		"Summe der Raten der laufenden Aufträge; dazu kommt der Unterhalt fertiger Gebäude (Posten „Bauten“)."
	)
	_gap()
	_heading("LANDESTEILE")
	for region: Dictionary in _sim.get_regions(_player):
		var button := _button(
			"%s – %s Einwohner, %d Stufen" % [
				str(region.get("name", "")), Format.compact(float(region.get("population", 0))), int(region.get("levels", 0))
			],
			"Gebäude und Vorhaben im Landesteil"
		)
		button.name = "Region_%d" % int(region.get("index", -1))
		var index := int(region.get("index", -1))
		button.pressed.connect(func() -> void: show_region(index))
		_content.add_child(button)


## Vorhaben: Name und Landesteil mit „Bauen“, darunter Kosten und Wirkung.
func _proposal_row(proposal: Dictionary) -> void:
	var row := HBoxContainer.new()
	row.add_theme_constant_override(&"separation", 8)
	var title := Label.new()
	title.text = "%s in %s (Stufe %d)" % [
		str(proposal.get("type_name", "")), str(proposal.get("region_name", "")), int(proposal.get("level", 1))
	]
	title.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	title.clip_text = true
	title.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	row.add_child(title)
	var region := int(proposal.get("region", -1))
	var type := int(proposal.get("type", -1))
	var build := _small_button("Bauen", "Auftrag geben: ab morgen in Monatsraten")
	build.name = "Build_%d_%d" % [region, type]
	build.pressed.connect(func() -> void: _order(region, type, str(proposal.get("type_name", "")), str(proposal.get("region_name", ""))))
	row.add_child(build)
	_content.add_child(row)
	_paragraph(effect_text(proposal), true)


## Kosten und Wirkung eines Vorhabens in einem Satz (auch für Tooltips und Tests).
static func effect_text(proposal: Dictionary) -> String:
	var parts := PackedStringArray([
		"Kosten %s in %d Monaten" % [Format.money(float(proposal.get("cost", 0.0))), int(proposal.get("months", 0))],
	])
	var income := float(proposal.get("income_gain", 0.0))
	var taxes := float(proposal.get("tax_gain", 0.0))
	if income > 0.0:
		parts.append("im Jahr +%s Einkommen, +%s Steuern" % [Format.money(income), Format.money(taxes)])
	var jobs := float(proposal.get("jobs", 0.0))
	if jobs >= 1.0:
		parts.append("%s Arbeitsplätze" % Format.integer(roundi(jobs)))
	var literacy := float(proposal.get("literacy", 0.0))
	if literacy > 0.0:
		parts.append("Alphabetisierung +%s Punkte" % Format.number(literacy * 100.0, 0))
	var upkeep := float(proposal.get("upkeep", 0.0))
	if upkeep > 0.0:
		parts.append("Unterhalt %s im Jahr" % Format.money(upkeep))
	return " · ".join(parts)


## Laufender Auftrag mit Fortschritt und „Abbrechen“.
func _order_row(order: Dictionary) -> void:
	var total := maxi(int(order.get("months_total", 1)), 1)
	var left := int(order.get("months_left", 0))
	var row := _bar(
		"%s in %s (Stufe %d)" % [str(order.get("type_name", "")), str(order.get("region_name", "")), int(order.get("level_after", 1))],
		float(total - left) / float(total), UiPalette.GOLD,
		"noch %d %s" % [left, "Monat" if left == 1 else "Monate"],
		"%s im Monat; fertig in %d %s" % [Format.money(float(order.get("monthly_cost", 0.0))), left, "Monat" if left == 1 else "Monaten"]
	)
	var region := int(order.get("region", -1))
	var type := int(order.get("type", -1))
	var cancel := _small_button("Abbrechen", "Auftrag abbrechen; bezahlte Raten sind verloren")
	cancel.name = "Cancel_%d_%d" % [region, type]
	cancel.pressed.connect(
		func() -> void:
			if _sim.cancel_build(_player, region, type):
				order_changed.emit("Bau abgebrochen: %s in %s" % [str(order.get("type_name", "")), str(order.get("region_name", ""))])
	)
	row.add_child(cancel)


## Ein Landesteil: Gebäude mit Stufe, „Bauen“ oder Grund, Wirkung der nächsten Stufe.
func _fill_region() -> void:
	var region := _sim.get_region(_region, _player)
	if region.is_empty():
		_region = -1
		_fill()
		return
	var back := _button("← Alle Landesteile", "Zurück zur Übersicht")
	back.name = "Back"
	back.pressed.connect(func() -> void: show_region(-1))
	_content.add_child(back)
	_heading(str(region.get("name", "")).to_upper())
	_line("Einwohner", Format.integer(int(region.get("population", 0))))
	var show := _button("Auf der Karte zeigen", "Fährt den Landesteil an")
	show.name = "ShowOnMap"
	var first := -1
	for entry: Dictionary in _sim.get_regions(_player):
		if int(entry.get("index", -1)) == _region:
			var locations: PackedInt32Array = entry.get("locations", PackedInt32Array())
			first = locations[0] if not locations.is_empty() else -1
	show.pressed.connect(func() -> void: location_requested.emit(first))
	show.disabled = first < 0
	_content.add_child(show)
	_gap()
	for building: Dictionary in region.get("buildings", []):
		var row := HBoxContainer.new()
		row.add_theme_constant_override(&"separation", 8)
		var label := Label.new()
		var months_left := int(building.get("months_left", -1))
		label.text = "%s – Stufe %d von %d%s" % [
			str(building.get("name", "")), int(building.get("level", 0)), int(building.get("max_level", 0)),
			(" (Bau läuft, noch %d Monate)" % months_left) if months_left >= 0 else "",
		]
		label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		row.add_child(label)
		var problem := str(building.get("problem", ""))
		var type := int(building.get("type", -1))
		var build := _small_button("Bauen", problem if not problem.is_empty() else "Auftrag geben: ab morgen in Monatsraten")
		build.name = "Build_%d_%d" % [_region, type]
		build.disabled = not problem.is_empty()
		var proposal: Dictionary = building.get("proposal", {})
		build.pressed.connect(func() -> void: _order(_region, type, str(building.get("name", "")), str(region.get("name", ""))))
		row.add_child(build)
		_content.add_child(row)
		if int(building.get("level", 0)) < int(building.get("max_level", 0)):
			_paragraph(effect_text(proposal) + (" – " + problem if not problem.is_empty() else ""), true)


func _order(region: int, type: int, type_name: String, region_name: String) -> void:
	if _sim.order_build(_player, region, type):
		order_changed.emit("Bau beauftragt: %s in %s (ab morgen)" % [type_name, region_name])
	else:
		order_changed.emit(_sim.get_build_problem(_player, region, type))
	refresh(true)


func _button(text: String, tooltip: String) -> Button:
	var button := Button.new()
	button.text = text
	button.tooltip_text = tooltip
	button.alignment = HORIZONTAL_ALIGNMENT_LEFT
	button.focus_mode = Control.FOCUS_NONE
	button.theme_type_variation = &"FlatButton"
	return button


func _small_button(text: String, tooltip: String) -> Button:
	var button := Button.new()
	button.text = text
	button.tooltip_text = tooltip
	button.focus_mode = Control.FOCUS_NONE
	return button
