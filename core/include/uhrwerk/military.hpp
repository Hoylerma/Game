#pragma once

// Militär (seit M10): docs/technik/militaer.md.
//
// Schritt a1: Nachbarschaft der Orte, Heerwesen je Staat, Garnisonen und das Friedensheer als Heere
// in ihren Garnisonen. Alles gilt nur in Welten mit Militärdaten (MilitaryRules::active, ab dem
// 'seit' in common/militaer.chron); ohne sie bleibt der Zustand leer (1648 bitgleich).

#include "uhrwerk/fixed.hpp"
#include "uhrwerk/world.hpp"

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
/// noch keine Heere gibt und das Militär gilt.
void ensure_military_state(World& world);

/// Am Monatsersten: Die Heere in ihren Garnisonen folgen der Friedensstärke ihres Staates
/// (Bevölkerung und heer ändern sich); Geschütze nach dem Heerwesen.
void update_garrisons(World& world);

} // namespace uhrwerk
