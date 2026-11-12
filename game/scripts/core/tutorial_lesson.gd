class_name TutorialLesson
extends Resource
## Geschichtsstunde (seit M13b3, vertical_slice.md 5.4): ein Lesezeichen, ein Staat und die Schritte. Die
## Texte stehen in der Ressource (game/tutorial/*.tres) und laufen über tr() (Lokalisierung mit b4).

@export var id := ""
@export var title := ""
@export_multiline var intro := ""
@export var scenario := "voelkerfruehling"
@export var state := "PRU"
@export var difficulty := "lehrling"
@export var steps: Array[TutorialStep] = []


## Fehler der Daten (leer: in Ordnung): unbekannte Prüfungen und Ziele, doppelte Kennungen, leere Texte.
func problems() -> PackedStringArray:
	var result := PackedStringArray()
	var ids := {}
	if steps.is_empty():
		result.append("keine Schritte")
	for i in steps.size():
		var step := steps[i]
		var where := "Schritt %d (%s)" % [i + 1, step.id if step != null else "leer"]
		if step == null:
			result.append(where + ": fehlt")
			continue
		if step.id.is_empty() or ids.has(step.id):
			result.append(where + ": Kennung leer oder doppelt")
		ids[step.id] = true
		if step.title.is_empty() or step.text.is_empty():
			result.append(where + ": Titel oder Text fehlt")
		if not TutorialStep.CHECKS.has(step.check):
			result.append(where + ": unbekannte Prüfung „%s“" % step.check)
		if not TutorialStep.TARGETS.has(step.target):
			result.append(where + ": unbekanntes Ziel „%s“" % step.target)
		if step.check != "weiter" and step.task.is_empty():
			result.append(where + ": Aufgabe fehlt")
	return result
