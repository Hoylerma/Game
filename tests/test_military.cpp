// Tests des Militärs (seit M10; docs/technik/militaer.md): Schritt a1 Nachbarschaft, Heerwesen und
// Garnisonen (Abschnitte 3.1 und 3.2), Schritt a2 Mobilmachung, Wege, Bahn und Versorgung (3.3–3.5).
#include "test_hilfen.hpp"
#include "uhrwerk/diplomacy.hpp"
#include "uhrwerk/military.hpp"
#include "uhrwerk/savegame.hpp"
#include "uhrwerk/simulation.hpp"

#include <algorithm>
#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

using uhrwerk::ArmyStatus;
using uhrwerk::Command;
using uhrwerk::CommandType;
using uhrwerk::Fixed;
using uhrwerk::Index;
using uhrwerk::LegMode;
using uhrwerk::MobilizationPhase;
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

    /// Eisenbahn Berlin–Potsdam, eröffnet 1640 (seit Schritt a2).
    void add_railway() const {
        testhilfen::write_file(
            dir_ / "history/eisenbahnen/test.chron",
            "strecke berlin_potsdam {\n    name = \"Berlin-Potsdamer Eisenbahn\"\n"
            "    quelle = \"Testdaten\"\n"
            "    abschnitt = { von = berlin_coelln bis = potsdam eroeffnet = 1640.01.01 }\n}\n");
    }

    /// Deutscher Bund wie in den Diplomatietests: alle Orte mit Kreis im Bundesgebiet, Grundbeziehung
    /// Brandenburg–Sachsen −60, Brandenburg–Anhalt 40.
    void add_federation() const {
        for (const char* file : {"history/orte/10_brandenburg.chron", "history/orte/20_sachsen.chron",
                                 "history/orte/30_anhalt.chron"}) {
            std::string text = testhilfen::read_file(dir_ / file);
            std::string::size_type pos = 0;
            while ((pos = text.find("\n    kreis", pos)) != std::string::npos) {
                text.insert(pos + 1, "    bund = ja\n");
                pos += 20;
            }
            testhilfen::write_file(dir_ / file, text);
        }
        std::string relations;
        for (const auto& [a, b, value] :
             {std::tuple{"BRA", "SAX", -60}, std::tuple{"BRA", "ANH", 40}, std::tuple{"SAX", "ANH", 0}}) {
            relations += "beziehung {\n    staaten = { " + std::string(a) + " " + std::string(b) +
                         " }\n    wert = " + std::to_string(value) + "\n    quelle = \"Testdaten\"\n}\n";
        }
        testhilfen::write_file(dir_ / "history/beziehungen/grundwerte.chron", relations);
    }

    /// Zusätzliche Regeln im Block 'militaer'.
    void add_rules(std::string_view rules) const {
        testhilfen::replace_in_file(dir_ / "common/militaer.chron", "    fluss_rang = 3\n",
                                    "    fluss_rang = 3\n" + std::string(rules));
    }

private:
    testhilfen::TempDir dir_;
};

std::size_t at(Index index) {
    return static_cast<std::size_t>(index);
}

Command command(CommandType type, Index state, std::int64_t value, Index target = uhrwerk::kNone) {
    Command c;
    c.type = type;
    c.state = state;
    c.value = Fixed::from_int(value);
    c.target = target;
    return c;
}

bool chronicle_contains(const World& w, std::string_view text) {
    return std::any_of(w.chronicle.begin(), w.chronicle.end(), [&](const uhrwerk::ChronicleEntry& e) {
        return e.text.find(text) != std::string::npos;
    });
}

