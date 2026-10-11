// Tests für die Lasttest-Welt (seit M4, docs/technik/bevoelkerung.md, Abschnitt 7).

#include "test_hilfen.hpp"
#include "uhrwerk/lasttest.hpp"
#include "uhrwerk/simulation.hpp"

#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <set>
#include <tuple>

using uhrwerk::Fixed;
using uhrwerk::Index;
using uhrwerk::LoadTestOptions;
using uhrwerk::World;

TEST_CASE("Lasttest-Welt: jeder Ort hat Daten, Blockzahl nahe am Ziel") {
    const World base = testhilfen::load_fixture();
    const World w = uhrwerk::make_load_test_world(base, LoadTestOptions{200, 7});
    REQUIRE(w.locations.size() == base.locations.size());
    CHECK(w.locations_with_data() == w.locations.size()); // auch zerbst (Ort ohne Daten)

    // Je gewählter Kultur 4 Berufe × 3 Konfessionen = 12 Blöcke; das Ziel wird bis auf einen
    // Kulturschritt je Ort erreicht.
    CHECK(w.pops.size() >= 200 - 12);
    CHECK(w.pops.size() <= 200 + 12);

    Index next = 0;
    for (std::size_t i = 0; i < w.locations.size(); ++i) {
        const uhrwerk::Location& loc = w.locations[i];
        INFO("Ort " << loc.id);
        CHECK(loc.owner >= 0);
        CHECK(static_cast<std::size_t>(loc.owner) < w.states.size());
        CHECK(loc.first_pop == next);
        next += loc.pop_count;
        CHECK(loc.devastation >= Fixed::zero());
        CHECK(loc.devastation <= Fixed::from_ratio(6, 10));
        // Schlüssel eindeutig, alle Blöcke des Ortes gleich groß.
        std::set<std::tuple<Index, Index, Index>> keys;
        for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
            const uhrwerk::PopBlock& pop = w.pops[static_cast<std::size_t>(p)];
            CHECK(pop.location == static_cast<Index>(i));
            CHECK(keys.insert({pop.profession, pop.confession, pop.culture}).second);
            CHECK(pop.size() >= Fixed::from_int(19)); // mindestens 20 Personen je Block (abgerundet)
            CHECK(pop.living_standard > Fixed::zero());
        }
        CHECK(loc.pop_count % 12 == 0);
    }
    CHECK(next == static_cast<Index>(w.pops.size()));
    // Echte Orte behalten ihren Besitzer, zerbst bekommt den Besitzer des nächsten Ortes (dessau).
    for (std::size_t i = 0; i < 5; ++i) {
        CHECK(w.locations[i].owner == base.locations[i].owner);
    }
    CHECK(w.locations[5].owner == base.locations[4].owner);
}

TEST_CASE("Lasttest-Welt: deterministisch je Seed") {
    const World base = testhilfen::load_fixture();
    const auto checksum = [&](std::uint64_t seed) {
        return uhrwerk::Simulation(uhrwerk::make_load_test_world(base, LoadTestOptions{300, seed}),
                                   uhrwerk::SimConfig{1648, 1})
            .checksum();
    };
    CHECK(checksum(1) == checksum(1));
    CHECK(checksum(1) != checksum(2));

    // Die Welt läuft: ein Jahr mit 2 und mit 1 Thread, gleiche Prüfsumme.
    const World w = uhrwerk::make_load_test_world(base, LoadTestOptions{300, 3});
    uhrwerk::Simulation a(w, uhrwerk::SimConfig{1648, 2});
    uhrwerk::Simulation b(w, uhrwerk::SimConfig{1648, 1});
    a.step_days(365);
    b.step_days(365);
    CHECK(a.checksum() == b.checksum());
}
