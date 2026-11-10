class_name OverviewPanel
extends PanelContainer
## Rechte Übersicht (seit M13, Schritt a5c; vertical_slice.md 4.1): laufende Dinge des Spielerstaates –
## Krisen, Kriege, offene Ereignisse, Journale mit ihrer Stufe, die nächste Wahl, Heere in Bewegung. Ein
## Klick öffnet das passende Fenster (main.gd). Einklappbar.

## Eintrag gewählt: kind ist crisis, war, event, journal, election oder army; value eine Nummer (Journal,
## Heer) oder −1.
signal entry_requested(kind: StringName, value: int)

const MAX_ENTRIES := 12

var _collapsed := false
var _header: Button = null
var _list: VBoxContainer = null
var _entries: Array[Dictionary] = []


func _ready() -> void:
	name = "OverviewPanel"
	mouse_filter = Control.MOUSE_FILTER_STOP
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 2)
	add_child(box)
	_header = Button.new()
	_header.name = "Header"
	_header.flat = true
	_header.alignment = HORIZONTAL_ALIGNMENT_LEFT
	_header.focus_mode = Control.FOCUS_NONE
	_header.theme_type_variation = &"CaptionLabel"
	_header.pressed.connect(func() -> void: set_collapsed(not _collapsed))
	box.add_child(_header)
	_list = VBoxContainer.new()
	_list.add_theme_constant_override(&"separation", 1)
	box.add_child(_list)
	_update_header()


func set_collapsed(collapsed: bool) -> void:
	_collapsed = collapsed
	_list.visible = not collapsed
	_update_header()


func is_collapsed() -> bool:
	return _collapsed


## Einträge: je { kind, value, text, tooltip, urgent }; leer blendet die Übersicht aus.
func show_entries(entries: Array[Dictionary]) -> void:
	_entries = entries
	visible = not entries.is_empty()
	for child in _list.get_children():
		_list.remove_child(child)
		child.queue_free()
	for i in mini(entries.size(), MAX_ENTRIES):
		var entry := entries[i]
		var b := Button.new()
		b.name = "Entry%d" % i
		b.flat = true
		b.alignment = HORIZONTAL_ALIGNMENT_LEFT
		b.focus_mode = Control.FOCUS_NONE
		b.clip_text = true
		b.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
		b.text = str(entry.get("text", ""))
		b.tooltip_text = str(entry.get("tooltip", ""))
		b.add_theme_font_size_override(&"font_size", 13)
		if bool(entry.get("urgent", false)):
			b.add_theme_color_override(&"font_color", UiPalette.ACCENT)
		var kind: StringName = entry.get("kind", &"")
		var value := int(entry.get("value", -1))
		b.pressed.connect(func() -> void: entry_requested.emit(kind, value))
		_list.add_child(b)
	_update_header()


## Texte der Einträge (Tests).
func entry_texts() -> PackedStringArray:
	var result := PackedStringArray()
	for entry in _entries:
		result.append(str(entry.get("text", "")))
	return result


## Knopf eines Eintrags (Tests).
func entry_button(index: int) -> Button:
	return _list.get_node_or_null("Entry%d" % index) as Button


func _update_header() -> void:
	if _header != null:
		_header.text = "%s ÜBERSICHT (%d)" % ["▸" if _collapsed else "▾", _entries.size()]


## Einträge aus den Daten des Kerns (main.gd ruft das höchstens zweimal je Sekunde).
static func collect(sim: SimulationClient, player: int, player_tag: String) -> Array[Dictionary]:
	var entries: Array[Dictionary] = []
	for event in sim.get_open_events():
		entries.append({
			"kind": &"event", "value": int(event.get("event_index", -1)), "urgent": true,
			"text": "Ereignis: %s" % str(event.get("title", "")),
			"tooltip": "Entscheidung offen, noch %d Tage" % int(event.get("days_left", 0)),
		})
	for play in sim.get_diplomatic_plays():
		var ours := int(play.get("demander_index", -1)) == player or int(play.get("target_index", -1)) == player
		entries.append({
			"kind": &"crisis", "value": -1, "urgent": ours,
			"text": "Krise: %s fordert von %s" % [str(play.get("demander_name", "")), str(play.get("target_name", ""))],
			"tooltip": "%s – %s, noch %d Tage" % [str(play.get("kind_text", "")), str(play.get("stage_text", "")), int(play.get("days_left", 0))],
		})
	for war in sim.get_wars():
		var side := int(war.get("player_side", 0))
		entries.append({
			"kind": &"war", "value": int(war.get("id", -1)), "urgent": side != 0,
			"text": "Krieg: %s" % str(war.get("name", "")),
			"tooltip": "seit %s, %d Schlachten" % [str(war.get("started_text", "")), int(war.get("battles", 0))],
		})
	for journal in sim.get_journals():
		if bool(journal.get("started", false)) and not bool(journal.get("finished", false)):
			entries.append({
				"kind": &"journal", "value": int(journal.get("index", 0)), "urgent": false,
				"text": "%s: %s" % [str(journal.get("title", "")), str(journal.get("stage_title", ""))],
				"tooltip": "Stufe seit %s" % str(journal.get("since_text", "")),
			})
	var next: Dictionary = {}
	for parliament in sim.get_parliaments():
		if bool(parliament.get("pending", false)) and (
			next.is_empty()
			or ElectionTally.day_number(parliament.get("election_date", {})) < ElectionTally.day_number(next.get("election_date", {}))
		):
			next = parliament
	if not next.is_empty():
		entries.append({
			"kind": &"election", "value": int(next.get("index", -1)), "urgent": false,
			"text": "Wahl: %s am %s" % [str(next.get("name", "")), str(next.get("election_date_text", ""))],
			"tooltip": "nächste ausstehende Wahl",
		})
	for army in sim.get_armies(player):
		if int(army.get("destination_index", -1)) >= 0 and int(army.get("days_left", 0)) > 0 and not bool(army.get("garrison", false)):
			entries.append({
				"kind": &"army", "value": int(army.get("id", -1)), "urgent": false,
				"text": "%s: noch %d Tage" % [str(army.get("name", "")), int(army.get("days_left", 0))],
				"tooltip": "%s marschiert (%s)" % [str(army.get("name", "")), str(army.get("status_text", ""))],
			})
	return entries
