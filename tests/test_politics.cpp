// Tests der Wahlen (seit M8, docs/technik/politik.md, Abschnitt 7 und 11) mit dem Landtag der Testdaten.

#include "test_hilfen.hpp"
#include "uhrwerk/parallel.hpp"
#include "uhrwerk/politics.hpp"
#include "uhrwerk/savegame.hpp"
#include "uhrwerk/simulation.hpp"

#include <algorithm>
#include <cstdint>
#include <doctest/doctest.h>
#include <set>
#include <string>
#include <vector>

namespace {

using uhrwerk::Date;
using uhrwerk::DistrictResult;
using uhrwerk::ElectionResult;
using uhrwerk::ElectoralSystem;
using uhrwerk::Fixed;
using uhrwerk::Franchise;
using uhrwerk::Index;
using uhrwerk::kNone;
using uhrwerk::LoadedGame;
using uhrwerk::SaveMeta;
using uhrwerk::SimConfig;
using uhrwerk::Simulation;
using uhrwerk::World;

using Bytes = std::vector<std::uint8_t>;

constexpr std::string_view kScenario = "jahresbeginn_1650"; // Start 1. Januar 1650, Wahltag 1. März
constexpr Date kElectionDay{1650, 3, 1};
constexpr std::uint64_t kSeed = 1648;

Fixed fx(const char* text) {
    return *Fixed::parse(text);
}

World landtag_world() {
    return testhilfen::load_fixture(kScenario);
}

/// Gleiche Neigung überall (Grundneigung je Strömung konservative, liberale, demokraten): keine
/// Interessengruppen, keine Verschiebungen, kein Rauschen, direkte Wahl.
World flat_world(Fixed conservative, Fixed liberal, Fixed democrat) {
    World w = landtag_world();
    w.interest_groups.clear();
    w.political_regions.clear();
    uhrwerk::ElectionRules& r = w.election_rules;
    r.base = {conservative, liberal, democrat};
    r.literacy.assign(3, Fixed{});
    r.poverty.assign(3, Fixed{});
    r.city.assign(3, Fixed{});
    r.noise = Fixed{};
    w.parliaments[0].law.indirect = false;
    return w;
}

/// Wahl in einer frisch gestarteten Simulation (Einkommen, Lebensstandard wie am Start).
ElectionResult elect(World w, std::uint64_t seed = kSeed) {
    const Simulation sim(std::move(w));
    return uhrwerk::run_election(sim.world(), 0, seed);
}

Fixed sum(const std::vector<Fixed>& values) {
    Fixed total;
    for (const Fixed v : values) {
        total += v;
    }
    return total;
}

void step_to(Simulation& sim, Date date) {
    while (sim.world().date != date) {
        sim.step_day();
    }
}

LoadedGame load(const Bytes& bytes) {
    return uhrwerk::load_game(bytes, landtag_world(), 1);
}

} // namespace

TEST_CASE("Politik: Neigung, Mitgliedschaften und Fraktionen") {
    const World w = landtag_world();
    REQUIRE(w.currents.size() == 3);
    REQUIRE_FALSE(w.pops.empty());
    for (const uhrwerk::PopBlock& pop : w.pops) {
        const std::vector<Fixed> leaning =
            uhrwerk::current_leaning(w, pop, uhrwerk::region_shift(w, pop.location));
        REQUIRE(leaning.size() == 3);
        CHECK(std::all_of(leaning.begin(), leaning.end(), [](Fixed v) { return v >= Fixed::zero(); }));
        CHECK(std::abs(sum(leaning).to_double() - 1.0) < 0.001);
        const std::vector<Fixed> m = uhrwerk::interest_memberships(w, pop);
        REQUIRE(m.size() == 2);
        if (pop.profession == w.find_profession("bauern")) {
            CHECK(m[0] == fx("0.9"));
        }
    }
    // Region elbtal (SAX) verschiebt zu den Demokraten.
    const std::vector<Fixed> shift = uhrwerk::region_shift(w, w.find_location("dresden"));
    CHECK(shift[static_cast<std::size_t>(w.find_current("demokraten"))] == fx("0.05"));
    CHECK(uhrwerk::region_shift(w, w.find_location("berlin_coelln"))[2] == Fixed::zero());

    const uhrwerk::Parliament& p = w.parliaments[0]; // linke [-100, -10), mitte [-10, 30), rechte [30, 100]
    CHECK(p.seat == w.find_location("dresden"));
    CHECK(uhrwerk::faction_of(p, Fixed::from_int(-100)) == 0);
    CHECK(uhrwerk::faction_of(p, Fixed::from_int(-10)) == 1);
    CHECK(uhrwerk::faction_of(p, fx("29.99")) == 1);
    CHECK(uhrwerk::faction_of(p, Fixed::from_int(30)) == 2);
    CHECK(uhrwerk::faction_of(p, Fixed::from_int(100)) == 2);
    uhrwerk::Parliament none = p;
    none.factions.clear();
    CHECK(uhrwerk::faction_of(none, Fixed::zero()) == kNone);
}

