class_name PartyPanel
extends PanelContainer
## Parteipanel (seit M8, docs/technik/politik.md Abschnitt 9): Im Parteienmodus steht es in der
## linken Spalte an der Stelle des Staatspanels. Mittel, Spenden und Wahlkampf für die nächste Wahl,
## dazu die Fraktionsansicht der letzten Wahl mit Sitzen und Abgeordneten. Den Wahlkampf selbst führt man in der
## Ortsinfo (Abschnitt „Wahl“). Der Aufbau steht im Code, die Hauptszene legt das Panel an (main.gd).

## Zurück zur Rolle des Staates.
signal leave_requested

## Höhe der Liste der Abgeordneten.
const DEPUTIES_HEIGHT := 180.0

var _swatch: ColorRect = null
var _name: Label = null
var _facts: GridContainer = null
var _result_caption: Label = null
var _result: GridContainer = null
var _hint: Label = null
var _deputies_key := ""
var _deputies_caption: Label = null
var _deputies: VBoxContainer = null
var _deputies_scroll: ScrollContainer = null


func _ready() -> void:
	name = "PartyPanel"
	custom_minimum_size = Vector2(340, 0)
	visible = false
	_build()


## Partei (SimulationClient.get_party) anzeigen: Mittel und Spenden, die nächste Wahl mit dem
## eigenen Wahlkampf (open) und das Ergebnis der letzten Wahl (held), beides auch zugleich (nach der
## Nationalversammlung 1848 vor der Preußischen Zweiten Kammer 1849).
func show_party(party: Dictionary) -> void:
	_swatch.color = party.get("color", UiPalette.BAR_NEUTRAL)
	_name.text = str(party.get("name", "–"))
	_clear(_facts)
	_add_fact(_facts, "Mittel", GermanFormat.money(float(party.get("funds", 0.0))))
	var reserved := float(party.get("reserved", 0.0))
	if reserved > 0.0:
		_add_fact(_facts, "Verplant", "%s (ab morgen)" % GermanFormat.money(reserved))
	_add_fact(_facts, "Spenden", "+%s im Monat" % GermanFormat.money(float(party.get("donations", 0.0))))
	var open := bool(party.get("open", false))
	var held := bool(party.get("held", false))
	if open:
		_add_fact(_facts, "Nächste Wahl", str(party.get("parliament_name", "")))
		_add_fact(_facts, "Wahltag", str(party.get("election_date_text", "")))
		var districts := int(party.get("campaign_districts", 0))
		_add_fact(
			_facts,
			"Wahlkampf",
			"noch in keinem Wahlkreis" if districts == 0
			else ("in 1 Wahlkreis" if districts == 1 else "in %s Wahlkreisen" % GermanFormat.integer(districts))
		)
	_clear(_result)
	_result_caption.visible = held
	_result.visible = held
	var hints := PackedStringArray()
	if open:
		hints.append(
			"Wahlkampf führt Ihr in der Ortsinfo: einen Ort wählen (am besten im Kartenmodus "
			+ "„Wahlen“, Taste Ä), dann Versammlung, Flugblätter oder Verein. Der Stimmungsbericht "
			+ "schätzt die Lage mit großem Fehler."
		)
	if held:
		var parliament := str(party.get("last_parliament_name", ""))
		_result_caption.text = "LETZTE WAHL: %s" % parliament.to_upper()
		var seats := int(party.get("seats", 0))
		var seated := int(party.get("seated", 0))
		_add_fact(
			_result,
			"Sitze",
			"%s von %s (%s)" % [
				GermanFormat.integer(seats),
				GermanFormat.integer(seated),
				GermanFormat.percent(100.0 * seats / seated if seated > 0 else 0.0, 1),
			]
		)
		var faction := str(party.get("faction_name", ""))
		if bool(party.get("factions_formed", false)) and not faction.is_empty():
			_add_fact(
				_result,
				"Fraktion",
				"%s (%d eigene von %d)" % [
					faction, int(party.get("faction_own", 0)), int(party.get("faction_seats", 0))
				]
			)
			if not open:
				hints.append("Ihr führt die Fraktion %s; Abstimmungen im Parlament folgen später." % faction)
		elif not open:
			hints.append("Die Fraktionen bilden sich bis zum %s." % str(party.get("faction_date_text", "")))
	_hint.text = "\n".join(hints)
	_hint.visible = not hints.is_empty()
	_show_deputies(party if held else {})


