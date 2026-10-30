// Tests der KI der Staaten (seit M12; docs/technik/ki.md): Schritt a1 Datenmodell – Regeln, Eigenschaften und
// Staatspersönlichkeiten (common/ki.chron), Personen mit Amtszeit und Bedingung
// (common/persoenlichkeiten.chron), Agenden (common/agenden.chron), Neigungen, Amtswechsel am Monatsersten,
// Zustand, Inhaltsbereich ki und Spielstand Format 6.

#include "test_hilfen.hpp"
#include "uhrwerk/ai.hpp"
#include "uhrwerk/savegame.hpp"
#include "uhrwerk/simulation.hpp"

#include <algorithm>
#include <cstdint>
#include <doctest/doctest.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using uhrwerk::AgendaKind;
using uhrwerk::Fixed;
using uhrwerk::Inclination;
using uhrwerk::Index;
using uhrwerk::kNone;
using uhrwerk::PersonRole;
using uhrwerk::SaveMeta;
using uhrwerk::Simulation;
using uhrwerk::World;
using Bytes = std::vector<std::uint8_t>;

constexpr std::size_t kAiPart = 17; // Inhaltsbereich ki in kContentParts

constexpr std::string_view kEventRules = "ereignisse {\n    seit = 1648.01.01\n}\n";

constexpr std::string_view kAiRules = R"(
ki {
    seit = 1648.01.01
    rauschen = 8
    abkuehlung = 6
    schulden_grenze = 2.5
    standard_persoenlichkeit = bewahrer
}
persoenlichkeit machtpolitiker { name = "Machtpolitiker" risiko = 1 expansion = 2 legitimitaet = -1 vermittlung = -1 }
persoenlichkeit bewahrer { name = "Bewahrer" risiko = -1 legitimitaet = 2 reform = -1 }
eigenschaft machtpolitiker { name = "Machtpolitiker" risiko = 1 expansion = 1 }
eigenschaft zauderer { name = "Zauderer" risiko = -2 }
eigenschaft sparsam { name = "Sparsam" sparsamkeit = 1 }
)";

constexpr std::string_view kPersons = R"(
person kurfuerst {
    name   = "Friedrich Wilhelm"
    staat  = BRA
    rolle  = herrscher
    amt    = "Kurfürst"
    von    = 1640.12.01
    bis    = 1688.05.09
    werte  = { staatskunst = 8 diplomatie = 7 kriegskunst = 7 wirkung = 6 }
    eigenschaften = { machtpolitiker sparsam }
    quelle = "Testdaten"
    sicherheit = spielwert
}
person schwerin {
    name   = "Otto von Schwerin"
    staat  = BRA
    rolle  = minister
    amt    = "Oberpräsident"
    von    = 1648.01.01
    bedingung { datum >= 1648.12.01 }
    eigenschaften = { zauderer }
    quelle = "Testdaten"
}
person johann_georg {
    name   = "Johann Georg I."
    staat  = SAX
    rolle  = herrscher
    von    = 1611.06.23
    bis    = 1656.10.08
    quelle = "Testdaten"
}
)";

constexpr std::string_view kAgendas = R"(
agenda BRA {
    persoenlichkeit = machtpolitiker
    ziel { art = annexion  gegner = ANH  gewicht = 40  ab = 1650.01.01 }
    ziel { art = buendnis  partner = SAX  gewicht = 20  bedingung { datum >= 1649.01.01 } }
    ziel { art = ruhe  gewicht = 10 }
}
)";

/// Test-Fixture mit Ereignissen und KI-Daten.
class AiData {
public:
    explicit AiData(std::string_view rules = kAiRules, std::string_view persons = kPersons,
                    std::string_view agendas = kAgendas)
        : dir_(true) {
        testhilfen::write_file(dir_ / "common/ereignisse.chron", kEventRules);
        testhilfen::write_file(dir_ / "common/ki.chron", rules);
        if (!persons.empty()) {
            testhilfen::write_file(dir_ / "common/persoenlichkeiten.chron", persons);
        }
        if (!agendas.empty()) {
            testhilfen::write_file(dir_ / "common/agenden.chron", agendas);
        }
    }

    uhrwerk::LoadResult result() const { return uhrwerk::load_world(dir_.path()); }

