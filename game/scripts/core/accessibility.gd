class_name Accessibility
extends RefCounted
## Barrierefreiheit (seit M13, Schritt a5d; vertical_slice.md 4.5): Farben für Farbsehschwäche, hoher
## Kontrast, reduzierte Animationen. Die Schalter sind statisch, weil Karte, Legende und Fenster sie an
## vielen Stellen lesen; UserSettings.apply setzt sie.

## Rot-Grün-Schwäche: Grüntöne werden zu Blau, Rot bleibt; so unterscheiden sich die Paletten der
## Kartenmodi (Beziehungen, Unruhe, Wahlen) auch ohne Rot-Grün-Sehen, und die Helligkeit bleibt.
static var color_blind := false
static var high_contrast := false
static var reduced_motion := false


static func adjust(color: Color) -> Color:
	if not color_blind or color.a <= 0.0:
		return color
	var hue := color.h * 360.0
	if hue >= 60.0 and hue <= 170.0 and color.s > 0.12:
		# Grün (60°–170°) auf Blau (200°–230°) abbilden, Sättigung und Wert behalten.
		var t := (hue - 60.0) / 110.0
		var adjusted := Color.from_hsv((200.0 + 30.0 * t) / 360.0, color.s, color.v, color.a)
		return adjusted
	return color


static func adjust_all(colors: PackedColorArray) -> PackedColorArray:
	if not color_blind:
		return colors
	var out := PackedColorArray()
	out.resize(colors.size())
	for i in colors.size():
		out[i] = adjust(colors[i])
	return out
