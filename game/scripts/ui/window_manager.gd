class_name WindowManager
extends RefCounted
## Fensterverwaltung (seit M13, Schritt a5a; vertical_slice.md 4.1 und 9): Die Fenster der Bereiche
## (Diplomatie, Krieg, Journale, Jahrbuch …) teilen sich einen Platz neben der linken Spalte; es ist
## höchstens eines offen. Escape schließt das zuletzt geöffnete. Ein Fenster braucht open(…), close() und
## is_open(); hat es fit_height(höhe), passt es seine Höhe dem Platz an.

var _windows: Array[Control] = []
## Reihenfolge des Öffnens, das jüngste zuletzt.
var _order: Array[Control] = []


func add(window: Control) -> void:
	if window != null and not _windows.has(window):
		_windows.append(window)


func windows() -> Array[Control]:
	return _windows


## Vor dem Öffnen eines Fensters: alle anderen schließen und es als jüngstes merken.
func opening(window: Control) -> void:
	for other in _windows:
		if other != window and other.call(&"is_open"):
			other.call(&"close")
	_order.erase(window)
	_order.append(window)


## Das offene Fenster oder null.
func open_window() -> Control:
	for i in range(_order.size() - 1, -1, -1):
		if _order[i].call(&"is_open"):
			return _order[i]
	for window in _windows:
		if window.call(&"is_open"):
			return window
	return null


## Escape: schließt das jüngste offene Fenster; true, wenn eines offen war.
func close_top() -> bool:
	var window := open_window()
	if window == null:
		return false
	window.call(&"close")
	_order.erase(window)
	return true


func close_all() -> void:
	for window in _windows:
		if window.call(&"is_open"):
			window.call(&"close")
	_order.clear()


## Alle Fenster an den gemeinsamen Platz: links oben bei (left, top), höchstens bis bottom.
func place(left: float, top: float, bottom: float) -> void:
	for window in _windows:
		if window.call(&"is_open") and window.has_method(&"fit_height"):
			window.call(&"fit_height", bottom - top)
		window.size = window.get_combined_minimum_size()
		window.position = Vector2(left, top)