TEST_CASE("Politik: Wahl am Wahltag mit Ergebnis und Chronik") {
    Simulation sim(landtag_world());
    step_to(sim, kElectionDay);
    CHECK(sim.world().elections.empty());
    sim.step_day();
    const World& w = sim.world();
    REQUIRE(w.elections.size() == 1);
    const ElectionResult& e = w.elections[0];
    CHECK(e.parliament == 0);
    CHECK(e.day == kElectionDay);
    REQUIRE(e.districts.size() == 2);
    CHECK(e.deputies.size() == 4); // Dresden 3, Leipzig 1
    std::set<std::int32_t> ranks;
    for (const DistrictResult& d : e.districts) {
        CHECK_FALSE(d.boycott);
        CHECK(d.electorate > d.voters);
        CHECK(d.voters > Fixed::zero());
        CHECK(d.votes.size() == 3);
        CHECK(d.winner != kNone);
        ranks.insert(d.report_rank);
    }
    CHECK(ranks == std::set<std::int32_t>{0, 1});
    for (const uhrwerk::Deputy& a : e.deputies) {
        const DistrictResult& d =
            *std::find_if(e.districts.begin(), e.districts.end(),
                          [&](const DistrictResult& r) { return r.district == a.district; });
        CHECK(a.current == d.winner);
        CHECK(a.position >= Fixed::from_int(-100));
        CHECK(a.position <= Fixed::from_int(100));
    }
    const uhrwerk::ChronicleEntry& entry = w.chronicle.back();
    CHECK(entry.date == kElectionDay);
    CHECK(entry.text.rfind("Wahltag – Sächsischer Landtag: 4 von 4 Sitzen besetzt; stärkste Strömung: ", 0) ==
          0);
    CHECK(entry.state == w.find_state("SAX"));
    CHECK(entry.location == w.find_location("dresden"));
    const std::vector<std::int32_t> by_current = uhrwerk::seats_by_current(w, e);
    CHECK(by_current[0] + by_current[1] + by_current[2] == 4);
    const std::vector<std::int32_t> by_faction = uhrwerk::seats_by_faction(w, e);
    REQUIRE(by_faction.size() == 4); // drei Fraktionen und fraktionslos
    CHECK(by_faction[0] + by_faction[1] + by_faction[2] + by_faction[3] == 4);

    // Keine zweite Wahl im weiteren Verlauf.
    sim.step_days(400);
    CHECK(sim.world().elections.size() == 1);
}

TEST_CASE("Politik: Wahl unabhängig von den Threads, abhängig vom Seed") {
    Simulation one(landtag_world(), SimConfig{kSeed, 1});
    Simulation four(landtag_world(), SimConfig{kSeed, 4});
    one.step_days(70);
    four.step_days(70);
    REQUIRE(one.world().elections.size() == 1);
    CHECK(one.checksum() == four.checksum());

    const World w = one.world();
    uhrwerk::ThreadPool pool(4);
    const ElectionResult serial = uhrwerk::run_election(w, 0, kSeed);
    const ElectionResult parallel = uhrwerk::run_election(w, 0, kSeed, &pool);
    REQUIRE(serial.deputies.size() == parallel.deputies.size());
    for (std::size_t i = 0; i < serial.deputies.size(); ++i) {
        CHECK(serial.deputies[i].position == parallel.deputies[i].position);
        CHECK(serial.deputies[i].faction == parallel.deputies[i].faction);
    }
    for (std::size_t i = 0; i < serial.districts.size(); ++i) {
        CHECK(serial.districts[i].votes == parallel.districts[i].votes);
        CHECK(serial.districts[i].report_rank == parallel.districts[i].report_rank);
    }
    const ElectionResult other = uhrwerk::run_election(w, 0, kSeed + 1);
    CHECK(other.deputies[0].position != serial.deputies[0].position);
}

