// Tests des Militärs (seit M10; docs/technik/militaer.md): Schritt a1 Nachbarschaft, Heerwesen und
// Garnisonen (Abschnitte 3.1 und 3.2), Schritt a2 Mobilmachung, Wege, Bahn und Versorgung (3.3–3.5),
// Schritt a3 Krieg, Schlacht, Besetzung, Belagerung und Verluste (4.1–4.4), Schritt a4 Kriegsziele,
// Kriegspunkte, Frieden und Generalstab (4.5, 5), Schritt a5 Kriegslabor (6.3).
#include "test_hilfen.hpp"
#include "uhrwerk/diplomacy.hpp"
#include "uhrwerk/economy.hpp"
#include "uhrwerk/military.hpp"
#include "uhrwerk/savegame.hpp"
#include "uhrwerk/simulation.hpp"
#include "uhrwerk/war_lab.hpp"

#include <algorithm>
#include <cstdint>
#include <doctest/doctest.h>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
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

    /// Bundesversammlung wie in den Diplomatietests (Plenum BRA 4, SAX 4, ANH 1, je eine Stimme im Rat).
    void add_diet() const {
        testhilfen::write_file(
            dir_ / "common/bundesversammlung.chron",
            "bundesversammlung bund {\n    name = \"Bundesversammlung\"\n    praesidium = SAX\n"
            "    plenum = { BRA = 4 SAX = 4 ANH = 1 }\n"
            "    rat = { stimme = { staaten = { BRA } } stimme = { staaten = { SAX } } "
            "stimme = { staaten = { ANH } } }\n    quelle = \"Testdaten\"\n}\n");
    }

    /// Festung in Dresden (seit Schritt a3).
    void add_fortress(int level) const {
        testhilfen::write_file(dir_ / "common/militaer.chron",
                               testhilfen::read_file(dir_ / "common/militaer.chron") +
                                   "festung dresden { ort = dresden stufe = " + std::to_string(level) +
                                   " }\n");
    }

    /// Verwaltungseinheit „Leipziger Kreis“ mit Leipzig (seit Schritt a4, Gebietsziel).
    void add_unit() const {
        testhilfen::write_file(dir_ / "history/verwaltung/test.chron",
                               "einheit leipziger_kreis {\n    name = \"Leipziger Kreis\"\n    art = kreis\n"
                               "    quelle = \"Testdaten\"\n}\n");
        testhilfen::replace_in_file(dir_ / "history/orte/20_sachsen.chron", "ort leipzig {\n",
                                    "ort leipzig {\n    verwaltung = leipziger_kreis\n");
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

std::string lines(const std::vector<std::string>& texts) {
    std::string result;
    for (const std::string& text : texts) {
        result += text + '\n';
    }
    return result;
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
    SUBCASE("Festungen") {
        const MilitaryData data;
        testhilfen::write_file(data.dir() / "common/militaer.chron",
                               std::string(kMilitary) + "festung f1 { ort = nirgendwo stufe = 1 }\n"
                                                        "festung f2 { ort = dresden stufe = 4 }\n"
                                                        "festung f3 { ort = dresden stufe = 2 }\n"
                                                        "festung f3 { ort = leipzig stufe = 2 }\n"
                                                        "festung f4 { ort = dresden stufe = 1 }\n");
        const uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path());
        INFO(testhilfen::issues_text(result));
        CHECK_FALSE(result.ok());
        CHECK(testhilfen::has_issue(result, "unbekannter Ort 'nirgendwo' in festung f1"));
        CHECK(testhilfen::has_issue(result, "'stufe' muss zwischen 1 und 3 liegen"));
        CHECK(testhilfen::has_issue(result, "festung f3 ist mehrfach vorhanden"));
        CHECK(testhilfen::has_issue(result, "festung f4: Ort dresden hat schon eine Festung"));
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

// ─── Schritt a3: Krieg, Schlacht, Besetzung, Belagerung ─────────────────────

namespace {

/// Welt mit gesetzten Heeren je 1.000 Einwohner.
World with_strength(World w, std::int64_t bra, std::int64_t sax) {
    w.states[at(w.find_state("BRA"))].army_per_mille = Fixed::from_int(bra);
    w.states[at(w.find_state("SAX"))].army_per_mille = Fixed::from_int(sax);
    w.states[at(w.find_state("ANH"))].army_per_mille = Fixed::zero();
    return w;
}

/// Kriegserklärung Brandenburgs an Sachsen am ersten Tag, die Garde marschiert am zweiten nach Dresden
/// (Überfahrt, drei Tage: Ankunft und Schlacht am vierten Tag, days_elapsed 3).
void brandenburg_attacks(Simulation& sim, std::int64_t days, std::int64_t goal = 0) {
    const World& v = sim.world();
    const Index bra = v.find_state("BRA");
    while (sim.days_elapsed() < days) {
        if (sim.days_elapsed() == 0) {
            sim.submit(command(CommandType::DeclareWar, bra, goal, v.find_state("SAX")));
        } else if (sim.days_elapsed() == 1) {
            sim.submit(command(CommandType::March, bra, 1, v.find_location("dresden")));
        }
        sim.step_day();
    }
}

} // namespace

TEST_CASE("Militär: Kriegserklärung und Bündnisfall") {
    const MilitaryData data;
    const World start = Simulation(data.load()).world();
    const Index bra = start.find_state("BRA");
    const Index sax = start.find_state("SAX");
    const Index anh = start.find_state("ANH");
    const auto alliance = [&](World& w, Index a, Index b) {
        w.treaties.push_back(uhrwerk::Treaty{uhrwerk::TreatyKind::Alliance, a, b, w.date});
    };
    std::vector<Index> attackers;
    std::vector<Index> defenders;

    SUBCASE("Verbündete und Garanten") {
        World w = start;
        alliance(w, anh, sax);
        uhrwerk::war_sides(w, bra, sax, attackers, defenders);
        CHECK(attackers == std::vector<Index>{bra});
        CHECK(defenders == std::vector<Index>{sax, anh});
        alliance(w, anh, bra); // mit beiden verbündet: neutral
        uhrwerk::war_sides(w, bra, sax, attackers, defenders);
        CHECK(defenders == std::vector<Index>{sax});
        World g = start;
        g.treaties.push_back(uhrwerk::Treaty{uhrwerk::TreatyKind::Guarantee, anh, sax, g.date});
        uhrwerk::war_sides(g, bra, sax, attackers, defenders);
        CHECK(defenders == std::vector<Index>{sax, anh});
        World a = start;
        alliance(a, bra, anh);
        uhrwerk::war_sides(a, bra, sax, attackers, defenders);
        CHECK(attackers == std::vector<Index>{bra, anh});
    }
    SUBCASE("Erklärung") {
        World w = start;
        alliance(w, anh, bra);
        alliance(w, bra, sax); // endet mit dem Krieg
        CHECK_FALSE(uhrwerk::can_declare_war(w, bra, bra));
        CHECK_FALSE(uhrwerk::can_declare_war(w, bra, uhrwerk::kNone));
        REQUIRE(uhrwerk::declare_war(w, bra, sax));
        REQUIRE(w.wars.size() == 1);
        CHECK(w.wars[0].id == 1);
        CHECK(w.next_war_id == 2);
        CHECK(w.wars[0].attackers == std::vector<Index>{bra, anh});
        CHECK(w.wars[0].defenders == std::vector<Index>{sax});
        CHECK(w.wars[0].cause == uhrwerk::WarCause::Declaration);
        CHECK(chronicle_contains(
            w, "Brandenburg erklärt Sachsen den Krieg. An der Seite von Brandenburg: Anhalt."));
        CHECK_FALSE(uhrwerk::allied(w, bra, sax));
        CHECK(uhrwerk::allied(w, bra, anh));
        CHECK(uhrwerk::at_war(w, bra, sax));
        CHECK(uhrwerk::at_war(w, sax, anh));
        CHECK_FALSE(uhrwerk::at_war(w, bra, anh));
        CHECK(uhrwerk::friendly(w, anh, bra));
        CHECK(uhrwerk::in_war(w, sax));
        // Alle machen mobil; im Krieg rüstet niemand ab.
        for (const Index s : {bra, sax, anh}) {
            CHECK(uhrwerk::mobilization_phase(w, s) == MobilizationPhase::Mobilizing);
        }
        CHECK_FALSE(uhrwerk::order_mobilization(w, bra, false));
        // Gegnerische Orte stehen offen, Orte ohne Daten nicht.
        CHECK(uhrwerk::accessible(w, bra, w.find_location("dresden")));
        CHECK(uhrwerk::accessible(w, sax, w.find_location("potsdam")));
        CHECK_FALSE(uhrwerk::accessible(w, bra, w.find_location("zerbst")));
        CHECK_FALSE(uhrwerk::can_declare_war(w, bra, sax)); // schon im Krieg
        CHECK_FALSE(uhrwerk::can_declare_war(w, bra, anh)); // auf derselben Seite
        CHECK_FALSE(uhrwerk::declare_war(w, sax, bra));
        CHECK(w.wars.size() == 1);
    }
}

TEST_CASE("Militär: Schlacht, Rückzug, Besetzung und Verluste") {
    const MilitaryData data;
    Simulation sim(with_strength(data.load(), 30, 5));
    const World& v = sim.world();
    const Index bra = v.find_state("BRA");
    const Index sax = v.find_state("SAX");
    const Index dresden = v.find_location("dresden");
    const Index leipzig = v.find_location("leipzig");
    const auto army = [&](std::int64_t id) -> const uhrwerk::Army& { return *uhrwerk::find_army(v, id); };
    const Fixed devastation = v.locations[at(dresden)].devastation;

    // Ankunft am vierten Tag: Schlacht, die Sachsen weichen nach Leipzig.
    brandenburg_attacks(sim, 4);
    REQUIRE(v.wars.size() == 1);
    CHECK(army(1).location == dresden);
    CHECK(army(1).status == ArmyStatus::Rest);
    CHECK(v.battles.empty());
    CHECK(chronicle_contains(v, "Schlacht bei Dresden: Garde schlägt Garnison Dresden; Verluste "));
    const uhrwerk::Army& saxons = army(3);
    CHECK(saxons.status == ArmyStatus::Retreat);
    REQUIRE(saxons.route.size() == 1);
    CHECK(saxons.route[0].location == leipzig);
    CHECK(saxons.morale < Fixed::from_ratio(3, 10));
    CHECK(army(1).morale > saxons.morale);
    REQUIRE(v.prisoners.size() == 1);
    CHECK(v.prisoners[0].state == sax);
    CHECK(v.prisoners[0].captor == bra);
    CHECK(v.prisoners[0].soldiers > Fixed::zero());
    CHECK_FALSE(v.convalescents.empty());
    CHECK(v.war_dead[at(sax)] > Fixed::zero());
    CHECK(v.locations[at(dresden)].devastation > devastation);
    CHECK(v.control[at(dresden)] == uhrwerk::kNone); // erst nach einem vollen Tag

    // Am nächsten Tag ist Dresden besetzt; die Sachsen stehen in Leipzig.
    sim.step_day();
    CHECK(v.control[at(dresden)] == bra);
    CHECK(uhrwerk::controller(v, dresden) == bra);
    CHECK(chronicle_contains(v, "Brandenburg besetzt Dresden, die Hauptstadt von Sachsen."));
    sim.step_days(6); // gut sechs Tage über die Elbe
    CHECK(army(3).location == leipzig);
    CHECK(army(3).status == ArmyStatus::Rest);

    // Am Monatsersten mindern die Gefallenen die Jahrgänge im wehrfähigen Alter.
    while (v.date.day != 1) {
        sim.step_day();
    }
    const Fixed dead = v.war_dead[at(sax)];
    REQUIRE(dead > Fixed::zero());
    const Fixed deaths_before = v.balance_current[at(dresden)].deaths + v.balance_current[at(leipzig)].deaths;
    sim.step_day();
    const Fixed deaths_after = v.balance_current[at(dresden)].deaths + v.balance_current[at(leipzig)].deaths;
    CHECK(v.war_dead[at(sax)] < dead / Fixed::from_int(10)); // nur die Abgänge des Tages
    CHECK(difference(deaths_after - deaths_before, dead) < Fixed::one());

    // Die Verwundeten der Schlacht kehren nach 60 Tagen zu ihren Heeren zurück.
    REQUIRE_FALSE(v.convalescents.empty());
    sim.step_days(60);
    CHECK(v.convalescents.empty());
}

TEST_CASE("Militär: Kontribution aus besetzten Orten") {
    const MilitaryData data;
    World w = Simulation(data.load()).world();
    const Index bra = w.find_state("BRA");
    const Index sax = w.find_state("SAX");
    const Index dresden = w.find_location("dresden");
    const Index leipzig = w.find_location("leipzig");
    const auto kontribution = static_cast<std::size_t>(uhrwerk::TaxKind::Kontribution);
    World peaceful = w;
    uhrwerk::run_state_budget(peaceful, bra, {w.find_location("berlin_coelln"), w.find_location("potsdam")});
    REQUIRE(uhrwerk::declare_war(w, bra, sax));
    w.control[at(dresden)] = bra;
    uhrwerk::run_state_budget(w, sax, {dresden, leipzig});
    uhrwerk::run_state_budget(w, bra, {w.find_location("berlin_coelln"), w.find_location("potsdam")});
    // Sachsen fehlen Dresdens Steuern; Brandenburg erhält die Hälfte als Kontribution.
    for (std::size_t t = 0; t < uhrwerk::kTaxKinds; ++t) {
        CHECK(w.states[at(sax)].budget.taxes[t] == w.economy[at(leipzig)].taxes[t]);
    }
    Fixed half;
    for (const Fixed tax : w.economy[at(dresden)].taxes) {
        half += tax / Fixed::from_int(2);
    }
    REQUIRE(half > Fixed::zero());
    CHECK(w.states[at(bra)].budget.taxes[kontribution] ==
          peaceful.states[at(bra)].budget.taxes[kontribution] + half);
}

TEST_CASE("Militär: Belagerung einer Festung") {
    const MilitaryData data;
    data.add_fortress(1);
    data.add_rules("    festung_besatzung = 100\n");
    const World start = with_strength(data.load(), 30, 5);
    REQUIRE(start.fortresses.size() == 1);
    CHECK(start.fortresses[0].name == "Festung Dresden");
    CHECK(uhrwerk::fortress_level(start, start.find_location("dresden")) == 1);
    Simulation sim(start);
    const World& v = sim.world();
    const Index bra = v.find_state("BRA");
    const Index dresden = v.find_location("dresden");
    brandenburg_attacks(sim, 4);
    CHECK(chronicle_contains(v, "Schlacht bei Dresden: Garde schlägt Garnison Dresden"));
    // Mindestens zehn Tage Belagerung (kleine Besatzung), dann ergibt sich die Festung.
    sim.step_day();
    REQUIRE(v.sieges.size() == 1);
    CHECK(v.sieges[0].besieger == bra);
    CHECK(difference(v.sieges[0].progress, Fixed::from_ratio(1, 10)) < Fixed::from_ratio(1, 1000));
    CHECK(uhrwerk::find_army(v, 1)->status == ArmyStatus::Siege);
    CHECK(v.control[at(dresden)] == uhrwerk::kNone);
    sim.step_days(8);
    CHECK(v.sieges.size() == 1);
    CHECK(v.control[at(dresden)] == uhrwerk::kNone);
    sim.step_day();
    CHECK(v.sieges.empty());
    CHECK(v.control[at(dresden)] == bra);
    CHECK(uhrwerk::find_army(v, 1)->status == ArmyStatus::Rest);
    CHECK(chronicle_contains(v, "Die Festung Dresden ergibt sich Brandenburg."));
}

TEST_CASE("Militär: Krise wird zum Krieg") {
    const MilitaryData data;
    data.add_federation();
    World w = with_strength(data.load(), 10, 0);
    const Index bra = w.find_state("BRA");
    const Index sax = w.find_state("SAX");
    // Sachsen fast so stark wie Brandenburg: keine Seite gibt nach.
    w.states[at(sax)].army_per_mille = uhrwerk::state_strength(w, bra) * Fixed::from_ratio(9, 10) *
                                       Fixed::from_int(1000) / w.state_population(sax);
    Simulation sim(w);
    const World& v = sim.world();
    const auto escalate = [&] {
        Command c;
        c.type = CommandType::Manoeuvre;
        c.state = sax;
        c.value = Fixed::from_int(static_cast<std::int64_t>(uhrwerk::Manoeuvre::Escalate));
        sim.submit(c);
    };
    sim.submit(
        command(CommandType::Demand, bra, static_cast<std::int64_t>(uhrwerk::DemandKind::Leadership), sax));
    sim.step_day();
    for (const std::int64_t day : {1, 31, 61}) {
        while (sim.days_elapsed() < day) {
            sim.step_day();
        }
        escalate();
    }
    while (!v.plays.empty() && sim.days_elapsed() < 120) {
        sim.step_day();
    }
    REQUIRE(v.play_results.size() == 1);
    CHECK(v.play_results[0].outcome == uhrwerk::PlayOutcome::War);
    CHECK(v.play_results[0].stage == 3);
    REQUIRE(v.wars.size() == 1);
    CHECK(v.wars[0].cause == uhrwerk::WarCause::Crisis);
    CHECK(v.wars[0].attackers.front() == bra);
    CHECK(v.wars[0].defenders.front() == sax);
    CHECK(chronicle_contains(v, "Die Krise zwischen Brandenburg und Sachsen wird zum Krieg."));
    // Die Mobilmachung der Krise bleibt im Krieg bestehen.
    CHECK(uhrwerk::mobilization_phase(v, bra) != MobilizationPhase::Demobilizing);
    CHECK_FALSE(v.mobilizations[at(bra)].crisis);
}

TEST_CASE("Militär: Bundesexekution") {
    const MilitaryData data;
    data.add_federation();
    data.add_diet();
    Simulation sim(data.load());
    const World& v = sim.world();
    const Index bra = v.find_state("BRA");
    const Index sax = v.find_state("SAX");
    const Index anh = v.find_state("ANH");
    sim.submit(
        command(CommandType::Motion, bra, static_cast<std::int64_t>(uhrwerk::MotionKind::Intervention), sax));
    while (v.motion_results.empty() && sim.days_elapsed() < 70) {
        sim.step_day();
    }
    REQUIRE(v.motion_results.size() == 1);
    REQUIRE(v.motion_results[0].accepted);
    REQUIRE(v.wars.size() == 1);
    CHECK(v.wars[0].cause == uhrwerk::WarCause::Execution);
    CHECK(v.wars[0].attackers == std::vector<Index>{bra, anh});
    CHECK(v.wars[0].defenders == std::vector<Index>{sax});
    CHECK(chronicle_contains(v, "Bundesexekution gegen Sachsen. An der Seite von Brandenburg: Anhalt."));
}

TEST_CASE("Militär: Spielstand während Schlacht und Belagerung") {
    const MilitaryData data;
    data.add_fortress(1);
    data.add_rules("    festung_besatzung = 100\n    verlustrate = 0.03\n");
    const World start = with_strength(data.load(), 30, 5);
    const auto run = [&](unsigned threads, std::int64_t days) {
        SimConfig config;
        config.threads = threads;
        Simulation sim(start, config);
        brandenburg_attacks(sim, days);
        return sim;
    };
    const Simulation full = run(1, 60);
    CHECK(run(4, 60).checksum() == full.checksum());
    bool saw_battle = false;
    bool saw_siege = false;
    for (std::int64_t day = 4; day < 45; ++day) {
        const Simulation part = run(1, day);
        saw_battle = saw_battle || !part.world().battles.empty();
        saw_siege = saw_siege || !part.world().sieges.empty();
        if (part.world().battles.empty() && part.world().sieges.empty()) {
            continue;
        }
        CAPTURE(day);
        const Bytes bytes = uhrwerk::save_game(part, SaveMeta{});
        uhrwerk::LoadedGame loaded = uhrwerk::load_game(bytes, start, 1);
        INFO(loaded.error);
        REQUIRE(loaded.ok());
        CHECK(loaded.simulation->world().battles == part.world().battles);
        CHECK(loaded.simulation->world().sieges == part.world().sieges);
        CHECK(loaded.simulation->world().wars == part.world().wars);
        brandenburg_attacks(*loaded.simulation, 60);
        CHECK(loaded.simulation->checksum() == full.checksum());
    }
    CHECK(saw_battle);
    CHECK(saw_siege);
    const uhrwerk::ReplayResult replay = uhrwerk::run_replay(uhrwerk::make_replay(full), start, 2);
    INFO(replay.error);
    CHECK(replay.ok);
}

// ─── Schritt a4: Kriegsziele, Kriegspunkte, Frieden, Generalstab ────────────

namespace {

std::int64_t goal(uhrwerk::WarGoalKind kind, Index param = 0) {
    return uhrwerk::encode_war_goal(kind, param);
}

Command peace(Index state, uhrwerk::PeaceTerms terms) {
    return command(CommandType::PeaceOffer, state, static_cast<std::int64_t>(terms), 1);
}

} // namespace

TEST_CASE("Militär: Kriegsziele und ihre Kosten") {
    using uhrwerk::WarGoalKind;
    const MilitaryData data;
    data.add_unit();
    World w = Simulation(data.load()).world();
    const Index bra = w.find_state("BRA");
    const Index sax = w.find_state("SAX");
    const Index unit = w.find_admin_unit("leipziger_kreis");
    REQUIRE(unit != uhrwerk::kNone);
    CHECK(uhrwerk::decode_war_goal(0, true) == std::nullopt);
    CHECK(uhrwerk::decode_war_goal(6, true) == std::nullopt);
    CHECK(uhrwerk::decode_war_goal(goal(WarGoalKind::Annexation, sax), false) ==
          uhrwerk::WarGoal{WarGoalKind::Annexation, false, sax});
    REQUIRE(uhrwerk::declare_war(w, bra, sax, goal(WarGoalKind::Territory, unit)));
    uhrwerk::War& war = w.wars[0];
    REQUIRE(war.goals.size() == 1);
    CHECK(chronicle_contains(w, "Kriegsziel: Leipziger Kreis."));
    CHECK(war.attacker_strength > Fixed::zero());
    CHECK(war.defender_strength > Fixed::zero());
    // Gebiet nach dem Anteil an der Bevölkerung Sachsens, mindestens 10.
    const Fixed part = w.location_population(w.find_location("leipzig"));
    const Fixed whole = part + w.location_population(w.find_location("dresden"));
    CHECK(uhrwerk::war_goal_cost(w, war, war.goals[0]) ==
          Fixed::max(Fixed::from_int(10), Fixed::from_int(100) * part / whole));
    // Weitere Ziele: Annexion 80, Entschädigung 10 je Monat; ungültig sind Führung (kein Bund), Zollunion,
    // Annexion des eigenen Staates, 25 Monate und Ziele der Mitkriegführenden.
    CHECK(uhrwerk::add_war_goal(w, bra, 1, goal(WarGoalKind::Annexation, sax)));
    CHECK(uhrwerk::add_war_goal(w, sax, 1, goal(WarGoalKind::Indemnity, 3)));
    CHECK_FALSE(uhrwerk::add_war_goal(w, bra, 1, goal(WarGoalKind::Annexation, sax))); // schon gestellt
    CHECK_FALSE(uhrwerk::add_war_goal(w, bra, 1, goal(WarGoalKind::Annexation, bra)));
    CHECK_FALSE(uhrwerk::add_war_goal(w, bra, 1, goal(WarGoalKind::Leadership)));
    CHECK_FALSE(uhrwerk::add_war_goal(w, bra, 1, goal(WarGoalKind::Customs)));
    CHECK_FALSE(uhrwerk::add_war_goal(w, bra, 1, goal(WarGoalKind::Indemnity, 25)));
    CHECK_FALSE(uhrwerk::add_war_goal(w, bra, 2, goal(WarGoalKind::Indemnity, 1))); // kein Krieg 2
    CHECK_FALSE(uhrwerk::add_war_goal(w, w.find_state("ANH"), 1, goal(WarGoalKind::Indemnity, 1)));
    CHECK(uhrwerk::war_goals_cost(w, war, false) == Fixed::from_int(30));
    CHECK(uhrwerk::war_goals_cost(w, war, true) ==
          uhrwerk::war_goal_cost(w, war, war.goals[0]) + Fixed::from_int(80));
    // Ohne Schlacht und Besetzung: keine Punkte, kaum Müdigkeit.
    CHECK(uhrwerk::war_score(w, war, true) == Fixed::zero());
    CHECK(uhrwerk::war_weariness(w, war, false) == Fixed::zero());
}

TEST_CASE("Militär: Kriegspunkte, Friedensangebote und Annexion") {
    const MilitaryData data;
    Simulation sim(with_strength(data.load(), 30, 5));
    const World& v = sim.world();
    const Index bra = v.find_state("BRA");
    const Index sax = v.find_state("SAX");
    brandenburg_attacks(sim, 5, goal(uhrwerk::WarGoalKind::Annexation, sax));
    REQUIRE(v.wars.size() == 1);
    REQUIRE(v.control[at(v.find_location("dresden"))] == bra);
    const uhrwerk::War& war = v.wars[0];
    const Fixed score = uhrwerk::war_score(v, war, true);
    CHECK(score > Fixed::from_int(20)); // Hauptstadt besetzt, Schlacht gewonnen
    CHECK(score < Fixed::from_int(80));
    CHECK(uhrwerk::war_score(v, war, false) == Fixed::zero());
    CHECK(uhrwerk::war_weariness(v, war, false) > uhrwerk::war_weariness(v, war, true));
    CHECK(war.attacker_inflicted > war.defender_inflicted);

    // Sachsen bietet weißen Frieden an, Brandenburg verlangt die Annexion: beides abgelehnt.
    sim.submit(peace(sax, uhrwerk::PeaceTerms::White));
    sim.submit(peace(bra, uhrwerk::PeaceTerms::OwnGoals));
    sim.submit(peace(v.find_state("ANH"), uhrwerk::PeaceTerms::White)); // keine Hauptpartei
    sim.step_day();
    CHECK(chronicle_contains(v,
                             "Sachsen bietet Brandenburg Frieden an (ohne Gewinner); Brandenburg lehnt ab."));
    CHECK(chronicle_contains(
        v, "Brandenburg bietet Sachsen Frieden an (zu seinen Bedingungen); Sachsen lehnt ab."));
    REQUIRE(v.wars.size() == 1);

    // Sachsen unterwirft sich: Annexion, Sachsen geht unter.
    const Fixed treasury = v.states[at(bra)].treasury + v.states[at(sax)].treasury;
    sim.submit(peace(sax, uhrwerk::PeaceTerms::Capitulation));
    sim.step_day();
    CHECK(v.wars.empty());
    CHECK(chronicle_contains(
        v, "Frieden zwischen Brandenburg und Sachsen: Brandenburg siegt. Brandenburg annektiert "
           "Sachsen (2 Orte). Sachsen geht unter."));
    CHECK(v.locations[at(v.find_location("leipzig"))].owner == bra);
    CHECK(v.locations[at(v.find_location("dresden"))].owner == bra);
    CHECK(v.states[at(sax)].capital == uhrwerk::kNone);
    CHECK(v.states[at(sax)].treasury == Fixed::zero());
    CHECK(v.states[at(bra)].treasury >= treasury);
    CHECK(std::none_of(v.armies.begin(), v.armies.end(),
                       [&](const uhrwerk::Army& a) { return a.state == sax; }));
    CHECK(std::all_of(v.control.begin(), v.control.end(), [](Index c) { return c == uhrwerk::kNone; }));
    CHECK(v.prisoners.empty());
    CHECK(uhrwerk::mobilization_phase(v, bra) == MobilizationPhase::Demobilizing);
    // Weiterrechnen mit den neuen Grenzen; nach der Abrüstung stehen die Heere in den Garnisonen.
    sim.step_days(40);
    CHECK(uhrwerk::mobilization_phase(v, bra) == MobilizationPhase::Peace);
    CHECK(uhrwerk::find_army(v, 1)->status == ArmyStatus::Garrison);
    CHECK(uhrwerk::find_army(v, 1)->location == v.find_location("berlin_coelln"));
}

TEST_CASE("Militär: Gebiet, Entschädigung und Stillstand") {
    using uhrwerk::WarGoalKind;
    const MilitaryData data;
    data.add_unit();
    const World start = with_strength(data.load(), 30, 5);
    const Index bra = start.find_state("BRA");
    const Index sax = start.find_state("SAX");
    SUBCASE("Gebiet") {
        Simulation sim(start);
        const World& v = sim.world();
        brandenburg_attacks(sim, 5, goal(WarGoalKind::Territory, v.find_admin_unit("leipziger_kreis")));
        sim.submit(peace(sax, uhrwerk::PeaceTerms::Capitulation));
        sim.step_day();
        CHECK(v.wars.empty());
        CHECK(chronicle_contains(v, "Brandenburg erhält Leipziger Kreis (1 Ort)."));
        CHECK(v.locations[at(v.find_location("leipzig"))].owner == bra);
        CHECK(v.locations[at(v.find_location("dresden"))].owner == sax);
        CHECK(v.states[at(sax)].capital == v.find_location("dresden"));
        // Brandenburgs Heer in Dresden kehrt heim; Sachsens Heer, das in Leipzig stand, ebenso.
        for (const uhrwerk::Army& a : v.armies) {
            CHECK(uhrwerk::accessible(v, a.state, a.location));
        }
    }
    SUBCASE("Entschädigung") {
        // Neun Monatseinnahmen kosten 90 Punkte: kein Frieden von selbst. Kapitulation nach Sachsens erstem
        // Rechentag, nicht am Rechentag eines der beiden Staaten.
        Simulation sim(start);
        const World& v = sim.world();
        brandenburg_attacks(sim, 45, goal(WarGoalKind::Indemnity, 9));
        while (v.date.day == 1 || v.date.day == 3) {
            sim.step_day();
        }
        REQUIRE(v.wars.size() == 1);
        const Fixed income = v.states[at(sax)].last_month_income;
        REQUIRE(income > Fixed::zero());
        const Fixed before = v.states[at(bra)].treasury;
        sim.submit(peace(sax, uhrwerk::PeaceTerms::Capitulation));
        sim.step_day();
        CHECK(v.wars.empty());
        CHECK(v.states[at(bra)].treasury == before + income * Fixed::from_int(9));
        CHECK(chronicle_contains(v, "Sachsen zahlt "));
    }
    SUBCASE("Stillstand") {
        Simulation sim(start);
        const World& v = sim.world();
        sim.submit(command(CommandType::DeclareWar, bra, 0, sax));
        sim.step_days(300);
        CHECK(v.wars.size() == 1);
        sim.step_days(100);
        CHECK(v.wars.empty());
        CHECK(chronicle_contains(v, "Frieden zwischen Brandenburg und Sachsen ohne Gewinner."));
    }
}

TEST_CASE("Militär: Führung im Bund") {
    const MilitaryData data;
    data.add_federation();
    data.add_diet();
    Simulation sim(with_strength(data.load(), 30, 5));
    const World& v = sim.world();
    const Index sax = v.find_state("SAX");
    REQUIRE(v.diet_presidency == sax);
    REQUIRE(uhrwerk::plenum_votes(v, sax) == 4);
    brandenburg_attacks(sim, 5, goal(uhrwerk::WarGoalKind::Leadership));
    REQUIRE(v.wars.size() == 1);
    REQUIRE(v.wars[0].goals.size() == 1);
    // Sachsen hat das Präsidium: Mit seiner Niederlage löst sich der Bund auf.
    sim.submit(peace(sax, uhrwerk::PeaceTerms::Capitulation));
    sim.step_day();
    CHECK(chronicle_contains(v, "Der Deutsche Bund löst sich auf."));
    for (std::size_t s = 0; s < v.states.size(); ++s) {
        CHECK_FALSE(uhrwerk::federal_state(v, static_cast<Index>(s)));
        CHECK(uhrwerk::plenum_votes(v, static_cast<Index>(s)) == 0);
    }
}

TEST_CASE("Militär: Generalstab und Lauf ohne Spieler") {
    const MilitaryData data;
    data.add_rules("    generalstab = ja\n");
    const World start = with_strength(data.load(), 30, 5);
    const Index bra = start.find_state("BRA");
    const Index sax = start.find_state("SAX");
    const auto run = [&](unsigned threads, std::int64_t days) {
        SimConfig config;
        config.threads = threads;
        Simulation sim(start, config);
        while (sim.days_elapsed() < days) {
            if (sim.days_elapsed() == 0) {
                sim.submit(
                    command(CommandType::DeclareWar, bra, goal(uhrwerk::WarGoalKind::Annexation, sax), sax));
            }
            sim.step_day();
        }
        return sim;
    };
    SUBCASE("Generalstab führt die Heere zum Sieg") {
        const Simulation sim = run(1, 200);
        const World& v = sim.world();
        CHECK(std::all_of(v.armies.begin(), v.armies.end(), [](const uhrwerk::Army& a) { return a.staff; }));
        CHECK(v.wars.empty());
        CHECK(chronicle_contains(v, "Brandenburg annektiert Sachsen"));
        CHECK(v.locations[at(v.find_location("dresden"))].owner == bra);
    }
    SUBCASE("Befehle nehmen dem Generalstab Heere ab") {
        Simulation sim(start);
        const World& v = sim.world();
        sim.submit(command(CommandType::DeclareWar, bra, 0, sax));
        sim.submit(command(CommandType::Staff, bra, 1, 0));
        sim.submit(command(CommandType::Staff, sax, 1, 1)); // nicht sein Heer
        sim.step_days(10);
        CHECK_FALSE(uhrwerk::find_army(v, 1)->staff);
        CHECK(uhrwerk::find_army(v, 1)->location == v.find_location("berlin_coelln"));
        CHECK(uhrwerk::find_army(v, 2)->staff);
        sim.submit(command(CommandType::March, bra, 2, v.find_location("berlin_coelln")));
        sim.step_day();
        CHECK_FALSE(uhrwerk::find_army(v, 2)->staff);
    }
    SUBCASE("Threads, Spielstand und Wiederholung") {
        const Simulation full = run(1, 150);
        CHECK(run(4, 150).checksum() == full.checksum());
        for (const std::int64_t day : {6, 40}) {
            CAPTURE(day);
            const Simulation part = run(1, day);
            const Bytes bytes = uhrwerk::save_game(part, SaveMeta{});
            uhrwerk::LoadedGame loaded = uhrwerk::load_game(bytes, start, 2);
            INFO(loaded.error);
            REQUIRE(loaded.ok());
            CHECK(loaded.simulation->world().wars == part.world().wars);
            loaded.simulation->step_days(150 - day);
            CHECK(loaded.simulation->checksum() == full.checksum());
        }
        const uhrwerk::ReplayResult replay = uhrwerk::run_replay(uhrwerk::make_replay(full), start, 2);
        INFO(replay.error);
        CHECK(replay.ok);
    }
}

TEST_CASE("Militär: Kriegslabor") {
    const MilitaryData data;
    data.add_rules("    generalstab = ja\n");
    data.add_unit();
    const World start = with_strength(data.load(), 30, 5);
    const Index bra = start.find_state("BRA");
    const Index sax = start.find_state("SAX");
    using uhrwerk::WarGoalKind;
    using uhrwerk::WarOutcome;

    SUBCASE("Kriegsziele lesen") {
        const uhrwerk::WarTargetsResult r = uhrwerk::parse_war_targets(
            "krieg sachsen {\n    angreifer = BRA\n    verteidiger = SAX\n"
            "    ziel = { annexion = SAX gebiet = leipziger_kreis }\n    ziel = { entschaedigung = 3 }\n"
            "    tage = 200\n    sieg = { 0.5 1 }\n    niederlage = { 0 0.2 }\n    weiss = { 0 0.5 }\n"
            "    dauer = { 10 150 }\n    offen = ja\n}\n"
            "krieg anhalt { angreifer = SAX verteidiger = ANH }\n",
            start);
        INFO(lines(r.errors));
        REQUIRE(r.ok());
        REQUIRE(r.targets.wars.size() == 2);
        const uhrwerk::WarScenario& war = r.targets.wars[0];
        CHECK(war.id == "sachsen");
        CHECK(war.attacker == bra);
        CHECK(war.defender == sax);
        REQUIRE(war.goals.size() == 3);
        CHECK(war.goals[0] == uhrwerk::WarGoal{WarGoalKind::Annexation, true, sax});
        CHECK(war.goals[1] ==
              uhrwerk::WarGoal{WarGoalKind::Territory, true, start.find_admin_unit("leipziger_kreis")});
        CHECK(war.goals[2] == uhrwerk::WarGoal{WarGoalKind::Indemnity, true, 3});
        CHECK(war.days == 200);
        REQUIRE(war.victory.has_value());
        CHECK(war.victory->min == Fixed::from_ratio(1, 2));
        CHECK(war.defeat.has_value());
        CHECK(war.white.has_value());
        REQUIRE(war.duration.has_value());
        CHECK(war.duration->max == Fixed::from_int(150));
        CHECK(war.open);
        const uhrwerk::WarScenario& anhalt = r.targets.wars[1];
        CHECK(anhalt.goals.empty());
        CHECK(anhalt.days == 730);
        CHECK_FALSE(anhalt.victory.has_value());
        CHECK_FALSE(anhalt.open);
    }

    SUBCASE("Fehler in den Kriegszielen") {
        for (const auto& [text, message] : {
                 std::pair{"krieg a { angreifer = XXX verteidiger = SAX }", "unbekannter Staat 'XXX'"},
                 std::pair{"krieg a { angreifer = SAX verteidiger = SAX }", "derselbe Staat"},
                 std::pair{"krieg a { angreifer = SAX }", "braucht 'angreifer' und 'verteidiger'"},
                 std::pair{"krieg a { angreifer = BRA verteidiger = SAX sieg = { 0 2 } }",
                           "innerhalb von 0 bis 1"},
                 std::pair{"krieg a { angreifer = BRA verteidiger = SAX dauer = { 9 3 } }", "von ≤ bis"},
                 std::pair{"krieg a { angreifer = BRA verteidiger = SAX tage = 0 }", "'tage' erwartet"},
                 std::pair{"krieg a { angreifer = BRA verteidiger = SAX sieger = BRA }",
                           "unbekanntes Feld 'sieger'"},
                 std::pair{"krieg a { angreifer = BRA verteidiger = SAX ziel = { entschaedigung = 30 } }",
                           "ganze Monatseinnahmen"},
                 std::pair{"krieg a { angreifer = BRA verteidiger = SAX ziel = { gebiet = nirgends } }",
                           "unbekannte Verwaltungseinheit"},
                 std::pair{"krieg a { angreifer = BRA verteidiger = SAX ziel = { fuehrung = nein } }",
                           "'fuehrung' erwartet ja"},
                 std::pair{"krieg a { angreifer = BRA verteidiger = SAX ziel = { beute = ja } }",
                           "unbekanntes Feld 'beute'"},
                 std::pair{"krieg a { angreifer = BRA verteidiger = SAX offen = vielleicht }",
                           "'offen' erwartet ja oder nein"},
                 std::pair{"krieg a { angreifer = BRA verteidiger = SAX }\nkrieg a { angreifer = SAX "
                           "verteidiger = BRA }",
                           "mehrfach"},
                 std::pair{"wechsel a { }", "unbekanntes Feld 'wechsel'"},
             }) {
            CAPTURE(text);
            const uhrwerk::WarTargetsResult r = uhrwerk::parse_war_targets(text, start, "ziele.chron");
            CHECK_FALSE(r.ok());
            CHECK(r.targets.wars.empty());
            const std::string errors = lines(r.errors);
            CAPTURE(errors);
            CHECK(errors.find(message) != std::string::npos);
            CHECK(errors.find("ziele.chron:") != std::string::npos);
        }
    }

    uhrwerk::WarScenario war;
    war.id = "sachsen";
    war.attacker = bra;
    war.defender = sax;
    war.goals = {uhrwerk::WarGoal{WarGoalKind::Annexation, true, sax},
                 uhrwerk::WarGoal{WarGoalKind::Indemnity, true, 2}};
    war.days = 200;

    SUBCASE("Lauf bis zum Frieden") {
        const uhrwerk::WarLabRun run = uhrwerk::run_war_lab(start, SimConfig{7, 1}, war);
        CHECK(run.started);
        CHECK(run.outcome == WarOutcome::Attacker);
        CHECK(run.days > 0);
        CHECK(run.days < 200);
        CHECK(run.battles >= 1);
        CHECK(run.transfers == 2);
        CHECK(run.defender_losses > Fixed::zero());
        CHECK(run.peace.find("Brandenburg annektiert Sachsen") != std::string::npos);
        // Gleicher Seed, gleiches Ergebnis, auch mit Threads.
        const uhrwerk::WarLabRun again = uhrwerk::run_war_lab(start, SimConfig{7, 4}, war);
        CHECK(again.days == run.days);
        CHECK(again.battles == run.battles);
        CHECK(again.attacker_losses == run.attacker_losses);
        CHECK(again.defender_debt == run.defender_debt);
        CHECK(again.peace == run.peace);
        // Der Krieg bleibt als Ergebnis im Zustand.
        Simulation sim(start);
        sim.submit(command(CommandType::DeclareWar, bra, goal(WarGoalKind::Annexation, sax), sax));
        sim.step_days(200);
        const World& v = sim.world();
        REQUIRE(v.war_results.size() == 1);
        const uhrwerk::WarResult& result = v.war_results[0];
        CHECK(result.attacker == bra);
        CHECK(result.defender == sax);
        CHECK(result.winner == 1);
        CHECK(result.transfers == 2);
        CHECK(result.battles >= 1);
        CHECK(result.ended > result.started);
        CHECK(result.defender_losses > Fixed::zero());
    }

    SUBCASE("Höchstdauer ohne Frieden") {
        war.days = 5;
        const uhrwerk::WarLabRun run = uhrwerk::run_war_lab(start, SimConfig{7, 1}, war);
        CHECK(run.started);
        CHECK(run.outcome == WarOutcome::Open);
        CHECK(run.days == 5);
        CHECK(run.peace.empty());
    }

    SUBCASE("Zusammenfassung und Verstöße") {
        std::vector<uhrwerk::WarLabRun> runs;
        for (const std::uint64_t seed : {1u, 2u, 3u}) {
            runs.push_back(uhrwerk::run_war_lab(start, SimConfig{seed, 1}, war));
        }
        uhrwerk::WarScenario invalid = war;
        invalid.id = "ungueltig";
        invalid.defender = bra; // sich selbst kann niemand den Krieg erklären
        const uhrwerk::WarLabRun none = uhrwerk::run_war_lab(start, SimConfig{1, 1}, invalid);
        CHECK_FALSE(none.started);

        war.victory = uhrwerk::ValueRange{Fixed::from_ratio(9, 10), Fixed::one()};
        uhrwerk::WarLabSummary s = uhrwerk::summarize_war_lab({war, invalid}, {runs, {none, none}});
        REQUIRE(s.wars.size() == 2);
        CHECK(s.wars[0].outcomes[static_cast<std::size_t>(WarOutcome::Attacker)] == 3);
        CHECK(s.wars[0].transfers.median == Fixed::from_int(2));
        CHECK(s.wars[0].days.median > Fixed::zero());
        CHECK(s.wars[1].not_started == 2);
        REQUIRE(s.violations.size() == 1);
        CHECK(s.violations[0].check == uhrwerk::WarCheck::NotStarted);
        CHECK(s.violations[0].war == 1);

        war.open = true;
        war.defeat = uhrwerk::ValueRange{Fixed::from_ratio(1, 2), Fixed::one()};
        war.duration = uhrwerk::ValueRange{Fixed::one(), Fixed::from_int(2)};
        s = uhrwerk::summarize_war_lab({war}, {runs});
        std::vector<uhrwerk::WarCheck> checks;
        for (const uhrwerk::WarViolation& v : s.violations) {
            checks.push_back(v.check);
        }
        CHECK(checks == std::vector<uhrwerk::WarCheck>{uhrwerk::WarCheck::Defeat, uhrwerk::WarCheck::Duration,
                                                       uhrwerk::WarCheck::Fixed});
        CHECK(s.violations[0].value == Fixed::zero());
        CHECK(s.violations[2].value == Fixed::from_int(3));
    }
}

// ─── Schritt b1: Lesezeichen im Krieg ───────────────────────────────────────

namespace {

constexpr std::string_view kDeployment =
    "heer bra_erste {\n    staat = BRA\n    name = \"I. Armee\"\n    befehlshaber = \"Derfflinger\"\n"
    "    fuehrung = 2\n    ort = potsdam\n    soldaten = 5000\n    moral = 0.9\n    quelle = "
    "\"Testdaten\"\n}\n"
    "krieg sachsenkrieg {\n    name = \"Sächsischer Krieg\"\n    beginn = 1648.10.01\n"
    "    angreifer = { BRA }\n    verteidiger = { SAX }\n    ziel = { annexion = SAX }\n"
    "    ziel_verteidiger = { entschaedigung = 2 }\n}\n"
    "buendnis { staaten = { BRA ANH } }\n";

constexpr std::string_view kHinterland =
    "hinterland SAX {\n    bevoelkerung = 100000\n    einnahmen = 1000\n    ersatz = 500\n"
    "    orte = { leipzig dresden }\n}\n";

/// Fixture mit Aufmarsch für das Szenario westfaelischer_frieden und Hinterland Sachsens.
void add_deployment(const MilitaryData& data, std::string_view deployment = kDeployment) {
    testhilfen::write_file(data.dir() / "common/aufmarsch_westfaelischer_frieden.chron", deployment);
    testhilfen::write_file(data.dir() / "common/militaer.chron",
                           testhilfen::read_file(data.dir() / "common/militaer.chron") +
                               std::string(kHinterland));
}

const uhrwerk::Army* army_named(const World& w, std::string_view name) {
    const auto it = std::find_if(w.armies.begin(), w.armies.end(),
                                 [&](const uhrwerk::Army& a) { return a.name == name; });
    return it == w.armies.end() ? nullptr : &*it;
}

} // namespace

TEST_CASE("Militär: Lesezeichen im Krieg") {
    const MilitaryData data;
    add_deployment(data);
    const World start = with_strength(data.load(), 10, 10);
    const Index bra = start.find_state("BRA");
    const Index sax = start.find_state("SAX");
    const Index anh = start.find_state("ANH");
    REQUIRE(start.deployments.size() == 1);
    REQUIRE(start.opening_wars.size() == 1);
    REQUIRE(start.hinterlands.size() == 1);
    CHECK(start.hinterlands[0].entries.size() == 2);
    CHECK(start.opening_wars[0].goals.size() == 2);

    SUBCASE("Aufmarsch, Bündnis und Krieg zum Start") {
        const Simulation sim(start);
        const World& v = sim.world();
        const uhrwerk::Army* first = army_named(v, "I. Armee");
        REQUIRE(first != nullptr);
        CHECK(first->state == bra);
        CHECK(first->location == v.find_location("potsdam"));
        CHECK(first->commander == "Derfflinger");
        CHECK(first->leadership == Fixed::from_int(2));
        CHECK(first->soldiers == Fixed::from_int(5000));
        CHECK(first->morale == Fixed::from_ratio(9, 10));
        CHECK(first->status == ArmyStatus::Rest);
        CHECK(first->garrison == uhrwerk::kNone);
        CHECK(uhrwerk::mobilization_phase(v, bra) == MobilizationPhase::Mobilized);
        CHECK(uhrwerk::mobilization_phase(v, sax) == MobilizationPhase::Mobilizing);
        // Die Garnisonen Brandenburgs bleiben stehen.
        CHECK(std::count_if(v.armies.begin(), v.armies.end(), [&](const uhrwerk::Army& a) {
                  return a.state == bra && a.status == ArmyStatus::Garrison;
              }) == 2);
        REQUIRE(v.wars.size() == 1);
        const uhrwerk::War& war = v.wars[0];
        CHECK(war.started == uhrwerk::Date{1648, 10, 1});
        CHECK(war.attackers == std::vector<Index>{bra});
        CHECK(war.defenders == std::vector<Index>{sax});
        CHECK(war.goals.size() == 2);
        CHECK(war.attacker_strength >= Fixed::from_int(5000));
        CHECK(std::any_of(v.treaties.begin(), v.treaties.end(), [&](const uhrwerk::Treaty& t) {
            return t.kind == uhrwerk::TreatyKind::Alliance && t.from == bra && t.to == anh;
        }));
        CHECK(chronicle_contains(v, "Brandenburg erklärt Sachsen den Krieg"));
    }

    SUBCASE("Hinterland: Ersatz, Einnahmen und Kriegspunkte") {
        Simulation sim(start);
        const World& v = sim.world();
        while (v.date.day != 1) {
            sim.step_day();
        }
        // Am Monatsersten trifft der Ersatz in Leipzig ein (erster Ort am Kartenrand).
        sim.step_day();
        const uhrwerk::Army* replacement = army_named(v, "Ersatz Leipzig");
        REQUIRE(replacement != nullptr);
        CHECK(replacement->state == sax);
        CHECK(replacement->soldiers == Fixed::from_int(500));
        // Das Hinterland zählt bei der Besetzung mit, die Hauptstadt auf der Karte nicht.
        World occupied = v;
        for (std::size_t l = 0; l < occupied.locations.size(); ++l) {
            if (occupied.locations[l].owner == sax) {
                occupied.control[l] = bra;
            }
        }
        World plain = occupied;
        plain.hinterlands.clear();
        const Fixed with = uhrwerk::war_score(occupied, occupied.wars[0], true);
        const Fixed without = uhrwerk::war_score(plain, plain.wars[0], true);
        CHECK(with < without);
        CHECK(uhrwerk::war_weariness(occupied, occupied.wars[0], false) <
              uhrwerk::war_weariness(plain, plain.wars[0], false));
    }

    SUBCASE("Führung in der Schlacht") {
        const auto inflicted = [&](std::string_view leadership) {
            const MilitaryData d;
            d.add_rules("    fuehrung = 0.5\n");
            std::string text(kDeployment);
            testhilfen::write_file(d.dir() / "common/aufmarsch_westfaelischer_frieden.chron", text);
            testhilfen::replace_in_file(d.dir() / "common/aufmarsch_westfaelischer_frieden.chron",
                                        "fuehrung = 2", "fuehrung = " + std::string(leadership));
            testhilfen::replace_in_file(d.dir() / "common/aufmarsch_westfaelischer_frieden.chron",
                                        "soldaten = 5000", "soldaten = 1000");
            Simulation sim(with_strength(d.load(), 10, 40));
            const uhrwerk::Army* first = army_named(sim.world(), "I. Armee");
            REQUIRE(first != nullptr);
            sim.submit(command(CommandType::March, bra, first->id, sim.world().find_location("dresden")));
            sim.step_days(12);
            const World& v = sim.world();
            if (!v.wars.empty()) {
                return v.wars[0].attacker_inflicted;
            }
            REQUIRE(v.war_results.size() == 1); // Frieden am Monatsersten
            return v.war_results[0].defender_losses;
        };
        const Fixed weak = inflicted("1");
        CHECK(weak > Fixed::zero());
        CHECK(inflicted("3") > weak);
    }

    SUBCASE("Frieden: Aufmarschheere lösen sich auf") {
        Simulation sim(start);
        const World& v = sim.world();
        sim.submit(peace(bra, uhrwerk::PeaceTerms::Capitulation));
        sim.step_days(40);
        CHECK(v.wars.empty());
        CHECK(army_named(v, "I. Armee") == nullptr);
        CHECK(uhrwerk::mobilization_phase(v, bra) == MobilizationPhase::Peace);
        CHECK(std::all_of(v.armies.begin(), v.armies.end(),
                          [](const uhrwerk::Army& a) { return a.garrison != uhrwerk::kNone; }));
    }

    SUBCASE("Spielstand und Wiederholung") {
        Simulation full(start);
        full.step_days(60);
        Simulation part(start);
        part.step_days(20);
        const Bytes bytes = uhrwerk::save_game(part, SaveMeta{});
        uhrwerk::LoadedGame loaded = uhrwerk::load_game(bytes, start, 2);
        INFO(loaded.error);
        REQUIRE(loaded.ok());
        CHECK(loaded.simulation->world().armies == part.world().armies);
        loaded.simulation->step_days(40);
        CHECK(loaded.simulation->checksum() == full.checksum());
        const uhrwerk::ReplayResult replay = uhrwerk::run_replay(uhrwerk::make_replay(full), start, 2);
        INFO(replay.error);
        CHECK(replay.ok);
        // Ein anderer Aufmarsch ist anderer Inhalt.
        World other = start;
        other.deployments[0].soldiers = Fixed::from_int(6000);
        CHECK_FALSE(uhrwerk::load_game(bytes, other, 2).ok());
    }

    SUBCASE("Nur im eigenen Szenario") {
        const uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path(), "jahresbeginn_1650");
        INFO(testhilfen::issues_text(result));
        REQUIRE(result.ok());
        CHECK(result.world->deployments.empty());
        CHECK(result.world->opening_wars.empty());
        CHECK(result.world->hinterlands.size() == 1);
    }
}

TEST_CASE("Militär: Ladefehler im Aufmarsch") {
    const auto issues = [](std::string_view deployment, std::string_view hinterland = "") {
        const MilitaryData data;
        testhilfen::write_file(data.dir() / "common/aufmarsch_westfaelischer_frieden.chron", deployment);
        if (!hinterland.empty()) {
            testhilfen::write_file(data.dir() / "common/militaer.chron",
                                   testhilfen::read_file(data.dir() / "common/militaer.chron") +
                                       std::string(hinterland));
        }
        return uhrwerk::load_world(data.dir().path());
    };
    for (const auto& [text, message] : {
             std::pair{"heer a { staat = XXX name = \"A\" ort = potsdam soldaten = 10 }",
                       "unbekannter Staat 'XXX'"},
             std::pair{"heer a { staat = BRA name = \"A\" ort = nirgends soldaten = 10 }",
                       "unbekannter Ort 'nirgends'"},
             std::pair{"heer a { staat = BRA name = \"A\" ort = potsdam soldaten = 0 }", "'soldaten'"},
             std::pair{"heer a { staat = BRA name = \"A\" ort = potsdam soldaten = 10 fuehrung = 4 }",
                       "'fuehrung'"},
             std::pair{"heer a { staat = BRA name = \"A\" ort = potsdam soldaten = 10 }\n"
                       "heer a { staat = BRA name = \"B\" ort = potsdam soldaten = 10 }",
                       "heer a ist mehrfach vorhanden"},
             std::pair{"krieg k { name = \"K\" angreifer = { BRA } verteidiger = { BRA } }",
                       "steht auf beiden Seiten"},
             std::pair{"krieg k { name = \"K\" angreifer = { BRA } verteidiger = { SAX } }\n"
                       "krieg l { name = \"L\" angreifer = { ANH } verteidiger = { SAX } }",
                       "führt schon einen anderen Krieg"},
             std::pair{
                 "krieg k { name = \"K\" beginn = 1700.01.01 angreifer = { BRA } verteidiger = { SAX } }",
                 "'beginn' liegt nach dem Startdatum"},
             std::pair{"krieg k { name = \"K\" anlass = laune angreifer = { BRA } verteidiger = { SAX } }",
                       "'anlass' in krieg k erwartet"},
             std::pair{
                 "krieg k { name = \"K\" angreifer = { BRA } verteidiger = { SAX } ziel = { beute = 1 } }",
                 "unbekanntes Kriegsziel 'beute'"},
             std::pair{"krieg k { name = \"K\" angreifer = { BRA } verteidiger = { SAX } ziel = { "
                       "entschaedigung = 30 } }",
                       "ganze Monatseinnahmen"},
             std::pair{"krieg k { name = \"K\" angreifer = { } verteidiger = { SAX } }",
                       "nennt keinen Staat"},
             std::pair{"buendnis { staaten = { BRA SAX ANH } }", "buendnis erwartet genau zwei Staaten"},
             std::pair{"garantie { von = BRA fuer = BRA }", "derselbe Staat"},
             std::pair{"krieg k { name = \"K\" angreifer = { BRA } verteidiger = { SAX } "
                       "besetzt = { gebiet = nirgends } }",
                       "'besetzt' in krieg k"},
             std::pair{"truppe a { }", "unbekannt"},
         }) {
        CAPTURE(text);
        const uhrwerk::LoadResult result = issues(text);
        INFO(testhilfen::issues_text(result));
        CHECK(testhilfen::has_issue(result, message, std::string_view(message) != "unbekannt"));
    }
    SUBCASE("Hinterland") {
        const uhrwerk::LoadResult result =
            issues("", "hinterland SAX { bevoelkerung = 1 einnahmen = 1 ersatz = 1 orte = { nirgends } }\n"
                       "hinterland SAX { bevoelkerung = 1 einnahmen = 1 ersatz = 1 orte = { dresden } }\n"
                       "hinterland XXX { bevoelkerung = 1 einnahmen = 1 ersatz = 1 orte = { dresden } }\n");
        INFO(testhilfen::issues_text(result));
        CHECK(testhilfen::has_issue(result, "unbekannter Ort 'nirgends' in hinterland SAX"));
        CHECK(testhilfen::has_issue(result, "hinterland SAX ist mehrfach vorhanden"));
        CHECK(testhilfen::has_issue(result, "unbekannter Staat 'XXX' in hinterland"));
    }
    SUBCASE("Aufmarsch ohne Militär") {
        const MilitaryData data;
        testhilfen::replace_in_file(data.dir() / "common/militaer.chron", "seit = 1648.01.01",
                                    "seit = 1700.01.01");
        testhilfen::write_file(data.dir() / "common/aufmarsch_westfaelischer_frieden.chron", "");
        const uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path());
        CHECK(testhilfen::has_issue(result, "Aufmarsch ohne Militär zum Startdatum"));
    }
}

