class_name ElectionNight
extends PanelContainer
## Wahlabend (seit M8, docs/technik/politik.md Abschnitt 8): eine Inszenierung des Ergebnisses, das
## der Kern am Wahltag schon gerechnet hat. Die Wahlkreise „treffen“ in der Meldereihenfolge ein;
## Kopf, Halbrund, Hochrechnung und Telegramme folgen dem Stand. Liegt über der linken Spalte, damit
## die Karte (Kartenmodus „Wahlen“, offene Wahlkreise schraffiert) sichtbar bleibt. Der Aufbau
## steht im Code, die Hauptszene legt den Wahlabend an (main.gd).

## Stand hat sich geändert (Zahl der gemeldeten Wahlkreise).
signal progress_changed(reported: int)
signal close_requested
## Klick auf ein Telegramm: Ort des Wahlkreises auf der Karte zeigen.
signal district_requested(location_index: int)

## Breite der linken Spalte (main.tscn), damit die Modusleiste frei bleibt.
const PANEL_WIDTH := 340.0
## Wahlkreise je Sekunde für die Tempostufen ×1 und ×4; „Sofort“ meldet alles auf einmal.
const RATES: PackedFloat64Array = [10.0, 40.0]
const SPEED_NAMES: PackedStringArray = ["×1", "×4", "Sofort"]
## Zahl der Telegramme (neueste zuerst).
const TELEGRAMS := 6
const HEMICYCLE_HEIGHT := 170.0
## Farbe der Fraktionslosen und noch leerer Sitze.
const INDEPENDENT_COLOR := Color("8a7558")
const OPEN_SEAT_COLOR := Color("ddd0b3")

var _election := {}
var _currents: Array[Dictionary] = []
var _parliament := {}
var _by_faction := false
## Gruppen der Balken und Sitze: { name, color } je Strömung oder Fraktion (zuletzt fraktionslos).
var _groups: Array[Dictionary] = []
var _order := PackedInt32Array()
var _rank_of_district := {}
var _seat_order := PackedInt32Array()
var _reported := 0
var _rate := 0.0
var _accumulator := 0.0
var _tally := {}
var _layout := {}
var _layout_key := Vector3.ZERO

var _title: Label = null
var _subtitle: Label = null
var _status: Label = null
var _hemicycle: Control = null
var _bars: VBoxContainer = null
var _bar_caption: Label = null
var _telegrams: VBoxContainer = null
var _speed_buttons: Array[Button] = []


func _ready() -> void:
	name = "ElectionNight"
	set_anchors_preset(Control.PRESET_LEFT_WIDE)
	offset_left = 12.0
	offset_top = 76.0
	offset_right = 12.0 + PANEL_WIDTH
	offset_bottom = -12.0
	mouse_filter = Control.MOUSE_FILTER_STOP
	visible = false
	_build()


## Öffnet den Wahlabend für election (SimulationClient.get_election). by_faction zeigt Fraktionen
## statt Strömungen (ab dem Fraktionsstichtag); complete zeigt sofort das ganze Ergebnis.
func open(
	election: Dictionary,
	currents: Array[Dictionary],
	parliament: Dictionary,
	by_faction: bool,
	complete: bool
) -> void:
	_election = election
	_currents = currents
	_parliament = parliament
	_by_faction = by_faction
	_groups.clear()
	if by_faction:
		for faction: Variant in parliament.get("factions", []):
			var f := faction as Dictionary
			_groups.append({"name": str(f.get("name", "")), "color": f.get("color", INDEPENDENT_COLOR)})
		_groups.append({"name": "fraktionslos", "color": INDEPENDENT_COLOR})
	else:
		for current in currents:
			_groups.append({"name": str(current.get("name", "")), "color": current.get("color", INDEPENDENT_COLOR)})
	_order = ElectionTally.report_order(election)
	_rank_of_district.clear()
	var districts: Array = election.get("districts", [])
	for k in _order.size():
		_rank_of_district[int((districts[_order[k]] as Dictionary).get("district_index", -1))] = k
	_seat_order = ElectionTally.seat_order(election)
	_layout_key = Vector3.ZERO
	_title.text = str(election.get("parliament_name", "Wahl"))
	var law := str(parliament.get("franchise", ""))
	if bool(parliament.get("indirect", false)):
		law += ", über Wahlmänner"
	_subtitle.text = "Wahltag %s · %s" % [str(election.get("date_text", "")), law]
	_bar_caption.text = (
		"Fraktionen (Stand %s)" % str(parliament.get("faction_date_text", ""))
		if by_faction
		else "Strömungen: gemeldet und Hochrechnung"
	)
	_build_bars()
	_reported = district_count() if complete else 0
	_rate = 0.0 if complete else RATES[0]
	_accumulator = 0.0
	_show_speed(2 if complete else 0)
	visible = true
	_refresh()
	progress_changed.emit(_reported)


