// Tests der Ereignisse (seit M11; docs/technik/ereignisse.md): Schritt a1 Datenmodell – Gesetzesgruppen,
// Wünsche der Interessengruppen, Lader der Ereignisse mit Bedingungen und Wirkungen (Abschnitt 3), Zustand,
// Inhaltsbereich ereignisse und Spielstand Format 5 (Abschnitt 8).

#include "test_hilfen.hpp"
#include "uhrwerk/event_lab.hpp"
#include "uhrwerk/events.hpp"
#include "uhrwerk/military.hpp"
#include "uhrwerk/savegame.hpp"
#include "uhrwerk/simulation.hpp"

#include <algorithm>
#include <cstdint>
#include <doctest/doctest.h>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using uhrwerk::ConditionKind;
using uhrwerk::EffectKind;
using uhrwerk::EventDef;
using uhrwerk::Fixed;
using uhrwerk::Index;
using uhrwerk::kNone;
using uhrwerk::SaveMeta;
using uhrwerk::Simulation;
using uhrwerk::World;
using Bytes = std::vector<std::uint8_t>;

constexpr std::size_t kEventPart = 16; // Inhaltsbereich ereignisse in kContentParts

constexpr std::string_view kLaws =
    "gesetz presse {\n    name = \"Presse\"\n    seit = 1600.01.01\n    standard = vorzensur\n"
    "    option vorzensur { name = \"Vorzensur\" }\n    option pressefreiheit { name = \"Pressefreiheit\" }\n"
    "    quelle = \"Testdaten\"\n}\n"
    "gesetz vereine {\n    name = \"Vereine\"\n    seit = 1700.01.01\n    standard = verbot\n"
    "    option verbot { name = \"Verbot\" }\n    option frei { name = \"Vereinsfreiheit\" }\n}\n";

// Ohne Annäherung an die Zielwerte (Schritt a3), damit die Tests der Schritte a1 und a2 die Wirkungen der
// Ereignisse allein sehen.
constexpr std::string_view kRules = "ereignisse {\n    seit = 1648.01.01\n    frist = 20\n"
                                    "    zustimmung_annaeherung = 0\n    unruhe_annaeherung = 0\n}\n";

constexpr std::string_view kEvents = R"(
ereignis test.unruhen {
    titel = "Unruhen"
    text {
        "Erste Zeile"
        "zweite Zeile."
        ""
        "Zweiter Absatz."
    }
    datum = 1648.11.01
    quelle = "Testdaten"
    sicherheit = spielwert
    art = historisch
    wendepunkt = ja
    bedingung {
        datum >= 1648.11.01
        gesetz_presse = vorzensur
        unruhe >= 30
        zustimmung_bildungsbuergertum < 5
        nicht { geschehen = test.unruhen }
        staat_in = { BRA SAX }
        oder { im_krieg = nein  spieler = ja }
        sitze { parlament = landtag stroemung = liberale mindestens = 2 }
        gesetz_vereine = frei
    }
    wartezeit = 10
    option nachgeben {
        text = "Nachgeben"
        historisch = ja
        gewicht = 70
        gewicht_wenn { unruhe >= 60  plus = 30 }
        wirkung {
            gesetz_presse = pressefreiheit
            gesetz_vereine = frei
            zustimmung_bildungsbuergertum = 3
            unruhe = -20
            merke = nachgegeben
            folge { ereignis = test.welt tage = 0 }
            zufall { 30 = { schatz = -1000 } 70 = { beziehung_SAX = 10 } }
        }
    }
    option hart {
        text = "Hart bleiben"
        gewicht = 30
        bedingung { geschehen { ereignis = test.welt option = b } }
        wirkung { verwuestung { ort = dresden wert = 0.1 }  folge { ereignis = test.unruhen tage = 30 staat = SAX } }
    }
}

ereignis test.welt {
    titel = "Welt"
    text = "Ein Ereignis für alle."
    art = atmosphaerisch
    wer = welt
    termin = 1649.01.01
    bedingung { staat SAX { unruhe > 1 } ein_staat { merkmal = nachgegeben } }
    option a { text = "a" gewicht = 1 wirkung { welt_merke = test alle_staaten { unruhe = 5 } } }
    option b { text = "b" gewicht = 0 wirkung { staat BRA { schatz = 5 } chronik = "Eintrag" } }
}

ereignis test.dynamisch {
    titel = "Wiederkehr"
    text = "x"
    art = dynamisch
    wiederholbar = ja
    abstand = 365
    frist = 5
    option a { text = "a" gewicht = 1 }
}
)";

/// Fixture mit Gesetzen, Wünschen und Ereignissen: Brandenburg hat Pressefreiheit, Sachsen den Standard.
class EventData {
public:
    explicit EventData(std::string_view events = kEvents) : dir_(true) {
        testhilfen::write_file(dir_ / "common/gesetze.chron", kLaws);
        testhilfen::write_file(dir_ / "common/ereignisse.chron", kRules);
        testhilfen::write_file(dir_ / "common/ereignisse/test.chron", events);
        testhilfen::replace_in_file(dir_ / "common/interessengruppen.chron", "nach_bildung = ja",
                                    "nach_bildung = ja\n    wuensche = { presse = { pressefreiheit = 2 "
                                    "vorzensur = -2 } vereine = { frei = 1 } }");
        testhilfen::replace_in_file(dir_ / "history/staaten/10_brandenburg.chron", "heer           = 10",
                                    "heer           = 10\n    gesetz_presse = pressefreiheit");
    }

    uhrwerk::LoadResult result() const { return uhrwerk::load_world(dir_.path()); }

    World load() const {
        uhrwerk::LoadResult r = result();
        INFO(testhilfen::issues_text(r));
        REQUIRE(r.ok());
        CHECK(r.issues.empty());
        return std::move(*r.world);
    }

    const testhilfen::TempDir& dir() const { return dir_; }

private:
    testhilfen::TempDir dir_;
};

const EventDef& event(const World& w, std::string_view id) {
    const Index i = w.find_event(id);
    REQUIRE(i != kNone);
    return w.events[static_cast<std::size_t>(i)];
}

} // namespace

TEST_CASE("Ereignisse: nur ab 'seit'") {
    const World fixture = testhilfen::load_fixture();
    CHECK_FALSE(fixture.event_rules.active);
    CHECK(fixture.events.empty());
    CHECK(fixture.approval.empty());

    // Regeln nach dem Startdatum: Ereignisse werden nicht gelesen, Zustand und Prüfsumme bleiben die der
    // Fixture (1648 bitgleich), der Bereich ereignisse zählt nicht.
    const EventData data;
    testhilfen::replace_in_file(data.dir() / "common/ereignisse.chron", "seit = 1648.01.01",
                                "seit = 1848.01.01");
    testhilfen::write_file(data.dir() / "common/ereignisse/kaputt.chron", "das ist { kein Ereignis");
    const World w = data.load();
    CHECK_FALSE(w.event_rules.active);
    CHECK(w.events.empty());
    CHECK(w.unrest.empty());
    CHECK(uhrwerk::content_fingerprint(w).parts[kEventPart] ==
          uhrwerk::content_fingerprint(fixture).parts[kEventPart]);
    Simulation a(fixture);
    Simulation b(w);
    a.step_days(40);
    b.step_days(40);
    CHECK(a.checksum() == b.checksum());
}

TEST_CASE("Ereignisse: Gesetze und Wünsche") {
    const World w = EventData().load();
    REQUIRE(w.law_groups.size() == 1); // vereine erst ab 1700
    const uhrwerk::LawGroup& presse = w.law_groups[0];
    CHECK(presse.id == "presse");
    CHECK(presse.options.size() == 2);
    CHECK(presse.standard == 0);
    CHECK(w.states[static_cast<std::size_t>(w.find_state("BRA"))].laws == std::vector<Index>{1});
    CHECK(w.states[static_cast<std::size_t>(w.find_state("SAX"))].laws == std::vector<Index>{0});
    const uhrwerk::InterestGroup& g =
        w.interest_groups[static_cast<std::size_t>(w.find_interest_group("bildungsbuergertum"))];
    REQUIRE(g.wishes.size() == 2); // der Wunsch an die spätere Gruppe vereine entfällt
    CHECK(g.wishes[0].law == 0);
    CHECK(g.wishes[0].option == 1);
    CHECK(g.wishes[0].approval == Fixed::from_int(2));
    CHECK(g.wishes[1].approval == Fixed::from_int(-2));
    // Zustand der Ereignisse: Zustimmung je Staat und Gruppe, Unruhe je Staat, beides 0.
    CHECK(w.approval.size() == w.states.size() * w.interest_groups.size());
    CHECK(w.unrest.size() == w.states.size());
}

