class_name MessageStack
extends VBoxContainer
## Meldekarten (seit M14a3, europa_und_oberflaeche.md 5): kleine Karten mit Symbol unten rechts über der
## Karte, die neueste unten. Sie ersetzen den Ticker (M13a5c) und die Schlachtberichte (M10b3).
##
## Eine Karte zeigt Datum und Art (Krieg, Vertrag, Wahl, Aufstand, Schlacht, Bund, Thron, Zollverein,
## Wendepunkt, Chronik), bei Schlachten mit eigenen Heeren den Titel „Sieg/Niederlage bei …“, und den
## Text der Chronik. Ein Klick fährt den Ort an und legt die Karte weg, × oder Rechtsklick legen sie nur
## weg. Gewöhnliche Karten gehen nach SHOW_SECONDS von selbst (die Zeit steht, solange die Maus über dem
## Stapel ist); wichtige (Wendepunkte, Krieg und Frieden des Spielerstaates) bleiben, bis man sie
## wegklickt. Mehr als MAX_CARDS verdrängen die älteste gewöhnliche Karte. Die Pausen bei wichtigen
## Meldungen regelt main wie bisher.

## Klick auf eine Karte mit Ort: Ort auswählen und anfahren.
signal location_requested(location_index: int)

const MAX_CARDS := 3
const SHOW_SECONDS := 12.0
## Breite der Karten (main passt sie dem Platz neben offenen Fenstern an).
const MAX_WIDTH := 340.0
const MIN_WIDTH := 220.0
const ICON_SIZE := 26.0

## Art → [Symbol, Bezeichnung].
const KINDS := {
	&"krieg": [&"krieg", "Krieg"],
	&"vertrag": [&"vertrag", "Vertrag"],
	&"wahl": [&"wahlen", "Wahl"],
	&"aufstand": [&"unruhe", "Aufstand"],
	&"schlacht": [&"heer", "Schlacht"],
	&"bund": [&"bund", "Bund"],
	&"thron": [&"krone", "Thron"],
	&"zoll": [&"zollunion", "Zollverein"],
	&"wendepunkt": [&"wendepunkt", "Wendepunkt"],
	&"bau": [&"bauen", "Bau"],
	&"chronik": [&"chronik", "Chronik"],
}

## Anlass der Zeitung (data/common/zeitung.chron) → Art der Karte.
const OCCASIONS := {
	"krieg": &"krieg", "krise_krieg": &"krieg", "ultimatum": &"krieg", "mobil": &"krieg",
	"intervention": &"krieg",
	"frieden_sieg": &"vertrag", "frieden_patt": &"vertrag", "buendnis": &"vertrag",
	"wahl": &"wahl", "abstimmung_an": &"wahl", "abstimmung_ab": &"wahl",
	"erhebung": &"aufstand", "barrikaden": &"aufstand", "aufstand_ende": &"aufstand",
	"schlacht": &"schlacht", "schlacht_mehrere": &"schlacht",
	"frage": &"bund", "frage_ende": &"bund", "gruendung": &"bund", "bund_ruht": &"bund", "bund_tagt": &"bund",
	"thron_koenig": &"thron", "thron_kaiser": &"thron", "thron_regent": &"thron",
	"thron_grossherzog": &"thron", "thron_herzog": &"thron", "ministerium": &"thron",
	"kaiserproklamation": &"thron", "kaiserdeputation": &"thron",
	"zollverein": &"zoll",
	"wendepunkt_staat": &"wendepunkt", "wendepunkt_welt": &"wendepunkt",
}

enum Tone { PLAIN, GOOD, BAD, ACCENT }

var _hovered := false


func _init() -> void:
	name = "MessageStack"
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	add_theme_constant_override(&"separation", 6)
	alignment = BoxContainer.ALIGNMENT_END
	visible = false


## Art einer Meldung: zuerst aus dem Anlass der Zeitung, sonst (z. B. 1648 ohne Zeitung) aus dem Text.
static func kind_for(occasion: String, text: String, turning_point: bool = false) -> StringName:
	if turning_point:
		return &"wendepunkt"
	if OCCASIONS.has(occasion):
		return OCCASIONS[occasion]
	if text.begins_with("Schlacht bei "):
		return &"schlacht"
	# Bauen (seit M15b3): Auftrag, Fertigstellung, Verfall.
	if text.contains(" fertig (Stufe ") or text.contains(" verfällt (Stufe ") or text.contains(" baut in "):
		return &"bau"
	if (
		text.contains(" den Krieg") or text.contains("Ultimatum") or text.contains(" wird zum Krieg")
		or text.contains(" ist mobil") or text.contains(" macht mobil")
	):
		return &"krieg"
	if text.begins_with("Frieden ") or text.contains(" Bündnis") or text.contains(" Frieden"):
		return &"vertrag"
	if text.contains("Wahl") or text.contains(" angenommen (") or text.contains(" abgelehnt ("):
		return &"wahl"
	if text.contains("Aufstand") or text.contains("Erhebung") or text.contains("Barrikaden"):
		return &"aufstand"
	if text.contains("Zollverein") or text.contains("Zollunion"):
		return &"zoll"
	return &"chronik"


