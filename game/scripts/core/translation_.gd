class_name Translation_
extends RefCounted
## Sprache der Oberfläche (seit M13b4, vertical_slice.md 6): Übersetzungen aus game/locale (gettext,
## Deutsch als Quellsprache) und Zahlen und Daten (Format). „pseudo“ ist das Pseudo-Locale für Tests:
## jeder übersetzte Text wird um 40 % länger und bekommt Akzente, damit abgeschnittene Beschriftungen auffallen.

const LANGUAGES: PackedStringArray = ["de", "en"]


## Sprache setzen: "de", "en" oder "pseudo" (Deutsch mit Pseudo-Locale).
static func use(code: String) -> void:
	var pseudo := code == "pseudo"
	var language := "de" if pseudo or not LANGUAGES.has(code) else code
	Format.set_locale(language)
	TranslationServer.set_locale(language)
	set_pseudo(pseudo)


## Pseudo-Locale an oder aus (Kapitel 22, Abschnitt 8): längere Texte mit Akzenten und Klammern.
static func set_pseudo(on: bool) -> void:
	ProjectSettings.set_setting("internationalization/pseudolocalization/use_pseudolocalization", on)
	ProjectSettings.set_setting("internationalization/pseudolocalization/expansion_ratio", 0.4)
	ProjectSettings.set_setting("internationalization/pseudolocalization/replace_with_accents", true)
	ProjectSettings.set_setting("internationalization/pseudolocalization/prefix", "[")
	ProjectSettings.set_setting("internationalization/pseudolocalization/suffix", "]")
	ProjectSettings.set_setting("internationalization/pseudolocalization/skip_placeholders", true)
	TranslationServer.pseudolocalization_enabled = on
	TranslationServer.reload_pseudolocalization()


static func is_pseudo() -> bool:
	return TranslationServer.pseudolocalization_enabled