TEST_CASE("Politik: Mehrheit, Stichwahl und Gleichstand") {
    const Index conservative = 0; // Position 70
    const Index democrat = 2;     // Position -45; liberale (15) steht den Konservativen näher

    SUBCASE("absolute Mehrheit") {
        const ElectionResult e = elect(flat_world(fx("0.6"), fx("0.1"), fx("0.3")));
        for (const DistrictResult& d : e.districts) {
            CHECK(d.winner == conservative);
            CHECK_FALSE(d.runoff);
        }
    }
    SUBCASE("Stichwahl: die Stimmen der Liberalen gehen an die näher stehenden Konservativen") {
        const ElectionResult e = elect(flat_world(fx("0.4"), fx("0.15"), fx("0.45")));
        for (const DistrictResult& d : e.districts) {
            CHECK(d.votes[2] > d.votes[0]);
            CHECK(d.winner == conservative);
            CHECK(d.runoff);
        }
    }
    SUBCASE("relative Mehrheit ohne Stichwahl") {
        World w = flat_world(fx("0.4"), fx("0.15"), fx("0.45"));
        w.parliaments[0].law.system = ElectoralSystem::Plurality;
        const ElectionResult e = elect(std::move(w));
        for (const DistrictResult& d : e.districts) {
            CHECK(d.winner == democrat);
            CHECK_FALSE(d.runoff);
        }
    }
    SUBCASE("Gleichstand: die gemäßigtere Strömung gewinnt") {
        const ElectionResult e = elect(flat_world(fx("0.5"), Fixed{}, fx("0.5")));
        for (const DistrictResult& d : e.districts) {
            CHECK(d.votes[0] == d.votes[2]);
            CHECK(d.winner == democrat); // |-45| < |70|
        }
    }
}

TEST_CASE("Politik: Boykott") {
    World w = landtag_world();
    const Index culture = w.location_majority_culture(w.find_location("dresden"));
    w.parliaments[0].boycott_cultures = {culture};
    const ElectionResult e = elect(std::move(w));
    const World check = landtag_world();
    std::int32_t open_seats = 0;
    bool any = false;
    for (const DistrictResult& d : e.districts) {
        if (d.boycott) {
            any = true;
            CHECK(d.electorate > Fixed::zero());
            CHECK(d.voters == Fixed::zero());
            CHECK(sum(d.votes) == Fixed::zero());
            CHECK(d.winner == kNone);
        } else {
            open_seats += check.electoral_districts[static_cast<std::size_t>(d.district)].seats;
        }
    }
    CHECK(any);
    CHECK(static_cast<std::int32_t>(e.deputies.size()) == open_seats);
}

TEST_CASE("Politik: Wahlrecht") {
    const ElectionResult manhood = elect(landtag_world());
    const auto electorate = [](const ElectionResult& e) {
        Fixed total;
        for (const DistrictResult& d : e.districts) {
            total += d.electorate;
        }
        return total;
    };

    SUBCASE("Dreiklassen: Klassen gleich gewichtet, Wähler mit hohem Einkommen zählen mehr") {
        World w = landtag_world();
        w.parliaments[0].law.indirect = false;
        w.election_rules.noise = Fixed{};
        const ElectionResult direct = elect(w);
        w.parliaments[0].law.franchise = Franchise::ThreeClass;
        const ElectionResult three = elect(std::move(w));
        CHECK(electorate(three) > electorate(direct)); // ab 24 statt ab 25
        Fixed liberal_three;
        Fixed liberal_manhood;
        for (std::size_t i = 0; i < three.districts.size(); ++i) {
            const DistrictResult& d = three.districts[i];
            // Je Ort zählt jede besetzte Klasse ein Drittel der Wähler: Die Stimmen ergeben die Wähler.
            CHECK(std::abs((sum(d.votes) - d.voters).to_double()) < d.voters.to_double() / 1000);
            liberal_three += d.votes[1] / sum(d.votes);
            liberal_manhood += direct.districts[i].votes[1] / sum(direct.districts[i].votes);
        }
        CHECK(liberal_three > liberal_manhood); // Kaufleute und Adel (Bildungsbürgertum) wählen liberal
    }
    SUBCASE("Zensus") {
        World w = landtag_world();
        w.parliaments[0].law.franchise = Franchise::Census;
        w.parliaments[0].law.census_threshold = Fixed{};
        CHECK(electorate(elect(w)) == electorate(manhood));
        w.parliaments[0].law.census_threshold = Fixed::from_int(1'000'000);
        const Fixed privileged = electorate(elect(std::move(w))); // nur Stände ohne Kontribution
        CHECK(privileged > Fixed::zero());
        CHECK(privileged < electorate(manhood) / Fixed::from_int(5));
    }
    SUBCASE("ohne Wahlrecht keine Berechtigten") {
        World w = landtag_world();
        w.parliaments[0].law.franchise = Franchise::Estates;
        const ElectionResult e = elect(std::move(w));
        CHECK(electorate(e) == Fixed::zero());
        CHECK(e.deputies.empty());
    }
}

