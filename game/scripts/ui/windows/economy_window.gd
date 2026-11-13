class_name EconomyWindow
extends AreaWindow
## Wirtschaft (seit M13a5b): Erzeugung und Steuern des Monats über alle Orte des Staates, Zollgebiet, Märkte,
## Güter mit Angebot, Nachfrage und Preis (nach Umsatz gemittelt) und die Betriebe nach Art.


func _window_title() -> String:
	return "WIRTSCHAFT"


func _window_name() -> String:
	return "EconomyWindow"


func _fill() -> void:
	var economy := _sim.get_state_economy(_player)
	if economy.is_empty():
		return
	_line("Erzeugung je Monat", Format.money(float(economy.get("production_month", 0.0))))
	_line("Steuern je Monat", Format.money(float(economy.get("taxes_month", 0.0))))
	_line("Zollgebiet", "%s, Tarif %s" % [str(economy.get("customs_zone_name", "")), Format.percent(float(economy.get("customs_tariff_percent", 0.0)), 1)])
	var markets := PackedStringArray()
	for m: Dictionary in economy.get("markets", []):
		markets.append("%s (%d)" % [str(m.get("name", "")), int(m.get("locations", 0))])
	if not markets.is_empty():
		_paragraph("Märkte (Orte): " + ", ".join(markets), true)
	_gap()
	_heading("GÜTER JE MONAT: ANGEBOT · NACHFRAGE · PREIS")
	for good: Dictionary in economy.get("goods", []):
		var factor := float(good.get("price_factor", 0.0))
		_line(
			str(good.get("name", "")),
			"%s · %s · %s (×%s)" % [
				Format.number(float(good.get("supply", 0.0))), Format.number(float(good.get("demand", 0.0))),
				Format.number(float(good.get("price", 0.0)), 2), Format.number(factor, 2),
			],
			"Preis nach Umsatz über die Märkte des Staates; ×: zum Basispreis",
			UiPalette.NEGATIVE if factor > 1.25 else (UiPalette.POSITIVE if factor < 0.8 else Color.TRANSPARENT)
		)
	_gap()
	_heading("BETRIEBE (BESCHÄFTIGTE)")
	var total := 0
	for b: Dictionary in economy.get("businesses", []):
		total += int(b.get("people", 0))
	for b: Dictionary in economy.get("businesses", []):
		var people := int(b.get("people", 0))
		_bar("%s (%s)" % [str(b.get("name", "")), str(b.get("profession", ""))], float(people) / maxf(total, 1.0), UiPalette.GOLD, Format.integer(people))
