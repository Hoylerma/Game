// Tests des Militärs (seit M10, Schritt a1; docs/technik/militaer.md Abschnitte 3.1 und 3.2).
#include "test_hilfen.hpp"
#include "uhrwerk/military.hpp"
#include "uhrwerk/savegame.hpp"
#include "uhrwerk/simulation.hpp"

#include <algorithm>
#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <string_view>
#include <vector>

namespace {

using uhrwerk::Fixed;
using uhrwerk::Index;
using uhrwerk::SaveMeta;
using uhrwerk::SimConfig;
using uhrwerk::Simulation;
using uhrwerk::World;
using Bytes = std::vector<std::uint8_t>;

Fixed difference(Fixed a, Fixed b) {
    return a > b ? a - b : b - a;
}

constexpr std::string_view kNeighbors = "ort_a;ort_b;grenze;fluss\n"
                                        "berlin_coelln;potsdam;120;0\n"
                                        "potsdam;zerbst;40;1\n"
                                        "zerbst;dessau;60;0\n"
                                        "dessau;leipzig;50;5\n"
                                        "leipzig;dresden;90;3\n";

constexpr std::string_view kMilitary =
    "militaer {\n    seit = 1648.01.01\n    fluss_rang = 3\n}\n"
    "heerwesen standard {\n    bewaffnung = 1\n    geschuetze = 2\n"
    "    mobilmachung = 2\n    mobilmachung_tage = 30\n}\n"
    "heerwesen bra {\n    staat = BRA\n    seit = 1600.01.01\n    bewaffnung = 1.5\n"
    "    geschuetze = 4\n    mobilmachung = 2.5\n    mobilmachung_tage = 20\n}\n"
    "garnison bra_berlin { staat = BRA ort = berlin_coelln anteil = 0.6 name = \"Garde\" }\n"
    "garnison bra_potsdam { staat = BRA ort = potsdam anteil = 0.4 }\n"
    "garnison bra_spaeter { staat = BRA ort = potsdam anteil = 0.5 seit = 1700.01.01 }\n";

/// Fixture mit Militär: Nachbarschaft der sechs Orte als Kette Berlin – Potsdam – Zerbst – Dessau –
/// Leipzig – Dresden, Überfahrt Berlin–Dresden, Brandenburg mit zwei Garnisonen.
class MilitaryData {
public:
    MilitaryData() : dir_(true) {
        testhilfen::write_file(dir_ / "karte/nachbarn.csv", kNeighbors);
        testhilfen::write_file(
            dir_ / "karte/ueberfahrten.csv",
            "ort_a;ort_b;tage;name;quelle\nberlin_coelln;dresden;3;\"Fähre; zum Test\";Testdaten\n");
        testhilfen::write_file(dir_ / "common/militaer.chron", kMilitary);
    }

    World load() const {
        uhrwerk::LoadResult result = uhrwerk::load_world(dir_.path());
        INFO(testhilfen::issues_text(result));
        REQUIRE(result.ok());
        CHECK(result.issues.empty());
        return std::move(*result.world);
    }

    const testhilfen::TempDir& dir() const { return dir_; }

private:
    testhilfen::TempDir dir_;
};

std::size_t at(Index index) {
    return static_cast<std::size_t>(index);
}

} // namespace

TEST_CASE("Militär: nur ab 'seit'") {
    SUBCASE("ohne common/militaer.chron") {
        const uhrwerk::LoadResult result = uhrwerk::load_world(testhilfen::fixture_dir());
        REQUIRE(result.ok());
        const World& w = *result.world;
        CHECK_FALSE(uhrwerk::military_active(w));
        CHECK(w.neighbors.empty());
        CHECK(w.garrisons.empty());
        Simulation sim(w);
        sim.step_days(40);
        CHECK(sim.world().armies.empty());
    }
    SUBCASE("'seit' nach dem Startdatum") {
        const MilitaryData data;
        testhilfen::replace_in_file(data.dir() / "common/militaer.chron", "seit = 1648.01.01",
                                    "seit = 1848.03.01");
        const World w = data.load();
        CHECK_FALSE(uhrwerk::military_active(w));
        CHECK(w.neighbors.empty());
        CHECK(Simulation(w).world().armies.empty());
        // Ohne Militär zählt der Bereich nicht: der Fingerabdruck gleicht dem der Fixture.
        const uhrwerk::LoadResult fixture = uhrwerk::load_world(testhilfen::fixture_dir());
        REQUIRE(fixture.ok());
        CHECK(uhrwerk::content_fingerprint(w).parts.back() ==
              uhrwerk::content_fingerprint(*fixture.world).parts.back());
    }
}

