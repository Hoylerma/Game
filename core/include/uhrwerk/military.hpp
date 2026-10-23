#pragma once

// Militär (seit M10): docs/technik/militaer.md.
//
// Schritt a1: Nachbarschaft der Orte, Heerwesen je Staat, Garnisonen und das Friedensheer als Heere
// in ihren Garnisonen. Schritt a2: Mobilmachung, Wege über Marsch und Eisenbahn, Bewegung, Versorgung
// und Zermürbung. Alles gilt nur in Welten mit Militärdaten (MilitaryRules::active, ab dem 'seit' in
// common/militaer.chron); ohne sie bleibt der Zustand leer (1648 bitgleich).

#include "uhrwerk/fixed.hpp"
#include "uhrwerk/world.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace uhrwerk {

/// Gibt es Heere und Krieg (Militärdaten zum Startdatum)?
bool military_active(const World& world);

/// Friedensstärke eines Staates: Soldaten je 1.000 Einwohner (heer) × Bevölkerung / 1.000, wie die
/// Heereskosten im Haushalt (wirtschaft.md, Abschnitt 4.6). 0 bei ungültigem Index.
Fixed peace_strength(const World& world, Index state);

/// Soldaten aller Heere eines Staates.
Fixed state_soldiers(const World& world, Index state);

/// Heere eines Staates (Zahl).
std::int64_t state_army_count(const World& world, Index state);

/// Nachbarschaft von a nach b, nullptr ohne gemeinsame Grenze oder Überfahrt.
const Neighbor* find_neighbor(const World& world, Index a, Index b);

/// Liegt zwischen a und b ein Flussübergang (gekreuzter Fluss bis Rang MilitaryRules::river_rank)?
bool river_crossing(const World& world, const Neighbor& neighbor);

/// Beim Start (wie ensure_diplomacy_state): legt je Garnison ein Heer mit der Friedensstärke an, wenn es
/// noch keine Heere gibt und das Militär gilt; Mobilmachung je Staat und Belegung je Bahnstrecke.
void ensure_military_state(World& world);

/// Am Monatsersten: Die Heere in ihren Garnisonen folgen der Friedensstärke ihres Staates
/// (Bevölkerung und heer ändern sich); Geschütze nach dem Heerwesen.
void update_garrisons(World& world);

// ─── Bewegung (seit Schritt a2, Abschnitt 3.4) ──────────────────────────────

/// Heer mit dieser Nummer, nullptr ohne.
Army* find_army(World& world, std::int64_t id);
const Army* find_army(const World& world, std::int64_t id);

/// Darf ein Heer von state den Ort betreten? Eigene und verbündete Orte; Orte ohne Daten und die
/// anderer Staaten sind gesperrt (gegnerische und besetzte Orte im Krieg kommen mit Schritt a3).
bool accessible(const World& world, Index state, Index location);

/// Marschtage zum Nachbarn: Abstand ÷ Marschleistung × Geländefaktor des Zielorts, ein Flussübergang
/// kostet river_days mehr; eine Überfahrt dauert ihre Tage aus den Daten.
Fixed march_days(const World& world, const Neighbor& neighbor);

/// Tage zum Einladen eines Heeres: Soldaten ÷ Kapazität je Strecke und Tag, aufgerundet, mindestens 1.
std::int32_t loading_days(const World& world, Fixed soldiers);

/// Ist der Ort ein Bahnhof (Endpunkt eines eröffneten Abschnitts)?
bool is_station(const World& world, Index location);

/// Weg eines Heeres: die Schritte und die geplanten Tage (ohne Warten auf belegte Strecken).
struct Route {
    std::vector<RouteStep> steps;
    Fixed days;
};

/// Schnellster Weg des Heeres zum Ziel (Dijkstra über Marsch- und Bahntage, Einladen und Ausladen
/// eingerechnet); nullopt ohne Weg, bei gesperrtem Ziel oder wenn das Heer schon dort steht.
std::optional<Route> find_route(const World& world, const Army& army, Index target);

/// Befehl Marsch: Feldarmee army von state zum Ziel; target kNone oder der eigene Ort hält an.
/// false, wenn der Befehl ungültig ist (kein Heer des Staates, Garnison, auf der Bahn, kein Weg).
bool order_march(World& world, Index state, std::int64_t army, Index target);

// ─── Mobilmachung (seit Schritt a2, Abschnitt 3.3) ──────────────────────────

MobilizationPhase mobilization_phase(const World& world, Index state);

/// Befehl Mobilmachung (mobilize) oder Abrüstung. crisis: durch eine Krise auf der Stufe
/// Mobilmachung; solche Staaten rüsten mit dem Ende der Krise wieder ab. false, wenn ohne Wirkung.
bool order_mobilization(World& world, Index state, bool mobilize, bool crisis = false);

/// Ende einer Krise: Ein Staat, den sie mobil gemacht hat, rüstet ab.
void end_crisis_mobilization(World& world, Index state);

/// Faktor auf die Heereskosten: Soldaten ÷ Friedensstärke, solange ein Staat nicht im Frieden ist;
/// sonst 1 (die Garnisonen kosten wie bisher).
Fixed army_cost_factor(const World& world, Index state);

// ─── Versorgung (seit Schritt a2, Abschnitt 3.5) ────────────────────────────

/// Versorgt ist ein Heer in eigenem oder verbündetem Gebiet, sonst höchstens supply_days Tagesmärsche
/// von einem Bahnhof seiner Seite.
bool supplied(const World& world, const Army& army);

/// Täglich nach den diplomatischen Spielen: Mobilmachung und Abrüstung, Bewegung, Versorgung und
/// Zermürbung, Belegung der Bahnstrecken.
void run_military(World& world);

} // namespace uhrwerk
