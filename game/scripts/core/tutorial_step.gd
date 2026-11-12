class_name TutorialStep
extends Resource
## Ein Schritt der Geschichtsstunde (seit M13b3, vertical_slice.md 5.4): Anleitung, Aufgabe und ihre Prüfung.
## Die Prüfungen kennt die Hauptszene (main.gd, _tutorial_check); „weiter“ ist ein Schritt zum Lesen.

## Prüfungen, die die Hauptszene kennt.
const CHECKS: PackedStringArray = [
	"weiter", "kartenmodus", "ort", "haushalt", "steuern", "kabinett", "gesandter", "zollunion",
	"diplomatie", "tempo", "ereignis", "wahl", "journal", "speichern",
]
## Elemente, die die Geschichtsstunde hervorheben kann (main.gd, tutorial_target).
const TARGETS: PackedStringArray = [
	"", "kartenmodi", "staatsuebersicht", "steuern", "tempo", "menue", "bereich:kabinett", "bereich:haushalt",
	"bereich:diplomatie", "bereich:zollunion", "bereich:journale", "uebersicht",
]

@export var id := ""
@export var title := ""
## Anleitung (zwei bis vier Sätze).
@export_multiline var text := ""
## Was zu tun ist; leer bei einem Schritt zum Lesen.
@export var task := ""
## Prüfung aus CHECKS.
@export var check := "weiter"
## Die Zeit darf laufen, solange der Schritt offen ist.
@export var time := false
## Hervorgehobenes Element aus TARGETS.
@export var target := ""
## Artikel im Lexikon (data/lexikon, ohne Endung), leer: keiner.
@export var lexicon := ""
## Satz nach der erledigten Aufgabe.
@export var done_text := ""
