// Tests der KI der Staaten (seit M12; docs/technik/ki.md): Schritt a1 Datenmodell – Regeln, Eigenschaften und
// Staatspersönlichkeiten (common/ki.chron), Personen mit Amtszeit und Bedingung
// (common/persoenlichkeiten.chron), Agenden (common/agenden.chron), Neigungen, Amtswechsel am Monatsersten,
// Zustand, Inhaltsbereich ki und Spielstand Format 6.

#include "test_hilfen.hpp"
#include "uhrwerk/ai.hpp"
#include "uhrwerk/diplomacy.hpp"
#include "uhrwerk/events.hpp"
#include "uhrwerk/military.hpp"
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

    const testhilfen::TempDir& dir() const { return dir_; }

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

namespace {

/// Monatserster mit gesetztem Haushalt: Einnahmen und Ausgaben des Vormonats, Schulden, Jahreseinnahmen.
void set_budget(World& w, std::string_view tag, std::int64_t income, std::int64_t expenses,
                std::int64_t debt) {
    uhrwerk::State& s = w.states[static_cast<std::size_t>(w.find_state(tag))];
    s.last_month_income = fx(income);
    s.last_month_expenses = fx(expenses);
    s.debt = fx(debt);
    s.yearly_income = fx(income * 12);
    s.treasury = fx(income * 12);                // Rücklage von einer Jahreseinnahme (ruecklage_ab)
    s.tax_rates[0] = Fixed::from_ratio(10, 100); // Kontribution
    s.tax_rates[1] = Fixed::from_ratio(5, 100);  // Akzise
}

std::string quiet_rules() {
    std::string rules(kAiRules);
    rules.replace(rules.find("rauschen = 8"), 12, "rauschen = 0");
    return rules;
}

} // namespace

TEST_CASE("KI: Haushalt – Steuern nach Defizit und Überschuss, mit Gründen und Abkühlung") {
    World w = AiData(quiet_rules()).load();
    w.date = uhrwerk::Date{1648, 11, 1};
    uhrwerk::EventCache cache;
    uhrwerk::rebuild_event_cache(w, cache);
    uhrwerk::ensure_ai_state(w, cache);
    // Brandenburg: Defizit 20 % und Schuldenquote 10; Sachsen: Überschuss 40 % ohne Schulden; Anhalt
    // ausgeglichen.
    set_budget(w, "BRA", 1000, 1200, 120000);
    set_budget(w, "SAX", 1000, 600, 0);
    set_budget(w, "ANH", 1000, 1000, 0);
    uhrwerk::Pcg32 rng(1, uhrwerk::RngStream::Ki);
    const std::vector<uhrwerk::Command> commands = uhrwerk::run_ai(w, rng, cache);
    REQUIRE(commands.size() == 2);
    const Index bra = w.find_state("BRA");
    const Index sax = w.find_state("SAX");
    CHECK(commands[0].type == uhrwerk::CommandType::SetTaxRate);
    CHECK(commands[0].state == bra);
    CHECK(commands[0].kind == uhrwerk::TaxKind::Akzise); // die niedrigere steigt
    CHECK(commands[0].value == Fixed::from_ratio(5, 100) + Fixed::from_ratio(1, 100));
    CHECK(commands[1].state == sax);
    CHECK(commands[1].kind == uhrwerk::TaxKind::Kontribution); // die höhere sinkt
    CHECK(commands[1].value == Fixed::from_ratio(10, 100) - Fixed::from_ratio(1, 100));
    // Protokoll mit den drei wichtigsten Gründen.
    REQUIRE(w.ai_log.size() == 2);
    const uhrwerk::AiDecision& raise = w.ai_log[0];
    CHECK(raise.kind == uhrwerk::AiDecisionKind::RaiseTax);
    CHECK(raise.reason_count == 3);
    CHECK(raise.reasons[0].factor == uhrwerk::AiFactor::DebtRatio); // min(30, 10 × (10 − 3))
    CHECK(raise.reasons[0].value == fx(30));
    CHECK(raise.reasons[1].factor == uhrwerk::AiFactor::Deficit);
    CHECK(raise.reasons[1].value.round() == 20); // 100 × 200 ÷ 1.000
    CHECK(uhrwerk::ai_reason_text(w, raise.reasons[0]) == "Schuldenquote +30");
    CHECK(uhrwerk::ai_decision_text(w, raise) == "erhöht die Akzise auf 6 %");
    CHECK(uhrwerk::ai_decision_text(w, w.ai_log[1]) == "senkt die Kontribution auf 9 %");
    // Abkühlung: im nächsten Monat keine neue Steueränderung; der Spielerstaat bleibt unberührt.
    w.date = uhrwerk::Date{1648, 12, 1};
    w.event_player = w.find_state("ANH");
    set_budget(w, "ANH", 1000, 1500, 120000);
    CHECK(uhrwerk::run_ai(w, rng, cache).empty());
    CHECK(w.ai_cooldowns.size() == 2);
    // Nach drei Monaten wieder.
    w.date = uhrwerk::Date{1649, 2, 1};
    CHECK(uhrwerk::run_ai(w, rng, cache).size() == 2);
}

