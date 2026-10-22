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
#include <optional>
#include <vector>

namespace uhrwerk {

/// Nachbarmarkt mit Frachtaufschlag (Anteil am Preis). Ab rail_from (seit M7) sind beide Märkte über
/// eröffnete Eisenbahnabschnitte verbunden, und es gilt rail_freight; ohne Verbindung bleibt rail_from leer.
struct MarketLink {
    Index market = kNone;
    Fixed freight;
    Fixed rail_freight;
    std::optional<Date> rail_from;

    /// Fracht am Tag date.
    Fixed freight_on(const Date& date) const {
        return rail_from && date >= *rail_from ? rail_freight : freight;
    }
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

/// Abstand in km wie bei der Wanderung: Grad, Länge mit cos 51° gestaucht, 111,2 km je Grad
/// (deterministisch; auch für die Meldereihenfolge der Wahlen, seit M8).
Fixed distance_km(Fixed lon_a, Fixed lat_a, Fixed lon_b, Fixed lat_b);

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
    std::vector<Fixed> no_imports;   // je Gut: 0 (Zollgebiet ohne Einfuhranteil im Markt, seit M9)
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

/// Staatsnachfrage eines Monats aus dem Haushalt des Vormonats (Abschnitt 4.6): Hof wie „oben“,
/// Verwaltung und Bauten wie „mittel“, Heer wie „unten“, jeweils im Markt der Hauptstadt.
std::vector<StateSpending> state_spending(const World& world);

/// Zuschlag des Landesausbaus auf die Tragfähigkeit eines Ortes (0 … ausbau_max, Abschnitt 4.6).
Fixed development_bonus(const World& world, Index location);

/// Haushalt eines Staates an seinem Rechentag (Abschnitt 4.6): Einnahmen aus den zuletzt festgestellten
/// Steuern seiner Orte (locations, aufsteigend), Ausgaben, Kredit, Tilgung, Bauten (Landesausbau der
/// Orte), gleitende Jahreseinnahmen. Gibt true zurück, wenn der Staat dabei bankrott geht.
bool run_state_budget(World& world, Index state, const std::vector<Index>& locations);

/// Ausgaben jedes Staates aus seiner Bevölkerung schätzen (Hof, Verwaltung, Heer, Zinsen) und die
/// Jahreseinnahmen aus den Steuern seiner Orte setzen, wo sie fehlen – für den Start, damit der Staat
/// schon im ersten Monat einkauft (Einschwingen, Abschnitt 4.8).
void estimate_budgets(World& world);

} // namespace uhrwerk
