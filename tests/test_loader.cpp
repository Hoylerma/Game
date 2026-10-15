// Tests für den Lader (data/-Format, Validierung, Startzustand).

#include "test_hilfen.hpp"
#include "uhrwerk/loader.hpp"
#include "uhrwerk/population.hpp"

#include <cstdint>
#include <cstdlib>
#include <doctest/doctest.h>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using testhilfen::has_issue;
using testhilfen::issues_text;
using testhilfen::replace_in_file;
using testhilfen::TempDir;
using uhrwerk::Date;
using uhrwerk::Fixed;
using uhrwerk::Index;
using uhrwerk::LoadResult;
using uhrwerk::TaxKind;
using uhrwerk::World;

namespace {

const char* const kStatesBra = "history/staaten/10_brandenburg.chron";
const char* const kStatesMid = "history/staaten/20_mitteldeutschland.chron";
const char* const kPlacesBra = "history/orte/10_brandenburg.chron";
const char* const kPlacesSax = "history/orte/20_sachsen.chron";

Fixed fx(const char* text) {
    const auto value = Fixed::parse(text);
    REQUIRE(value.has_value());
    return *value;
}

LoadResult load(const TempDir& dir, std::string_view scenario = uhrwerk::kDefaultScenario) {
    return uhrwerk::load_world(dir.path(), scenario);
}

/// Erwartet einen fatalen Fehler mit diesem Text (und optional Datei/Zeile).
void expect_error(const LoadResult& result, std::string_view needle, const char* file = nullptr,
                  int line = -1) {
    INFO(issues_text(result));
    CHECK_FALSE(result.ok());
    const uhrwerk::LoadIssue* issue = testhilfen::find_issue(result, needle);
    REQUIRE_MESSAGE(issue != nullptr, "erwartete Meldung fehlt: " << needle);
    CHECK(issue->fatal);
    if (file != nullptr) {
        CHECK(issue->file == file);
    }
    if (line >= 0) {
        CHECK(issue->line == line);
    }
}

struct PlaceSpec {
    const char* id;
    std::int64_t population;
    bool city;
    std::vector<std::pair<const char*, std::int64_t>> confessions;
};

} // namespace

// ─── Fixture ────────────────────────────────────────────────────────────────

TEST_CASE("Lader: Fixture lädt ohne Fehler und Warnungen") {
    const LoadResult result = uhrwerk::load_world(testhilfen::fixture_dir());
    INFO(issues_text(result));
    REQUIRE(result.ok());
    CHECK(result.issues.empty());
    const World& w = *result.world;

    CHECK(w.terrains.size() == 3);
    CHECK(w.confessions.size() == 3);
    CHECK(w.cultures.size() == 3);
    CHECK(w.professions.size() == 4);
    CHECK(w.states.size() == 3);
    CHECK(w.districts.size() == 3);
    CHECK(w.locations.size() == 6); // davon ein Ort ohne Daten (zerbst)
    CHECK(w.locations_with_data() == 5);
    CHECK(w.pops.size() == 29);
    CHECK(w.chronicle.empty());

    // Indizes folgen der Ladereihenfolge: Dateien nach Namen, Einträge in Dateireihenfolge.
    CHECK(w.states[0].tag == "BRA");
    CHECK(w.states[1].tag == "SAX");
    CHECK(w.states[2].tag == "ANH");
    const char* const districts[] = {"kr_berlin", "kr_dresden", "kr_anhalt"};
    for (std::size_t i = 0; i < 3; ++i) {
        CHECK(w.districts[i].id == districts[i]);
    }
    const char* const places[] = {"berlin_coelln", "potsdam", "dresden", "leipzig", "dessau", "zerbst"};
    for (std::size_t i = 0; i < 6; ++i) {
        CHECK(w.locations[i].id == places[i]);
    }
    CHECK(w.terrains[2].id == "sandboden");
    CHECK(w.confessions[1].id == "reformiert");
    CHECK(w.professions[3].id == "adel");

    // Szenario und Datum
    CHECK(w.scenario.id == "westfaelischer_frieden");
    CHECK(w.scenario.name == "Der Westfälische Frieden");
    CHECK(w.scenario.start == Date{1648, 10, 24});
    CHECK(w.scenario.player_tag == "BRA");
    CHECK(w.date == Date{1648, 10, 24});
}

TEST_CASE("Lader: Werte aus common/") {
    const World w = testhilfen::load_fixture();
    const uhrwerk::Rules& r = w.rules;
    CHECK(r.devastation_decay == fx("0.03"));
    CHECK(r.devastation_yield_loss == fx("0.5"));
    CHECK(r.plague_chance == fx("0.01"));
    CHECK(r.plague_loss_min == fx("0.04"));
    CHECK(r.plague_loss_max == fx("0.12"));
    CHECK(r.tax_rate_max == fx("0.5"));

    const uhrwerk::TerrainType& aue = w.terrains[0];
    CHECK(aue.id == "flussaue");
    CHECK(aue.name == "Flussaue");
    CHECK(aue.color == uhrwerk::Color{128, 160, 112});
    CHECK(aue.yield_factor.raw() == 75366);

    CHECK(w.confessions[0].name == "Lutherisch");
    CHECK(w.confessions[0].color == uhrwerk::Color{70, 110, 170});

    const uhrwerk::Profession& bauern = w.professions[0];
    CHECK(bauern.name == "Bauern");
    CHECK(bauern.stratum == uhrwerk::Stratum::Unten);
    CHECK(bauern.agrarian);
    CHECK(bauern.taxable);
    CHECK(bauern.share_city == fx("0.10"));
    CHECK(bauern.share_rural == fx("0.80"));
    CHECK(w.professions[1].stratum == uhrwerk::Stratum::Mittel);
    CHECK(w.professions[2].taxable); // "yes"
    CHECK(w.professions[2].share_rural == Fixed::zero());
    CHECK(w.professions[3].stratum == uhrwerk::Stratum::Oben);
    CHECK_FALSE(w.professions[3].taxable);

    // Bevölkerung (seit M4)
    CHECK(r.share_children == fx("0.34"));
    CHECK(r.share_young == fx("0.26"));
    CHECK(r.share_working == fx("0.33"));
    CHECK(r.fertility == fx("0.088"));
    CHECK(r.mortality_old == fx("0.11"));
    CHECK(r.capacity_city == fx("1.75"));
    CHECK(r.living_standard_reference == Fixed::from_int(12));
    CHECK(r.migration_targets == 4);
    CHECK(r.migration_radius_km == Fixed::from_int(60));
    CHECK(r.block_min_size == Fixed::one());
    CHECK(r.migration_min_amount == fx("0.05"));
    CHECK(aue.capacity == Fixed::from_int(45));
    CHECK(w.confessions[0].literacy_factor == fx("1.2"));
    CHECK(w.cultures[0].id == "maerker");
    CHECK(w.cultures[0].name == "Märker");
    CHECK(w.cultures[2].color == uhrwerk::Color{150, 100, 180});
    CHECK(bauern.agrarian);
    CHECK(bauern.literacy == fx("0.1"));
    CHECK(bauern.city_profession == w.find_profession("handwerker"));
    CHECK_FALSE(w.professions[1].agrarian);
    CHECK(w.professions[1].city_profession == uhrwerk::kNone);
}

TEST_CASE("Lader: Staaten und Orte") {
    const World w = testhilfen::load_fixture();
    const uhrwerk::State& bra = w.states[0];
    CHECK(bra.name == "Kurfürstentum Brandenburg");
    CHECK(bra.short_name == "Brandenburg");
    CHECK(bra.color == uhrwerk::Color{58, 95, 143});
    CHECK(bra.capital == w.find_location("berlin_coelln"));
    CHECK(bra.government == "absolute_monarchie");
    CHECK(bra.ruler == "Friedrich Wilhelm");
    CHECK(bra.confession == w.find_confession("reformiert"));
    CHECK(bra.treasury == Fixed::from_int(20000));
    CHECK(bra.tax_rate(TaxKind::Kontribution) == fx("0.10"));
    CHECK(bra.court_expenses == Fixed::from_int(1500));
    CHECK(bra.last_month_income == Fixed::zero());
    CHECK(bra.last_month_expenses == Fixed::zero());
    CHECK(bra.source == "Testdaten, geschätzt");
    CHECK(w.states[1].capital == w.find_location("dresden"));
    CHECK(w.states[2].capital == w.find_location("dessau"));

    const uhrwerk::Location& berlin = w.locations[0];
    CHECK(berlin.name == "Berlin-Cölln");
    CHECK(berlin.owner == 0);
    CHECK(berlin.terrain == w.find_terrain("flussaue"));
    CHECK(berlin.longitude == fx("13.40"));
    CHECK(berlin.latitude == fx("52.52"));
    CHECK(berlin.area_km2 == Fixed::from_int(150));
    CHECK(berlin.is_city);
    CHECK(berlin.devastation == fx("0.4"));
    CHECK(berlin.source == "Testdaten, geschätzt");
    CHECK_FALSE(w.locations[1].is_city);
    CHECK(w.locations[4].owner == w.find_state("ANH"));
}

TEST_CASE("Lader: Startzustand der Bevölkerungsblöcke (Abschnitt 4)") {
    const World w = testhilfen::load_fixture();
    const std::vector<PlaceSpec> specs = {
        {"berlin_coelln", 12000, true, {{"lutherisch", 90}, {"reformiert", 10}}},
        {"potsdam", 1500, false, {{"lutherisch", 100}}},
        {"dresden", 20000, true, {{"lutherisch", 95}, {"katholisch", 5}}},
        {"leipzig", 15000, true, {{"lutherisch", 100}, {"reformiert", 0}}},
        {"dessau", 3000, false, {{"reformiert", 60}, {"lutherisch", 40}}},
    };

    Index next = 0;
    for (std::size_t li = 0; li < specs.size(); ++li) {
        const PlaceSpec& spec = specs[li];
        const uhrwerk::Location& loc = w.locations[li];
        INFO("Ort " << spec.id);
        REQUIRE(loc.id == spec.id);
        CHECK(loc.first_pop == next); // zusammenhängend

        // Erwartete Blöcke: Berufe in Ladereihenfolge, darin Konfessionen in Dateireihenfolge.
        Index expected_index = loc.first_pop;
        for (std::size_t pi = 0; pi < w.professions.size(); ++pi) {
            const uhrwerk::Profession& prof = w.professions[pi];
            const Fixed share = spec.city ? prof.share_city : prof.share_rural;
            if (share == Fixed::zero()) {
                continue;
            }
            for (const auto& [confession, percent] : spec.confessions) {
                if (percent == 0) {
                    continue;
                }
                REQUIRE(expected_index < loc.first_pop + loc.pop_count);
                const uhrwerk::PopBlock& pop = w.pops[static_cast<std::size_t>(expected_index)];
                CHECK(pop.location == static_cast<Index>(li));
                CHECK(pop.profession == static_cast<Index>(pi));
                CHECK(pop.confession == w.find_confession(confession));
                const Fixed size = Fixed::from_int(spec.population) * share * Fixed::from_int(percent) /
                                   Fixed::from_int(100);
                CHECK(pop.size() == size);
                ++expected_index;
            }
        }
        CHECK(expected_index == loc.first_pop + loc.pop_count);
        next = loc.first_pop + loc.pop_count;
    }
    CHECK(next == static_cast<Index>(w.pops.size()));
    // Der Ort ohne Daten am Ende hat keine Blöcke.
    const uhrwerk::Location& zerbst = w.locations[5];
    REQUIRE(zerbst.id == "zerbst");
    CHECK(zerbst.first_pop == next);
    CHECK(zerbst.pop_count == 0);

    // Handgerechnet: 12000 × 0,10 (6554/65536) × 90 / 100, abgerundet bzw. abgeschnitten.
    CHECK(w.pops[0].size().raw() == 70783200);
    // Blockzahlen: Kaufleute fehlen auf dem Land, Reformierte mit 0 % in Leipzig.
    CHECK(w.locations[0].pop_count == 8);
    CHECK(w.locations[1].pop_count == 3);
    CHECK(w.locations[2].pop_count == 8);
    CHECK(w.locations[3].pop_count == 4);
    CHECK(w.locations[4].pop_count == 6);
}