TEST_CASE("Ereignisse: Lader") {
    const World w = EventData().load();
    REQUIRE(w.event_rules.active);
    CHECK(w.event_rules.deadline == 20);
    REQUIRE(w.events.size() == 3);
    // Nach Kennung geordnet (Reihenfolge der täglichen Prüfung).
    CHECK(w.events[0].id == "test.dynamisch");
    CHECK(w.events[1].id == "test.unruhen");
    CHECK(w.events[2].id == "test.welt");

    const EventDef& e = event(w, "test.unruhen");
    CHECK(e.title == "Unruhen");
    CHECK(e.text == "Erste Zeile zweite Zeile.\n\nZweiter Absatz.");
    CHECK(e.has_date);
    CHECK(e.date == uhrwerk::Date{1648, 11, 1});
    CHECK(e.certainty == "spielwert");
    CHECK(e.kind == uhrwerk::EventKind::Historical);
    CHECK(e.turning_point);
    CHECK(e.scope == uhrwerk::EventScope::State);
    CHECK(e.wait == 10);
    CHECK(e.deadline == 0);
    CHECK(e.file == "common/ereignisse/test.chron");

    const auto& checks = e.condition.children;
    REQUIRE(checks.size() == 9);
    CHECK(checks[0].kind == ConditionKind::Date);
    CHECK(checks[0].op == uhrwerk::CompareOp::Ge);
    CHECK(checks[1].kind == ConditionKind::Law);
    CHECK(checks[1].b == 0);
    CHECK(checks[2].kind == ConditionKind::Unrest);
    CHECK(checks[2].value == Fixed::from_int(30));
    CHECK(checks[3].kind == ConditionKind::Approval);
    CHECK(checks[3].op == uhrwerk::CompareOp::Lt);
    CHECK(checks[4].kind == ConditionKind::Not);
    REQUIRE(checks[4].children.size() == 1);
    CHECK(checks[4].children[0].kind == ConditionKind::Happened);
    CHECK(checks[4].children[0].a == w.find_event("test.unruhen"));
    CHECK(checks[5].kind == ConditionKind::StateIn);
    CHECK(checks[5].list.size() == 2);
    CHECK(checks[6].kind == ConditionKind::Any);
    REQUIRE(checks[6].children.size() == 2);
    CHECK(checks[6].children[0].kind == ConditionKind::Not); // im_krieg = nein
    CHECK(checks[6].children[1].kind == ConditionKind::Player);
    // Der Landtag gibt es erst seit 1649, die Gesetzesgruppe vereine erst seit 1700: nie wahr.
    CHECK(checks[7].kind == ConditionKind::Never);
    CHECK(checks[8].kind == ConditionKind::Never);

    REQUIRE(e.options.size() == 2);
    const uhrwerk::EventOption& give = e.options[0];
    CHECK(give.id == "nachgeben");
    CHECK(give.historical);
    CHECK(give.weight == Fixed::from_int(70));
    REQUIRE(give.weight_modifiers.size() == 1);
    CHECK(give.weight_modifiers[0].plus == Fixed::from_int(30));
    // gesetz_vereine entfällt (späterer Inhalt).
    REQUIRE(give.effects.size() == 6);
    CHECK(give.effects[0].kind == EffectKind::Law);
    CHECK(give.effects[0].b == 1);
    CHECK(give.effects[1].kind == EffectKind::Approval);
    CHECK(give.effects[1].value == Fixed::from_int(3));
    CHECK(give.effects[2].kind == EffectKind::Unrest);
    CHECK(give.effects[2].value == Fixed::from_int(-20));
    CHECK(give.effects[3].kind == EffectKind::SetFlag);
    CHECK(give.effects[3].name == "nachgegeben");
    CHECK(give.effects[4].kind == EffectKind::Follow);
    CHECK(give.effects[4].a == w.find_event("test.welt"));
    CHECK(give.effects[5].kind == EffectKind::Random);
    REQUIRE(give.effects[5].branches.size() == 2);
    CHECK(give.effects[5].weights[1] == Fixed::from_int(70));
    CHECK(give.effects[5].branches[1][0].kind == EffectKind::Relation);

    const uhrwerk::EventOption& hard = e.options[1];
    REQUIRE(hard.condition.children.size() == 1);
    CHECK(hard.condition.children[0].b == 1); // Option b von test.welt
    REQUIRE(hard.effects.size() == 2);
    CHECK(hard.effects[0].kind == EffectKind::Devastation);
    CHECK(hard.effects[0].a == w.find_location("dresden"));
    CHECK(hard.effects[1].days == 30);
    CHECK(hard.effects[1].b == w.find_state("SAX"));

    const EventDef& world_event = event(w, "test.welt");
    CHECK(world_event.scope == uhrwerk::EventScope::World);
    CHECK(world_event.has_fixed_date);
    CHECK(world_event.condition.children[0].kind == ConditionKind::OfState);
    CHECK(world_event.condition.children[1].kind == ConditionKind::AnyState);
    CHECK(world_event.options[0].effects[1].kind == EffectKind::AllStates);
    CHECK(world_event.options[1].effects[0].kind == EffectKind::OfState);
    CHECK(world_event.options[1].effects[1].name == "Eintrag");

    const EventDef& repeat = event(w, "test.dynamisch");
    CHECK(repeat.repeatable);
    CHECK(repeat.spacing == 365);
    CHECK(repeat.deadline == 5);
}

TEST_CASE("Ereignisse: Ladefehler") {
    struct Case {
        std::string_view name;
        std::string_view from; // Text in kEvents
        std::string_view to;
        std::string_view message;
    };
    const Case cases[] = {
        {"unbekannte Bedingung", "unruhe >= 30", "aufruhr >= 30", "unbekannte Bedingung 'aufruhr'"},
        {"unbekannte Wirkung", "unruhe = -20", "revolution = ja", "unbekannte Wirkung 'revolution'"},
        {"unbekannte Gesetzesoption", "gesetz_presse = pressefreiheit", "gesetz_presse = frei",
         "unbekannte Option 'frei' der Gesetzesgruppe presse"},
        {"unbekannte Gesetzesgruppe", "gesetz_presse = vorzensur", "gesetz_zensur = vorzensur",
         "unbekannte Gesetzesgruppe 'zensur'"},
        {"unbekannte Gruppe", "zustimmung_bildungsbuergertum = 3", "zustimmung_adel = 3",
         "unbekannte Interessengruppe 'adel'"},
        {"unbekannter Staat", "staat_in = { BRA SAX }", "staat_in = { BRA HAN }", "unbekannter Staat 'HAN'"},
        {"unbekanntes Ereignis", "folge { ereignis = test.welt tage = 0 }", "folge { ereignis = test.fehlt }",
         "unbekanntes Ereignis 'test.fehlt'"},
        {"unbekannte Option", "option = b }", "option = c }", "unbekannte Option 'c' von Ereignis test.welt"},
        {"Zustimmung außerhalb", "zustimmung_bildungsbuergertum < 5", "zustimmung_bildungsbuergertum < 11",
         "'zustimmung_bildungsbuergertum' muss zwischen -10 und 10 liegen"},
        {"Vergleich", "datum >= 1648.11.01", "datum = { 1648 }", "unbekannte Bedingung 'datum'"},
        {"zwei historische", "gewicht = 30", "gewicht = 30 historisch = ja", "mehr als eine Option"},
        {"Gewichte 0", "option a { text = \"a\" gewicht = 1 }", "option a { text = \"a\" gewicht = 0 }",
         "müssen zusammen größer als 0 sein"},
        {"ohne Quelle", "quelle = \"Testdaten\"\n    sicherheit", "sicherheit", "Pflichtfeld 'quelle' fehlt"},
        {"ohne Text", "text = \"x\"", "", "Pflichtfeld 'text' fehlt in Ereignis test.dynamisch"},
        {"ohne Abstand", "abstand = 365", "", "braucht 'abstand'"},
        {"Termin und Wartezeit", "termin = 1649.01.01", "termin = 1649.01.01 wartezeit = 3",
         "schließen sich aus"},
        {"doppelte Kennung", "ereignis test.dynamisch {", "ereignis test.welt {",
         "doppelte Kennung 'test.welt'"},
        {"doppelte Option", "option b {", "option a {", "doppelte Option 'a'"},
        {"Staatsbedingung in der Welt", "staat SAX { unruhe > 1 }", "unruhe > 1",
         "'unruhe' braucht einen Staat"},
        {"Staatswirkung in der Welt", "chronik = \"Eintrag\"", "schatz = 1",
         "Wirkung 'schatz' braucht einen Staat"},
        {"Folge ohne Staat", "welt_merke = test", "folge { ereignis = test.dynamisch }",
         "braucht einen Staat (staat = TAG"},
        {"Kreis", "option a { text = \"a\" gewicht = 1 }",
         "option a { text = \"a\" gewicht = 1 wirkung { folge { ereignis = test.dynamisch } } }",
         "bilden einen Kreis: test.dynamisch → test.dynamisch"},
        {"Kennung", "ereignis test.dynamisch {", "ereignis Test {", "ungültige Kennung 'Test'"},
        {"Merkmal", "merke = nachgegeben", "merke = Nach-Gegeben", "ungültiger Name 'Nach-Gegeben'"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.name);
        std::string text(kEvents);
        const std::size_t pos = text.find(c.from);
        REQUIRE(pos != std::string::npos);
        text.replace(pos, c.from.size(), c.to);
        const EventData data(text);
        const uhrwerk::LoadResult result = data.result();
        INFO(testhilfen::issues_text(result));
        CHECK_FALSE(result.ok());
        CHECK(testhilfen::has_issue(result, c.message));
    }

    SUBCASE("Regeln ohne seit") {
        const EventData data;
        testhilfen::write_file(data.dir() / "common/ereignisse.chron", "ereignisse { frist = 3 }\n");
        CHECK(testhilfen::has_issue(data.result(), "Pflichtfeld 'seit' fehlt in ereignisse"));
    }
    SUBCASE("Gesetz eines Staates") {
        const EventData data;
        testhilfen::replace_in_file(data.dir() / "history/staaten/10_brandenburg.chron",
                                    "gesetz_presse = pressefreiheit", "gesetz_presse = frei");
        CHECK(testhilfen::has_issue(data.result(), "'frei'"));
    }
}

