// Tests der Ereignisse (seit M11; docs/technik/ereignisse.md): Schritt a1 Datenmodell – Gesetzesgruppen,
// Wünsche der Interessengruppen, Lader der Ereignisse mit Bedingungen und Wirkungen (Abschnitt 3), Zustand,
// Inhaltsbereich ereignisse und Spielstand Format 5 (Abschnitt 8).

#include "test_hilfen.hpp"
#include "uhrwerk/events.hpp"
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

constexpr std::string_view kRules = "ereignisse {\n    seit = 1648.01.01\n    frist = 20\n}\n";

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
        {"unbekannte Wirkung", "unruhe = -20", "aufstand = barrikaden", "unbekannte Wirkung 'aufstand'"},
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
