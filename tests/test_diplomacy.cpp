// Tests der Diplomatie (seit M9, Schritt b1; docs/technik/diplomatie.md Abschnitte 4.1–4.3).
#include "test_hilfen.hpp"
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
    SUBCASE("Konfessionsgruppen") {
        DiplomacyData data(-40, 40, 0);
        testhilfen::replace_in_file(data.dir() / "common/diplomatie.chron", "{ lutherisch reformiert }",
                                    "{ lutherisch reformiert lutherisch heidnisch }");
        const uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path());
        CHECK(testhilfen::has_issue(result, "steht in mehreren Gruppen"));
        CHECK(testhilfen::has_issue(result, "unbekannte Konfession 'heidnisch'"));
    }
}
