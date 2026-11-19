class_name LegendView
extends RefCounted
## Legende je Kartenmodus (seit M13a5d aus main.gd ausgelagert): füllt das Legendenpanel aus den Daten des
## Kerns (LegendData). main.gd übergibt die Werte des Lesezeichens, die es zwischenspeichert.


static func show(
	legend: LegendPanel, sim: SimulationClient, mode: int, catalog: LocationCatalog, player: int,
	currency: String, market_indices: PackedInt32Array, admin_indices: PackedInt32Array, has_admin_units: bool,
	revealing: bool
) -> void:
	var title := MapModes.name_of(mode)
	if sim == null:
		legend.show_message(title, "Ohne Simulation liegen keine Daten vor.")
		return
	var no_data: Array[Dictionary] = []
	if catalog.no_data_count() > 0:
		no_data.append(LegendData.no_data_entry(catalog.no_data_count()))
	match mode:
		MapModes.MAP_POLITICAL:
			var states := LegendData.political_entries(
				sim, LegendData.MAX_POLITICAL_ENTRIES, player
			)
			states.append_array(no_data)
			legend.show_swatches(title, states)
		MapModes.MAP_DENSITY:
			var scale_max := MapModes.DENSITY_SCALE_MAX
			var density_labels := PackedStringArray(
				[
					"0",
					Format.number(scale_max * 0.5),
					"≥ " + Format.number(scale_max),
				]
			)
			legend.show_gradient(
				title,
				LegendData.gradient_for(sim, MapModes.MAP_DENSITY, "density", scale_max),
				density_labels,
				"Einwohner je km²",
				no_data
			)
		MapModes.MAP_CONFESSION:
			var confessions := LegendData.category_entries(
				sim, MapModes.MAP_CONFESSION, "majority_confession"
			)
			legend.show_swatches(title, confessions)
		MapModes.MAP_TERRAIN:
			var terrains := LegendData.category_entries(
				sim, MapModes.MAP_TERRAIN, "terrain", catalog
			)
			legend.show_swatches(title, terrains)
		MapModes.MAP_DEVASTATION:
			var devastation_labels := PackedStringArray(
				[Format.percent(0), Format.percent(50), Format.percent(100)]
			)
			legend.show_gradient(
				title,
				LegendData.gradient_for(
					sim, MapModes.MAP_DEVASTATION, "devastation_percent", 100.0
				),
				devastation_labels,
				"Kriegsschäden der Orte",
				no_data
			)
		MapModes.MAP_LEBENSSTANDARD:
			var living_labels := PackedStringArray(["0", "50", "100"])
			legend.show_gradient(
				title,
				LegendData.gradient_for(
					sim, MapModes.MAP_LEBENSSTANDARD, "living_standard", 100.0
				),
				living_labels,
				"Mittlerer Lebensstandard der Einwohner",
				no_data
			)
		MapModes.MAP_KULTUR:
			var cultures := LegendData.category_entries(
				sim, MapModes.MAP_KULTUR, "majority_culture"
			)
			legend.show_swatches(title, cultures)
		MapModes.MAP_KREISE:
			# Nur Kreise mit Orten und Orte mit Kreis (ein leerer Kreis ist nur eine Warnung).
			var kreis_count := catalog.kreise_with_places()
			var text := (
				(
					"%s Kreise fassen %s zusammen. Die Farben wiederholen sich; Grenzlinien "
					+ "trennen die Kreise, ihre Namen erscheinen beim Hineinzoomen."
				)
				% [
					Format.integer(kreis_count),
					LegendData.count_text(catalog.places_in_kreisen())
				]
			)
			if kreis_count == 0:
				text = "Die Daten enthalten keine Kreise (history/kreise fehlt oder ist leer)."
			legend.show_palette(title, LegendData.kreis_palette(sim, catalog), text)
		MapModes.MAP_WIRTSCHAFT:
			var economy_max := MapModes.ECONOMY_SCALE_MAX
			var economy_labels := PackedStringArray(
				[
					"0",
					Format.number(economy_max * 0.5),
					"≥ " + Format.number(economy_max),
				]
			)
			legend.show_gradient(
				title,
				LegendData.gradient_for(
					sim, MapModes.MAP_WIRTSCHAFT, "production_per_capita", economy_max
				),
				economy_labels,
				"%s je Einwohner und Jahr" % currency,
				no_data
			)
		MapModes.MAP_MAERKTE:
			var markets := LegendData.market_entries(sim, market_indices)
			legend.show_swatches(title, markets)
		MapModes.MAP_VERWALTUNG:
			var units := LegendData.admin_entries(sim, admin_indices, player)
			if units.is_empty():
				legend.show_message(
					title,
					(
						"Euer Staat hat keine Verwaltungsgliederung in den Daten. Staaten ohne "
						+ "Gliederung erscheinen in ihrer aufgehellten Farbe."
					)
					if has_admin_units
					else "Dieses Szenario enthält keine Verwaltungseinheiten."
				)
			else:
				legend.show_swatches(title, units)
		MapModes.MAP_WAHLEN:
			var entries := LegendData.election_entries(sim, revealing)
			if entries.is_empty():
				legend.show_message(title, "In diesem Szenario gibt es kein Parlament und keine Wahl.")
			else:
				entries.append_array(no_data)
				legend.show_swatches(LegendData.election_title(sim), entries)
		MapModes.MAP_EISENBAHNEN:
			var rail := LegendData.railway_entries(sim)
			if rail.is_empty():
				legend.show_message(title, "Dieses Szenario enthält keine Eisenbahnen.")
			else:
				legend.show_swatches(title, rail)
		MapModes.MAP_ZOLLGEBIETE:
			var customs := LegendData.customs_entries(sim)
			if customs.is_empty():
				legend.show_message(
					title,
					(
						"In diesem Szenario gibt es keine Zollunion: Jeder Staat ist ein eigenes "
						+ "Zollgebiet und erhebt seinen Zoll (blass in der Staatsfarbe)."
					)
				)
			else:
				customs.append_array(no_data)
				legend.show_swatches(title, customs)
		MapModes.MAP_KRIEG:
			var war := LegendData.war_entries(sim)
			if war.is_empty():
				legend.show_message(
					title,
					(
						"Zurzeit führt kein Staat Krieg. Heere und Kriege gibt es seit 1848; die Lesezeichen "
						+ "Düppel, Königgrätz und Sedan beginnen im Krieg."
					)
				)
			else:
				war.append_array(no_data)
				legend.show_swatches(title, war)
		MapModes.MAP_BUND:
			var federation := LegendData.federation_entries(sim)
			if federation.is_empty():
				legend.show_message(title, "Den Deutschen Bund und neue Bünde gibt es erst ab 1848.")
			else:
				title = "%s" % str(sim.get_diet().get("federation_name", title))
				federation.append_array(no_data)
				legend.show_swatches(title, federation)
		MapModes.MAP_BAUEN:
			if not bool(sim.get_building_info().get("active", false)):
				legend.show_message(title, "Gebaut wird erst ab 1848 (Gebäude in Landesteilen).")
			else:
				var entries: Array[Dictionary] = [
					{"color": Color(0.24, 0.52, 0.30), "text": "Lohnt sich", "detail": "Bestes Vorhaben bringt im Jahr die Hälfte seiner Kosten oder mehr"},
					{"color": Color(0.59, 0.72, 0.57), "text": "Mittel", "detail": "Bestes Vorhaben bringt im Jahr etwa ein Viertel seiner Kosten"},
					{"color": Color(0.94, 0.91, 0.83), "text": "Kaum", "detail": "Kaum ein Vorhaben lohnt sich"},
					{"color": Color(0.80, 0.62, 0.22), "text": "Bau läuft"},
					{"color": Color(0.88, 0.86, 0.80), "text": "Anderer Staat"},
				]
				entries.append_array(no_data)
				legend.show_swatches(title, entries)
		MapModes.MAP_UNRUHE:
			var unrest := LegendData.unrest_entries(sim)
			if unrest.is_empty():
				legend.show_message(title, "Unruhe und Aufstände gibt es erst mit den Ereignissen (seit 1848).")
			else:
				unrest.append_array(no_data)
				legend.show_swatches(title, unrest)
		MapModes.MAP_BEZIEHUNGEN:
			var relations := LegendData.relation_entries(sim, player)
			if relations.is_empty():
				legend.show_message(
					title,
					"Beziehungen zwischen den Staaten gibt es erst mit dem Deutschen Bund (Lesezeichen 1848)."
				)
			else:
				relations.append_array(no_data)
				legend.show_swatches(title, relations)
