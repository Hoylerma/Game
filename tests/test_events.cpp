// Tests der Ereignisse (seit M11; docs/technik/ereignisse.md): Schritt a1 Datenmodell – Gesetzesgruppen,
// Wünsche der Interessengruppen, Lader der Ereignisse mit Bedingungen und Wirkungen (Abschnitt 3), Zustand,
// Inhaltsbereich ereignisse und Spielstand Format 5 (Abschnitt 8).

#include "test_hilfen.hpp"
#include "uhrwerk/savegame.hpp"
#include "uhrwerk/simulation.hpp"

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
