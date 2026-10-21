extends SceneTree
## Rauchtest der Brücke und der Kartendaten (Godot-Seite, Fassung M6).
##
## Aufruf aus dem Repository-Hauptordner (nach einem Import des Projekts):
##   godot --headless --path game --script res://tests/smoke_test.gd
## Optionen nach "--": --ohne-karte (Kartenprüfung überspringen).
## Datenordner: $UHRWERK_DATEN, sonst data/ im Repository.
## Ergebnis: "SMOKE OK" und Exitcode 0, sonst Fehlermeldungen und Exitcode 1.

const DAYS_TO_SIMULATE := 365
const STATE_FIELDS: PackedStringArray = [
	"tag",
	"name",
	"short_name",
	"color",
	"ruler",
	"government",
	"capital_index",
	"treasury",
	"tax_rate_percent",
	"income_month",
	"expenses_month",
	"population",
	"location_count",
	"balance_previous",
	"living_standard",
	"tax_rates_percent",
	"taxes_month",
	"court_month",
	"administration_month",
	"army_month",
	"interest_month",
	"building_month",
	"debt",
	"interest_percent",
	"yearly_income",
	"army_per_mille",
	"soldiers",
	"bankruptcies",
	"premium_months",
	"zollverein",
	"bund",
]
const LOCATION_FIELDS: PackedStringArray = [
	"id",
	"name",
	"owner_index",
	"owner_name",
	"terrain",
	"is_city",
	"population",
	"area_km2",
	"density",
	"devastation_percent",
	"majority_confession",
	"confessions",
	"production_month",
	"taxes_month",
	"source",
	"has_data",
	"kreis_index",
	"kreis_name",
	"majority_culture",
	"cultures",
	"children",
	"young",
	"working",
	"old",
	"living_standard",
	"literacy_percent",
	"balance_previous",
	"market_index",
	"market_name",
	"is_port",
	"development_percent",
	"production_per_capita",
	"professions",
	"businesses",
	"goods",
	"verwaltung",
	"bundesgebiet",
]
const MARKET_FIELDS: PackedStringArray = [
	"id", "name", "location_count", "active", "is_port", "harvest", "goods"
]
const MARKET_GOOD_FIELDS: PackedStringArray = [
	"id", "name", "color", "base_price", "price", "price_factor", "supply", "demand", "imports",
	"exports", "buy_percent"
]
const KREIS_FIELDS: PackedStringArray = ["id", "name", "location_count", "source"]
## Kartenmodi, in denen Orte ohne Daten Alpha 0 haben (bridge-api.md); im Marktmodus (9)
## nur Orte ohne Daten in Märkten ohne Orte mit Daten.
const NO_DATA_ALPHA_MODES: PackedInt32Array = [0, 1, 2, 4, 6, 7, 8, 10, 11, 12]
const CHRONICLE_FIELDS: PackedStringArray = ["date_text", "text", "location_index", "state_index"]

var _failures := PackedStringArray()
var _check_map := true


func _initialize() -> void:
	_check_map = not OS.get_cmdline_user_args().has("--ohne-karte")
	print("EINIGKEIT – Rauchtest der Godot-Seite")
	print("Datenordner: %s (%s)" % [UhrwerkPaths.data_dir(), UhrwerkPaths.data_dir_origin()])
	_run()
	if _failures.is_empty():
		print("SMOKE OK")
		quit(0)
	else:
		printerr("SMOKE FEHLGESCHLAGEN: %d Fehler" % _failures.size())
		for failure in _failures:
			printerr("  - ", failure)
		quit(1)


func _run() -> void:
	var library := SimulationClient.expected_library_file()
	if not _check(
		ClassDB.class_exists(SimulationClient.NATIVE_CLASS),
		"Klasse UhrwerkSimulation registriert",
		"Klasse UhrwerkSimulation fehlt – ist game/%s gebaut?" % library
	):
		return
	var missing := SimulationClient.missing_methods()
	if not _check(
		missing.is_empty(),
		"alle Methoden der Schnittstelle vorhanden",
		"fehlende Methoden: " + ", ".join(missing)
	):
		return

	var sim := SimulationClient.create()
	if not _check(sim != null, "Simulation erzeugt", "UhrwerkSimulation ließ sich nicht erzeugen"):
		return
	_check_before_load(sim)

	var broken := SimulationClient.create()
	var broken_errors := broken.load_scenario("/gibt/es/nicht/einigkeit-daten")
	_check(
		not broken_errors.is_empty() and not broken.is_loaded(),
		"falscher Datenordner wird mit Fehlermeldung abgelehnt",
		"falscher Datenordner lieferte keinen Fehler"
	)

	var errors := sim.load_scenario(UhrwerkPaths.data_dir())
	if not _check(
		errors.is_empty() and sim.is_loaded(), "Daten geladen", "Laden fehlgeschlagen:\n" + errors
	):
		return
	_check_counts(sim)
	var player := sim.get_player_state_index()
	var start := sim.get_date()
	var max_tax := sim.get_max_tax_rate_percent()
	_check(max_tax > 0, "Höchststeuersatz %d %%" % max_tax, "Höchststeuersatz ist %d" % max_tax)
	_check_invalid_indices(sim, max_tax, player)
	_check_map_colors(sim)
	_check_data_and_kreise(sim)
	_check_economy(sim, player)
	_check_railways_1648(sim)

	# Steuerbefehle: wirksam am nächsten Tag; Kontribution über den alten Befehl, Akzise über
	# set_tax_percent (seit M5).
	var old_tax := int(sim.get_state(player).get("tax_rate_percent", -1))
	var new_tax := old_tax + 2 if old_tax + 2 <= max_tax else maxi(old_tax - 2, 0)
	_check(
		sim.set_tax_rate_percent(player, new_tax),
		"Steuerbefehl angenommen",
		"set_tax_rate_percent(%d, %d) lieferte false" % [player, new_tax]
	)
	var old_rates: Array = sim.get_state(player).get("tax_rates_percent", [0, 0, 0])
	var new_excise := int(old_rates[SimulationClient.TAX_AKZISE]) + 1
	_check(
		sim.set_tax_percent(player, SimulationClient.TAX_AKZISE, new_excise),
		"Akzise-Befehl angenommen",
		"set_tax_percent(%d, Akzise, %d) lieferte false" % [player, new_excise]
	)
	sim.advance_days(1)
	var tax_after := int(sim.get_state(player).get("tax_rate_percent", -1))
	_check(
		tax_after == new_tax,
		"Steuersatz nach einem Tag %d %% (vorher %d %%)" % [tax_after, old_tax],
		"Steuersatz nach einem Tag %d statt %d" % [tax_after, new_tax]
	)
	var rates_after: Array = sim.get_state(player).get("tax_rates_percent", [])
	_check(
		rates_after.size() == 3
		and int(rates_after[SimulationClient.TAX_KONTRIBUTION]) == new_tax
		and int(rates_after[SimulationClient.TAX_AKZISE]) == new_excise
		and int(rates_after[SimulationClient.TAX_ZOLL]) == int(old_rates[SimulationClient.TAX_ZOLL]),
		"Sätze nach einem Tag %s" % str(rates_after),
		"Sätze nach einem Tag %s, erwartet Kontribution %d und Akzise %d" % [
			str(rates_after), new_tax, new_excise
		]
	)
	var excise_entry := false
	for entry in sim.get_chronicle(0):
		excise_entry = excise_entry or str(entry["text"]).contains("legt die Akzise auf")
	_check(excise_entry, "Chronik meldet die neue Akzise", "kein Chronikeintrag zur Akzise")

	sim.advance_days(DAYS_TO_SIMULATE - 1)
	_check_date(sim, start)
	_check_after_simulation(sim, player)
	_check_chronicle(sim)
	_check_determinism(sim, player, new_tax, new_excise)
	_check_savegames(sim, player)
	_check_scenario_1848()
	if _check_map:
		_check_map_files(sim)
	else:
		print("  –   Kartenprüfung übersprungen (--ohne-karte)")


