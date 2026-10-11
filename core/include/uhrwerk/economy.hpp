#pragma once

// Wirtschaft (seit M5): Formeln aus docs/technik/wirtschaft.md, Abschnitt 4.
//
// Märkte rechnen am Monatsersten nacheinander (update_markets); jeder Ort rechnet an seinem Rechentag
// (run_location_economy) und liest dabei nur den veröffentlichten Zustand seines Marktes und den
// Steuersatz seines Besitzers. Damit kann die Ortsrechnung parallel laufen (Abschnitt 5).

#include "uhrwerk/fixed.hpp"
#include "uhrwerk/population.hpp"
#include "uhrwerk/rng.hpp"
#include "uhrwerk/world.hpp"

#include <array>
#include <vector>

namespace uhrwerk {

/// Nachbarmarkt mit Frachtaufschlag (Anteil am Preis).
struct MarketLink {
    Index market = kNone;
    Fixed freight;
};

/// Abgeleitete Wirtschaftsdaten aus dem unveränderlichen Inhalt (kein Zustand, nicht in der Prüfsumme).
struct EconomyTopology {
    std::vector<std::vector<Index>> market_locations;       // Orte mit Daten je Markt, aufsteigend
    std::vector<std::vector<MarketLink>> neighbors;         // je Markt, nach Abstand (nur aktive Märkte)
    std::vector<bool> port;                                 // Markt mit Hafenort (mit Daten)
    std::vector<std::vector<Index>> business_by_profession; // Betriebe je Beruf, aufsteigend
    std::array<Fixed, 3> base_basket_cost{};                // Warenkorb je Schicht zum Basispreis (Jahr)

    bool active(Index market) const;
};

EconomyTopology build_economy_topology(const World& world);

/// Darf der Betrieb im Ort stehen (Standort, Vorkommen)? Der Beruf wird nicht geprüft.
bool business_allowed(const World& world, const Location& location, const BusinessType& business);

/// Legt fehlende Zustandsvektoren der Wirtschaft an: Preise = Basispreis, Quoten 1, Betriebsanteile
/// aus den Gewichten (Abschnitt 4.8). Vorhandener, passender Zustand bleibt unverändert.
void ensure_economy_state(World& world, const EconomyTopology& topology);

/// Staatsnachfrage eines Monats in einem Markt: Betrag (Reichstaler) und Warenkorb der Schicht.
struct StateSpending {
    Index market = kNone;
    Stratum stratum = Stratum::Oben;
    Fixed amount;
};

/// Märkte am Monatsersten (Abschnitt 4.1): Summen aus den Orten, Handel mit Nachbarn und Weltmarkt,
/// Preise, Quoten, Handelsspanne. spending: Staatsausgaben, die als Nachfrage in Märkte gehen.
void update_markets(World& world, const EconomyTopology& topology,
                    const std::vector<StateSpending>& spending);

/// Erntefaktor je Markt für das neue Jahr (Abschnitt 4.7), aus dem Strom Wirtschaft.
void draw_harvest(World& world, const EconomyTopology& topology, Pcg32& rng);

/// Arbeitsspeicher einer Ortsrechnung (wird wiederverwendet).
struct EconomyScratch {
    std::vector<Fixed> people;       // je Beruf: Köpfe im Ort
    std::vector<Fixed> income;       // je Beruf: Einkommen des Monats (ganzer Beruf)
    std::vector<Fixed> agrar_income; // je Beruf: davon aus Agrarbetrieben
    std::vector<Fixed> profit;       // je Betrieb: Gewinn je Kopf und Monat
    std::vector<Fixed> consumer;     // je Gut: Verbraucherpreis
    std::vector<Fixed> bought;       // je Gut: gekaufte Menge des Monats
};

/// Wirtschaft eines Ortes an seinem Rechentag (Abschnitte 4.2–4.5). Schreibt nur Daten dieses Ortes:
/// Erzeugung und Nachfrage, Einkommen, Bedarfsdeckung, Steuern, Betriebsanteile; dazu factors.target
/// (Ziel-Lebensstandard je Beruf). factors muss vorher mit compute_location_factors gefüllt sein.
void run_location_economy(World& world, const EconomyTopology& topology, Index location,
                          LocationFactors& factors, EconomyScratch& scratch);

/// Deckung der Bedürfnisse mit hunger = ja (kleinste) eines Berufs im Ort, 0..1 (1 ohne solche).
Fixed food_coverage(const World& world, Index location, Index profession);

/// Einschwingen beim Start (Abschnitt 4.8): months Monate nur Wirtschaft für alle Orte mit Daten.
void settle_economy(World& world, const EconomyTopology& topology, int months);

/// Staatsnachfrage eines Monats aus der Hofhaltung (bis Schritt 3 von M5 der einzige Posten).
std::vector<StateSpending> state_spending(const World& world);

} // namespace uhrwerk
