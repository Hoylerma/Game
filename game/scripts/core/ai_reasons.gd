class_name AiReasons
extends RefCounted
## Gründe einer Entscheidung der KI als übersetzbarer Text (seit M13b4, vertical_slice.md 6). Der Kern liefert
## je Grund einen Schlüssel mit Werten (SimulationClient.get_ai_decisions, reason_keys: { key, state_index,
## state_name, value }); hier wird daraus der Satz in der Sprache der Oberfläche. Ohne Schlüssel bleibt der
## fertige deutsche Text des Kerns (reasons_text).


## Ein Grund: „Stärke gegen Österreich +12“.
static func text(reason: Dictionary) -> String:
	var state := str(reason.get("state_name", ""))
	var value := int(reason.get("value", 0))
	var amount := ("+%d" % value) if value >= 0 else ("%s%d" % [Format.MINUS, -value])
	var template := _template(str(reason.get("key", "")), not state.is_empty())
	if template.is_empty():
		return ("%s %s" % [state, amount]).strip_edges()
	return template.format({"staat": state, "wert": amount})


## Alle Gründe einer Entscheidung, durch Kommas getrennt.
static func of(decision: Dictionary) -> String:
	var keyed: Array = decision.get("reason_keys", [])
	if keyed.is_empty():
		return str(decision.get("reasons_text", ""))
	var parts := PackedStringArray()
	for reason: Variant in keyed:
		parts.append(text(reason as Dictionary))
	return ", ".join(parts)


static func _template(key: String, with_state: bool) -> String:
	match key:
		"defizit":
			return TranslationServer.translate("Defizit {wert}")
		"schuldenquote":
			return TranslationServer.translate("Schuldenquote {wert}")
		"ueberschuss":
			return TranslationServer.translate("Überschuss {wert}")
		"unruhe":
			return TranslationServer.translate("Unruhe {wert}")
		"sparsamkeit":
			return TranslationServer.translate("Sparsamkeit {wert}")
		"reform":
			return TranslationServer.translate("Reform {wert}")
		"risiko":
			return TranslationServer.translate("Risiko {wert}")
		"bedrohung":
			return TranslationServer.translate("Bedrohung durch {staat} {wert}")
		"mobil":
			return TranslationServer.translate("mobil: {staat} {wert}")
		"frieden":
			return TranslationServer.translate("Frieden {wert}")
		"agenda":
			return TranslationServer.translate("Agenda {staat} {wert}") if with_state else TranslationServer.translate("Agenda {wert}")
		"rivale":
			return TranslationServer.translate("Rivale {staat} {wert}")
		"beziehung":
			return TranslationServer.translate("Beziehung zu {staat} {wert}")
		"staerke":
			return TranslationServer.translate("Stärke gegen {staat} {wert}")
		"nachbar":
			return TranslationServer.translate("Nachbar {staat} {wert}")
		"verbuendet":
			return TranslationServer.translate("verbündet mit {staat} {wert}")
		"expansion":
			return TranslationServer.translate("Expansion {wert}")
		"legitimitaet":
			return TranslationServer.translate("Legitimität {wert}")
		"vermittlung":
			return TranslationServer.translate("Vermittlung {wert}")
		"buendnisse":
			return TranslationServer.translate("Bündnisse {wert}")
		"kriegspunkte":
			return TranslationServer.translate("Kriegspunkte {staat} {wert}") if with_state else TranslationServer.translate("Kriegspunkte {wert}")
		"kriegsmuedigkeit":
			return TranslationServer.translate("Kriegsmüdigkeit {wert}")
	return ""
