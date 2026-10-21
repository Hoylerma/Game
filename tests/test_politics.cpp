// Tests der Wahlen (seit M8, docs/technik/politik.md, Abschnitt 7 und 11) mit dem Landtag der Testdaten.

#include "test_hilfen.hpp"
#include "uhrwerk/election_lab.hpp"
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

TEST_CASE("Politik: Stand der Interessengruppen je Staat") {
    World w = landtag_world();
    const Index saxony = w.find_state("SAX");
    REQUIRE(saxony != kNone);
    CHECK(uhrwerk::interest_group_standing(w, kNone).empty());
    CHECK(uhrwerk::interest_group_standing(w, static_cast<Index>(w.states.size())).empty());
    // Bauernschaft: 90 % der erwachsenen Bauern; Einfluss mit gleichem Einkommen je Kopf überall.
    w.location_income.assign(w.locations.size() * w.professions.size(), Fixed::one());
    Fixed adults;
    Fixed farmers;
    Fixed people;
    Fixed farm_people;
    for (const Index l : w.state_locations(saxony)) {
        const uhrwerk::Location& loc = w.locations[static_cast<std::size_t>(l)];
        for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
            const uhrwerk::PopBlock& pop = w.pops[static_cast<std::size_t>(p)];
            const Fixed grown = pop.young + pop.working + pop.old;
            adults += grown;
            people += pop.size();
            if (pop.profession == w.find_profession("bauern")) {
                farmers += grown;
                farm_people += pop.size();
            }
        }
    }
    REQUIRE(farmers > Fixed::zero());
    const std::vector<uhrwerk::InterestGroupStanding> standing = uhrwerk::interest_group_standing(w, saxony);
    REQUIRE(standing.size() == 2);
    CHECK(std::abs(standing[0].share.to_double() - 0.9 * farmers.to_double() / adults.to_double()) < 0.001);
    CHECK(std::abs(standing[0].influence.to_double() - 0.9 * farm_people.to_double() / people.to_double()) <
          0.001);
    for (const uhrwerk::InterestGroupStanding& g : standing) {
        CHECK(g.share >= Fixed::zero());
        CHECK(g.share <= Fixed::one());
        CHECK(g.influence >= Fixed::zero());
        CHECK(g.influence <= Fixed::one());
    }
    // Ohne Einkommen kein Einfluss, der Anteil bleibt.
    w.location_income.assign(w.location_income.size(), Fixed{});
    const std::vector<uhrwerk::InterestGroupStanding> poor = uhrwerk::interest_group_standing(w, saxony);
    CHECK(poor[0].influence == Fixed::zero());
    CHECK(poor[0].share == standing[0].share);
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
    const Index liberal = 1;      // Position 15
    const Index democrat = 2;     // Position -45

    SUBCASE("absolute Mehrheit") {
        const ElectionResult e = elect(flat_world(fx("0.6"), fx("0.1"), fx("0.3")));
        for (const DistrictResult& d : e.districts) {
            CHECK(d.winner == conservative);
            CHECK_FALSE(d.runoff);
        }
    }
    SUBCASE("Stichwahl: die Konservativen geben anteilig nach der Nähe, überwiegend an die Liberalen") {
        // Konservative zu den Liberalen 55, zu den Demokraten 115: 115/170 an die Liberalen,
        // also 0,40 + 0,108 gegen 0,44 + 0,052.
        const ElectionResult e = elect(flat_world(fx("0.16"), fx("0.4"), fx("0.44")));
        for (const DistrictResult& d : e.districts) {
            CHECK(d.votes[2] > d.votes[1]);
            CHECK(d.winner == liberal);
            CHECK(d.runoff);
        }
    }
    SUBCASE("Stichwahl: knapper Rückstand reicht nicht, wenn die Dritten fast gleich nah stehen") {
        // Liberale zu den Konservativen 55, zu den Demokraten 60: fast halbe-halbe.
        const ElectionResult e = elect(flat_world(fx("0.4"), fx("0.15"), fx("0.45")));
        for (const DistrictResult& d : e.districts) {
            CHECK(d.winner == democrat);
            CHECK(d.runoff);
        }
    }
    SUBCASE("relative Mehrheit ohne Stichwahl") {
        World w = flat_world(fx("0.16"), fx("0.4"), fx("0.44"));
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

TEST_CASE("Politik: Boykott nur in den genannten Märkten") {
    World w = landtag_world();
    const Index dresden = w.find_location("dresden");
    w.parliaments[0].boycott_cultures = {w.location_majority_culture(dresden)};
    w.parliaments[0].boycott_markets = {w.locations[static_cast<std::size_t>(dresden)].market};
    World elsewhere = w;
    const ElectionResult e = elect(std::move(w));
    CHECK(e.districts[0].boycott); // lt_dresden
    for (Index m = 0; m < static_cast<Index>(elsewhere.markets.size()); ++m) {
        if (m != elsewhere.locations[static_cast<std::size_t>(dresden)].market) {
            elsewhere.parliaments[0].boycott_markets = {m};
            break;
        }
    }
    const ElectionResult other = elect(std::move(elsewhere));
    CHECK_FALSE(other.districts[0].boycott);
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
            uhrwerk::hex16(info->payload_hash) == "ec2bc31377c9c171",
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

// ─── Wahllabor (seit M8, Schritt a4) ─────────────────────────────────────────

TEST_CASE("Wahllabor: Wahlziele lesen") {
    const World w = landtag_world();
    SUBCASE("gültig") {
        const uhrwerk::ElectionTargetsResult r = uhrwerk::parse_election_targets(R"(
            wahl landtag {
                besetzt     = { 3 4 }
                beteiligung = { 0.2 0.9 }
                block links { fraktionen = { linke fraktionslos } anteil = { 0 0.5 } }
                block mitte { stroemungen = { liberale } anteil = { 0.1 1 } }
            })",
                                                                                 w);
        INFO(r.errors.size());
        REQUIRE(r.ok());
        const uhrwerk::ElectionTargets* t = r.find(0);
        REQUIRE(t != nullptr);
        CHECK(t->seated->min == Fixed::from_int(3));
        CHECK(t->turnout->max == fx("0.9"));
        REQUIRE(t->blocks.size() == 2);
        CHECK(t->blocks[0].factions == std::vector<Index>{0, kNone});
        CHECK(t->blocks[1].currents == std::vector<Index>{w.find_current("liberale")});
        CHECK(r.find(1) == nullptr);
    }
    SUBCASE("Fehler") {
        const uhrwerk::ElectionTargetsResult r = uhrwerk::parse_election_targets(R"(
            wahl reichstag { }
            wahl landtag {
                farbe = rot
                block a { fraktionen = { zentrum } anteil = { 0 1 } }
                block b { stroemungen = { radikale } anteil = { 0 1 } }
                block c { fraktionen = { linke } stroemungen = { liberale } anteil = { 0 1 } }
                block d { fraktionen = { linke } anteil = { 0.5 2 } }
                block e { fraktionen = { mitte mitte } anteil = { 0 1 } }
                beteiligung = { 45 70 }
            }
            wahl landtag { })",
                                                                                 w, "ziele.chron");
        CHECK_FALSE(r.ok());
        CHECK(r.targets.empty());
        const auto has = [&](std::string_view needle) {
            return std::any_of(r.errors.begin(), r.errors.end(),
                               [&](const std::string& e) { return e.find(needle) != std::string::npos; });
        };
        CHECK(has("unbekanntes Parlament 'reichstag'"));
        CHECK(has("unbekanntes Feld 'farbe'"));
        CHECK(has("unbekannte Fraktion 'zentrum' in parlament landtag"));
        CHECK(has("unbekannte Strömung 'radikale'"));
        CHECK(has("block c braucht genau eines von 'fraktionen' und 'stroemungen'"));
        CHECK(has("'anteil' erwartet einen Bereich innerhalb von 0 bis 1"));
        CHECK(has("'mitte' steht mehrfach in block e"));
        CHECK(has("'beteiligung' erwartet einen Bereich innerhalb von 0 bis 1"));
        CHECK(has("Zielwerte für parlament landtag sind mehrfach angegeben"));
        CHECK(has("ziele.chron:"));
    }
}