## Lesezeichen 1848 (seit M7): Szenarioliste, Staatswahl, Verwaltung, Bundesgebiet, Zollverein.
func _check_scenario_1848() -> void:
	var sim := SimulationClient.create()
	var scenarios := sim.get_scenarios(UhrwerkPaths.data_dir())
	var ids := PackedStringArray()
	for scenario in scenarios:
		ids.append(str(scenario.get("id", "")))
	_check(
		ids.has("westfaelischer_frieden") and ids.has("voelkerfruehling"),
		"Szenarien ohne Laden gelesen: %s" % ", ".join(ids),
		"get_scenarios lieferte %s" % ", ".join(ids)
	)
	var errors := sim.load_scenario(UhrwerkPaths.data_dir(), "voelkerfruehling")
	if not _check(errors.is_empty(), "Szenario 1848 geladen", "Szenario 1848: " + errors):
		return
	var scenario := sim.get_scenario()
	_check(
		str(scenario.get("currency", "")) == "Taler"
		and PackedStringArray(scenario.get("playable", PackedStringArray())).has("BAD"),
		"Szenario 1848: Währung Taler, Baden spielbar",
		"Szenario 1848: %s" % str(scenario)
	)
	var prussia := sim.get_state_index("PRU")
	var state := sim.get_state(prussia)
	_check(
		bool(state.get("zollverein", false)) and bool(state.get("bund", false)),
		"Preußen im Zollverein und im Deutschen Bund",
		"Preußen: zollverein %s, bund %s" % [state.get("zollverein"), state.get("bund")]
	)
	var berlin := sim.get_location(sim.get_location_index("berlin_coelln"))
	var chain: Array = berlin.get("verwaltung", [])
	_check(
		not chain.is_empty() and str(chain[0].get("id", "")) == "pr_brandenburg"
		and bool(berlin.get("bundesgebiet", false)),
		"Berlin: Provinz Brandenburg, Bundesgebiet",
		"Berlin: verwaltung %s, bundesgebiet %s" % [str(chain), berlin.get("bundesgebiet")]
	)
	var posen := sim.get_location(sim.get_location_index("posen"))
	_check(
		not bool(posen.get("bundesgebiet", true)),
		"Posen außerhalb des Bundesgebiets",
		"Posen im Bundesgebiet"
	)
	var units := sim.get_admin_unit_count()
	var admin := sim.get_location_admin_indices()
	_check(
		units > 0 and admin.size() == sim.get_location_count(),
		"%d Verwaltungseinheiten, Index je Ort" % units,
		"Verwaltung: %d Einheiten, %d Indizes" % [units, admin.size()]
	)
	var unit := sim.get_admin_unit(admin[sim.get_location_index("berlin_coelln")])
	var district := sim.get_admin_unit(int(unit.get("parent_index", -1)))
	_check(
		str(unit.get("kind", "")) == "kreis" and int(unit.get("location_count", 0)) > 0
		and str(district.get("id", "")) == "pr_rb_potsdam" and chain.size() == 3,
		"get_admin_unit: %s im %s (%d Orte)" % [
			unit.get("name"), district.get("name"), unit.get("location_count")
		],
		"get_admin_unit: %s, darüber %s, Kette %s" % [str(unit), str(district), str(chain)]
	)
	var bavaria := sim.get_state_index("BAV")
	_check(
		sim.set_player_state(bavaria) and sim.get_player_state_index() == bavaria
		and not sim.set_player_state(-1),
		"Spielerstaat gewechselt (Bayern)",
		"set_player_state hat den Spieler nicht gewechselt"
	)
	_check_railways_1848(sim)
	_check_elections_1848(sim)


