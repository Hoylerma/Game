// Bauen (seit M15b1, docs/technik/bauen_und_anleitung.md Abschnitt 3): Landesteile, Bauaufträge, Wirkung,
// Anwerbung, Spielstand. Mit den Spieldaten 1848.

#include "uhrwerk/buildings.hpp"
#include "uhrwerk/loader.hpp"
#include "uhrwerk/savegame.hpp"
#include "uhrwerk/simulation.hpp"

#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <vector>

using uhrwerk::Command;
using uhrwerk::CommandType;
using uhrwerk::Fixed;
using uhrwerk::Index;
using uhrwerk::kNone;
using uhrwerk::Simulation;
using uhrwerk::World;

namespace {

World world_1848() {
    uhrwerk::LoadResult data = uhrwerk::load_world(UHRWERK_GAME_DATA_DIR, "voelkerfruehling");
    REQUIRE(data.ok());
    return std::move(*data.world);
}

Index type_of(const World& w, const std::string& id) {
    for (std::size_t i = 0; i < w.building_types.size(); ++i) {
        if (w.building_types[i].id == id) {
            return static_cast<Index>(i);
        }
    }
    return kNone;
}

Index region_of(const World& w, const std::string& id) {
    for (std::size_t i = 0; i < w.regions.size(); ++i) {
        if (w.regions[i].id == id) {
            return static_cast<Index>(i);
        }
    }
    return kNone;
}

Command build(Index state, Index region, Index type, CommandType kind = CommandType::Build) {
    Command c;
    c.type = kind;
    c.state = state;
    c.target = region;
    c.value = Fixed::from_int(type);
    return c;
}

Fixed heads_in(const World& w, Index region, Index profession) {
    Fixed total;
    for (const Index li : w.regions[static_cast<std::size_t>(region)].locations) {
        const uhrwerk::Location& loc = w.locations[static_cast<std::size_t>(li)];
        for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
            if (w.pops[static_cast<std::size_t>(p)].profession == profession) {
                total += w.pops[static_cast<std::size_t>(p)].size();
            }
        }
        for (const uhrwerk::PopBlock& a : w.arrivals[static_cast<std::size_t>(li)]) {
            if (a.profession == profession) {
                total += a.size();
            }
        }
    }
    return total;
}

} // namespace

TEST_CASE("Bauen: Landesteile aus Verwaltung, Kreisen und Orten") {
    const World w = world_1848();
    REQUIRE(w.building_rules.active);
    REQUIRE(!w.regions.empty());
    std::size_t with_data = 0;
    std::size_t assigned = 0;
    for (std::size_t li = 0; li < w.locations.size(); ++li) {
        if (w.locations[li].has_data()) {
            ++with_data;
            assigned += w.location_region[li] != kNone ? 1 : 0;
        }
    }
    CHECK(assigned == with_data);
    // Preußen: seine Provinzen (mit Daten acht) statt 327 Kreise.
    const Index prussia = w.find_state("PRU");
    std::size_t prussian = 0;
    for (std::size_t r = 0; r < w.regions.size(); ++r) {
        if (uhrwerk::region_owner(w, static_cast<Index>(r)) == prussia &&
            w.regions[r].id.rfind("verwaltung:", 0) == 0) {
            ++prussian;
        }
    }
    CHECK(prussian >= 6);
    CHECK(prussian <= 13);
    CHECK(w.regions.size() < 200); // statt 1.447 Orten
}

TEST_CASE("Bauen: 1648 ohne Gebäude") {
    uhrwerk::LoadResult data = uhrwerk::load_world(UHRWERK_GAME_DATA_DIR);
    REQUIRE(data.ok());
    CHECK(!data.world->building_rules.active);
    Simulation sim(std::move(*data.world));
    CHECK(sim.world().building_levels.empty());
    CHECK(sim.world().building_yield.empty());
}