func close() -> void:
	visible = false
	_rate = 0.0


func is_open() -> bool:
	return visible


func district_count() -> int:
	return (_election.get("districts", []) as Array).size()


func reported_count() -> int:
	return _reported


func is_complete() -> bool:
	return _reported >= district_count()


## Gemeldete Position (Meldereihenfolge) eines Wahlkreises oder -1.
func rank_of_district(district_index: int) -> int:
	return int(_rank_of_district.get(district_index, -1))


## Stand der Hochrechnung (ElectionTally.tally) für Tests.
func current_tally() -> Dictionary:
	return _tally


## Tempo: 0 = ×1, 1 = ×4, 2 = sofort alles melden.
func set_speed(index: int) -> void:
	_show_speed(index)
	if index >= RATES.size():
		finish()
		return
	_rate = RATES[clampi(index, 0, RATES.size() - 1)]


## Alle Wahlkreise melden.
func finish() -> void:
	_rate = 0.0
	_show_speed(2)
	_advance(district_count())


func _process(delta: float) -> void:
	if not visible or _rate <= 0.0 or is_complete():
		return
	_accumulator += delta * _rate
	var steps := floori(_accumulator)
	if steps > 0:
		_accumulator -= steps
		_advance(steps)


func _advance(steps: int) -> void:
	var before := _reported
	_reported = mini(_reported + steps, district_count())
	if _reported == before:
		return
	if is_complete():
		_rate = 0.0
		_show_speed(2)
	_refresh()
	progress_changed.emit(_reported)


func _refresh() -> void:
	_tally = ElectionTally.tally(_election, _reported, _by_faction, _groups.size())
	var total := district_count()
	var status := "%s von %s Wahlkreisen gemeldet" % [
		GermanFormat.integer(_reported), GermanFormat.integer(total)
	]
	if int(_tally.get("empty", 0)) > 0:
		status += " · %s Sitze leer (Boykott)" % GermanFormat.integer(int(_tally["empty"]))
	if is_complete():
		var counts: PackedInt32Array = _tally.get("counts", PackedInt32Array())
		var best := 0
		for g in counts.size():
			if counts[g] > counts[best]:
				best = g
		status = "Alle %s Wahlkreise gemeldet: %s von %s Sitzen besetzt" % [
			GermanFormat.integer(total),
			GermanFormat.integer(int(_tally.get("seated", 0))),
			GermanFormat.integer(int(_parliament.get("seats", 0))),
		]
		if not counts.is_empty() and counts[best] > 0:
			status += "; stärkste %s: %s mit %s Sitzen" % [
				"Fraktion" if _by_faction else "Strömung",
				str(_groups[best].get("name", "")),
				GermanFormat.integer(counts[best]),
			]
	_status.text = status
	_update_bars()
	_update_telegrams()
	_hemicycle.queue_redraw()