## Wahlen 1848 (seit M8): Parlamente, Strömungen, Wahlkreise je Ort, Kartenmodus, die Wahl am 1. Mai
## mit Ergebnis je Wahlkreis und Abgeordneten, Ortsinfo.
func _check_elections_1848(sim: SimulationClient) -> void:
	var parliaments := sim.get_parliaments()
	var currents := sim.get_currents()
	var national: Dictionary = parliaments[0] if not parliaments.is_empty() else {}
	_check(
		parliaments.size() == 2 and currents.size() == 5
		and int(national.get("seats", 0)) == 649 and int(national.get("district_count", 0)) == 633
		and (national.get("factions", []) as Array).size() == 8
		and int(national.get("election_index", 0)) == -1 and sim.get_shown_parliament() == 0,
		"Parlamente 1848: %s mit %d Sitzen, %d Wahlkreisen; %d Strömungen" % [
			national.get("name"), national.get("seats"), national.get("district_count"), currents.size()
		],
		"Parlamente 1848: %d, %s; Strömungen %d" % [parliaments.size(), str(national), currents.size()]
	)
	var districts := sim.get_location_electoral_district_indices(0)
	var in_district := 0
	for d in districts:
		in_district += 1 if d >= 0 else 0
	var colors := sim.get_map_colors(MapModes.MAP_WAHLEN)
	_check(
		districts.size() == sim.get_location_count() and in_district > 3000
		and colors.size() == sim.get_location_count()
		and sim.get_location_electoral_district_indices(5).is_empty(),
		"Wahlkreise je Ort: %d Orte in Wahlkreisen der Nationalversammlung" % in_district,
		"Wahlkreise je Ort: %d Einträge, %d in Wahlkreisen" % [districts.size(), in_district]
	)
	_check_state_politics_1848(sim)
	var liberals := _check_party_1848(sim, currents)
	sim.advance_days(62)
	var count := sim.get_election_count()
	var election := sim.get_election(0) if count > 0 else {}
	var results: Array = election.get("districts", [])
	var ranks := {}
	var seats_sum := 0
	for r: Variant in results:
		ranks[int((r as Dictionary).get("report_rank", -1))] = true
	for seats in (election.get("seats_by_current", PackedInt32Array()) as PackedInt32Array):
		seats_sum += seats
	var seated := int(election.get("seated", 0))
	_check(
		count == 1 and results.size() == 633 and ranks.size() == 633 and ranks.has(0) and ranks.has(632)
		and seated == seats_sum and seated > 500
		and (election.get("deputy_position", PackedFloat32Array()) as PackedFloat32Array).size() == seated
		and int(election.get("boycott_seats", 0)) + seated == 649,
		"Wahl %s: %d von 649 Sitzen besetzt, Meldereihenfolge vollständig" % [
			election.get("date_text"), seated
		],
		"Wahl 1848: %d Wahlen, %d Wahlkreise, %d Ränge, %d Sitze (Summe %d), leer %d" % [
			count, results.size(), ranks.size(), seated, seats_sum, int(election.get("boycott_seats", 0))
		]
	)
	var mannheim: Dictionary = sim.get_location(sim.get_location_index("mannheim")).get("wahl", {})
	var shares: Dictionary = mannheim.get("shares", {})
	var total := 0.0
	for value: Variant in shares.values():
		total += float(value)
	_check(
		bool(mannheim.get("held", false)) and not str(mannheim.get("winner_name", "")).is_empty()
		and shares.size() == 5 and absf(total - 100.0) < 0.01
		and float(mannheim.get("turnout_percent", 0.0)) > 20.0,
		"Ortsinfo Mannheim: Wahlkreis %s, gewählt %s" % [
			mannheim.get("district_name"), mannheim.get("winner_name")
		],
		"Ortsinfo Mannheim: %s" % str(mannheim)
	)
	var legend := LegendData.election_entries(sim)
	_check(
		legend.any(func(entry: Dictionary) -> bool: return str(entry.get("text", "")) == "Boykott"),
		"Legende Wahlen: %d Einträge mit Boykott" % legend.size(),
		"Legende Wahlen: %s" % str(legend)
	)
	var party := sim.get_party(liberals)
	var deputies: Array = party.get("deputies", [])
	var by_current: PackedInt32Array = election.get("seats_by_current", PackedInt32Array())
	_check(
		bool(party.get("held", false)) and liberals >= 0 and liberals < by_current.size()
		and int(party.get("seats", -1)) == by_current[liberals] and deputies.size() == by_current[liberals]
		and int(party.get("seated", 0)) == seated
		and not bool(sim.get_campaign(sim.get_location_electoral_district_indices(0)[
			sim.get_location_index("mannheim")
		], liberals).get("open", true)),
		"Partei nach der Wahl: %d Sitze, Wahlkampf beendet" % int(party.get("seats", 0)),
		"Partei nach der Wahl: %s" % str(party)
	)


## Verfassung und Interessengruppen 1848 (seit M8, politik.md 3.3): Baden konstitutionell mit
## Zensuswahlrecht; zehn Gruppen (die Arbeiterbewegung erst ab August), Anteil und Einfluss in Prozent.
func _check_state_politics_1848(sim: SimulationClient) -> void:
	var baden := sim.get_state_politics(sim.get_state_index("BAD"))
	var groups: Array = baden.get("interest_groups", [])
	var valid := groups.size() == 10
	var influence := 0.0
	for entry: Variant in groups:
		var group := entry as Dictionary
		var share := float(group.get("share_percent", -1.0))
		var weight := float(group.get("influence_percent", -1.0))
		valid = valid and share >= 0.0 and share <= 100.0 and weight >= 0.0 and weight <= 100.0
		influence += weight
	_check(
		valid and influence > 0.0 and str(baden.get("constitution", "")) == "konstitutionell"
		and str(baden.get("franchise", "")) == "Zensuswahlrecht" and bool(baden.get("elected", false))
		and float(baden.get("census_threshold", 0.0)) > 0.0
		and bool(sim.get_state(sim.get_state_index("BAD")).get("politik", false))
		and sim.get_state_politics(-1).is_empty() and sim.get_state_politics(sim.get_state_count()).is_empty(),
		"Verfassung und Gruppen Baden: %s, %s, %d Gruppen" % [
			baden.get("constitution"), baden.get("franchise"), groups.size()
		],
		"Verfassung und Gruppen Baden: %s" % str(baden)
	)


