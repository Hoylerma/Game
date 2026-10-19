#pragma once

// Politik (seit M8): Wahlen nach docs/technik/politik.md, Abschnitt 7.
//
// Eine Wahl rechnet an ihrem Wahltag in einem Schritt aus den Bevölkerungsblöcken der Wahlkreise:
// Wahlberechtigte nach Wahlrecht, Beteiligung, Neigung zu den Strömungen (Interessengruppen, Bildung,
// Not, Stadt, Region), Gewichte (Dreiklassen, Wahlmänner), Rauschen, Mehrheit oder Stichwahl, dann
// Abgeordnete mit Position und Fraktion. Alles in Festkomma und in fester Reihenfolge mit einem
// eigenen Zufallsgenerator (Strom Wahlen), also auf jeder Plattform gleich. Die Wahl ändert die Welt
// nicht; die Simulation legt das Ergebnis in World::elections ab.

#include "uhrwerk/fixed.hpp"
#include "uhrwerk/world.hpp"

#include <cstdint>
#include <vector>

namespace uhrwerk {

class ThreadPool;

/// Mitgliedschaft eines Blocks in jeder Interessengruppe (0..1, Abschnitt 3.3), parallel zu
/// World::interest_groups.
std::vector<Fixed> interest_memberships(const World& world, const PopBlock& pop);

/// Verschiebung der Neigung an einem Ort: Summe der passenden Regionen (je Strömung).
std::vector<Fixed> region_shift(const World& world, Index location);

/// Neigung eines Blocks zu den Strömungen ohne Rauschen (Abschnitt 7.3), Summe 1; region aus region_shift.
std::vector<Fixed> current_leaning(const World& world, const PopBlock& pop, const std::vector<Fixed>& region);

/// Fraktion zu einer Position auf der Links-rechts-Achse; kNone, wenn keine passt.
Index faction_of(const Parliament& parliament, Fixed position);

/// Rechnet die Wahl eines Parlaments zum heutigen Datum der Welt (Abschnitt 7). seed ist der Seed der
/// Partie; je Wahlkreis entsteht daraus ein eigener Zufallsgenerator (Strom Wahlen), deshalb hängt das
/// Ergebnis nicht davon ab, ob und mit wie vielen Threads (pool) die Wahlkreise rechnen.
ElectionResult run_election(const World& world, Index parliament, std::uint64_t seed,
                            ThreadPool* pool = nullptr);

/// Sitze je Strömung (parallel zu World::currents).
std::vector<std::int32_t> seats_by_current(const World& world, const ElectionResult& election);

/// Sitze je Fraktion des Parlaments; der letzte Eintrag zählt die Fraktionslosen.
std::vector<std::int32_t> seats_by_faction(const World& world, const ElectionResult& election);

} // namespace uhrwerk