TEST_CASE("Lader: Szenarioauswahl") {
    SUBCASE("zweites Szenario") {
        const World w = testhilfen::load_fixture("jahresbeginn_1650");
        CHECK(w.scenario.id == "jahresbeginn_1650");
        CHECK(w.scenario.name == "Jahresbeginn 1650");
        CHECK(w.scenario.player_tag == "SAX");
        CHECK(w.date == Date{1650, 1, 1});
    }
    SUBCASE("unbekanntes Szenario") {
        const LoadResult r = uhrwerk::load_world(testhilfen::fixture_dir(), "gibt_es_nicht");
        expect_error(r, "Szenario 'gibt_es_nicht' nicht gefunden");
        CHECK(has_issue(r, "jahresbeginn_1650, westfaelischer_frieden"));
    }
}

// ─── Warnungen ──────────────────────────────────────────────────────────────

TEST_CASE("Lader: unbekannte Felder und Einträge erzeugen Warnungen") {
    TempDir dir(true);
    replace_in_file(dir / kPlacesBra, "    stadt        = ja\n",
                    "    stadt        = ja\n    festung      = nein\n");
    replace_in_file(dir / "common/gelaende.chron", "# Testdaten: Geländetypen\n",
                    "# Testdaten: Geländetypen\nfluss elbe { name = \"Elbe\" }\nlose_angabe\n");
    replace_in_file(dir / kStatesBra, "    quelle", "    { anonym }\n    quelle");

    const LoadResult r = load(dir);
    INFO(issues_text(r));
    REQUIRE(r.ok());
    REQUIRE(r.issues.size() == 4);
    for (const uhrwerk::LoadIssue& issue : r.issues) {
        CHECK_FALSE(issue.fatal);
    }
    const uhrwerk::LoadIssue* festung = testhilfen::find_issue(r, "unbekanntes Feld 'festung'");
    REQUIRE(festung != nullptr);
    CHECK(festung->file == kPlacesBra);
    CHECK(festung->line == 10);
    CHECK(festung->to_string() ==
          "history/orte/10_brandenburg.chron:10: Warnung: unbekanntes Feld 'festung' in ort "
          "berlin_coelln wird ignoriert");
    CHECK(has_issue(r, "unbekannter Eintrag 'fluss'", false));
    CHECK(has_issue(r, "unerwarteter Eintrag 'lose_angabe'", false));
    CHECK(has_issue(r, "anonymer Block in staat BRA", false));
}

// ─── Fatale Fehler ──────────────────────────────────────────────────────────

TEST_CASE("Lader: fehlende Pflichtfelder") {
    TempDir dir(true);
    SUBCASE("quelle im Staat") {
        replace_in_file(dir / kStatesBra, "    quelle         = \"Testdaten, geschätzt\"\n", "");
        expect_error(load(dir), "Pflichtfeld 'quelle' fehlt in staat BRA", kStatesBra, 2);
    }
    SUBCASE("lage im Ort") {
        replace_in_file(dir / kPlacesBra, "    lage         = { 13.40 52.52 }\n", "");
        expect_error(load(dir), "Pflichtfeld 'lage' fehlt in ort berlin_coelln", kPlacesBra, 2);
    }
    SUBCASE("Regelwert") {
        replace_in_file(dir / "common/regeln.chron",
                        "    seuche_chance         = 0.01    # jährlich je Ort\n", "");
        expect_error(load(dir), "Pflichtfeld 'seuche_chance' fehlt in regeln", "common/regeln.chron");
    }
    SUBCASE("Szenariodatum") {
        replace_in_file(dir / "szenarien/westfaelischer_frieden.chron", "    datum   = 1648.10.24\n", "");
        expect_error(load(dir), "Pflichtfeld 'datum' fehlt in szenario westfaelischer_frieden");
    }
    SUBCASE("Block regeln fehlt") {
        testhilfen::write_file(dir / "common/regeln.chron", "# leer\n");
        expect_error(load(dir), "Block 'regeln' fehlt", "common/regeln.chron");
    }
}

TEST_CASE("Lader: ungültige Werte") {
    TempDir dir(true);
    SUBCASE("Steuersatz über dem Maximum") {
        replace_in_file(dir / kStatesBra, "kontribution   = 0.10", "kontribution   = 0.6");
        expect_error(load(dir), "'kontribution' muss zwischen 0 und 0.5 liegen (ist 0.6)", kStatesBra, 11);
    }
    SUBCASE("Steuersatz über einem abgesenkten Maximum") {
        replace_in_file(dir / "common/regeln.chron", "steuersatz_max        = 0.5",
                        "steuersatz_max        = 0.1");
        expect_error(load(dir), "'kontribution' muss zwischen 0 und 0.1 liegen (ist 0.12)", kStatesMid);
    }
    SUBCASE("Akzise und Zoll über dem Maximum, Heer negativ") {
        replace_in_file(dir / kStatesBra, "akzise         = 0.05", "akzise         = 0.7");
        expect_error(load(dir), "'akzise' muss zwischen 0 und 0.5 liegen (ist 0.7)", kStatesBra, 12);
        replace_in_file(dir / kStatesBra, "akzise         = 0.7", "akzise         = 0.05");
        replace_in_file(dir / kStatesBra, "zoll           = 0.03", "zoll           = 0.51");
        expect_error(load(dir), "'zoll' muss zwischen 0 und 0.5 liegen (ist 0.51)", kStatesBra, 13);
        replace_in_file(dir / kStatesBra, "zoll           = 0.51", "zoll           = 0.03");
        replace_in_file(dir / kStatesBra, "heer           = 10", "heer           = -1");
        expect_error(load(dir), "'heer' muss zwischen 0 und 1000 liegen (ist -1)", kStatesBra, 15);
    }
    SUBCASE("Steuersatz alter Form") {
        replace_in_file(dir / kStatesBra, "kontribution   = 0.10", "steuersatz     = 0.10");
        const LoadResult r = load(dir);
        CHECK(has_issue(r, "Pflichtfeld 'kontribution' fehlt in staat BRA", true));
        CHECK(has_issue(r, "unbekanntes Feld 'steuersatz' in staat BRA wird ignoriert", false));
    }
    SUBCASE("Bevölkerung 0") {
        replace_in_file(dir / kPlacesBra, "bevoelkerung = 12000", "bevoelkerung = 0");
        expect_error(load(dir), "'bevoelkerung' muss zwischen 1 und 100000000 liegen (ist 0)", kPlacesBra, 8);
    }
    SUBCASE("Bevölkerung keine Ganzzahl") {
        replace_in_file(dir / kPlacesBra, "bevoelkerung = 12000", "bevoelkerung = 12k");
        expect_error(load(dir), "ungültiger Wert '12k' für 'bevoelkerung'");
    }
    SUBCASE("Wahrheitswert") {
        replace_in_file(dir / kPlacesBra, "stadt        = ja", "stadt        = vielleicht");
        expect_error(load(dir), "ungültiger Wert 'vielleicht' für 'stadt' (erwartet ja oder nein)");
    }
    SUBCASE("Farbe außerhalb 0–255") {
        replace_in_file(dir / kStatesBra, "{ 58 95 143 }", "{ 58 95 300 }");
        expect_error(load(dir), "Farbwerte in 'farbe' müssen zwischen 0 und 255 liegen", kStatesBra, 5);
    }
    SUBCASE("Farbe mit zwei Werten") {
        replace_in_file(dir / kStatesBra, "{ 58 95 143 }", "{ 58 95 }");
        expect_error(load(dir), "'farbe' erwartet drei Zahlen von 0 bis 255");
    }
    SUBCASE("Farbe ohne Block") {
        replace_in_file(dir / kStatesBra, "{ 58 95 143 }", "blau");
        expect_error(load(dir), "'farbe' erwartet einen Block");
    }
    SUBCASE("Datum") {
        replace_in_file(dir / "szenarien/westfaelischer_frieden.chron", "1648.10.24", "1648.02.30");
        expect_error(load(dir), "ungültiges Datum '1648.02.30' für 'datum'");
    }
    SUBCASE("Datum ohne führende Nullen") {
        // Wie der Archivar: nur JJJJ.MM.TT (Spezifikation 1.5)
        replace_in_file(dir / "szenarien/westfaelischer_frieden.chron", "1648.10.24", "1648.10.4");
        expect_error(load(dir), "ungültiges Datum '1648.10.4' für 'datum'");
    }
    SUBCASE("Lage mit einer Zahl") {
        replace_in_file(dir / kPlacesBra, "{ 13.40 52.52 }", "{ 13.40 }");
        expect_error(load(dir), "'lage' erwartet genau zwei Zahlen", kPlacesBra, 6);
    }
    SUBCASE("Lage außerhalb der Erde") {
        replace_in_file(dir / kPlacesBra, "{ 13.40 52.52 }", "{ 13.40 95 }");
        expect_error(load(dir), "'lage' außerhalb des gültigen Bereichs");
    }
    SUBCASE("Schicht") {
        replace_in_file(dir / "common/berufe.chron", "schicht         = unten",
                        "schicht         = ganz_unten");
        expect_error(load(dir), "ungültige Schicht 'ganz_unten'", "common/berufe.chron", 4);
    }
    SUBCASE("Verwüstung über 1") {
        replace_in_file(dir / kPlacesBra, "verwuestung  = 0.4", "verwuestung  = 1.5");
        expect_error(load(dir), "'verwuestung' muss zwischen 0 und 1 liegen (ist 1.5)");
    }
    SUBCASE("Fläche 0") {
        replace_in_file(dir / kPlacesBra, "flaeche      = 150", "flaeche      = 0");
        expect_error(load(dir), "'flaeche' muss größer als 0");
    }
    SUBCASE("Zahl mit Komma") {
        replace_in_file(dir / "common/gelaende.chron", "ertrag   = 1.15", "ertrag   = 1,15");
        expect_error(load(dir), "ungültiger Wert '1,15' für 'ertrag'", "common/gelaende.chron", 5);
    }
    SUBCASE("Konfessionsanteile ergeben nicht 100") {
        replace_in_file(dir / kPlacesBra, "{ lutherisch = 90 reformiert = 10 }",
                        "{ lutherisch = 90 reformiert = 20 }");
        expect_error(load(dir), "die Konfessionsanteile in ort berlin_coelln ergeben 110 %, erwartet 100 %");
    }
    SUBCASE("Konfessionsanteil ungültig") {
        replace_in_file(dir / kPlacesBra, "{ lutherisch = 90 reformiert = 10 }",
                        "{ lutherisch = 90 reformiert = 10.5 }");
        expect_error(load(dir), "ungültiger Prozentwert '10.5'");
    }
    SUBCASE("Konfession doppelt") {
        replace_in_file(dir / kPlacesBra, "{ lutherisch = 90 reformiert = 10 }",
                        "{ lutherisch = 90 lutherisch = 10 }");
        expect_error(load(dir), "Konfession 'lutherisch' ist in ort berlin_coelln mehrfach angegeben");
    }
    SUBCASE("leerer Name") {
        replace_in_file(dir / kStatesBra, "\"Kurfürstentum Brandenburg\"", "\"\"");
        expect_error(load(dir), "Feld 'name' darf nicht leer sein", kStatesBra, 3);
    }
    SUBCASE("falscher Operator") {
        replace_in_file(dir / kStatesBra, "schatz         = 20000", "schatz         >= 20000");
        const LoadResult r = load(dir);
        expect_error(r, "Feld 'schatz' erwartet '=' statt '>='", kStatesBra, 10);
        CHECK_FALSE(has_issue(r, "Pflichtfeld 'schatz' fehlt")); // keine Folgemeldung
    }
    SUBCASE("Block statt Wert") {
        replace_in_file(dir / kStatesBra, "\"Kurfürstentum Brandenburg\"", "{ Kurfürstentum Brandenburg }");
        expect_error(load(dir), "'name' erwartet einen einzelnen Wert, keinen Block");
    }
    SUBCASE("Feld doppelt") {
        replace_in_file(dir / kStatesBra, "    schatz         = 20000\n",
                        "    schatz         = 20000\n    schatz         = 5\n");
        expect_error(load(dir), "Feld 'schatz' in staat BRA ist mehrfach angegeben (zuerst in Zeile 10)",
                     kStatesBra, 11);
    }
    SUBCASE("Seuchenverlust Minimum größer als Maximum") {
        replace_in_file(dir / "common/regeln.chron", "seuche_verlust_min    = 0.04",
                        "seuche_verlust_min    = 0.2");
        expect_error(load(dir), "'seuche_verlust_min' (0.2) ist größer als 'seuche_verlust_max' (0.12)");
    }
    SUBCASE("Definition ohne Kennung") {
        replace_in_file(dir / "common/konfessionen.chron", "konfession katholisch {", "konfession {");
        expect_error(load(dir), "ungültige Definition, erwartet: konfession <kennung> { … }",
                     "common/konfessionen.chron", 14);
    }
}