TEST_CASE("Ereignisse: Spielstand und Fingerabdruck") {
    const EventData data;
    Simulation sim(data.load());
    sim.step_days(30);
    const Bytes bytes = uhrwerk::save_game(sim, SaveMeta{});
    const auto info = uhrwerk::read_save_info(bytes);
    REQUIRE(info);
    CHECK(info->format_version == 5);
    uhrwerk::LoadedGame loaded = uhrwerk::load_game(bytes, data.load(), 1);
    INFO(loaded.error);
    REQUIRE(loaded.ok());
    CHECK(loaded.simulation->checksum() == sim.checksum());
    CHECK(loaded.simulation->world().unrest == sim.world().unrest);
    CHECK(loaded.simulation->world().states[0].laws == sim.world().states[0].laws);

    // Ereignisse zählen zur Prüfsumme der Welt mit Ereignissen: ohne sie ist der Zustand ein anderer.
    World without = data.load();
    without.event_rules.active = false;
    CHECK(Simulation(without).checksum() != Simulation(data.load()).checksum());

    // Ein anderes Gewicht ist anderer Inhalt: Der Spielstand passt nicht mehr.
    testhilfen::replace_in_file(data.dir() / "common/ereignisse/test.chron", "gewicht = 70", "gewicht = 60");
    const World changed = data.load();
    CHECK(uhrwerk::content_fingerprint(changed).parts[kEventPart] !=
          uhrwerk::content_fingerprint(sim.world()).parts[kEventPart]);
    const uhrwerk::LoadedGame refused = uhrwerk::load_game(bytes, changed, 1);
    CHECK_FALSE(refused.ok());
    CHECK(refused.error.find("ereignisse") != std::string::npos);
}

// ─── Schritt a2: Auslösung, Wahl, Ereigniswahl, Folgen, Wirkungen ──────────

namespace {

using uhrwerk::Command;
using uhrwerk::CommandType;

constexpr std::string_view kRun = R"(
ereignis a.sofort {
    titel = "Sofort" text = "x" art = dynamisch
    option ja {
        text = "Ja" gewicht = 1
        wirkung {
            gesetz_presse = pressefreiheit
            zustimmung_bildungsbuergertum = 15
            unruhe = 40
            merke = sofort
            schatz = 1000
            folge { ereignis = b.folge tage = 0 }
            folge { ereignis = c.spaeter tage = 10 }
        }
    }
}
ereignis b.folge {
    titel = "Folge" text = "x" art = dynamisch nur_folge = ja
    bedingung { merkmal = sofort }
    option a { text = "a" gewicht = 1 wirkung { unruhe = 100 chronik = "Folge eingetreten" } }
}
ereignis c.spaeter {
    titel = "Später" text = "x" art = dynamisch nur_folge = ja
    option a { text = "a" gewicht = 1 wirkung { vergiss = sofort } }
}
ereignis d.termin {
    titel = "Termin" text = "x" art = atmosphaerisch wer = welt termin = 1648.11.01
    option a { text = "a" gewicht = 1 wirkung { welt_merke = termin staat SAX { schatz = -500 } } }
}
ereignis e.warten {
    titel = "Warten" text = "x" art = dynamisch wartezeit = 20
    bedingung { staat = ANH }
    option a { text = "a" gewicht = 1 }
}
ereignis f.wahl {
    titel = "Wahl" text = "x" art = dynamisch wiederholbar = ja abstand = 1
    bedingung { staat = SAX }
    option selten { text = "selten" gewicht = 30 }
    option oft { text = "oft" gewicht = 70 }
    option nie { text = "nie" gewicht = 100 bedingung { merkmal = gibt_es_nicht } }
}
)";

std::size_t ix(Index i) {
    return static_cast<std::size_t>(i);
}

uhrwerk::Index state_of(const World& w, std::string_view tag) {
    const Index s = w.find_state(tag);
    REQUIRE(s != kNone);
    return s;
}

std::size_t count_log(const World& w, std::string_view event, Index state, Index option = kNone) {
    const Index e = w.find_event(event);
    std::size_t n = 0;
    for (const uhrwerk::EventRecord& r : w.event_log) {
        if (r.event == e && r.state == state && (option == kNone || r.option == option)) {
            ++n;
        }
    }
    return n;
}

bool has_chronicle(const World& w, std::string_view text) {
    return std::any_of(w.chronicle.begin(), w.chronicle.end(), [&](const uhrwerk::ChronicleEntry& e) {
        return e.text.find(text) != std::string::npos;
    });
}

Command command(CommandType type, Index state, std::int64_t value = 0, Index target = kNone) {
    Command c;
    c.type = type;
    c.state = state;
    c.value = Fixed::from_int(value);
    c.target = target;
    return c;
}

} // namespace

