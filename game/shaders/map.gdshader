// Kartenansicht: dekodiert das Ort-ID-Bild, färbt jeden Ort über eine
// Nachschlagetextur ein und zeichnet Grenzen, Küste und Hervorhebungen.
//
// ID-Bild (provinzen.png): Kartenindex + 1 = R + G * 256 (0 = kein Ort).
// Nachschlagetexturen sind 1 Pixel hoch, Breite = Anzahl Kartenindizes:
//   color_lut  – Farbe des Ortes im aktuellen Kartenmodus (Alpha 0 = unbekannt)
//   owner_lut  – Besitzer (Staatsindex + 1) als R + G * 256
// Grenzen werden im Bildschirmraum gesucht (Nachbarn im Abstand von
// Bildschirmpixeln), damit die Linien bei jedem Zoom gleich dick bleiben.
shader_type canvas_item;

const int MAX_COAST_POINTS = 128;

uniform sampler2D id_map : filter_nearest, repeat_disable;
uniform sampler2D color_lut : filter_nearest, repeat_disable;
uniform sampler2D owner_lut : filter_nearest, repeat_disable;

// Kartenindizes (nicht Simulationsindizes); -1 = keiner.
uniform int selected_index = -1;
uniform int hovered_index = -1;

// Gebiete ohne Ort: nördlich der Küstenlinie Meer, sonst unerfasstes Land.
uniform vec4 sea_color = vec4(0.608, 0.690, 0.706, 1.0);
uniform vec4 empty_land_color = vec4(0.867, 0.835, 0.753, 1.0);
// Ort der Karte, den die Simulation nicht kennt.
uniform vec4 unknown_color = vec4(0.62, 0.60, 0.56, 1.0);

// Papierton und Körnung.
uniform vec4 paper_color = vec4(0.953, 0.918, 0.835, 1.0);
uniform float paper_strength : hint_range(0.0, 1.0) = 0.16;
uniform float grain_strength : hint_range(0.0, 0.2) = 0.035;

// Linien (Alpha = Deckkraft).
uniform vec4 state_border_color = vec4(0.169, 0.129, 0.086, 0.85);
uniform vec4 location_border_color = vec4(0.169, 0.129, 0.086, 0.26);
uniform vec4 coast_color = vec4(0.169, 0.129, 0.086, 0.95);
uniform vec4 hatch_color = vec4(0.42, 0.35, 0.26, 0.16);
uniform vec4 selection_color = vec4(0.557, 0.169, 0.114, 1.0);
uniform vec4 selection_glow = vec4(1.0, 0.973, 0.894, 0.2);
uniform vec4 hover_glow = vec4(1.0, 0.973, 0.894, 0.24);

// Breiten in Bildschirmpixeln.
uniform float state_border_width = 1.0;
uniform float coast_width = 2.0;
uniform float selection_width = 3.0;

// Geschlossenes Meerespolygon in Bildkoordinaten (Küste + obere Bildecken).
uniform int coast_point_count = 0;
uniform vec2 coast_points[MAX_COAST_POINTS];

int decode_id(vec2 texel_pos) {
	ivec2 size = textureSize(id_map, 0);
	ivec2 p = ivec2(floor(texel_pos));
	if (p.x < 0 || p.y < 0 || p.x >= size.x || p.y >= size.y) {
		return -1;
	}
	vec4 c = texelFetch(id_map, p, 0);
	return int(c.r * 255.0 + 0.5) + int(c.g * 255.0 + 0.5) * 256 - 1;
}

int owner_of(int map_index) {
	if (map_index < 0 || map_index >= textureSize(owner_lut, 0).x) {
		return -1;
	}
	vec4 c = texelFetch(owner_lut, ivec2(map_index, 0), 0);
	return int(c.r * 255.0 + 0.5) + int(c.g * 255.0 + 0.5) * 256 - 1;
}

