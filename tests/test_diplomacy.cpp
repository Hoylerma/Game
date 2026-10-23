// Tests der Diplomatie (seit M9, Schritt b1; docs/technik/diplomatie.md Abschnitte 4.1–4.3).
#include "test_hilfen.hpp"
#include "uhrwerk/customs.hpp"
#include "uhrwerk/diplomacy.hpp"
#include "uhrwerk/savegame.hpp"
#include "uhrwerk/simulation.hpp"

#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <string_view>
#include <vector>

namespace {

using uhrwerk::Command;
using uhrwerk::CommandType;
using uhrwerk::Date;
using uhrwerk::DiplomacyDecision;
using uhrwerk::Fixed;
using uhrwerk::Index;
using uhrwerk::SaveMeta;
using uhrwerk::SimConfig;
using uhrwerk::Simulation;
using uhrwerk::TreatyAction;
using uhrwerk::TreatyKind;
using uhrwerk::World;
using Bytes = std::vector<std::uint8_t>;

Fixed fx(std::int64_t value) {
    return Fixed::from_int(value);
}

/// Fixture mit Deutschem Bund: Jeder Ort mit Kreis liegt im Bundesgebiet, die Grundbeziehungen
/// Brandenburg–Sachsen, Brandenburg–Anhalt und Sachsen–Anhalt sind wählbar. Alle drei Herrscherhäuser
/// sind evangelisch (lutherisch oder reformiert): Bund +10, Konfession +10.
class DiplomacyData {
public:
    DiplomacyData(int bra_sax, int bra_anh, int sax_anh, bool federation = true) : dir_(true) {
        if (federation) {
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
        }
        std::string relations;
        const auto add = [&](std::string_view a, std::string_view b, int value) {
            relations += "beziehung {\n    staaten = { " + std::string(a) + " " + std::string(b) +
                         " }\n    wert = " + std::to_string(value) + "\n    quelle = \"Testdaten\"\n}\n";
        };
        add("BRA", "SAX", bra_sax);
        add("BRA", "ANH", bra_anh);
        add("SAX", "ANH", sax_anh);
        testhilfen::write_file(dir_ / "history/beziehungen/grundwerte.chron", relations);
    }

    /// Bundesversammlung (seit Schritt b2) mit Plenum und Engerem Rat, Präsidium Sachsen.
    void write_diet(std::string_view plenum, std::string_view council) const {
        testhilfen::write_file(
            dir_ / "common/bundesversammlung.chron",
            "bundesversammlung bund {\n    name = \"Bundesversammlung\"\n    praesidium = SAX\n"
            "    plenum = { " +
                std::string(plenum) + " }\n    rat = { " + std::string(council) +
                " }\n    quelle = \"Testdaten\"\n}\n");
    }

