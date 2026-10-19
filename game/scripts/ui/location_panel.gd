class_name LocationPanel
extends PanelContainer
## Ortsinfo: alle Felder aus get_location für den ausgewählten Ort.
##
## Orte ohne Daten (has_data = false) zeigen nur die Kartenfelder (Art, Kreis,
## Gelände, Fläche) und einen Hinweis; Bevölkerung, Wirtschaft, Verwüstung,
## Konfessionen, Kulturen und Gesellschaft sind dort ausgeblendet.

## Höchstzahl der Güter im Abschnitt Wirtschaft (nach Wert der Erzeugung, seit M5).
const MAX_GOODS := 6

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
## „Diesen Staat spielen“ gedrückt (seit M7): Besitzer des Ortes als Spielerstaat.
signal play_state_requested(state_index: int)
## Wahlkampf der eigenen Strömung im Wahlkreis (seit M8, Parteienmodus).
signal campaign_requested(district: int, action: int)

## Farben bekannter Konfessionen, solange keine Kartenfarbe vorliegt.
var confession_colors := {}
## Farben der Kulturen (aus dem Kartenmodus Kultur), seit M4.
var culture_colors := {}
## Zeile „Deutscher Bund“ zeigen (seit M7: nur in Szenarien mit Bundesgebiet, main.gd).
var show_federation := false
## Spielerstaat; der Knopf „Diesen Staat spielen“ erscheint bei Orten anderer Staaten (seit M7).
var player_index := -1
## Parteienmodus (seit M8, set_party_mode): Abschnitt Wahl ganz oben, kein „Diesen Staat spielen“.
var party_mode := false

var _play_button: Button = null
## Abschnitt Wahl (seit M8): Wahlkreis und Ergebnis der letzten Wahl.
var _election_box: VBoxContainer = null
var _owner_index := -1

## Höchste Höhe des Panels (fit_height); unbegrenzt, solange nicht gesetzt.
var _max_height := 100000.0

@onready var _name: Label = %LocationName
@onready var _coat: CoatOfArms = %OwnerCoat
@onready var _owner: Label = %Owner
@onready var _kind: Label = %Kind
@onready var _kreis: Label = %Kreis
@onready var _admin_caption: Label = %AdminCaption
@onready var _admin: Label = %Admin
@onready var _federal_caption: Label = %FederalCaption
@onready var _federal: Label = %Federal
@onready var _rail_caption: Label = %RailCaption
@onready var _rail: Label = %Rail
@onready var _no_data_note: Label = %NoDataNote
@onready var _terrain: Label = %Terrain
@onready var _population: Label = %Population
@onready var _area: Label = %Area
@onready var _density: Label = %Density
@onready var _production: Label = %Production
@onready var _taxes: Label = %Taxes
@onready var _devastation_bar: ProgressBar = %DevastationBar
@onready var _devastation: Label = %Devastation
@onready var _economy: VBoxContainer = %Economy
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
	$Scroll/Margin/Content/EconomySeparator,
	$Scroll/Margin/Content/EconomyHeader,
	%Economy,
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
	_play_button = Button.new()
	_play_button.name = "PlayState"
	_play_button.text = "Diesen Staat spielen"
	_play_button.focus_mode = Control.FOCUS_NONE
	_play_button.tooltip_text = "Den Besitzer dieses Ortes als eigenen Staat übernehmen"
	_play_button.pressed.connect(func() -> void: play_state_requested.emit(_owner_index))
	var facts := $Scroll/Margin/Content/Facts
	facts.get_parent().add_child(_play_button)
	facts.get_parent().move_child(_play_button, facts.get_index() + 1)
	_election_box = VBoxContainer.new()
	_election_box.name = "Election"
	_election_box.add_theme_constant_override(&"separation", 4)
	# Gleich unter den Eckdaten: Im Kartenmodus „Wahlen“ ist das Ergebnis der Grund des Klicks.
	facts.get_parent().add_child(_election_box)
	facts.get_parent().move_child(_election_box, _play_button.get_index() + 1)


