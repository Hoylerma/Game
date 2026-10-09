// Tests für die Hilfsfunktionen von World.

#include "test_hilfen.hpp"
#include "uhrwerk/world.hpp"

#include <doctest/doctest.h>
#include <vector>

using uhrwerk::Fixed;
using uhrwerk::Index;
using uhrwerk::kNone;
using uhrwerk::World;

namespace {

uhrwerk::PopBlock pop(Index location, Index profession, Index confession, std::int64_t size) {
    uhrwerk::PopBlock p;
    p.location = location;
    p.profession = profession;
    p.confession = confession;
    p.size = Fixed::from_int(size);
    return p;
}

/// Kleine Welt: Staaten A, B, C (C ohne Orte); Orte x (A), y (B), z (A), leer (B, ohne Bevölkerung).
World make_world() {
    World w;
    for (const char* id : {"luth", "ref", "kath"}) {
        uhrwerk::Confession c;
        c.id = id;
        w.confessions.push_back(c);
    }
    for (const char* id : {"acker", "wald"}) {
        uhrwerk::TerrainType t;
        t.id = id;
        w.terrains.push_back(t);
    }
    for (const char* id : {"bauern", "adel"}) {
        uhrwerk::Profession p;
        p.id = id;
        w.professions.push_back(p);
    }
    for (const char* tag : {"AAA", "BBB", "CCC"}) {
        uhrwerk::State s;
        s.tag = tag;
        w.states.push_back(s);
    }
    const auto add_location = [&w](const char* id, Index owner) {
        uhrwerk::Location l;
        l.id = id;
        l.owner = owner;
        w.locations.push_back(l);
    };
    add_location("x", 0);
    add_location("y", 1);
    add_location("z", 0);
    add_location("leer", 1);

    // x: luth 100 + 50 = 150, ref 120 → Mehrheit luth
    w.pops.push_back(pop(0, 0, 0, 100));
    w.pops.push_back(pop(0, 0, 1, 120));
    w.pops.push_back(pop(0, 1, 0, 50));
    // y: ref 70, kath 70 → Gleichstand, kleinerer Index (ref)
    w.pops.push_back(pop(1, 0, 2, 70));
    w.pops.push_back(pop(1, 1, 1, 70));
    // z: kath 5
    w.pops.push_back(pop(2, 0, 2, 5));

    w.locations[0].first_pop = 0;
    w.locations[0].pop_count = 3;
    w.locations[1].first_pop = 3;
    w.locations[1].pop_count = 2;
    w.locations[2].first_pop = 5;
    w.locations[2].pop_count = 1;
    w.locations[3].first_pop = 6;
    w.locations[3].pop_count = 0;
    return w;
}

} // namespace

TEST_CASE("World: Suchfunktionen") {
    const World w = make_world();
    CHECK(w.find_state("AAA") == 0);
    CHECK(w.find_state("CCC") == 2);
    CHECK(w.find_state("aaa") == kNone);
    CHECK(w.find_state("") == kNone);
    CHECK(w.find_location("x") == 0);
    CHECK(w.find_location("leer") == 3);
    CHECK(w.find_location("w") == kNone);
    CHECK(w.find_terrain("wald") == 1);
    CHECK(w.find_terrain("sumpf") == kNone);
    CHECK(w.find_confession("kath") == 2);
    CHECK(w.find_confession("") == kNone);
    CHECK(w.find_profession("adel") == 1);
    CHECK(w.find_profession("klerus") == kNone);
}

TEST_CASE("World: Bevölkerung von Orten und Staaten") {
    const World w = make_world();
    CHECK(w.location_population(0) == Fixed::from_int(270));
    CHECK(w.location_population(1) == Fixed::from_int(140));
    CHECK(w.location_population(2) == Fixed::from_int(5));
    CHECK(w.location_population(3) == Fixed::zero());
    CHECK(w.location_population(kNone) == Fixed::zero());
    CHECK(w.location_population(99) == Fixed::zero());

    CHECK(w.state_population(0) == Fixed::from_int(275));
    CHECK(w.state_population(1) == Fixed::from_int(140));
    CHECK(w.state_population(2) == Fixed::zero());
    CHECK(w.state_population(kNone) == Fixed::zero());
    CHECK(w.state_population(3) == Fixed::zero());
}

TEST_CASE("World: Orte eines Staates aufsteigend") {
    const World w = make_world();
    CHECK(w.state_locations(0) == std::vector<Index>{0, 2});
    CHECK(w.state_locations(1) == std::vector<Index>{1, 3});
    CHECK(w.state_locations(2).empty());
    CHECK(w.state_locations(-1).empty());
    CHECK(w.state_locations(17).empty());
}

TEST_CASE("World: Mehrheitskonfession") {
    World w = make_world();
    CHECK(w.location_majority_confession(0) == 0);
    CHECK(w.location_majority_confession(1) == 1); // Gleichstand → kleinerer Index
    CHECK(w.location_majority_confession(2) == 2);
    CHECK(w.location_majority_confession(3) == kNone); // keine Bevölkerung
    CHECK(w.location_majority_confession(kNone) == kNone);
    CHECK(w.location_majority_confession(4) == kNone);

    // Reformierte überholen knapp.
    w.pops[1].size = Fixed::from_int(150) + Fixed::from_raw(1);
    CHECK(w.location_majority_confession(0) == 1);
    // Genau gleich → wieder lutherisch (kleinerer Index).
    w.pops[1].size = Fixed::from_int(150);
    CHECK(w.location_majority_confession(0) == 0);
}

TEST_CASE("World: Hilfsfunktionen auf der Fixture") {
    const World w = testhilfen::load_fixture();
    const Index sax = w.find_state("SAX");
    REQUIRE(sax == 1);
    CHECK(w.state_locations(sax) == std::vector<Index>{2, 3});
    CHECK(w.state_population(sax) == w.location_population(w.find_location("dresden")) +
                                         w.location_population(w.find_location("leipzig")));
    CHECK(w.location_majority_confession(w.find_location("dessau")) == w.find_confession("reformiert"));
    CHECK(w.location_majority_confession(w.find_location("berlin_coelln")) ==
          w.find_confession("lutherisch"));
    // Bevölkerung entspricht der Datei bis auf Rundung der Blockgrößen.
    const Fixed berlin = w.location_population(w.find_location("berlin_coelln"));
    CHECK(berlin > Fixed::from_int(11999));
    CHECK(berlin < Fixed::from_int(12001));
}
