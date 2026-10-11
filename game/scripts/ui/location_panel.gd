class_name LocationPanel
extends PanelContainer
## Ortsinfo: alle Felder aus get_location für den ausgewählten Ort.
##
## Orte ohne Daten (has_data = false) zeigen nur die Kartenfelder (Art, Kreis,
## Gelände, Fläche) und einen Hinweis; Bevölkerung, Wirtschaft, Verwüstung,
## Konfessionen, Kulturen und Gesellschaft sind dort ausgeblendet.

## Altersgruppen der Bevölkerung (Feldname in get_location, Anzeigename), seit M4.
const AGE_GROUPS: Array[Array] = [
	["children", "0–14 Jahre"],
	["young", "15–29 Jahre"],
	["working", "30–64 Jahre"],
	["old", "65 und älter"],
]
## Felder der Jahresbilanz (get_location → balance_previous) mit Anzeigename und Vorzeichen.
const BALANCE_FIELDS: Array[Array] = [
	["births", "Geburten", 1],
	["deaths", "Sterbefälle", -1],
	["plague_deaths", "Seuchentote", -1],
	["moved_in", "Zuzug", 1],
	["moved_out", "Wegzug", -1],
	["emigrated", "Auswanderung", -1],
]

## Schließen-Schaltfläche gedrückt.
signal close_requested

## Farben bekannter Konfessionen, solange keine Kartenfarbe vorliegt.
var confession_colors := {}
## Farben der Kulturen (aus dem Kartenmodus Kultur), seit M4.
var culture_colors := {}

## Höchste Höhe des Panels (fit_height); unbegrenzt, solange nicht gesetzt.
var _max_height := 100000.0

@onready var _name: Label = %LocationName
@onready var _coat: CoatOfArms = %OwnerCoat
@onready var _owner: Label = %Owner
@onready var _kind: Label = %Kind
@onready var _kreis: Label = %Kreis
@onready var _no_data_note: Label = %NoDataNote
@onready var _terrain: Label = %Terrain
@onready var _population: Label = %Population
@onready var _area: Label = %Area
@onready var _density: Label = %Density
@onready var _production: Label = %Production
@onready var _taxes: Label = %Taxes
@onready var _devastation_bar: ProgressBar = %DevastationBar
@onready var _devastation: Label = %Devastation
@onready var _majority: Label = %Majority
@onready var _confessions: VBoxContainer = %Confessions
@onready var _majority_culture: Label = %MajorityCulture
@onready var _cultures: VBoxContainer = %Cultures
@onready var _living_standard: Label = %LivingStandard
@onready var _literacy: Label = %Literacy
@onready var _ages: VBoxContainer = %Ages
@onready var _balance: GridContainer = %Balance
@onready var _identifier: Label = %Identifier
@onready var _source: Label = %Source
@onready var _close: Button = %Close
@onready var _scroll: ScrollContainer = %Scroll
@onready var _content: MarginContainer = $Scroll/Margin
## Knoten, die nur für Orte mit Daten erscheinen.
@onready var _data_only: Array[Control] = [
	$Scroll/Margin/Content/Facts/PopulationCaption,
	%Population,
	$Scroll/Margin/Content/Facts/DensityCaption,
	%Density,
	$Scroll/Margin/Content/Facts/ProductionCaption,
	%Production,
	$Scroll/Margin/Content/Facts/TaxesCaption,
	%Taxes,
	$Scroll/Margin/Content/DevastationRow,
	$Scroll/Margin/Content/Separator2,
	$Scroll/Margin/Content/ConfessionRow,
	%Confessions,
	$Scroll/Margin/Content/Separator4,
	$Scroll/Margin/Content/CultureRow,
	%Cultures,
	$Scroll/Margin/Content/Separator5,
	$Scroll/Margin/Content/SocietyHeader,
	$Scroll/Margin/Content/Society,
	$Scroll/Margin/Content/AgeHeader,
	%Ages,
	$Scroll/Margin/Content/BalanceHeader,
	%Balance,
]


func _ready() -> void:
	_close.focus_mode = Control.FOCUS_NONE
	_close.pressed.connect(func() -> void: close_requested.emit())
	var fill := StyleBoxFlat.new()
	fill.bg_color = UiPalette.NEGATIVE
	fill.set_corner_radius_all(2)
	_devastation_bar.add_theme_stylebox_override(&"fill", fill)
	_content.minimum_size_changed.connect(_apply_height, CONNECT_DEFERRED)


## Begrenzt die Höhe des Panels; überzähliger Inhalt wird scrollbar.
func fit_height(max_height: float) -> void:
	_max_height = max_height
	_apply_height()


## Passt die Höhe an; läuft erneut, sobald umbrochene Zeilen ihre Höhe ändern.
func _apply_height() -> void:
	var frame := get_theme_stylebox(&"panel").get_minimum_size().y
	var content := _content.get_combined_minimum_size().y
	var room := maxf(_max_height - frame, 80.0)
	var height := minf(content, room)
	if not is_equal_approx(_scroll.custom_minimum_size.y, height):
		_scroll.custom_minimum_size.y = height
	# Platz für die Bildlaufleiste nur, wenn sie erscheint.
	var margin := 8 if content > room else 0
	if _content.get_theme_constant(&"margin_right") != margin:
		_content.add_theme_constant_override(&"margin_right", margin)


