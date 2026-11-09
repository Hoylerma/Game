class_name ChronicleExport
extends RefCounted
## Chronik als HTML-Datei (seit M13, Schritt a4; vertical_slice.md 3.5): eine eigenständige Seite mit
## Kopf (Lesezeichen, Staat, Zeitraum), dem Urteil in Kurzform und allen Einträgen der Chronik nach
## Jahren, Wendepunkte hervorgehoben. Die Seite braucht nichts außer sich selbst (Stil im Kopf).

const FOLDER := "user://chronik"


## Ersetzt die Zeichen, die in HTML eine Bedeutung haben.
static func escape(text: String) -> String:
	return (
		text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace("\"", "&quot;")
	)


## Jahr eines Chronikeintrags aus dem Datumstext („1. März 1848“, „01.03.1848“); 0, wenn keines.
static func year_of(date_text: String) -> int:
	var digits := ""
	for i in range(date_text.length() - 1, -1, -1):
		var c := date_text[i]
		if c >= "0" and c <= "9":
			digits = c + digits
		elif not digits.is_empty():
			break
	return int(digits) if digits.length() == 4 else 0


## Die Seite: title (Kopf), facts (Zeilen unter dem Kopf), sections (je { heading, lines }) vor der Chronik
## und die Einträge der Chronik ({ date_text, text, turning_point }).
static func build_html(
	title: String, facts: PackedStringArray, sections: Array[Dictionary], entries: Array[Dictionary]
) -> String:
	var html := PackedStringArray()
	html.append("<!DOCTYPE html>")
	html.append("<html lang=\"de\"><head><meta charset=\"utf-8\">")
	html.append("<title>%s</title>" % escape(title))
	html.append(
		"<style>body{font-family:Georgia,serif;max-width:46em;margin:2em auto;padding:0 1em;"
		+ "background:#f4ecda;color:#2b2116;line-height:1.45}h1{font-size:1.7em;margin-bottom:.2em}"
		+ "h2{border-bottom:1px solid #b49a6c;margin-top:1.6em}.fakt{color:#5b4a36;margin:.1em 0}"
		+ ".datum{color:#5b4a36;display:inline-block;min-width:9em}.wende{font-weight:bold;color:#8e2b1d}"
		+ "ul{padding-left:1.2em}li{margin:.2em 0}</style>"
	)
	html.append("</head><body>")
	html.append("<h1>%s</h1>" % escape(title))
	for fact in facts:
		html.append("<p class=\"fakt\">%s</p>" % escape(fact))
	for section in sections:
		html.append("<h2>%s</h2>" % escape(str(section.get("heading", ""))))
		html.append("<ul>")
		for line: Variant in section.get("lines", PackedStringArray()):
			html.append("<li>%s</li>" % escape(str(line)))
		html.append("</ul>")
	html.append("<h2>Chronik</h2>")
	var year := -1
	for entry in entries:
		var date_text := str(entry.get("date_text", ""))
		var y := year_of(date_text)
		if y != year:
			if year != -1:
				html.append("</ul>")
			year = y
			html.append("<h3>%d</h3><ul>" % y if y > 0 else "<ul>")
		var css := " class=\"wende\"" if bool(entry.get("turning_point", false)) else ""
		html.append(
			"<li%s><span class=\"datum\">%s</span> %s</li>"
			% [css, escape(date_text), escape(str(entry.get("text", "")))]
		)
	if year != -1:
		html.append("</ul>")
	html.append("</body></html>")
	return "\n".join(html) + "\n"


## Dateiname aus Lesezeichen und Datum, nur Buchstaben, Ziffern und Unterstriche.
static func file_name(scenario_id: String, date: Dictionary) -> String:
	var safe := ""
	for c in scenario_id:
		safe += c if (c >= "a" and c <= "z") or (c >= "0" and c <= "9") or c == "_" else "_"
	return "chronik_%s_%04d-%02d-%02d.html" % [
		safe if not safe.is_empty() else "partie",
		int(date.get("year", 0)), int(date.get("month", 0)), int(date.get("day", 0)),
	]


## Schreibt die Chronik der Partie mit dem Urteil in Kurzform nach FOLDER; Pfad oder "" bei Fehler.
static func write(sim: SimulationClient, folder: String = FOLDER) -> String:
	if sim == null:
		return ""
	var player := sim.get_player_state_index()
	var state := sim.get_state(player) if player >= 0 else {}
	var verdict := sim.get_verdict()
	var title := "Chronik – %s" % sim.get_scenario_name()
	var facts := PackedStringArray()
	if not state.is_empty():
		facts.append("Gespielt als %s" % str(state.get("name", "")))
	facts.append(
		"Vom %s bis zum %s" % [str(verdict.get("start_date_text", "")), sim.get_date_text()]
	)
	var sections: Array[Dictionary] = []
	var journals := PackedStringArray()
	for journal in sim.get_journals():
		if bool(journal.get("finished", false)):
			journals.append(
				"%s: %s" % [str(journal.get("title", "")), str(journal.get("stage_title", ""))]
			)
	if not journals.is_empty():
		sections.append({"heading": "Ausgang", "lines": journals})
	var legacy := PackedStringArray()
	for dimension: Dictionary in sim.get_legacy(player).get("dimensions", []):
		legacy.append(
			"%s %d – %s" % [
				str(dimension.get("name", "")), roundi(float(dimension.get("value", 0.0))),
				str(dimension.get("sentence", "")),
			]
		)
	if not legacy.is_empty():
		sections.append({"heading": "Vermächtnis", "lines": legacy})
	var html := build_html(title, facts, sections, sim.get_chronicle(0))
	if not DirAccess.dir_exists_absolute(folder):
		if DirAccess.make_dir_recursive_absolute(folder) != OK:
			return ""
	var path := folder.path_join(file_name(str(sim.get_scenario().get("id", "")), sim.get_date()))
	var file := FileAccess.open(path, FileAccess.WRITE)
	if file == null:
		return ""
	file.store_string(html)
	file.close()
	return path
