// Tests für das Bevölkerungsmodul (seit M4, docs/technik/bevoelkerung.md, Abschnitt 4).

#include "test_hilfen.hpp"
#include "uhrwerk/population.hpp"
#include "uhrwerk/world.hpp"

#include <doctest/doctest.h>
#include <vector>

using uhrwerk::Fixed;
using uhrwerk::Index;
using uhrwerk::kNone;
using uhrwerk::PopBlock;
using uhrwerk::World;

namespace {

Fixed fx(const char* text) {
    return *Fixed::parse(text);
}

PopBlock block(Index location, Index profession, Index confession, Index culture, std::int64_t young,
               const char* living_standard = "50", const char* literacy = "0.2") {
    PopBlock p;
    p.location = location;
    p.profession = profession;
    p.confession = confession;
    p.culture = culture;
    p.young = Fixed::from_int(young);
    p.living_standard = fx(living_standard);
    p.literacy = fx(literacy);
    return p;
}

} // namespace

TEST_CASE("Bevölkerung: Kennzahlen eines Ortes (Abschnitt 4.1)") {
    World w = testhilfen::load_fixture();
    uhrwerk::LocationFactors f;
    // Potsdam: Land, Sandboden (Tragfähigkeit 20/km²), 120 km², 1500 Einwohner, Verwüstung 0,6.
    const uhrwerk::Location& potsdam = w.locations[1];
    REQUIRE(potsdam.id == "potsdam");
    uhrwerk::compute_location_factors(w, potsdam, f);
    CHECK(f.population == w.location_population(1));
    CHECK(f.capacity == potsdam.area_km2 * Fixed::from_int(20));
    CHECK(f.reserve == Fixed::one() - f.population / f.capacity);
    CHECK(f.condition == Fixed::one() - fx("0.6") * fx("0.5"));
    CHECK(f.agrarian == Fixed::one()); // nicht übervölkert

    SUBCASE("übervölkert: Agrarbetriebe teilen sich die Tragfähigkeit") {
        for (Index p = potsdam.first_pop; p < potsdam.first_pop + potsdam.pop_count; ++p) {
            w.pops[static_cast<std::size_t>(p)].working += Fixed::from_int(5000);
        }
        uhrwerk::compute_location_factors(w, potsdam, f);
        CHECK(f.population > f.capacity);
        CHECK(f.reserve == Fixed::zero());
        CHECK(f.agrarian == f.capacity / f.population);
    }
}

TEST_CASE("Bevölkerung: Ziel der Alphabetisierung (Abschnitt 4.4)") {
    const World w = testhilfen::load_fixture();
    const uhrwerk::Location& berlin = w.locations[0];  // Stadt
    const uhrwerk::Location& potsdam = w.locations[1]; // Land
    const PopBlock bauern = block(0, 0, 0, 0, 100);    // Bauern, lutherisch
    // 0,1 × 1,2 × (50 + 50) / 100 = 0,12 auf dem Land; in der Stadt × 1,25.
    CHECK(uhrwerk::literacy_target(w, potsdam, bauern, Fixed::from_int(50)) == fx("0.1") * fx("1.2"));
    CHECK(uhrwerk::literacy_target(w, berlin, bauern, Fixed::from_int(50)) ==
          fx("0.1") * fx("1.2") * fx("1.25"));
    // Höherer Lebensstandard → höheres Ziel, begrenzt auf 1.
    CHECK(uhrwerk::literacy_target(w, potsdam, bauern, Fixed::from_int(90)) >
          uhrwerk::literacy_target(w, potsdam, bauern, Fixed::from_int(50)));
    const PopBlock adel = block(0, 3, 0, 0, 10); // 0,8 × 1,2 × 1,25 × 1,4 > 1
    CHECK(uhrwerk::literacy_target(w, berlin, adel, Fixed::from_int(90)) == Fixed::one());
}

TEST_CASE("Bevölkerung: absorb mittelt nach Personen") {
    PopBlock a = block(0, 0, 0, 0, 300, "40", "0.1");
    a.children = Fixed::from_int(100);
    const PopBlock b = block(0, 0, 0, 1, 100, "80", "0.5");
    uhrwerk::absorb(a, b);
    CHECK(a.size() == Fixed::from_int(500));
    CHECK(a.young == Fixed::from_int(400));
    // (40 × 400 + 80 × 100) / 500 = 48
    CHECK(a.living_standard == Fixed::from_int(48));
    // (0,1 × 400 + 0,5 × 100) / 500 = 0,18 (Festkomma)
    CHECK(a.literacy ==
          (fx("0.1") * Fixed::from_int(400) + fx("0.5") * Fixed::from_int(100)) / Fixed::from_int(500));
}