TEST_CASE("Ereignisse: Auslösung, Folgen und Wirkungen") {
    const EventData data(kRun);
    Simulation sim(data.load());
    const World& w = sim.world();
    const Index bra = state_of(w, "BRA");
    const Index sax = state_of(w, "SAX");
    const Index anh = state_of(w, "ANH");
    sim.step_day();

    // Ohne Wartezeit sofort, je Staat einmal; die Folge ohne Frist am selben Tag.
    for (const Index s : {bra, sax, anh}) {
        CAPTURE(s);
        CHECK(count_log(w, "a.sofort", s) == 1);
        CHECK(count_log(w, "b.folge", s) == 1);
        const uhrwerk::State& st = w.states[ix(s)];
        CHECK(st.laws[0] == 1);                         // Pressefreiheit
        CHECK(w.unrest[ix(s)] == Fixed::from_int(100)); // 40, dann 100 (begrenzt)
        const std::size_t group = ix(w.find_interest_group("bildungsbuergertum"));
        CHECK(w.approval[ix(s) * w.interest_groups.size() + group] == Fixed::from_int(10));
    }
    CHECK(has_chronicle(w, "Folge eingetreten"));
    CHECK(has_chronicle(w, "Sofort – Ja"));
    CHECK(w.scheduled_events.size() == 3); // c.spaeter je Staat in zehn Tagen
    sim.step_days(5);
    CHECK(count_log(w, "a.sofort", bra) == 1); // nicht wiederholbar

    // Folge mit Frist: nach zehn Tagen, Merkmal wieder gelöscht.
    CHECK(std::any_of(w.event_flags.begin(), w.event_flags.end(),
                      [&](const uhrwerk::EventFlag& f) { return f.state == bra && f.name == "sofort"; }));
    sim.step_days(5);
    CHECK(count_log(w, "c.spaeter", bra) == 1);
    CHECK(w.scheduled_events.empty());
    CHECK(std::none_of(w.event_flags.begin(), w.event_flags.end(),
                       [](const uhrwerk::EventFlag& f) { return f.name == "sofort"; }));

    // Termin am 1. November 1648: ein Welt-Ereignis, Wirkung auf Sachsen.
    while (w.date <= uhrwerk::Date{1648, 11, 1}) {
        sim.step_day();
    }
    CHECK(count_log(w, "d.termin", kNone) == 1);
    CHECK(std::any_of(w.event_flags.begin(), w.event_flags.end(),
                      [](const uhrwerk::EventFlag& f) { return f.state == kNone && f.name == "termin"; }));

    // Wartezeit 20: tritt für Anhalt ein, nur für Anhalt.
    sim.step_days(200);
    CHECK(count_log(w, "e.warten", anh) == 1);
    CHECK(count_log(w, "e.warten", bra) == 0);

    // Wahl nach Gewicht: täglich wiederholbar für Sachsen, nicht verfügbare Option nie.
    const Index f = w.find_event("f.wahl");
    std::size_t rare = count_log(w, "f.wahl", sax, 0);
    std::size_t often = count_log(w, "f.wahl", sax, 1);
    CHECK(count_log(w, "f.wahl", sax, 2) == 0);
    CHECK(rare + often == count_log(w, "f.wahl", sax));
    CHECK(rare + often > 200);
    CHECK(often > 2 * rare); // 70 zu 30
    CHECK(often < 4 * rare);
    CHECK(f != kNone);
}

TEST_CASE("Ereignisse: Wahl des Spielers, Frist und Befehl") {
    const EventData data(kRun);
    Simulation sim(data.load());
    const World& w = sim.world();
    const Index bra = state_of(w, "BRA");
    const Index sax = state_of(w, "SAX");
    const Index sofort = w.find_event("a.sofort");
    sim.submit(command(CommandType::EventPlayer, bra));
    sim.step_day();
    CHECK(w.event_player == bra);
    // Brandenburg wartet, Sachsen wählt sofort.
    REQUIRE(w.pending_events.size() == 1);
    CHECK(w.pending_events[0].event == sofort);
    CHECK(w.pending_events[0].due == uhrwerk::Date{1648, 11, 13}); // Frist 20 Tage
    CHECK(count_log(w, "a.sofort", bra) == 0);
    CHECK(count_log(w, "a.sofort", sax) == 1);

    SUBCASE("Befehl Ereigniswahl") {
        sim.submit(command(CommandType::EventChoice, sax, sofort, 0)); // kein offenes Ereignis Sachsens
        sim.submit(command(CommandType::EventChoice, bra, sofort, 5)); // keine solche Option
        sim.step_day();
        CHECK(w.pending_events.size() == 1);
        sim.submit(command(CommandType::EventChoice, bra, sofort, 0));
        sim.step_day();
        CHECK(count_log(w, "a.sofort", bra) == 1);
        // Die Folge ohne Frist wartet ebenfalls auf die Wahl des Spielers.
        REQUIRE(w.pending_events.size() == 1);
        CHECK(w.pending_events[0].event == w.find_event("b.folge"));
        sim.submit(command(CommandType::EventPlayer, kNone));
        sim.submit(command(CommandType::EventChoice, bra, w.find_event("b.folge"), 0));
        sim.step_day();
        CHECK(w.event_player == kNone);
        CHECK(w.pending_events.empty());
        CHECK(count_log(w, "b.folge", bra) == 1);
    }
    SUBCASE("Frist") {
        sim.step_days(19); // bis zum 12. November
        CHECK(w.pending_events.size() == 1);
        CHECK(count_log(w, "a.sofort", bra) == 0);
        sim.step_day(); // 13. November: Frist
        CHECK(count_log(w, "a.sofort", bra, 0) == 1);
        REQUIRE(w.pending_events.size() == 1); // die Folge wartet
        CHECK(w.pending_events[0].event == w.find_event("b.folge"));
    }
    SUBCASE("Spielstand und Wiederholung mit offenem Ereignis") {
        const Bytes bytes = uhrwerk::save_game(sim, SaveMeta{});
        uhrwerk::LoadedGame loaded = uhrwerk::load_game(bytes, data.load(), 1);
        INFO(loaded.error);
        REQUIRE(loaded.ok());
        CHECK(loaded.simulation->world().pending_events == w.pending_events);
        CHECK(loaded.simulation->world().scheduled_events == w.scheduled_events);
        sim.submit(command(CommandType::EventChoice, bra, sofort, 0));
        loaded.simulation->submit(command(CommandType::EventChoice, bra, sofort, 0));
        sim.step_days(40);
        loaded.simulation->step_days(40);
        CHECK(loaded.simulation->checksum() == sim.checksum());
        const uhrwerk::Replay replay = uhrwerk::make_replay(sim);
        const uhrwerk::ReplayResult result = uhrwerk::run_replay(replay, data.load(), 2);
        INFO(result.error);
        CHECK(result.ok);
    }
}

TEST_CASE("Ereignisse: befristete Wirkungen und Bevölkerung") {
    const EventData data(R"(
ereignis a.befristet {
    titel = "Belagerungszustand" text = "x" art = dynamisch
    bedingung { staat = SAX }
    option a {
        text = "a" gewicht = 1
        wirkung {
            befristet { tage = 40 text = "Belagerungszustand" steuerertrag = -0.5 unruhe = 3 }
            auswanderung = 0.1
            verwuestung { ort = dresden wert = 0.2 }
            zufall { 1 = { merke = eins } 0 = { merke = zwei } }
        }
    }
}
)");
    const World start = data.load();
    const Index sax = state_of(start, "SAX");
    const Fixed people = start.state_population(sax);
    Simulation sim(start);
    const World& w = sim.world();
    sim.step_day();
    REQUIRE(w.timed_effects.size() == 1);
    CHECK(w.timed_effects[0].text == "Belagerungszustand");
    CHECK(uhrwerk::timed_tax_factor(w, sax) == Fixed::from_ratio(1, 2));
    CHECK(uhrwerk::timed_tax_factor(w, state_of(w, "BRA")) == Fixed::one());
    CHECK(w.state_population(sax) < people);
    CHECK(w.locations[ix(w.find_location("dresden"))].devastation >= Fixed::from_ratio(1, 5));
    CHECK(std::any_of(w.event_flags.begin(), w.event_flags.end(),
                      [](const uhrwerk::EventFlag& f) { return f.name == "eins"; }));
    // Unruhe je Monat am Monatsersten (1. November und 1. Dezember), dann läuft die Wirkung aus.
    sim.step_days(45);
    CHECK(w.timed_effects.empty());
    CHECK(w.unrest[ix(sax)] == Fixed::from_int(6));
}

TEST_CASE("Ereignisse: Lader a2") {
    for (const auto& [text, message] :
         {std::pair{"ereignis x { titel = \"x\" text = \"x\" art = dynamisch nur_folge = ja wartezeit = 3 "
                    "option a { text = \"a\" gewicht = 1 } }",
                    "'nur_folge' schließt"},
          std::pair{
              "ereignis x { titel = \"x\" text = \"x\" art = dynamisch option a { text = \"a\" gewicht = 1 "
              "wirkung { befristet { tage = 3 text = \"t\" } } } }",
              "braucht mindestens einen Wert"},
          std::pair{
              "ereignis x { titel = \"x\" text = \"x\" art = dynamisch wer = welt option a { text = \"a\" "
              "gewicht = 1 wirkung { befristet { tage = 3 text = \"t\" moral = 0.1 } } } }",
              "'befristet' braucht einen Staat"},
          std::pair{
              "ereignis x { titel = \"x\" text = \"x\" art = dynamisch option a { text = \"a\" gewicht = 1 "
              "wirkung { befristet { text = \"t\" moral = 2 } } } }",
              "'moral' muss zwischen -1 und 1 liegen"}}) {
        CAPTURE(text);
        const EventData data(text);
        const uhrwerk::LoadResult result = data.result();
        INFO(testhilfen::issues_text(result));
        CHECK(testhilfen::has_issue(result, message));
    }
}