## Parteienmodus 1848 (seit M8): Mittel und Spenden, Wahlkampf als Befehl für den nächsten Tag,
## Stimmungsbericht, Rolle im Spielstand. Gibt den Index der Liberalen zurück.
func _check_party_1848(sim: SimulationClient, currents: Array[Dictionary]) -> int:
	var liberals := -1
	for i in currents.size():
		if str(currents[i].get("id", "")) == "liberale":
			liberals = i
	sim.advance_days(1) # Spenden am Monatsersten
	var party := sim.get_party(liberals)
	var funds := float(party.get("funds", 0.0))
	_check(
		liberals >= 0 and funds > 1000.0 and is_equal_approx(funds, float(party.get("donations", 0.0)))
		and not bool(party.get("held", true)) and int(party.get("campaign_districts", -1)) == 0
		and sim.get_party(-1).is_empty() and sim.get_party(currents.size()).is_empty(),
		"Partei %s: %s Taler Mittel, Wahl %s" % [
			party.get("name"), GermanFormat.integer(roundi(funds)), party.get("election_date_text")
		],
		"Partei: %s" % str(party)
	)
	var district := sim.get_location_electoral_district_indices(0)[sim.get_location_index("mannheim")]
	var campaign := sim.get_campaign(district, liberals)
	var poll: PackedFloat64Array = campaign.get("poll", PackedFloat64Array())
	var poll_total := 0.0
	for value in poll:
		poll_total += value
	var actions: Array = campaign.get("actions", [])
	_check(
		bool(campaign.get("open", false)) and poll.size() == currents.size()
		and absf(poll_total - 100.0) < 0.01 and actions.size() == 3
		and float(campaign.get("poll_error", 0.0)) > 0.0
		and actions.all(func(a: Dictionary) -> bool: return bool(a.get("allowed", false))),
		"Stimmungsbericht %s: ± %s Punkte, %d Arten" % [
			campaign.get("district_name"), campaign.get("poll_error"), actions.size()
		],
		"Stimmungsbericht: %s" % str(campaign)
	)
	var rally_cost := float((actions[SimulationClient.CAMPAIGN_VERSAMMLUNG] as Dictionary).get("cost", 0.0))
	_check(
		sim.campaign(liberals, district, SimulationClient.CAMPAIGN_VERSAMMLUNG)
		and not sim.campaign(liberals, -1, SimulationClient.CAMPAIGN_VERSAMMLUNG)
		and not sim.campaign(liberals, district, 3) and not sim.campaign(-1, district, 0),
		"Wahlkampf angesetzt (Versammlung in %s)" % campaign.get("district_name"),
		"Wahlkampf: Befehle falsch angenommen oder abgelehnt"
	)
	sim.advance_days(1)
	party = sim.get_party(liberals)
	campaign = sim.get_campaign(district, liberals)
	_check(
		is_equal_approx(float(party.get("funds", 0.0)), funds - rally_cost)
		and int(party.get("campaign_districts", 0)) == 1 and float(campaign.get("mobilization", 0.0)) > 0.0,
		"Wahlkampf am nächsten Tag: Mittel %s, Mobilisierung %s" % [
			GermanFormat.integer(roundi(float(party.get("funds", 0.0)))), campaign.get("mobilization")
		],
		"Wahlkampf am nächsten Tag: %s, %s" % [str(party), str(campaign)]
	)
	# Rolle im Spielstand (Metadaten „spielerstroemung“).
	var dir := OS.get_user_data_dir().path_join("smoke_test_partei")
	_remove_dir(dir)
	DirAccess.make_dir_recursive_absolute(dir)
	var path := dir.path_join("partei.einigkeit")
	var role_ok := sim.set_player_current(liberals) and sim.get_player_current() == liberals
	role_ok = role_ok and not sim.set_player_current(currents.size())
	var error := sim.save_game(path, sim.get_player_state_index())
	var info := sim.get_save_info(path)
	role_ok = role_ok and sim.set_player_current(-1) and sim.get_player_current() == -1
	error += sim.load_game(UhrwerkPaths.data_dir(), path)
	_check(
		role_ok and error.is_empty() and str(info.get("player_current", "")) == "liberale"
		and sim.get_player_current() == liberals,
		"Rolle im Spielstand: Partei %s" % str(info.get("player_current", "")),
		"Rolle im Spielstand: %s, Fehler „%s“, geladen %d" % [str(info), error, sim.get_player_current()]
	)
	_remove_dir(dir)
	sim.set_player_current(-1)
	return liberals


## 1648 gibt es die Strecken als Inhalt, aber noch keine Eröffnung (Review M7).
func _check_railways_1648(sim: SimulationClient) -> void:
	var access := sim.get_rail_access()
	var any := false
	for value in access:
		any = any or value != 0
	var legend := LegendData.railway_entries(sim)
	var detail := str(legend[0].get("detail", "")) if not legend.is_empty() else ""
	_check(
		access.size() == sim.get_location_count() and not any and detail.contains("noch keine Eisenbahn"),
		"Eisenbahnen 1648: kein Bahnhof, Legende „%s“" % detail,
		"Eisenbahnen 1648: %d Orte, Bahn %s, Legende %s" % [access.size(), str(any), str(legend)]
	)


## Eisenbahnen 1848 (seit M7): Strecken, Stufen je Ort wie die Farben, Legende, Ortsinfo.
func _check_railways_1848(sim: SimulationClient) -> void:
	var railways := sim.get_railways()
	var access := sim.get_rail_access()
	var counts := [0, 0, 0]
	for value in access:
		counts[clampi(value, 0, 2)] += 1
	var sections: Array = (railways[0] as Dictionary).get("sections", []) if not railways.is_empty() else []
	_check(
		access.size() == sim.get_location_count() and counts[2] > 0 and counts[1] > 0
		and not sections.is_empty() and (sections[0] as Dictionary).has("opened"),
		"Eisenbahnen 1848: %d Strecken, %d Bahnhöfe, %d Orte mit Bahnfracht" % [
			railways.size(), counts[2], counts[1]
		],
		"Eisenbahnen 1848: %d Strecken, Stufen %s" % [railways.size(), str(counts)]
	)
	var legend := LegendData.railway_entries(sim)
	var leipzig: PackedStringArray = sim.get_location(sim.get_location_index("leipzig")).get(
		"eisenbahnen", PackedStringArray()
	)
	_check(
		not legend.is_empty() and str(legend[0].get("text", "")) == "Bahnhof" and not leipzig.is_empty(),
		"Legende Eisenbahnen, Leipzig: %s" % ", ".join(leipzig),
		"Legende %s, Leipzig %s" % [str(legend), str(leipzig)]
	)


func _check_before_load(sim: SimulationClient) -> void:
	_check(
		(
			not sim.is_loaded()
			and sim.get_state_count() == 0
			and sim.get_location_count() == 0
			and sim.get_state(0).is_empty()
			and sim.get_location(0).is_empty()
			and sim.get_map_colors(MapModes.MAP_POLITICAL).is_empty()
			and sim.get_chronicle_size() == 0
			and sim.get_location_owner_indices().is_empty()
			and sim.get_location_kreis_indices().is_empty()
			and sim.get_kreis_count() == 0
			and sim.get_kreis(0).is_empty()
		),
		"vor dem Laden leere Abfragen",
		"vor dem Laden liefern Abfragen schon Werte"
	)