## Parteienmodus ein- oder ausschalten: Der Wahlkampf ist dann der Grund des Klicks, der Abschnitt
## Wahl rückt vor die Eckdaten.
func set_party_mode(on: bool) -> void:
	party_mode = on
	var parent := _election_box.get_parent()
	var anchor: Control = $Scroll/Margin/Content/Facts if on else _play_button
	var index := anchor.get_index() + (0 if on else 1)
	# move_child zählt mit dem Kasten selbst: Steht er vor dem Ziel, rückt das Ziel um eins vor.
	if _election_box.get_index() < index:
		index -= 1
	parent.move_child(_election_box, index)


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
	_owner_index = int(location.get("owner_index", -1))
	_play_button.visible = (
		has_data and _owner_index >= 0 and _owner_index != player_index and not party_mode
	)
	var is_city := bool(location.get("is_city", false))
	_kind.text = "Stadt mit Umland" if is_city else "Ländlicher Ort"
	var kreis_name := str(location.get("kreis_name", ""))
	_kreis.text = kreis_name if not kreis_name.is_empty() else "–"
	# Verwaltung (seit M7): unterste Einheit zuerst, z. B. „Salzburgkreis · Österreich ob der Enns“.
	var chain := PackedStringArray()
	for unit: Variant in location.get("verwaltung", []):
		if unit is Dictionary:
			chain.insert(0, str((unit as Dictionary).get("name", "")))
	_admin.text = " · ".join(chain)
	_admin_caption.visible = not chain.is_empty()
	_admin.visible = not chain.is_empty()
	# Die Kreise aus M3 sind räumliche Gruppen der Karte, keine historischen Kreise: neben der
	# Verwaltung heißen sie deshalb „Kartenkreis“.
	$Scroll/Margin/Content/Facts/KreisCaption.text = "Kartenkreis" if not chain.is_empty() else "Kreis"
	var federal := bool(location.get("bundesgebiet", false))
	_federal.text = "Bundesgebiet" if federal else "außerhalb"
	_federal_caption.visible = show_federation and has_data
	_federal.visible = show_federation and has_data
	# Eisenbahn (seit M7): Strecken mit eröffnetem Abschnitt an diesem Ort, mit Jahr.
	var lines: PackedStringArray = location.get("eisenbahnen", PackedStringArray())
	_rail.text = ", ".join(lines)
	_rail_caption.visible = not lines.is_empty()
	_rail.visible = not lines.is_empty()
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
	_show_economy(location)
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
	_show_election(location.get("wahl", {}))
	_identifier.text = "Kennung: %s" % str(location.get("id", "–"))
	var source := str(location.get("source", ""))
	_source.text = "Quelle: %s" % (source if not source.is_empty() else "keine Angabe")
	visible = true