bool is_sea(vec2 p) {
	if (coast_point_count < 3) {
		return true;
	}
	bool inside = false;
	for (int i = 0; i < MAX_COAST_POINTS; i++) {
		if (i >= coast_point_count) {
			break;
		}
		vec2 a = coast_points[i];
		vec2 b = coast_points[i + 1 < coast_point_count ? i + 1 : 0];
		if ((a.y > p.y) != (b.y > p.y)) {
			float x = a.x + (p.y - a.y) / (b.y - a.y) * (b.x - a.x);
			if (p.x < x) {
				inside = !inside;
			}
		}
	}
	return inside;
}

float hash(vec2 p) {
	return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float value_noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = hash(i);
	float b = hash(i + vec2(1.0, 0.0));
	float c = hash(i + vec2(0.0, 1.0));
	float d = hash(i + vec2(1.0, 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

vec3 blend(vec3 base, vec4 over) {
	return mix(base, over.rgb, over.a);
}

void fragment() {
	vec2 texel = UV * vec2(textureSize(id_map, 0));
	// Ein Bildschirmpixel in Bildkoordinaten.
	vec2 step_x = dFdx(texel);
	vec2 step_y = dFdy(texel);
	int id = decode_id(texel);
	vec3 color;

	if (id < 0) {
		if (is_sea(texel)) {
			color = sea_color.rgb;
		} else {
			// Unerfasstes Land: Papier mit feiner Schraffur.
			float period = 7.0;
			float stripe = fract((texel.x + texel.y) / period);
			float aa = max(fwidth((texel.x + texel.y) / period), 0.001);
			float line = smoothstep(0.0, aa, stripe) - smoothstep(0.18, 0.18 + aa, stripe);
			color = blend(empty_land_color.rgb, vec4(hatch_color.rgb, hatch_color.a * line));
		}
	} else {
		vec4 lut = vec4(0.0);
		if (id < textureSize(color_lut, 0).x) {
			lut = texelFetch(color_lut, ivec2(id, 0), 0);
		}
		color = lut.a > 0.5 ? lut.rgb : unknown_color.rgb;
		if (id == hovered_index) {
			color = blend(color, hover_glow);
		}
		if (id == selected_index) {
			color = blend(color, selection_glow);
		}

		int owner = owner_of(id);
		// Nachbarn im Abstand der Grenzbreiten.
		vec2 sx = step_x * state_border_width;
		vec2 sy = step_y * state_border_width;
		int n_right = decode_id(texel + sx);
		int n_left = decode_id(texel - sx);
		int n_down = decode_id(texel + sy);
		int n_up = decode_id(texel - sy);

		// Ortsgrenze: nur zu einer Seite, damit sie dünn bleibt.
		if (decode_id(texel + step_x) != id || decode_id(texel + step_y) != id) {
			color = blend(color, location_border_color);
		}
		// Staatsgrenze: beide Seiten zeichnen, dadurch kräftiger.
		if (owner_of(n_right) != owner || owner_of(n_left) != owner
				|| owner_of(n_down) != owner || owner_of(n_up) != owner) {
			color = blend(color, state_border_color);
		}
		// Küste und Rand zu Gebieten ohne Ort.
		vec2 cx = step_x * coast_width;
		vec2 cy = step_y * coast_width;
		if (decode_id(texel + cx) < 0 || decode_id(texel - cx) < 0
				|| decode_id(texel + cy) < 0 || decode_id(texel - cy) < 0) {
			color = blend(color, coast_color);
		}
		// Umriss des ausgewählten Ortes (innen).
		if (id == selected_index) {
			vec2 ex = step_x * selection_width;
			vec2 ey = step_y * selection_width;
			if (decode_id(texel + ex) != id || decode_id(texel - ex) != id
					|| decode_id(texel + ey) != id || decode_id(texel - ey) != id
					|| decode_id(texel + ex + ey) != id || decode_id(texel - ex - ey) != id
					|| decode_id(texel + ex - ey) != id || decode_id(texel - ex + ey) != id) {
				color = selection_color.rgb;
			}
		}
	}

	// Papierton: leichte Angleichung an das Papier und feine Körnung.
	color = mix(color, paper_color.rgb, paper_strength);
	float grain = value_noise(texel * 0.9) * 0.55 + value_noise(texel * 0.07) * 0.45;
	color *= 1.0 - grain_strength * (grain - 0.5) * 2.0;
	COLOR = vec4(color, 1.0);
}