TEST_CASE("KI: Protokoll je Staat begrenzt") {
    std::string rules = quiet_rules();
    rules.replace(rules.find("abkuehlung = 6"), 14,
                  "abkuehlung = 6\n    protokoll = 2\n    abkuehlung_steuern = 0");
    World w = AiData(rules).load();
    uhrwerk::EventCache cache;
    uhrwerk::rebuild_event_cache(w, cache);
    uhrwerk::ensure_ai_state(w, cache);
    uhrwerk::Pcg32 rng(1, uhrwerk::RngStream::Ki);
    for (std::int32_t month = 1; month <= 5; ++month) {
        w.date = uhrwerk::Date{1649, month, 1};
        set_budget(w, "BRA", 1000, 1200, 120000);
        uhrwerk::run_ai(w, rng, cache);
    }
    std::size_t brandenburg = 0;
    for (const uhrwerk::AiDecision& d : w.ai_log) {
        brandenburg += d.state == w.find_state("BRA") ? 1 : 0;
    }
    CHECK(brandenburg == 2);
    CHECK(w.ai_log.back().date == uhrwerk::Date{1649, 5, 1});
}

namespace {

constexpr std::string_view kNeighbors = "ort_a;ort_b;grenze;fluss\n"
                                        "berlin_coelln;potsdam;120;0\n"
                                        "potsdam;zerbst;40;1\n"
                                        "zerbst;dessau;60;0\n"
                                        "dessau;leipzig;50;5\n"
                                        "leipzig;dresden;90;3\n";

constexpr std::string_view kMilitary = "militaer {\n    seit = 1648.01.01\n    fluss_rang = 3\n}\n"
                                       "heerwesen standard {\n    bewaffnung = 1\n    geschuetze = 2\n    "
                                       "mobilmachung = 2\n    mobilmachung_tage = 20\n}\n"
                                       "garnison bra_berlin { staat = BRA ort = berlin_coelln anteil = 1 }\n"
                                       "garnison sax_dresden { staat = SAX ort = dresden anteil = 1 }\n";

} // namespace

TEST_CASE("KI: Abrüstung nach zwei ruhigen Monatsersten, nicht für den Spielerstaat") {
    for (const bool player : {false, true}) {
        CAPTURE(player);
        AiData data(quiet_rules());
        testhilfen::write_file(data.dir() / "karte/nachbarn.csv", kNeighbors);
        testhilfen::write_file(data.dir() / "common/militaer.chron", kMilitary);
        Simulation sim(data.load());
        const Index bra = sim.world().find_state("BRA");
        if (player) {
            uhrwerk::Command p;
            p.type = uhrwerk::CommandType::PlayerState;
            p.state = bra;
            sim.submit(p);
        }
        uhrwerk::Command mobilize;
        mobilize.type = uhrwerk::CommandType::Mobilization;
        mobilize.state = bra;
        mobilize.value = Fixed::one();
        sim.submit(mobilize);
        while (sim.world().date < uhrwerk::Date{1648, 12, 3}) {
            sim.step_day();
        }
        const auto phase = uhrwerk::mobilization_phase(sim.world(), bra);
        const bool demobilized =
            std::any_of(sim.world().ai_log.begin(), sim.world().ai_log.end(), [&](const auto& d) {
                return d.state == bra && d.kind == uhrwerk::AiDecisionKind::Demobilize;
            });
        if (player) {
            CHECK_FALSE(demobilized);
            CHECK(phase != uhrwerk::MobilizationPhase::Demobilizing);
        } else {
            CHECK(demobilized);
            CHECK(phase == uhrwerk::MobilizationPhase::Demobilizing);
        }
    }
}

