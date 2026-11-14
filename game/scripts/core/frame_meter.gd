class_name FrameMeter
extends Node
## Bildrate über die Sitzung (seit M13b6, vertical_slice.md 10): zählt die Bilder je Sekunde und behält
## bis zu MAX_SAMPLES Sekunden. Die Rückmeldung (FeedbackReport) meldet Median, 5-%-Perzentil und
## Messdauer – so misst der Spieltest die Bildrate auf echter Hardware. Sekunden mit minimiertem oder
## nicht fokussiertem Fenster zählen nicht; ohne Grafikausgabe (headless) misst der Zähler nichts.

## Vier Stunden.
const MAX_SAMPLES := 14400

var _samples := PackedFloat32Array()
var _frames := 0
var _elapsed := 0.0
var _started_msec := 0


func _ready() -> void:
	name = "FrameMeter"
	process_mode = Node.PROCESS_MODE_ALWAYS
	_started_msec = Time.get_ticks_msec()


func _process(delta: float) -> void:
	if DisplayServer.get_name() == "headless" or not _window_counts():
		_frames = 0
		_elapsed = 0.0
		return
	_frames += 1
	_elapsed += delta
	if _elapsed >= 1.0:
		add_sample(_frames / _elapsed)
		_frames = 0
		_elapsed = 0.0


## Messwert hinzufügen (Bilder je Sekunde); auch für Tests.
func add_sample(fps: float) -> void:
	if _samples.size() >= MAX_SAMPLES:
		_samples.remove_at(0)
	_samples.append(fps)


## { seconds (gemessene Sekunden), median, p5 (5 % der Sekunden waren langsamer), min, session_seconds }.
func summary() -> Dictionary:
	var sorted := _samples.duplicate()
	sorted.sort()
	return {
		"seconds": sorted.size(),
		"median": percentile(sorted, 50.0),
		"p5": percentile(sorted, 5.0),
		"min": sorted[0] if not sorted.is_empty() else 0.0,
		"session_seconds": (Time.get_ticks_msec() - _started_msec) / 1000,
	}


## Perzentil p (0–100) einer aufsteigend sortierten Reihe (nächster Rang); 0 bei leerer Reihe.
static func percentile(sorted: PackedFloat32Array, p: float) -> float:
	if sorted.is_empty():
		return 0.0
	var rank := clampi(ceili(p / 100.0 * sorted.size()) - 1, 0, sorted.size() - 1)
	return sorted[rank]


func _window_counts() -> bool:
	var window := get_window()
	if window == null:
		return true
	return window.mode != Window.MODE_MINIMIZED and window.has_focus()