TEST_CASE("Lader: Summe der Berufsanteile") {
    TempDir dir(true);
    SUBCASE("deutlich daneben") {
        replace_in_file(dir / "common/berufe.chron", "anteil_stadt    = 0.10", "anteil_stadt    = 0.20");
        expect_error(load(dir), "die Summe aller 'anteil_stadt' ist 1.1", "common/berufe.chron");
    }
    SUBCASE("knapp innerhalb der Toleranz") {
        replace_in_file(dir / "common/berufe.chron", "anteil_stadt    = 0.10", "anteil_stadt    = 0.1005");
        const LoadResult r = load(dir);
        INFO(issues_text(r));
        CHECK(r.ok());
    }
    SUBCASE("knapp außerhalb der Toleranz") {
        replace_in_file(dir / "common/berufe.chron", "anteil_land     = 0.80", "anteil_land     = 0.7985");
        expect_error(load(dir), "die Summe aller 'anteil_land'");
    }
}

TEST_CASE("Lader: doppelte Kennungen") {
    TempDir dir(true);
    SUBCASE("Ort") {
        testhilfen::add_market(dir.path(), "40_doppelt");
        testhilfen::write_file(dir / "history/orte/40_doppelt.chron",
                               "ort berlin_coelln {\n name = \"B\" besitzer = BRA gelaende = flussaue\n"
                               " lage = { 13 52 } flaeche = 1 bevoelkerung = 1 stadt = ja\n"
                               " konfession = { lutherisch = 100 } verwuestung = 0 quelle = \"x\"\n}\n");
        expect_error(load(dir),
                     "doppelte Kennung 'berlin_coelln' für Ort (bereits definiert in "
                     "history/orte/10_brandenburg.chron:2)",
                     "history/orte/40_doppelt.chron", 1);
    }
    SUBCASE("Staat") {
        replace_in_file(dir / kStatesMid, "staat ANH {", "staat SAX {");
        expect_error(load(dir), "doppelte Kennung 'SAX' für Staat", kStatesMid, 19);
    }
    SUBCASE("Gelände") {
        replace_in_file(dir / "common/gelaende.chron", "gelaende sandboden {", "gelaende ackerland {");
        expect_error(load(dir), "doppelte Kennung 'ackerland' für Gelände");
    }
    SUBCASE("Szenario") {
        replace_in_file(dir / "szenarien/jahresbeginn.chron", "jahresbeginn_1650", "westfaelischer_frieden");
        expect_error(load(dir), "doppelte Kennung 'westfaelischer_frieden' für Szenario");
    }
}

TEST_CASE("Lader: unbekannte Verweise") {
    TempDir dir(true);
    SUBCASE("Besitzer") {
        replace_in_file(dir / kPlacesBra, "    besitzer     = BRA\n    gelaende     = sandboden",
                        "    besitzer     = XYZ\n    gelaende     = sandboden");
        expect_error(load(dir), "unbekannter Staat 'XYZ' als Besitzer von ort potsdam", kPlacesBra, 19);
    }
    SUBCASE("Gelände") {
        replace_in_file(dir / kPlacesBra, "gelaende     = sandboden", "gelaende     = sumpf");
        expect_error(load(dir), "unbekanntes Gelände 'sumpf' in ort potsdam");
    }
    SUBCASE("Konfession des Herrscherhauses") {
        replace_in_file(dir / kStatesBra, "konfession     = reformiert", "konfession     = calvinistisch");
        expect_error(load(dir), "unbekannte Konfession 'calvinistisch' in staat BRA", kStatesBra, 9);
    }
    SUBCASE("Konfession im Ort") {
        replace_in_file(dir / kPlacesSax, "katholisch = 5", "orthodox = 5");
        expect_error(load(dir), "unbekannte Konfession 'orthodox' in ort dresden");
    }
    SUBCASE("Hauptstadt eines anderen Staates") {
        replace_in_file(dir / kStatesBra, "hauptstadt     = berlin_coelln", "hauptstadt     = dresden");
        expect_error(load(dir), "Hauptstadt 'dresden' von BRA gehört nicht zu diesem Staat", kStatesBra, 6);
    }
    SUBCASE("unbekannte Hauptstadt") {
        replace_in_file(dir / kStatesBra, "hauptstadt     = berlin_coelln", "hauptstadt     = atlantis");
        expect_error(load(dir), "unbekannter Ort 'atlantis' als Hauptstadt von BRA");
    }
    SUBCASE("Spieler") {
        replace_in_file(dir / "szenarien/westfaelischer_frieden.chron", "spieler = BRA", "spieler = XYZ");
        expect_error(load(dir), "unbekannter Staat 'XYZ' als Spieler");
    }
}

// ─── Datierte Historie (Abschnitt 3.8) ──────────────────────────────────────

TEST_CASE("Lader: datierte Blöcke gelten ab ihrem Datum") {
    TempDir dir(true);
    replace_in_file(dir / kPlacesBra, "    besitzer     = BRA\n    gelaende     = sandboden",
                    "    besitzer     = BRA\n"
                    "    1649.06.01 = { besitzer = SAX bevoelkerung = 2000 }\n"
                    "    gelaende     = sandboden");

    SUBCASE("vor dem Datum gilt die Grundangabe") {
        const LoadResult r = load(dir);
        INFO(issues_text(r));
        REQUIRE(r.ok());
        const World& w = *r.world;
        const Index potsdam = w.find_location("potsdam");
        CHECK(w.locations[static_cast<std::size_t>(potsdam)].owner == w.find_state("BRA"));
        CHECK(w.location_population(potsdam).round() == 1500);
    }
    SUBCASE("ab dem Datum gilt der Block") {
        const LoadResult r = load(dir, "jahresbeginn_1650");
        INFO(issues_text(r));
        REQUIRE(r.ok());
        const World& w = *r.world;
        const Index potsdam = w.find_location("potsdam");
        CHECK(w.locations[static_cast<std::size_t>(potsdam)].owner == w.find_state("SAX"));
        CHECK(w.location_population(potsdam).round() == 2000);
        CHECK(r.issues.empty());
    }
}

TEST_CASE("Lader: datierte Blöcke in zeitlicher Reihenfolge, nicht in Dateireihenfolge") {
    TempDir dir(true);
    replace_in_file(dir / kStatesBra, "    herrscher      = \"Friedrich Wilhelm\"\n",
                    "    1649.12.31 = { herrscher = \"Später\" }\n"
                    "    1648.01.01 = { herrscher = \"Früher\" }\n"
                    "    1700.01.01 = { herrscher = \"Zukunft\" }\n");
    const LoadResult r1648 = load(dir);
    REQUIRE(r1648.ok());
    CHECK(r1648.world->states[0].ruler == "Früher");
    const LoadResult r1650 = load(dir, "jahresbeginn_1650");
    REQUIRE(r1650.ok());
    CHECK(r1650.world->states[0].ruler == "Später");
}

TEST_CASE("Lader: Pflichtfeld nur in einem datierten Block") {
    TempDir dir(true);
    replace_in_file(dir / kPlacesBra, "    besitzer     = BRA\n    gelaende     = sandboden",
                    "    1649.06.01 = { besitzer = BRA }\n    gelaende     = sandboden");
    SUBCASE("vor dem Datum fehlt es") {
        // Die übrigen Simulationsfelder sind da: Fehler „alle fünf oder keines“ (Abschnitt 3.6).
        const LoadResult r = load(dir);
        expect_error(
            r,
            "Ort potsdam hat zum Startdatum 'bevoelkerung', 'konfession', 'kultur' und 'verwuestung', "
            "aber nicht 'besitzer'",
            kPlacesBra, 17);
        CHECK_FALSE(has_issue(r, "Pflichtfeld 'besitzer' fehlt")); // keine doppelte Meldung
    }
    SUBCASE("ab dem Datum ist es vorhanden") {
        const LoadResult r = load(dir, "jahresbeginn_1650");
        INFO(issues_text(r));
        CHECK(r.ok());
    }
}

TEST_CASE("Lader: Fehler in datierten Blöcken") {
    TempDir dir(true);
    const auto insert = [&](std::string_view text) {
        replace_in_file(dir / kPlacesBra, "    besitzer     = BRA\n    gelaende     = sandboden",
                        "    besitzer     = BRA\n" + std::string(text) + "    gelaende     = sandboden");
    };
    SUBCASE("ungültiges Datum") {
        insert("    1648.13.01 = { besitzer = SAX }\n");
        expect_error(load(dir), "ungültiges Datum '1648.13.01' in ort potsdam", kPlacesBra, 20);
    }
    SUBCASE("Wert statt Block") {
        insert("    1649.01.01 = SAX\n");
        expect_error(load(dir), "datierter Eintrag '1649.01.01' in ort potsdam erwartet einen Block");
    }
    SUBCASE("Datum doppelt") {
        insert("    1649.01.01 = { besitzer = SAX }\n    1649.01.01 = { besitzer = BRA }\n");
        expect_error(load(dir),
                     "Datum '1649.01.01' ist in ort potsdam mehrfach angegeben (zuerst in Zeile 20)",
                     kPlacesBra, 21);
    }
    SUBCASE("Feld, das nicht datiert werden kann") {
        insert("    1649.01.01 = { lage = { 13.0 52.0 } }\n");
        expect_error(load(dir), "Feld 'lage' in ort potsdam kann nicht datiert werden");
    }
    SUBCASE("Feld doppelt im Block") {
        insert("    1649.01.01 = { besitzer = SAX besitzer = BRA }\n");
        expect_error(load(dir),
                     "Feld 'besitzer' ist im Block '1649.01.01' von ort potsdam mehrfach angegeben");
    }
    SUBCASE("Verweise in künftigen Blöcken prüft erst das passende Szenario") {
        insert("    1700.01.01 = { besitzer = XYZ }\n");
        const LoadResult r = load(dir, "jahresbeginn_1650");
        INFO(issues_text(r));
        CHECK(r.ok());
    }
    SUBCASE("die Form künftiger Blöcke wird immer geprüft") {
        insert("    1700.01.01 = { flaeche = 10 }\n");
        expect_error(load(dir), "Feld 'flaeche' in ort potsdam kann nicht datiert werden");
    }
    SUBCASE("unbekanntes Feld im Block") {
        insert("    1648.01.01 = { einwohner = 5 }\n");
        const LoadResult r = load(dir);
        CHECK(r.ok());
        CHECK(has_issue(r, "unbekanntes Feld 'einwohner' in ort potsdam wird ignoriert", false));
    }
}