TEST_CASE("Militär: Nachbarschaft, Heerwesen und Garnisonen") {
    const MilitaryData data;
    const World w = data.load();
    REQUIRE(uhrwerk::military_active(w));
    REQUIRE(w.neighbors.size() == w.locations.size());
    const Index berlin = w.find_location("berlin_coelln");
    const Index potsdam = w.find_location("potsdam");
    const Index zerbst = w.find_location("zerbst");
    const Index dessau = w.find_location("dessau");
    const Index leipzig = w.find_location("leipzig");
    const Index dresden = w.find_location("dresden");

    // Symmetrisch, nach Nachbarort sortiert, Abstand aus den Lagen.
    CHECK(w.neighbors[at(berlin)].size() == 2); // Potsdam, Überfahrt nach Dresden
    CHECK(w.neighbors[at(dessau)].size() == 2);
    for (const std::vector<uhrwerk::Neighbor>& list : w.neighbors) {
        CHECK(std::is_sorted(list.begin(), list.end(),
                             [](const auto& a, const auto& b) { return a.location < b.location; }));
    }
    const uhrwerk::Neighbor* bp = uhrwerk::find_neighbor(w, berlin, potsdam);
    const uhrwerk::Neighbor* pb = uhrwerk::find_neighbor(w, potsdam, berlin);
    REQUIRE(bp != nullptr);
    REQUIRE(pb != nullptr);
    CHECK(bp->distance_km == pb->distance_km);
    CHECK(bp->distance_km > Fixed::from_int(20));
    CHECK(bp->distance_km < Fixed::from_int(35));
    CHECK(bp->border == 120);
    CHECK(uhrwerk::find_neighbor(w, berlin, leipzig) == nullptr);
    // Flussübergang bis Rang 3: Potsdam–Zerbst (1) und Leipzig–Dresden (3), nicht Dessau–Leipzig (5).
    CHECK(uhrwerk::river_crossing(w, *uhrwerk::find_neighbor(w, potsdam, zerbst)));
    CHECK(uhrwerk::river_crossing(w, *uhrwerk::find_neighbor(w, leipzig, dresden)));
    CHECK_FALSE(uhrwerk::river_crossing(w, *uhrwerk::find_neighbor(w, dessau, leipzig)));
    const uhrwerk::Neighbor* ferry = uhrwerk::find_neighbor(w, dresden, berlin);
    REQUIRE(ferry != nullptr);
    CHECK(ferry->ferry_days == 3);
    CHECK(ferry->border == 0);

    // Heerwesen: Brandenburg eigens, die anderen nach dem Standard.
    const Index bra = w.find_state("BRA");
    const Index sax = w.find_state("SAX");
    const Index anh = w.find_state("ANH");
    CHECK(w.states[at(bra)].army.firepower == Fixed::from_ratio(15, 10));
    CHECK(w.states[at(bra)].army.mobilization_days == 20);
    CHECK(w.states[at(sax)].army.firepower == Fixed::one());
    CHECK(w.states[at(sax)].army.guns_per_mille == Fixed::from_int(2));

    // Garnisonen: Brandenburg zwei (die spätere zählt 1648 nicht), Sachsen und Anhalt in der Hauptstadt.
    REQUIRE(w.garrisons.size() == 4);
    CHECK(w.garrisons[0].id == "bra_berlin");
    CHECK(w.garrisons[0].name == "Garde");
    CHECK(w.garrisons[1].name == "Garnison Potsdam");
    CHECK(w.garrisons[2].state == sax);
    CHECK(w.garrisons[2].location == dresden);
    CHECK(w.garrisons[3].state == anh);
    CHECK(w.garrisons[3].share == Fixed::one());

    // Friedensheere beim Start: Anteil der Friedensstärke (heer × Bevölkerung / 1.000).
    Simulation sim(w);
    const World& v = sim.world();
    REQUIRE(v.armies.size() == 4);
    CHECK(v.next_army_id == 5);
    const Fixed peace = uhrwerk::peace_strength(v, bra);
    CHECK(peace > Fixed::zero());
    CHECK(v.armies[0].id == 1);
    CHECK(v.armies[0].location == berlin);
    CHECK(v.armies[0].soldiers == peace * Fixed::from_ratio(6, 10) / Fixed::one());
    CHECK(v.armies[0].guns == v.armies[0].soldiers * Fixed::from_int(4) / Fixed::from_int(1000));
    CHECK(v.armies[0].morale == Fixed::one());
    CHECK(v.armies[0].status == uhrwerk::ArmyStatus::Garrison);
    CHECK(uhrwerk::state_army_count(v, bra) == 2);
    CHECK(difference(uhrwerk::state_soldiers(v, sax), uhrwerk::peace_strength(v, sax)) <
          Fixed::from_ratio(1, 100));

    // Am Monatsersten folgt das Friedensheer der Bevölkerung.
    sim.step_days(70);
    const Fixed later = uhrwerk::peace_strength(v, bra);
    CHECK(later != peace);
    CHECK(difference(uhrwerk::state_soldiers(v, bra), later) < Fixed::from_int(1));
}

