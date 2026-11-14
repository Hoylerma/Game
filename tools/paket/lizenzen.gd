extends SceneTree
## Gibt die Lizenzangaben der Godot-Engine aus (seit M13b5): Engine-Lizenz, Urheberangaben der in Godot
## enthaltenen Bibliotheken und deren Lizenztexte. paket.py schreibt sie in DRITTANBIETER.txt.
##   godot --headless --script tools/paket/lizenzen.gd


func _initialize() -> void:
	var out := PackedStringArray()
	# Godot schreibt vorher eine eigene Zeile; paket.py liest erst ab der Marke.
	out.append("=== LIZENZEN ===")
	var version: Dictionary = Engine.get_version_info()
	out.append("Godot Engine %s" % version.get("string", ""))
	out.append("")
	out.append(Engine.get_license_text())
	out.append("")
	out.append("Bestandteile der Godot Engine:")
	for part: Dictionary in Engine.get_copyright_info():
		out.append("")
		out.append("- %s" % part.get("name", ""))
		for entry: Dictionary in part.get("parts", []):
			for holder: String in entry.get("copyright", PackedStringArray()):
				out.append("  © %s" % holder)
			out.append("  Lizenz: %s" % entry.get("license", ""))
	out.append("")
	out.append("Lizenztexte:")
	var licenses: Dictionary = Engine.get_license_info()
	var names := licenses.keys()
	names.sort()
	for license_name: String in names:
		out.append("")
		out.append("~~~ %s ~~~" % license_name)
		out.append("")
		out.append(str(licenses[license_name]).strip_edges())
	print("\n".join(out))
	quit(0)