TEST_CASE("Lader: Form von Kennungen und Tags") {
    TempDir dir(true);
    SUBCASE("Staatstag mit Kleinbuchstaben") {
        replace_in_file(dir / kStatesMid, "staat ANH {", "staat Anh {");
        expect_error(load(dir), "ungültiger Staatstag 'Anh'");
    }
    SUBCASE("Staatstag zu lang") {
        replace_in_file(dir / kStatesMid, "staat ANH {", "staat ANHAL {");
        expect_error(load(dir), "ungültiger Staatstag 'ANHAL'");
    }
    SUBCASE("Ortskennung mit Großbuchstaben") {
        replace_in_file(dir / kPlacesSax, "ort leipzig {", "ort Leipzig {");
        expect_error(load(dir), "ungültige Ortskennung 'Leipzig'");
    }
}

TEST_CASE("Lader: Syntaxfehler mit Datei, Zeile und Spalte") {
    TempDir dir(true);
    replace_in_file(dir / "common/konfessionen.chron", "\"Lutherisch\"", "\"Lutherisch");
    const LoadResult r = load(dir);
    expect_error(r, "nicht beendete Zeichenkette (Spalte 13)", "common/konfessionen.chron", 3);
    // Der Rest der Datei wird trotzdem gelesen: keine Folgefehler für andere Konfessionen.
    CHECK_FALSE(has_issue(r, "unbekannte Konfession"));
}

TEST_CASE("Lader: fehlende Dateien und Ordner") {
    TempDir dir(true);
    SUBCASE("Datei") {
        std::filesystem::remove(dir / "common/berufe.chron");
        expect_error(load(dir), "Datei fehlt", "common/berufe.chron", 0);
    }
    SUBCASE("Ordner") {
        std::filesystem::remove_all(dir / "history/orte");
        expect_error(load(dir), "Ordner fehlt", "history/orte");
    }
    SUBCASE("leerer Szenarioordner") {
        std::filesystem::remove(dir / "szenarien/westfaelischer_frieden.chron");
        std::filesystem::remove(dir / "szenarien/jahresbeginn.chron");
        const LoadResult r = load(dir);
        expect_error(r, "Ordner enthält keine .chron-Dateien", "szenarien");
        CHECK(has_issue(r, "keine Szenarien vorhanden"));
    }
    SUBCASE("Datenordner") {
        expect_error(uhrwerk::load_world(dir / "gibt_es_nicht"), "Datenordner nicht gefunden");
    }
    SUBCASE("andere Dateien werden ignoriert") {
        testhilfen::write_file(dir / "history/orte/notizen.txt", "kein { gültiges ! Skript");
        std::filesystem::create_directories(dir / "history/orte/unterordner.chron");
        const LoadResult r = load(dir);
        INFO(issues_text(r));
        CHECK(r.ok());
        CHECK(r.issues.empty());
    }
}

TEST_CASE("Lader: Dateien werden nach Namen sortiert geladen") {
    TempDir dir(true);
    testhilfen::add_market(dir.path(), "05_cottbus");
    testhilfen::write_file(dir / "history/orte/05_cottbus.chron",
                           "ort cottbus {\n name = \"Cottbus\" besitzer = BRA gelaende = sandboden\n"
                           " lage = { 14.33 51.76 } flaeche = 160 bevoelkerung = 2000 stadt = ja\n"
                           " konfession = { lutherisch = 100 } kultur = { maerker = 100 } verwuestung = 0.5\n"
                           " quelle = \"Testdaten\"\n}\n");
    const LoadResult r = load(dir);
    INFO(issues_text(r));
    REQUIRE(r.ok());
    const World& w = *r.world;
    REQUIRE(w.locations.size() == 7);
    CHECK(w.locations[0].id == "cottbus");
    CHECK(w.locations[1].id == "berlin_coelln");
    CHECK(w.locations[0].first_pop == 0);
    CHECK(w.locations[1].first_pop == w.locations[0].pop_count);
    CHECK(w.states[0].capital == 1); // Hauptstadtindex folgt der neuen Reihenfolge
}

TEST_CASE("Lader: LoadIssue::to_string") {
    CHECK(uhrwerk::LoadIssue{"a.chron", 12, "Nachricht", true}.to_string() ==
          "a.chron:12: Fehler: Nachricht");
    CHECK(uhrwerk::LoadIssue{"a.chron", 12, "Nachricht", false}.to_string() ==
          "a.chron:12: Warnung: Nachricht");
    CHECK(uhrwerk::LoadIssue{"a.chron", 0, "Nachricht", true}.to_string() == "a.chron: Fehler: Nachricht");
    CHECK(uhrwerk::LoadIssue{"", 0, "Nachricht", true}.to_string() == "Fehler: Nachricht");
}

// ─── Orte ohne Daten (Abschnitt 3.6) ────────────────────────────────────────

namespace {

const char* const kPlacesAnh = "history/orte/30_anhalt.chron";
const char* const kZerbstFields = "    stadt    = ja\n    kreis    = kr_anhalt\n";

/// Fügt dem Ort ohne Daten (zerbst) Zeilen nach stadt/kreis hinzu.
void add_to_zerbst(const TempDir& dir, std::string_view lines) {
    replace_in_file(dir / kPlacesAnh, kZerbstFields, std::string(kZerbstFields) + std::string(lines));
}

} // namespace

TEST_CASE("Lader: Ort ohne Daten") {
    const World w = testhilfen::load_fixture();
    const Index index = w.find_location("zerbst");
    REQUIRE(index == 5);
    const uhrwerk::Location& zerbst = w.locations[static_cast<std::size_t>(index)];
    CHECK(zerbst.name == "Zerbst");
    CHECK_FALSE(zerbst.has_data());
    CHECK(zerbst.owner == uhrwerk::kNone);
    CHECK(zerbst.pop_count == 0);
    CHECK(zerbst.devastation == Fixed::zero());
    CHECK(w.location_population(index) == Fixed::zero());
    CHECK(w.location_majority_confession(index) == uhrwerk::kNone);
    // Kartenfelder sind vollständig.
    CHECK(zerbst.terrain == w.find_terrain("ackerland"));
    CHECK(zerbst.longitude == fx("12.08"));
    CHECK(zerbst.latitude == fx("51.97"));
    CHECK(zerbst.area_km2 == Fixed::from_int(250));
    CHECK(zerbst.is_city);
    CHECK(zerbst.district == w.find_district("kr_anhalt"));
    CHECK(zerbst.source == "Testdaten, ohne Simulationsdaten");
    // Er gehört keinem Staat.
    CHECK(w.state_locations(w.find_state("ANH")) == std::vector<Index>{4});
    for (std::size_t i = 0; i < 5; ++i) {
        CHECK(w.locations[i].has_data());
    }
}

TEST_CASE("Lader: Simulationsfelder alle oder keines") {
    TempDir dir(true);
    SUBCASE("ein Feld fehlt") {
        replace_in_file(dir / kPlacesBra, "    verwuestung  = 0.6\n", "");
        const LoadResult r = load(dir);
        expect_error(r,
                     "Ort potsdam hat zum Startdatum 'besitzer', 'bevoelkerung', 'konfession' und 'kultur', "
                     "aber nicht 'verwuestung'",
                     kPlacesBra, 17);
        CHECK_FALSE(has_issue(r, "Pflichtfeld 'verwuestung' fehlt"));
    }
    SUBCASE("nur ein Feld vorhanden") {
        add_to_zerbst(dir, "    besitzer = ANH\n");
        const LoadResult r = load(dir);
        expect_error(r,
                     "Ort zerbst hat zum Startdatum 'besitzer', aber nicht 'bevoelkerung', 'konfession', "
                     "'kultur' und 'verwuestung'",
                     kPlacesAnh, 19);
        // Nur diese eine Meldung, keine Folgefehler je fehlendem Feld.
        std::size_t errors = 0;
        for (const uhrwerk::LoadIssue& issue : r.issues) {
            errors += issue.fatal ? 1 : 0;
        }
        CHECK(errors == 1);
    }
    SUBCASE("alle fünf Felder machen einen Ort mit Daten") {
        add_to_zerbst(dir,
                      "    besitzer = ANH\n    bevoelkerung = 2000\n    konfession = { lutherisch = 100 }\n"
                      "    kultur = { obersachsen = 100 }\n    verwuestung = 0.25\n");
        const LoadResult r = load(dir);
        INFO(issues_text(r));
        REQUIRE(r.ok());
        CHECK(r.issues.empty());
        const World& w = *r.world;
        const uhrwerk::Location& zerbst = w.locations[5];
        CHECK(zerbst.has_data());
        CHECK(zerbst.owner == w.find_state("ANH"));
        CHECK(zerbst.devastation == fx("0.25"));
        CHECK(zerbst.pop_count == 4); // Stadt: alle vier Berufe × lutherisch
        CHECK(w.location_population(5).round() == 2000);
    }
    SUBCASE("Werte werden auch bei Orten mit Daten geprüft") {
        add_to_zerbst(dir,
                      "    besitzer = XYZ\n    bevoelkerung = 2000\n    konfession = { lutherisch = 100 }\n"
                      "    kultur = { obersachsen = 100 }\n    verwuestung = 0\n");
        expect_error(load(dir), "unbekannter Staat 'XYZ' als Besitzer von ort zerbst", kPlacesAnh, 26);
    }
}

TEST_CASE("Lader: Orte ohne Daten und datierte Blöcke") {
    TempDir dir(true);
    SUBCASE("Daten ab einem späteren Datum") {
        add_to_zerbst(
            dir, "    1649.06.01 = { besitzer = ANH bevoelkerung = 2000 konfession = { lutherisch = 100 } "
                 "kultur = { obersachsen = 100 } verwuestung = 0.1 }\n");
        const LoadResult r1648 = load(dir);
        INFO(issues_text(r1648));
        REQUIRE(r1648.ok());
        CHECK_FALSE(r1648.world->locations[5].has_data()); // vor dem Datum: Ort ohne Daten
        CHECK(r1648.world->locations[5].owner == uhrwerk::kNone);

        const LoadResult r1650 = load(dir, "jahresbeginn_1650");
        INFO(issues_text(r1650));
        REQUIRE(r1650.ok());
        const World& w = *r1650.world;
        CHECK(w.locations[5].has_data());
        CHECK(w.locations[5].owner == w.find_state("ANH"));
        CHECK(w.locations[5].devastation == fx("0.1"));
        CHECK(w.location_population(5).round() == 2000);
    }
    SUBCASE("unvollständiger künftiger Block fällt erst zu seinem Datum auf") {
        add_to_zerbst(dir, "    1649.06.01 = { besitzer = ANH }\n");
        const LoadResult r1648 = load(dir);
        INFO(issues_text(r1648));
        CHECK(r1648.ok());
        expect_error(load(dir, "jahresbeginn_1650"),
                     "Ort zerbst hat zum Startdatum 'besitzer', aber nicht 'bevoelkerung'", kPlacesAnh, 19);
    }
}