    /// Zollunion „Mittelbund“ mit Sachsen als Leitmacht und einzigem Mitglied.
    void add_customs_union() const {
        testhilfen::write_file(dir_ / "common/zollunionen.chron",
                               "zollunion mittelbund {\n    name = \"Mittelbund\"\n    leitung = SAX\n"
                               "    tarif = 0.05\n    quelle = \"Testdaten\"\n}\n");
        testhilfen::replace_in_file(dir_ / "history/staaten/20_mitteldeutschland.chron",
                                    "    heer           = 6\n",
                                    "    heer           = 6\n    zollunion      = mittelbund\n");
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

const std::string& last_chronicle(const Simulation& sim) {
    REQUIRE_FALSE(sim.world().chronicle.empty());
    return sim.world().chronicle.back().text;
}

Command envoy_command(Index state, Index target, bool send) {
    Command c;
    c.type = CommandType::Envoy;
    c.state = state;
    c.target = target;
    c.value = send ? Fixed::one() : Fixed::zero();
    return c;
}

Command treaty_command(Index state, Index target, TreatyAction action) {
    Command c;
    c.type = CommandType::Treaty;
    c.state = state;
    c.target = target;
    c.value = fx(static_cast<std::int64_t>(action));
    return c;
}

/// Bis zum nächsten Monatsersten rechnen (einschließlich: dessen Schritt ist gerechnet).
void step_past_month_start(Simulation& sim) {
    do {
        sim.step_day();
    } while (sim.world().date.day != 2);
}

struct States {
    Index bra;
    Index sax;
    Index anh;
};

States states_of(const World& w) {
    return {w.find_state("BRA"), w.find_state("SAX"), w.find_state("ANH")};
}

} // namespace

TEST_CASE("Diplomatie: nur mit Deutschem Bund") {
    const World plain = DiplomacyData(-40, 40, 0, false).load();
    CHECK_FALSE(uhrwerk::diplomacy_active(plain));
    Simulation sim(plain);
    CHECK(sim.world().relations.empty());
    const States s = states_of(sim.world());
    CHECK(uhrwerk::relation(sim.world(), s.bra, s.sax) == Fixed::zero());
    CHECK(uhrwerk::evaluate_envoy(sim.world(), s.bra, s.sax, true) == DiplomacyDecision::Invalid);
    sim.submit(envoy_command(s.bra, s.sax, true));
    sim.step_days(40);
    CHECK(sim.world().envoys.empty());
    CHECK(sim.world().relations.empty());
    // Ohne Bund dieselbe Prüfsumme wie die unveränderte Fixture: Der Zustand der Diplomatie wird nicht
    // besucht, die Grundwerte zählen nicht zum Inhalt.
    Simulation reference(testhilfen::load_fixture());
    reference.step_days(40);
    CHECK(reference.checksum() == sim.checksum());
}

TEST_CASE("Diplomatie: Beziehungen, Zielwerte und Drift") {
    const World w = DiplomacyData(-60, 40, 0).load();
    REQUIRE(uhrwerk::diplomacy_active(w));
    REQUIRE(w.base_relations.size() == 3);
    CHECK(w.diplomacy_rules.drift == Fixed::from_ratio(5, 100));
    Simulation sim(w);
    const World& v = sim.world();
    const States s = states_of(v);
    REQUIRE(v.relations.size() == uhrwerk::relation_count(3));
    CHECK(uhrwerk::relation_count(38) == 703);
    // Slots: (0,1) (0,2) (1,2), symmetrisch.
    CHECK(uhrwerk::relation_slot(3, 0, 1) == 0);
    CHECK(uhrwerk::relation_slot(3, 2, 0) == 1);
    CHECK(uhrwerk::relation_slot(3, 1, 2) == 2);
    CHECK(uhrwerk::relation_slot(38, 36, 37) == 702);
    // Zum Start auf dem Zielwert: Grundwert + Bund 10 + evangelisch 10.
    CHECK(uhrwerk::relation(v, s.bra, s.sax) == fx(-40));
    CHECK(uhrwerk::relation(v, s.sax, s.bra) == fx(-40));
    CHECK(uhrwerk::relation(v, s.bra, s.anh) == fx(60));
    CHECK(uhrwerk::relation(v, s.sax, s.anh) == fx(20));
    CHECK(uhrwerk::relation(v, s.bra, s.bra) == Fixed::zero());
    CHECK(uhrwerk::relation_target(v, s.bra, s.anh) == fx(60));
    CHECK(uhrwerk::rivals(v, s.bra, s.sax));
    CHECK_FALSE(uhrwerk::rivals(v, s.sax, s.anh));

    // Eine Garantie (ab 50 angenommen) hebt den Zielwert um 20; die Beziehung folgt mit 5 % je Monat.
    sim.submit(treaty_command(s.bra, s.anh, TreatyAction::Guarantee));
    sim.step_day();
    CHECK(last_chronicle(sim) == "Brandenburg garantiert den Bestand von Anhalt.");
    CHECK(uhrwerk::guarantees(v, s.bra, s.anh));
    CHECK_FALSE(uhrwerk::guarantees(v, s.anh, s.bra));
    CHECK(uhrwerk::relation_target(v, s.bra, s.anh) == fx(80));
    // Alle Zielwerte eines Staates in einem Durchgang (Anzeige), gleich den einzelnen.
    const std::vector<Fixed> targets = uhrwerk::relation_targets(v, s.bra);
    REQUIRE(targets.size() == 3);
    CHECK(targets[static_cast<std::size_t>(s.anh)] == fx(80));
    CHECK(targets[static_cast<std::size_t>(s.sax)] == uhrwerk::relation_target(v, s.bra, s.sax));
    CHECK(targets[static_cast<std::size_t>(s.bra)] == Fixed::zero());
    CHECK(uhrwerk::relation_targets(v, uhrwerk::kNone).empty());
    const Fixed drift = v.diplomacy_rules.drift;
    step_past_month_start(sim);
    const Fixed first = fx(60) + (fx(80) - fx(60)) * drift;
    CHECK(uhrwerk::relation(v, s.bra, s.anh) == first);
    CHECK(first.round() == 61);
    step_past_month_start(sim);
    CHECK(uhrwerk::relation(v, s.bra, s.anh) == first + (fx(80) - first) * drift);
    CHECK(uhrwerk::relation(v, s.bra, s.sax) == fx(-40)); // ohne Änderung bleibt sie stehen

    // Sachsen garantiert Anhalt nicht: Beziehung 20 liegt unter 50.
    sim.submit(treaty_command(s.sax, s.anh, TreatyAction::Guarantee));
    sim.step_day();
    CHECK(last_chronicle(sim) == "Anhalt lehnt eine Garantie von Sachsen ab.");
    CHECK_FALSE(uhrwerk::guarantees(v, s.sax, s.anh));
}

TEST_CASE("Diplomatie: Gesandte mit Kapazität, Wirkung und Kosten") {
    Simulation sim(DiplomacyData(-60, 40, 0).load());
    const World& v = sim.world();
    const States s = states_of(v);
    CHECK(uhrwerk::envoy_capacity(v, s.bra) == 1); // weit unter 5 Mio. Einwohnern
    CHECK(uhrwerk::evaluate_envoy(v, s.bra, s.bra, true) == DiplomacyDecision::Invalid);
    CHECK(uhrwerk::evaluate_envoy(v, s.bra, s.sax, false) == DiplomacyDecision::Invalid);
    CHECK(uhrwerk::evaluate_envoy(v, s.bra, s.sax, true) == DiplomacyDecision::Accepted);

    sim.submit(envoy_command(s.bra, s.sax, true));
    sim.submit(envoy_command(s.bra, s.anh, true)); // Kapazität erschöpft
    sim.submit(envoy_command(s.bra, s.sax, true)); // schon dort
    sim.step_day();
    REQUIRE(v.envoys.size() == 1);
    CHECK(uhrwerk::has_envoy(v, s.bra, s.sax));
    CHECK_FALSE(uhrwerk::has_envoy(v, s.sax, s.bra));
    CHECK(last_chronicle(sim) == "Brandenburg entsendet einen Gesandten nach Sachsen.");
    CHECK(uhrwerk::envoy_count(v, s.bra) == 1);

    // +5 je Monat über den Zielwert hinaus, höchstens 60 darüber (−40 + 60 = 20).
    step_past_month_start(sim);
    CHECK(uhrwerk::relation(v, s.bra, s.sax) == fx(-35));
    for (int month = 0; month < 30; ++month) {
        step_past_month_start(sim);
    }
    CHECK(uhrwerk::relation(v, s.bra, s.sax) <= fx(20));
    CHECK(uhrwerk::relation(v, s.bra, s.sax) > fx(0));
    CHECK(uhrwerk::relation_target(v, s.bra, s.sax) == fx(-40));

    // Kosten: 0,5 % der Monatseinnahmen, als eigener Posten in den Ausgaben.
    const uhrwerk::State& brandenburg = v.states[static_cast<std::size_t>(s.bra)];
    CHECK(brandenburg.budget.diplomacy == brandenburg.budget.income() * v.diplomacy_rules.envoy_cost);
    CHECK(brandenburg.budget.diplomacy > Fixed::zero());
    CHECK(brandenburg.last_month_expenses == brandenburg.budget.expenses());
    CHECK(v.states[static_cast<std::size_t>(s.sax)].budget.diplomacy == Fixed::zero());

    // Abberufen: ohne Gesandten driftet die Beziehung zurück zum Zielwert.
    sim.submit(envoy_command(s.bra, s.sax, false));
    sim.step_day();
    CHECK(v.envoys.empty());
    CHECK(last_chronicle(sim) == "Brandenburg ruft seinen Gesandten aus Sachsen ab.");
    const Fixed before = uhrwerk::relation(v, s.bra, s.sax);
    step_past_month_start(sim);
    CHECK(uhrwerk::relation(v, s.bra, s.sax) < before);
}

TEST_CASE("Diplomatie: Bündnisse nach Regel, Lager und Kündigung") {
    // Brandenburg–Anhalt 110, Sachsen–Anhalt 110, Brandenburg–Sachsen −60 (Gegner).
    Simulation sim(DiplomacyData(-80, 90, 90).load());
    const World& v = sim.world();
    const States s = states_of(v);
    REQUIRE(uhrwerk::rivals(v, s.bra, s.sax));
    CHECK(uhrwerk::evaluate_treaty(v, s.bra, s.sax, TreatyAction::Alliance) == DiplomacyDecision::Rejected);
    CHECK(uhrwerk::evaluate_treaty(v, s.bra, s.anh, TreatyAction::CancelAlliance) ==
          DiplomacyDecision::Invalid);

    sim.submit(treaty_command(s.bra, s.anh, TreatyAction::Alliance));
    sim.step_day();
    CHECK(last_chronicle(sim) == "Brandenburg und Anhalt schließen ein Bündnis.");
    CHECK(uhrwerk::allied(v, s.bra, s.anh));
    CHECK(uhrwerk::allied(v, s.anh, s.bra));
    CHECK(uhrwerk::relation_target(v, s.bra, s.anh) == fx(150));
    // Ein Bündnis schließt eine Garantie zwischen denselben Staaten aus, ein zweites Bündnis auch.
    CHECK(uhrwerk::evaluate_treaty(v, s.anh, s.bra, TreatyAction::Guarantee) == DiplomacyDecision::Invalid);
    CHECK(uhrwerk::evaluate_treaty(v, s.anh, s.bra, TreatyAction::Alliance) == DiplomacyDecision::Invalid);

    // Anhalt ist mit Sachsens Gegner verbündet: kein Bündnis Sachsen–Anhalt, in beiden Richtungen.
    sim.submit(treaty_command(s.sax, s.anh, TreatyAction::Alliance));
    sim.step_day();
    CHECK(last_chronicle(sim) == "Anhalt lehnt ein Bündnis mit Sachsen ab.");
    CHECK(uhrwerk::evaluate_treaty(v, s.anh, s.sax, TreatyAction::Alliance) == DiplomacyDecision::Rejected);

    // Nach der Kündigung (durch einen der beiden) ist der Weg frei.
    sim.submit(treaty_command(s.anh, s.bra, TreatyAction::CancelAlliance));
    sim.step_day();
    CHECK(last_chronicle(sim) == "Anhalt kündigt das Bündnis mit Brandenburg.");
    CHECK(v.treaties.empty());
    sim.submit(treaty_command(s.sax, s.anh, TreatyAction::Alliance));
    sim.step_day();
    CHECK(uhrwerk::allied(v, s.sax, s.anh));

    // Garantie und ihre Kündigung durch den Garanten.
    sim.submit(treaty_command(s.bra, s.anh, TreatyAction::Guarantee));
    sim.submit(treaty_command(s.anh, s.bra, TreatyAction::CancelGuarantee)); // Anhalt garantiert nichts
    sim.step_day();
    CHECK(uhrwerk::guarantees(v, s.bra, s.anh));
    sim.submit(treaty_command(s.bra, s.anh, TreatyAction::CancelGuarantee));
    sim.step_day();
    CHECK(last_chronicle(sim) == "Brandenburg kündigt die Garantie für Anhalt.");
    CHECK_FALSE(uhrwerk::guarantees(v, s.bra, s.anh));

    // Ungültige Vertragshandlungen bleiben ohne Wirkung und Chronik.
    const std::size_t entries = v.chronicle.size();
    Command odd = treaty_command(s.bra, s.anh, TreatyAction::Guarantee);
    odd.value = Fixed::from_ratio(3, 2);
    sim.submit(odd);
    odd.value = fx(4);
    sim.submit(odd);
    odd.value = fx(-1);
    sim.submit(odd);
    sim.submit(treaty_command(s.bra, 7, TreatyAction::Guarantee));
    sim.step_day();
    CHECK(v.chronicle.size() == entries);
    CHECK_FALSE(uhrwerk::guarantees(v, s.bra, s.anh));
}

TEST_CASE("Diplomatie: Spielstand, Wiederholung und Threads mit Gesandten und Verträgen") {
    const DiplomacyData data(-80, 90, 90);
    const auto run = [&](unsigned threads, std::int64_t days) {
        SimConfig config;
        config.threads = threads;
        Simulation sim(data.load(), config);
        const States s = states_of(sim.world());
        sim.submit(envoy_command(s.sax, s.bra, true));
        sim.submit(treaty_command(s.bra, s.anh, TreatyAction::Alliance));
        sim.submit(treaty_command(s.sax, s.anh, TreatyAction::Guarantee));
        sim.step_days(days);
        return sim;
    };
    const Simulation one = run(1, 45);
    REQUIRE(one.world().envoys.size() == 1);
    REQUIRE(one.world().treaties.size() == 2);
    const Bytes bytes = uhrwerk::save_game(one, SaveMeta{});
    uhrwerk::LoadedGame loaded = uhrwerk::load_game(bytes, data.load(), 2);
    INFO(loaded.error);
    REQUIRE(loaded.ok());
    CHECK(loaded.simulation->checksum() == one.checksum());
    CHECK(loaded.simulation->world().relations == one.world().relations);
    CHECK(loaded.simulation->world().envoys == one.world().envoys);
    CHECK(loaded.simulation->world().treaties == one.world().treaties);

    Simulation continued = one;
    continued.step_days(90);
    loaded.simulation->step_days(90);
    CHECK(loaded.simulation->checksum() == continued.checksum());
    CHECK(run(4, 135).checksum() == continued.checksum());

    const uhrwerk::Replay replay = uhrwerk::make_replay(continued, {});
    const uhrwerk::ReplayResult result = uhrwerk::run_replay(replay, data.load(), 1);
    INFO(result.error);
    CHECK(result.ok);
}

TEST_CASE("Diplomatie: Ladefehler") {
    SUBCASE("Paar doppelt, unbekannter Staat, Wert außerhalb") {
        DiplomacyData data(-40, 40, 0);
        std::string text = testhilfen::read_file(data.dir() / "history/beziehungen/grundwerte.chron");
        text += "beziehung {\n    staaten = { SAX BRA }\n    wert = 5\n    quelle = \"x\"\n}\n"
                "beziehung {\n    staaten = { XYZ BRA }\n    wert = 5\n    quelle = \"x\"\n}\n"
                "beziehung {\n    staaten = { SAX SAX }\n    wert = 5\n    quelle = \"x\"\n}\n"
                "beziehung {\n    staaten = { SAX ANH }\n    wert = 300\n    quelle = \"x\"\n}\n";
        testhilfen::write_file(data.dir() / "history/beziehungen/grundwerte.chron", text);
        const uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path());
        INFO(testhilfen::issues_text(result));
        CHECK_FALSE(result.ok());
        CHECK(testhilfen::has_issue(result, "beziehung SAX–BRA ist doppelt"));
        CHECK(testhilfen::has_issue(result, "unbekannter Staat 'XYZ' in beziehung"));
        CHECK(testhilfen::has_issue(result, "zwei verschiedene Staaten"));
        CHECK(testhilfen::has_issue(result, "wert"));
    }
    SUBCASE("Bund ohne Regeln") {
        DiplomacyData data(-40, 40, 0);
        std::filesystem::remove(data.dir() / "common/diplomatie.chron");
        const uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path());
        CHECK_FALSE(result.ok());
        CHECK(
            testhilfen::has_issue(result, "Szenarien mit Deutschem Bund brauchen die Regeln der Diplomatie"));
    }
    SUBCASE("Bundesversammlung") {
        DiplomacyData data(-40, 40, 0);
        data.write_diet("BRA = 4 SAX = 4 XYZ = 1 ANH = 0 BRA = 2",
                        "stimme = { staaten = { BRA } } stimme = { staaten = { SAX ANH } } "
                        "stimme = { name = \"Doppelt\" staaten = { BRA } }");
        const uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path());
        INFO(testhilfen::issues_text(result));
        CHECK_FALSE(result.ok());
        CHECK(testhilfen::has_issue(result, "unbekannter Staat 'XYZ' in 'plenum'"));
        CHECK(testhilfen::has_issue(result, "ungültiger Eintrag in 'plenum'"));
        CHECK(testhilfen::has_issue(result, "Staat BRA steht zweimal in 'plenum'"));
        CHECK(testhilfen::has_issue(result, "eine Kuriatstimme braucht einen Namen"));
        CHECK(testhilfen::has_issue(result, "Staat BRA hat mehr als eine Stimme im Engeren Rat"));
    }
    SUBCASE("Konfessionsgruppen") {
        DiplomacyData data(-40, 40, 0);
        testhilfen::replace_in_file(data.dir() / "common/diplomatie.chron", "{ lutherisch reformiert }",
                                    "{ lutherisch reformiert lutherisch heidnisch }");
        const uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path());
        CHECK(testhilfen::has_issue(result, "steht in mehreren Gruppen"));
        CHECK(testhilfen::has_issue(result, "unbekannte Konfession 'heidnisch'"));
    }
}