TEST_CASE("Bauen: Auftrag, Raten, Fertigstellung, Wirkung und Anwerbung") {
    World base = world_1848();
    const Index prussia = base.find_state("PRU");
    const Index spinnerei = type_of(base, "spinnerei");
    REQUIRE(spinnerei != kNone);
    // Ein preußischer Landesteil mit Städten (die Spinnerei steht nur in Städten).
    Index region = kNone;
    for (std::size_t r = 0; r < base.regions.size() && region == kNone; ++r) {
        if (uhrwerk::region_owner(base, static_cast<Index>(r)) == prussia &&
            uhrwerk::build_problem(base, prussia, static_cast<Index>(r), spinnerei).empty()) {
            region = static_cast<Index>(r);
        }
    }
    REQUIRE(region != kNone);
    Simulation sim(base);
    const Fixed cost = uhrwerk::build_cost(sim.world(), region, spinnerei);
    CHECK(cost > Fixed::zero());
    const std::size_t chronicle = sim.world().chronicle.size();
    sim.submit(build(prussia, region, spinnerei));
    sim.step_day();
    REQUIRE(sim.world().build_orders.size() == 1);
    const uhrwerk::BuildOrder order = sim.world().build_orders.front();
    CHECK(order.months_left == base.building_types[static_cast<std::size_t>(spinnerei)].months);
    bool announced = false;
    for (std::size_t i = chronicle; i < sim.world().chronicle.size(); ++i) {
        announced = announced || sim.world().chronicle[i].text.find("Spinnerei") != std::string::npos;
    }
    CHECK(announced);

    SUBCASE("ein zweiter Auftrag derselben Art im selben Landesteil geht nicht") {
        CHECK(!uhrwerk::build_problem(sim.world(), prussia, region, spinnerei).empty());
    }
    SUBCASE("Abbrechen") {
        sim.submit(build(prussia, region, spinnerei, CommandType::CancelBuild));
        sim.step_day();
        CHECK(sim.world().build_orders.empty());
    }
    SUBCASE("fertig nach der Bauzeit, mit Mehrertrag und Anwerbung") {
        const Index workers = base.building_types[static_cast<std::size_t>(spinnerei)].profession;
        const Fixed before = heads_in(sim.world(), region, workers);
        bool paid = false;
        for (int day = 0; day < 31 * (order.months_left + 1) && !sim.world().build_orders.empty(); ++day) {
            sim.step_day();
            paid = paid || sim.world().states[static_cast<std::size_t>(prussia)].budget.building >=
                               order.monthly_cost;
        }
        CHECK(paid);
        REQUIRE(sim.world().build_orders.empty());
        CHECK(uhrwerk::building_level(sim.world(), region, spinnerei) == 1);
        const std::size_t businesses = sim.world().businesses.size();
        const Index business = base.building_types[static_cast<std::size_t>(spinnerei)].business;
        const Index first = sim.world().regions[static_cast<std::size_t>(region)].locations.front();
        CHECK(sim.world().building_yield[static_cast<std::size_t>(first) * businesses +
                                         static_cast<std::size_t>(business)] ==
              base.building_types[static_cast<std::size_t>(spinnerei)].yield);
        sim.step_days(95); // drei Monatserste mit Anwerbung
        CHECK(heads_in(sim.world(), region, workers) > before);

        // Spielstand: Stufen bleiben, die Rechnung läuft gleich weiter.
        const std::vector<std::uint8_t> bytes = uhrwerk::save_game(sim, uhrwerk::SaveMeta{});
        uhrwerk::LoadedGame loaded = uhrwerk::load_game(bytes, base, 2);
        INFO(loaded.error);
        REQUIRE(loaded.ok());
        CHECK(uhrwerk::building_level(loaded.simulation->world(), region, spinnerei) == 1);
        CHECK(loaded.simulation->world().building_yield == sim.world().building_yield);
        sim.step_days(40);
        loaded.simulation->step_days(40);
        CHECK(loaded.simulation->checksum() == sim.checksum());
    }
}

TEST_CASE("Bauen: ungültige Aufträge") {
    World w = world_1848();
    const Index prussia = w.find_state("PRU");
    const Index school = type_of(w, "volksschule");
    REQUIRE(school != kNone);
    Simulation sim(w);
    // Ein Landesteil, der Preußen nicht gehört.
    Index foreign = kNone;
    for (std::size_t r = 0; r < w.regions.size() && foreign == kNone; ++r) {
        bool any = false;
        for (const Index li : w.regions[r].locations) {
            any = any || w.locations[static_cast<std::size_t>(li)].owner == prussia;
        }
        if (!any) {
            foreign = static_cast<Index>(r);
        }
    }
    REQUIRE(foreign != kNone);
    CHECK(!uhrwerk::build_problem(sim.world(), prussia, foreign, school).empty());
    sim.submit(build(prussia, foreign, school));
    sim.submit(build(prussia, 1'000'000, school));
    sim.submit(build(prussia, foreign, 99));
    sim.step_day();
    CHECK(sim.world().build_orders.empty());
    // Bauamt: höchstens order_capacity Aufträge zugleich.
    const std::int32_t capacity = uhrwerk::order_capacity(sim.world(), prussia);
    CHECK(capacity >= 5);
    std::int32_t submitted = 0;
    for (const Index type : {school, type_of(w, "mustergut")}) {
        for (std::size_t r = 0; r < w.regions.size(); ++r) {
            if (uhrwerk::region_owner(w, static_cast<Index>(r)) == prussia) {
                sim.submit(build(prussia, static_cast<Index>(r), type));
                ++submitted;
            }
        }
    }
    REQUIRE(submitted > capacity);
    sim.step_day();
    CHECK(uhrwerk::orders_of(sim.world(), prussia) == capacity);
}