TEST_CASE("Lader: Hauptstadt muss ein Ort mit Daten sein") {
    TempDir dir(true);
    replace_in_file(dir / kStatesMid, "hauptstadt     = dessau", "hauptstadt     = zerbst");
    expect_error(load(dir), "Hauptstadt 'zerbst' von ANH ist ein Ort ohne Daten", kStatesMid, 23);
}

// ─── Kreise (Abschnitt 3.9) ─────────────────────────────────────────────────

namespace {

const char* const kDistrictsBra = "history/kreise/10_brandenburg.chron";
const char* const kDistrictsMid = "history/kreise/20_mitteldeutschland.chron";

/// Entfernt alle Zeilen "kreis = …" aus den Ortsdateien.
void remove_district_fields(const TempDir& dir) {
    for (const char* file : {kPlacesBra, kPlacesSax, kPlacesAnh}) {
        const std::string text = testhilfen::read_file(dir / file);
        std::string result;
        std::size_t start = 0;
        while (start < text.size()) {
            std::size_t end = text.find('\n', start);
            end = end == std::string::npos ? text.size() : end + 1;
            const std::string line = text.substr(start, end - start);
            if (line.find_first_not_of(' ') == line.find("kreis ")) {
                start = end;
                continue;
            }
            result += line;
            start = end;
        }
        testhilfen::write_file(dir / file, result);
    }
}

} // namespace

TEST_CASE("Lader: Kreise") {
    const World w = testhilfen::load_fixture();
    REQUIRE(w.districts.size() == 3);
    const uhrwerk::District& berlin = w.districts[0];
    CHECK(berlin.id == "kr_berlin");
    CHECK(berlin.name == "Berlin");
    CHECK(berlin.source == "Testdaten");
    CHECK(w.districts[2].name == "Anhalt");
    CHECK(w.find_district("kr_dresden") == 1);
    CHECK(w.find_district("kr_anhalt") == 2);
    CHECK(w.find_district("kr_leipzig") == uhrwerk::kNone);

    CHECK(w.locations[0].district == 0);
    CHECK(w.locations[1].district == 0);
    CHECK(w.locations[2].district == 1);
    CHECK(w.locations[3].district == uhrwerk::kNone); // Leipzig ohne Kreis
    CHECK(w.locations[4].district == 2);
    CHECK(w.district_locations(0) == std::vector<Index>{0, 1});
    CHECK(w.district_locations(2) == std::vector<Index>{4, 5}); // auch der Ort ohne Daten
}

TEST_CASE("Lader: Fehler und Warnungen bei Kreisen") {
    TempDir dir(true);
    SUBCASE("unbekannter Kreis") {
        replace_in_file(dir / kPlacesBra,
                        "    kreis        = kr_berlin\n    quelle       = \"Testdaten, geschätzt\"\n}\n",
                        "    kreis        = kr_potsdam\n    quelle       = \"Testdaten, geschätzt\"\n}\n");
        expect_error(load(dir), "unbekannter Kreis 'kr_potsdam' in ort berlin_coelln", kPlacesBra, 13);
    }
    SUBCASE("unbekannter Kreis im Ort ohne Daten") {
        replace_in_file(dir / kPlacesAnh, "kreis    = kr_anhalt", "kreis    = kr_zerbst");
        expect_error(load(dir), "unbekannter Kreis 'kr_zerbst' in ort zerbst", kPlacesAnh, 25);
    }
    SUBCASE("kreis kann nicht datiert werden") {
        add_to_zerbst(dir, "    1649.01.01 = { kreis = kr_dresden }\n");
        expect_error(load(dir), "Feld 'kreis' in ort zerbst kann nicht datiert werden", kPlacesAnh, 26);
    }
    SUBCASE("Kreis ohne Orte ist eine Warnung") {
        testhilfen::write_file(
            dir / "history/kreise/30_leer.chron",
            "# leer\n\nkreis kr_leer {\n    name = \"Leer\"\n    quelle = \"Testdaten\"\n}\n");
        const LoadResult r = load(dir);
        INFO(issues_text(r));
        REQUIRE(r.ok());
        REQUIRE(r.issues.size() == 1);
        const uhrwerk::LoadIssue& issue = r.issues[0];
        CHECK_FALSE(issue.fatal);
        CHECK(issue.message == "Kreis 'kr_leer' hat keine Orte");
        CHECK(issue.file == "history/kreise/30_leer.chron");
        CHECK(issue.line == 3);
        CHECK(r.world->districts.size() == 4);
    }
    SUBCASE("doppelte Kennung") {
        replace_in_file(dir / kDistrictsMid, "kreis kr_anhalt {", "kreis kr_berlin {");
        expect_error(load(dir),
                     "doppelte Kennung 'kr_berlin' für Kreis (bereits definiert in " +
                         std::string(kDistrictsBra) + ":2)",
                     kDistrictsMid, 7);
    }
    SUBCASE("ungültige Kennung") {
        replace_in_file(dir / kDistrictsBra, "kreis kr_berlin {", "kreis Kr-Berlin {");
        expect_error(load(dir), "ungültige Kreiskennung 'Kr-Berlin'", kDistrictsBra, 2);
    }
    SUBCASE("Pflichtfeld fehlt") {
        replace_in_file(dir / kDistrictsBra, "    name   = \"Berlin\"\n", "");
        expect_error(load(dir), "Pflichtfeld 'name' fehlt in kreis kr_berlin", kDistrictsBra, 2);
    }
    SUBCASE("Kreise sind nicht datiert") {
        replace_in_file(dir / kDistrictsBra, "    name   = \"Berlin\"\n",
                        "    name   = \"Berlin\"\n    1701.01.18 = { name = \"Berlin (Residenz)\" }\n");
        expect_error(load(dir),
                     "datierter Eintrag '1701.01.18' in kreis kr_berlin: Kreise können nicht datiert werden",
                     kDistrictsBra, 4);
    }
    SUBCASE("unbekanntes Feld ist eine Warnung") {
        replace_in_file(dir / kDistrictsBra, "    name   = \"Berlin\"\n",
                        "    name   = \"Berlin\"\n    hauptort = berlin_coelln\n");
        const LoadResult r = load(dir);
        INFO(issues_text(r));
        CHECK(r.ok());
        CHECK(has_issue(r, "unbekanntes Feld 'hauptort' in kreis kr_berlin wird ignoriert", false));
    }
}

TEST_CASE("Lader: Ordner history/kreise ist optional") {
    TempDir dir(true);
    remove_district_fields(dir);
    SUBCASE("Ordner fehlt") {
        std::filesystem::remove_all(dir / "history/kreise");
    }
    SUBCASE("Ordner ohne .chron-Dateien") {
        std::filesystem::remove(dir / kDistrictsBra);
        std::filesystem::remove(dir / kDistrictsMid);
    }
    const LoadResult r = load(dir);
    INFO(issues_text(r));
    REQUIRE(r.ok());
    CHECK(r.issues.empty());
    const World& w = *r.world;
    CHECK(w.districts.empty());
    CHECK(w.locations.size() == 6);
    for (const uhrwerk::Location& loc : w.locations) {
        CHECK(loc.district == uhrwerk::kNone);
    }
}

TEST_CASE("Lader: Verweis auf Kreis ohne Ordner history/kreise") {
    TempDir dir(true);
    std::filesystem::remove_all(dir / "history/kreise");
    expect_error(load(dir), "unbekannter Kreis 'kr_berlin' in ort berlin_coelln", kPlacesBra, 13);
}

// ─── Verwaltung, Bundesgebiet, Zollverein und Staatenbestand (seit M7) ──────

namespace {

const char* const kAdminFile = "history/verwaltung/10_sachsen.chron";
const char* const kDresdenDistrict = "    kreis        = kr_dresden\n";

void write_admin_units(const TempDir& dir) {
    testhilfen::write_file(dir / kAdminFile, "# Testdaten\n"
                                             "\n"
                                             "einheit sx_land {\n"
                                             "    name   = \"Kurfürstentum Sachsen\"\n"
                                             "    art    = land\n"
                                             "    quelle = \"Testdaten\"\n"
                                             "}\n"
                                             "\n"
                                             "einheit sx_meissen {\n"
                                             "    name   = \"Meißnischer Kreis\"\n"
                                             "    art    = kreis\n"
                                             "    ueber  = sx_land\n"
                                             "    quelle = \"Testdaten\"\n"
                                             "}\n");
}

/// Ein weiterer Staat NEU mit Hauptstadt Dessau, der zunächst keinen Ort besitzt.
void add_state_neu(const TempDir& dir) {
    testhilfen::write_file(dir / "history/staaten/30_neu.chron", "staat NEU {\n"
                                                                 "    name           = \"Neuland\"\n"
                                                                 "    kurzname       = \"Neuland\"\n"
                                                                 "    farbe          = { 1 2 3 }\n"
                                                                 "    hauptstadt     = dessau\n"
                                                                 "    regierungsform = republik\n"
                                                                 "    herrscher      = \"Rat\"\n"
                                                                 "    konfession     = lutherisch\n"
                                                                 "    schatz         = 0\n"
                                                                 "    kontribution   = 0.1\n"
                                                                 "    akzise         = 0.05\n"
                                                                 "    zoll           = 0.02\n"
                                                                 "    hofhaltung     = 100\n"
                                                                 "    heer           = 2\n"
                                                                 "    quelle         = \"Testdaten\"\n"
                                                                 "}\n");
}

} // namespace

TEST_CASE("Lader: Verwaltung, Bundesgebiet und Zollverein") {
    TempDir dir(true);
    write_admin_units(dir);
    replace_in_file(dir / kPlacesSax, kDresdenDistrict,
                    std::string(kDresdenDistrict) + "    verwaltung   = sx_meissen\n    bund         = ja\n"
                                                    "    1649.06.01 = { verwaltung = sx_land }\n");
    replace_in_file(dir / "history/orte/30_anhalt.chron", "    kreis        = kr_anhalt\n",
                    "    kreis        = kr_anhalt\n    verwaltung   = sx_land\n"
                    "    1649.06.01 = { verwaltung = keine bund = ja }\n");
    replace_in_file(dir / kStatesMid, "    heer           = 6\n",
                    "    heer           = 6\n    zollverein     = ja\n");

    const LoadResult before = load(dir, "westfaelischer_frieden");
    INFO(issues_text(before));
    REQUIRE(before.ok());
    CHECK(before.issues.empty());
    const World& w = *before.world;
    REQUIRE(w.admin_units.size() == 2);
    CHECK(w.admin_units[0].id == "sx_land");
    CHECK(w.admin_units[0].name == "Kurfürstentum Sachsen");
    CHECK(w.admin_units[0].kind == "land");
    CHECK(w.admin_units[0].parent == uhrwerk::kNone);
    CHECK(w.admin_units[0].source == "Testdaten");
    CHECK(w.admin_units[1].parent == 0);
    CHECK(w.find_admin_unit("sx_meissen") == 1);
    CHECK(w.find_admin_unit("sx_gibtsnicht") == uhrwerk::kNone);

    const uhrwerk::Location& dresden = w.locations[static_cast<std::size_t>(w.find_location("dresden"))];
    CHECK(dresden.admin == 1);
    CHECK(dresden.federal);
    const uhrwerk::Location& dessau = w.locations[static_cast<std::size_t>(w.find_location("dessau"))];
    CHECK(dessau.admin == 0);
    CHECK_FALSE(dessau.federal);
    CHECK(w.locations[static_cast<std::size_t>(w.find_location("leipzig"))].admin == uhrwerk::kNone);
    CHECK(w.states[static_cast<std::size_t>(w.find_state("SAX"))].customs_union);
    CHECK_FALSE(w.states[static_cast<std::size_t>(w.find_state("ANH"))].customs_union);

    // Datierte Blöcke: ab 1649.06.01 liegt Dresden direkt im Land, Dessau in keiner Einheit, aber im Bund.
    const LoadResult after = load(dir, "jahresbeginn_1650");
    INFO(issues_text(after));
    REQUIRE(after.ok());
    const World& w2 = *after.world;
    CHECK(w2.locations[static_cast<std::size_t>(w2.find_location("dresden"))].admin == 0);
    const uhrwerk::Location& dessau2 = w2.locations[static_cast<std::size_t>(w2.find_location("dessau"))];
    CHECK(dessau2.admin == uhrwerk::kNone);
    CHECK(dessau2.federal);
}