TEST_CASE("Wahllabor: Messung und Zusammenfassung") {
    const World w = landtag_world();
    std::vector<uhrwerk::ElectionRun> runs;
    for (std::uint64_t seed = 0; seed < 3; ++seed) {
        Simulation sim(w, SimConfig{kSeed + seed, 1});
        const auto e = uhrwerk::run_to_election(sim, 0);
        REQUIRE(e.has_value());
        CHECK(e->day == kElectionDay);
        CHECK(sim.world().date == Date{1650, 3, 2});                  // der Wahltag ist gerechnet
        CHECK(uhrwerk::run_to_election(sim, 0)->day == kElectionDay); // schon gewählt: dasselbe Ergebnis
        CHECK_FALSE(uhrwerk::run_to_election(sim, 5).has_value());
        runs.push_back(uhrwerk::measure_election(sim.world(), *e));
    }
    const uhrwerk::ElectionRun& run = runs[0];
    CHECK(run.seated == 4);
    CHECK(run.by_faction.size() == 4);
    CHECK(run.turnout > Fixed::zero());
    CHECK(run.turnout < Fixed::one());
    const auto sax = static_cast<std::size_t>(w.find_state("SAX"));
    CHECK(run.state_seats[sax] == 4);
    CHECK(run.state_electorate[sax] > run.state_voters[sax]);

    const uhrwerk::ElectionTargetsResult targets = uhrwerk::parse_election_targets(R"(
        wahl landtag {
            besetzt = { 4 4 }
            beteiligung = { 0.95 1 }
            block alle { fraktionen = { linke mitte rechte fraktionslos } anteil = { 1 1 } }
            block nichts { stroemungen = { liberale } anteil = { 2 2 } }
        })",
                                                                                   w);
    // 'anteil' außerhalb von 0..1 ist ein Lesefehler; ohne den Block prüfen.
    CHECK_FALSE(targets.ok());
    const uhrwerk::ElectionTargetsResult valid = uhrwerk::parse_election_targets(R"(
        wahl landtag {
            besetzt = { 4 4 }
            beteiligung = { 0.95 1 }
            block alle { fraktionen = { linke mitte rechte fraktionslos } anteil = { 1 1 } }
            block keiner { fraktionen = { linke mitte rechte fraktionslos } anteil = { 0 0.5 } }
        })",
                                                                                 w);
    REQUIRE(valid.ok());
    const uhrwerk::ElectionSummary s = uhrwerk::summarize_elections(w, 0, runs, valid.find(0));
    CHECK(s.runs == 3);
    CHECK(s.seated.median == Fixed::from_int(4));
    REQUIRE(s.block_share.size() == 2);
    CHECK(s.block_share[0].median == Fixed::one());
    REQUIRE(s.violations.size() == 2); // Beteiligung und Block keiner
    CHECK(s.violations[0].check == uhrwerk::ElectionCheck::Turnout);
    CHECK(s.violations[1].check == uhrwerk::ElectionCheck::Block);
    CHECK(s.violations[1].block == 1);
    const uhrwerk::ElectionSummary plain = uhrwerk::summarize_elections(w, 0, runs, nullptr);
    CHECK(plain.violations.empty());
    CHECK(plain.by_current.size() == 3);
}