TEST_CASE("Ereignisse: Determinismus mit 1 und 4 Threads") {
    const EventData data(kRun);
    const auto run = [&](unsigned threads) {
        uhrwerk::SimConfig config;
        config.threads = threads;
        Simulation sim(data.load(), config);
        sim.submit(command(CommandType::EventPlayer, sim.world().find_state("ANH")));
        sim.step_days(120);
        return sim.checksum();
    };
    CHECK(run(1) == run(4));
}

// ─── Schritt a3: Zustimmung, Unruhe, Aufstände ─────────────────────────────

namespace {

/// Fixture mit Ereignissen und eigenen Regeln (ereignisse { … }); ohne Ereignisse, die von selbst eintreten,
/// außer denen in events.
class RuleData : public EventData {
public:
    RuleData(std::string_view rules, std::string_view events) : EventData(events) {
        testhilfen::write_file(dir() / "common/ereignisse.chron",
                               "ereignisse {\n    seit = 1648.01.01\n" + std::string(rules) + "\n}\n");
    }
};

constexpr std::string_view kQuiet = R"(
ereignis z.nie { titel = "x" text = "x" art = dynamisch nur_folge = ja option a { text = "a" gewicht = 1 } }
)";

Fixed fx(std::int64_t n, std::int64_t d = 1) {
    return Fixed::from_ratio(n, d);
}

} // namespace

TEST_CASE("Ereignisse: Zustimmung und Unruhe am Monatsersten") {
    const RuleData data("    unruhe_not = 0\n    unruhe_teuerung = 0\n    unruhe_zustimmung = 0\n"
                        "    unruhe_verfassung = { keine = 40 staendisch = 20 konstitutionell = 0 }\n",
                        kQuiet);
    testhilfen::replace_in_file(data.dir() / "common/gesetze.chron",
                                "option vorzensur { name = \"Vorzensur\" }",
                                "option vorzensur { name = \"Vorzensur\" unruhe = 10 }");
    Simulation sim(data.load());
    const World& w = sim.world();
    const Index bra = state_of(w, "BRA");
    const Index sax = state_of(w, "SAX");
    const std::size_t groups = w.interest_groups.size();
    const std::size_t educated = ix(w.find_interest_group("bildungsbuergertum"));
    CHECK(w.law_groups[0].options[0].unrest == Fixed::from_int(10));
    sim.step_days(8); // bis einschließlich 31. Oktober
    CHECK(w.unrest[ix(sax)] == Fixed::zero());
    sim.step_day(); // 1. November
    // Zielwert Sachsen: keine Verfassung 40 + Vorzensur 10; Brandenburg mit Pressefreiheit 40.
    CHECK(w.unrest[ix(sax)] == fx(50) * fx(1, 4));
    CHECK(w.unrest[ix(bra)] == fx(40) * fx(1, 4));
    // Zustimmung: Bildungsbürgertum wünscht Pressefreiheit +2, Vorzensur −2; ein Fünftel der Lücke.
    CHECK(w.approval[ix(bra) * groups + educated] == fx(2) * fx(1, 5));
    CHECK(w.approval[ix(sax) * groups + educated] == fx(-2) * fx(1, 5));
    uhrwerk::EventCache cache;
    uhrwerk::rebuild_event_cache(w, cache);
    CHECK(uhrwerk::unrest_target(w, cache, sax) == fx(50));

    // Hohe Unruhe: Steuerverweigerung und mehr Auswanderung der unteren Schicht.
    World high = data.load();
    high.unrest[ix(sax)] = fx(60);
    CHECK(uhrwerk::event_tax_factor(high, sax) == fx(9, 10));
    CHECK(uhrwerk::event_tax_factor(high, bra) == Fixed::one());
    CHECK(uhrwerk::unrest_emigration_factor(high, sax) == fx(3, 2));
}

TEST_CASE("Ereignisse: Ansteckung über ein Welt-Merkmal") {
    const RuleData data("    unruhe_not = 0\n    unruhe_teuerung = 0\n    unruhe_zustimmung = 0\n"
                        "    unruhe_verfassung = { keine = 0 }\n    unruhe_ansteckung = 10\n"
                        "    ansteckung_merkmal = februarrevolution\n",
                        R"(
ereignis paris { titel = "Paris" text = "x" art = dynamisch wer = welt
    option a { text = "a" gewicht = 1 wirkung { welt_merke = februarrevolution } } }
)");
    Simulation sim(data.load());
    const World& w = sim.world();
    sim.step_day();
    uhrwerk::EventCache cache;
    uhrwerk::rebuild_event_cache(w, cache);
    // Ohne Nachbarschaft der Orte (kein Militär in der Fixture) zählt nur das Merkmal: Anteil 1.
    CHECK(uhrwerk::unrest_target(w, cache, state_of(w, "SAX")) == fx(10));
}

TEST_CASE("Ereignisse: Aufstände") {
    const std::string revolt_event = R"(
ereignis a.aufstand { titel = "Aufstand" text = "x" art = dynamisch
    bedingung { staat = SAX }
    option a { text = "a" gewicht = 1 wirkung { unruhe = UNRUHE aufstand = ART } } }
ereignis b.hilfe { titel = "Hilfe" text = "x" art = dynamisch
    bedingung { staat = BRA  staat SAX { aufstand = ja } }
    option a { text = "a" gewicht = 1 wirkung { intervention = SAX } } }
ereignis c.standrecht { titel = "Standrecht" text = "x" art = dynamisch nur_folge = ja
    option a { text = "a" gewicht = 1 wirkung { merke = standrecht } } }
)";
    const auto make = [&](std::string_view unrest, std::string_view kind) {
        std::string text = revolt_event;
        text.replace(text.find("UNRUHE"), 6, unrest);
        text.replace(text.find("ART"), 3, kind);
        return text;
    };
    const auto flag = [](const World& w, Index s, std::string_view name) {
        return std::any_of(w.event_flags.begin(), w.event_flags.end(),
                           [&](const uhrwerk::EventFlag& f) { return f.state == s && f.name == name; });
    };

    SUBCASE("Niederschlagung mit Folgeereignis") {
        const RuleData data("    aufstand_schritt = 4\n    aufstand_zufall = 0\n"
                            "    folge_niederschlagung = c.standrecht\n",
                            make("0", "barrikaden"));
        Simulation sim(data.load());
        const World& w = sim.world();
        const Index sax = state_of(w, "SAX");
        sim.step_day();
        REQUIRE(w.revolts.size() == 1);
        CHECK(w.revolts[0].kind == uhrwerk::RevoltKind::Barricades);
        CHECK(has_chronicle(w, "Barrikadenkämpfe"));
        // Brandenburg greift ein (am selben Tag, nach Sachsen in der Reihenfolge der Kennungen).
        CHECK(w.revolts[0].helper == state_of(w, "BRA"));
        CHECK(has_chronicle(w, "schickt Truppen gegen den Aufstand"));
        const Index capital = w.states[ix(sax)].capital;
        const Fixed devastation = w.locations[ix(capital)].devastation;
        sim.step_days(7);
        CHECK(w.revolts.empty());
        CHECK(has_chronicle(w, "niedergeschlagen"));
        CHECK(flag(w, sax, "standrecht"));
        CHECK(w.locations[ix(capital)].devastation == devastation + fx(1, 20));
    }
    SUBCASE("Sieg der Aufständischen") {
        const RuleData data("    aufstand_schritt = 2\n    aufstand_zufall = 0\n", make("100", "erhebung"));
        // Ohne Helfer: Brandenburg greift nicht ein.
        Simulation sim(data.load());
        const World& w = sim.world();
        sim.submit(command(CommandType::EventPlayer, state_of(w, "BRA")));
        sim.step_days(8);
        CHECK(w.revolts.empty());
        CHECK(flag(w, state_of(w, "SAX"), "revolutionsregierung"));
        CHECK(has_chronicle(w, "Die Aufständischen siegen"));
    }
    SUBCASE("Nachgeben nach der Höchstdauer, Spielstand mit laufendem Aufstand") {
        const RuleData data(
            "    aufstand_schritt = 0.5\n    aufstand_zufall = 0\n    aufstand_nachgeben = 0\n"
            "    aufstand_tage = { 7 14 }\n",
            make("100", "erhebung"));
        Simulation sim(data.load());
        const World& w = sim.world();
        sim.submit(command(CommandType::EventPlayer, state_of(w, "BRA"))); // Brandenburg wartet, hilft nicht
        sim.step_days(8);
        REQUIRE(w.revolts.size() == 1);
        CHECK(w.revolts[0].strength > Fixed::zero());
        const Bytes bytes = uhrwerk::save_game(sim, SaveMeta{});
        uhrwerk::LoadedGame loaded = uhrwerk::load_game(bytes, data.load(), 1);
        INFO(loaded.error);
        REQUIRE(loaded.ok());
        CHECK(loaded.simulation->world().revolts == w.revolts);
        sim.step_days(7);
        loaded.simulation->step_days(7);
        CHECK(loaded.simulation->checksum() == sim.checksum());
        CHECK(w.revolts.empty());
        CHECK(has_chronicle(w, "gibt den Aufständischen nach"));
    }
}

