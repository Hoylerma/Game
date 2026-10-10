// Kartenansicht (Fassung M3): dekodiert das Ort-ID-Bild, färbt jeden Ort über
// Nachschlagetexturen ein und zeichnet Meer, Seen, Ausland, Flüsse, abgestufte
// Grenzen, Küste, die Schraffur „keine Daten“ und Hervorhebungen.
//
// provinzen.png: Kartenindex + 1 = R + G * 256 (0 = kein Ort), B = Flächenart
//   (0 Kernraum-Land, 1 Meer, 2 Binnengewässer, 3 Ausland).
// fluesse.png: Deckung durch Flüsse (0..1), mit Mipmaps.
// Nachschlagetexturen mit lut_columns Spalten; Kartenindex i liegt bei
// (i % lut_columns, i / lut_columns):
//   color_lut – Farbe im aktuellen Kartenmodus; Alpha 1 = Farbe, 0 = keine Daten
//               (Schraffur), 0,5 = Ort der Karte, den die Simulation nicht kennt
//   info_lut  – Besitzer + 1 als R + G * 256, Kreis + 1 als B + A * 256 (0 = keiner)
// Grenzen werden im Bildschirmraum gesucht (Nachbarn im Abstand eines
// Bildschirmpixels), damit die Linien bei jedem Zoom gleich dick bleiben.
// Kreis- und Ortsgrenzen blendet die Kartenansicht je nach Zoom ein (MapDetail).
//
// Leistung: Besitzer und Kreis der Nachbarn werden nur gelesen, wenn sich ihr
// Kartenindex unterscheidet; im Inneren eines Ortes entfällt die Grenzsuche ganz.
shader_type canvas_item;
render_mode unshaded;

uniform sampler2D id_map : filter_nearest, repeat_disable;
uniform sampler2D river_map : hint_default_black, filter_linear_mipmap, repeat_disable;
uniform sampler2D color_lut : filter_nearest, repeat_disable;
uniform sampler2D info_lut : filter_nearest, repeat_disable;
uniform sampler2D grain_map : hint_default_white, filter_linear, repeat_enable;
uniform int lut_columns = 256;
// Größe von provinzen.png in Pixeln (gesetzt von der Kartenansicht).
uniform ivec2 map_size = ivec2(1, 1);

// Kartenindizes (nicht Simulationsindizes); -1 = keiner.
uniform int selected_index = -1;
uniform int hovered_index = -1;

// Deckkraft der zoomabhängigen Grenzen (0..1), gesetzt von der Kartenansicht.
uniform float kreis_border_alpha = 0.0;
uniform float location_border_alpha = 0.0;

// Flächen.
uniform vec4 sea_color = vec4(0.608, 0.690, 0.706, 1.0);
uniform vec4 lake_color = vec4(0.655, 0.737, 0.745, 1.0);
uniform vec4 foreign_color = vec4(0.773, 0.761, 0.725, 1.0);
uniform vec4 no_data_color = vec4(0.910, 0.882, 0.812, 1.0);
uniform vec4 unknown_color = vec4(0.62, 0.60, 0.56, 1.0);
uniform vec4 river_color = vec4(0.333, 0.471, 0.553, 0.9);

// Schraffur „keine Daten“: Linienabstand in Bildschirmpixeln, Linienanteil.
uniform vec4 hatch_color = vec4(0.42, 0.35, 0.26, 0.30);
uniform float hatch_spacing = 7.0;
uniform float hatch_width : hint_range(0.05, 0.5) = 0.2;

// Papierton und Körnung.
uniform vec4 paper_color = vec4(0.953, 0.918, 0.835, 1.0);
uniform float paper_strength : hint_range(0.0, 1.0) = 0.16;
uniform float grain_strength : hint_range(0.0, 0.2) = 0.035;

// Linien (Alpha = Deckkraft).
uniform vec4 state_border_color = vec4(0.169, 0.129, 0.086, 0.85);
uniform vec4 kreis_border_color = vec4(0.169, 0.129, 0.086, 0.42);
uniform vec4 location_border_color = vec4(0.169, 0.129, 0.086, 0.2);
uniform vec4 coast_color = vec4(0.169, 0.129, 0.086, 0.9);
uniform vec4 selection_color = vec4(0.557, 0.169, 0.114, 1.0);
uniform vec4 selection_glow = vec4(1.0, 0.973, 0.894, 0.2);
uniform vec4 hover_glow = vec4(1.0, 0.973, 0.894, 0.24);