// ─── Wahlkampf (seit M8, Schritt b2) ─────────────────────────────────────────

namespace {

uhrwerk::Command campaign_command(Index current, Index district, uhrwerk::CampaignAction action) {
    uhrwerk::Command c;
    c.type = uhrwerk::CommandType::Campaign;
    c.state = current;
    c.target = district;
    c.action = action;
    return c;
}

} // namespace

TEST_CASE("Wahlkampf: Spenden, Kosten und abnehmender Ertrag") {
    Simulation sim(landtag_world());
    const World& w = sim.world();
    const std::size_t currents = w.currents.size();
    REQUIRE(w.campaign.size() == w.electoral_districts.size() * currents);
    REQUIRE(w.party_funds.size() == currents);
    sim.step_day(); // 1. Januar 1650: Spenden des Monats
    const Index democrats = w.find_current("demokraten");
    const auto d = static_cast<std::size_t>(democrats);
    REQUIRE(w.party_donations[d] > Fixed::zero());
    CHECK(w.party_funds[d] == w.party_donations[d]);
    const uhrwerk::CampaignRule& rally = w.election_rules.campaign[0];

    const Fixed before = w.party_funds[d];
    sim.submit(campaign_command(democrats, 0, uhrwerk::CampaignAction::Rally));
    sim.submit(campaign_command(democrats, 0, uhrwerk::CampaignAction::Rally));
    // Ungültig: unbekannte Strömung, unbekannter Wahlkreis, unbekannte Art.
    sim.submit(campaign_command(-1, 0, uhrwerk::CampaignAction::Rally));
    sim.submit(campaign_command(democrats, 99, uhrwerk::CampaignAction::Rally));
    sim.submit(campaign_command(democrats, 0, static_cast<uhrwerk::CampaignAction>(3)));
    // Verplant sind die beiden gültigen Versammlungen der Demokraten (und die mit unbekanntem
    // Wahlkreis, die die Brücke nie einreiht); die unbekannte Art kostet nichts.
    CHECK(sim.reserved_campaign_funds(democrats) == rally.cost * Fixed::from_int(3));
    CHECK(sim.reserved_campaign_funds(w.find_current("liberale")) == Fixed::zero());
    sim.step_day();
    CHECK(sim.reserved_campaign_funds(democrats) == Fixed::zero());
    const uhrwerk::CampaignState& state = w.campaign[d];
    const Fixed first = rally.effect;
    const Fixed second = first + rally.effect * (Fixed::one() - first / rally.cap);
    CHECK(state.mobilization == second);
    CHECK(w.party_funds[d] == before - rally.cost - rally.cost);
    CHECK(uhrwerk::campaign_allowed(w, democrats, 0, uhrwerk::CampaignAction::Club) ==
          (w.party_funds[d] >= w.election_rules.campaign[2].cost));

    SUBCASE("ohne Mittel kein Wahlkampf") {
        World poor = w;
        poor.party_funds[d] = Fixed{};
        CHECK_FALSE(uhrwerk::campaign_allowed(poor, democrats, 0, uhrwerk::CampaignAction::Leaflets));
        CHECK_FALSE(uhrwerk::apply_campaign(poor, democrats, 0, uhrwerk::CampaignAction::Leaflets));
        CHECK(poor.campaign[d].persuasion == Fixed::zero());
    }
    SUBCASE("Obergrenze") {
        World rich = w;
        rich.party_funds[d] = Fixed::from_int(1'000'000);
        for (int i = 0; i < 200; ++i) {
            uhrwerk::apply_campaign(rich, democrats, 0, uhrwerk::CampaignAction::Rally);
        }
        CHECK(rich.campaign[d].mobilization <= rally.cap);
        CHECK(rich.campaign[d].mobilization > rally.cap - fx("0.01"));
    }
}