TEST_CASE("Ereignisse: Lader a3") {
    struct Case {
        std::string_view rules;
        std::string_view events;
        std::string_view message;
    };
    const Case cases[] = {
        {"    unruhe_verfassung = { republik = 3 }\n", kQuiet, "ungültige Verfassung 'republik'"},
        {"    spaltung_gruppen = { adel }\n", kQuiet, "unbekannte Interessengruppe 'adel'"},
        {"    folge_sieg = gibt.es.nicht\n", kQuiet, "unbekanntes Ereignis 'gibt.es.nicht'"},
        {"    getreide = brot\n", kQuiet, "unbekanntes Gut 'brot'"},
        {"    aufstand_tage = { 9 3 }\n", kQuiet, "'aufstand_tage' erwartet"},
        {"    unruhe_annaeherung = 2\n", kQuiet, "'unruhe_annaeherung' muss zwischen 0 und 1 liegen"},
        {"",
         "ereignis x { titel = \"x\" text = \"x\" art = dynamisch option a { text = \"a\" gewicht = 1 "
         "wirkung { aufstand = putsch } } }",
         "ungültige Art 'putsch' für 'aufstand'"},
        {"",
         "ereignis x { titel = \"x\" text = \"x\" art = dynamisch option a { text = \"a\" gewicht = 1 "
         "wirkung { intervention = XYZ } } }",
         "unbekannter Staat 'XYZ'"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.message);
        const RuleData data(c.rules, c.events);
        const uhrwerk::LoadResult result = data.result();
        INFO(testhilfen::issues_text(result));
        CHECK_FALSE(result.ok());
        CHECK(testhilfen::has_issue(result, c.message));
    }
}

// ─── Schritt a4: Journale, Abstimmungen, Bundesversammlung ─────────────────

namespace {

constexpr std::string_view kJournal = R"(
journal test {
    titel  = "Testjournal"
    text   = "Ein Journal."
    beginn = 1648.11.01
    ende   = 1649.06.01
    stufe eins {
        titel = "Erste Stufe"
        ziel { text = "Merkmal x" bedingung { welt_merkmal = x } }
        ziel { text = "Staaten mit Schatz" zaehle { schatz >= 0 } soll = 3 }
        weiter { zu = zwei  wenn { welt_merkmal = x } }
    }
    stufe zwei {
        titel = "Zweite Stufe"
        weiter { zu = sieg  wenn { welt_merkmal = y } }
    }
    ausgang sieg { titel = "Sieg" wertung = gelungen wirkung { welt_merke = gesiegt } }
    ausgang ende { titel = "Ende" wertung = gescheitert standard = ja }
}
)";

constexpr std::string_view kJournalEvents = R"(
ereignis a.x { titel = "x" text = "x" art = dynamisch wer = welt termin = 1648.12.01
    option a { text = "a" gewicht = 1 wirkung { welt_merke = x } } }
ereignis b.zwei { titel = "zwei" text = "x" art = dynamisch wer = welt
    bedingung { journal_test = zwei }
    option a { text = "a" gewicht = 1 wirkung { welt_merke = in_zwei } } }
ereignis c.zaehlen { titel = "z" text = "x" art = dynamisch wer = welt
    bedingung { staaten { wenn { schatz >= 0 } mindestens = 3 }
                nicht { bevoelkerungsanteil { wenn { staat = BRA } mindestens = 0.99 } } }
    option a { text = "a" gewicht = 1 wirkung { welt_merke = gezaehlt } } }
)";

bool world_flag(const World& w, std::string_view name) {
    return std::any_of(w.event_flags.begin(), w.event_flags.end(),
                       [&](const uhrwerk::EventFlag& f) { return f.state == kNone && f.name == name; });
}

void step_until(Simulation& sim, uhrwerk::Date date) {
    while (sim.world().date <= date) {
        sim.step_day();
    }
}

} // namespace

TEST_CASE("Ereignisse: Journal") {
    const RuleData data("", kJournalEvents);
    testhilfen::write_file(data.dir() / "common/journale/test.chron", kJournal);
    const World start = data.load();
    REQUIRE(start.journals.size() == 1);
    const uhrwerk::JournalDef& j = start.journals[0];
    CHECK(j.stages.size() == 4);
    CHECK(j.fallback == 3);
    CHECK(j.stages[0].goals.size() == 2);
    CHECK(j.stages[0].goals[1].counting);
    CHECK(j.stages[0].goals[1].target == 3);
    CHECK(j.stages[2].outcome);
    CHECK(j.stages[2].rating == "gelungen");
    Simulation sim(start);
    const World& w = sim.world();
    sim.step_day();
    CHECK(world_flag(w, "gezaehlt")); // drei Staaten, Brandenburg unter 99 % der Bevölkerung
    CHECK(w.journal_states[0].stage == kNone);
    step_until(sim, uhrwerk::Date{1648, 11, 1});
    CHECK(w.journal_states[0].stage == 0);
    CHECK(has_chronicle(w, "Testjournal: Erste Stufe."));
    step_until(sim, uhrwerk::Date{1648, 12, 1});
    CHECK(w.journal_states[0].stage == 1); // am Termin x, dann der Übergang am selben Tag
    CHECK(w.journal_states[0].since == uhrwerk::Date{1648, 12, 1});
    sim.step_day();
    CHECK(world_flag(w, "in_zwei"));
    REQUIRE(w.journal_history.size() == 2);

    SUBCASE("Ende ohne Übergang: Ausgang standard") {
        step_until(sim, uhrwerk::Date{1649, 6, 1});
        CHECK(w.journal_states[0].stage == 3);
        CHECK(has_chronicle(w, "Testjournal endet: Ende (gescheitert)."));
        CHECK_FALSE(world_flag(w, "gesiegt"));
    }
    SUBCASE("Spielstand mit Journal") {
        const Bytes bytes = uhrwerk::save_game(sim, SaveMeta{});
        uhrwerk::LoadedGame loaded = uhrwerk::load_game(bytes, data.load(), 1);
        INFO(loaded.error);
        REQUIRE(loaded.ok());
        CHECK(loaded.simulation->world().journal_states == w.journal_states);
        CHECK(loaded.simulation->world().journal_history == w.journal_history);
        sim.step_days(30);
        loaded.simulation->step_days(30);
        CHECK(loaded.simulation->checksum() == sim.checksum());
    }
}

