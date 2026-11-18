class_name Format
extends RefCounted
## Zahlen- und Textformatierung je Sprache (seit M13b4, vertical_slice.md 6; vorher Format).
##
## Deutsch: Tausenderpunkt, Dezimalkomma, „1. März 1848“; Englisch: Tausenderkomma, Dezimalpunkt,
## „1 March 1848“. Geschütztes Leerzeichen vor Einheiten; Währungen bleiben historisch (Taler).
## Gleitkommazahlen kommen hier nur zur Anzeige vor (Werte aus der Brücke). Die Sprache setzt
## UserSettings.apply (Einstellung „sprache/sprache“).

const MINUS := "−"
const NBSP := "\u00A0"
const CURRENCY := "Taler"
const LOCALES: PackedStringArray = ["de", "en"]
const _MONTHS := {
	"de": ["Januar", "Februar", "März", "April", "Mai", "Juni", "Juli", "August", "September", "Oktober",
		"November", "Dezember"],
	"en": ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October",
		"November", "December"],
}

## Sprache der Zahlen und Daten: "de" oder "en".
static var locale := "de"

const _GOVERNMENT_NAMES := {
	"staendisches_fuerstentum": "Ständisches Fürstentum",
	"geistliches_fuerstentum": "Geistliches Fürstentum",
	"staendische_monarchie": "Ständische Monarchie",
	"erbmonarchie": "Erbmonarchie",
	"absolute_monarchie": "Absolute Monarchie",
	"wahlmonarchie": "Wahlmonarchie",
	"reichsstadt": "Reichsstadt",
	"republik": "Republik",
	"konstitutionelle_monarchie": "Konstitutionelle Monarchie",
	"stadtrepublik": "Stadtrepublik",
}


## Sprache setzen; unbekannte bleiben bei Deutsch.
static func set_locale(code: String) -> void:
	locale = code if LOCALES.has(code) else "de"


static func thousands_separator() -> String:
	return "," if locale == "en" else "."


static func decimal_separator() -> String:
	return "." if locale == "en" else ","


## Datum in Worten: „1. März 1848“ bzw. „1 March 1848“.
static func date(day: int, month: int, year: int) -> String:
	var months: Array = _MONTHS.get(locale, _MONTHS["de"])
	var name := str(months[clampi(month, 1, 12) - 1])
	return ("%d %s %d" if locale == "en" else "%d. %s %d") % [day, name, year]


## Datum aus einem Dictionary { year, month, day } (wie SimulationClient.get_date).
static func date_of(value: Dictionary) -> String:
	return date(int(value.get("day", 1)), int(value.get("month", 1)), int(value.get("year", 0)))


## Ganzzahl mit Tausendertrennzeichen: 1234567 → "1.234.567" (en: "1,234,567").
static func integer(value: int) -> String:
	var digits := str(absi(value))
	var grouped := ""
	var count := 0
	for i in range(digits.length() - 1, -1, -1):
		if count > 0 and count % 3 == 0:
			grouped = thousands_separator() + grouped
		grouped = digits[i] + grouped
		count += 1
	return (MINUS if value < 0 else "") + grouped


## Große Zahl kurz (seit M14a2, obere Leiste): ab einer Million „15,98 Mio.“ (en: "15.98 M"), ab
## Hunderttausend „250 Tsd.“ (en: "250 k"), darunter mit Tausendertrennzeichen.
static func compact(value: float) -> String:
	var size := absf(value)
	if size >= 1_000_000.0:
		return "%s %s" % [number(value / 1_000_000.0, 2), "M" if locale == "en" else "Mio."]
	if size >= 100_000.0:
		return "%s %s" % [number(value / 1000.0, 0), "k" if locale == "en" else "Tsd."]
	return integer(roundi(value))


## Zahl mit fester Anzahl Nachkommastellen: 1234.56 → "1.234,6" (decimals = 1).
static func number(value: float, decimals: int = 0) -> String:
	if is_nan(value) or is_inf(value):
		return "–"
	decimals = clampi(decimals, 0, 6)
	var factor := 1
	for _i in decimals:
		factor *= 10
	var scaled := roundi(absf(value) * factor)
	var whole := floori(float(scaled) / factor)
	var text := integer(whole)
	if decimals > 0:
		var fraction := str(scaled - whole * factor).lpad(decimals, "0")
		text += decimal_separator() + fraction
	if value < 0.0 and scaled != 0:
		text = MINUS + text
	return text


## Zahl mit Vorzeichen: "+1.234" bzw. "−1.234".
static func signed_number(value: float, decimals: int = 0) -> String:
	var text := number(value, decimals)
	if value > 0.0 and text != number(0.0, decimals):
		return "+" + text
	return text


## Geldbetrag in Talern: "23.000 Taler".
static func money(value: float) -> String:
	return number(value) + NBSP + CURRENCY


## Geldbetrag mit Vorzeichen: "+8.339 Taler".
static func signed_money(value: float) -> String:
	return signed_number(value) + NBSP + CURRENCY


## Prozentwert: "12 %" bzw. "12,5 %".
static func percent(value: float, decimals: int = 0) -> String:
	return number(value, decimals) + NBSP + "%"


## Fläche: "150 km²".
static func area(value_km2: float) -> String:
	return number(value_km2) + NBSP + "km²"


## Bevölkerungsdichte: "82,3 Einw./km²" (en: "82.3 inh./km²").
static func density(value: float) -> String:
	return number(value, 1) + NBSP + ("inh./km²" if locale == "en" else "Einw./km²")


## Regierungsform aus dem Bezeichner der Daten:
## "staendisches_fuerstentum" → "Ständisches Fürstentum".
static func government(identifier: String) -> String:
	if _GOVERNMENT_NAMES.has(identifier):
		return TranslationServer.translate(_GOVERNMENT_NAMES[identifier])
	return humanize(identifier)


## Macht aus einem Bezeichner einen lesbaren Text ("flussaue" → "Flussaue").
## Bereits lesbare Namen bleiben unverändert.
static func humanize(text: String) -> String:
	if text.is_empty():
		return "–"
	if text.contains(" ") or text != text.to_lower():
		return text
	var words := text.split("_", false)
	for i in words.size():
		words[i] = words[i].left(1).to_upper() + words[i].substr(1)
	return " ".join(words)