TEST_CASE("Militär: Regeln der Kalibrierung (b2)") {
    using uhrwerk::WarGoalKind;

    SUBCASE("Einschließung: ein Heer in der Festung ergibt sich mit ihr") {
        const MilitaryData data;
        data.add_fortress(1);
        data.add_rules("    festung_verteidigung = 2\n    festung_besatzung = 10\n");
        Simulation sim(with_strength(data.load(), 30, 4));
        const World& v = sim.world();
        const Index bra = v.find_state("BRA");
        const Index sax = v.find_state("SAX");
        const Index dresden = v.find_location("dresden");
        brandenburg_attacks(sim, 5);
        INFO(lines([&] {
            std::vector<std::string> texts;
            for (const uhrwerk::ChronicleEntry& e : v.chronicle) {
                texts.push_back(e.text);
            }
            return texts;
        }()));
        // Die Garde übertrifft die Sachsen an Soldaten, die Festung schützt sie vor dem Sturm.
        CHECK_FALSE(chronicle_contains(v, "Schlacht bei Dresden"));
        REQUIRE(v.sieges.size() == 1);
        CHECK(uhrwerk::find_army(v, 1)->status == ArmyStatus::Siege);
        for (int day = 0; day < 200 && !v.sieges.empty(); ++day) {
            sim.step_day();
        }
        CHECK(chronicle_contains(v, "Die Festung Dresden ergibt sich Brandenburg. Mit ihr ergibt sich "
                                    "Garnison Dresden."));
        CHECK(std::none_of(v.armies.begin(), v.armies.end(),
                           [&](const uhrwerk::Army& a) { return a.state == sax && a.location == dresden; }));
        CHECK(std::any_of(v.prisoners.begin(), v.prisoners.end(),
                          [&](const uhrwerk::Prisoners& p) { return p.state == sax && p.captor == bra; }));
    }

    SUBCASE("Moral: Sieg und Erholung vor dem nächsten Befehl") {
        const auto morale_after_battle = [](std::string_view rules) {
            const MilitaryData data;
            data.add_rules(rules);
            Simulation sim(with_strength(data.load(), 30, 5));
            brandenburg_attacks(sim, 4);
            REQUIRE(chronicle_contains(sim.world(), "Schlacht bei Dresden: Garde schlägt"));
            return uhrwerk::find_army(sim.world(), 1)->morale;
        };
        CHECK(morale_after_battle("    moral_sieg = 0.3\n") > morale_after_battle(""));

        // Unter generalstab_moral rückt ein Heer nicht vor, es erholt sich erst.
        const auto marched = [](Fixed morale) {
            const MilitaryData data;
            data.add_rules("    generalstab = ja\n    generalstab_moral = 0.5\n    moral_erholung = 0\n");
            World w = with_strength(data.load(), 30, 5);
            const Index bra = w.find_state("BRA");
            Simulation sim(w);
            sim.submit(command(CommandType::DeclareWar, bra,
                               goal(WarGoalKind::Annexation, w.find_state("SAX")), w.find_state("SAX")));
            sim.step_day();
            World started = sim.world();
            for (uhrwerk::Army& a : started.armies) {
                if (a.state == bra) {
                    a.morale = morale;
                }
            }
            Simulation next(started);
            next.step_days(3);
            const uhrwerk::Army* garde = uhrwerk::find_army(next.world(), 1);
            return garde->status == ArmyStatus::March || garde->location != started.armies[0].location;
        };
        CHECK(marched(Fixed::one()));
        CHECK_FALSE(marched(Fixed::from_ratio(3, 10)));
    }

    SUBCASE("Kosten der Annexion nach dem Anteil an der Gegenseite") {
        const MilitaryData data;
        World w = Simulation(data.load()).world();
        const Index bra = w.find_state("BRA");
        const Index sax = w.find_state("SAX");
        const Index anh = w.find_state("ANH");
        REQUIRE(uhrwerk::declare_war(w, bra, sax, goal(WarGoalKind::Annexation, sax)));
        uhrwerk::War& war = w.wars[0];
        CHECK(uhrwerk::war_goal_cost(w, war, war.goals[0]) == Fixed::from_int(80)); // die ganze Gegenseite
        war.defenders.push_back(anh);
        Fixed saxony;
        Fixed anhalt;
        for (std::size_t l = 0; l < w.locations.size(); ++l) {
            const Fixed people = w.location_population(static_cast<Index>(l));
            if (w.locations[l].owner == sax) {
                saxony += people;
            } else if (w.locations[l].owner == anh) {
                anhalt += people;
            }
        }
        const uhrwerk::WarGoal annex_anhalt{WarGoalKind::Annexation, true, anh};
        CHECK(uhrwerk::war_goal_cost(w, war, annex_anhalt) ==
              Fixed::max(Fixed::from_int(10), Fixed::from_int(80) * anhalt / (saxony + anhalt)));
    }

    SUBCASE("Frieden, wenn die Müdigkeit der Gegenseite die Kosten der Ziele erreicht") {
        const MilitaryData data;
        World w = Simulation(data.load()).world();
        const Index bra = w.find_state("BRA");
        const Index sax = w.find_state("SAX");
        REQUIRE(uhrwerk::declare_war(w, bra, sax, goal(WarGoalKind::Indemnity, 1)));
        // Sechs Monate Krieg ohne Schlacht: Müdigkeit 12 ≥ Kosten 10, Kriegspunkte null.
        w.wars[0].started = uhrwerk::from_day_number(uhrwerk::to_day_number(w.date) - 185);
        REQUIRE(uhrwerk::war_score(w, w.wars[0], true) == Fixed::zero());
        REQUIRE(uhrwerk::war_weariness(w, w.wars[0], false) >= Fixed::from_int(10));
        // Vorab-Prüfung (seit b3): Sachsen nähme Brandenburgs Ziele an; ungültig ohne Ziele oder Krieg.
        CHECK(uhrwerk::peace_acceptance(w, bra, 1, uhrwerk::PeaceTerms::OwnGoals) ==
              std::optional<bool>(true));
        CHECK(uhrwerk::peace_acceptance(w, sax, 1, uhrwerk::PeaceTerms::OwnGoals) == std::nullopt);
        CHECK(uhrwerk::peace_acceptance(w, bra, 2, uhrwerk::PeaceTerms::White) == std::nullopt);
        CHECK(uhrwerk::peace_acceptance(w, sax, 1, uhrwerk::PeaceTerms::Capitulation) ==
              std::optional<bool>(true));
        CHECK(uhrwerk::war_goal_text(w, w.wars[0], w.wars[0].goals[0]) ==
              "Entschädigung von 1 Monatseinnahmen");
        Simulation sim(w);
        while (sim.world().date.day != 1) {
            sim.step_day();
        }
        sim.step_day();
        const World& v = sim.world();
        CHECK(v.wars.empty());
        REQUIRE(v.war_results.size() == 1);
        CHECK(v.war_results[0].winner == 1);
    }

    SUBCASE("Aufmarsch: besetzte Gebiete und ein Staat mit Hinterland verlässt die Karte") {
        const MilitaryData data;
        data.add_unit();
        std::string deployment(kDeployment);
        deployment.replace(deployment.find("    ziel_verteidiger"), 0,
                           "    besetzt = { gebiet = leipziger_kreis }\n");
        add_deployment(data, deployment);
        const World start = with_strength(data.load(), 10, 10);
        REQUIRE(start.opening_wars.size() == 1);
        CHECK(start.opening_wars[0].occupied == std::vector<Index>{start.find_admin_unit("leipziger_kreis")});
        Simulation sim(start);
        const World& v = sim.world();
        const Index bra = v.find_state("BRA");
        const Index sax = v.find_state("SAX");
        CHECK(v.control[at(v.find_location("leipzig"))] == bra);
        CHECK(v.control[at(v.find_location("dresden"))] == uhrwerk::kNone);
        // Sachsen unterwirft sich: Brandenburg annektiert es, Sachsen mit Hinterland verlässt die Karte;
        // Kasse und Schulden gehen nicht an Brandenburg.
        World indebted = v;
        indebted.states[at(sax)].debt = Fixed::from_int(123456);
        Simulation after(indebted);
        const Fixed debt = after.world().states[at(bra)].debt;
        after.submit(peace(sax, uhrwerk::PeaceTerms::Capitulation));
        after.step_day();
        CHECK(chronicle_contains(after.world(), "Sachsen verlässt die Karte."));
        CHECK_FALSE(chronicle_contains(after.world(), "Sachsen geht unter."));
        CHECK(after.world().states[at(bra)].debt < debt + Fixed::from_int(123456));
    }
}