func _check_counts(sim: SimulationClient) -> void:
	var states := sim.get_state_count()
	var locations := sim.get_location_count()
	var player := sim.get_player_state_index()
	_check(states > 0, "%d Staaten" % states, "keine Staaten geladen")
	_check(locations > 0, "%d Orte" % locations, "keine Orte geladen")
	_check(
		player >= 0 and player < states,
		"Spielerstaat %d" % player,
		"Spielerstaat-Index %d ungültig" % player
	)
	var scenario := sim.get_scenario_name()
	_check(not scenario.is_empty(), "Szenario „%s“" % scenario, "Szenarioname leer")

	var state := sim.get_state(player)
	for key in STATE_FIELDS:
		_check(state.has(key), "", "get_state ohne Feld „%s“" % key)
	var state_population := int(state.get("population", 0))
	var state_locations := int(state.get("location_count", 0))
	_check(
		state_population > 0 and state_locations > 0,
		(
			"Spielerstaat %s: %s Einwohner in %d Orten"
			% [str(state.get("tag", "")), GermanFormat.integer(state_population), state_locations]
		),
		"Spielerstaat ohne Bevölkerung oder Orte"
	)

	var total_population := 0
	var total_area := 0.0
	for index in locations:
		var location := sim.get_location(index)
		for key in LOCATION_FIELDS:
			if not location.has(key):
				_check(false, "", "get_location(%d) ohne Feld „%s“" % [index, key])
				return
		total_population += int(location["population"])
		total_area += float(location["area_km2"])
		var id := str(location["id"])
		if sim.get_location_index(id) != index:
			_check(false, "", "get_location_index(„%s“) liefert nicht %d" % [id, index])
	_check(
		total_population > 0 and total_area > 0.0,
		(
			"Orte: %s Einwohner auf %s km²"
			% [GermanFormat.integer(total_population), GermanFormat.number(total_area)]
		),
		"Orte ohne Bevölkerung oder Fläche"
	)
	var date := sim.get_date()
	_check(
		int(date.get("year", 0)) > 0 and not sim.get_date_text().is_empty(),
		"Startdatum %s" % sim.get_date_text(),
		"Startdatum fehlt"
	)


func _check_invalid_indices(sim: SimulationClient, max_tax: int, player: int) -> void:
	var rejected := (
		sim.get_state(-1).is_empty()
		and sim.get_state(sim.get_state_count()).is_empty()
		and sim.get_location(-1).is_empty()
		and sim.get_location(sim.get_location_count()).is_empty()
		and sim.get_location_index("gibt_es_nicht") == -1
		and sim.get_state_index("???") == -1
		and sim.get_kreis(-1).is_empty()
		and sim.get_kreis(sim.get_kreis_count()).is_empty()
		and sim.get_kreis_index("gibt_es_nicht") == -1
		and not sim.set_tax_rate_percent(-1, 10)
		and not sim.set_tax_rate_percent(sim.get_state_count(), 10)
		and not sim.set_tax_rate_percent(player, max_tax + 1)
		and not sim.set_tax_rate_percent(player, -1)
		and not sim.set_tax_percent(player, -1, 10)
		and not sim.set_tax_percent(player, SimulationClient.TAX_KIND_COUNT, 10)
		and not sim.set_tax_percent(player, SimulationClient.TAX_ZOLL, max_tax + 1)
		and sim.get_market(-1).is_empty()
		and sim.get_market(sim.get_market_count()).is_empty()
		# 1648 ohne Parlament: keine Angaben zu Verfassung und Gruppen (seit M8).
		and sim.get_state_politics(player).is_empty()
		and not bool(sim.get_state(player).get("politik", true))
	)
	_check(
		rejected,
		"ungültige Indizes und Befehle werden abgewiesen",
		"ungültige Indizes oder Befehle werden nicht abgewiesen"
	)
	# Darf nicht abstürzen; das Ergebnis wird nur angezeigt.
	var odd_colors := sim.get_map_colors(99).size()
	print("  ok  unbekannter Kartenmodus liefert %d Farben" % odd_colors)


func _check_map_colors(sim: SimulationClient) -> void:
	var expected := sim.get_location_count()
	for mode in MapModes.COUNT:
		var count := sim.get_map_colors(mode).size()
		_check(
			count == expected,
			"Kartenmodus %d (%s): %d Farben" % [mode, MapModes.NAMES[mode], count],
			"Kartenmodus %d liefert %d statt %d Farben" % [mode, count, expected]
		)


## Orte ohne Daten und Kreise (seit M3): Indexarrays, get_kreis, Alpha der Kartenfarben.
func _check_data_and_kreise(sim: SimulationClient) -> void:
	var count := sim.get_location_count()
	var owners := sim.get_location_owner_indices()
	var kreise := sim.get_location_kreis_indices()
	if not _check(
		owners.size() == count and kreise.size() == count,
		"Indexarrays für Besitzer und Kreise mit %d Einträgen" % count,
		"get_location_owner_indices/get_location_kreis_indices: %d/%d statt %d Einträge"
		% [owners.size(), kreise.size(), count]
	):
		return
	var kreis_count := sim.get_kreis_count()
	var kreis_names := PackedStringArray()
	var kreis_members := PackedInt32Array()
	kreis_members.resize(kreis_count)
	for kreis in kreis_count:
		var entry := sim.get_kreis(kreis)
		for key in KREIS_FIELDS:
			if not entry.has(key):
				_check(false, "", "get_kreis(%d) ohne Feld „%s“" % [kreis, key])
				return
		kreis_names.append(str(entry["name"]))
		if sim.get_kreis_index(str(entry["id"])) != kreis:
			_check(false, "", "get_kreis_index(„%s“) liefert nicht %d" % [entry["id"], kreis])
	var with_data := 0
	var without_kreis := 0
	var mismatches := PackedStringArray()
	for index in count:
		var location := sim.get_location(index)
		var has_data := bool(location["has_data"])
		var owner := int(location["owner_index"])
		var kreis := int(location["kreis_index"])
		if has_data:
			with_data += 1
		if owners[index] != owner or (has_data != (owner >= 0)):
			mismatches.append("%s: Besitzer %d/%d, Daten %s" % [
				location["id"], owners[index], owner, has_data
			])
		if kreise[index] != kreis or kreis >= kreis_count:
			mismatches.append("%s: Kreis %d/%d" % [location["id"], kreise[index], kreis])
		elif kreis < 0:
			without_kreis += 1
			if not str(location["kreis_name"]).is_empty():
				mismatches.append("%s: kreis_name ohne Kreis" % location["id"])
		else:
			kreis_members[kreis] += 1
			if str(location["kreis_name"]) != kreis_names[kreis]:
				mismatches.append("%s: kreis_name „%s“" % [location["id"], location["kreis_name"]])
		if not has_data and (
			int(location["population"]) != 0 or not str(location["owner_name"]).is_empty()
		):
			mismatches.append("%s: ohne Daten, aber mit Bevölkerung oder Besitzer" % location["id"])
	_check(
		mismatches.is_empty(),
		(
			"%d Orte mit Daten, %d ohne Daten; Indexarrays passen zu get_location"
			% [with_data, count - with_data]
		),
		"Abweichungen bei Orten ohne Daten oder Kreisen: " + "; ".join(mismatches.slice(0, 10))
	)
	_check(with_data > 0, "", "kein Ort hat Daten")
	var counts_ok := true
	for kreis in kreis_count:
		if int(sim.get_kreis(kreis)["location_count"]) != kreis_members[kreis]:
			counts_ok = false
	_check(
		counts_ok,
		"%d Kreise, %d Orte ohne Kreis; location_count stimmt" % [kreis_count, without_kreis],
		"location_count der Kreise passt nicht zu den Orten"
	)
	var market_of := sim.get_location_market_indices()
	var active_market := {}
	for market in sim.get_market_count():
		if bool(sim.get_market(market).get("active", false)):
			active_market[market] = true
	for mode in MapModes.COUNT:
		var colors := sim.get_map_colors(mode)
		var wrong := 0
		for index in mini(colors.size(), count):
			var expect_clear := NO_DATA_ALPHA_MODES.has(mode) and owners[index] < 0
			if mode == MapModes.MAP_MAERKTE:
				expect_clear = owners[index] < 0 and not active_market.has(market_of[index])
			if expect_clear != (colors[index].a == 0.0) or (
				not expect_clear and colors[index].a != 1.0
			):
				wrong += 1
		_check(
			wrong == 0,
			"",
			"Kartenmodus %d: %d Orte mit falschem Alpha (0 nur für Orte ohne Daten, Märkte: ohne aktiven Markt)"
			% [mode, wrong]
		)


