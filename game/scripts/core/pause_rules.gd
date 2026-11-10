class_name PauseRules
extends RefCounted
## Pausenregeln (seit M13, Schritt a5d; vertical_slice.md 4.4) aus den Einträgen der Chronik: der Grund
## der ersten zutreffenden Regel, die in den Einstellungen an ist, sonst "".


static func reason(entries: Array[Dictionary], own: String, player: int, settings: UserSettings) -> String:
	for entry in entries:
		var text := str(entry.get("text", ""))
		var state := int(entry.get("state_index", -1))
		if text.contains(" den Krieg") and text.contains(own) and settings.pause_on("pause_krieg"):
			return text
		if text.contains(" fordert von %s " % own) and settings.pause_on("pause_forderung"):
			return text
		if text.begins_with("Frieden zwischen") and text.contains(own) and settings.pause_on("pause_frieden"):
			return text
		if text.begins_with("Gründung:") and settings.pause_on("pause_bund"):
			return text
		if (
			state == player and text.begins_with(own + ": ") and not text.contains(" – ")
			and (text.contains(" wird ") or text.ends_with("scheidet aus dem Amt"))
			and settings.pause_on("pause_amt")
		):
			return text
	return ""
