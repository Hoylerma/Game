class_name TutorialPanel
extends PanelContainer
## Geschichtsstunde (seit M13b3, vertical_slice.md 5.4): zeigt den Schritt mit Anleitung und Aufgabe. Die
## Hauptszene prüft die Aufgabe (mark_done); „Weiter“ geht erst danach. Ein Schritt zum Lesen („weiter“)
## ist sofort erledigt.

signal step_entered(step: TutorialStep)
signal lexicon_requested(article: String)
## Ende der Geschichtsstunde: completed = alle Schritte, sonst abgebrochen.
signal closed(completed: bool)

const WIDTH := 380.0
const DONE_COLOR := Color(0.24, 0.45, 0.2)

var _lesson: TutorialLesson = null
var _index := -1
var _done := false
var _header: Label = null
var _title: Label = null
var _text: Label = null
var _task: Label = null
var _lexicon: Button = null
var _next: Button = null
var _end: Button = null


func _ready() -> void:
	name = "TutorialPanel"
	custom_minimum_size = Vector2(WIDTH, 0)
	visible = false
	mouse_filter = Control.MOUSE_FILTER_STOP
	_build()
	EpochTheme.frame(self)


func start(lesson: TutorialLesson) -> void:
	_lesson = lesson
	_index = -1
	visible = true
	_enter(0)


func is_active() -> bool:
	return visible and _lesson != null and _index >= 0


func lesson() -> TutorialLesson:
	return _lesson


func step_index() -> int:
	return _index


func step_count() -> int:
	return _lesson.steps.size() if _lesson != null else 0


func current_step() -> TutorialStep:
	return _lesson.steps[_index] if is_active() else null


func is_done() -> bool:
	return _done


## Die Aufgabe des Schritts ist erledigt (bleibt erledigt).
func mark_done() -> void:
	if not is_active() or _done:
		return
	_done = true
	_show_state()


## Weiter zum nächsten Schritt (nur, wenn der Schritt erledigt ist); nach dem letzten das Ende.
func advance() -> bool:
	if not is_active() or not _done:
		return false
	if _index + 1 >= _lesson.steps.size():
		_close(true)
	else:
		_enter(_index + 1)
	return true


func stop() -> void:
	if is_active():
		_close(false)


## Knopf (Tests): Next, End, Lexicon.
func button(node_name: String) -> Button:
	return find_child(node_name, true, false) as Button


func panel_text() -> String:
	return "\n".join([_header.text, _title.text, _text.text, _task.text])


func _enter(index: int) -> void:
	_index = index
	var step := current_step()
	_done = step.check == "weiter"
	_header.text = tr("GESCHICHTSSTUNDE · %s · Schritt %d von %d") % [tr(_lesson.title), index + 1, step_count()]
	_title.text = tr(step.title)
	_text.text = tr(step.text)
	_lexicon.visible = not step.lexicon.is_empty()
	_next.text = tr("Zum Spiel") if index + 1 == step_count() else tr("Weiter")
	_show_state()
	reset_size()
	step_entered.emit(step)


func _show_state() -> void:
	var step := current_step()
	if step == null:
		return
	if _done:
		_task.text = ("✓ " + tr(step.done_text)) if not step.done_text.is_empty() else ""
		_task.add_theme_color_override(&"font_color", DONE_COLOR)
	else:
		_task.text = tr("Aufgabe: ") + tr(step.task)
		_task.remove_theme_color_override(&"font_color")
	_task.visible = not _task.text.is_empty()
	_next.disabled = not _done


func _close(completed: bool) -> void:
	_index = -1
	visible = false
	closed.emit(completed)


func _build() -> void:
	var box := VBoxContainer.new()
	box.add_theme_constant_override(&"separation", 6)
	add_child(box)
	_header = Label.new()
	_header.theme_type_variation = &"CaptionLabel"
	_header.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	box.add_child(_header)
	_title = Label.new()
	_title.name = "Title"
	_title.theme_type_variation = &"NameLabel"
	_title.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	box.add_child(_title)
	_text = Label.new()
	_text.name = "Text"
	_text.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_text.custom_minimum_size = Vector2(WIDTH - 30.0, 0)
	box.add_child(_text)
	_task = Label.new()
	_task.name = "Task"
	_task.theme_type_variation = &"ValueLabel"
	_task.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_task.custom_minimum_size = Vector2(WIDTH - 30.0, 0)
	box.add_child(_task)
	var buttons := HBoxContainer.new()
	buttons.add_theme_constant_override(&"separation", 6)
	_lexicon = Button.new()
	_lexicon.name = "Lexicon"
	_lexicon.text = tr("Im Lexikon")
	_lexicon.focus_mode = Control.FOCUS_NONE
	_lexicon.pressed.connect(func() -> void: lexicon_requested.emit(current_step().lexicon))
	buttons.add_child(_lexicon)
	var spacer := Control.new()
	spacer.size_flags_horizontal = Control.SIZE_EXPAND_FILL
	buttons.add_child(spacer)
	_end = Button.new()
	_end.name = "End"
	_end.text = tr("Beenden")
	_end.focus_mode = Control.FOCUS_NONE
	_end.tooltip_text = tr("Die Geschichtsstunde beenden und frei weiterspielen")
	_end.pressed.connect(stop)
	buttons.add_child(_end)
	_next = Button.new()
	_next.name = "Next"
	_next.focus_mode = Control.FOCUS_NONE
	_next.pressed.connect(func() -> void: advance())
	buttons.add_child(_next)
	box.add_child(buttons)