## Wirtschaft (seit M5): Märkte, Haushalt und Wirtschaft der Orte.
func _check_economy(sim: SimulationClient, player: int) -> void:
	var market_count := sim.get_market_count()
	var market_of := sim.get_location_market_indices()
	_check(
		market_count > 0 and market_of.size() == sim.get_location_count(),
		"%d Märkte, Marktindex je Ort" % market_count,
		"keine Märkte oder Marktindizes passen nicht zu den Orten"
	)
	var active := 0
	var fields_ok := true
	var prices_ok := true
	for market in market_count:
		var info := sim.get_market(market)
		for key in MARKET_FIELDS:
			fields_ok = fields_ok and info.has(key)
		if not bool(info.get("active", false)):
			continue
		active += 1
		for good: Dictionary in info.get("goods", []):
			for key in MARKET_GOOD_FIELDS:
				fields_ok = fields_ok and good.has(key)
			prices_ok = prices_ok and float(good.get("price", 0.0)) > 0.0
	_check(fields_ok, "", "get_market liefert nicht alle Felder")
	_check(active > 0 and prices_ok, "%d aktive Märkte mit Preisen" % active, "Märkte ohne Preise")
	var with_data_ok := true
	for index in sim.get_location_count():
		var location := sim.get_location(index)
		if bool(location.get("has_data", false)):
			var market := int(location.get("market_index", -1))
			with_data_ok = (
				with_data_ok and market >= 0 and market == market_of[index]
				and bool(sim.get_market(market).get("active", false))
			)
	_check(with_data_ok, "", "ein Ort mit Daten liegt in keinem aktiven Markt")

	var state := sim.get_state(player)
	var taxes: Array = state.get("taxes_month", [])
	var tax_sum := 0.0
	for amount: Variant in taxes:
		tax_sum += float(amount)
	_check(
		taxes.size() == 3 and absf(tax_sum - float(state.get("income_month", 0.0))) < 0.01,
		"Haushalt: Steuern je Art ergeben die Einnahmen (%s)" % GermanFormat.money(tax_sum),
		"Steuern je Art %s passen nicht zu den Einnahmen %s" % [
			str(taxes), str(state.get("income_month", 0.0))
		]
	)
	var expenses := 0.0
	for key in ["court_month", "administration_month", "army_month", "interest_month", "building_month"]:
		expenses += float(state.get(key, 0.0))
	_check(
		absf(expenses - float(state.get("expenses_month", 0.0))) < 0.01
		and int(state.get("soldiers", 0)) > 0,
		"Haushalt: Posten ergeben die Ausgaben, %s Soldaten" % GermanFormat.integer(
			int(state.get("soldiers", 0))
		),
		"Posten %s passen nicht zu den Ausgaben %s" % [
			str(expenses), str(state.get("expenses_month", 0.0))
		]
	)

	var capital := sim.get_location(int(state.get("capital_index", -1)))
	var professions: Array = capital.get("professions", [])
	var businesses: Array = capital.get("businesses", [])
	var goods: Array = capital.get("goods", [])
	var income_ok := true
	for row: Dictionary in professions:
		income_ok = income_ok and float(row.get("income_year", 0.0)) > 0.0
	_check(
		not professions.is_empty() and not businesses.is_empty() and not goods.is_empty() and income_ok
		and not str(capital.get("market_name", "")).is_empty(),
		(
			"Hauptstadt %s: Markt %s, %d Berufe, %d Betriebe, %d Güter"
			% [
				str(capital.get("name", "")), str(capital.get("market_name", "")),
				professions.size(), businesses.size(), goods.size()
			]
		),
		"Hauptstadt ohne Wirtschaftsdaten"
	)


func _check_date(sim: SimulationClient, start: Dictionary) -> void:
	var expected := _add_days(start, DAYS_TO_SIMULATE)
	var actual := sim.get_date()
	var same: bool = (
		int(actual.get("year", 0)) == expected["year"]
		and int(actual.get("month", 0)) == expected["month"]
		and int(actual.get("day", 0)) == expected["day"]
	)
	var expected_text := "%d.%d.%d" % [expected["day"], expected["month"], expected["year"]]
	_check(
		same,
		"Datum nach %d Tagen: %s" % [DAYS_TO_SIMULATE, sim.get_date_text()],
		"Datum nach %d Tagen %s, erwartet %s" % [DAYS_TO_SIMULATE, str(actual), expected_text]
	)
	var elapsed := sim.get_days_elapsed()
	_check(
		elapsed == DAYS_TO_SIMULATE,
		"%d Tage vergangen" % elapsed,
		"get_days_elapsed() = %d statt %d" % [elapsed, DAYS_TO_SIMULATE]
	)
	var date_text := sim.get_date_text()
	_check(
		date_text.contains(str(expected["year"])),
		"Datumstext „%s“" % date_text,
		"Datumstext „%s“ enthält das Jahr nicht" % date_text
	)