// Abstand der Grenznachbarn und Breite des Auswahlumrisses in Bildschirmpixeln.
uniform float line_width = 1.0;
uniform float selection_width = 3.0;

const int AREA_SEA = 1;
const int AREA_INLAND_WATER = 2;
const int AREA_FOREIGN = 3;
const int AREA_OUTSIDE = -1;

// (Kartenindex oder -1, Flächenart oder AREA_OUTSIDE) am Bildpunkt.
ivec2 cell_at(vec2 texel_pos) {
	ivec2 p = ivec2(floor(texel_pos));
	if (p.x < 0 || p.y < 0 || p.x >= map_size.x || p.y >= map_size.y) {
		return ivec2(-1, AREA_OUTSIDE);
	}
	vec4 c = texelFetch(id_map, p, 0);
	int value = int(c.r * 255.0 + 0.5) + int(c.g * 255.0 + 0.5) * 256;
	return ivec2(value - 1, int(c.b * 255.0 + 0.5));
}

ivec2 lut_texel(int map_index) {
	return ivec2(map_index % lut_columns, map_index / lut_columns);
}

// (Besitzer, Kreis) eines Kartenindex; -1 = keiner.
ivec2 info_of(int map_index) {
	if (map_index < 0) {
		return ivec2(-1, -1);
	}
	vec4 c = texelFetch(info_lut, lut_texel(map_index), 0);
	return ivec2(
		int(c.r * 255.0 + 0.5) + int(c.g * 255.0 + 0.5) * 256 - 1,
		int(c.b * 255.0 + 0.5) + int(c.a * 255.0 + 0.5) * 256 - 1);
}

bool is_water(ivec2 cell) {
	return cell.y == AREA_SEA || cell.y == AREA_INLAND_WATER;
}

bool is_land(ivec2 cell) {
	return cell.y == 0 || cell.y == AREA_FOREIGN;
}

// Gebiet für Staatsgrenzen: Besitzer (−1 ohne), Ausland −2.
int region_of(ivec2 cell, ivec2 info) {
	return cell.y == AREA_FOREIGN ? -2 : info.x;
}

vec3 blend(vec3 base, vec4 over) {
	return mix(base, over.rgb, over.a);
}

// Eine Streifenschar mit Periode period (Bildpixel); px = Bildpixel je Bildschirmpixel.
float stripe(float u, float period, float px) {
	float f = fract(u / period);
	float edge = clamp(px / period, 0.001, 0.5);
	return smoothstep(0.0, edge, f) - smoothstep(hatch_width, hatch_width + edge, f);
}

// Diagonale Schraffur, an der Karte verankert. Der Abstand bleibt auf dem Bildschirm
// etwa gleich: Die Periode springt in Zweierpotenzen und wird dazwischen überblendet.
float hatch(vec2 texel, float px) {
	float u = (texel.x + texel.y) * 0.70710678;
	float level = log2(max(px * hatch_spacing, 0.0001));
	float l0 = floor(level);
	float t = level - l0;
	return mix(stripe(u, exp2(l0), px), stripe(u, exp2(l0 + 1.0), px), t);
}

