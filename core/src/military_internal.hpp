#pragma once

// Gemeinsame Hilfen von military.cpp und war.cpp (seit M10, Schritt a3); nicht Teil der Schnittstelle.

#include "uhrwerk/fixed.hpp"
#include "uhrwerk/rng.hpp"
#include "uhrwerk/world.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace uhrwerk::military_internal {

template <typename T> bool in_range(const std::vector<T>& items, Index index) {
    return index >= 0 && static_cast<std::size_t>(index) < items.size();
}

inline std::size_t at(Index index) {
    return static_cast<std::size_t>(index);
}

void chronicle(World& world, Index state, std::string text, Index location = kNone);

/// Summe der Anteile aller Garnisonen eines Staates (je Staat).
std::vector<Fixed> garrison_shares(const World& world);

/// Soldaten einer Garnison: ihr Anteil an der Friedensstärke ihres Staates.
Fixed garrison_soldiers(const Garrison& g, Fixed peace, Fixed shares);

/// Soldaten setzen; die Geschütze folgen nach dem Heerwesen.
void set_soldiers(const World& world, Army& army, Fixed soldiers);

/// Weg beenden: Ruhe am Ort.
void stop(Army& army);

/// Ganzzahl mit Tausenderpunkten (Chronik).
std::string grouped(std::int64_t value);

/// Steht das Heer im Ort (nicht auf der Bahn und nicht auf dem Rückzug)? Nur solche Heere kämpfen,
/// halten einen Ort oder halten einen Gegner auf.
bool engaged(const Army& army);

/// Steht im Ort ein Heer, das mit state im Krieg ist?
bool hostile_army_at(const World& world, Index state, Index location);

/// Fügt dem Staat Gefallene oder Abgänge hinzu (mindern am Monatsersten die Bevölkerung).
void add_war_dead(World& world, Index state, Fixed soldiers);

/// Schlachten, Besetzung und Belagerung, Verwundete eines Tages (war.cpp).
void run_battles(World& world, Pcg32& rng);
void run_occupation(World& world);
void return_convalescents(World& world);

/// Am Monatsersten: Gefallene und Abgänge mindern die Jahrgänge im wehrfähigen Alter (war.cpp).
void apply_war_dead(World& world);

} // namespace uhrwerk::military_internal