## Wahlkampf im Wahlkreis des Ortes (seit M8, Parteienmodus): Stimmungsbericht mit Fehler, Stand des
## eigenen Wahlkampfs, Knöpfe je Art. campaign aus SimulationClient.get_campaign, currents aus
## get_currents. Nach show_location aufrufen; ohne offene Wahl nichts.
func show_campaign(district: int, campaign: Dictionary, currents: Array[Dictionary], party_name: String) -> void:
	if campaign.is_empty() or not bool(campaign.get("open", false)) or not _election_box.visible:
		return
	var box := VBoxContainer.new()
	box.name = "Campaign"
	box.add_theme_constant_override(&"separation", 4)
	_election_box.add_child(box)
	box.add_child(_caption("STIMMUNGSBERICHT (± %s PUNKTE)" % GermanFormat.number(
		float(campaign.get("poll_error", 0.0)), 0
	)))
	if bool(campaign.get("boycott", false)):
		var note := Label.new()
		note.text = "Die Mehrheit boykottiert die Wahl; dieser Wahlkreis bleibt wohl leer."
		note.theme_type_variation = &"MutedLabel"
		note.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
		note.custom_minimum_size = Vector2(1, 0)
		box.add_child(note)
	var poll: PackedFloat64Array = campaign.get("poll", PackedFloat64Array())
	var shares := {}
	var colors := {}
	for c in mini(poll.size(), currents.size()):
		var current_name := str(currents[c].get("name", ""))
		shares[current_name] = poll[c]
		colors[current_name] = currents[c].get("color", UiPalette.BAR_NEUTRAL)
	var poll_box := VBoxContainer.new()
	box.add_child(poll_box)
	_show_shares(poll_box, shares, colors)
	box.add_child(_caption("WAHLKAMPF: %s" % party_name.to_upper()))
	var levels := Label.new()
	levels.theme_type_variation = &"MutedLabel"
	levels.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	levels.custom_minimum_size = Vector2(1, 0)
	levels.text = "Mobilisierung +%s, Vereine +%s, Flugblätter %s" % [
		GermanFormat.percent(roundi(float(campaign.get("mobilization", 0.0)) * 100.0)),
		GermanFormat.percent(roundi(float(campaign.get("club", 0.0)) * 100.0)),
		GermanFormat.number(float(campaign.get("persuasion", 0.0)), 2),
	]
	box.add_child(levels)
	for entry: Variant in campaign.get("actions", []):
		var action := entry as Dictionary
		var button := Button.new()
		button.name = "Campaign%d" % int(action.get("action", 0))
		button.text = "%s – %s" % [
			str(action.get("name", "")), GermanFormat.money(float(action.get("cost", 0.0)))
		]
		button.focus_mode = Control.FOCUS_NONE
		button.disabled = not bool(action.get("allowed", false))
		button.tooltip_text = (
			"Wirkt ab dem nächsten Tag bis zum Wahltag, mit abnehmendem Ertrag"
			if not button.disabled
			else "Die Mittel reichen nicht"
		)
		var kind := int(action.get("action", 0))
		button.pressed.connect(func() -> void: campaign_requested.emit(district, kind))
		box.add_child(button)


## Wahlkreis und Ergebnis der letzten Wahl (seit M8); leer außerhalb des Wahlgebiets.
func _show_election(election: Dictionary) -> void:
	_clear(_election_box)
	_election_box.visible = not election.is_empty()
	if election.is_empty():
		return
	_election_box.add_child(HSeparator.new())
	_election_box.add_child(_caption("WAHL"))
	var grid := GridContainer.new()
	grid.columns = 2
	_election_box.add_child(grid)
	_add_fact(grid, "Parlament", str(election.get("parliament_name", "")))
	var district := str(election.get("district_name", ""))
	var seats := int(election.get("seats", 1))
	if seats > 1:
		district += " (%d Sitze)" % seats
	_add_fact(grid, "Wahlkreis", district)
	if not bool(election.get("held", false)):
		_add_fact(grid, "Wahl am", str(election.get("election_date_text", "")))
		return
	if bool(election.get("boycott", false)):
		_add_fact(grid, "Ergebnis", "Boykott, kein Abgeordneter")
		return
	var winner := str(election.get("winner_name", "–"))
	if bool(election.get("runoff", false)):
		winner += " (Stichwahl)"
	_add_fact(grid, "Gewählt", winner)
	_add_fact(grid, "Berechtigte", GermanFormat.integer(int(election.get("electorate", 0))))
	_add_fact(grid, "Beteiligung", GermanFormat.percent(float(election.get("turnout_percent", 0.0)), 1))
	var shares_box := VBoxContainer.new()
	_election_box.add_child(shares_box)
	_show_shares(shares_box, election.get("shares", {}), election.get("colors", {}))


## Anteile (Name → Prozent) als Balken, größter zuerst; Farben je Name, sonst neutral.
func _show_shares(container: VBoxContainer, shares: Dictionary, colors: Dictionary) -> void:
	_clear(container)
	var names: Array = shares.keys()
	names.sort_custom(func(a: Variant, b: Variant) -> bool: return float(shares[a]) > float(shares[b]))
	for share_name: Variant in names:
		var text := GermanFormat.humanize(str(share_name))
		var color: Color = colors.get(str(share_name), UiPalette.BAR_NEUTRAL)
		container.add_child(_share_row(text, float(shares[share_name]), color))