void fragment() {
	vec2 texel = UV * vec2(map_size);
	// Ein Bildschirmpixel in Bildkoordinaten (vor jeder Verzweigung berechnet).
	vec2 step_x = dFdx(texel);
	vec2 step_y = dFdy(texel);
	float px = max(length(step_x), length(step_y));
	float river = texture(river_map, UV).r;
	float grain = texture(grain_map, texel / 5.0).r;

	ivec2 here = cell_at(texel);
	vec3 color;
	if (here.y == AREA_SEA) {
		color = sea_color.rgb;
	} else if (here.y == AREA_INLAND_WATER) {
		color = lake_color.rgb;
	} else if (here.y == AREA_FOREIGN) {
		color = foreign_color.rgb;
	} else if (here.x < 0) {
		// Kernraum-Land ohne Ort (z. B. kleine Inseln): wie „keine Daten“.
		color = blend(no_data_color.rgb, vec4(hatch_color.rgb, hatch_color.a * hatch(texel, px)));
	} else {
		vec4 lut = texelFetch(color_lut, lut_texel(here.x), 0);
		if (lut.a > 0.75) {
			color = lut.rgb;
		} else if (lut.a > 0.25) {
			color = unknown_color.rgb;
		} else {
			color = blend(no_data_color.rgb, vec4(hatch_color.rgb, hatch_color.a * hatch(texel, px)));
		}
		if (here.x == hovered_index) {
			color = blend(color, hover_glow);
		}
		if (here.x == selected_index) {
			color = blend(color, selection_glow);
		}
	}

	if (here.y != AREA_SEA) {
		color = blend(color, vec4(river_color.rgb, river_color.a * clamp(river * 1.4, 0.0, 1.0)));
	}

	// Nachbarn im Abstand eines Bildschirmpixels.
	vec2 sx = step_x * line_width;
	vec2 sy = step_y * line_width;
	ivec2 right = cell_at(texel + sx);
	ivec2 left = cell_at(texel - sx);
	ivec2 down = cell_at(texel + sy);
	ivec2 up = cell_at(texel - sy);

	bool inside = right == here && left == here && down == here && up == here;
	if (!inside && is_land(here)) {
		ivec2 here_info = info_of(here.x);
		ivec2 right_info = right.x == here.x ? here_info : info_of(right.x);
		ivec2 down_info = down.x == here.x ? here_info : info_of(down.x);
		ivec2 left_info = left.x == here.x ? here_info : info_of(left.x);
		ivec2 up_info = up.x == here.x ? here_info : info_of(up.x);
		// Ortsgrenze, nur zu einer Seite (rechts, unten): ein Pixel breit.
		if (location_border_alpha > 0.0 && here.x >= 0
				&& ((right.y == 0 && right.x != here.x) || (down.y == 0 && down.x != here.x))) {
			color = blend(color, vec4(location_border_color.rgb,
					location_border_color.a * location_border_alpha));
		}
		// Kreisgrenze, ebenfalls einseitig.
		if (kreis_border_alpha > 0.0 && here.y == 0
				&& ((right.y == 0 && right_info.y != here_info.y)
						|| (down.y == 0 && down_info.y != here_info.y))) {
			color = blend(color,
					vec4(kreis_border_color.rgb, kreis_border_color.a * kreis_border_alpha));
		}
		// Staatsgrenze (auch Kernraum ↔ Ausland): beide Seiten, dadurch kräftiger.
		int region = region_of(here, here_info);
		if ((is_land(right) && region_of(right, right_info) != region)
				|| (is_land(left) && region_of(left, left_info) != region)
				|| (is_land(down) && region_of(down, down_info) != region)
				|| (is_land(up) && region_of(up, up_info) != region)) {
			color = blend(color, state_border_color);
		}
	}
	// Küste und Ufer: Land neben Wasser, auf beiden Seiten.
	if (!inside) {
		bool coast = is_land(here)
				? (is_water(right) || is_water(left) || is_water(down) || is_water(up))
				: (is_water(here)
						&& (is_land(right) || is_land(left) || is_land(down) || is_land(up)));
		if (coast) {
			color = blend(color, coast_color);
		}
	}

	// Umriss des ausgewählten Ortes (innen).
	if (here.x >= 0 && here.x == selected_index) {
		vec2 ex = step_x * selection_width;
		vec2 ey = step_y * selection_width;
		if (cell_at(texel + ex).x != here.x || cell_at(texel - ex).x != here.x
				|| cell_at(texel + ey).x != here.x || cell_at(texel - ey).x != here.x
				|| cell_at(texel + ex + ey).x != here.x || cell_at(texel - ex - ey).x != here.x
				|| cell_at(texel + ex - ey).x != here.x || cell_at(texel - ex + ey).x != here.x) {
			color = selection_color.rgb;
		}
	}

	// Papierton: leichte Angleichung an das Papier und feine Körnung.
	color = mix(color, paper_color.rgb, paper_strength);
	color *= 1.0 - grain_strength * (grain - 0.5) * 2.0;
	COLOR = vec4(color, 1.0);
}
