#pragma once

// Bauen (seit M15b1, docs/technik/bauen_und_anleitung.md Abschnitt 3): Landesteile, Gebäude mit Stufen,
// Bauaufträge, Wirkung auf Ertrag, Arbeitsplätze, Landesausbau und Alphabetisierung.

#include "uhrwerk/world.hpp"

#include <cstdint>
#include <string>

namespace uhrwerk {

/// Landesteile aus Verwaltung (oberste Einheit), Kreisen und Orten; setzt World::regions und
/// World::location_region (Lader, nach den Orten).
void build_regions(World& world);

/// Stufen anlegen (alle 0), falls noch nicht da, und die abgeleiteten Wirkungen neu berechnen. Ohne
/// building_rules.active bleiben alle Vektoren leer.
void ensure_building_state(World& world);

/// Mehrertrag je Ort und Betrieb und Ziel der Alphabetisierung je Ort aus den Stufen.
void rebuild_building_effects(World& world);

std::int32_t building_level(const World& world, Index region, Index type);

/// Einwohner eines Landesteils (Blöcke und Zugänge).
Fixed region_population(const World& world, Index region);

/// Staat, dem der erste Ort des Landesteils gehört (er trägt den Unterhalt), kNone ohne.
Index region_owner(const World& world, Index region);

/// Baukosten der nächsten Stufe in Talern (nach den Einwohnern jetzt).
Fixed build_cost(const World& world, Index region, Index type);

/// Gleichzeitige Aufträge, die das Bauamt eines Staates fasst, und seine laufenden.
std::int32_t order_capacity(const World& world, Index state);
std::int32_t orders_of(const World& world, Index state);

/// Warum der Auftrag nicht geht (Text für die Oberfläche), leer, wenn er geht.
std::string build_problem(const World& world, Index state, Index region, Index type);

/// Befehle Build und CancelBuild; false, wenn ungültig (dann ohne Wirkung).
bool order_building(World& world, Index state, Index region, Index type);
bool cancel_building(World& world, Index state, Index region, Index type);

/// Monat eines Staates an seinem Rechentag: Aufträge voranbringen und fertigstellen, Unterhalt.
/// Rückgabe: Ausgaben des Monats (Raten der Aufträge plus Unterhalt) für den Posten „Bauten“.
Fixed run_buildings(World& world, Index state);

/// Anwerbung am Monatsersten: Gebäude mit Arbeitsplätzen werben aus dem Quellberuf an (Zugänge).
void recruit_workers(World& world);

} // namespace uhrwerk