TEST_CASE("Lader: Fehler bei Verwaltungseinheiten") {
    TempDir dir(true);
    write_admin_units(dir);
    SUBCASE("unbekannte Einheit im Ort") {
        replace_in_file(dir / kPlacesSax, kDresdenDistrict,
                        std::string(kDresdenDistrict) + "    verwaltung   = sx_dresden\n");
        expect_error(load(dir), "unbekannte Verwaltungseinheit 'sx_dresden' in ort dresden", kPlacesSax, 14);
    }
    SUBCASE("unbekannte übergeordnete Einheit") {
        replace_in_file(dir / kAdminFile, "ueber  = sx_land", "ueber  = sx_reich");
        expect_error(load(dir), "unbekannte Verwaltungseinheit 'sx_reich' als 'ueber' von einheit sx_meissen",
                     kAdminFile, 12);
    }
    SUBCASE("Einheit in sich selbst") {
        replace_in_file(dir / kAdminFile, "    art    = land\n",
                        "    art    = land\n    ueber  = sx_meissen\n");
        expect_error(load(dir), "Verwaltungseinheit sx_land liegt (über 'ueber') in sich selbst", kAdminFile,
                     6);
    }
    SUBCASE("Kette in einen Kreis") {
        // sx_vorort führt in den Kreis sx_land ↔ sx_meissen, gehört aber nicht dazu.
        replace_in_file(dir / kAdminFile, "einheit sx_land {",
                        "einheit sx_vorort {\n    name   = \"Vorort\"\n    art    = kreis\n"
                        "    ueber  = sx_land\n    quelle = \"Testdaten\"\n}\n\neinheit sx_land {");
        replace_in_file(dir / kAdminFile, "    art    = land\n",
                        "    art    = land\n    ueber  = sx_meissen\n");
        const LoadResult r = load(dir);
        expect_error(r, "Verwaltungseinheit sx_land liegt (über 'ueber') in sich selbst", kAdminFile);
        CHECK(testhilfen::find_issue(r, "sx_vorort") == nullptr);
    }
    SUBCASE("datierter Eintrag") {
        replace_in_file(dir / kAdminFile, "    art    = land\n",
                        "    art    = land\n    1700.01.01 = { name = \"Kurfürstentum\" }\n");
        expect_error(load(dir), "datierter Eintrag '1700.01.01' in einheit sx_land", kAdminFile, 6);
    }
    SUBCASE("doppelte Kennung") {
        replace_in_file(dir / kAdminFile, "einheit sx_meissen {", "einheit sx_land {");
        expect_error(load(dir), "doppelte Kennung 'sx_land' für Verwaltungseinheit", kAdminFile, 9);
    }
    SUBCASE("ungültige und reservierte Kennung") {
        replace_in_file(dir / kAdminFile, "einheit sx_meissen {", "einheit Sx-Meissen {");
        replace_in_file(dir / kAdminFile, "einheit sx_land {", "einheit keine {");
        const LoadResult r = load(dir);
        expect_error(r, "ungültige Kennung 'Sx-Meissen' für eine Verwaltungseinheit", kAdminFile, 9);
        expect_error(r, "„keine“ ist als Kennung einer Verwaltungseinheit reserviert", kAdminFile, 3);
    }
    SUBCASE("Pflichtfeld fehlt") {
        replace_in_file(dir / kAdminFile, "    art    = kreis\n", "");
        expect_error(load(dir), "Pflichtfeld 'art' fehlt in einheit sx_meissen", kAdminFile, 9);
    }
    SUBCASE("bund und zollverein erwarten ja oder nein") {
        replace_in_file(dir / kPlacesSax, kDresdenDistrict,
                        std::string(kDresdenDistrict) + "    bund         = 1\n");
        replace_in_file(dir / kStatesMid, "    heer           = 6\n",
                        "    heer           = 6\n    zollverein     = 1\n");
        const LoadResult r = load(dir);
        INFO(issues_text(r));
        CHECK_FALSE(r.ok());
        CHECK(has_issue(r, "'bund'"));
        CHECK(has_issue(r, "'zollverein'"));
    }
}

TEST_CASE("Lader: Staaten ohne Ort zum Startdatum bestehen nicht") {
    TempDir dir(true);
    add_state_neu(dir);
    replace_in_file(dir / "history/orte/30_anhalt.chron", "    kreis        = kr_anhalt\n",
                    "    kreis        = kr_anhalt\n    1649.06.01 = { besitzer = NEU }\n");
    SUBCASE("1648: NEU besitzt nichts und fällt weg") {
        const LoadResult r = load(dir, "westfaelischer_frieden");
        INFO(issues_text(r));
        REQUIRE(r.ok());
        CHECK(r.issues.empty());
        const World& w = *r.world;
        CHECK(w.states.size() == 3);
        CHECK(w.find_state("NEU") == uhrwerk::kNone);
        CHECK(w.find_state("ANH") == 2);
    }
    SUBCASE("1650: Anhalt besitzt nichts mehr, NEU rückt nach") {
        const LoadResult r = load(dir, "jahresbeginn_1650");
        INFO(issues_text(r));
        REQUIRE(r.ok());
        const World& w = *r.world;
        REQUIRE(w.states.size() == 3);
        CHECK(w.find_state("ANH") == uhrwerk::kNone);
        const Index neu = w.find_state("NEU");
        CHECK(neu == 2);
        CHECK(w.states[2].tag == "NEU");
        CHECK(w.states[2].capital == w.find_location("dessau"));
        CHECK(w.locations[static_cast<std::size_t>(w.find_location("dessau"))].owner == neu);
        CHECK(w.locations[static_cast<std::size_t>(w.find_location("dresden"))].owner == w.find_state("SAX"));
    }
    SUBCASE("Spieler ohne Ort im gewählten Szenario") {
        replace_in_file(dir / "szenarien/jahresbeginn.chron", "spieler = SAX", "spieler = ANH");
        expect_error(load(dir, "jahresbeginn_1650"),
                     "Spielerstaat 'ANH' von szenario jahresbeginn_1650 besitzt zum Startdatum keinen Ort",
                     "szenarien/jahresbeginn.chron");
        // Im anderen Szenario besteht Anhalt; dort ist der Spieler des ersten kein Fehler.
        const LoadResult other = load(dir, "westfaelischer_frieden");
        INFO(issues_text(other));
        CHECK(other.ok());
    }
    SUBCASE("spielbarer Staat ohne Ort im gewählten Szenario") {
        replace_in_file(dir / "szenarien/jahresbeginn.chron", "spieler = SAX", "spieler = SAX\n    spielbar = { SAX ANH }");
        expect_error(load(dir, "jahresbeginn_1650"),
                     "Spielbarer Staat 'ANH' von szenario jahresbeginn_1650 besitzt zum Startdatum keinen Ort",
                     "szenarien/jahresbeginn.chron");
    }
    SUBCASE("Pflichtfelder nur für bestehende Staaten") {
        replace_in_file(dir / "history/staaten/30_neu.chron", "    heer           = 2\n", "");
        const LoadResult r = load(dir, "westfaelischer_frieden");
        INFO(issues_text(r));
        CHECK(r.ok());
        expect_error(load(dir, "jahresbeginn_1650"), "Pflichtfeld 'heer' fehlt in staat NEU",
                     "history/staaten/30_neu.chron", 1);
    }
    SUBCASE("unbekannter Spieler bleibt ein Fehler") {
        replace_in_file(dir / "szenarien/jahresbeginn.chron", "spieler = SAX", "spieler = XYZ");
        expect_error(load(dir, "westfaelischer_frieden"),
                     "unbekannter Staat 'XYZ' als Spieler in szenario jahresbeginn_1650");
    }
}

TEST_CASE("Lader: spielbare Staaten und Währung des Szenarios") {
    TempDir dir(true);
    const char* const scenario_file = "szenarien/jahresbeginn.chron";
    replace_in_file(dir / scenario_file, "spieler = SAX",
                    "spieler = SAX\n    spielbar = { SAX BRA }\n    waehrung = \"Taler\"");
    SUBCASE("gelesen") {
        const LoadResult r = load(dir, "jahresbeginn_1650");
        INFO(issues_text(r));
        REQUIRE(r.ok());
        CHECK(r.issues.empty());
        CHECK(r.world->scenario.playable == std::vector<std::string>{"SAX", "BRA"});
        CHECK(r.world->scenario.currency == "Taler");
        const LoadResult other = load(dir, "westfaelischer_frieden");
        REQUIRE(other.ok());
        CHECK(other.world->scenario.playable.empty());
        CHECK(other.world->scenario.currency.empty());
    }
    SUBCASE("unbekannter spielbarer Staat") {
        replace_in_file(dir / scenario_file, "{ SAX BRA }", "{ SAX XYZ }");
        expect_error(load(dir), "unbekannter Staat 'XYZ' als spielbarer Staat in szenario jahresbeginn_1650",
                     scenario_file);
    }
    SUBCASE("ungültiger Eintrag") {
        replace_in_file(dir / scenario_file, "{ SAX BRA }", "{ SAX bra }");
        expect_error(load(dir), "ungültiger Eintrag in 'spielbar' von szenario jahresbeginn_1650",
                     scenario_file);
    }
    SUBCASE("Szenarien ohne die übrigen Daten lesen") {
        std::vector<uhrwerk::LoadIssue> issues;
        const std::vector<uhrwerk::Scenario> list = uhrwerk::list_scenarios(dir.path(), issues);
        CHECK(issues.empty());
        REQUIRE(list.size() == 2);
        CHECK(list[0].id == "jahresbeginn_1650");
        CHECK(list[0].start == Date{1650, 1, 1});
        CHECK(list[0].player_tag == "SAX");
        CHECK(list[0].playable.size() == 2);
        CHECK(list[1].id == "westfaelischer_frieden");
        CHECK(list[1].name == "Der Westfälische Frieden");
    }
}

// ─── find_data_dir ──────────────────────────────────────────────────────────

