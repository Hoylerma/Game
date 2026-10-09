class_name GermanFormat
extends RefCounted
## Zahlen- und Textformatierung nach deutscher Schreibweise.
##
## Tausenderpunkt, Dezimalkomma, geschütztes Leerzeichen vor Einheiten.
## Gleitkommazahlen kommen hier nur zur Anzeige vor (Werte aus der Brücke).

const THOUSANDS_SEPARATOR := "."
const DECIMAL_SEPARATOR := ","
const MINUS := "−"
const NBSP := "\u00A0"
const CURRENCY := "Taler"

const _GOVERNMENT_NAMES := {
	"staendisches_fuerstentum": "Ständisches Fürstentum",
	"geistliches_fuerstentum": "Geistliches Fürstentum",
	"staendische_monarchie": "Ständische Monarchie",
	"erbmonarchie": "Erbmonarchie",
	"absolute_monarchie": "Absolute Monarchie",
	"wahlmonarchie": "Wahlmonarchie",
	"reichsstadt": "Reichsstadt",
	"republik": "Republik",
}


## Ganzzahl mit Tausenderpunkt: 1234567 → "1.234.567".
static func integer(value: int) -> String:
	var digits := str(absi(value))
	var grouped := ""
	var count := 0
	for i in range(digits.length() - 1, -1, -1):
		if count > 0 and count % 3 == 0:
			grouped = THOUSANDS_SEPARATOR + grouped
		grouped = digits[i] + grouped
		count += 1
	return (MINUS if value < 0 else "") + grouped


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
		text += DECIMAL_SEPARATOR + fraction
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


## Bevölkerungsdichte: "82,3 Einw./km²".
static func density(value: float) -> String:
	return number(value, 1) + NBSP + "Einw./km²"


## Regierungsform aus dem Bezeichner der Daten:
## "staendisches_fuerstentum" → "Ständisches Fürstentum".
static func government(identifier: String) -> String:
	if _GOVERNMENT_NAMES.has(identifier):
		return _GOVERNMENT_NAMES[identifier]
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