namespace {

/// Deutscher Bund im Test: alle Orte mit Kreis im Bundesgebiet, Grundbeziehungen wie angegeben.
void federation(const AiData& data, int bra_sax, int bra_anh, int sax_anh) {
    for (const char* file : {"history/orte/10_brandenburg.chron", "history/orte/20_sachsen.chron",
                             "history/orte/30_anhalt.chron"}) {
        std::string text = testhilfen::read_file(data.dir() / file);
        std::string::size_type pos = 0;
        while ((pos = text.find("\n    kreis", pos)) != std::string::npos) {
            text.insert(pos + 1, "    bund = ja\n");
            pos += 20;
        }
        testhilfen::write_file(data.dir() / file, text);
    }
    std::string relations;
    const auto add = [&](std::string_view a, std::string_view b, int value) {
        relations += "beziehung {\n    staaten = { " + std::string(a) + " " + std::string(b) +
                     " }\n    wert = " + std::to_string(value) + "\n    quelle = \"Testdaten\"\n}\n";
    };
    add("BRA", "SAX", bra_sax);
    add("BRA", "ANH", bra_anh);
    add("SAX", "ANH", sax_anh);
    testhilfen::write_file(data.dir() / "history/beziehungen/grundwerte.chron", relations);
}

} // namespace

TEST_CASE("KI: Bündnis gegen einen gemeinsamen Rivalen, Annahme nach Nutzen") {
    const std::string agendas =
        "agenda BRA {\n    ziel { art = schwaechen  gegner = ANH  gewicht = 30 }\n}\n"
        "agenda SAX {\n    ziel { art = schwaechen  gegner = ANH  gewicht = 30 }\n}\n";
    AiData data(quiet_rules(), kPersons, agendas);
    federation(data, 60, -150, -150);
    Simulation sim(data.load());
    const Index bra = sim.world().find_state("BRA");
    const Index sax = sim.world().find_state("SAX");
    // Nach der Regel aus M9 (bündnis_ab = 100) wäre das Bündnis abgelehnt; mit gemeinsamem Rivalen nimmt
    // Sachsen an: (60 − 50) ÷ 5 + 20 = 22 ≥ 10.
    while (sim.world().date < uhrwerk::Date{1648, 11, 3}) {
        sim.step_day();
    }
    const World& w = sim.world();
    CHECK(w.ai_strategy[static_cast<std::size_t>(bra)].rival == w.find_state("ANH"));
    CHECK(uhrwerk::allied(w, bra, sax));
    const auto offer = std::find_if(w.ai_log.begin(), w.ai_log.end(), [](const uhrwerk::AiDecision& d) {
        return d.kind == uhrwerk::AiDecisionKind::OfferAlliance;
    });
    REQUIRE(offer != w.ai_log.end());
    CHECK(offer->state == sax); // Sachsen plant zuerst (Reihenfolge der Staaten)
    CHECK(offer->target == bra);
    CHECK(uhrwerk::ai_decision_text(w, *offer) == "bietet Brandenburg ein Bündnis an");
    CHECK(std::any_of(offer->reasons.begin(), offer->reasons.begin() + offer->reason_count,
                      [&](const uhrwerk::AiReason& r) { return r.factor == uhrwerk::AiFactor::Rival; }));
    // Ohne gemeinsamen Rivalen lehnt ein Staat ohne Spieler ab, der Spielerstaat nach der Regel.
    World other = data.load();
    uhrwerk::EventCache cache;
    uhrwerk::rebuild_event_cache(other, cache);
    uhrwerk::ensure_ai_state(other, cache);
    CHECK_FALSE(uhrwerk::ai_accepts_alliance(other, sax, bra));
}

