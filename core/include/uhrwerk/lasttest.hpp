#pragma once

// Lasttest-Welt (seit M4, docs/technik/bevoelkerung.md, Abschnitt 7).
//
// Aus einer geladenen Welt (Karte, Staaten, Gelände, Berufe, Konfessionen, Kulturen) entsteht eine
// Welt, in der jeder Ort Daten hat und die Zahl der Bevölkerungsblöcke einem Ziel nahekommt.
// Orte mit echten Daten behalten Besitzer, Gelände und Lage; Bevölkerung, Verwüstung und Blöcke
// werden für alle Orte neu erzeugt. Der Aufbau ist deterministisch (eigener Zufallsstrom
// RngStream::Lasttest): gleiche Welt, gleiche Optionen ⇒ gleiche Lasttest-Welt.

#include "uhrwerk/world.hpp"

#include <cstdint>

namespace uhrwerk {

struct LoadTestOptions {
    std::int64_t blocks = 500'000; // angestrebte Zahl der Bevölkerungsblöcke
    std::uint64_t seed = 1648;
};

/// Baut die Lasttest-Welt. Voraussetzung: mindestens ein Staat, ein Beruf, eine Konfession und
/// eine Kultur.
///
/// - Besitzer: echte Orte behalten ihren Besitzer; Orte ohne Daten bekommen den Besitzer des
///   nächsten Ortes mit Daten (bei Gleichstand der kleinere Index), ohne Orte mit Daten Staat
///   Index mod Staatenzahl.
/// - Bevölkerung: Tragfähigkeit × gleichverteilt 0,3–0,9, mindestens 20 Personen je Block;
///   Verwüstung gleichverteilt 0–0,6.
/// - Blöcke: alle Berufe × alle Konfessionen × k zufällig gewählte Kulturen; k so, dass die
///   Summe dem Ziel folgt. Alle Blöcke eines Ortes sind gleich groß (keiner wird sofort
///   aufgelöst). Startwerte wie beim Laden (initialize_population).
World make_load_test_world(const World& base, const LoadTestOptions& options);

} // namespace uhrwerk