func _check_after_simulation(sim: SimulationClient, player: int) -> void:
	var state := sim.get_state(player)
	var income := float(state.get("income_month", 0.0))
	var expenses := float(state.get("expenses_month", 0.0))
	_check(
		income > 0.0 and expenses > 0.0,
		(
			"Einnahmen %s, Ausgaben %s je Monat"
			% [GermanFormat.money(income), GermanFormat.money(expenses)]
		),
		"nach einem Jahr keine Einnahmen oder Ausgaben"
	)
	var production := 0.0
	for index in sim.get_location_count():
		production += float(sim.get_location(index).get("production_month", 0.0))
	_check(
		production > 0.0,
		"Produktion aller Orte %s je Monat" % GermanFormat.money(production),
		"keine Produktion"
	)


func _check_chronicle(sim: SimulationClient) -> void:
	var count := sim.get_chronicle_size()
	var entries := sim.get_chronicle(0)
	_check(
		entries.size() == count,
		"Chronik: %d Einträge" % count,
		"get_chronicle(0) liefert %d statt %d Einträge" % [entries.size(), count]
	)
	for entry in entries:
		for key in CHRONICLE_FIELDS:
			if not entry.has(key):
				_check(false, "", "Chronikeintrag ohne Feld „%s“: %s" % [key, str(entry)])
				return
	if count > 0:
		var newest := entries[count - 1]
		print("      neuester Eintrag: %s – %s" % [newest["date_text"], newest["text"]])
		_check(
			sim.get_chronicle(count - 1).size() == 1,
			"",
			"get_chronicle(size − 1) liefert nicht genau einen Eintrag"
		)
	_check(
		sim.get_chronicle(count).is_empty() and sim.get_chronicle(count + 10).is_empty(),
		"Chronik ab dem Ende leer",
		"get_chronicle hinter dem Ende liefert Einträge"
	)


func _check_determinism(
	sim: SimulationClient, player: int, new_tax: int, new_excise: int
) -> void:
	var twin := SimulationClient.create()
	twin.load_scenario(UhrwerkPaths.data_dir())
	twin.set_tax_rate_percent(player, new_tax)
	twin.set_tax_percent(player, SimulationClient.TAX_AKZISE, new_excise)
	twin.advance_days(DAYS_TO_SIMULATE)
	var checksum := sim.get_checksum()
	_check(
		checksum.length() == 16 and checksum.is_valid_hex_number(),
		"Prüfsumme %s" % checksum,
		"Prüfsumme „%s“ ist keine 16-stellige Hexzahl" % checksum
	)
	_check(
		twin.get_checksum() == checksum,
		"zweiter Lauf mit gleicher Prüfsumme",
		"zweiter Lauf: %s statt %s" % [twin.get_checksum(), checksum]
	)


## Spielstände über die Brücke (seit M6): Speichern, Angaben, Laden, Weiterrechnen, Fehlerfälle.
func _check_savegames(sim: SimulationClient, player: int) -> void:
	var dir := OS.get_user_data_dir().path_join("smoke_test_spielstaende")
	_remove_dir(dir)
	DirAccess.make_dir_recursive_absolute(dir)
	var path := dir.path_join("rauchtest.einigkeit")
	var version := sim.get_save_format_version()
	_check(version >= 1, "Formatversion %d" % version, "Formatversion %d" % version)
	var error := sim.save_game(path, player)
	if not _check(
		error.is_empty() and FileAccess.file_exists(path),
		"Spielstand geschrieben (%d Byte)" % FileAccess.get_file_as_bytes(path).size(),
		"Speichern fehlgeschlagen: " + error
	):
		return
	var checksum := sim.get_checksum()
	var days := sim.get_days_elapsed()
	var info := sim.get_save_info(path)
	var tag := str(sim.get_state(player).get("tag", ""))
	_check(
		bool(info.get("ok", false)) and bool(info.get("compatible", false))
		and str(info.get("checksum", "")) == checksum and int(info.get("days", -1)) == days
		and str(info.get("player_tag", "")) == tag
		and str(info.get("date_text", "")) == sim.get_date_text()
		and int(info.get("format_version", 0)) == version,
		"Angaben zum Spielstand: %s, %s" % [str(info.get("date_text", "")), tag],
		"Angaben zum Spielstand passen nicht: %s" % str(info)
	)

	sim.advance_days(30)
	var continued := sim.get_checksum()
	error = sim.load_game(UhrwerkPaths.data_dir(), path)
	_check(
		error.is_empty() and sim.get_checksum() == checksum and sim.get_days_elapsed() == days
		and sim.get_player_state_index() == player,
		"Spielstand geladen, Prüfsumme %s" % checksum,
		"Laden: „%s“, Prüfsumme %s statt %s" % [error, sim.get_checksum(), checksum]
	)
	sim.advance_days(30)
	_check(
		sim.get_checksum() == continued,
		"nach dem Laden 30 Tage weiter wie ohne Unterbrechung",
		"nach dem Laden %s statt %s" % [sim.get_checksum(), continued]
	)

	# Fehlerfälle: Die laufende Simulation bleibt unverändert.
	var before := sim.get_checksum()
	var garbage := dir.path_join("kaputt.einigkeit")
	var file := FileAccess.open(garbage, FileAccess.WRITE)
	file.store_string("kein Spielstand")
	file.close()
	var bytes := FileAccess.get_file_as_bytes(path)
	var truncated := dir.path_join("abgeschnitten.einigkeit")
	file = FileAccess.open(truncated, FileAccess.WRITE)
	file.store_buffer(bytes.slice(0, bytes.size() - 10))
	file.close()
	for bad: String in [dir.path_join("fehlt.einigkeit"), garbage, truncated]:
		error = sim.load_game(UhrwerkPaths.data_dir(), bad)
		_check(
			not error.is_empty() and sim.get_checksum() == before,
			"%s abgelehnt: %s" % [bad.get_file(), error.get_slice("\n", 0)],
			"%s: Fehler „%s“, Prüfsumme %s statt %s" % [
				bad.get_file(), error, sim.get_checksum(), before
			]
		)
	var bad_info := sim.get_save_info(garbage)
	_check(
		not bool(bad_info.get("ok", true)) and not bool(bad_info.get("compatible", true))
		and not str(bad_info.get("reason", "")).is_empty(),
		"Angaben zu einer fremden Datei: %s" % str(bad_info.get("reason", "")),
		"Angaben zu einer fremden Datei: %s" % str(bad_info)
	)
	_remove_dir(dir)