TEST_CASE("Bevölkerung: ähnlichster Block (Abschnitt 4.7)") {
    // Kandidaten: 0 anderer Beruf/gleiche Konfession, 1 gleicher Beruf, 2 gleicher Beruf + Kultur,
    // 3 gleicher Beruf + Konfession, 4 groß, aber fremd.
    std::vector<PopBlock> blocks = {
        block(0, 9, 1, 9, 50), block(0, 2, 9, 9, 50),  block(0, 2, 9, 3, 50),
        block(0, 2, 1, 9, 50), block(0, 8, 8, 8, 900),
    };
    std::vector<bool> keep(blocks.size(), true);
    const PopBlock pop = block(0, 2, 1, 3, 1);
    CHECK(uhrwerk::most_similar_block(blocks, keep, 0, 5, pop) == 3);
    keep[3] = false;
    CHECK(uhrwerk::most_similar_block(blocks, keep, 0, 5, pop) == 2);
    keep[2] = false;
    CHECK(uhrwerk::most_similar_block(blocks, keep, 0, 5, pop) == 1);
    keep[1] = false;
    CHECK(uhrwerk::most_similar_block(blocks, keep, 0, 5, pop) == 0);
    keep[0] = false;
    CHECK(uhrwerk::most_similar_block(blocks, keep, 0, 5, pop) == 4); // der größte
    keep[4] = false;
    CHECK(uhrwerk::most_similar_block(blocks, keep, 0, 5, pop) == kNone);
    // Bei Gleichstand in Stufe 4 der kleinere Index.
    std::vector<PopBlock> equal = {block(0, 8, 8, 8, 10), block(0, 7, 7, 7, 10)};
    CHECK(uhrwerk::most_similar_block(equal, std::vector<bool>(2, true), 0, 2, pop) == 0);
}

TEST_CASE("Bevölkerung: Neuordnung löst kleine Blöcke auf und legt Zugänge an") {
    World w;
    w.rules.block_min_size = Fixed::one();
    w.locations.resize(3);
    // Ort 0: Blöcke 0 (groß), 1 (zu klein, gleicher Beruf und Konfession wie 0).
    // Ort 1: nur ein zu kleiner Block → bleibt (größter Block eines Ortes bleibt immer).
    // Ort 2: ohne Blöcke.
    w.pops = {block(0, 1, 1, 1, 100), block(0, 1, 1, 2, 0), block(1, 1, 1, 1, 0)};
    w.pops[1].young = fx("0.5");
    w.pops[2].young = fx("0.25");
    w.locations[0].first_pop = 0;
    w.locations[0].pop_count = 2;
    w.locations[1].first_pop = 2;
    w.locations[1].pop_count = 1;
    w.locations[2].first_pop = 3;
    w.locations[2].pop_count = 0;

    w.arrivals.resize(3);
    w.arrivals[1].push_back(block(1, 2, 2, 2, 20)); // neuer Schlüssel in Ort 1
    w.arrivals[0].push_back(block(0, 1, 1, 1, 5));  // gleicher Schlüssel wie Block 0 → aufgenommen
    w.arrivals[1].push_back(block(1, 2, 2, 2, 3));  // gleicher Schlüssel wie der erste Zugang
    CHECK(w.location_population(1) == fx("0.25") + Fixed::from_int(23)); // Zugänge zählen schon mit
    CHECK(uhrwerk::reorganize(w));
    CHECK(w.arrivals[0].empty());
    CHECK(w.arrivals[1].empty());

    REQUIRE(w.pops.size() == 3);
    CHECK(w.locations[0].first_pop == 0);
    CHECK(w.locations[0].pop_count == 1);
    CHECK(w.pops[0].young == Fixed::from_int(105) + fx("0.5"));
    CHECK(w.locations[1].first_pop == 1);
    CHECK(w.locations[1].pop_count == 2);
    CHECK(w.pops[1].young == fx("0.25")); // der einzige alte Block bleibt
    CHECK(w.pops[2].profession == 2);
    CHECK(w.pops[2].young == Fixed::from_int(23));
    CHECK(w.locations[2].first_pop == 3);
    CHECK(w.locations[2].pop_count == 0);

    // Nichts mehr zu tun → false, Welt unverändert.
    w.pops[1].young = Fixed::from_int(2);
    CHECK_FALSE(uhrwerk::reorganize(w));
}
