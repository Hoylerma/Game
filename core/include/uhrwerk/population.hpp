#pragma once

// Bevölkerungsmodell (seit M4): Formeln aus docs/technik/bevoelkerung.md, Abschnitt 4.
//
// Die Funktionen hier sind rein (sie lesen die Welt und schreiben nur ihre Ausgaben) oder
// arbeiten auf genau einem Ort. Lader und Simulation benutzen dieselben Formeln.

#include "uhrwerk/fixed.hpp"
#include "uhrwerk/world.hpp"

#include <vector>

namespace uhrwerk {

/// Kennzahlen eines Ortes für Bevölkerung und Wirtschaft (Abschnitt 4.1; seit M5 kommt der
/// Ziel-Lebensstandard aus der Wirtschaft, wirtschaft.md Abschnitt 4.4).
struct LocationFactors {
    Fixed population;          // n: Personen in allen Blöcken
    Fixed capacity;            // Tragfähigkeit (Personen)
    Fixed reserve;             // Landreserve 0..1
    Fixed condition;           // 1 − verwüstung × verwuestung_ertrag
    Fixed agrarian;            // Ertragsfaktor der Agrarbetriebe: min(1, Tragfähigkeit / n)
    std::vector<Fixed> target; // je Beruf: Ziel-Lebensstandard 0..100 (run_location_economy)
};

/// Berechnet population, capacity, reserve, condition und agrarian eines Ortes mit Daten. Die
/// Tragfähigkeit enthält seit M5 den Zuschlag des Landesausbaus (wirtschaft.md Abschnitt 4.6).
void compute_location_factors(const World& world, Index location, LocationFactors& out);

/// Ziel der Alphabetisierung eines Blocks bei Lebensstandard living_standard (Abschnitt 4.4).
Fixed literacy_target(const World& world, const Location& location, const PopBlock& pop,
                      Fixed living_standard);

/// Übernimmt die Menschen von from in into: Altersgruppen addiert, Lebensstandard und
/// Alphabetisierung nach Personen gewichtet gemittelt.
void absorb(PopBlock& into, const PopBlock& from);

/// Ähnlichster Block für die Auflösung von pop (Abschnitt 4.7) unter den Kandidaten
/// [first, first + count) von blocks: gleicher Beruf und gleiche Konfession → gleicher Beruf und
/// gleiche Kultur → gleicher Beruf → gleiche Konfession → größter Block; bei Gleichstand der
/// kleinere Index. kNone, wenn es keinen Kandidaten gibt. Kandidaten mit keep[i] == false werden
/// übergangen; ein leeres keep lässt alle zu.
Index most_similar_block(const std::vector<PopBlock>& blocks, const std::vector<bool>& keep, Index first,
                         Index count, const PopBlock& pop);

/// Neuordnung (Abschnitt 4.7): Blöcke unter rules.block_min_size gehen im ähnlichsten
/// verbleibenden Block ihres Ortes auf (der größte Block eines Ortes bleibt immer), die Zugänge
/// (world.arrivals[ort], in Eingangsreihenfolge) werden als neue Blöcke angehängt, und world.pops
/// wird neu aufgebaut (first_pop, pop_count). Danach sind alle Listen in world.arrivals leer.
/// Gibt true zurück, wenn sich etwas geändert hat.
bool reorganize(World& world);

/// Abgeleitete Konstanten der Bevölkerungsrechnung, einmal aus den Regeln berechnet.
struct PopulationRates {
    Fixed aging_children; // 1/180: Anteil der Kinder, die je Monat 15 werden
    Fixed aging_young;    // 1/180
    Fixed aging_working;  // 1/420
    Fixed literacy_month; // bildung_anpassung / 12
    Fixed fifty;          // 50
    Fixed hundred;        // 100
    Fixed twelve;         // 12
    Fixed twelve_hundred; // 1200 (Jahresrate in Prozentpunkten je Monat)
};
PopulationRates derive_rates(const Rules& rules);

/// Startwerte nach dem Laden (Abschnitt 2.6): Neuordnung (zu kleine Blöcke auflösen), Einschwingen der
/// Wirtschaft (wirtschaft.md Abschnitt 4.8), dann Lebensstandard = Ziel des Berufs im Ort und
/// Alphabetisierung = Zielwert; Momentaufnahmen und Ziel-Lebensstandard werden veröffentlicht.
void initialize_population(World& world);

} // namespace uhrwerk