    World load() const {
        uhrwerk::LoadResult r = result();
        INFO(testhilfen::issues_text(r));
        REQUIRE(r.ok());
        CHECK(r.issues.empty());
        return std::move(*r.world);
    }

private:
    testhilfen::TempDir dir_;
};

Fixed fx(std::int64_t v) {
    return Fixed::from_int(v);
}

Fixed of(const uhrwerk::Inclinations& values, Inclination i) {
    return values[static_cast<std::size_t>(i)];
}

} // namespace

TEST_CASE("KI: ohne Daten oder vor 'seit' nichts (1648 bitgleich)") {
    testhilfen::TempDir plain(true);
    uhrwerk::LoadResult r = uhrwerk::load_world(plain.path());
    REQUIRE(r.ok());
    CHECK_FALSE(r.world->ai_rules.active);
    Simulation sim(std::move(*r.world));
    sim.step_days(40);
    CHECK(sim.world().offices.empty());

    std::string late(kAiRules);
    late.replace(late.find("1648.01.01"), 10, "1700.01.01");
    const World w = AiData(late).load();
    CHECK_FALSE(w.ai_rules.active);
    CHECK(w.persons.empty());
    CHECK(w.ai_traits.empty());
    const auto fp = uhrwerk::content_fingerprint(w);
    CHECK(fp.parts[kAiPart] == uhrwerk::content_fingerprint(w).parts[kAiPart]);
}

TEST_CASE("KI: Lader") {
    const World w = AiData().load();
    REQUIRE(w.ai_rules.active);
    CHECK(w.ai_rules.noise == fx(8));
    CHECK(w.ai_rules.cooldown_months == 6);
    CHECK(w.ai_rules.debt_limit == Fixed::from_ratio(5, 2));
    CHECK(w.ai_rules.default_personality == w.find_personality("bewahrer"));
    REQUIRE(w.ai_traits.size() == 3);
    REQUIRE(w.ai_personalities.size() == 2);
    // Personen nach Kennung geordnet.
    REQUIRE(w.persons.size() == 3);
    CHECK(w.persons[0].id == "johann_georg");
    CHECK(w.persons[1].id == "kurfuerst");
    CHECK(w.persons[2].id == "schwerin");
    const auto& kf = w.persons[static_cast<std::size_t>(w.find_person("kurfuerst"))];
    CHECK(kf.state == w.find_state("BRA"));
    CHECK(kf.role == PersonRole::Ruler);
    CHECK(kf.title == "Kurfürst");
    CHECK(kf.has_to);
    CHECK(kf.values == std::array<std::int32_t, 4>{8, 7, 7, 6});
    CHECK(kf.traits == std::vector<Index>{w.find_trait("machtpolitiker"), w.find_trait("sparsam")});
    const auto& sw = w.persons[static_cast<std::size_t>(w.find_person("schwerin"))];
    CHECK(sw.role == PersonRole::Minister);
    CHECK(sw.has_condition);
    CHECK_FALSE(sw.has_to);
    // Agenden je Staat.
    REQUIRE(w.agendas.size() == w.states.size());
    const auto& bra = w.agendas[static_cast<std::size_t>(w.find_state("BRA"))];
    CHECK(bra.personality == w.find_personality("machtpolitiker"));
    REQUIRE(bra.goals.size() == 3);
    CHECK(bra.goals[0].kind == AgendaKind::Annexation);
    CHECK(bra.goals[0].target == w.find_state("ANH"));
    CHECK(bra.goals[0].weight == fx(40));
    CHECK(bra.goals[0].has_from);
    CHECK(bra.goals[1].kind == AgendaKind::Alliance);
    CHECK(bra.goals[1].has_condition);
    CHECK(bra.goals[2].kind == AgendaKind::Calm);
    CHECK(bra.goals[2].target == kNone);
    CHECK(w.agendas[static_cast<std::size_t>(w.find_state("SAX"))].goals.empty());
}