// ─── Bundesversammlung (seit Schritt b2, Abschnitt 4.4) ────────────────────

namespace {

Command motion_command(Index applicant, uhrwerk::MotionKind kind, Index target = uhrwerk::kNone) {
    Command c;
    c.type = CommandType::Motion;
    c.state = applicant;
    c.target = target;
    c.value = fx(static_cast<std::int64_t>(kind));
    return c;
}

Command demand_command(Index demander, Index target, uhrwerk::DemandKind kind) {
    Command c;
    c.type = CommandType::Demand;
    c.state = demander;
    c.target = target;
    c.value = fx(static_cast<std::int64_t>(kind));
    return c;
}

Command customs_command(Index state, Index target) {
    Command c;
    c.type = CommandType::CustomsUnion;
    c.state = state;
    c.target = target;
    return c;
}

Command manoeuvre_command(Index state, uhrwerk::Manoeuvre manoeuvre) {
    Command c;
    c.type = CommandType::Manoeuvre;
    c.state = state;
    c.value = fx(static_cast<std::int64_t>(manoeuvre));
    return c;
}

/// Bis zum Schritt eines Monatsersten einschließlich rechnen.
void step_through_month_start(Simulation& sim) {
    do {
        sim.step_day();
    } while (sim.world().date.day != 2);
}

bool chronicle_contains(const Simulation& sim, std::string_view text) {
    for (const uhrwerk::ChronicleEntry& e : sim.world().chronicle) {
        if (e.text.find(text) != std::string::npos) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("Bundesversammlung: Daten, Antrag und Abstimmung am Monatsersten") {
    using uhrwerk::MotionKind;
    // Brandenburg–Sachsen −40 (Gegner), Brandenburg–Anhalt 60, Sachsen–Anhalt 20.
    const DiplomacyData data(-60, 40, 0);
    SUBCASE("Bundesintervention im Engeren Rat mit Kurie") {
        data.write_diet("BRA = 4 SAX = 4 ANH = 1",
                        "stimme = { staaten = { BRA } } stimme = { name = \"Mitte\" staaten = { SAX ANH } }");
        Simulation sim(data.load());
        const World& v = sim.world();
        const States s = states_of(v);
        REQUIRE(uhrwerk::has_diet(v));
        CHECK(v.diet.presidency == s.sax);
        CHECK(v.diet_presidency == s.sax);
        CHECK(v.diet.seat == uhrwerk::kNone);
        CHECK(v.diet.council.size() == 2);
        CHECK(v.diet.council[0].name == "Brandenburg");
        CHECK(uhrwerk::plenum_votes(v, s.anh) == 1);
        CHECK(uhrwerk::motion_opponent(v, MotionKind::Intervention, s.bra, s.sax) == s.sax);
        CHECK(uhrwerk::motion_opponent(v, MotionKind::Reform, s.bra, uhrwerk::kNone) == s.sax);
        CHECK(uhrwerk::votes_yes(v, s.anh, s.bra, s.sax)); // 60 > 20
        CHECK_FALSE(uhrwerk::votes_yes(v, s.sax, s.bra, s.sax));
        CHECK(uhrwerk::evaluate_motion(v, s.bra, MotionKind::Intervention, s.bra) ==
              DiplomacyDecision::Invalid);
        CHECK(uhrwerk::evaluate_motion(v, s.sax, MotionKind::Presidency, uhrwerk::kNone) ==
              DiplomacyDecision::Invalid); // schon Präsidialmacht
        CHECK(uhrwerk::evaluate_motion(v, s.bra, MotionKind::CustomsUnification, 0) ==
              DiplomacyDecision::Invalid); // keine Zollunion

        sim.submit(motion_command(s.bra, MotionKind::Intervention, s.sax));
        sim.submit(motion_command(s.anh, MotionKind::Reform)); // schon ein Antrag offen
        sim.step_day();
        REQUIRE(v.motions.size() == 1);
        CHECK(last_chronicle(sim) ==
              "Brandenburg beantragt in der Bundesversammlung eine Bundesintervention gegen Sachsen; "
              "abgestimmt wird am 1. November 1648.");
        step_through_month_start(sim);
        CHECK(v.motions.empty());
        REQUIRE(v.motion_results.size() == 1);
        const uhrwerk::MotionResult& r = v.motion_results[0];
        CHECK(r.plenum_yes == 5);
        CHECK(r.plenum_total == 9);
        CHECK(r.council_yes == 1); // die Kurie stimmt mit Sachsens 4 gegen Anhalts 1 Stimme
        CHECK(r.council_total == 2);
        CHECK_FALSE(r.accepted);
        CHECK(chronicle_contains(
            sim, "Die Bundesversammlung lehnt den Antrag von Brandenburg auf eine "
                 "Bundesintervention gegen Sachsen ab (Engerer Rat: 1 von 2 Stimmen dafür)."));
    }
    SUBCASE("Bundesintervention angenommen, Antrag am Monatsersten erst im Folgemonat") {
        data.write_diet(
            "BRA = 4 SAX = 4 ANH = 1",
            "stimme = { staaten = { BRA } } stimme = { staaten = { SAX } } stimme = { staaten = { ANH } }");
        Simulation sim(data.load());
        const World& v = sim.world();
        const States s = states_of(v);
        while (v.date.day != 1) {
            sim.step_day();
        }
        sim.submit(motion_command(s.bra, MotionKind::Intervention, s.sax));
        sim.step_day(); // am Monatsersten gestellt: noch nicht abgestimmt
        CHECK(v.motions.size() == 1);
        CHECK(v.motion_results.empty());
        const Fixed before = uhrwerk::relation(v, s.anh, s.sax);
        step_through_month_start(sim);
        REQUIRE(v.motion_results.size() == 1);
        CHECK(v.motion_results[0].accepted);
        CHECK(v.motion_results[0].council_yes == 2);
        // Anhalt und Brandenburg stimmten dafür: −20 zu Sachsen, nach der monatlichen Drift.
        CHECK(uhrwerk::relation(v, s.anh, s.sax) < before - fx(15));
        CHECK(chronicle_contains(
            sim, "nimmt den Antrag von Brandenburg auf eine Bundesintervention gegen Sachsen an"));
    }
    SUBCASE("Bundesreform und Präsidium scheitern an der Einstimmigkeit") {
        data.write_diet("BRA = 4 SAX = 4 ANH = 1",
                        "stimme = { staaten = { BRA } } stimme = { staaten = { SAX ANH } "
                        "name = \"Mitte\" }");
        Simulation sim(data.load());
        const World& v = sim.world();
        const States s = states_of(v);
        sim.submit(motion_command(s.bra, MotionKind::Presidency));
        step_through_month_start(sim);
        REQUIRE(v.motion_results.size() == 1);
        CHECK_FALSE(v.motion_results[0].accepted);
        CHECK(v.diet_presidency == s.sax);
        CHECK(chronicle_contains(
            sim, "auf den Vorsitz im Bund ab (Plenum: 5 von 9 Stimmen dafür, Einstimmigkeit nötig)."));
    }
    SUBCASE("Zolleinigung mit zwei Dritteln") {
        DiplomacyData customs(-60, 40, 60);
        customs.add_customs_union();
        customs.write_diet(
            "BRA = 2 SAX = 4 ANH = 4",
            "stimme = { staaten = { BRA } } stimme = { staaten = { SAX } } stimme = { staaten = { ANH } }");
        Simulation sim(customs.load());
        const World& v = sim.world();
        const States s = states_of(v);
        // Gegner: Brandenburg (eigenes Zollgebiet); Anhalt steht Sachsen näher (80 gegen 60).
        CHECK(uhrwerk::motion_opponent(v, MotionKind::CustomsUnification, s.sax, 0) == s.bra);
        sim.submit(motion_command(s.sax, MotionKind::CustomsUnification, 0));
        step_through_month_start(sim);
        REQUIRE(v.motion_results.size() == 1);
        CHECK(v.motion_results[0].plenum_yes == 8);
        CHECK(v.motion_results[0].accepted);
        CHECK(v.customs_requests.size() == 2);
        CHECK(chronicle_contains(sim, "Alle Bundesstaaten treten zum 1. Januar 1649 dem Mittelbund bei."));
        // Der Beschluss ist verbindlich: Brandenburg kann den Beitritt nicht zurückziehen (Review M9).
        CHECK(uhrwerk::customs_request_binding(v, s.bra));
        CHECK(uhrwerk::evaluate_customs_change(v, s.bra, uhrwerk::kNone) ==
              uhrwerk::CustomsDecision::Invalid);
        sim.submit(customs_command(s.bra, uhrwerk::kNone));
        step_through_month_start(sim);
        CHECK(v.customs_requests.size() == 2);
        step_through_month_start(sim);
        step_through_month_start(sim); // 1. Januar 1649
        CHECK(v.states[static_cast<std::size_t>(s.bra)].customs_union == 0);
        CHECK(v.states[static_cast<std::size_t>(s.anh)].customs_union == 0);
        CHECK_FALSE(uhrwerk::customs_request_binding(v, s.bra));
    }
    SUBCASE("Zolleinigung am 1. Januar wird zum folgenden 1. Januar wirksam") {
        DiplomacyData customs(-60, 40, 60);
        customs.add_customs_union();
        customs.write_diet(
            "BRA = 2 SAX = 4 ANH = 4",
            "stimme = { staaten = { BRA } } stimme = { staaten = { SAX } } stimme = { staaten = { ANH } }");
        Simulation sim(customs.load());
        const World& v = sim.world();
        const States s = states_of(v);
        step_through_month_start(sim);
        step_through_month_start(sim); // 1. Dezember 1648
        sim.submit(motion_command(s.sax, MotionKind::CustomsUnification, 0));
        step_through_month_start(sim); // Abstimmung am 1. Januar 1649, nach dem Jahreswechsel
        REQUIRE(v.motion_results.size() == 1);
        CHECK(v.motion_results[0].date == uhrwerk::Date{1649, 1, 1});
        CHECK(chronicle_contains(sim, "Alle Bundesstaaten treten zum 1. Januar 1650 dem Mittelbund bei."));
        CHECK(v.states[static_cast<std::size_t>(s.bra)].customs_union == uhrwerk::kNone);
        CHECK(uhrwerk::customs_request_binding(v, s.bra));
    }
}

// ─── Diplomatische Spiele (seit Schritt b2, Abschnitt 4.5) ─────────────────

namespace {

/// Welt mit gesetzten Heeren (Soldaten je 1.000 Einwohner).
World with_armies(World w, std::int64_t bra, std::int64_t sax, std::int64_t anh) {
    const States s = states_of(w);
    w.states[static_cast<std::size_t>(s.bra)].army_per_mille = fx(bra);
    w.states[static_cast<std::size_t>(s.sax)].army_per_mille = fx(sax);
    w.states[static_cast<std::size_t>(s.anh)].army_per_mille = fx(anh);
    return w;
}

} // namespace

TEST_CASE("Diplomatische Spiele: Parteinahme, Stufen und Auflösung nach Stärke") {
    using uhrwerk::DemandKind;
    using uhrwerk::PlayOutcome;
    const DiplomacyData data(-60, 40, 0);
    SUBCASE("die schwächere Seite gibt nach der Mobilmachung nach") {
        Simulation sim(with_armies(data.load(), 10, 0, 0));
        const World& v = sim.world();
        const States s = states_of(v);
        CHECK(uhrwerk::evaluate_demand(v, s.bra, s.bra, DemandKind::Leadership) ==
              DiplomacyDecision::Invalid);
        CHECK(uhrwerk::evaluate_demand(v, s.bra, s.sax, DemandKind::CustomsUnion) ==
              DiplomacyDecision::Invalid); // ohne Zollunion
        CHECK(uhrwerk::evaluate_demand(v, s.bra, s.sax, DemandKind::BreakAlliance) ==
              DiplomacyDecision::Invalid); // Sachsen hat keine Bündnisse
        std::vector<Index> own;
        std::vector<Index> other;
        uhrwerk::play_sides(v, s.bra, s.sax, own, other);
        CHECK(own == std::vector<Index>{s.anh}); // Neigung 60 − 20 = 40 > 20
        CHECK(other.empty());

        sim.submit(demand_command(s.bra, s.sax, DemandKind::Leadership));
        sim.submit(demand_command(s.anh, s.sax, DemandKind::Leadership)); // Sachsen ist schon im Spiel
        sim.step_day();
        REQUIRE(v.plays.size() == 1);
        CHECK(uhrwerk::play_of(v, s.sax) == 0);
        CHECK(uhrwerk::play_of(v, s.anh) == uhrwerk::kNone);
        CHECK(chronicle_contains(sim,
                                 "Brandenburg fordert von Sachsen die Führung im Bund (Protest). Auf seiner "
                                 "Seite 1, auf der Gegenseite 0 Bundesstaaten."));
        // Einmalig −30 zwischen den Seiten.
        CHECK(uhrwerk::relation(v, s.bra, s.sax) == fx(-70));
        CHECK(uhrwerk::relation(v, s.anh, s.sax) == fx(-10));
        CHECK(uhrwerk::relation(v, s.bra, s.anh) == fx(60));

        sim.step_days(29);
        CHECK(v.plays.size() == 1);
        CHECK(v.plays[0].stage == 1);
        sim.step_day(); // Ende der ersten Stufe: Sachsen bietet Vermittlung an, Brandenburg lehnt ab
        REQUIRE(v.plays.size() == 1);
        CHECK(v.plays[0].stage == 2);
        CHECK(chronicle_contains(sim, "Sachsen bietet Vermittlung an; Brandenburg lehnt ab."));
        CHECK(chronicle_contains(sim, "Krise zwischen Brandenburg und Sachsen: Ultimatum."));
        sim.step_days(30);
        REQUIRE(v.plays.size() == 1);
        CHECK(v.plays[0].stage == 3);
        CHECK(uhrwerk::mobilized(v, s.bra));
        CHECK(uhrwerk::mobilized(v, s.sax));
        CHECK_FALSE(uhrwerk::mobilized(v, s.anh));
        const Fixed army = v.states[static_cast<std::size_t>(s.bra)].budget.army;
        sim.step_days(29); // ein Rechentag Brandenburgs liegt in der Mobilmachung
        const Fixed mobilized_army = v.states[static_cast<std::size_t>(s.bra)].budget.army;
        CHECK(mobilized_army > army * Fixed::from_ratio(14, 10));
        sim.step_day();
        CHECK(v.plays.empty());
        REQUIRE(v.play_results.size() == 1);
        CHECK(v.play_results[0].outcome == PlayOutcome::DemanderWon);
        CHECK(v.play_results[0].stage == 3);
        CHECK(chronicle_contains(sim, "Sachsen gibt nach: Brandenburg setzt die Führung im Bund durch."));
        CHECK_FALSE(uhrwerk::mobilized(v, s.bra));
    }
    SUBCASE("ähnlich starke Seiten einigen sich nach der ersten Stufe") {
        Simulation sim(with_armies(data.load(), 10, 0, 0));
        const States s = states_of(sim.world());
        // Sachsen knapp schwächer als Brandenburg, aber über zwei Dritteln.
        World& w = const_cast<World&>(sim.world());
        const Fixed brandenburg = uhrwerk::state_strength(w, s.bra);
        const Fixed people = w.state_population(s.sax);
        w.states[static_cast<std::size_t>(s.sax)].army_per_mille =
            brandenburg * Fixed::from_ratio(8, 10) * fx(1000) / people;
        sim.submit(demand_command(s.bra, s.sax, DemandKind::Leadership));
        sim.step_days(31);
        CHECK(sim.world().plays.empty());
        REQUIRE(sim.world().play_results.size() == 1);
        CHECK(sim.world().play_results[0].outcome == PlayOutcome::Compromise);
        CHECK(sim.world().play_results[0].stage == 1);
        CHECK(chronicle_contains(sim, "Sachsen bietet Vermittlung an; Brandenburg nimmt an."));
        CHECK(chronicle_contains(sim,
                                 "Die Krise zwischen Brandenburg und Sachsen endet mit einem Kompromiss."));
    }
}

TEST_CASE("Diplomatische Spiele: Manöver und Wirkungen der Forderungen") {
    using uhrwerk::DemandKind;
    using uhrwerk::Manoeuvre;
    using uhrwerk::PlayOutcome;
    SUBCASE("Nachgeben, abgelehnte Vermittlung, Eskalieren") {
        Simulation sim(with_armies(DiplomacyData(-60, 40, 0).load(), 10, 0, 0));
        const World& v = sim.world();
        const States s = states_of(v);
        CHECK(uhrwerk::evaluate_manoeuvre(v, s.sax, Manoeuvre::Yield) == DiplomacyDecision::Invalid);
        sim.submit(demand_command(s.bra, s.sax, DemandKind::Leadership));
        sim.step_day();
        CHECK(uhrwerk::evaluate_manoeuvre(v, s.sax, Manoeuvre::Mediate) == DiplomacyDecision::Rejected);
        sim.submit(manoeuvre_command(s.sax, Manoeuvre::Mediate));
        sim.submit(manoeuvre_command(s.sax, Manoeuvre::Escalate));
        sim.submit(manoeuvre_command(s.anh, Manoeuvre::Escalate)); // Anhalt ist keine Hauptpartei
        sim.step_day();
        CHECK(last_chronicle(sim) == "Sachsen bietet Vermittlung an; Brandenburg lehnt ab.");
        REQUIRE(v.plays.size() == 1);
        CHECK(v.plays[0].target_escalates);
        CHECK(uhrwerk::evaluate_manoeuvre(v, s.sax, Manoeuvre::Escalate) == DiplomacyDecision::Invalid);
        // Wer eskaliert, bietet am Ende der Stufe keine Vermittlung an.
        const std::size_t entries = v.chronicle.size();
        sim.step_days(29);
        REQUIRE(v.plays.size() == 1);
        CHECK(v.plays[0].stage == 2);
        CHECK_FALSE(v.plays[0].target_escalates);
        CHECK(v.chronicle.size() == entries + 1); // nur „Ultimatum“
        sim.submit(manoeuvre_command(s.sax, Manoeuvre::Yield));
        sim.step_day();
        CHECK(v.plays.empty());
        REQUIRE(v.play_results.size() == 1);
        CHECK(v.play_results[0].outcome == PlayOutcome::DemanderWon);
        CHECK(v.play_results[0].stage == 2);
    }
    SUBCASE("Beitritt zur Zollunion: Sieg und Kompromiss mit Zahlung") {
        DiplomacyData data(-60, 40, 0);
        data.add_customs_union();
        SUBCASE("Sieg") {
            Simulation sim(with_armies(data.load(), 0, 10, 0));
            const World& v = sim.world();
            const States s = states_of(v);
            sim.submit(demand_command(s.sax, s.bra, DemandKind::CustomsUnion));
            sim.step_day();
            REQUIRE(v.plays.size() == 1);
            CHECK(v.plays[0].customs_union == 0);
            sim.submit(manoeuvre_command(s.bra, Manoeuvre::Yield));
            sim.step_day();
            CHECK(chronicle_contains(
                sim, "Brandenburg gibt nach: Sachsen setzt den Beitritt zum Mittelbund durch."));
            CHECK(chronicle_contains(sim, "Brandenburg tritt zum 1. Januar 1649 dem Mittelbund bei."));
            REQUIRE(v.customs_requests.size() == 1);
            CHECK(v.customs_requests[0] == uhrwerk::CustomsRequest{s.bra, 0});
            // Der Ausgang ist verbindlich (Review M9).
            CHECK(uhrwerk::customs_request_binding(v, s.bra));
            CHECK(uhrwerk::evaluate_customs_change(v, s.bra, uhrwerk::kNone) ==
                  uhrwerk::CustomsDecision::Invalid);
            CHECK_FALSE(uhrwerk::customs_request_binding(v, s.sax));
        }
        SUBCASE("Kompromiss") {
            Simulation sim(data.load());
            const World& v = sim.world();
            const States s = states_of(v);
            sim.step_days(40); // ein Rechentag: Einnahmen des Vormonats
            const Fixed income = v.states[static_cast<std::size_t>(s.bra)].last_month_income;
            REQUIRE(income > Fixed::zero());
            sim.submit(demand_command(s.sax, s.bra, DemandKind::CustomsUnion));
            sim.step_day();
            World& w = const_cast<World&>(v);
            const Fixed sachsen = w.states[static_cast<std::size_t>(s.sax)].treasury;
            // Die stärkere Seite bekommt eine Vermittlung immer durch.
            if (uhrwerk::apply_manoeuvre(w, s.bra, Manoeuvre::Mediate) != DiplomacyDecision::Accepted) {
                REQUIRE(uhrwerk::apply_manoeuvre(w, s.sax, Manoeuvre::Mediate) ==
                        DiplomacyDecision::Accepted);
            }
            REQUIRE(v.play_results.size() == 1);
            CHECK(v.play_results[0].outcome == PlayOutcome::Compromise);
            CHECK(v.customs_requests.empty());
            CHECK(w.states[static_cast<std::size_t>(s.sax)].treasury ==
                  sachsen + v.states[static_cast<std::size_t>(s.bra)].last_month_income / fx(2));
            CHECK(chronicle_contains(sim, "Brandenburg zahlt Sachsen "));
        }
    }
    SUBCASE("Bündnis lösen") {
        // Sachsen–Anhalt 110 (Bündnis), Brandenburg–Sachsen −40 (Gegner).
        Simulation sim(with_armies(DiplomacyData(-60, 40, 90).load(), 10, 0, 0));
        const World& v = sim.world();
        const States s = states_of(v);
        sim.submit(treaty_command(s.sax, s.anh, TreatyAction::Alliance));
        sim.step_day();
        REQUIRE(uhrwerk::allied(v, s.sax, s.anh));
        std::vector<Index> own;
        std::vector<Index> other;
        uhrwerk::play_sides(v, s.bra, s.anh, own, other);
        CHECK(own.empty());
        CHECK(other == std::vector<Index>{s.sax}); // der Verbündete steht beim Ziel
        sim.submit(demand_command(s.bra, s.anh, DemandKind::BreakAlliance));
        sim.step_day();
        CHECK(chronicle_contains(sim,
                                 "Brandenburg fordert von Anhalt die Kündigung der Bündnisse mit Sachsen"));
        sim.submit(manoeuvre_command(s.anh, Manoeuvre::Yield));
        sim.step_day();
        CHECK_FALSE(uhrwerk::allied(v, s.sax, s.anh));
        CHECK(chronicle_contains(sim, "Anhalt kündigt das Bündnis mit Sachsen."));
    }
}

TEST_CASE("Bundesversammlung und Spiele: Spielstand, Wiederholung und Threads") {
    DiplomacyData data(-60, 40, 0);
    data.write_diet(
        "BRA = 4 SAX = 4 ANH = 1",
        "stimme = { staaten = { BRA } } stimme = { staaten = { SAX } } stimme = { staaten = { ANH } }");
    const auto run = [&](unsigned threads, std::int64_t days) {
        SimConfig config;
        config.threads = threads;
        Simulation sim(with_armies(data.load(), 10, 0, 0), config);
        const States s = states_of(sim.world());
        sim.submit(motion_command(s.anh, uhrwerk::MotionKind::Intervention, s.sax));
        sim.submit(demand_command(s.bra, s.sax, uhrwerk::DemandKind::Leadership));
        sim.step_days(days);
        return sim;
    };
    const Simulation one = run(1, 4);
    REQUIRE(one.world().plays.size() == 1);
    REQUIRE(one.world().motions.size() == 1);
    const Bytes bytes = uhrwerk::save_game(one, SaveMeta{});
    uhrwerk::LoadedGame loaded = uhrwerk::load_game(bytes, data.load(), 2);
    INFO(loaded.error);
    REQUIRE(loaded.ok());
    // Die Heere stehen im Spielstand; der Inhalt (Fixture ohne geänderte Heere) passt.
    CHECK(loaded.simulation->checksum() == one.checksum());
    CHECK(loaded.simulation->world().plays == one.world().plays);
    CHECK(loaded.simulation->world().motions == one.world().motions);

    Simulation continued = one;
    continued.step_days(100);
    loaded.simulation->step_days(100);
    CHECK(loaded.simulation->checksum() == continued.checksum());
    CHECK(continued.world().plays.empty());
    CHECK(continued.world().play_results.size() == 1);
    CHECK(continued.world().motion_results.size() == 1);
    CHECK(run(4, 104).checksum() == continued.checksum());
}
