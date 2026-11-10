class_name Lexicon
extends RefCounted
## Lexikon (seit M13, Schritt a5d; vertical_slice.md 4.2): Artikel als Markdown in data/lexikon/<kennung>.md,
## gelesen wie die Kartendateien. Erste Zeile „# Titel“, dann Absätze; **fett**, *kursiv* und Verweise
## [[kennung]] oder [[kennung|Text]]. Für die Anzeige wird daraus BBCode mit [url=begriff:kennung].

## kennung → { title, body (Markdown ohne Titel), path }
var articles := {}
var problems := PackedStringArray()


static func load_dir(dir_path: String) -> Lexicon:
	var lexicon := Lexicon.new()
	var dir := DirAccess.open(dir_path)
	if dir == null:
		return lexicon
	var names := Array(dir.get_files())
	names.sort()
	for file_name: String in names:
		if not file_name.ends_with(".md"):
			continue
		var id := file_name.get_basename()
		var text := FileAccess.get_file_as_string(dir_path.path_join(file_name))
		var lines := text.split("\n")
		if lines.is_empty() or not lines[0].begins_with("# "):
			lexicon.problems.append("%s: erste Zeile muss „# Titel“ sein" % file_name)
			continue
		lexicon.articles[id] = {
			"title": lines[0].substr(2).strip_edges(),
			"body": "\n".join(lines.slice(1)).strip_edges(),
			"path": dir_path.path_join(file_name),
		}
	# Verweise auf fehlende Artikel melden.
	var link := RegEx.create_from_string("\\[\\[([a-z0-9_]+)(\\|[^\\]]*)?\\]\\]")
	for id: String in lexicon.articles:
		for found in link.search_all(str(lexicon.articles[id]["body"])):
			if not lexicon.articles.has(found.get_string(1)):
				lexicon.problems.append("%s: Verweis auf fehlenden Artikel „%s“" % [id, found.get_string(1)])
	return lexicon


func has(id: String) -> bool:
	return articles.has(id)


func title(id: String) -> String:
	return str(articles.get(id, {}).get("title", ""))


## Kennungen nach Titel geordnet, optional nur mit dem Suchwort in Titel oder Text.
func ids(query: String = "") -> PackedStringArray:
	var result: Array[String] = []
	var needle := query.strip_edges().to_lower()
	for id: String in articles:
		var article: Dictionary = articles[id]
		if needle.is_empty() or str(article["title"]).to_lower().contains(needle) or str(article["body"]).to_lower().contains(needle):
			result.append(id)
	result.sort_custom(func(a: String, b: String) -> bool: return title(a).naturalnocasecmp_to(title(b)) < 0)
	return PackedStringArray(result)


## Artikel als BBCode (Titel fett, Verweise unterstrichen).
func bbcode(id: String) -> String:
	if not articles.has(id):
		return ""
	return "[font_size=20][b]%s[/b][/font_size]\n\n%s" % [title(id), to_bbcode(str(articles[id]["body"]))]


static func to_bbcode(markdown: String) -> String:
	var text := markdown.replace("[", "\u0001").replace("]", "\u0002")
	text = text.replace("\u0001\u0001", "[[").replace("\u0002\u0002", "]]")
	var link := RegEx.create_from_string("\\[\\[([a-z0-9_]+)(?:\\|([^\\]]*))?\\]\\]")
	var out := ""
	var at := 0
	for found in link.search_all(text):
		out += text.substr(at, found.get_start() - at)
		var label := found.get_string(2) if not found.get_string(2).is_empty() else found.get_string(1)
		out += "[url=begriff:%s]%s[/url]" % [found.get_string(1), label]
		at = found.get_end()
	out += text.substr(at)
	out = RegEx.create_from_string("\\*\\*([^*]+)\\*\\*").sub(out, "[b]$1[/b]", true)
	out = RegEx.create_from_string("\\*([^*]+)\\*").sub(out, "[i]$1[/i]", true)
	return out.replace("\u0001", "[lb]").replace("\u0002", "[rb]")
