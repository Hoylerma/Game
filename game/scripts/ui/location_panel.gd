class_name LocationPanel
extends PanelContainer
## Ortsinfo: alle Felder aus get_location für den ausgewählten Ort.

## Schließen-Schaltfläche gedrückt.
signal close_requested

## Farben bekannter Konfessionen, solange keine Kartenfarbe vorliegt.
var confession_colors := {}

## Höchste Höhe des Panels (fit_height); unbegrenzt, solange nicht gesetzt.
var _max_height := 100000.0

@onready var _name: Label = %LocationName
@onready var _coat: CoatOfArms = %OwnerCoat
@onready var _owner: Label = %Owner
@onready var _kind: Label = %Kind
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
@onready var _identifier: Label = %Identifier
@onready var _source: Label = %Source
@onready var _close: Button = %Close
@onready var _scroll: ScrollContainer = %Scroll
@onready var _content: MarginContainer = $Scroll/Margin


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
	_name.text = str(location.get("name", "–"))
	_coat.shield_color = owner_color
	_owner.text = str(location.get("owner_name", "–"))
	var is_city := bool(location.get("is_city", false))
	_kind.text = "Stadt mit Umland" if is_city else "Ländlicher Ort"
	_terrain.text = GermanFormat.humanize(str(location.get("terrain", "")))
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
	_show_confessions(confessions)
	_identifier.text = "Kennung: %s" % str(location.get("id", "–"))
	var source := str(location.get("source", ""))
	_source.text = "Quelle: %s" % (source if not source.is_empty() else "keine Angabe")
	visible = true


func _show_confessions(confessions: Dictionary) -> void:
	for child in _confessions.get_children():
		_confessions.remove_child(child)
		child.queue_free()
	var names: Array = confessions.keys()
	names.sort_custom(
		func(a: Variant, b: Variant) -> bool: return float(confessions[a]) > float(confessions[b])
	)
	for confession_name: Variant in names:
		var share := float(confessions[confession_name])
		var row := HBoxContainer.new()
		var label := Label.new()
		label.text = GermanFormat.humanize(str(confession_name))
		label.custom_minimum_size = Vector2(96, 0)
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
		fill.bg_color = confession_colors.get(str(confession_name), UiPalette.BAR_NEUTRAL)
		fill.set_corner_radius_all(2)
		bar.add_theme_stylebox_override(&"fill", fill)
		row.add_child(bar)
		var value := Label.new()
		value.text = GermanFormat.percent(share, 1)
		value.custom_minimum_size = Vector2(62, 0)
		value.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
		row.add_child(value)
		_confessions.add_child(row)