TEST_CASE("Politik: Spielstand vor und nach dem Wahltag") {
    Simulation sim(landtag_world());
    for (const std::int64_t day : {50, 70}) { // 20. Februar und 12. März 1650
        sim.step_days(day - sim.days_elapsed());
        const Bytes bytes = uhrwerk::save_game(sim, SaveMeta{"SAX"});
        LoadedGame loaded = load(bytes);
        INFO(loaded.error);
        REQUIRE(loaded.ok());
        CHECK(loaded.simulation->world().elections.size() == sim.world().elections.size());
        Simulation copy = sim;
        copy.step_days(40);
        loaded.simulation->step_days(40);
        CHECK(loaded.simulation->checksum() == copy.checksum());
        CHECK(loaded.simulation->world().elections.size() == 1);
    }

    SUBCASE("ungültiger Verweis im Wahlergebnis") {
        World w = sim.world();
        w.elections[0].districts[0].district = 99;
        const Bytes bytes = uhrwerk::save_game(Simulation(std::move(w)), SaveMeta{});
        const LoadedGame loaded = load(bytes);
        CHECK(loaded.error.find("Wahlergebnis mit ungültigem Verweis") != std::string::npos);
    }
    SUBCASE("Formatwache der Wahlen") {
        // Wie „Spielstand: Formatwache“, für den Zustand der Wahlen (nur in Welten mit Parlamenten).
        Simulation year(landtag_world());
        year.step_days(365);
        const auto info = uhrwerk::read_save_info(uhrwerk::save_game(year, SaveMeta{}));
        REQUIRE(info.has_value());
        CHECK_MESSAGE(
            uhrwerk::hex16(info->payload_hash) == "53d0d332aa53477c",
            "Prüfwert der Nutzdaten geändert: Formatversion erhöhen oder Referenzwert neu festhalten");
    }
}

TEST_CASE("Politik: Spielstand im Format 1 zeigt seine Angaben, lädt aber nicht") {
    // Format 1 (bis M7) hat 13 Inhaltsbereiche, der Gesamtwert läuft über diese 13.
    const World w = testhilfen::load_fixture();
    Bytes bytes = uhrwerk::save_game(Simulation(w), SaveMeta{});
    const uhrwerk::ContentFingerprint fp = uhrwerk::content_fingerprint(w);
    std::uint64_t total = 0xcbf29ce484222325ULL;
    for (std::size_t i = 0; i < 13; ++i) {
        for (int b = 0; b < 8; ++b) {
            total ^= static_cast<std::uint8_t>(fp.parts[i] >> (8 * b));
            total *= 0x100000001b3ULL;
        }
    }
    const std::string needle = "\ninhalt=";
    const auto it = std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end());
    REQUIRE(it != bytes.end());
    const std::string value = uhrwerk::hex16(total);
    std::copy(value.begin(), value.end(), it + static_cast<std::ptrdiff_t>(needle.size()));
    bytes[8] = 1; // Formatversion im Kopf

    std::string error;
    const auto info = uhrwerk::read_save_info(bytes, &error);
    INFO(error);
    REQUIRE(info.has_value());
    CHECK(info->format_version == 1);
    CHECK(info->content.parts[0] == fp.parts[0]);
    CHECK(info->content.parts[13] == 0);
    const LoadedGame loaded = uhrwerk::load_game(bytes, w, 1);
    CHECK(loaded.error == "Spielstand im Format 1 wird nicht unterstützt");
}