## Wirtschaft (seit M5): Markt, Landesausbau, Berufe mit Einkommen und Bedarfsdeckung,
## Betriebe und die wichtigsten Güter mit Preis gegenüber dem Basispreis.
func _show_economy(location: Dictionary) -> void:
	_clear(_economy)
	var facts := GridContainer.new()
	facts.columns = 2
	var market := str(location.get("market_name", ""))
	if bool(location.get("is_port", false)):
		market += " · Hafen"
	_add_fact(facts, "Markt", market if not market.is_empty() else "–")
	var development := float(location.get("development_percent", 0.0))
	if development > 0.05:
		_add_fact(
			facts, "Landesausbau", "+%s Tragfähigkeit" % GermanFormat.percent(development, 1)
		)
	_economy.add_child(facts)

	var professions: Array = location.get("professions", [])
	if not professions.is_empty():
		_economy.add_child(_caption("Einkommen je Kopf und Jahr · Bedarfsdeckung"))
		var grid := GridContainer.new()
		grid.columns = 3
		for row: Dictionary in professions:
			var name_label := Label.new()
			name_label.text = str(row.get("name", ""))
			name_label.theme_type_variation = &"MutedLabel"
			name_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			grid.add_child(name_label)
			grid.add_child(_right(GermanFormat.money(float(row.get("income_year", 0.0)))))
			var coverage := _right(GermanFormat.percent(float(row.get("coverage_percent", 0.0))))
			coverage.custom_minimum_size = Vector2(52, 0)
			var food := float(row.get("food_percent", 100.0))
			if food < 90.0:
				coverage.add_theme_color_override(&"font_color", UiPalette.NEGATIVE)
				coverage.tooltip_text = "Grundnahrung nur zu %s gedeckt" % GermanFormat.percent(food)
				coverage.mouse_filter = Control.MOUSE_FILTER_PASS
			grid.add_child(coverage)
		_economy.add_child(grid)

	var businesses: Array = location.get("businesses", [])
	if not businesses.is_empty():
		_economy.add_child(_caption("Betriebe (Köpfe)"))
		var grid := GridContainer.new()
		grid.columns = 2
		for row: Dictionary in businesses:
			_add_fact(
				grid,
				"%s (%s)" % [str(row.get("name", "")), str(row.get("profession", ""))],
				GermanFormat.integer(int(row.get("people", 0)))
			)
		_economy.add_child(grid)

	var goods: Array = location.get("goods", []).duplicate()
	goods.sort_custom(
		func(a: Dictionary, b: Dictionary) -> bool:
			return (
				float(a.get("supply", 0.0)) * float(a.get("price", 0.0))
				> float(b.get("supply", 0.0)) * float(b.get("price", 0.0))
			)
	)
	if not goods.is_empty():
		_economy.add_child(_caption("Erzeugung je Monat · Preis zum Basispreis"))
		var grid := GridContainer.new()
		grid.columns = 3
		for row: Dictionary in goods.slice(0, MAX_GOODS):
			var name_label := Label.new()
			name_label.text = str(row.get("name", ""))
			name_label.theme_type_variation = &"MutedLabel"
			name_label.size_flags_horizontal = Control.SIZE_EXPAND_FILL
			grid.add_child(name_label)
			grid.add_child(_right(GermanFormat.number(float(row.get("supply", 0.0)), 1)))
			var factor := _right("×" + GermanFormat.number(float(row.get("price_factor", 0.0)), 2))
			factor.custom_minimum_size = Vector2(52, 0)
			grid.add_child(factor)
		_economy.add_child(grid)


func _caption(text: String) -> Label:
	var label := Label.new()
	label.text = text
	label.theme_type_variation = &"CaptionLabel"
	return label


func _right(text: String) -> Label:
	var label := Label.new()
	label.text = text
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_RIGHT
	return label


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
	# Lange Werte (Marktnamen) umbrechen statt das Panel zu verbreitern.
	value_label.custom_minimum_size = Vector2(1, 0)
	value_label.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
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