## Neue Einträge der Chronik ({ date_text, text, location_index, turning_point, occasion, important }).
## Wendepunkte und wichtige Einträge bleiben, bis man sie wegklickt.
func push(entries: Array[Dictionary]) -> void:
	for entry in entries:
		var turning := bool(entry.get("turning_point", false))
		var text := str(entry.get("text", ""))
		var kind := kind_for(str(entry.get("occasion", "")), text, turning)
		var tone := Tone.ACCENT if turning else Tone.PLAIN
		if kind == &"krieg" and bool(entry.get("important", false)):
			tone = Tone.BAD
		add_card({
			"kind": kind, "date_text": str(entry.get("date_text", "")), "text": text,
			"location_index": int(entry.get("location_index", -1)), "tone": tone,
			"sticky": turning or bool(entry.get("important", false)),
		})


## Schlachtbericht mit eigenen Heeren (seit M10b3): Titel „Sieg bei …“ bzw. „Niederlage bei …“.
func add_report(title: String, text: String, location_index: int, victory: bool, date_text: String = "") -> void:
	add_card({
		"kind": &"schlacht", "title": title, "text": text, "date_text": date_text,
		"location_index": location_index, "tone": Tone.GOOD if victory else Tone.BAD, "sticky": false,
	})


## Karte aus { kind, date_text, title?, text, location_index, tone, sticky }.
func add_card(data: Dictionary) -> void:
	var card := _make_card(data)
	add_child(card)
	_trim()
	visible = true


func clear() -> void:
	for card in get_children():
		_remove(card)


func card_count() -> int:
	var count := 0
	for card in get_children():
		if not card.is_queued_for_deletion():
			count += 1
	return count


func card(index: int) -> Control:
	return get_child(index) as Control if index >= 0 and index < get_child_count() else null


func kind_of(index: int) -> StringName:
	var c := card(index)
	return c.get_meta(&"kind", &"") if c != null else &""


func is_sticky(index: int) -> bool:
	var c := card(index)
	return c != null and bool(c.get_meta(&"sticky", false))


## Kopf, Titel und Text aller Karten, zeilenweise (Tests).
func cards_text() -> String:
	var lines := PackedStringArray()
	for c in get_children():
		if c.is_queued_for_deletion():
			continue
		for part in [&"Caption", &"Title", &"Body"]:
			var label := c.find_child(part, true, false) as Label
			if label != null and label.visible:
				lines.append(label.text)
	return "\n".join(lines)


## Karte wie mit einem Klick öffnen (Ort anfahren, weglegen).
func activate(index: int) -> void:
	var c := card(index)
	if c != null:
		_activate(c)


## Karte wegklicken (×).
func dismiss(index: int) -> void:
	var c := card(index)
	if c != null:
		_remove(c)


func _process(delta: float) -> void:
	if not visible:
		return
	_hovered = is_inside_tree() and get_global_rect().has_point(get_global_mouse_position())
	if not _hovered:
		age(delta)


## Lässt die gewöhnlichen Karten um seconds altern; abgelaufene gehen (auch Tests).
func age(seconds: float) -> void:
	for c in get_children():
		if bool(c.get_meta(&"sticky", false)):
			continue
		var left := float(c.get_meta(&"left", SHOW_SECONDS)) - seconds
		c.set_meta(&"left", left)
		if left <= 0.0:
			_remove(c)


## Zu viele Karten: erst die älteste gewöhnliche, sonst die älteste überhaupt.
func _trim() -> void:
	while card_count() > MAX_CARDS:
		var victim: Node = null
		for c in get_children():
			if not c.is_queued_for_deletion() and not bool(c.get_meta(&"sticky", false)):
				victim = c
				break
		if victim == null:
			for c in get_children():
				if not c.is_queued_for_deletion():
					victim = c
					break
		_remove(victim)


