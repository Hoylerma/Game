#pragma once

// Zollgebiete und Zollunionen (seit M9): docs/technik/diplomatie.md, Abschnitt 3.
//
// Ein Zollgebiet ist eine Zollunion oder ein Staat ohne Union. Kennung eines Zollgebiets
// (customs_zone): der Index der Union, für Staaten ohne Union die Zahl der Unionen plus der Index
// des Staates. Alles hier gilt nur in Welten mit Zollunionen; ohne sie rechnet der Zoll wie in M5
// (Entscheidung 11: 1648 bleibt bitgleich).

#include "uhrwerk/economy.hpp"
#include "uhrwerk/fixed.hpp"
#include "uhrwerk/world.hpp"

#include <cstddef>
#include <vector>

namespace uhrwerk {

/// Zollgebiet eines Staates (gültiger Index vorausgesetzt).
Index customs_zone(const World& world, Index state);

/// Zahl der möglichen Zollgebiete: Unionen plus Staaten.
std::size_t customs_zone_count(const World& world);

/// Tarif eines Zollgebiets: der Vereinstarif einer Union oder der Zollsatz eines Staates ohne Union.
Fixed zone_tariff(const World& world, Index zone);

/// Anteil des Zollgebiets an der Bevölkerung des Marktes (0, wenn es dort nicht vorkommt).
Fixed zone_share(const MarketState& market, Index zone);

/// Index des Zollgebiets in market.zones, kNone, wenn es dort nicht vorkommt.
Index zone_slot(const MarketState& market, Index zone);

/// Anteile der Zollgebiete an der Bevölkerung jedes aktiven Marktes (Abschnitt 3.2, Orte mit Daten);
/// setzt zones und zone_shares der Märkte.
void update_customs_zones(World& world, const EconomyTopology& topology);

/// Erwarteter Tarif auf Waren aus Markt from, die in Markt to gekauft werden (Abschnitt 3.3):
/// Σ_z s_to(z) × (1 − s_from(z)) × tarif(z). Mit from = nullptr der Tarif auf Einfuhr vom Weltmarkt:
/// Σ_z s_to(z) × tarif(z). tariffs: zone_tariff je Zollgebiet (Index wie customs_zone).
Fixed customs_tariff(const MarketState* from, const MarketState& to, const std::vector<Fixed>& tariffs);

/// Vereinskasse (Abschnitt 3.4): sammelt den zuletzt festgestellten Zoll der Orte jedes Mitglieds und
/// verteilt ihn nach der Bevölkerung auf die Mitglieder (State::customs_income, 0 für Staaten ohne
/// Union). Setzt World::customs_revenue.
void distribute_customs(World& world);

/// Ergebnis eines Antrags auf Wechsel der Zollunion (Befehl Zollunion, Abschnitt 3.5).
enum class CustomsDecision {
    Invalid,   // ungültig oder ohne Wirkung (gleiche Union ohne offenen Antrag, Leitmacht will austreten)
    Withdrawn, // offener Antrag zurückgezogen (Antrag auf die bisherige Union)
    Rejected,  // Beitritt von der Leitmacht abgelehnt
    Accepted,  // Wechsel zum nächsten 1. Januar vorgemerkt
};

/// Befehl Zollunion: state will zum nächsten 1. Januar in die Union target wechseln (kNone = Austritt).
/// Beitritte entscheidet die Leitmacht (bis zur KI nach Regel: abgelehnt, wer in excluded steht);
/// Austritte brauchen keine Zustimmung, die Leitmacht selbst kann nicht austreten. Ein neuer Antrag
/// ersetzt einen offenen. Schreibt die Chronik.
CustomsDecision request_customs_change(World& world, Index state, Index target);

/// Am 1. Januar: die vorgemerkten Wechsel werden wirksam (in Antragsreihenfolge).
void apply_customs_changes(World& world);

} // namespace uhrwerk