TEST_CASE("Militär: Spielstand, Wiederholung und Fingerabdruck") {
    const MilitaryData data;
    const auto run = [&](unsigned threads, std::int64_t days) {
        SimConfig config;
        config.threads = threads;
        Simulation sim(data.load(), config);
        sim.step_days(days);
        return sim;
    };
    const Simulation one = run(1, 45);
    REQUIRE(one.world().armies.size() == 4);
    const Bytes bytes = uhrwerk::save_game(one, SaveMeta{});
    uhrwerk::LoadedGame loaded = uhrwerk::load_game(bytes, data.load(), 2);
    INFO(loaded.error);
    REQUIRE(loaded.ok());
    CHECK(loaded.simulation->checksum() == one.checksum());
    CHECK(loaded.simulation->world().armies == one.world().armies);
    CHECK(loaded.simulation->world().next_army_id == one.world().next_army_id);
    Simulation continued = one;
    continued.step_days(60);
    loaded.simulation->step_days(60);
    CHECK(loaded.simulation->checksum() == continued.checksum());
    CHECK(run(4, 105).checksum() == continued.checksum());

    // Eine andere Nachbarschaft ist anderer Inhalt: Der Spielstand passt nicht mehr.
    testhilfen::replace_in_file(data.dir() / "karte/nachbarn.csv", "leipzig;dresden;90;3",
                                "leipzig;dresden;90;1");
    const World changed = data.load();
    CHECK(uhrwerk::content_fingerprint(changed).parts.back() !=
          uhrwerk::content_fingerprint(one.world()).parts.back());
    const uhrwerk::LoadedGame refused = uhrwerk::load_game(bytes, data.load(), 1);
    CHECK_FALSE(refused.ok());
    CHECK(refused.error.find("militaer") != std::string::npos);
}