func _make_card(data: Dictionary) -> PanelContainer:
	var kind: StringName = data.get("kind", &"chronik")
	if not KINDS.has(kind):
		kind = &"chronik"
	var tone: int = data.get("tone", Tone.PLAIN)
	var location := int(data.get("location_index", -1))
	var card := PanelContainer.new()
	card.name = "Card"
	card.theme_type_variation = &"NoticePanel"
	card.mouse_filter = Control.MOUSE_FILTER_STOP
	card.mouse_default_cursor_shape = Control.CURSOR_POINTING_HAND if location >= 0 else Control.CURSOR_ARROW
	card.set_meta(&"kind", kind)
	card.set_meta(&"sticky", bool(data.get("sticky", false)))
	card.set_meta(&"left", SHOW_SECONDS)
	card.set_meta(&"location", location)
	card.tooltip_text = "Klick: Ort zeigen · Rechtsklick: weglegen" if location >= 0 else "Rechtsklick: weglegen"
	var style := _card_style(tone)
	if style != null:
		card.add_theme_stylebox_override(&"panel", style)
	card.gui_input.connect(_on_card_input.bind(card))
	var row := HBoxContainer.new()
	row.add_theme_constant_override(&"separation", 8)
	row.mouse_filter = Control.MOUSE_FILTER_IGNORE
	card.add_child(row)
	var icon := TextureRect.new()
	icon.name = "Icon"
	icon.texture = Icons.texture(KINDS[kind][0], ICON_SIZE, _tone_color(tone, UiPalette.INK))
	icon.custom_minimum_size = Vector2(ICON_SIZE, ICON_SIZE)
	icon.expand_mode = TextureRect.EXPAND_IGNORE_SIZE
	icon.stretch_mode = TextureRect.STRETCH_KEEP_ASPECT_CENTERED
	icon.size_flags_vertical = Control.SIZE_SHRINK_BEGIN
	icon.mouse_filter = Control.MOUSE_FILTER_IGNORE
	row.add_child(icon)
	var box := VBoxContainer.new()
	box.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	box.add_theme_constant_override(&"separation", 2)
	box.mouse_filter = Control.MOUSE_FILTER_IGNORE
	row.add_child(box)
	var head := HBoxContainer.new()
	head.mouse_filter = Control.MOUSE_FILTER_IGNORE
	box.add_child(head)
	var caption := Label.new()
	caption.name = "Caption"
	var date_text := str(data.get("date_text", ""))
	caption.text = tr(KINDS[kind][1]) if date_text.is_empty() else "%s · %s" % [date_text, tr(KINDS[kind][1])]
	caption.add_theme_font_size_override(&"font_size", 12)
	caption.add_theme_color_override(&"font_color", _tone_color(tone, UiPalette.INK_SOFT))
	caption.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	caption.clip_text = true
	caption.custom_minimum_size = Vector2(1, 0)
	head.add_child(caption)
	var close := Button.new()
	close.name = "Close"
	close.text = "×"
	close.theme_type_variation = &"FlatButton"
	close.focus_mode = Control.FOCUS_NONE
	close.tooltip_text = "Meldung weglegen"
	close.pressed.connect(_remove.bind(card))
	head.add_child(close)
	var title := Label.new()
	title.name = "Title"
	title.theme_type_variation = &"NameLabel"
	title.text = str(data.get("title", ""))
	title.visible = not title.text.is_empty()
	title.clip_text = true
	title.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	title.custom_minimum_size = Vector2(1, 0)
	if tone != Tone.PLAIN:
		title.add_theme_color_override(&"font_color", _tone_color(tone, UiPalette.INK))
	box.add_child(title)
	var body := Label.new()
	body.name = "Body"
	body.text = str(data.get("text", ""))
	body.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	# Höchstens drei Zeilen; ohne Auslassungszeichen (mit ihm meldet Godot nur eine Mindesthöhe von 1 Pixel).
	body.max_lines_visible = 3
	body.custom_minimum_size = Vector2(1, 0)
	body.add_theme_font_size_override(&"font_size", 13)
	body.add_theme_constant_override(&"line_spacing", 0)
	body.mouse_filter = Control.MOUSE_FILTER_IGNORE
	box.add_child(body)
	return card


func _on_card_input(event: InputEvent, card: Control) -> void:
	# Erst beim Loslassen: Die Karte hat den Druck angenommen, so bekommt sie auch das Loslassen und die
	# Karte darunter keinen Klick.
	var click := event as InputEventMouseButton
	if click == null or (click.button_index != MOUSE_BUTTON_LEFT and click.button_index != MOUSE_BUTTON_RIGHT):
		return
	card.accept_event()
	if click.pressed:
		return
	if click.button_index == MOUSE_BUTTON_LEFT:
		_activate(card)
	else:
		_remove(card)


func _activate(card: Control) -> void:
	var location := int(card.get_meta(&"location", -1))
	_remove(card)
	if location >= 0:
		location_requested.emit(location)


func _remove(card: Node) -> void:
	if card == null:
		return
	if card.get_parent() == self:
		remove_child(card)
	card.queue_free()
	visible = get_child_count() > 0


## Farbe der Art: Sieg grün, Niederlage und eigener Krieg rot, Wendepunkt in der Akzentfarbe.
static func _tone_color(tone: int, plain: Color) -> Color:
	match tone:
		Tone.GOOD:
			return Accessibility.adjust(UiPalette.POSITIVE)
		Tone.BAD:
			return UiPalette.NEGATIVE
		Tone.ACCENT:
			return UiPalette.ACCENT
	return plain


## Rand der Karte: links ein Streifen in der Farbe der Art.
func _card_style(tone: int) -> StyleBox:
	var theme := ThemeDB.get_project_theme()
	if theme == null or not theme.has_stylebox(&"panel", &"NoticePanel"):
		return null
	var base := theme.get_stylebox(&"panel", &"NoticePanel") as StyleBoxFlat
	if base == null:
		return null
	var style := base.duplicate() as StyleBoxFlat
	style.border_width_left = 4
	style.border_color = _tone_color(tone, UiPalette.EDGE) if tone != Tone.PLAIN else UiPalette.EDGE
	return style