#ifndef _WIN32
namespace {

/// Setzt UHRWERK_DATEN für die Dauer eines Tests (nullptr = entfernen) und stellt den alten Wert wieder her.
class EnvGuard {
public:
    explicit EnvGuard(const char* value) {
        if (const char* old = std::getenv("UHRWERK_DATEN")) {
            old_ = old;
        }
        if (value != nullptr) {
            setenv("UHRWERK_DATEN", value, 1);
        } else {
            unsetenv("UHRWERK_DATEN");
        }
    }
    ~EnvGuard() {
        if (old_) {
            setenv("UHRWERK_DATEN", old_->c_str(), 1);
        } else {
            unsetenv("UHRWERK_DATEN");
        }
    }
    EnvGuard(const EnvGuard&) = delete;
    EnvGuard& operator=(const EnvGuard&) = delete;

private:
    std::optional<std::string> old_;
};

bool same_dir(const std::optional<std::filesystem::path>& a, const std::filesystem::path& b) {
    return a.has_value() && std::filesystem::equivalent(*a, b);
}

} // namespace

TEST_CASE("find_data_dir: Suche aufwärts") {
    const EnvGuard env(nullptr);
    TempDir dir;
    std::filesystem::create_directories(dir / "data/szenarien");
    std::filesystem::create_directories(dir / "a/b/c");
    std::filesystem::create_directories(dir / "leer/data"); // ohne szenarien → kein Treffer

    CHECK(same_dir(uhrwerk::find_data_dir(dir / "a/b/c"), dir / "data"));
    CHECK(same_dir(uhrwerk::find_data_dir(dir.path()), dir / "data"));
    CHECK(same_dir(uhrwerk::find_data_dir(dir / "data/szenarien"), dir / "data"));
    CHECK(same_dir(uhrwerk::find_data_dir(dir / "leer"), dir / "data"));
    CHECK(same_dir(uhrwerk::find_data_dir(dir / "a/b/c/"), dir / "data"));

    const auto found = uhrwerk::find_data_dir(dir / "a/b/c");
    REQUIRE(found.has_value());
    CHECK(found->filename() == "data");
    CHECK(found->is_absolute());
}

TEST_CASE("find_data_dir: Umgebungsvariable UHRWERK_DATEN hat Vorrang") {
    TempDir dir;
    std::filesystem::create_directories(dir / "data/szenarien");
    {
        const EnvGuard env("/irgendwo/daten");
        CHECK(uhrwerk::find_data_dir(dir.path()) == std::filesystem::path("/irgendwo/daten"));
    }
    {
        const EnvGuard env(""); // leer → ignoriert
        CHECK(same_dir(uhrwerk::find_data_dir(dir.path()), dir / "data"));
    }
}
#endif

// ─── Bevölkerung (seit M4, bevoelkerung.md Abschnitt 2) ──────────────────────

TEST_CASE("Lader: Blöcke je Kultur und Altersaufbau") {
    TempDir dir(true);
    replace_in_file(dir / kPlacesBra, "    kultur       = { maerker = 100 }\n    verwuestung  = 0.6",
                    "    kultur       = { maerker = 70 sorben = 30 }\n    verwuestung  = 0.6");
    const LoadResult r = load(dir);
    INFO(issues_text(r));
    REQUIRE(r.ok());
    const World& w = *r.world;
    const Index potsdam = w.find_location("potsdam");
    const uhrwerk::Location& loc = w.locations[static_cast<std::size_t>(potsdam)];
    // Land: Bauern, Handwerker, Adel (keine Kaufleute) × lutherisch × zwei Kulturen.
    REQUIRE(loc.pop_count == 6);
    const Index maerker = w.find_culture("maerker");
    const Index sorben = w.find_culture("sorben");
    const Fixed hundred = Fixed::from_int(100);
    Index p = loc.first_pop;
    for (const char* profession : {"bauern", "handwerker", "adel"}) {
        const Index pi = w.find_profession(profession);
        const Fixed share = w.professions[static_cast<std::size_t>(pi)].share_rural;
        for (const auto& [culture, percent] : {std::pair{maerker, 70}, std::pair{sorben, 30}}) {
            const uhrwerk::PopBlock& pop = w.pops[static_cast<std::size_t>(p++)];
            INFO(profession << " Kultur " << culture);
            CHECK(pop.profession == pi);
            CHECK(pop.culture == culture);
            const Fixed size = Fixed::from_int(1500) * share * Fixed::from_int(100) / hundred *
                               Fixed::from_int(percent) / hundred;
            CHECK(pop.size() == size);
            CHECK(pop.children == size * w.rules.share_children);
            CHECK(pop.young == size * w.rules.share_young);
            CHECK(pop.working == size * w.rules.share_working);
            CHECK(pop.old >= Fixed::zero());
        }
    }
    CHECK(w.location_majority_culture(potsdam) == maerker);
    CHECK(w.location_majority_culture(w.find_location("zerbst")) == uhrwerk::kNone);
}

TEST_CASE("Lader: Startwerte für Lebensstandard und Alphabetisierung") {
    // Seit M5 kommt der Ziel-Lebensstandard aus der Wirtschaft nach dem Einschwingen; er ist als
    // Momentaufnahme veröffentlicht (wirtschaft.md, Abschnitt 4.8).
    const World w = testhilfen::load_fixture();
    const std::size_t professions = w.professions.size();
    REQUIRE(w.snapshot_targets.size() == w.locations.size() * professions);
    for (std::size_t li = 0; li < w.locations.size(); ++li) {
        const uhrwerk::Location& loc = w.locations[li];
        if (!loc.has_data()) {
            continue;
        }
        INFO("Ort " << loc.id);
        for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
            const uhrwerk::PopBlock& pop = w.pops[static_cast<std::size_t>(p)];
            CHECK(pop.living_standard ==
                  w.snapshot_targets[li * professions + static_cast<std::size_t>(pop.profession)]);
            CHECK(pop.living_standard > Fixed::zero());
            CHECK(pop.living_standard < Fixed::from_int(100));
            CHECK(pop.literacy == uhrwerk::literacy_target(w, loc, pop, pop.living_standard));
        }
    }
    // Der Adel ist gebildeter als die Bauern im selben Ort (Grundwert 0,8 statt 0,1).
    const uhrwerk::Location& berlin = w.locations[0];
    const uhrwerk::PopBlock& bauern = w.pops[static_cast<std::size_t>(berlin.first_pop)];
    const uhrwerk::PopBlock& adel = w.pops[static_cast<std::size_t>(berlin.first_pop + berlin.pop_count - 1)];
    REQUIRE(bauern.profession == w.find_profession("bauern"));
    REQUIRE(adel.profession == w.find_profession("adel"));
    CHECK(adel.literacy > bauern.literacy);
}

TEST_CASE("Lader: zu kleine Blöcke gehen beim Start im ähnlichsten Block auf") {
    TempDir dir(true);
    // Potsdam: 1500 Einwohner, Adel 5 % → 75; mit 1 % sorbisch wären das 0,75 Personen.
    replace_in_file(dir / kPlacesBra, "    kultur       = { maerker = 100 }\n    verwuestung  = 0.6",
                    "    kultur       = { maerker = 99 sorben = 1 }\n    verwuestung  = 0.6");
    const LoadResult r = load(dir);
    INFO(issues_text(r));
    REQUIRE(r.ok());
    const World& w = *r.world;
    const Index potsdam = w.find_location("potsdam");
    const uhrwerk::Location& loc = w.locations[static_cast<std::size_t>(potsdam)];
    CHECK(loc.pop_count == 5); // sorbischer Adel aufgelöst, sorbische Bauern und Handwerker bleiben
    for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
        CHECK(w.pops[static_cast<std::size_t>(p)].size() >= Fixed::one());
    }
    CHECK(w.location_population(potsdam).round() == 1500); // niemand geht verloren
    // Die Blöcke liegen weiter zusammenhängend.
    Index next = 0;
    for (const uhrwerk::Location& l : w.locations) {
        CHECK(l.first_pop == next);
        next += l.pop_count;
    }
    CHECK(next == static_cast<Index>(w.pops.size()));
}

TEST_CASE("Lader: Fehler bei Kulturen und neuen Feldern") {
    TempDir dir(true);
    SUBCASE("unbekannte Kultur im Ort") {
        replace_in_file(dir / kPlacesBra, "kultur       = { maerker = 100 }\n    verwuestung  = 0.6",
                        "kultur       = { preussen = 100 }\n    verwuestung  = 0.6");
        expect_error(load(dir), "unbekannte Kultur 'preussen' in ort potsdam");
    }
    SUBCASE("Kulturanteile ergeben nicht 100") {
        replace_in_file(dir / kPlacesBra, "kultur       = { maerker = 100 }\n    verwuestung  = 0.6",
                        "kultur       = { maerker = 60 sorben = 30 }\n    verwuestung  = 0.6");
        expect_error(load(dir), "die Kulturanteile in ort potsdam ergeben 90 %, erwartet 100 %");
    }
    SUBCASE("Kultur doppelt") {
        replace_in_file(dir / kPlacesBra, "kultur       = { maerker = 100 }\n    verwuestung  = 0.6",
                        "kultur       = { maerker = 50 maerker = 50 }\n    verwuestung  = 0.6");
        expect_error(load(dir), "Kultur 'maerker' ist in ort potsdam mehrfach angegeben");
    }
    SUBCASE("kulturen.chron fehlt") {
        std::filesystem::remove(dir / "common/kulturen.chron");
        expect_error(load(dir), "Datei fehlt", "common/kulturen.chron", 0);
    }
    SUBCASE("ungültige Kulturkennung") {
        replace_in_file(dir / "common/kulturen.chron", "kultur sorben {", "kultur Sorben {");
        expect_error(load(dir), "ungültige Kulturkennung 'Sorben'");
    }
    SUBCASE("unbekannter Beruf in in_stadt") {
        replace_in_file(dir / "common/berufe.chron", "in_stadt        = handwerker",
                        "in_stadt        = tageloehner");
        expect_error(load(dir), "unbekannter Beruf 'tageloehner' für 'in_stadt' in beruf bauern",
                     "common/berufe.chron");
    }
    SUBCASE("Altersaufbau größer als 1") {
        replace_in_file(dir / "common/regeln.chron", "anteil_erwerb             = 0.33",
                        "anteil_erwerb             = 0.5");
        expect_error(load(dir), "'anteil_kinder', 'anteil_jung' und 'anteil_erwerb' ergeben zusammen");
    }
    SUBCASE("Pflichtfeld tragfaehigkeit fehlt") {
        replace_in_file(dir / "common/gelaende.chron", "    tragfaehigkeit = 45\n", "");
        expect_error(load(dir), "Pflichtfeld 'tragfaehigkeit' fehlt in gelaende flussaue");
    }
}

// ─── Wirtschaft (seit M5, docs/technik/wirtschaft.md, Abschnitt 2) ─────────