func _remove_dir(dir: String) -> void:
	var access := DirAccess.open(dir)
	if access == null:
		return
	for file in access.get_files():
		access.remove(file)
	DirAccess.remove_absolute(dir)


func _check_map_files(sim: SimulationClient) -> void:
	var map := MapData.load_from_dir(UhrwerkPaths.map_dir())
	if not _check(
		map.is_valid(),
		"Karte geladen (%dx%d, %d Kartenindizes)" % [map.size.x, map.size.y, map.map_count()],
		map.error
	):
		return
	for warning in map.warnings:
		_check(false, "", warning)
	_check(
		map.river_texture != null,
		"fluesse.png vorhanden und so groß wie provinzen.png",
		"fluesse.png fehlt oder passt nicht (siehe Hinweise oben)"
	)
	var rivers := Image.load_from_file(UhrwerkPaths.map_dir().path_join("fluesse.png"))
	if rivers != null and not rivers.is_empty():
		_check(
			rivers.get_format() == Image.FORMAT_L8,
			"fluesse.png ist ein Graustufenbild",
			"fluesse.png hat das Format %d statt L8 (Graustufen)" % rivers.get_format()
		)
	var unresolved := map.bind_simulation(sim)
	_check(
		unresolved.is_empty(),
		"jeder Eintrag der definition.csv über get_location_index auflösbar",
		"Kennungen der definition.csv ohne Ort: " + ", ".join(unresolved)
	)
	var absent := PackedStringArray()
	for sim_index in map.sim_to_map.size():
		if map.sim_to_map[sim_index] < 0:
			absent.append(str(sim.get_location(sim_index).get("id", sim_index)))
	_check(
		absent.is_empty(),
		"jeder Ort der Simulation steht in der definition.csv",
		"Orte ohne Eintrag in der definition.csv: " + ", ".join(absent)
	)
	for map_index in map.map_count():
		if map.map_ids[map_index].is_empty():
			_check(false, "", "definition.csv: Kartenindex %d fehlt (Lücke)" % map_index)
	if map.config != null:
		var configured := Vector2i(map.config.image_width, map.config.image_height)
		_check(
			map.size == configured,
			"Bildgröße passt zu karte.chron",
			(
				"provinzen.png ist %dx%d, karte.chron verlangt %dx%d"
				% [map.size.x, map.size.y, configured.x, configured.y]
			)
		)
	_check_map_pixels(map)


## Pixel von provinzen.png (chronik-skript.md 6.3): Blaukanal = Flächenart 0..3, bei
## B ≠ 0 kein Ort; Kartenindizes bekannt; Pixelzahl und Beschriftungspunkt je Ort wie in
## der definition.csv.
func _check_map_pixels(map: MapData) -> void:
	var bytes := map.image.get_data()
	var count := map.map_count()
	var pixels := PackedInt32Array()
	pixels.resize(count)
	var areas := PackedInt32Array()
	areas.resize(4)
	var invalid := 0
	var bad_area := 0
	var water_with_location := 0
	var land_without := 0
	for offset in range(0, bytes.size(), 3):
		var area := bytes[offset + 2]
		var value := bytes[offset] + bytes[offset + 1] * 256
		if area > 3:
			bad_area += 1
			continue
		areas[area] += 1
		if area != 0:
			if value != 0:
				water_with_location += 1
		elif value == 0:
			land_without += 1
		elif value > count:
			invalid += 1
		else:
			pixels[value - 1] += 1
	_check(
		bad_area == 0,
		(
			"Flächenarten: %s Kernraum-Land, %s Meer, %s Binnengewässer, %s Ausland"
			% [
				GermanFormat.integer(areas[0]), GermanFormat.integer(areas[1]),
				GermanFormat.integer(areas[2]), GermanFormat.integer(areas[3])
			]
		),
		"%d Pixel mit Blaukanal > 3 (Flächenart 0..3)" % bad_area
	)
	_check(
		water_with_location == 0,
		"Meer, Binnengewässer und Ausland ohne Ort",
		"%d Pixel mit B ≠ 0 und Kartenindex (Spezifikation: wert = 0)" % water_with_location
	)
	_check(
		invalid == 0,
		"alle Pixel tragen bekannte Kartenindizes",
		"%d Pixel mit Kartenindex außerhalb der definition.csv" % invalid
	)
	var without := PackedStringArray()
	var wrong_count := PackedStringArray()
	var wrong_point := PackedStringArray()
	for map_index in count:
		if map.map_ids[map_index].is_empty():
			continue
		if pixels[map_index] == 0:
			without.append(map.map_ids[map_index])
		if pixels[map_index] != map.pixel_counts[map_index]:
			wrong_count.append("%s (%d statt %d)" % [
				map.map_ids[map_index], pixels[map_index], map.pixel_counts[map_index]
			])
		var point := map.label_points[map_index]
		if point.x >= 0.0 and map.map_index_at_pixel(Vector2i(point.floor())) != map_index:
			wrong_point.append(map.map_ids[map_index])
	_check(
		without.is_empty(),
		(
			"jeder Ort hat Kartenfläche (%s Pixel Kernraum-Land ohne Ort)"
			% GermanFormat.integer(land_without)
		),
		"Orte ohne Pixel in provinzen.png: " + ", ".join(without)
	)
	_check(
		wrong_count.is_empty(),
		"Pixelzahl je Ort wie in definition.csv",
		"Pixelzahl weicht ab: " + ", ".join(wrong_count.slice(0, 10))
	)
	_check(
		wrong_point.is_empty(),
		"jeder Beschriftungspunkt liegt im eigenen Ort",
		"Beschriftungspunkt nicht im Ort: " + ", ".join(wrong_point.slice(0, 10))
	)


func _add_days(date: Dictionary, days: int) -> Dictionary:
	var year := int(date.get("year", 0))
	var month := int(date.get("month", 1))
	var day := int(date.get("day", 1))
	for _i in days:
		day += 1
		if day > _days_in_month(year, month):
			day = 1
			month += 1
			if month > 12:
				month = 1
				year += 1
	return {"year": year, "month": month, "day": day}


func _days_in_month(year: int, month: int) -> int:
	if month == 2:
		var leap := (year % 4 == 0 and year % 100 != 0) or year % 400 == 0
		return 29 if leap else 28
	return 30 if month in [4, 6, 9, 11] else 31


## Meldet eine Prüfung; liefert condition zurück.
func _check(condition: bool, success: String, failure: String) -> bool:
	if condition:
		if not success.is_empty():
			print("  ok  ", success)
	else:
		_failures.append(failure)
		printerr("  FEHLER  ", failure)
	return condition