## Ort (get_location) und Farbe seines Besitzers anzeigen.
func show_location(location: Dictionary, owner_color: Color) -> void:
	if location.is_empty():
		visible = false
		return
	var has_data := bool(location.get("has_data", true))
	_name.text = str(location.get("name", "–"))
	_coat.shield_color = owner_color if has_data else UiPalette.PAPER_DEEP
	_owner.text = str(location.get("owner_name", "–")) if has_data else "Keine Daten"
	var is_city := bool(location.get("is_city", false))
	_kind.text = "Stadt mit Umland" if is_city else "Ländlicher Ort"
	var kreis_name := str(location.get("kreis_name", ""))
	_kreis.text = kreis_name if not kreis_name.is_empty() else "–"
	_terrain.text = GermanFormat.humanize(str(location.get("terrain", "")))
	_no_data_note.visible = not has_data
	for node in _data_only:
		node.visible = has_data
	_population.text = GermanFormat.integer(int(location.get("population", 0)))
	_area.text = GermanFormat.area(float(location.get("area_km2", 0.0)))
	_density.text = GermanFormat.density(float(location.get("density", 0.0)))
	_production.text = GermanFormat.money(float(location.get("production_month", 0.0)))
	_taxes.text = GermanFormat.money(float(location.get("taxes_month", 0.0)))
	var devastation := int(location.get("devastation_percent", 0))
	_devastation_bar.value = devastation
	_devastation.text = GermanFormat.percent(devastation)
	_majority.text = "Mehrheit: %s" % GermanFormat.humanize(
		str(location.get("majority_confession", ""))
	)
	var confessions: Dictionary = location.get("confessions", {})
	_show_shares(_confessions, confessions, confession_colors)
	_majority_culture.text = "Mehrheit: %s" % str(location.get("majority_culture", "–"))
	var cultures: Dictionary = location.get("cultures", {})
	_show_shares(_cultures, cultures, culture_colors)
	_living_standard.text = "%s von 100" % GermanFormat.number(
		float(location.get("living_standard", 0.0))
	)
	_literacy.text = GermanFormat.percent(float(location.get("literacy_percent", 0.0)))
	_show_ages(location)
	_show_balance(location.get("balance_previous", {}))
	_identifier.text = "Kennung: %s" % str(location.get("id", "–"))
	var source := str(location.get("source", ""))
	_source.text = "Quelle: %s" % (source if not source.is_empty() else "keine Angabe")
	visible = true


## Anteile (Name → Prozent) als Balken, größter zuerst; Farben je Name, sonst neutral.
func _show_shares(container: VBoxContainer, shares: Dictionary, colors: Dictionary) -> void:
	_clear(container)
	var names: Array = shares.keys()
	names.sort_custom(func(a: Variant, b: Variant) -> bool: return float(shares[a]) > float(shares[b]))
	for share_name: Variant in names:
		var text := GermanFormat.humanize(str(share_name))
		var color: Color = colors.get(str(share_name), UiPalette.BAR_NEUTRAL)
		container.add_child(_share_row(text, float(shares[share_name]), color))


## Altersaufbau: Anteile der vier Altersgruppen an der Bevölkerung.
func _show_ages(location: Dictionary) -> void:
	_clear(_ages)
	var total := 0
	for group in AGE_GROUPS:
		total += int(location.get(group[0], 0))
	for group in AGE_GROUPS:
		var share := 100.0 * int(location.get(group[0], 0)) / total if total > 0 else 0.0
		_ages.add_child(_share_row(str(group[1]), share, UiPalette.BAR_NEUTRAL))


## Bilanz des Vorjahres; im ersten (unvollständigen) Jahr ein Hinweis.
func _show_balance(balance: Dictionary) -> void:
	_clear(_balance)
	var change := 0
	var any := false
	for field in BALANCE_FIELDS:
		var value := int(balance.get(field[0], 0))
		any = any or value != 0
		change += value * int(field[2])
	if not any:
		var note := Label.new()
		note.text = "Noch kein volles Jahr gerechnet."
		note.theme_type_variation = &"CaptionLabel"
		_balance.add_child(note)
		return
	for field in BALANCE_FIELDS:
		var value := int(balance.get(field[0], 0))
		if value == 0 and str(field[0]) == "plague_deaths":
			continue
		_add_fact(_balance, str(field[1]), GermanFormat.signed_number(value * int(field[2])))
	_add_fact(_balance, "Veränderung", GermanFormat.signed_number(change))


func _add_fact(grid: GridContainer, caption: String, value: String) -> void:
	var caption_label := Label.new()
	caption_label.text = caption
	caption_label.theme_type_variation = &"MutedLabel"
	grid.add_child(caption_label)
	var value_label := Label.new()
	value_label.text = value
	value_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	value_label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	grid.add_child(value_label)


func _share_row(text: String, share: float, color: Color) -> HBoxContainer:
	var row := HBoxContainer.new()
	var label := Label.new()
	label.text = text
	label.custom_minimum_size = Vector2(96, 0)
	label.text_overrun_behavior = TextServer.OVERRUN_TRIM_ELLIPSIS
	row.add_child(label)
	var bar := ProgressBar.new()
	bar.show_percentage = false
	bar.min_value = 0.0
	bar.max_value = 100.0
	bar.value = share
	bar.custom_minimum_size = Vector2(0, 12)
	bar.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	bar.size_flags_vertical = Control.SIZE_SHRINK_CENTER
	var fill := StyleBoxFlat.new()
	fill.bg_color = color
	fill.set_corner_radius_all(2)
	bar.add_theme_stylebox_override(&"fill", fill)
	row.add_child(bar)
	var value := Label.new()
	value.text = GermanFormat.percent(share, 1)
	value.custom_minimum_size = Vector2(62, 0)
	value.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	row.add_child(value)
	return row


func _clear(container: Container) -> void:
	for child in container.get_children():
		container.remove_child(child)
		child.queue_free()
