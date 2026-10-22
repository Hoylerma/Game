class_name PlayBar
extends PanelContainer
## Krisenleiste (seit M9, Schritt b3, docs/technik/diplomatie.md Abschnitte 4.5 und 6): ein laufendes
## diplomatisches Spiel über der Karte – Forderung, Seiten mit Stärke, Stufe und verbleibende Tage. Ist
## der Spielerstaat Hauptpartei, gibt es die Manöver Nachgeben, Vermittlung anbieten und Eskalieren.
## Ohne laufendes Spiel ist die Leiste unsichtbar.

## Manöver des Spielerstaates (SimulationClient.MANOEUVRE_*).
signal manoeuvre_requested(kind: int)

const WIDTH := 620.0

var _title: Label = null
var _stage: Label = null
var _sides: Label = null
var _buttons: HBoxContainer = null
var _yield: Button = null
var _mediate: Button = null
var _escalate: Button = null
var _note: Label = null


func _ready() -> void:
	name = "PlayBar"
	custom_minimum_size = Vector2(WIDTH, 0)
	visible = false
	mouse_filter = Control.MOUSE_FILTER_STOP
	_build()


## Zeigt das Spiel des Spielerstaates, sonst das erste laufende; versteckt sich ohne Spiel.
func show_plays(plays: Array[Dictionary], player: int) -> void:
	var play: Dictionary = {}
	for entry in plays:
		if int(entry.get("demander_index", -1)) == player or int(entry.get("target_index", -1)) == player:
			play = entry
			break
	if play.is_empty() and not plays.is_empty():
		play = plays[0]
	visible = not play.is_empty()
	if play.is_empty():
		_note.text = ""
		return
	var demander := int(play.get("demander_index", -1))
	var target := int(play.get("target_index", -1))
	_title.text = "KRISE: %s fordert von %s: %s" % [
		str(play.get("demander_name", "")), str(play.get("target_name", "")), str(play.get("kind_text", ""))
	]
	var days := int(play.get("days_left", 0))
	_stage.text = "Stufe %d von 3: %s – %s" % [
		int(play.get("stage", 1)),
		str(play.get("stage_text", "")),
		"Entscheidung heute" if days <= 0 else ("noch 1 Tag" if days == 1 else "noch %d Tage" % days),
	]
	_sides.text = "%s: %s Soldaten%s  ·  %s: %s Soldaten%s" % [
		str(play.get("demander_name", "")),
		GermanFormat.integer(int(play.get("demander_strength", 0))),
		_supporters(play.get("demander_side", PackedStringArray())),
		str(play.get("target_name", "")),
		GermanFormat.integer(int(play.get("target_strength", 0))),
		_supporters(play.get("target_side", PackedStringArray())),
	]
	var own := player == demander or player == target
	_buttons.visible = own
	if not own:
		return
	var is_demander := player == demander
	var escalates := bool(play.get("demander_escalates" if is_demander else "target_escalates", false))
	var mediation := str(play.get("demander_mediation" if is_demander else "target_mediation", ""))
	_escalate.disabled = escalates
	_escalate.text = "Eskaliert" if escalates else "Eskalieren"
	_mediate.tooltip_text = (
		"Vermittlung anbieten: Kompromiss mit halber Wirkung. Die Gegenseite nimmt an, wenn unsere Seite "
		+ "mindestens zwei Drittel ihrer Stärke hat – "
		+ ("das ist der Fall." if mediation == "angenommen" else "das ist nicht der Fall, sie lehnt ab.")
	)


## Rückmeldung nach einem Manöver (main.gd).
func show_note(text: String) -> void:
	_note.text = text


## Text der Leiste (für Tests).
func bar_text() -> String:
	return "\n".join(PackedStringArray([_title.text, _stage.text, _sides.text, _note.text]))


func _supporters(names: PackedStringArray) -> String:
	if names.is_empty():
		return ""
	if names.size() > 4:
		return " (mit %s, %s und %d weiteren)" % [names[0], names[1], names.size() - 2]
	return " (mit %s)" % ", ".join(names)


func _build() -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 4)
	add_child(box)
	_title = Label.new()
	_title.theme_type_variation = &"HeaderLabel"
	_title.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_title.custom_minimum_size = Vector2(WIDTH - 30.0, 0)
	box.add_child(_title)
	_stage = Label.new()
	box.add_child(_stage)
	_sides = Label.new()
	_sides.theme_type_variation = &"MutedLabel"
	_sides.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_sides.custom_minimum_size = Vector2(WIDTH - 30.0, 0)
	box.add_child(_sides)
	_buttons = HBoxContainer.new()
	_buttons.alignment = BoxContainer.ALIGNMENT_END
	_buttons.add_theme_constant_override(&"separation", 6)
	_yield = _button("Yield", "Nachgeben", "Die Krise sofort beenden: Die Gegenseite setzt sich durch.")
	_yield.pressed.connect(func() -> void: manoeuvre_requested.emit(SimulationClient.MANOEUVRE_YIELD))
	_mediate = _button("Mediate", "Vermittlung anbieten", "")
	_mediate.pressed.connect(func() -> void: manoeuvre_requested.emit(SimulationClient.MANOEUVRE_MEDIATE))
	_escalate = _button(
		"Escalate",
		"Eskalieren",
		"Am Ende dieser Stufe weiter eskalieren, auch wenn unsere Seite schwächer ist (sonst bietet sie Vermittlung an)."
	)
	_escalate.pressed.connect(func() -> void: manoeuvre_requested.emit(SimulationClient.MANOEUVRE_ESCALATE))
	box.add_child(_buttons)
	_note = Label.new()
	_note.theme_type_variation = &"MutedLabel"
	box.add_child(_note)


func _button(button_name: String, caption: String, tooltip: String) -> Button:
	var button := Button.new()
	button.name = button_name
	button.text = caption
	button.tooltip_text = tooltip
	button.focus_mode = Control.FOCUS_NONE
	_buttons.add_child(button)
	return button