TEST_CASE("Wahlkampf: Wirkung auf die Stimmen und Ende mit der Wahl") {
    Simulation sim(landtag_world());
    sim.step_day();
    const World& w = sim.world();
    const Index democrats = w.find_current("demokraten");
    const auto d = static_cast<std::size_t>(democrats);
    const uhrwerk::DistrictEstimate plain = uhrwerk::estimate_district(w, 0);
    REQUIRE(plain.shares.size() == w.currents.size());
    CHECK(std::abs(sum(plain.shares).to_double() - 1.0) < 0.001);

    World campaigned = w;
    campaigned.party_funds[d] = Fixed::from_int(1000);
    REQUIRE(uhrwerk::apply_campaign(campaigned, democrats, 0, uhrwerk::CampaignAction::Rally));
    const uhrwerk::DistrictEstimate rally = uhrwerk::estimate_district(campaigned, 0);
    CHECK(rally.shares[d] > plain.shares[d]);
    REQUIRE(uhrwerk::apply_campaign(campaigned, democrats, 0, uhrwerk::CampaignAction::Leaflets));
    const uhrwerk::DistrictEstimate leaflets = uhrwerk::estimate_district(campaigned, 0);
    CHECK(leaflets.shares[d] > rally.shares[d]);
    CHECK(uhrwerk::estimate_district(campaigned, 1).shares == uhrwerk::estimate_district(w, 1).shares);

    // Bis zum Wahltag wirkt alles; danach bleiben nur die Vereine.
    sim.submit(campaign_command(democrats, 0, uhrwerk::CampaignAction::Rally));
    sim.submit(campaign_command(democrats, 0, uhrwerk::CampaignAction::Club));
    step_to(sim, kElectionDay);
    CHECK(w.campaign[d].mobilization > Fixed::zero());
    sim.submit(campaign_command(democrats, 1, uhrwerk::CampaignAction::Rally)); // am Wahltag: zählt
    sim.step_day();
    REQUIRE(w.elections.size() == 1);
    CHECK(w.campaign[d].mobilization == Fixed::zero());
    CHECK(w.campaign[d].persuasion == Fixed::zero());
    CHECK(w.campaign[d].club > Fixed::zero());
    const Fixed funds = w.party_funds[d];
    sim.submit(campaign_command(democrats, 0, uhrwerk::CampaignAction::Rally)); // nach der Wahl: nichts
    sim.step_day();
    CHECK(w.party_funds[d] == funds);
    CHECK_FALSE(uhrwerk::campaign_allowed(w, democrats, 0, uhrwerk::CampaignAction::Rally));
}