TEST_CASE("KI: Ladefehler") {
    struct Case {
        std::string_view name;
        int file; // 0 Regeln, 1 Personen, 2 Agenden
        std::string_view from;
        std::string_view to;
        std::string_view message;
    };
    const Case cases[] = {
        {"ohne seit", 0, "seit = 1648.01.01", "", "Pflichtfeld 'seit' fehlt in ki"},
        {"Neigung zu groß", 0, "risiko = -2 }", "risiko = -3 }", "'risiko' muss zwischen -2 und 2 liegen"},
        {"Standard unbekannt", 0, "standard_persoenlichkeit = bewahrer",
         "standard_persoenlichkeit = haendler", "unbekannte Persönlichkeit 'haendler'"},
        {"Eigenschaft ohne Name", 0, "{ name = \"Sparsam\" sparsamkeit", "{ sparsamkeit",
         "Pflichtfeld 'name' fehlt in eigenschaft sparsam"},
        {"unbekannte Eigenschaft", 1, "{ machtpolitiker sparsam }", "{ machtpolitiker gierig }",
         "unbekannte Eigenschaft 'gierig'"},
        {"unbekannter Staat", 1, "staat  = SAX", "staat  = XXX", "unbekannter Staat 'XXX'"},
        {"Rolle", 1, "rolle  = minister", "rolle  = kanzler", "ungültige Rolle 'kanzler'"},
        {"ohne Quelle", 1, "quelle = \"Testdaten\"\n    sicherheit", "sicherheit",
         "Pflichtfeld 'quelle' fehlt in person kurfuerst"},
        {"bis vor von", 1, "bis    = 1656.10.08", "bis    = 1600.01.01", "'bis' liegt vor 'von'"},
        {"Wert zu groß", 1, "wirkung = 6", "wirkung = 11", "'wirkung' muss zwischen 1 und 10 liegen"},
        {"doppelte Person", 1, "person schwerin {", "person kurfuerst {", "doppelte Kennung 'kurfuerst'"},
        {"Bedingung", 1, "datum >= 1648.12.01", "aufruhr >= 3", "unbekannte Bedingung 'aufruhr'"},
        {"Art unbekannt", 2, "art = ruhe", "art = frieden", "ungültige Art 'frieden'"},
        {"Gegenstand fehlt", 2, "art = annexion  gegner = ANH", "art = annexion",
         "Art annexion braucht 'gegner'"},
        {"falscher Gegenstand", 2, "art = ruhe", "art = ruhe  gegner = SAX",
         "Art ruhe hat keinen Gegenstand"},
        {"ohne Gewicht", 2, "gewicht = 10 }", "}", "Pflichtfeld 'gewicht' fehlt in ziel"},
        {"Persönlichkeit", 2, "persoenlichkeit = machtpolitiker", "persoenlichkeit = tyrann",
         "unbekannte Persönlichkeit 'tyrann'"},
        {"doppelte Agenda", 2, "ziel { art = ruhe  gewicht = 10 }\n}",
         "ziel { art = ruhe  gewicht = 10 }\n}\nagenda BRA { }", "Agenda von 'BRA' ist doppelt"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.name);
        std::string files[3] = {std::string(kAiRules), std::string(kPersons), std::string(kAgendas)};
        std::string& text = files[c.file];
        const auto pos = text.find(c.from);
        REQUIRE(pos != std::string::npos);
        text.replace(pos, c.from.size(), c.to);
        const uhrwerk::LoadResult r = AiData(files[0], files[1], files[2]).result();
        const std::string issues = testhilfen::issues_text(r);
        CAPTURE(issues);
        CHECK(issues.find(c.message) != std::string::npos);
    }
}