## Liste der Abgeordneten; nur neu aufgebaut, wenn sich Wahl oder Fraktionsstand ändern (Review M8:
## mehrere Hundert Zeilen kosten bei jedem Tag spürbar Zeit).
func _show_deputies(party: Dictionary) -> void:
	var deputies: Array = party.get("deputies", [])
	var key := "%s|%s|%s|%d" % [
		str(party.get("last_parliament_name", "")), str(party.get("last_election_date_text", "")),
		str(party.get("factions_formed", false)), deputies.size()
	]
	if key != _deputies_key:
		_deputies_key = key
		_clear(_deputies)
		for deputy: Variant in deputies:
			var d := deputy as Dictionary
			var label := Label.new()
			label.text = "%s (%s) · %s" % [
				str(d.get("district_name", "")), str(d.get("state_name", "")), str(d.get("faction_name", ""))
			]
			label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
			label.clip_text = true
			label.custom_minimum_size = Vector2(1, 0)
			_deputies.add_child(label)
	_deputies_caption.text = "ABGEORDNETE (%d)" % deputies.size()
	_deputies_caption.visible = not deputies.is_empty()
	_deputies_scroll.visible = not deputies.is_empty()


## Texte der Angaben (Tests).
func facts_text() -> String:
	var parts := PackedStringArray()
	for grid in [_facts, _result]:
		for child in (grid as GridContainer).get_children():
			parts.append((child as Label).text)
	return " ".join(parts)


func _build() -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 6)
	add_child(box)
	var header := Label.new()
	header.text = "PARTEI"
	header.theme_type_variation = &"HeaderLabel"
	box.add_child(header)
	var identity := HBoxContainer.new()
	identity.add_theme_constant_override(&"separation", 10)
	_swatch = ColorRect.new()
	_swatch.custom_minimum_size = Vector2(22, 22)
	_swatch.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	identity.add_child(_swatch)
	_name = Label.new()
	_name.theme_type_variation = &"NameLabel"
	_name.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_name.custom_minimum_size = Vector2(1, 0)
	_name.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	identity.add_child(_name)
	box.add_child(identity)
	_facts = GridContainer.new()
	_facts.columns = 2
	box.add_child(_facts)
	_result_caption = Label.new()
	_result_caption.theme_type_variation = &"CaptionLabel"
	_result_caption.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_result_caption.custom_minimum_size = Vector2(1, 0)
	box.add_child(_result_caption)
	_result = GridContainer.new()
	_result.columns = 2
	box.add_child(_result)
	_hint = Label.new()
	_hint.theme_type_variation = &"MutedLabel"
	_hint.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_hint.custom_minimum_size = Vector2(1, 0)
	box.add_child(_hint)
	_deputies_caption = Label.new()
	_deputies_caption.theme_type_variation = &"CaptionLabel"
	box.add_child(_deputies_caption)
	_deputies_scroll = ScrollContainer.new()
	_deputies_scroll.custom_minimum_size = Vector2(0, DEPUTIES_HEIGHT)
	_deputies_scroll.horizontal_scroll_mode = ScrollContainer.SCROLL_MODE_DISABLED
	_deputies = VBoxContainer.new()
	_deputies.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	_deputies_scroll.add_child(_deputies)
	box.add_child(_deputies_scroll)
	var leave := Button.new()
	leave.name = "LeaveParty"
	leave.text = "Staat spielen"
	leave.tooltip_text = "Den Parteienmodus verlassen und wieder den Staat spielen"
	leave.focus_mode = Control.FOCUS_NONE
	leave.pressed.connect(func() -> void: leave_requested.emit())
	box.add_child(leave)


func _add_fact(grid: GridContainer, caption: String, value: String) -> void:
	var caption_label := Label.new()
	caption_label.text = caption
	caption_label.theme_type_variation = &"MutedLabel"
	grid.add_child(caption_label)
	var value_label := Label.new()
	value_label.text = value
	value_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	value_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	value_label.custom_minimum_size = Vector2(1, 0)
	value_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	grid.add_child(value_label)


static func _clear(container: Container) -> void:
	for child in container.get_children():
		container.remove_child(child)
		child.queue_free()