/// Feldarmee ohne Garnison (Versorgungstests).
uhrwerk::Army field_army(std::int64_t id, Index state, Index location, Fixed soldiers, Fixed morale) {
    uhrwerk::Army a;
    a.id = id;
    a.state = state;
    a.location = location;
    a.name = "Heer " + std::to_string(id);
    a.soldiers = soldiers;
    a.morale = morale;
    a.status = ArmyStatus::Rest;
    return a;
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
    SUBCASE("Regeln der Bewegung") {
        const MilitaryData data;
        data.add_rules("    marsch_km = 0\n    ausladen_tage = 11\n"
                       "    gelaende = { sumpfland = 2 sandboden = 0.5 flussaue = 1.2 }\n");
        const uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path());
        INFO(testhilfen::issues_text(result));
        CHECK_FALSE(result.ok());
        CHECK(testhilfen::has_issue(result, "'marsch_km'"));
        CHECK(testhilfen::has_issue(result, "'ausladen_tage' muss zwischen 0 und 10 liegen"));
        // Unbekanntes Gelände und Faktor unter 1; flussaue = 1.2 ist gültig.
        CHECK(std::count_if(result.issues.begin(), result.issues.end(), [](const uhrwerk::LoadIssue& i) {
                  return i.message.find("ungültiger Eintrag in 'gelaende' von militaer") != std::string::npos;
              }) == 2);
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

// ─── Schritt a2: Wege, Mobilmachung, Versorgung ─────────────────────────────

TEST_CASE("Militär: Marschtage, Zugang und Wege über Land und Bahn") {
    const MilitaryData data;
    data.add_rules("    marsch_km = 10\n    gelaende = { sandboden = 1.5 }\n");
    const World plain = data.load();
    const Index berlin = plain.find_location("berlin_coelln");
    const Index potsdam = plain.find_location("potsdam");
    const Index zerbst = plain.find_location("zerbst");
    const Index dessau = plain.find_location("dessau");
    const Index leipzig = plain.find_location("leipzig");
    const Index dresden = plain.find_location("dresden");
    const Index bra = plain.find_state("BRA");
    const Index sax = plain.find_state("SAX");
    const Fixed ten = Fixed::from_int(10);

    // Marschtage: Abstand ÷ 10 km × Gelände des Zielorts (Potsdam Sandboden 1,5), Fluss +1, Überfahrt 3.
    const uhrwerk::Neighbor& bp = *uhrwerk::find_neighbor(plain, berlin, potsdam);
    const uhrwerk::Neighbor& pb = *uhrwerk::find_neighbor(plain, potsdam, berlin);
    const uhrwerk::Neighbor& pz = *uhrwerk::find_neighbor(plain, potsdam, zerbst);
    CHECK(uhrwerk::march_days(plain, bp) == bp.distance_km / ten * Fixed::from_ratio(15, 10));
    CHECK(uhrwerk::march_days(plain, pb) == pb.distance_km / ten);
    CHECK(uhrwerk::march_days(plain, pz) == pz.distance_km / ten + Fixed::one());
    CHECK(uhrwerk::march_days(plain, *uhrwerk::find_neighbor(plain, berlin, dresden)) == Fixed::from_int(3));

    // Zugang: eigene Orte; Orte ohne Daten und fremde gesperrt.
    CHECK(uhrwerk::accessible(plain, bra, potsdam));
    CHECK_FALSE(uhrwerk::accessible(plain, bra, zerbst));
    CHECK_FALSE(uhrwerk::accessible(plain, bra, dessau));
    CHECK_FALSE(uhrwerk::accessible(plain, bra, dresden));

    // Einladen nach der Kapazität je Strecke und Tag.
    World small = plain;
    small.military_rules.rail_capacity = Fixed::from_int(1000);
    CHECK(uhrwerk::loading_days(small, Fixed::zero()) == 1);
    CHECK(uhrwerk::loading_days(small, Fixed::from_int(1000)) == 1);
    CHECK(uhrwerk::loading_days(small, Fixed::from_int(1001)) == 2);
    CHECK(uhrwerk::loading_days(small, Fixed::from_int(2500)) == 3);

    SUBCASE("zu Fuß") {
        const World v = Simulation(plain).world();
        const uhrwerk::Army& garde = *uhrwerk::find_army(v, 1);
        CHECK_FALSE(uhrwerk::is_station(v, berlin));
        const auto route = uhrwerk::find_route(v, garde, potsdam);
        REQUIRE(route.has_value());
        REQUIRE(route->steps.size() == 1);
        CHECK(route->steps[0] == uhrwerk::RouteStep{potsdam, LegMode::March, uhrwerk::kNone});
        CHECK(route->days == uhrwerk::march_days(v, bp));
        CHECK_FALSE(uhrwerk::find_route(v, garde, berlin).has_value());  // steht schon dort
        CHECK_FALSE(uhrwerk::find_route(v, garde, dresden).has_value()); // Sachsen ist fremd
        CHECK_FALSE(uhrwerk::find_route(v, garde, zerbst).has_value());

        // Mit einem Bündnis stehen Sachsens Orte offen: über die Überfahrt nach Dresden, dann über die Elbe.
        World allied = v;
        allied.treaties.push_back(uhrwerk::Treaty{uhrwerk::TreatyKind::Alliance, bra, sax, allied.date});
        CHECK(uhrwerk::accessible(allied, bra, dresden));
        const auto far = uhrwerk::find_route(allied, garde, leipzig);
        REQUIRE(far.has_value());
        REQUIRE(far->steps.size() == 2);
        CHECK(far->steps[0].location == dresden);
        CHECK(far->steps[1].location == leipzig);
        CHECK(far->days == Fixed::from_int(3) + uhrwerk::march_days(allied, *uhrwerk::find_neighbor(
                                                                                allied, dresden, leipzig)));
    }
    SUBCASE("mit der Bahn") {
        data.add_railway();
        const World v = Simulation(data.load()).world();
        const uhrwerk::Army& garde = *uhrwerk::find_army(v, 1);
        CHECK(uhrwerk::is_station(v, berlin));
        CHECK(uhrwerk::is_station(v, potsdam));
        CHECK_FALSE(uhrwerk::is_station(v, dessau));
        REQUIRE(v.rail_backlog.size() == 1);
        // Einladen 1 Tag, Fahrt 300 km je Tag, Ausladen 1 Tag: schneller als 10 km je Tag zu Fuß.
        const auto route = uhrwerk::find_route(v, garde, potsdam);
        REQUIRE(route.has_value());
        REQUIRE(route->steps.size() == 1);
        CHECK(route->steps[0] == uhrwerk::RouteStep{potsdam, LegMode::Rail, 0});
        CHECK(route->days == Fixed::from_int(2) + bp.distance_km / Fixed::from_int(300));
        CHECK(route->days < uhrwerk::march_days(v, bp));
    }
}

TEST_CASE("Militär: Mobilmachung, Marsch, Bahnfahrt und Abrüstung") {
    const MilitaryData data;
    data.add_railway();
    data.add_rules("    marsch_km = 10\n");
    const World start = data.load();
    const Index berlin = start.find_location("berlin_coelln");
    const Index potsdam = start.find_location("potsdam");
    const Index zerbst = start.find_location("zerbst");
    const Index bra = start.find_state("BRA");
    Simulation sim(start);
    Simulation peaceful(start);
    const World& v = sim.world();
    REQUIRE(v.mobilizations.size() == v.states.size());
    const auto army = [&](std::int64_t id) -> const uhrwerk::Army& { return *uhrwerk::find_army(v, id); };

    // Friedensheere marschieren nicht.
    sim.submit(command(CommandType::March, bra, 1, potsdam));
    sim.step_day();
    peaceful.step_day();
    CHECK(army(1).location == berlin);
    CHECK(army(1).status == ArmyStatus::Garrison);

    // Mobilmachung: Die Garnisonen werden Feldarmeen und wachsen in 20 Tagen auf das 2,5-Fache.
    const Fixed garde = army(1).soldiers;
    sim.submit(command(CommandType::Mobilization, bra, 1));
    sim.submit(command(CommandType::Mobilization, bra, 1)); // schon mobil: ohne Wirkung
    sim.submit(command(CommandType::Mobilization, bra, 2)); // ungültig
    sim.step_day();
    peaceful.step_day();
    CHECK(uhrwerk::mobilization_phase(v, bra) == MobilizationPhase::Mobilizing);
    CHECK(army(1).status == ArmyStatus::Rest);
    CHECK(army(2).status == ArmyStatus::Rest);
    CHECK(army(3).status == ArmyStatus::Garrison); // Sachsen
    CHECK(chronicle_contains(v, "Brandenburg macht mobil."));
    sim.step_days(19);
    peaceful.step_days(19);
    CHECK(uhrwerk::mobilization_phase(v, bra) == MobilizationPhase::Mobilized);
    CHECK(chronicle_contains(v, "Brandenburg ist mobil:"));
    CHECK(army(1).soldiers > garde * Fixed::from_ratio(24, 10));
    CHECK(army(1).soldiers < garde * Fixed::from_ratio(25, 10));
    CHECK(army(1).guns == army(1).soldiers * Fixed::from_int(4) / Fixed::from_int(1000));

    // Die Heereskosten folgen den Soldaten (Rechentag Brandenburgs am Monatsersten).
    sim.step_days(20);
    peaceful.step_days(20);
    const Fixed cost = v.states[at(bra)].budget.army;
    const Fixed base = peaceful.world().states[at(bra)].budget.army;
    CHECK(cost > base * Fixed::from_ratio(24, 10));
    CHECK(cost < base * Fixed::from_ratio(25, 10));
    CHECK(uhrwerk::army_cost_factor(v, bra) > Fixed::from_ratio(24, 10));

    // Bahnfahrt: Die Garde fährt nach Potsdam, das Heer aus Potsdam am selben Tag nach Berlin und wartet
    // einen Tag auf die belegte Strecke.
    sim.submit(command(CommandType::March, bra, 1, potsdam));
    sim.submit(command(CommandType::March, bra, 2, berlin));
    sim.submit(command(CommandType::March, bra, 1, zerbst)); // gesperrt: ohne Wirkung
    sim.step_day();
    CHECK(army(1).status == ArmyStatus::Rail);
    CHECK(army(1).location == berlin);
    CHECK(army(2).status == ArmyStatus::Rail);
    CHECK(army(2).days_left > army(1).days_left + Fixed::from_ratio(99, 100));
    CHECK(v.rail_backlog[0] == 1);
    sim.step_days(2);
    CHECK(army(1).location == potsdam);
    CHECK(army(1).status == ArmyStatus::Rest);
    CHECK(army(1).route.empty());
    CHECK(army(2).status == ArmyStatus::Rail);
    sim.step_day();
    CHECK(army(2).location == berlin);
    CHECK(army(2).status == ArmyStatus::Rest);
    CHECK(v.rail_backlog[0] == 0);

    // Halt auf dem Marsch: ohne Bahn zu Fuß (Kapazität so klein, dass die Bahn langsamer ist).
    World& w = const_cast<World&>(v);
    w.military_rules.rail_capacity = Fixed::from_int(1);
    sim.submit(command(CommandType::March, bra, 1, berlin));
    sim.step_day();
    CHECK(army(1).status == ArmyStatus::March);
    REQUIRE(army(1).route.size() == 1);
    CHECK(army(1).route[0].mode == LegMode::March);
    sim.submit(command(CommandType::March, bra, 1, uhrwerk::kNone));
    sim.step_day();
    CHECK(army(1).status == ArmyStatus::Rest);
    CHECK(army(1).location == potsdam);
    CHECK(army(1).days_left == Fixed::zero());

    // Abrüstung über 30 Tage; danach stehen die Heere wieder in ihren Garnisonen.
    sim.submit(command(CommandType::Mobilization, bra, 0));
    sim.step_day();
    CHECK(uhrwerk::mobilization_phase(v, bra) == MobilizationPhase::Demobilizing);
    CHECK(chronicle_contains(v, "Brandenburg rüstet ab."));
    sim.step_days(29);
    CHECK(uhrwerk::mobilization_phase(v, bra) == MobilizationPhase::Peace);
    CHECK(army(1).status == ArmyStatus::Garrison);
    CHECK(army(1).location == berlin);
    CHECK(army(2).location == potsdam);
    CHECK(difference(uhrwerk::state_soldiers(v, bra), uhrwerk::peace_strength(v, bra)) < Fixed::one());
    CHECK(chronicle_contains(v, "Brandenburg hat abgerüstet"));
}

TEST_CASE("Militär: Versorgung und Zermürbung") {
    const MilitaryData data;
    data.add_railway();
    World w = data.load();
    const Index berlin = w.find_location("berlin_coelln");
    const Index dessau = w.find_location("dessau");
    const Index bra = w.find_state("BRA");
    const Fixed strength = Fixed::from_int(10000);
    w.armies.push_back(field_army(1, bra, dessau, strength, Fixed::one()));
    w.armies.push_back(field_army(2, bra, berlin, strength, Fixed::from_ratio(1, 2)));
    w.next_army_id = 3;

    // In Anhalt mehr als drei Tagesmärsche vom nächsten Bahnhof (Potsdam, über Zerbst und einen Fluss).
    CHECK_FALSE(uhrwerk::supplied(w, w.armies[0]));
    CHECK(uhrwerk::supplied(w, w.armies[1]));
    // Zerbst hat keine Daten und sperrt; der Weg über Leipzig, Dresden und die Fähre nach Berlin ist
    // knapp 12 Tagesmärsche lang.
    World wide = w;
    wide.military_rules.supply_days = Fixed::from_int(10);
    CHECK_FALSE(uhrwerk::supplied(wide, w.armies[0]));
    wide.military_rules.supply_days = Fixed::from_int(15);
    CHECK(uhrwerk::supplied(wide, w.armies[0]));

    SUBCASE("Herbst") {
        Simulation sim(w);
        sim.step_day();
        const uhrwerk::Army& out = sim.world().armies[0];
        const uhrwerk::Army& home = sim.world().armies[1];
        CHECK(out.soldiers == strength - strength * Fixed::from_ratio(5, 1000));
        CHECK(out.morale == Fixed::one() - Fixed::from_ratio(2, 100));
        CHECK(out.guns == out.soldiers * Fixed::from_int(4) / Fixed::from_int(1000));
        CHECK(home.soldiers == strength - strength * Fixed::from_ratio(5, 10000));
        CHECK(home.morale == Fixed::from_ratio(1, 2) + Fixed::from_ratio(1, 100));
    }
    SUBCASE("Winter: doppelt") {
        w.date = uhrwerk::Date{1649, 1, 10};
        Simulation sim(w);
        sim.step_day();
        const uhrwerk::Army& out = sim.world().armies[0];
        CHECK(out.soldiers == strength - strength * (Fixed::from_ratio(5, 1000) * Fixed::from_int(2)));
        CHECK(out.morale == Fixed::one() - Fixed::from_ratio(2, 100) * Fixed::from_int(2));
    }
}

TEST_CASE("Militär: Mobilmachung einer Krise") {
    const MilitaryData data;
    World w = Simulation(data.load()).world();
    const Index bra = w.find_state("BRA");
    const Index sax = w.find_state("SAX");
    const Index anh = w.find_state("ANH");

    CHECK(uhrwerk::order_mobilization(w, bra, true, true));
    CHECK(w.mobilizations[at(bra)].crisis);
    uhrwerk::end_crisis_mobilization(w, bra);
    CHECK(uhrwerk::mobilization_phase(w, bra) == MobilizationPhase::Demobilizing);

    // Übernimmt die Regierung die Mobilmachung, bleibt sie nach der Krise bestehen.
    CHECK(uhrwerk::order_mobilization(w, sax, true, true));
    CHECK(uhrwerk::order_mobilization(w, sax, true));
    CHECK_FALSE(w.mobilizations[at(sax)].crisis);
    CHECK_FALSE(uhrwerk::order_mobilization(w, sax, true));
    uhrwerk::end_crisis_mobilization(w, sax);
    CHECK(uhrwerk::mobilization_phase(w, sax) == MobilizationPhase::Mobilizing);

    CHECK_FALSE(uhrwerk::order_mobilization(w, anh, false)); // nicht mobil
    CHECK(uhrwerk::army_cost_factor(w, anh) == Fixed::one());
    CHECK_FALSE(uhrwerk::order_mobilization(w, uhrwerk::kNone, true));
}

TEST_CASE("Militär: Spielstand während Mobilmachung, Marsch und Bahnfahrt") {
    const MilitaryData data;
    data.add_railway();
    data.add_rules("    marsch_km = 10\n");
    const Index bra = 0;
    const auto feed = [&](Simulation& sim, std::int64_t until) {
        while (sim.days_elapsed() < until) {
            const std::int64_t day = sim.days_elapsed();
            if (day == 0) {
                sim.submit(command(CommandType::Mobilization, bra, 1));
            } else if (day == 21) {
                sim.submit(command(CommandType::March, bra, 1, 1)); // Garde nach Potsdam (Bahn)
                sim.submit(command(CommandType::March, bra, 2, 0)); // Potsdam nach Berlin, wartet
            }
            sim.step_day();
        }
    };
    const World start = data.load();
    REQUIRE(start.find_state("BRA") == bra);
    REQUIRE(start.find_location("berlin_coelln") == 0);
    REQUIRE(start.find_location("potsdam") == 1);
    for (const std::int64_t save_day : {10, 22, 23}) {
        CAPTURE(save_day);
        Simulation sim(start);
        feed(sim, save_day);
        const Bytes bytes = uhrwerk::save_game(sim, SaveMeta{});
        uhrwerk::LoadedGame loaded = uhrwerk::load_game(bytes, data.load(), 1);
        INFO(loaded.error);
        REQUIRE(loaded.ok());
        CHECK(loaded.simulation->world().armies == sim.world().armies);
        CHECK(loaded.simulation->world().mobilizations == sim.world().mobilizations);
        CHECK(loaded.simulation->world().rail_backlog == sim.world().rail_backlog);
        feed(sim, 60);
        feed(*loaded.simulation, 60);
        CHECK(loaded.simulation->checksum() == sim.checksum());
    }
    // Wiederholung aus dem Protokoll.
    Simulation sim(start);
    feed(sim, 60);
    const uhrwerk::ReplayResult result = uhrwerk::run_replay(uhrwerk::make_replay(sim), data.load(), 1);
    INFO(result.error);
    CHECK(result.ok);
    CHECK(result.mismatch_day == -1);
}

TEST_CASE("Militär: Krise auf der Stufe Mobilmachung") {
    // Mit Militär macht eine Krise die Hauptparteien wirklich mobil; die Kosten folgen den Soldaten statt
    // dem Faktor der Diplomatie, und mit dem Ende der Krise rüsten beide ab (militaer.md 3.3).
    const MilitaryData data;
    data.add_federation();
    World w = data.load();
    REQUIRE(uhrwerk::diplomacy_active(w));
    const Index bra = w.find_state("BRA");
    const Index sax = w.find_state("SAX");
    const Index anh = w.find_state("ANH");
    w.states[at(bra)].army_per_mille = Fixed::from_int(10);
    w.states[at(sax)].army_per_mille = Fixed::zero();
    w.states[at(anh)].army_per_mille = Fixed::zero();
    Simulation sim(w);
    const World& v = sim.world();
    sim.submit(
        command(CommandType::Demand, bra, static_cast<std::int64_t>(uhrwerk::DemandKind::Leadership), sax));
    sim.step_days(60);
    REQUIRE(v.plays.size() == 1);
    CHECK(v.plays[0].stage == 2);
    sim.step_day();
    REQUIRE(v.plays.size() == 1);
    CHECK(v.plays[0].stage == 3);
    CHECK(uhrwerk::mobilization_phase(v, bra) == MobilizationPhase::Mobilizing);
    CHECK(uhrwerk::mobilization_phase(v, sax) == MobilizationPhase::Mobilizing);
    CHECK(uhrwerk::mobilization_phase(v, anh) == MobilizationPhase::Peace);
    CHECK(v.mobilizations[at(bra)].crisis);
    sim.step_days(29); // Brandenburg ist nach 20 Tagen mobil, sein Rechentag liegt dazwischen
    CHECK(uhrwerk::mobilization_phase(v, bra) == MobilizationPhase::Mobilized);
    CHECK(uhrwerk::army_cost_factor(v, bra) > Fixed::from_int(2));
    sim.step_day();
    CHECK(v.plays.empty());
    CHECK(uhrwerk::mobilization_phase(v, bra) == MobilizationPhase::Demobilizing);
    CHECK(uhrwerk::mobilization_phase(v, sax) == MobilizationPhase::Demobilizing);
    sim.step_days(30);
    CHECK(uhrwerk::mobilization_phase(v, bra) == MobilizationPhase::Peace);
    CHECK(std::all_of(v.armies.begin(), v.armies.end(),
                      [](const uhrwerk::Army& a) { return a.status == ArmyStatus::Garrison; }));
}