TEST_CASE("Ereignisse: Abstimmung im Parlament") {
    const RuleData data("", R"(
ereignis a.abstimmen { titel = "Abstimmung" text = "x" art = dynamisch wer = welt termin = 1650.03.05
    option a { text = "a" gewicht = 1 wirkung { abstimmung = verfassung } } }
ereignis b.danach { titel = "danach" text = "x" art = dynamisch wer = welt
    bedingung { abstimmung_verfassung = angenommen }
    option a { text = "a" gewicht = 1 wirkung { welt_merke = verfassung_da } } }
)");
    testhilfen::write_file(data.dir() / "common/abstimmungen.chron", R"(
abstimmung verfassung {
    titel = "Landesverfassung"
    parlament = landtag
    neigung = { rechte = 0 mitte = 1 linke = 1 }
    fraktionslos = 1
    ja { welt_merke = angenommen }
    nein { welt_merke = abgelehnt }
}
)");
    uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path(), "jahresbeginn_1650");
    INFO(testhilfen::issues_text(result));
    REQUIRE(result.ok());
    REQUIRE(result.world->votes.size() == 1);
    CHECK(result.world->votes[0].leaning.size() == 3);
    Simulation sim(std::move(*result.world));
    const World& w = sim.world();
    step_until(sim, uhrwerk::Date{1650, 3, 5});
    REQUIRE(w.elections.size() == 1);
    REQUIRE(w.vote_results.size() == 1);
    const uhrwerk::VoteResult& r = w.vote_results[0];
    CHECK(r.yes + r.no == static_cast<std::int32_t>(w.elections[0].deputies.size()));
    CHECK(world_flag(w, r.accepted ? "angenommen" : "abgelehnt"));
    CHECK(has_chronicle(w, "Sächsischer Landtag: Landesverfassung"));
    sim.step_day();
    CHECK(world_flag(w, "verfassung_da") == r.accepted);

    // Ohne Wahl findet die Abstimmung nicht statt (westfälischer Frieden: Landtag erst ab 1649).
    const World before = data.load();
    CHECK(before.votes[0].parliament == kNone);
}

TEST_CASE("Ereignisse: Bundesversammlung ruht") {
    const RuleData data("", R"(
ereignis a.ruhe { titel = "Ruhe" text = "x" art = dynamisch wer = welt
    option a { text = "a" gewicht = 1 wirkung { bundesversammlung = ruht } } }
ereignis b.tagt { titel = "Tagt" text = "x" art = dynamisch wer = welt termin = 1648.12.01
    bedingung { bundesversammlung = ruht }
    option a { text = "a" gewicht = 1 wirkung { bundesversammlung = tagt } } }
)");
    // Deutscher Bund wie in den Diplomatietests: Orte im Bundesgebiet, Bundesversammlung mit Plenum.
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
    testhilfen::write_file(
        data.dir() / "common/bundesversammlung.chron",
        "bundesversammlung bund {\n    name = \"Bundesversammlung\"\n    praesidium = SAX\n"
        "    plenum = { BRA = 4 SAX = 4 ANH = 1 }\n"
        "    rat = { stimme = { staaten = { BRA } } stimme = { staaten = { SAX } } "
        "stimme = { staaten = { ANH } } }\n    quelle = \"Testdaten\"\n}\n");
    Simulation sim(data.load());
    const World& w = sim.world();
    REQUIRE_FALSE(w.diet.id.empty());
    sim.step_day();
    CHECK(w.diet_suspended == 1);
    CHECK(has_chronicle(w, "Die Bundesversammlung ruht."));
    // Kein Antrag, solange sie ruht.
    sim.submit(command(CommandType::Motion, state_of(w, "BRA"),
                       static_cast<std::int64_t>(uhrwerk::MotionKind::Reform)));
    sim.step_day();
    CHECK(w.motions.empty());
    step_until(sim, uhrwerk::Date{1648, 12, 1});
    CHECK(w.diet_suspended == 0);
    CHECK(has_chronicle(w, "tritt wieder zusammen"));
}

TEST_CASE("Ereignisse: Lader a4") {
    struct Case {
        std::string_view file;
        std::string_view text;
        std::string_view message;
    };
    const std::string journal(kJournal);
    const auto variant = [&](std::string_view from, std::string_view to) {
        std::string text = journal;
        text.replace(text.find(from), from.size(), to);
        return text;
    };
    const std::string no_stage = variant("    stufe eins {", "    ausgang eins {");
    const std::string bad_target = variant("zu = zwei", "zu = drei");
    const std::string no_fallback = variant("standard = ja", "");
    const std::string bad_goal = variant("soll = 3", "");
    const Case cases[] = {
        {"common/journale/test.chron", no_stage, "braucht als Erstes eine Stufe"},
        {"common/journale/test.chron", bad_target, "unbekannte Stufe 'drei' des Journals test"},
        {"common/journale/test.chron", no_fallback, "keinen Ausgang mit standard = ja"},
        {"common/journale/test.chron", bad_goal, "'ziel' braucht entweder bedingung oder zaehle mit soll"},
        {"common/abstimmungen.chron",
         "abstimmung v { titel = \"v\" parlament = landtag neigung = { mitte = 1 } }\n"
         "abstimmung v { titel = \"w\" parlament = landtag neigung = { mitte = 1 } }",
         "doppelte Kennung 'v' für Abstimmung"},
        {"common/abstimmungen.chron", "abstimmung v { titel = \"v\" parlament = reichstag neigung = { } }",
         "unbekanntes Parlament 'reichstag'"},
    };
    for (const Case& c : cases) {
        CAPTURE(c.message);
        const RuleData data("", kQuiet);
        testhilfen::write_file(data.dir() / c.file, c.text);
        uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path(), "jahresbeginn_1650");
        INFO(testhilfen::issues_text(result));
        CHECK_FALSE(result.ok());
        CHECK(testhilfen::has_issue(result, c.message));
    }
    SUBCASE("Verweise aus Ereignissen") {
        const RuleData data("", R"(
ereignis x { titel = "x" text = "x" art = dynamisch wer = welt bedingung { journal_test = vier }
    option a { text = "a" gewicht = 1 wirkung { abstimmung = gibt_es_nicht } } }
)");
        testhilfen::write_file(data.dir() / "common/journale/test.chron", kJournal);
        const uhrwerk::LoadResult result = data.result();
        INFO(testhilfen::issues_text(result));
        CHECK(testhilfen::has_issue(result, "unbekannte Stufe 'vier' des Journals test"));
        CHECK(testhilfen::has_issue(result, "unbekannte Abstimmung 'gibt_es_nicht'"));
    }
    SUBCASE("Fraktion") {
        const RuleData data("", kQuiet);
        testhilfen::write_file(data.dir() / "common/abstimmungen.chron",
                               "abstimmung v { titel = \"v\" parlament = landtag neigung = { casino = 1 } }");
        uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path(), "jahresbeginn_1650");
        CHECK(testhilfen::has_issue(result, "unbekannte Fraktion 'casino' des Parlaments landtag"));
    }
}

// ─── Schritt a5: Ereignislabor ─────────────────────────────────────────────