TEST_CASE("Wahlkampf: Wiederholung und Spielstand mit Wahlkampfbefehlen") {
    const auto play = [](Simulation& sim, std::int64_t days) {
        const World& w = sim.world();
        while (sim.days_elapsed() < days) {
            const std::int64_t day = sim.days_elapsed();
            if (day % 5 == 1) {
                const auto current = static_cast<Index>(day % static_cast<std::int64_t>(w.currents.size()));
                const auto action = static_cast<uhrwerk::CampaignAction>(day % 3);
                sim.submit(campaign_command(current, static_cast<Index>(day % 2), action));
            }
            sim.step_day();
        }
    };
    Simulation one(landtag_world(), SimConfig{kSeed, 1});
    Simulation four(landtag_world(), SimConfig{kSeed, 4});
    play(one, 40);
    play(four, 40);
    CHECK(one.checksum() == four.checksum());
    const Bytes bytes = uhrwerk::save_game(one, SaveMeta{"SAX"});
    LoadedGame loaded = load(bytes);
    INFO(loaded.error);
    REQUIRE(loaded.ok());
    CHECK(loaded.simulation->command_log() == one.command_log());
    play(one, 90);
    play(*loaded.simulation, 90);
    CHECK(loaded.simulation->checksum() == one.checksum());

    const uhrwerk::Replay replay = uhrwerk::make_replay(one);
    const uhrwerk::ReplayResult result = uhrwerk::run_replay(replay, landtag_world(), 2);
    INFO(result.error);
    CHECK(result.ok);
    CHECK(result.mismatch_day == -1);
}