TEST_CASE("KI: Ereignisoptionen nach Neigung, Befehl Spielerströmung") {
    AiData data(quiet_rules());
    testhilfen::write_file(
        data.dir() / "common/ereignisse/test.chron",
        "ereignis test.wagnis {\n    titel = \"Wagnis\"\n    text = \"x\"\n    art = dynamisch\n"
        "    wiederholbar = ja\n    abstand = 365\n"
        "    option kuehn { text = \"kühn\" gewicht = 10 ki { risiko = 1 expansion = 1 } }\n"
        "    option still { text = \"still\" gewicht = 10 }\n}\n");
    Simulation sim(data.load());
    const World& w = sim.world();
    const uhrwerk::EventDef& event = w.events[static_cast<std::size_t>(w.find_event("test.wagnis"))];
    REQUIRE(event.options[0].has_ai);
    CHECK_FALSE(event.options[1].has_ai);
    uhrwerk::EventCache cache;
    uhrwerk::rebuild_event_cache(w, cache);
    // Brandenburg (Machtpolitiker, Herrscher Machtpolitiker): risiko 1,6, expansion 2 → 1 + ¼ × 3,6 = 1,9.
    const Index bra = w.find_state("BRA");
    const auto weights = uhrwerk::option_weights(w, cache, event, bra);
    CHECK(weights[0].round() == 19);
    CHECK(weights[1] == fx(10));
    // Sachsen (Bewahrer): risiko −1 → 1 − ¼ = 0,75.
    const auto saxony = uhrwerk::option_weights(w, cache, event, w.find_state("SAX"));
    CHECK(saxony[0] == Fixed::from_ratio(15, 2));
    // Spielerströmung: gültig gesetzt, ungültig ohne Wirkung.
    uhrwerk::Command c;
    c.type = uhrwerk::CommandType::PlayerCurrent;
    c.state = 0;
    sim.submit(c);
    c.state = 99;
    sim.submit(c);
    sim.step_day();
    CHECK(sim.world().player_current == 0);
}

TEST_CASE("KI: Kriegserklärung aus der Agenda bei Übermacht, Gründe in der Chronik") {
    for (const bool window : {true, false}) {
        CAPTURE(window);
        // Annexion Anhalts: aktiv ab 1648.11.01 bzw. erst ab 1700 (dann kein Krieg).
        const std::string agendas =
            std::string("agenda BRA {\n    ziel { art = annexion  gegner = ANH  gewicht = 60  ab = ") +
            (window ? "1648.11.01" : "1700.01.01") + " }\n}\n";
        AiData data(quiet_rules(), kPersons, agendas);
        testhilfen::write_file(data.dir() / "karte/nachbarn.csv", kNeighbors);
        testhilfen::write_file(data.dir() / "common/militaer.chron", kMilitary);
        Simulation sim(data.load());
        const Index bra = sim.world().find_state("BRA");
        const Index anh = sim.world().find_state("ANH");
        while (sim.world().date < uhrwerk::Date{1648, 11, 3}) {
            sim.step_day();
        }
        const World& w = sim.world();
        const auto declared =
            std::find_if(w.ai_log.begin(), w.ai_log.end(), [](const uhrwerk::AiDecision& d) {
                return d.kind == uhrwerk::AiDecisionKind::DeclareWar;
            });
        if (!window) {
            CHECK(declared == w.ai_log.end());
            CHECK_FALSE(uhrwerk::at_war(w, bra, anh));
            continue;
        }
        REQUIRE(declared != w.ai_log.end());
        CHECK(declared->state == bra);
        CHECK(declared->target == anh);
        CHECK(uhrwerk::ai_decision_text(w, *declared) == "erklärt Anhalt den Krieg");
        CHECK(uhrwerk::at_war(w, bra, anh));
        CHECK(std::any_of(w.chronicle.begin(), w.chronicle.end(), [](const uhrwerk::ChronicleEntry& e) {
            return e.text.rfind("Brandenburg – Gründe: Agenda Anhalt +60", 0) == 0;
        }));
        // Abkühlung gegen denselben Gegner.
        CHECK(std::any_of(w.ai_cooldowns.begin(), w.ai_cooldowns.end(), [&](const uhrwerk::AiCooldown& c) {
            return c.state == bra && c.kind == uhrwerk::AiDecisionKind::DeclareWar && c.target == anh;
        }));
    }
}
