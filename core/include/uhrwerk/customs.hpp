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
    Invalid,   // ungültig oder ohne Wirkung (gleiche Union ohne offenen Antrag, aufgelöste Union)
    Withdrawn, // offener Antrag zurückgezogen (Antrag auf die bisherige Union)
    Rejected,  // Beitritt von der Leitmacht abgelehnt
    Accepted,  // Wechsel zum nächsten 1. Januar vorgemerkt
};

/// Wie request_customs_change entschieden würde, ohne die Welt zu ändern (seit Schritt a4: Knöpfe
/// der Oberfläche zeigen vorher, ob ein Antrag möglich ist).
CustomsDecision evaluate_customs_change(const World& world, Index state, Index target);

/// Datum, zu dem ein heute angenommener Wechsel wirksam wird: der nächste 1. Januar (heute, wenn heute
/// der 1. Januar ist, denn die Befehle laufen vor dem Jahreswechsel).
Date customs_effective_date(const Date& today);

/// Datum, zu dem ein Beitritt wirksam wird, den die Simulation selbst nach dem Jahresabschluss des
/// Tages vormerkt (Zolleinigung des Bundes, Ausgang einer Krise): immer der 1. Januar des Folgejahres,
/// auch am 1. Januar, denn dessen Jahreswechsel ist dann schon gelaufen.
Date customs_forced_date(const Date& today);

/// Ob der offene Antrag des Staates verbindlich ist (seit Schritt b4): ein Beitritt, den eine
/// angenommene Zolleinigung oder eine verlorene Krise „Beitritt zur Zollunion“ vorgemerkt hat
/// (diplomatie.md 4.4 und 4.5). Er lässt sich bis zum Wirksamwerden weder zurückziehen noch ändern;
/// erkannt wird er an den Ergebnissen (Abstimmungen, Spiele), nicht an einem eigenen Zustand.
bool customs_request_binding(const World& world, Index state);

/// Befehl Zollunion: state will zum nächsten 1. Januar in die Union target wechseln (kNone = Austritt).
/// Beitritte entscheidet die Leitmacht (bis zur KI nach Regel: abgelehnt, wer in excluded steht);
/// einer aufgelösten Union kann nur ihre Leitmacht beitreten. Austritte brauchen keine Zustimmung;
/// tritt die Leitmacht aus, löst sich ihre Union zum selben Tag auf. Ein neuer Antrag ersetzt einen
/// offenen, außer der offene ist verbindlich (customs_request_binding). Schreibt die Chronik.
CustomsDecision request_customs_change(World& world, Index state, Index target);

/// Am 1. Januar: die vorgemerkten Wechsel werden wirksam (in Antragsreihenfolge); danach lösen sich
/// Unionen ohne ihre Leitmacht auf.
void apply_customs_changes(World& world);

} // namespace uhrwerk