TEST_CASE("Ereignisse: Ereignislabor") {
    testhilfen::TempDir dir(true);
    std::filesystem::copy(testhilfen::fixture_dir().parent_path() / "daten_ereignisse", dir.path(),
                          std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing);
    const uhrwerk::LoadResult loaded = uhrwerk::load_world(dir.path());
    INFO(testhilfen::issues_text(loaded));
    REQUIRE(loaded.ok());
    const World& w = *loaded.world;
    const uhrwerk::EventTargetsResult targets =
        uhrwerk::load_event_targets(dir / "common/ereignisziele_westfaelischer_frieden.chron", w);
    const std::string first_error = targets.errors.empty() ? std::string() : targets.errors.front();
    INFO(first_error);
    REQUIRE(targets.ok());
    const uhrwerk::EventTargets& t = targets.targets;
    CHECK(t.until == uhrwerk::Date{1650, 12, 31});
    CHECK(t.journal == w.find_journal("revolution"));
    CHECK(t.outcomes.size() == 2);
    CHECK(t.events.size() == 2);
    CHECK(t.events[0].states);
    REQUIRE(t.strategies.size() == 2);
    CHECK(t.strategies[1].choices.size() == 2);

    std::vector<uhrwerk::EventLabRun> runs;
    for (std::uint64_t seed = 1648; seed < 1652; ++seed) {
        runs.push_back(uhrwerk::run_event_lab(w, uhrwerk::SimConfig{seed, 1}, t));
    }
    // Gleicher Seed, gleicher Lauf.
    const uhrwerk::EventLabRun again = uhrwerk::run_event_lab(w, uhrwerk::SimConfig{1648, 2}, t);
    CHECK(again.outcome == runs[0].outcome);
    CHECK(again.path == runs[0].path);
    for (const uhrwerk::EventLabRun& run : runs) {
        CHECK(run.outcome != kNone);
        CHECK(run.path.size() == 3);
        REQUIRE(run.turning.size() == 1);
        CHECK(run.events[1] == Fixed::one()); // die Kaiserkrone wird immer angeboten
    }
    const uhrwerk::EventLabSummary summary = uhrwerk::summarize_event_lab(w, t, nullptr, runs);
    CHECK(summary.open == 0);

    // Strategie krone: immer das Reich, gegen den Korridor 100 %.
    std::vector<uhrwerk::EventLabRun> crown;
    for (std::uint64_t seed = 1648; seed < 1652; ++seed) {
        crown.push_back(uhrwerk::run_event_lab(w, uhrwerk::SimConfig{seed, 1}, t, &t.strategies[0]));
    }
    CHECK(uhrwerk::summarize_event_lab(w, t, &t.strategies[0], crown).violations.empty());
    // Dieselben Läufe gegen die Korridore ohne Strategie: das Reich in allen Läufen ist zu viel und
    // festgelegt.
    const uhrwerk::EventLabSummary wrong = uhrwerk::summarize_event_lab(w, t, nullptr, crown);
    CHECK(std::any_of(wrong.violations.begin(), wrong.violations.end(), [](const uhrwerk::EventViolation& v) {
        return v.check == uhrwerk::EventCheck::Determined;
    }));
    CHECK(std::any_of(wrong.violations.begin(), wrong.violations.end(), [](const uhrwerk::EventViolation& v) {
        return v.check == uhrwerk::EventCheck::Outcome;
    }));

    SUBCASE("Fehler der Korridore") {
        for (const auto& [text, message] :
             {std::pair{"ausgang reich { anteil = { 0 1 } }", "Block 'labor' fehlt"},
              std::pair{
                  "labor { bis = 1650.12.31 journal = revolution }\nausgang himmel { anteil = { 0 1 } }",
                  "unbekannter Ausgang 'himmel'"},
              std::pair{
                  "labor { bis = 1650.12.31 }\nereignis maerz.forderungen { option = nie anteil = { 0 1 } }",
                  "unbekannte Option 'nie'"},
              std::pair{"labor { bis = 1650.12.31 }\nstrategie s { spieler = XYZ }", "bestehenden Staat"},
              std::pair{"labor { bis = 1650.12.31 }\nereignis krone.angebot { anteil = { 0 2 } }",
                        "innerhalb von 0 bis 1"}}) {
            CAPTURE(text);
            const uhrwerk::EventTargetsResult r = uhrwerk::parse_event_targets(text, w);
            CHECK_FALSE(r.ok());
            CHECK(std::any_of(r.errors.begin(), r.errors.end(),
                              [&](const std::string& e) { return e.find(message) != std::string::npos; }));
        }
    }
}

TEST_CASE("Ereignisse: Journal beginnt nicht nach seinem Ende") {
    // Das Testjournal endet am 1. Juni 1649; das Szenario jahresbeginn_1650 beginnt danach (wie die
    // Lesezeichen 1864–1870 nach dem Ende der Revolution 1848/49).
    const RuleData data("", kJournalEvents);
    testhilfen::write_file(data.dir() / "common/journale/test.chron", kJournal);
    uhrwerk::LoadResult result = uhrwerk::load_world(data.dir().path(), "jahresbeginn_1650");
    INFO(testhilfen::issues_text(result));
    REQUIRE(result.ok());
    Simulation sim(std::move(*result.world));
    sim.step_days(40);
    CHECK(sim.world().journal_states[0].stage == kNone);
    CHECK(sim.world().journal_history.empty());
}

// ─── Schritt b3: Ausgang eines Krieges, Kriegsziele und Abrüstung im Lader ──────────────────────────────

TEST_CASE("Ereignisse: krieg_gewonnen, krieg_verloren und krieg { … } (b3)") {
    const RuleData data("", R"(
ereignis s.sieg { titel = "Sieg" text = "x" art = dynamisch
    bedingung { staat = BRA  krieg_gewonnen = SAX }
    option a { text = "a" gewicht = 1 wirkung { merke = sieg } } }
ereignis t.niederlage { titel = "Niederlage" text = "x" art = dynamisch
    bedingung { staat = SAX  krieg_verloren = BRA }
    option a { text = "a" gewicht = 1 wirkung { merke = niederlage } } }
ereignis u.falsch { titel = "Falsch" text = "x" art = dynamisch
    bedingung { staat = SAX  krieg_gewonnen = BRA }
    option a { text = "a" gewicht = 1 wirkung { merke = falsch } } }
ereignis v.krieg { titel = "Krieg" text = "x" art = dynamisch nur_folge = ja
    option a { text = "a" gewicht = 1 wirkung {
        krieg { gegner = SAX  fuehrung = ja  zollunion = nein  entschaedigung = 3 }
        mobilmachung = nein } } }
)");
    World start = data.load();
    const Index bra = state_of(start, "BRA");
    const Index sax = state_of(start, "SAX");
    // Lader: Kriegsziele als Codes wie im Befehl, mobilmachung = nein als a = 0.
    const std::vector<uhrwerk::Effect>& effects = event(start, "v.krieg").options[0].effects;
    REQUIRE(effects.size() == 2);
    CHECK(effects[0].kind == EffectKind::War);
    CHECK(effects[0].a == sax);
    CHECK(effects[0].goals ==
          std::vector<std::int64_t>{uhrwerk::encode_war_goal(uhrwerk::WarGoalKind::Leadership, 0),
                                    uhrwerk::encode_war_goal(uhrwerk::WarGoalKind::Indemnity, 3)});
    CHECK(effects[1].kind == EffectKind::Mobilize);
    CHECK(effects[1].a == 0);
    // Brandenburg hat als Verteidiger gegen Sachsen gewonnen (winner −1).
    uhrwerk::WarResult result;
    result.id = 1;
    result.attacker = sax;
    result.defender = bra;
    result.winner = -1;
    start.war_results.push_back(result);
    Simulation sim(std::move(start));
    sim.step_day();
    const World& w = sim.world();
    const auto flagged = [&](Index s, std::string_view name) {
        return std::any_of(w.event_flags.begin(), w.event_flags.end(),
                           [&](const uhrwerk::EventFlag& f) { return f.state == s && f.name == name; });
    };
    CHECK(flagged(bra, "sieg"));
    CHECK(flagged(sax, "niederlage"));
    CHECK_FALSE(flagged(sax, "falsch"));

    SUBCASE("Ladefehler") {
        for (const auto& [effect, message] :
             {std::pair{"krieg { fuehrung = ja }", "Pflichtfeld 'gegner' fehlt in krieg"},
              std::pair{"krieg { gegner = SAX gebiet = nirgendwo }",
                        "unbekannte Verwaltungseinheit 'nirgendwo'"},
              std::pair{"krieg { gegner = SAX sieg = ja }", "in 'krieg' sind nur gegner, gebiet"},
              std::pair{"krieg { gegner = SAX entschaedigung = 30 }", "entschaedigung"},
              std::pair{"mobilmachung = vielleicht", "'mobilmachung' erwartet ja oder nein"}}) {
            CAPTURE(message);
            const RuleData bad("", std::string("ereignis x { titel = \"x\" text = \"x\" art = dynamisch\n"
                                               "    option a { text = \"a\" gewicht = 1 wirkung { ") +
                                       effect + " } } }\n");
            const uhrwerk::LoadResult r = bad.result();
            INFO(testhilfen::issues_text(r));
            CHECK_FALSE(r.ok());
            CHECK(testhilfen::has_issue(r, message));
        }
    }
}

// ─── Schritt b4: Folgentexte und Ziele der Journale für die Oberfläche ─────────────────────────────────

TEST_CASE("Ereignisse: Folgen als Text und Stand der Journalziele (b4)") {
    const EventData data;
    const World w = data.load();
    const std::vector<std::string> lines =
        uhrwerk::describe_effects(w, event(w, "test.unruhen").options[0].effects);
    std::string all;
    for (const std::string& line : lines) {
        all += line + "\n";
    }
    INFO(all);
    // Das Vereinsgesetz gilt im Fixture erst ab 1700 (späterer Inhalt), seine Wirkung entfällt beim Laden.
    REQUIRE(lines.size() == 6);
    CHECK(lines[0] == "Presse: Pressefreiheit");
    CHECK(lines[1].find("Zustimmung ") == 0);
    CHECK(lines[1].find(" +3") != std::string::npos);
    CHECK(lines[2] == "Unruhe \u221220");
    CHECK(lines[3] == "Folgt: „Welt“");
    CHECK(lines[4] == "mit 30 %: Kasse \u22121.000");
    CHECK(lines[5] == "mit 70 %: Beziehung zu Sachsen +10");
}