TEST_CASE("Lader: Güter, Bedürfnisse, Betriebe und Märkte") {
    TempDir dir(true);
    const LoadResult r = load(dir);
    INFO(issues_text(r));
    REQUIRE(r.ok());
    const World& w = *r.world;

    REQUIRE(w.goods.size() == 4);
    const Index grain = w.find_good("getreide");
    const Index cloth = w.find_good("tuch");
    const Index salt = w.find_good("salz");
    const Index colonial = w.find_good("kolonialwaren");
    CHECK(grain == 0);
    CHECK(w.goods[0].name == "Getreide");
    CHECK(w.goods[0].base_price == Fixed::one());
    CHECK(w.goods[0].world_factor == fx("0.9"));
    CHECK(w.goods[static_cast<std::size_t>(cloth)].world_factor == Fixed::zero()); // nicht am Weltmarkt
    CHECK(w.goods[static_cast<std::size_t>(colonial)].base_price == Fixed::from_int(8));

    // Bedürfnisse in Dateireihenfolge, Warenkorb je Schicht.
    REQUIRE(w.needs.size() == 3);
    CHECK(w.needs[0].id == "nahrung");
    CHECK(w.needs[0].hunger);
    CHECK_FALSE(w.needs[1].hunger);
    const auto& unten = w.needs[0].basket[static_cast<std::size_t>(uhrwerk::Stratum::Unten)];
    REQUIRE(unten.size() == 2);
    CHECK(unten[0].good == grain);
    CHECK(unten[0].amount == Fixed::from_int(4));
    CHECK(unten[1].good == salt);
    CHECK(unten[1].amount == fx("0.05"));
    CHECK(
        w.needs[2].basket[static_cast<std::size_t>(uhrwerk::Stratum::Unten)].empty()); // leerer Korb erlaubt
    CHECK(w.needs[1].basket[static_cast<std::size_t>(uhrwerk::Stratum::Oben)].size() == 2);

    // Betriebe
    REQUIRE(w.businesses.size() == 4);
    const auto& hof = w.businesses[static_cast<std::size_t>(w.find_business("hof"))];
    CHECK(hof.profession == w.find_profession("bauern"));
    CHECK(hof.agrarian);
    CHECK(hof.siting == uhrwerk::Siting::All);
    REQUIRE(hof.outputs.size() == 1);
    CHECK(hof.outputs[0].amount == Fixed::from_int(40));
    CHECK(hof.inputs.empty());
    CHECK(hof.deposit == uhrwerk::kNone);
    const auto& weberei = w.businesses[static_cast<std::size_t>(w.find_business("weberei"))];
    REQUIRE(weberei.inputs.size() == 1);
    CHECK(weberei.inputs[0].good == grain);
    REQUIRE(weberei.terrain_factors.size() == w.terrains.size());
    CHECK(weberei.terrain_factors[static_cast<std::size_t>(w.find_terrain("sandboden"))] ==
          Fixed::from_int(2));
    CHECK(weberei.terrain_factors[static_cast<std::size_t>(w.find_terrain("flussaue"))] == Fixed::one());
    const auto& saline = w.businesses[static_cast<std::size_t>(w.find_business("saline"))];
    CHECK(saline.siting == uhrwerk::Siting::City);
    REQUIRE(w.deposits.size() == 1);
    CHECK(w.deposits[0] == "salz");
    CHECK(saline.deposit == 0);
    const auto& kontor = w.businesses[static_cast<std::size_t>(w.find_business("kontor"))];
    CHECK(kontor.trade);
    CHECK(kontor.outputs.empty());

    // Märkte: ein Markt kann mehrere Landschaften haben; jeder Ort kennt seinen Markt.
    REQUIRE(w.markets.size() == 2);
    CHECK(w.markets[1].landscapes == std::vector<std::string>{"20_sachsen", "30_anhalt"});
    const Index brandenburg = w.find_market("brandenburg");
    const Index mitte = w.find_market("mitte");
    CHECK(w.locations[static_cast<std::size_t>(w.find_location("potsdam"))].market == brandenburg);
    CHECK(w.locations[static_cast<std::size_t>(w.find_location("leipzig"))].market == mitte);
    CHECK(w.locations[static_cast<std::size_t>(w.find_location("zerbst"))].market == mitte);

    // Orte: Hafen und Vorkommen
    CHECK(w.locations[static_cast<std::size_t>(w.find_location("potsdam"))].is_port);
    CHECK_FALSE(w.locations[static_cast<std::size_t>(w.find_location("berlin_coelln"))].is_port);
    CHECK(w.locations[static_cast<std::size_t>(w.find_location("leipzig"))].deposits == 1u);
    CHECK(w.locations[static_cast<std::size_t>(w.find_location("dresden"))].deposits == 0u);

    // Berufe und Staaten
    CHECK(w.professions[static_cast<std::size_t>(w.find_profession("adel"))].agrarian_levy == fx("0.3"));
    CHECK(w.professions[static_cast<std::size_t>(w.find_profession("bauern"))].agrarian_levy ==
          Fixed::zero());
    const auto& bra = w.states[static_cast<std::size_t>(w.find_state("BRA"))];
    CHECK(bra.tax_rate(TaxKind::Kontribution) == fx("0.10"));
    CHECK(bra.tax_rate(TaxKind::Akzise) == fx("0.05"));
    CHECK(bra.tax_rate(TaxKind::Zoll) == fx("0.03"));
    CHECK(bra.army_per_mille == Fixed::from_int(10));
    CHECK(bra.debt == Fixed::zero()); // schulden ist optional
    CHECK(w.states[static_cast<std::size_t>(w.find_state("ANH"))].debt == Fixed::from_int(1500));

    // Regeln
    CHECK(w.rules.price_max == Fixed::from_int(4));
    CHECK(w.rules.trade_neighbors == 6);
    CHECK(w.rules.settling_months == 60);
    CHECK(w.rules.bankruptcy_years == 20);
    CHECK(w.rules.development_half == Fixed::from_int(200));
}

TEST_CASE("Lader: Fehler in den Wirtschaftsdaten") {
    TempDir dir(true);
    SUBCASE("unbekanntes Gut im Warenkorb") {
        replace_in_file(dir / "common/beduerfnisse.chron", "unten  = { tuch = 0.4 }",
                        "unten  = { tuche = 0.4 }");
        expect_error(load(dir), "unbekanntes Gut 'tuche' in beduerfnis kleidung",
                     "common/beduerfnisse.chron");
    }
    SUBCASE("Warenkorb einer Schicht fehlt") {
        replace_in_file(dir / "common/beduerfnisse.chron", "    unten  = { }\n", "");
        expect_error(load(dir), "Pflichtfeld 'unten' fehlt in beduerfnis luxus", "common/beduerfnisse.chron");
    }
    SUBCASE("Gut doppelt im Mengenblock") {
        replace_in_file(dir / "common/betriebe.chron", "erzeugt  = { getreide = 40 }",
                        "erzeugt  = { getreide = 40 getreide = 1 }");
        expect_error(load(dir), "Gut 'getreide' ist in 'erzeugt' von betrieb hof mehrfach angegeben");
    }
    SUBCASE("negative Menge") {
        replace_in_file(dir / "common/betriebe.chron", "erzeugt  = { getreide = 40 }",
                        "erzeugt  = { getreide = -8 }");
        expect_error(load(dir), "ungültige Menge '-8' für Gut 'getreide'");
    }
    SUBCASE("Betrieb mit unbekanntem Beruf") {
        replace_in_file(dir / "common/betriebe.chron", "beruf    = bauern", "beruf    = fischer");
        expect_error(load(dir), "unbekannter Beruf 'fischer' in betrieb hof", "common/betriebe.chron");
    }
    SUBCASE("Betrieb ohne Erzeugung") {
        replace_in_file(dir / "common/betriebe.chron", "    erzeugt  = { getreide = 40 }\n", "");
        expect_error(load(dir), "Pflichtfeld 'erzeugt' fehlt in betrieb hof");
    }
    SUBCASE("Handel mit Erzeugung") {
        replace_in_file(dir / "common/betriebe.chron", "    handel   = ja\n",
                        "    handel   = ja\n    erzeugt  = { tuch = 1 }\n");
        expect_error(load(dir), "betrieb kontor mit 'handel = ja' erzeugt und verbraucht keine Güter");
    }
    SUBCASE("ungültiger Standort") {
        replace_in_file(dir / "common/betriebe.chron", "ort        = stadt", "ort        = dorf");
        expect_error(load(dir), "ungültiger Wert 'dorf' für 'ort' (erlaubt: alle, stadt, land)");
    }
    SUBCASE("unbekanntes Gelände im Betrieb") {
        replace_in_file(dir / "common/betriebe.chron", "gelaende   = { sandboden = 2 }",
                        "gelaende   = { moor = 2 }");
        expect_error(load(dir), "unbekanntes Gelände 'moor' in betrieb weberei");
    }
    SUBCASE("unbekanntes Vorkommen im Ort") {
        replace_in_file(dir / kPlacesSax, "vorkommen    = { salz }", "vorkommen    = { gold }");
        expect_error(load(dir), "unbekanntes Vorkommen 'gold' in ort leipzig", kPlacesSax);
    }
    SUBCASE("Vorkommen ist nicht datierbar") {
        replace_in_file(dir / kPlacesSax,
                        "    vorkommen    = { salz }                 # seit M5 (Testdaten)\n",
                        "    1650.01.01 = { vorkommen = { salz } }\n");
        expect_error(load(dir), "Feld 'vorkommen' in ort leipzig kann nicht datiert werden", kPlacesSax);
    }
    SUBCASE("Hafen erwartet ja oder nein") {
        replace_in_file(dir / kPlacesBra, "hafen        = ja", "hafen        = vielleicht");
        expect_error(load(dir), "ungültiger Wert 'vielleicht' für 'hafen' (erwartet ja oder nein)",
                     kPlacesBra);
    }
    SUBCASE("Landschaft ohne Markt") {
        replace_in_file(dir / "common/maerkte.chron", "landschaften = { 20_sachsen 30_anhalt }",
                        "landschaften = { 20_sachsen }");
        expect_error(load(dir), "Landschaft '30_anhalt' gehört zu keinem Markt (common/maerkte.chron)",
                     "history/orte/30_anhalt.chron", 0);
    }
    SUBCASE("Landschaft in zwei Märkten") {
        replace_in_file(dir / "common/maerkte.chron", "landschaften = { 10_brandenburg }",
                        "landschaften = { 10_brandenburg 20_sachsen }");
        expect_error(load(dir), "Landschaft '20_sachsen' gehört schon zu markt brandenburg",
                     "common/maerkte.chron");
    }
    SUBCASE("Landschaft ohne Datei ist eine Warnung") {
        replace_in_file(dir / "common/maerkte.chron", "landschaften = { 10_brandenburg }",
                        "landschaften = { 10_brandenburg 99_atlantis }");
        const LoadResult r = load(dir);
        INFO(issues_text(r));
        CHECK(r.ok());
        CHECK(has_issue(r, "Landschaft '99_atlantis' hat keine Datei in history/orte", false));
    }
    SUBCASE("Agrarabgaben über 1") {
        replace_in_file(dir / "common/berufe.chron", "agrarabgabe     = 0.3", "agrarabgabe     = 0.6");
        replace_in_file(dir / "common/berufe.chron", "    bildung         = 0.6\n",
                        "    bildung         = 0.6\n    agrarabgabe     = 0.5\n");
        expect_error(load(dir), "die Summe aller 'agrarabgabe' ist 1.1, höchstens 1 ist erlaubt",
                     "common/berufe.chron");
    }
    SUBCASE("Regelfeld fehlt") {
        replace_in_file(dir / "common/regeln.chron", "    handelsspanne             = 0.25",
                        "    # ohne Handelsspanne");
        expect_error(load(dir), "Pflichtfeld 'handelsspanne' fehlt in regeln", "common/regeln.chron");
    }
    SUBCASE("Zins: Grundsatz über der Obergrenze") {
        replace_in_file(dir / "common/regeln.chron", "zins_basis                = 0.05",
                        "zins_basis                = 0.3");
        expect_error(load(dir), "'zins_basis' (0.3) ist größer als 'zins_max' (0.2)", "common/regeln.chron");
    }
    SUBCASE("Datei fehlt") {
        std::filesystem::remove(dir / "common/gueter.chron");
        expect_error(load(dir), "Datei fehlt", "common/gueter.chron", 0);
    }
}
