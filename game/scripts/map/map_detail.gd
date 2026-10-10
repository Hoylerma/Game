class_name MapDetail
extends RefCounted
## Detailstufen der Karte nach Zoom: welche Grenzen und Namen erscheinen.
##
## Maß ist die Bildschirmgröße eines typischen Ortes: s = Zoom × typischer
## Ortsdurchmesser in Bildpixeln (MapData.typical_location_diameter). Damit gelten
## die Stufen für jede Kartenauflösung gleich. Auf der Kernraum-Karte
## (4096 × 3329 Pixel, Median 1.212 Pixel je Ort) ist der Durchmesser etwa 35 Pixel;
## die Übersicht (Zoom ≈ 0,18 bei 1600 × 900 Pixeln) zeigt einen Ort etwa 6 Pixel groß.
##
##   fern   (s < 17)   Staatsgrenzen, Staatsnamen
##   mittel (17 … 40)  dazu Kreisgrenzen (ab s ≈ 10 eingeblendet), Kreisnamen
##   nah    (s ≥ 40)   dazu Ortsgrenzen (ab s ≈ 28 eingeblendet), Ortsnamen

enum Tier { STATE, KREIS, PLACE }

## Bildschirmgröße eines Ortes, zwischen der Kreisgrenzen ein- und vollständig erscheinen.
const KREIS_BORDER_FADE := Vector2(10.0, 18.0)
## Dasselbe für Ortsgrenzen.
const LOCATION_BORDER_FADE := Vector2(28.0, 42.0)
## Ab dieser Ortsgröße (Pixel) erscheinen Kreisnamen statt Staatsnamen …
const KREIS_LABELS_FROM := 17.0
## … und ab dieser Ortsnamen statt Kreisnamen. Die Ebenen wechseln hart: Eine
## Überblendung zeigte beide Ebenen halb durchsichtig übereinander, und ein
## Mausradschritt kann genau dort stehen bleiben.
const PLACE_LABELS_FROM := 40.0
## Repräsentative Ortsgrößen der drei Stufen (Screenshots, Klicktest).
const LEVEL_SIZES: PackedFloat32Array = [8.0, 26.0, 64.0]
const LEVEL_NAMES: PackedStringArray = ["fern", "mittel", "nah"]


## Bildschirmgröße eines typischen Ortes beim Zoom zoom.
static func screen_size(zoom: float, diameter: float) -> float:
	return zoom * diameter


## Zoom, bei dem ein typischer Ort screen_px Bildschirmpixel groß ist.
static func zoom_for(screen_px: float, diameter: float) -> float:
	return screen_px / maxf(diameter, 0.001)


## Deckkraft der Kreisgrenzen (0..1).
static func kreis_border_alpha(size: float) -> float:
	return smoothstep(KREIS_BORDER_FADE.x, KREIS_BORDER_FADE.y, size)


## Deckkraft der Ortsgrenzen (0..1).
static func location_border_alpha(size: float) -> float:
	return smoothstep(LOCATION_BORDER_FADE.x, LOCATION_BORDER_FADE.y, size)


## Beschriftungsebene (Tier) bei der Ortsgröße size.
static func label_tier(size: float) -> int:
	if size >= PLACE_LABELS_FROM:
		return Tier.PLACE
	if size >= KREIS_LABELS_FROM:
		return Tier.KREIS
	return Tier.STATE


## Deckkraft einer Beschriftungsebene: 1 für die Ebene der Ortsgröße size, sonst 0.
static func tier_alpha(tier: int, size: float) -> float:
	return 1.0 if tier == label_tier(size) else 0.0


## Name der Stufe („fern“, „mittel“, „nah“) für Meldungen.
static func level_name(size: float) -> String:
	return LEVEL_NAMES[label_tier(size)]