TEST_CASE("Militär: Ladefehler") {
    SUBCASE("Nachbarschaft") {
        const MilitaryData data;
        testhilfen::write_file(data.dir() / "karte/nachbarn.csv",
                               std::string(kNeighbors) +
                                   "potsdam;berlin_coelln;5;0\nnirgendwo;dresden;5;0\ndessau;dessau;5;0\n"
                                   "zerbst;leipzig;x;0\nzerbst;leipzig;5;9\nzerbst;leipzig\n");
        const uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path());
        INFO(testhilfen::issues_text(result));
        CHECK_FALSE(result.ok());
        CHECK(testhilfen::has_issue(result, "Nachbarschaft potsdam – berlin_coelln ist mehrfach vorhanden"));
        CHECK(testhilfen::has_issue(result, "unbekannter Ort 'nirgendwo'"));
        CHECK(testhilfen::has_issue(result, "Ort dessau ist sein eigener Nachbar"));
        CHECK(testhilfen::has_issue(result, "ungültiger Wert 'x' in Spalte 'grenze'"));
        CHECK(testhilfen::has_issue(result, "ungültiger Wert '9' in Spalte 'fluss'"));
        CHECK(testhilfen::has_issue(result, "2 Felder, erwartet 4"));
    }
    SUBCASE("Kopfzeile und fehlende Datei") {
        const MilitaryData data;
        testhilfen::write_file(data.dir() / "karte/nachbarn.csv", "a;b;c;d\n");
        uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path());
        CHECK(testhilfen::has_issue(result, "Kopfzeile 'a;b;c;d'"));
        std::filesystem::remove(data.dir() / "karte/nachbarn.csv");
        result = uhrwerk::load_world(data.dir().path());
        CHECK_FALSE(result.ok());
        CHECK(testhilfen::has_issue(result, "Datei fehlt (erzeugt von tools/kartengenerator)"));
    }
    SUBCASE("Ort mit Daten ohne Nachbarn") {
        const MilitaryData data;
        testhilfen::write_file(data.dir() / "karte/ueberfahrten.csv", "ort_a;ort_b;tage;name;quelle\n");
        testhilfen::replace_in_file(data.dir() / "karte/nachbarn.csv", "leipzig;dresden;90;3\n", "");
        const uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path());
        CHECK(result.ok());
        CHECK(testhilfen::has_issue(result, "Ort dresden hat Daten, aber keine Nachbarn", false));
    }
    SUBCASE("Garnisonen und Heerwesen") {
        const MilitaryData data;
        testhilfen::write_file(data.dir() / "common/militaer.chron",
                               std::string(kMilitary) +
                                   "garnison x1 { staat = XYZ ort = dresden anteil = 0.5 }\n"
                                   "garnison x2 { staat = SAX ort = nirgendwo anteil = 0.5 }\n"
                                   "garnison x3 { staat = SAX ort = berlin_coelln anteil = 0.5 }\n"
                                   "garnison x3 { staat = SAX ort = dresden anteil = 0.5 }\n"
                                   "garnison x4 { staat = SAX ort = dresden anteil = 2 }\n"
                                   "heerwesen x5 { staat = XYZ }\n");
        const uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path());
        INFO(testhilfen::issues_text(result));
        CHECK_FALSE(result.ok());
        CHECK(testhilfen::has_issue(result, "unbekannter Staat 'XYZ' in garnison x1"));
        CHECK(testhilfen::has_issue(result, "unbekannter Ort 'nirgendwo' in garnison x2"));
        CHECK(testhilfen::has_issue(result, "garnison x3: Ort berlin_coelln gehört zum Startdatum nicht SAX",
                                    false));
        CHECK(testhilfen::has_issue(result, "garnison x3 ist mehrfach vorhanden"));
        CHECK(testhilfen::has_issue(result, "anteil"));
        CHECK(testhilfen::has_issue(result, "unbekannter Staat 'XYZ' in heerwesen"));
    }
    SUBCASE("Block 'militaer' fehlt") {
        const MilitaryData data;
        testhilfen::write_file(data.dir() / "common/militaer.chron",
                               "heerwesen standard { bewaffnung = 1 }\n");
        const uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path());
        CHECK_FALSE(result.ok());
        CHECK(testhilfen::has_issue(result, "Block 'militaer' fehlt"));
    }
}