func _build() -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 6)
	add_child(box)

	var head := HBoxContainer.new()
	var caption := Label.new()
	caption.text = "WAHLABEND"
	caption.theme_type_variation = &"CaptionLabel"
	caption.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	head.add_child(caption)
	var close_button := Button.new()
	close_button.text = "Schließen"
	close_button.focus_mode = Control.FOCUS_NONE
	close_button.pressed.connect(func() -> void: close_requested.emit())
	head.add_child(close_button)
	box.add_child(head)

	_title = Label.new()
	_title.theme_type_variation = &"HeaderLabel"
	_title.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_title.custom_minimum_size = Vector2(1, 0)
	box.add_child(_title)
	_subtitle = Label.new()
	_subtitle.theme_type_variation = &"MutedLabel"
	_subtitle.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_subtitle.custom_minimum_size = Vector2(1, 0)
	box.add_child(_subtitle)
	_status = Label.new()
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_status.custom_minimum_size = Vector2(1, 0)
	box.add_child(_status)

	_hemicycle = Control.new()
	_hemicycle.name = "Hemicycle"
	_hemicycle.custom_minimum_size = Vector2(0, HEMICYCLE_HEIGHT)
	_hemicycle.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_hemicycle.draw.connect(_draw_hemicycle)
	box.add_child(_hemicycle)

	_bar_caption = Label.new()
	_bar_caption.theme_type_variation = &"CaptionLabel"
	box.add_child(_bar_caption)
	_bars = VBoxContainer.new()
	_bars.add_theme_constant_override(&"separation", 3)
	box.add_child(_bars)

	var telegram_caption := Label.new()
	telegram_caption.text = "TELEGRAMME"
	telegram_caption.theme_type_variation = &"CaptionLabel"
	box.add_child(telegram_caption)
	_telegrams = VBoxContainer.new()
	_telegrams.add_theme_constant_override(&"separation", 0)
	_telegrams.size_flags_vertical = Control.SIZE_EXPAND_FILL
	box.add_child(_telegrams)

	var controls := HBoxContainer.new()
	controls.add_theme_constant_override(&"separation", 6)
	var tempo := Label.new()
	tempo.text = "Tempo"
	tempo.theme_type_variation = &"MutedLabel"
	controls.add_child(tempo)
	var group := ButtonGroup.new()
	for i in SPEED_NAMES.size():
		var button := Button.new()
		button.text = SPEED_NAMES[i]
		button.toggle_mode = true
		button.button_group = group
		button.focus_mode = Control.FOCUS_NONE
		button.custom_minimum_size = Vector2(56, 28)
		button.pressed.connect(set_speed.bind(i))
		controls.add_child(button)
		_speed_buttons.append(button)
	box.add_child(controls)


func _show_speed(index: int) -> void:
	for i in _speed_buttons.size():
		_speed_buttons[i].set_pressed_no_signal(i == index)


## Eine Zeile je Gruppe: Name, Balken (gemeldet kräftig, Hochrechnung hell, Fehlerband als Strich),
## Zahl.
func _build_bars() -> void:
	for child in _bars.get_children():
		_bars.remove_child(child)
		child.queue_free()
	for g in _groups.size():
		var row := HBoxContainer.new()
		var label := Label.new()
		label.text = str(_groups[g].get("name", ""))
		label.custom_minimum_size = Vector2(100, 0)
		label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		label.clip_text = true
		row.add_child(label)
		var bar := Control.new()
		bar.custom_minimum_size = Vector2(0, 14)
		bar.size_flags_horizontal = Control.SIZE_EXPAND_FILL
		bar.size_flags_vertical = Control.SIZE_SHRINK_CENTER
		bar.draw.connect(_draw_bar.bind(bar, g))
		row.add_child(bar)
		var value := Label.new()
		value.custom_minimum_size = Vector2(104, 0)
		value.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		row.add_child(value)
		row.set_meta(&"bar", bar)
		row.set_meta(&"value", value)
		_bars.add_child(row)


func _update_bars() -> void:
	var counts: PackedInt32Array = _tally.get("counts", PackedInt32Array())
	var projection: PackedFloat64Array = _tally.get("projection", PackedFloat64Array())
	var band: PackedFloat64Array = _tally.get("band", PackedFloat64Array())
	for g in _bars.get_child_count():
		var row := _bars.get_child(g)
		var value := row.get_meta(&"value") as Label
		if g >= counts.size():
			continue
		if is_complete() or _reported == 0:
			value.text = GermanFormat.integer(counts[g])
		else:
			value.text = "%s (≈ %s ± %s)" % [
				GermanFormat.integer(counts[g]),
				GermanFormat.integer(roundi(projection[g])),
				GermanFormat.integer(roundi(band[g])),
			]
		(row.get_meta(&"bar") as Control).queue_redraw()