TEST_CASE("KI: Neigungen aus Persönlichkeit und Personen") {
    Simulation sim(AiData().load());
    const World& w = sim.world();
    const Index bra = w.find_state("BRA");
    const Index sax = w.find_state("SAX");
    REQUIRE(w.office(bra, PersonRole::Ruler) == w.find_person("kurfuerst"));
    CHECK(w.office(bra, PersonRole::Minister) == kNone); // Bedingung erst ab Dezember
    // Ohne Verfassung zählt der Herrscher 0,6: Machtpolitiker (+1 risiko, +2 expansion) und seine
    // Eigenschaften.
    const auto values = uhrwerk::state_inclinations(w, bra);
    CHECK(of(values, Inclination::Risk) == fx(1) + Fixed::from_ratio(3, 5));
    CHECK(of(values, Inclination::Expansion) == fx(2)); // 2 + 0,6 begrenzt auf 2
    CHECK(of(values, Inclination::Thrift) == Fixed::from_ratio(3, 5));
    CHECK(of(values, Inclination::Legitimacy) == fx(-1));
    // Sachsen ohne Agenda: Standardpersönlichkeit Bewahrer, Herrscher ohne Eigenschaften.
    const auto saxony = uhrwerk::state_inclinations(w, sax);
    CHECK(of(saxony, Inclination::Legitimacy) == fx(2));
    CHECK(of(saxony, Inclination::Risk) == fx(-1));
    // Ziele der Agenda nach Zeitfenster und Bedingung.
    uhrwerk::EventCache cache;
    uhrwerk::rebuild_event_cache(w, cache);
    const auto& goals = w.agendas[static_cast<std::size_t>(bra)].goals;
    CHECK_FALSE(uhrwerk::agenda_goal_active(w, cache, bra, goals[0]));
    CHECK_FALSE(uhrwerk::agenda_goal_active(w, cache, bra, goals[1]));
    CHECK(uhrwerk::agenda_goal_active(w, cache, bra, goals[2]));
}

TEST_CASE("KI: Amtswechsel am Monatsersten mit Chronik") {
    Simulation sim(AiData().load());
    const Index bra = sim.world().find_state("BRA");
    const std::size_t chronicle = sim.world().chronicle.size();
    while (!(sim.world().date.month == 12 && sim.world().date.day == 2)) {
        sim.step_day();
    }
    const World& w = sim.world();
    CHECK(w.office(bra, PersonRole::Minister) == w.find_person("schwerin"));
    const auto entry = std::find_if(w.chronicle.begin() + static_cast<std::ptrdiff_t>(chronicle),
                                    w.chronicle.end(), [](const uhrwerk::ChronicleEntry& e) {
                                        return e.text == "Brandenburg: Otto von Schwerin wird Oberpräsident";
                                    });
    REQUIRE(entry != w.chronicle.end());
    CHECK(entry->state == bra);
    CHECK(entry->date == uhrwerk::Date{1648, 12, 1});
    // Mit dem Minister zählt der Zauderer zu 0,4: risiko 1 + 0,6 − 0,8.
    const Fixed ruler = Fixed::from_ratio(3, 5);
    CHECK(of(uhrwerk::state_inclinations(w, bra), Inclination::Risk) ==
          fx(1) + ruler + (Fixed::one() - ruler) * fx(-2));
}

TEST_CASE("KI: Spielstand, Fingerabdruck und Spielerstaat") {
    const AiData data;
    Simulation sim(data.load());
    uhrwerk::Command player;
    player.type = uhrwerk::CommandType::PlayerState;
    player.state = sim.world().find_state("SAX");
    sim.submit(player);
    sim.step_days(45);
    CHECK(sim.world().event_player == sim.world().find_state("SAX"));
    const Bytes bytes = uhrwerk::save_game(sim, SaveMeta{});
    const auto info = uhrwerk::read_save_info(bytes);
    REQUIRE(info);
    CHECK(info->format_version == 6);
    uhrwerk::LoadedGame loaded = uhrwerk::load_game(bytes, data.load(), 1);
    INFO(loaded.error);
    REQUIRE(loaded.ok());
    CHECK(loaded.simulation->checksum() == sim.checksum());
    CHECK(loaded.simulation->world().offices == sim.world().offices);
    loaded.simulation->step_days(30);
    sim.step_days(30);
    CHECK(loaded.simulation->checksum() == sim.checksum());

    // Der Inhaltsbereich ki ändert sich mit einer Person, die anderen nicht.
    const auto a = uhrwerk::content_fingerprint(data.load());
    std::string persons(kPersons);
    persons.replace(persons.find("wirkung = 6"), 11, "wirkung = 7");
    const auto b = uhrwerk::content_fingerprint(AiData(kAiRules, persons).load());
    CHECK(a.parts[kAiPart] != 0);
    CHECK(a.parts[kAiPart] != b.parts[kAiPart]);
    CHECK(uhrwerk::content_differences(a, b) == std::vector<std::string_view>{"ki"});
}