func _draw_bar(bar: Control, group: int) -> void:
	var counts: PackedInt32Array = _tally.get("counts", PackedInt32Array())
	var projection: PackedFloat64Array = _tally.get("projection", PackedFloat64Array())
	var band: PackedFloat64Array = _tally.get("band", PackedFloat64Array())
	if group >= counts.size():
		return
	var width := bar.size.x
	var height := bar.size.y
	# Maßstab: die Hälfte der Sollzahl füllt den Balken (eine absolute Mehrheit reicht bis zum Ende).
	var scale := width / maxf(float(_parliament.get("seats", 1)) * 0.5, 1.0)
	var color: Color = _groups[group].get("color", INDEPENDENT_COLOR)
	bar.draw_rect(Rect2(0, 0, width, height), Color(UiPalette.PAPER_DEEP, 0.6))
	var projected := minf(projection[group] * scale, width)
	bar.draw_rect(Rect2(0, 0, projected, height), Color(color, 0.35))
	bar.draw_rect(Rect2(0, 0, minf(counts[group] * scale, width), height), color)
	if not is_complete() and band[group] > 0.0:
		var low := clampf((projection[group] - band[group]) * scale, 0.0, width)
		var high := clampf((projection[group] + band[group]) * scale, 0.0, width)
		var mid := height * 0.5
		bar.draw_line(Vector2(low, mid), Vector2(high, mid), UiPalette.INK, 1.0)
		bar.draw_line(Vector2(low, 2), Vector2(low, height - 2), UiPalette.INK, 1.0)
		bar.draw_line(Vector2(high, 2), Vector2(high, height - 2), UiPalette.INK, 1.0)
	# Strich bei der absoluten Mehrheit der Sollzahl.
	bar.draw_line(Vector2(width - 1, 0), Vector2(width - 1, height), UiPalette.INK_FAINT, 1.0)


func _update_telegrams() -> void:
	for child in _telegrams.get_children():
		_telegrams.remove_child(child)
		child.queue_free()
	var districts: Array = _election.get("districts", [])
	if _reported == 0:
		var waiting := Label.new()
		waiting.text = "Noch keine Meldung."
		waiting.theme_type_variation = &"MutedLabel"
		_telegrams.add_child(waiting)
		return
	for k in range(_reported - 1, maxi(_reported - TELEGRAMS, 0) - 1, -1):
		var district := districts[_order[k]] as Dictionary
		var button := Button.new()
		button.text = ElectionTally.telegram(district, _currents)
		button.theme_type_variation = &"FlatButton"
		button.alignment = HORIZONTAL_ALIGNMENT_LEFT
		button.focus_mode = Control.FOCUS_NONE
		button.clip_text = true
		button.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		button.tooltip_text = "Wahlkreis auf der Karte zeigen"
		var location := int(district.get("location_index", -1))
		button.pressed.connect(func() -> void: district_requested.emit(location))
		_telegrams.add_child(button)


## Halbrund: Abgeordnete von links nach rechts nach ihrer Position; gemeldete in der Farbe ihrer
## Gruppe, noch offene hell.
func _draw_hemicycle() -> void:
	var seats := _seat_order.size()
	var size := _hemicycle.size
	var key := Vector3(size.x, size.y, seats)
	if key != _layout_key:
		_layout_key = key
		var radius := minf(size.x * 0.5 - 6.0, size.y - 10.0)
		_layout = ElectionTally.hemicycle(seats, Vector2(size.x * 0.5, size.y - 4.0), radius)
	var points: PackedVector2Array = _layout.get("points", PackedVector2Array())
	var dot := float(_layout.get("dot_size", 4.0)) * 0.5
	var deputy_district: PackedInt32Array = _election.get("deputy_district", PackedInt32Array())
	var faction_count := _groups.size() - 1 if _by_faction else _groups.size()
	for k in mini(points.size(), seats):
		var deputy := _seat_order[k]
		var rank := rank_of_district(deputy_district[deputy])
		var color := OPEN_SEAT_COLOR
		if rank >= 0 and rank < _reported:
			var group := ElectionTally.group_of(_election, deputy, _by_faction, faction_count)
			if group >= 0 and group < _groups.size():
				color = _groups[group].get("color", INDEPENDENT_COLOR)
		_hemicycle.draw_circle(points[k], dot, color)
