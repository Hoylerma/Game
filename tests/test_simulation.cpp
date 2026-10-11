// Tests für die Simulation: Determinismus, Staffelung, Befehle, Formeln, Ereignisse.

#include "test_hilfen.hpp"
#include "uhrwerk/economy.hpp"
#include "uhrwerk/population.hpp"
#include "uhrwerk/rng.hpp"
#include "uhrwerk/simulation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <doctest/doctest.h>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

using uhrwerk::Command;
using uhrwerk::CommandType;
using uhrwerk::Date;
using uhrwerk::Fixed;
using uhrwerk::Index;
using uhrwerk::kNone;
using uhrwerk::Pcg32;
using uhrwerk::RngStream;
using uhrwerk::SimConfig;
using uhrwerk::Simulation;
using uhrwerk::TaxKind;
using uhrwerk::World;

namespace {

Fixed fx(const char* text) {
    const auto value = Fixed::parse(text);
    REQUIRE(value.has_value());
    return *value;
}

Command set_tax(Index state, Fixed value) {
    Command c;
    c.type = CommandType::SetTaxRate;
    c.state = state;
    c.value = value;
    return c;
}

/// Kleine handgebaute Welt mit Werten, die im Binärformat exakt darstellbar sind.
/// Ein Staat TST, ein Ort mit zwei Blöcken (1200 Bauern, 100 Adlige), Datum 1.3.1650.
/// Wirtschaft (seit M5): Getreide und Tuch, ein Markt, ein Bauernhof; der Adel lebt von der
/// Grundrente (Agrarabgabe 1/4).
World make_small_world() {
    World w;
    w.scenario.id = "test";
    w.date = Date{1650, 3, 1};
    w.rules.devastation_yield_loss = Fixed::from_ratio(1, 2);
    w.rules.tax_rate_max = Fixed::from_ratio(1, 2);
    w.rules.price_min = Fixed::from_ratio(1, 4);
    w.rules.price_max = Fixed::from_int(4);
    w.rules.price_adjustment = Fixed::from_ratio(1, 2);
    w.rules.trade_margin = Fixed::from_ratio(1, 8);
    w.rules.living_standard_reference = Fixed::from_int(8);
    w.rules.capacity_city = Fixed::one();

    uhrwerk::TerrainType acker;
    acker.id = "acker";
    acker.yield_factor = Fixed::from_ratio(5, 4);
    acker.capacity = Fixed::from_int(30);
    w.terrains.push_back(acker);

    uhrwerk::Good getreide;
    getreide.id = "getreide";
    getreide.base_price = Fixed::one();
    w.goods.push_back(getreide);
    uhrwerk::Good tuch;
    tuch.id = "tuch";
    tuch.base_price = Fixed::from_int(4);
    w.goods.push_back(tuch);

    uhrwerk::Need nahrung;
    nahrung.id = "nahrung";
    nahrung.hunger = true;
    nahrung.basket[0] = {{0, Fixed::from_int(4)}}; // unten
    nahrung.basket[2] = {{0, Fixed::from_int(4)}}; // oben
    w.needs.push_back(nahrung);
    uhrwerk::Need kleidung;
    kleidung.id = "kleidung";
    kleidung.basket[0] = {{1, Fixed::from_ratio(1, 2)}};
    kleidung.basket[2] = {{1, Fixed::from_int(4)}};
    w.needs.push_back(kleidung);

    uhrwerk::BusinessType hof;
    hof.id = "hof";
    hof.profession = 0;
    hof.outputs = {{0, Fixed::from_int(12)}};
    hof.agrarian = true;
    hof.weight = Fixed::one();
    hof.terrain_factors = {Fixed::one()};
    w.businesses.push_back(hof);

    uhrwerk::Market markt;
    markt.id = "markt";
    markt.landscapes = {"test"};
    w.markets.push_back(markt);

    uhrwerk::Confession luth;
    luth.id = "luth";
    w.confessions.push_back(luth);

    uhrwerk::Profession bauern;
    bauern.id = "bauern";
    bauern.taxable = true;
    w.professions.push_back(bauern);

    uhrwerk::Profession adel;
    adel.id = "adel";
    adel.stratum = uhrwerk::Stratum::Oben;
    adel.taxable = false;
    adel.agrarian_levy = Fixed::from_ratio(1, 4);
    w.professions.push_back(adel);

    uhrwerk::State s;
    s.tag = "TST";
    s.short_name = "Test";
    s.treasury = Fixed::from_int(1000);
    s.tax_rates[0] = Fixed::from_ratio(1, 4);
    s.court_expenses = Fixed::from_int(1200);
    s.capital = 0;
    w.states.push_back(s);

    uhrwerk::Location l;
    l.id = "ort";
    l.name = "Testort";
    l.owner = 0;
    l.terrain = 0;
    l.market = 0;
    l.area_km2 = Fixed::from_int(100);
    l.devastation = Fixed::from_ratio(1, 2);
    l.first_pop = 0;
    l.pop_count = 2;
    w.locations.push_back(l);

    uhrwerk::PopBlock p;
    p.location = 0;
    p.confession = 0;
    p.profession = 0;
    p.working = Fixed::from_int(1200);
    w.pops.push_back(p);
    p.profession = 1;
    p.working = Fixed::from_int(100);
    w.pops.push_back(p);
    return w;
}

/// Fixture ohne Seuchen (damit sich die Bevölkerung nur durch die Demografie ändert).
World fixture_without_plague() {
    World w = testhilfen::load_fixture();
    w.rules.plague_chance = Fixed::zero();
    return w;
}

/// Wie fixture_without_plague, aber ohne Wanderung zwischen Orten: Dann ändert sich die Bevölkerung
/// eines Ortes nur an seinem eigenen Rechentag.
World fixture_without_migration() {
    World w = fixture_without_plague();
    w.rules.migration_targets = 0;
    return w;
}

/// Gesamtbevölkerung aller Orte (Blöcke und Zugänge).
Fixed total_population(const World& w) {
    Fixed total;
    for (std::size_t i = 0; i < w.locations.size(); ++i) {
        total += w.location_population(static_cast<Index>(i));
    }
    return total;
}

/// Summe einer Bilanz über alle Orte.
uhrwerk::PopulationBalance sum_balance(const std::vector<uhrwerk::PopulationBalance>& balances) {
    uhrwerk::PopulationBalance sum;
    for (const uhrwerk::PopulationBalance& b : balances) {
        sum.births += b.births;
        sum.deaths += b.deaths;
        sum.plague_deaths += b.plague_deaths;
        sum.moved_in += b.moved_in;
        sum.moved_out += b.moved_out;
        sum.emigrated += b.emigrated;
    }
    return sum;
}

/// Viele Orte für die Mehrkernberechnung: die Orte mit Daten der Fixture `copies`-mal, jeweils um
/// 0,3 Grad versetzt (Pakete zu 8 Orten brauchen mehr als 16 Orte je Rechentag).
World many_locations(int copies) {
    const World base = testhilfen::load_fixture();
    World w = base;
    w.locations.clear();
    w.pops.clear();
    for (int c = 0; c < copies; ++c) {
        for (const uhrwerk::Location& original : base.locations) {
            if (!original.has_data()) {
                continue;
            }
            uhrwerk::Location loc = original;
            loc.id = original.id + "_" + std::to_string(c);
            loc.longitude += Fixed::from_ratio(3 * (c % 10), 10);
            loc.latitude += Fixed::from_ratio(3 * (c / 10), 10);
            loc.first_pop = static_cast<Index>(w.pops.size());
            for (Index p = original.first_pop; p < original.first_pop + original.pop_count; ++p) {
                uhrwerk::PopBlock pop = base.pops[static_cast<std::size_t>(p)];
                pop.location = static_cast<Index>(w.locations.size());
                // Etwas Vielfalt, damit sich die Orte unterscheiden und wandern.
                pop.young += Fixed::from_int(c % 7);
                w.pops.push_back(pop);
            }
            w.locations.push_back(loc);
        }
    }
    for (uhrwerk::State& s : w.states) {
        s.capital = kNone; // Hauptstädte spielen für die Bevölkerung keine Rolle
    }
    return w;
}

void step_until(Simulation& sim, const Date& date) {
    while (sim.world().date < date) {
        sim.step_day();
    }
    REQUIRE(sim.world().date == date);
}

/// Prüft Tag für Tag: Ort i ändert seine Bevölkerung genau an Monatstag (i % 28) + 1,
/// Staat j seinen Schatz genau an Monatstag (j % 28) + 1. Orte ohne Daten ändern sich nie.
void check_staggering(Simulation& sim, int days) {
    for (int d = 0; d < days; ++d) {
        const World& w = sim.world();
        const std::int32_t today = w.date.day;
        std::vector<Fixed> population;
        for (std::size_t i = 0; i < w.locations.size(); ++i) {
            population.push_back(w.location_population(static_cast<Index>(i)));
        }
        std::vector<Fixed> treasury;
        for (const uhrwerk::State& s : w.states) {
            treasury.push_back(s.treasury);
        }
        const std::string date = uhrwerk::format_date(w.date);

        sim.step_day();

        for (std::size_t i = 0; i < population.size(); ++i) {
            const bool changed = sim.world().location_population(static_cast<Index>(i)) != population[i];
            const bool due =
                sim.world().locations[i].has_data() && static_cast<std::int32_t>(i % 28) + 1 == today;
            INFO("Ort " << i << " am " << date);
            CHECK(changed == due);
        }
        for (std::size_t j = 0; j < treasury.size(); ++j) {
            const bool changed = sim.world().states[j].treasury != treasury[j];
            const bool due = static_cast<std::int32_t>(j % 28) + 1 == today;
            INFO("Staat " << j << " am " << date);
            CHECK(changed == due);
        }
    }
}

} // namespace

// ─── Grundlagen ─────────────────────────────────────────────────────────────

TEST_CASE("Simulation: Tage zählen und Datum weiterschalten") {
    Simulation sim(testhilfen::load_fixture());
    CHECK(sim.days_elapsed() == 0);
    CHECK(sim.config().seed == 1648);
    CHECK(sim.world().date == Date{1648, 10, 24});
    sim.step_day();
    CHECK(sim.days_elapsed() == 1);
    CHECK(sim.world().date == Date{1648, 10, 25});
    sim.step_days(0);
    sim.step_days(-3);
    CHECK(sim.days_elapsed() == 1);
    sim.step_days(69); // bis 1.1.1649 einschließlich
    CHECK(sim.world().date == Date{1649, 1, 2});
    CHECK(sim.days_elapsed() == 70);
}

TEST_CASE("Simulation: Determinismus") {
    World base = testhilfen::load_fixture();
    base.rules.plague_chance = Fixed::from_ratio(1, 2); // viele Ereignisse, damit Seeds sichtbar werden

    Simulation a(base, SimConfig{1648});
    Simulation b(base, SimConfig{1648});
    Simulation c(base, SimConfig{1649});
    CHECK(a.checksum() == b.checksum());
    CHECK(a.checksum() != c.checksum()); // Zufallsströme gehören zum Zustand

    std::uint64_t previous = a.checksum();
    for (int year = 0; year < 10; ++year) {
        a.step_days(365);
        b.step_days(365);
        c.step_days(365);
        INFO("Jahr " << year + 1);
        CHECK(a.checksum() == b.checksum());
        CHECK(a.checksum() != previous);
        previous = a.checksum();
    }
    CHECK(a.checksum() != c.checksum());

    // Gleicher Seed: identische Chronik; anderer Seed: andere Ereignisse.
    REQUIRE(a.world().chronicle.size() == b.world().chronicle.size());
    for (std::size_t i = 0; i < a.world().chronicle.size(); ++i) {
        CHECK(a.world().chronicle[i].text == b.world().chronicle[i].text);
        CHECK(a.world().chronicle[i].date == b.world().chronicle[i].date);
    }
    CHECK_FALSE(a.world().chronicle.empty());
    bool chronicle_differs = a.world().chronicle.size() != c.world().chronicle.size();
    for (std::size_t i = 0; !chronicle_differs && i < a.world().chronicle.size(); ++i) {
        chronicle_differs = a.world().chronicle[i].text != c.world().chronicle[i].text ||
                            a.world().chronicle[i].location != c.world().chronicle[i].location;
    }
    CHECK(chronicle_differs);

    // Ein neu geladener Datensatz ergibt denselben Verlauf (deterministischer Lader).
    World reloaded = testhilfen::load_fixture();
    reloaded.rules.plague_chance = Fixed::from_ratio(1, 2);
    Simulation d(std::move(reloaded), SimConfig{1648});
    d.step_days(3650);
    CHECK(d.checksum() == a.checksum());
}

TEST_CASE("Simulation: Prüfsumme erfasst den veränderlichen Zustand") {
    const World base = testhilfen::load_fixture();
    const std::uint64_t reference = Simulation(base).checksum();

    World w = base;
    w.states[1].treasury += Fixed::from_raw(1);
    CHECK(Simulation(w).checksum() != reference);

    w = base;
    w.pops[17].working += Fixed::from_raw(1);
    CHECK(Simulation(w).checksum() != reference);

    w = base;
    w.locations[3].devastation -= Fixed::from_raw(1);
    CHECK(Simulation(w).checksum() != reference);

    w = base;
    w.chronicle.push_back(uhrwerk::ChronicleEntry{w.date, "Eintrag", kNone, kNone});
    CHECK(Simulation(w).checksum() != reference);

    w = base;
    w.date = uhrwerk::next_day(w.date);
    CHECK(Simulation(w).checksum() != reference);

    w = base;
    w.locations[2].owner = 0;
    CHECK(Simulation(w).checksum() != reference);

    Simulation pending(base);
    pending.submit(set_tax(0, Fixed::from_ratio(1, 5)));
    CHECK(pending.checksum() != reference);

    // Die Kreiszugehörigkeit ist unveränderlicher Inhalt wie Gelände und Lage, kein Zustand.
    w = base;
    w.locations[0].district = 2;
    w.locations[3].district = 1;
    CHECK(Simulation(w).checksum() == reference);

    CHECK(Simulation(base).checksum() == reference);
}

// ─── Staffelung ─────────────────────────────────────────────────────────────

TEST_CASE("Simulation: Staffelung auf der Fixture") {
    Simulation sim(fixture_without_migration());
    REQUIRE_FALSE(sim.world().locations[5].has_data()); // zerbst rechnet nie
    check_staggering(sim, 75); // über einen Monatswechsel und den Jahreswechsel 1649
}

TEST_CASE("Simulation: Staffelung mit mehr als 28 Orten") {
    World w = make_small_world();
    w.rules.fertility = Fixed::from_ratio(1, 8);
    w.rules.fertile_share_working = Fixed::one();
    w.locations.clear();
    w.pops.clear();
    for (int i = 0; i < 30; ++i) {
        uhrwerk::Location l;
        l.id = "ort" + std::to_string(i);
        l.owner = 0;
        l.terrain = 0;
        l.first_pop = i;
        l.pop_count = 1;
        w.locations.push_back(l);
        uhrwerk::PopBlock p;
        p.location = i;
        p.profession = 0;
        p.confession = 0;
        p.working = Fixed::from_int(1000);
        w.pops.push_back(p);
    }
    Simulation sim(std::move(w));
    check_staggering(sim, 62); // März und April 1650
    // Orte 0 und 28 rechnen beide am 1., Orte 1 und 29 am 2.: gleiche Größe nach zwei Monaten.
    CHECK(sim.world().pops[0].size() == sim.world().pops[28].size());
    CHECK(sim.world().pops[1].size() == sim.world().pops[29].size());
}

// ─── Befehle ────────────────────────────────────────────────────────────────

TEST_CASE("Simulation: Steuerbefehl") {
    Simulation sim(testhilfen::load_fixture());
    const Fixed original = sim.world().states[0].tax_rate(TaxKind::Kontribution);

    SUBCASE("gültig: wird am nächsten Tag ausgeführt und vermerkt") {
        sim.submit(set_tax(0, Fixed::from_ratio(1, 5)));
        CHECK(sim.world().states[0].tax_rate(TaxKind::Kontribution) == original); // noch nicht ausgeführt
        sim.step_day();
        CHECK(sim.world().states[0].tax_rate(TaxKind::Kontribution) == Fixed::from_ratio(1, 5));
        REQUIRE(sim.world().chronicle.size() == 1);
        const uhrwerk::ChronicleEntry& e = sim.world().chronicle[0];
        CHECK(e.text == "Brandenburg legt die Kontribution auf 20 % fest.");
        CHECK(e.date == Date{1648, 10, 24});
        CHECK(e.state == 0);
        CHECK(e.location == kNone);
    }
    SUBCASE("Grenzwerte 0 und Maximum sind gültig") {
        sim.submit(set_tax(1, Fixed::zero()));
        sim.submit(set_tax(2, sim.world().rules.tax_rate_max));
        sim.step_day();
        CHECK(sim.world().states[1].tax_rate(TaxKind::Kontribution) == Fixed::zero());
        CHECK(sim.world().states[2].tax_rate(TaxKind::Kontribution) == fx("0.5"));
        REQUIRE(sim.world().chronicle.size() == 2);
        CHECK(sim.world().chronicle[0].text == "Sachsen legt die Kontribution auf 0 % fest.");
        CHECK(sim.world().chronicle[1].text == "Anhalt legt die Kontribution auf 50 % fest.");
    }
    SUBCASE("ungültig: wird verworfen und nicht vermerkt") {
        const std::uint64_t untouched_after_step = [] {
            Simulation reference(testhilfen::load_fixture());
            reference.step_day();
            return reference.checksum();
        }();
        sim.submit(set_tax(-1, Fixed::from_ratio(1, 5)));
        sim.submit(set_tax(kNone, Fixed::from_ratio(1, 5)));
        sim.submit(set_tax(3, Fixed::from_ratio(1, 5)));
        sim.submit(set_tax(0, -Fixed::from_raw(1)));
        sim.submit(set_tax(0, sim.world().rules.tax_rate_max + Fixed::from_raw(1)));
        sim.submit(set_tax(0, Fixed::from_int(5)));
        sim.step_day();
        CHECK(sim.world().states[0].tax_rate(TaxKind::Kontribution) == original);
        CHECK(sim.world().chronicle.empty());
        CHECK(sim.checksum() == untouched_after_step); // als hätte es die Befehle nie gegeben
    }
    SUBCASE("mehrere Befehle an einem Tag: der letzte gilt, ein Eintrag") {
        sim.submit(set_tax(0, Fixed::from_ratio(1, 5)));
        sim.submit(set_tax(0, Fixed::from_ratio(3, 10)));
        sim.step_day();
        CHECK(sim.world().states[0].tax_rate(TaxKind::Kontribution) == Fixed::from_ratio(3, 10));
        REQUIRE(sim.world().chronicle.size() == 1);
        CHECK(sim.world().chronicle[0].text == "Brandenburg legt die Kontribution auf 30 % fest.");
    }
    SUBCASE("unveränderter Steuersatz wird nicht vermerkt") {
        sim.submit(set_tax(0, original));
        sim.step_day();
        CHECK(sim.world().chronicle.empty());
    }
    SUBCASE("neuer Steuersatz gilt schon für die Wirtschaft desselben Tages") {
        step_until(sim, Date{1648, 11, 1}); // Rechentag von BRA (Index 0)
        Simulation twin = sim;
        sim.submit(set_tax(0, Fixed::from_ratio(1, 5)));
        sim.step_day();
        twin.step_day();
        CHECK(sim.world().states[0].last_month_income > twin.world().states[0].last_month_income);
    }
}

// ─── Formeln ────────────────────────────────────────────────────────────────

TEST_CASE("Wirtschaft: Ortsrechnung exakt gegen Handrechnung (wirtschaft.md 4.2–4.4)") {
    // Handrechnung zu Basispreisen (Getreide 1, Tuch 4), Land (keine Akzise), kein Zoll:
    //   Bauern je Kopf und Monat: 12 × 5/4 × 3/4 / 12 = 0,9375 Getreide; Erlös × (1 − 1/8) = 0,8203125;
    //   Grundrente 1/4 → 0,615234375; Kontribution 1/4 → 0,46142578125 (Steuer 0,15380859375).
    //   Adel: Grundrente 0,205078125 × 1200 / 100 = 2,4609375 je Kopf.
    World w = make_small_world();
    const uhrwerk::EconomyTopology topology = uhrwerk::build_economy_topology(w);
    uhrwerk::ensure_economy_state(w, topology);
    uhrwerk::LocationFactors f;
    uhrwerk::compute_location_factors(w, 0, f);
    CHECK(f.condition == fx("0.75"));
    CHECK(f.agrarian == Fixed::one());
    uhrwerk::EconomyScratch scratch;
    uhrwerk::run_location_economy(w, topology, 0, f, scratch);

    const Fixed twelve = Fixed::from_int(12);
    CHECK(w.location_supply[0] == fx("1125"));
    CHECK(w.location_supply[1] == Fixed::zero());
    CHECK(w.location_income[0] == fx("0.46142578125"));
    CHECK(w.location_income[1] == fx("2.4609375"));
    const uhrwerk::LocationEconomy& e = w.economy[0];
    CHECK(e.production == fx("984.375"));
    CHECK(e.taxes[0] == fx("184.5703125"));
    CHECK(e.taxes[1] == Fixed::zero()); // keine Akzise auf dem Land
    CHECK(e.taxes[2] == Fixed::zero()); // keine Einfuhr
    CHECK(e.total_taxes() == fx("184.5703125"));

    // Bauern (unten): 5,537109375 im Jahr reichen für die Nahrung (4) und gut drei Viertel der
    // Kleidung (2): 1,537109375 / 2 = 0,7685546875.
    const Fixed cloth_share = fx("0.7685546875");
    // Adel (oben): 29,53125 im Jahr, Korb 20, davon Nahrung 4 → Rest über die übrigen Bedürfnisse:
    // (29,53125 − 4) / 16 = 1,595703125.
    const Fixed noble_scale = fx("1.595703125");
    const Fixed grain = Fixed::from_int(1200) * Fixed::from_int(4) * Fixed::one() / twelve +
                        Fixed::from_int(100) * Fixed::from_int(4) * Fixed::one() / twelve;
    const Fixed cloth = Fixed::from_int(1200) * Fixed::from_ratio(1, 2) * cloth_share / twelve +
                        Fixed::from_int(100) * Fixed::from_int(4) * noble_scale / twelve;
    CHECK(w.location_demand[0] == grain);
    CHECK(w.location_demand[1] == cloth);
    // Bedarfsdeckung [beruf × bedürfnisse + bedürfnis]
    CHECK(w.location_coverage[0] == Fixed::one());
    CHECK(w.location_coverage[1] == cloth_share);
    CHECK(w.location_coverage[2] == Fixed::one());
    CHECK(w.location_coverage[3] == Fixed::one());
    CHECK(uhrwerk::food_coverage(w, 0, 0) == Fixed::one());

    // Ziel-Lebensstandard aus dem Realeinkommen (Preisniveau 1).
    const Fixed hundred = Fixed::from_int(100);
    const Fixed real_bauern = fx("5.537109375");
    const Fixed real_adel = fx("29.53125");
    CHECK(f.target[0] == hundred * real_bauern / (real_bauern + Fixed::from_int(8)));
    CHECK(f.target[1] == hundred * real_adel / (real_adel + Fixed::from_int(8)));

    SUBCASE("Stadt: Akzise auf den Verbrauch, Kontribution nur auf Agrareinkommen") {
        World city = make_small_world();
        city.locations[0].is_city = true;
        city.states[0].tax_rates[1] = Fixed::from_ratio(1, 4);
        uhrwerk::ensure_economy_state(city, topology);
        uhrwerk::LocationFactors g;
        uhrwerk::compute_location_factors(city, 0, g);
        uhrwerk::run_location_economy(city, topology, 0, g, scratch);
        // Das Einkommen der Bauern kommt ganz aus dem Hof: Kontribution wie auf dem Land.
        CHECK(city.economy[0].taxes[0] == fx("184.5703125"));
        // Akzise: ein Viertel des Werts der gekauften Waren (zu Marktpreisen).
        Fixed bought_value;
        bought_value += city.location_demand[0] * Fixed::one();
        bought_value += city.location_demand[1] * Fixed::from_int(4);
        CHECK(city.economy[0].taxes[1] > Fixed::zero());
        CHECK(city.economy[0].taxes[1] <= bought_value / Fixed::from_int(4));
        // Teurere Waren: niedrigerer Ziel-Lebensstandard als auf dem Land.
        CHECK(g.target[0] < f.target[0]);
    }
    SUBCASE("ohne Adel bleibt die Grundrente bei den Bauern") {
        World free = make_small_world();
        free.pops.pop_back();
        free.locations[0].pop_count = 1;
        uhrwerk::ensure_economy_state(free, topology);
        uhrwerk::LocationFactors g;
        uhrwerk::compute_location_factors(free, 0, g);
        uhrwerk::run_location_economy(free, topology, 0, g, scratch);
        CHECK(free.location_income[0] == fx("0.8203125") * fx("0.75"));
    }
}

TEST_CASE("Wirtschaft: Markt am Monatsersten (wirtschaft.md 4.1)") {
    World w = make_small_world();
    const uhrwerk::EconomyTopology topology = uhrwerk::build_economy_topology(w);
    uhrwerk::ensure_economy_state(w, topology);
    // Meldungen des Ortes: 1000 Getreide angeboten, 1500 gefragt; Tuch 200 angeboten, 100 gefragt.
    w.location_supply = {Fixed::from_int(1000), Fixed::from_int(200)};
    w.location_demand = {Fixed::from_int(1500), Fixed::from_int(100)};
    w.economy[0].traders = Fixed::from_int(10);
    uhrwerk::update_markets(w, topology, {});
    const uhrwerk::MarketState& m = w.market_states[0];
    // Zielpreis 1 × 1,5; halbe Anpassung → 1,25. Tuch: Ziel 4 × 0,5 = 2 → 3.
    CHECK(m.price[0] == fx("1.25"));
    CHECK(m.price[1] == Fixed::from_int(3));
    CHECK(m.buy_ratio[0] == Fixed::from_int(1000) / Fixed::from_int(1500));
    CHECK(m.sell_ratio[0] == Fixed::one());
    CHECK(m.buy_ratio[1] == Fixed::one());
    CHECK(m.sell_ratio[1] == fx("0.5"));
    CHECK(m.import_share[0] == Fixed::zero());
    CHECK(m.supply[0] == Fixed::from_int(1000));
    CHECK(m.demand[1] == Fixed::from_int(100));
    // Handelsspanne: 1/8 des Umsatzes (zu neuen Preisen) je Kopf im Handelshaus.
    const Fixed sales = fx("1.25") * Fixed::from_int(1000) + Fixed::from_int(3) * Fixed::from_int(100);
    CHECK(m.margin_per_trader == Fixed::from_ratio(1, 8) * sales / Fixed::from_int(10));

    SUBCASE("Grenzen: ohne Angebot steigt der Preis zum Höchstpreis, ohne Nachfrage fällt er") {
        w.location_supply = {Fixed::zero(), Fixed::from_int(200)};
        w.location_demand = {Fixed::from_int(10), Fixed::zero()};
        for (int i = 0; i < 40; ++i) {
            uhrwerk::update_markets(w, topology, {});
        }
        CHECK(w.market_states[0].price[0] > fx("3.99"));
        CHECK(w.market_states[0].price[1] < fx("1.01")); // 4 × 1/4
        CHECK(w.market_states[0].buy_ratio[0] == Fixed::zero());
    }
    SUBCASE("Staatsnachfrage: Hofhaltung kauft den Korb der Oberschicht") {
        w.location_supply = {Fixed::from_int(1000), Fixed::from_int(200)};
        w.location_demand = {Fixed::zero(), Fixed::zero()};
        // 1200 Reichstaler je Monat; Korb oben zu Preisen 1,25 und 3: 4 × 1,25 + 4 × 3 = 17.
        const uhrwerk::StateSpending court{0, uhrwerk::Stratum::Oben, Fixed::from_int(1200)};
        World copy = w;
        uhrwerk::update_markets(copy, topology, {court});
        const Fixed baskets = Fixed::from_int(1200) / Fixed::from_int(17);
        CHECK(copy.market_states[0].demand[0] == Fixed::from_int(4) * baskets);
        CHECK(copy.market_states[0].demand[1] == Fixed::from_int(4) * baskets);
    }
}

TEST_CASE("Wirtschaft: Handel zwischen Märkten und mit dem Weltmarkt") {
    // Zwei Märkte, je ein Ort, 100 km auseinander; im Osten ist Getreide knapp.
    World w = make_small_world();
    w.rules.trade_neighbors = 4;
    w.rules.trade_radius_km = Fixed::from_int(200);
    w.rules.trade_rate = Fixed::from_int(5);
    w.rules.freight_land = Fixed::from_ratio(1, 10);
    w.rules.freight_sea = Fixed::from_ratio(1, 10);
    uhrwerk::Market ost;
    ost.id = "ost";
    ost.landscapes = {"ost"};
    w.markets.push_back(ost);
    uhrwerk::Location l = w.locations[0];
    l.id = "ort_ost";
    l.market = 1;
    l.longitude = Fixed::from_ratio(1, 2) + Fixed::from_int(1); // etwa 70 km östlich
    l.first_pop = 2;
    w.locations.push_back(l);
    for (std::size_t i = 0; i < 2; ++i) {
        uhrwerk::PopBlock p = w.pops[i];
        p.location = 1;
        w.pops.push_back(p);
    }
    const uhrwerk::EconomyTopology topology = uhrwerk::build_economy_topology(w);
    REQUIRE(topology.neighbors[0].size() == 1);
    REQUIRE(topology.neighbors[0][0].market == 1);
    const Fixed freight = topology.neighbors[0][0].freight;
    CHECK(freight > Fixed::zero());
    uhrwerk::ensure_economy_state(w, topology);
    // Vormonat: West hat Getreide im Überfluss (Preis 1), Ost ist knapp (Preis 2).
    w.market_states[0].supply[0] = Fixed::from_int(1000);
    w.market_states[0].demand[0] = Fixed::from_int(600);
    w.market_states[1].supply[0] = Fixed::from_int(300);
    w.market_states[1].demand[0] = Fixed::from_int(900);
    w.market_states[1].price[0] = Fixed::from_int(2);
    w.location_supply.assign(w.location_supply.size(), Fixed::zero());
    w.location_demand.assign(w.location_demand.size(), Fixed::zero());

    uhrwerk::update_markets(w, topology, {});
    const Fixed gap = Fixed::from_int(2) - Fixed::one() * (Fixed::one() + freight);
    const Fixed share = Fixed::min(Fixed::one(), Fixed::from_int(5) * gap / Fixed::from_int(2));
    const Fixed flow = share * Fixed::min(Fixed::from_int(1000), Fixed::from_int(900));
    // Höchstens 4/5 des Vorjahresangebots bzw. der Nachfrage.
    const Fixed capped = Fixed::min(flow, Fixed::from_ratio(4, 5) * Fixed::from_int(900));
    CHECK(w.market_states[0].exports[0] == capped);
    CHECK(w.market_states[1].imports[0] == capped);
    CHECK(w.market_states[0].exports[0] == w.market_states[1].imports[0]); // bitgenau
    CHECK(w.market_states[1].import_share[0] == Fixed::one()); // ohne eigenes Angebot diesen Monat
    CHECK(w.market_states[0].imports[0] == Fixed::zero());

    SUBCASE("Weltmarkt: Hafenmarkt kauft Kolonialwaren zum Weltpreis") {
        World port = w;
        uhrwerk::Good kolonial;
        kolonial.id = "kolonial";
        kolonial.base_price = Fixed::from_int(8);
        kolonial.world_factor = Fixed::one();
        port.goods.push_back(kolonial);
        port.locations[1].is_port = true;
        const uhrwerk::EconomyTopology t2 = uhrwerk::build_economy_topology(port);
        port.market_states.clear();
        port.location_supply.clear();
        port.location_demand.clear();
        uhrwerk::ensure_economy_state(port, t2);
        CHECK(t2.port[1]);
        CHECK_FALSE(t2.port[0]);
        port.market_states[1].price[2] = Fixed::from_int(16);  // doppelt so teuer wie am Weltmarkt
        port.market_states[1].demand[2] = Fixed::from_int(50); // Nachfrage des Vormonats
        uhrwerk::update_markets(port, t2, {});
        const Fixed world_price = Fixed::from_int(8);
        const Fixed world_gap = Fixed::from_int(16) - world_price * fx("1.1");
        const Fixed world_share =
            Fixed::min(Fixed::one(), Fixed::from_int(5) * world_gap / Fixed::from_int(16));
        CHECK(port.market_states[1].imports[2] ==
              Fixed::min(world_share * Fixed::from_int(50), Fixed::from_ratio(4, 5) * Fixed::from_int(50)));
        CHECK(port.market_states[0].imports[2] == Fixed::zero()); // ohne Hafen kein Weltmarkt
    }
}

TEST_CASE("Simulation: Staatseinnahmen aus den Ortssteuern") {
    // 1.3.1650: Markt, dann Ort 0, dann Staat 0 (alle am 1.). Einnahmen = Steuern des Ortes,
    // Ausgaben = Hofhaltung / 12 = 100.
    Simulation sim(make_small_world());
    sim.step_day();
    const uhrwerk::LocationEconomy e = sim.location_economy(0);
    const Fixed taxes = e.total_taxes();
    CHECK(taxes > Fixed::from_int(100));
    const uhrwerk::State& s = sim.world().states[0];
    CHECK(s.last_month_income == taxes);
    CHECK(s.last_month_expenses == Fixed::from_int(100));
    CHECK(s.treasury == Fixed::from_int(1000) + taxes - Fixed::from_int(100));

    // Nur am Rechentag: der Rest des Monats ändert den Schatz nicht.
    const Fixed after_first = s.treasury;
    sim.step_days(30); // bis 31.3. einschließlich
    CHECK(sim.world().date == Date{1650, 4, 1});
    CHECK(sim.world().states[0].treasury == after_first);

    // location_economy ändert nichts.
    const std::uint64_t checksum = sim.checksum();
    (void)sim.location_economy(0);
    (void)sim.location_economy(5);
    CHECK(sim.checksum() == checksum);
    CHECK(sim.location_economy(-1).production == Fixed::zero());
    CHECK(sim.location_economy(1).total_taxes() == Fixed::zero());
}

namespace {

/// Kleine Welt mit Haushaltsregeln (Werte wie data/common/regeln.chron) und einem zweiten Ort des
/// Staates ohne Bevölkerung (für die Verteilung des Landesausbaus nach Fläche).
World make_budget_world() {
    World w = make_small_world();
    uhrwerk::Rules& r = w.rules;
    r.administration_per_capita = fx("0.2");
    r.soldier_pay = Fixed::from_int(30);
    r.reserve_years = Fixed::from_ratio(1, 2);
    r.building_share = Fixed::from_ratio(1, 20);
    r.development_max = fx("0.3");
    r.development_half = Fixed::from_int(200);
    r.interest_base = fx("0.05");
    r.interest_risk = fx("0.02");
    r.interest_max = fx("0.2");
    r.bankruptcy_ratio = Fixed::from_int(4);
    r.bankruptcy_haircut = Fixed::from_ratio(1, 2);
    r.bankruptcy_premium = fx("0.04");
    r.bankruptcy_years = 20;
    w.states[0].army_per_mille = Fixed::from_int(10);
    w.states[0].yearly_income = Fixed::from_int(4320);

    uhrwerk::Location leer = w.locations[0];
    leer.id = "leer";
    leer.area_km2 = Fixed::from_int(300);
    leer.first_pop = 2;
    leer.pop_count = 0;
    w.locations.push_back(leer);
    w.economy.assign(w.locations.size(), {});
    w.economy[0].taxes = {Fixed::from_int(300), Fixed::from_int(50), Fixed::from_int(10)};
    w.location_development.assign(w.locations.size(), Fixed::zero());
    return w;
}

} // namespace

TEST_CASE("Simulation: Staatshaushalt exakt (Abschnitt 4.6)") {
    World w = make_budget_world();
    const std::vector<Index> locations{0, 1};
    const Fixed twelve = Fixed::from_int(12);
    const Fixed income = Fixed::from_int(360);
    const Fixed people = w.location_population(0);
    REQUIRE(people == Fixed::from_int(1300));
    const Fixed court = Fixed::from_int(100); // 1.200 / 12
    const Fixed admin = fx("0.2") * people / twelve;
    const Fixed army = Fixed::from_int(10) * people / Fixed::from_int(1000) * Fixed::from_int(30) / twelve;
    uhrwerk::State& s = w.states[0];

    SUBCASE("Überschuss über der Rücklage: Bauten und Landesausbau nach Fläche") {
        s.treasury = Fixed::from_int(5000);
        CHECK_FALSE(uhrwerk::run_state_budget(w, 0, locations));
        const uhrwerk::StateBudget& b = s.budget;
        CHECK(b.taxes[0] == Fixed::from_int(300));
        CHECK(b.taxes[1] == Fixed::from_int(50));
        CHECK(b.taxes[2] == Fixed::from_int(10));
        CHECK(b.court == court);
        CHECK(b.administration == admin);
        CHECK(b.army == army);
        CHECK(b.interest == Fixed::zero());
        const Fixed available = Fixed::from_int(5000) + income - court - admin - army;
        const Fixed reserve = Fixed::from_ratio(1, 2) * (court + admin + army) * twelve;
        const Fixed building = (available - reserve) * Fixed::from_ratio(1, 20);
        CHECK(b.building == building);
        CHECK(b.expenses() == court + admin + army + building);
        CHECK(s.treasury == Fixed::from_int(5000) + (income - b.expenses()));
        CHECK(s.debt == Fixed::zero());
        CHECK(s.last_month_income == income);
        CHECK(s.last_month_expenses == b.expenses());
        CHECK(s.yearly_income == Fixed::from_int(4320)); // 12 × 360, unverändert
        CHECK(s.interest_rate == fx("0.05"));

        // Landesausbau: ein Viertel der Fläche, ein Viertel der Bauten.
        const Fixed dev0 = building * Fixed::from_int(100) / Fixed::from_int(400);
        const Fixed dev1 = building * Fixed::from_int(300) / Fixed::from_int(400);
        CHECK(w.location_development[0] == dev0);
        CHECK(w.location_development[1] == dev1);
        const Fixed per_km2 = dev0 / Fixed::from_int(100);
        CHECK(uhrwerk::development_bonus(w, 0) == fx("0.3") * per_km2 / (per_km2 + Fixed::from_int(200)));
        CHECK(uhrwerk::development_bonus(w, 0) > Fixed::zero());

        // Staatsnachfrage im Markt der Hauptstadt: Hof oben, Verwaltung und Bauten mittel, Heer unten.
        const std::vector<uhrwerk::StateSpending> spending = uhrwerk::state_spending(w);
        REQUIRE(spending.size() == 3);
        CHECK(spending[0].market == 0);
        CHECK(spending[0].stratum == uhrwerk::Stratum::Oben);
        CHECK(spending[0].amount == court);
        CHECK(spending[1].stratum == uhrwerk::Stratum::Mittel);
        CHECK(spending[1].amount == admin + building);
        CHECK(spending[2].stratum == uhrwerk::Stratum::Unten);
        CHECK(spending[2].amount == army);
    }

    SUBCASE("Landesausbau sättigt bei ausbau_max") {
        w.location_development[0] = Fixed::from_int(1'000'000);
        CHECK(uhrwerk::development_bonus(w, 0) < fx("0.3"));
        CHECK(uhrwerk::development_bonus(w, 0) > fx("0.29"));
        CHECK(uhrwerk::development_bonus(w, 5) == Fixed::zero());
    }

    SUBCASE("Unter der Rücklage wird nicht gebaut") {
        s.treasury = Fixed::from_int(200);
        CHECK_FALSE(uhrwerk::run_state_budget(w, 0, locations));
        CHECK(s.budget.building == Fixed::zero());
        CHECK(s.treasury == Fixed::from_int(200) + income - court - admin - army);
        CHECK(w.location_development[0] == Fixed::zero());
    }

    SUBCASE("Fehlbetrag wird geliehen") {
        s.treasury = Fixed::from_int(50);
        s.court_expenses = Fixed::from_int(12000); // 1.000 je Monat
        CHECK_FALSE(uhrwerk::run_state_budget(w, 0, locations));
        const Fixed expenses = Fixed::from_int(1000) + admin + army;
        CHECK(s.budget.expenses() == expenses);
        CHECK(s.budget.building == Fixed::zero());
        CHECK(s.treasury == Fixed::zero());
        CHECK(s.debt == expenses - income - Fixed::from_int(50));
    }

    SUBCASE("Überschuss tilgt zuerst die Schulden") {
        s.treasury = Fixed::zero();
        s.debt = Fixed::from_int(1000);
        CHECK_FALSE(uhrwerk::run_state_budget(w, 0, locations));
        const Fixed interest = Fixed::from_int(1000) * fx("0.05") / twelve; // Schulden unter J
        CHECK(s.budget.interest == interest);
        CHECK(s.budget.building == Fixed::zero()); // keine Bauten mit Schulden
        CHECK(s.debt == Fixed::from_int(1000) - (income - court - admin - army - interest));
        CHECK(s.treasury == Fixed::zero());

        // Ist alles getilgt, bleibt der Rest im Schatz.
        s.debt = Fixed::from_int(10);
        CHECK_FALSE(uhrwerk::run_state_budget(w, 0, locations));
        const Fixed interest2 = Fixed::from_int(10) * fx("0.05") / twelve;
        CHECK(s.debt == Fixed::zero());
        CHECK(s.treasury == income - court - admin - army - interest2 - Fixed::from_int(10));
    }

    SUBCASE("Risikoaufschlag, Staatsbankrott und Zinsaufschlag danach") {
        s.treasury = Fixed::zero();
        s.debt = Fixed::from_int(8000);
        s.yearly_income = Fixed::from_int(1200);
        CHECK(uhrwerk::run_state_budget(w, 0, locations));
        const Fixed rate =
            fx("0.05") + fx("0.02") * (Fixed::from_int(8000) / Fixed::from_int(1200) - Fixed::one());
        CHECK(s.interest_rate == rate);
        const Fixed interest = Fixed::from_int(8000) * rate / twelve;
        CHECK(s.budget.interest == interest);
        const Fixed yearly = Fixed::from_int(1200) + (income * twelve - Fixed::from_int(1200)) / twelve;
        CHECK(s.yearly_income == yearly);
        const Fixed debt = Fixed::from_int(8000) - (income - court - admin - army - interest);
        REQUIRE(debt > Fixed::from_int(4) * yearly);
        CHECK(s.debt == debt - debt * Fixed::from_ratio(1, 2));
        CHECK(s.premium_months == 240);
        CHECK(s.bankruptcies == 1);

        // Im nächsten Monat: Zins mit Aufschlag, ein Monat weniger.
        const Fixed debt_after = s.debt;
        CHECK_FALSE(uhrwerk::run_state_budget(w, 0, locations));
        const Fixed excess = debt_after / yearly - Fixed::one();
        const Fixed expected = Fixed::min(
            fx("0.05") + (excess > Fixed::zero() ? fx("0.02") * excess : Fixed::zero()) + fx("0.04"),
            fx("0.2"));
        CHECK(s.interest_rate == expected);
        CHECK(s.premium_months == 239);
        CHECK(s.bankruptcies == 1);
    }

    SUBCASE("Zins höchstens zins_max") {
        s.debt = Fixed::from_int(60000); // 0,05 + 0,02 × (60.000 / 4.320 − 1) + 0,04 > 0,2
        s.yearly_income = Fixed::from_int(4320);
        s.premium_months = 5;
        (void)uhrwerk::run_state_budget(w, 0, locations);
        CHECK(s.interest_rate == fx("0.2"));
    }
}

TEST_CASE("Simulation: Wirtschaft auf der Fixture") {
    Simulation sim(fixture_without_plague());

    // Dresden (Index 2) rechnet am 3. des Monats; Steuern in plausibler Größe.
    step_until(sim, Date{1648, 11, 4});
    const uhrwerk::LocationEconomy dresden = sim.location_economy(2);
    CHECK(dresden.production > Fixed::zero());
    CHECK(dresden.total_taxes() > Fixed::from_int(100));
    CHECK(dresden.total_taxes() < Fixed::from_int(10000));
    CHECK(dresden.taxes[1] > Fixed::zero()); // Dresden ist eine Stadt: Akzise

    // Haushalt (Abschnitt 4.6): Einnahmen = Summe der zuletzt festgestellten Ortssteuern, Ausgaben nach
    // Hof, Verwaltung und Heer; Sachsen hat keine Schulden und baut (SAX rechnet am 2.).
    step_until(sim, Date{1648, 12, 2});
    const uhrwerk::State sax = sim.world().states[1];
    sim.step_day();
    const World& w = sim.world();
    Fixed income;
    Fixed people;
    for (const Index li : w.state_locations(1)) {
        income += w.economy[static_cast<std::size_t>(li)].total_taxes();
        people += w.location_population(li);
    }
    const Fixed twelve = Fixed::from_int(12);
    const uhrwerk::State& after = w.states[1];
    const uhrwerk::StateBudget& b = after.budget;
    CHECK(sax.debt == Fixed::zero());
    CHECK(b.income() == income);
    CHECK(b.court == sax.court_expenses / twelve);
    CHECK(b.administration == w.rules.administration_per_capita * people / twelve);
    CHECK(b.army == sax.army_per_mille * people / Fixed::from_int(1000) * w.rules.soldier_pay / twelve);
    CHECK(b.interest == Fixed::zero());
    CHECK(b.building > Fixed::zero());
    CHECK(after.last_month_income == income);
    CHECK(after.last_month_expenses == b.expenses());
    CHECK(after.treasury == sax.treasury + (income - b.expenses()));
    CHECK(after.debt == Fixed::zero());
}

TEST_CASE("Simulation: Bevölkerungsformeln exakt gegen Nachrechnung (Abschnitt 4.2–4.4)") {
    // Ein Ort, ein Block, keine Wanderung: Der Schritt am Rechentag muss genau den Formeln folgen.
    World w = make_small_world();
    w.pops.pop_back();
    w.locations[0].pop_count = 1;
    w.locations[0].area_km2 = Fixed::from_int(100);
    w.terrains[0].capacity = Fixed::from_int(30);
    w.professions[0].agrarian = true;
    w.professions[0].literacy = Fixed::from_ratio(1, 5);
    w.confessions[0].literacy_factor = Fixed::from_ratio(6, 5);
    uhrwerk::Rules& r = w.rules;
    r.fertility = *Fixed::parse("0.088");
    r.fertile_share_working = *Fixed::parse("0.4");
    r.fertility_reserve = *Fixed::parse("0.3");
    r.fertility_wealth = *Fixed::parse("0.15");
    r.mortality_children = *Fixed::parse("0.045");
    r.mortality_young = *Fixed::parse("0.010");
    r.mortality_working = *Fixed::parse("0.018");
    r.mortality_old = *Fixed::parse("0.11");
    r.mortality_poverty = *Fixed::parse("0.3");
    r.capacity_city = Fixed::one();
    r.living_standard_reference = Fixed::from_int(12);
    r.living_standard_adjustment = Fixed::from_ratio(1, 4);
    r.literacy_city = Fixed::one();
    r.literacy_adjustment = *Fixed::parse("0.03");
    r.emigration_threshold = Fixed::from_int(30);
    r.emigration_rate = *Fixed::parse("0.05");
    r.block_min_size = Fixed::one();
    uhrwerk::PopBlock& pop = w.pops[0];
    pop.children = Fixed::from_int(340);
    pop.young = Fixed::from_int(260);
    pop.working = Fixed::from_int(330);
    pop.old = Fixed::from_int(70);
    pop.living_standard = Fixed::from_int(20);
    pop.literacy = Fixed::from_ratio(1, 10);

    Simulation sim(std::move(w)); // 1.3.1650: Rechentag von Ort 0
    const World before = sim.world();
    sim.step_day();
    const uhrwerk::PopBlock& after = sim.world().pops[0];

    // Nachrechnung in der Reihenfolge der Spezifikation: am Monatsersten zuerst der Markt, dann die
    // Wirtschaft des Ortes (Ziel-Lebensstandard, Bedarfsdeckung), dann die Bevölkerung.
    const Fixed one = Fixed::one();
    const Fixed fifty = Fixed::from_int(50);
    const Fixed twelve = Fixed::from_int(12);
    World economy = before;
    const uhrwerk::EconomyTopology topology = uhrwerk::build_economy_topology(economy);
    uhrwerk::update_markets(economy, topology, uhrwerk::state_spending(economy));
    uhrwerk::LocationFactors f;
    uhrwerk::compute_location_factors(economy, 0, f);
    uhrwerk::EconomyScratch scratch;
    uhrwerk::run_location_economy(economy, topology, 0, f, scratch);
    CHECK(f.population == Fixed::from_int(1000));
    CHECK(f.capacity == Fixed::from_int(3000));
    const Fixed gap = one - uhrwerk::food_coverage(economy, 0, 0);
    const Fixed hunger = one + r.mortality_hunger * gap * gap;
    const uhrwerk::PopBlock& old = before.pops[0];
    const Fixed living =
        old.living_standard + (f.target[0] - old.living_standard) * r.living_standard_adjustment;
    const Fixed deviation = (living - fifty) / fifty;
    const Fixed mortality =
        (one - r.mortality_poverty * deviation).clamp(Fixed::from_ratio(1, 2), Fixed::from_int(2));
    const Fixed mortality_month = mortality * hunger / twelve;
    const Fixed wealth = Fixed::max(Fixed::zero(), one + r.fertility_wealth * deviation);
    const Fixed fertility_month = r.fertility * (one + r.fertility_reserve * f.reserve) * wealth / twelve;
    const Fixed births = (old.young + old.working * r.fertile_share_working) * fertility_month;
    const Fixed dk = old.children * r.mortality_children * mortality_month;
    const Fixed dj = old.young * r.mortality_young * mortality_month;
    const Fixed de = old.working * r.mortality_working * mortality_month;
    const Fixed da = old.old * r.mortality_old * mortality_month;
    const Fixed ak = old.children * Fixed::from_ratio(1, 180);
    const Fixed aj = old.young * Fixed::from_ratio(1, 180);
    const Fixed ae = old.working * Fixed::from_ratio(1, 420);
    const Fixed young = old.young + ak - dj - aj;
    // Lebensstandard unter der Schwelle 30: Auswanderung der Jungen.
    REQUIRE(living < r.emigration_threshold);
    const Fixed emigrated =
        young * r.emigration_rate * (r.emigration_threshold - living) / Fixed::from_int(1200);

    CHECK(after.living_standard == living);
    CHECK(after.children == old.children + births - dk - ak);
    CHECK(after.young == young - emigrated);
    CHECK(after.working == old.working + aj - de - ae);
    CHECK(after.old == old.old + ae - da);
    const Fixed literacy_target = uhrwerk::literacy_target(before, before.locations[0], old, living);
    CHECK(after.literacy == old.literacy + (literacy_target - old.literacy) * r.literacy_adjustment / twelve);

    const uhrwerk::PopulationBalance& b = sim.world().balance_current[0];
    CHECK(b.births == births);
    CHECK(b.deaths == dk + dj + de + da);
    CHECK(b.emigrated == emigrated);
    CHECK(b.moved_out == Fixed::zero());

    // Größenordnung: rund 3,7 Geburten und 3 Sterbefälle je Monat bei 1.000 Menschen.
    CHECK(births > Fixed::from_int(2));
    CHECK(births < Fixed::from_int(5));
    CHECK(b.deaths > Fixed::from_int(2));
    CHECK(b.deaths < Fixed::from_int(5));

    SUBCASE("kein Wert wird negativ, auch bei extremen Regeln") {
        World extreme = before;
        extreme.rules.mortality_children = Fixed::one();
        extreme.rules.mortality_young = Fixed::one();
        extreme.rules.mortality_working = Fixed::one();
        extreme.rules.mortality_old = Fixed::one();
        extreme.rules.mortality_poverty = Fixed::one();
        extreme.rules.emigration_rate = Fixed::from_int(10);
        extreme.rules.emigration_threshold = Fixed::from_int(100);
        extreme.pops[0].living_standard = Fixed::zero();
        Simulation harsh(std::move(extreme));
        for (int year = 0; year < 30; ++year) {
            harsh.step_days(365);
            const uhrwerk::PopBlock& p = harsh.world().pops[0];
            CHECK(p.children >= Fixed::zero());
            CHECK(p.young >= Fixed::zero());
            CHECK(p.working >= Fixed::zero());
            CHECK(p.old >= Fixed::zero());
        }
        CHECK(harsh.world().pops[0].size() < Fixed::from_int(10)); // fast ausgestorben
    }
}

// ─── Jahresereignisse ───────────────────────────────────────────────────────

TEST_CASE("Simulation: Seuche mit Wahrscheinlichkeit 1 und Chronikeintrag") {
    World w = testhilfen::load_fixture();
    w.rules.plague_chance = Fixed::one();
    // Ohne Wanderung ändert sich am 1. Januar nur der Ort mit diesem Rechentag (Index 0) außer
    // durch die Seuche.
    w.rules.migration_targets = 0;
    Simulation sim(std::move(w), SimConfig{4711});

    step_until(sim, Date{1649, 1, 1});
    CHECK(sim.world().chronicle.empty());
    const World before = sim.world();
    sim.step_day();
    const World& after = sim.world();

    // Erwartung mit eigenem Ereignisstrom nachrechnen: chance(1) verbraucht nichts, uniform() eine Zahl je
    // Ort mit Daten. Orte ohne Daten werden übersprungen.
    Pcg32 events(4711, RngStream::Ereignisse);
    REQUIRE(before.locations_with_data() == 5);
    REQUIRE(after.chronicle.size() == before.locations_with_data());
    std::size_t entry_index = 0;
    for (std::size_t i = 0; i < before.locations.size(); ++i) {
        const uhrwerk::Location& loc = before.locations[i];
        if (!loc.has_data()) {
            CHECK(after.locations[i].devastation == Fixed::zero());
            continue;
        }
        const Fixed loss = events.uniform(before.rules.plague_loss_min, before.rules.plague_loss_max);
        CHECK(loss >= fx("0.04"));
        CHECK(loss < fx("0.12"));
        // Die Seuche trifft jede Altersgruppe mit demselben Anteil. Ort 0 rechnet am 1. Januar
        // vorher seine Bevölkerung; dort prüft die Bilanz.
        Fixed dead;
        for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count && i != 0; ++p) {
            const uhrwerk::PopBlock& old_pop = before.pops[static_cast<std::size_t>(p)];
            const uhrwerk::PopBlock& new_pop = after.pops[static_cast<std::size_t>(p)];
            CHECK(new_pop.children == old_pop.children - old_pop.children * loss);
            CHECK(new_pop.young == old_pop.young - old_pop.young * loss);
            CHECK(new_pop.working == old_pop.working - old_pop.working * loss);
            CHECK(new_pop.old == old_pop.old - old_pop.old * loss);
            dead +=
                old_pop.children * loss + old_pop.young * loss + old_pop.working * loss + old_pop.old * loss;
        }
        if (i != 0) {
            CHECK(after.balance_current[i].plague_deaths == dead);
        }
        CHECK(after.balance_current[i].plague_deaths > Fixed::zero());
        const uhrwerk::ChronicleEntry& e = after.chronicle[entry_index++];
        const std::int64_t percent = (loss * Fixed::from_int(100)).round();
        CHECK(percent >= 4);
        CHECK(percent <= 12);
        CHECK(e.text ==
              "Seuche in " + loc.name + ": " + std::to_string(percent) + " % der Bevölkerung sterben.");
        CHECK(e.date == Date{1649, 1, 1});
        CHECK(e.location == static_cast<Index>(i));
        CHECK(e.state == loc.owner);
    }
    CHECK(after.chronicle[0].text.rfind("Seuche in Berlin-Cölln: ", 0) == 0);

    // Am Folgetag keine weiteren Seuchen.
    sim.step_day();
    CHECK(sim.world().chronicle.size() == before.locations_with_data());
}

TEST_CASE("Simulation: Seuche mit erhöhter Wahrscheinlichkeit folgt dem Ereignisstrom") {
    World w = testhilfen::load_fixture();
    w.rules.plague_chance = Fixed::from_ratio(1, 2);
    std::vector<bool> has_data;
    for (const uhrwerk::Location& loc : w.locations) {
        has_data.push_back(loc.has_data());
    }
    REQUIRE(has_data == std::vector<bool>{true, true, true, true, true, false});
    const Fixed chance = w.rules.plague_chance;
    const Fixed lo = w.rules.plague_loss_min;
    const Fixed hi = w.rules.plague_loss_max;
    Simulation sim(std::move(w));
    step_until(sim, Date{1661, 1, 2}); // Jahreswechsel 1649 … 1661

    Pcg32 events(SimConfig{}.seed, RngStream::Ereignisse);
    std::vector<std::pair<Index, std::string>> expected;
    for (int year = 1649; year <= 1661; ++year) {
        for (std::size_t i = 0; i < has_data.size(); ++i) {
            if (!has_data[i]) {
                continue; // Orte ohne Daten ziehen keine Zufallszahlen
            }
            if (events.chance(chance)) {
                const Fixed loss = events.uniform(lo, hi);
                expected.emplace_back(static_cast<Index>(i),
                                      std::to_string((loss * Fixed::from_int(100)).round()) + " % der");
            }
        }
    }
    const auto& chronicle = sim.world().chronicle;
    REQUIRE(chronicle.size() == expected.size());
    CHECK(expected.size() > 15); // 13 Jahre × 5 Orte × ½
    CHECK(expected.size() < 50);
    for (std::size_t k = 0; k < expected.size(); ++k) {
        CHECK(chronicle[k].location == expected[k].first);
        CHECK(chronicle[k].text.find(expected[k].second) != std::string::npos);
        CHECK(chronicle[k].date.month == 1);
        CHECK(chronicle[k].date.day == 1);
    }
}

TEST_CASE("Simulation: keine Seuchen bei Wahrscheinlichkeit 0") {
    Simulation sim(fixture_without_plague());
    sim.step_days(50 * 365);
    CHECK(sim.world().chronicle.empty());
}

TEST_CASE("Simulation: Abbau der Verwüstung am Jahreswechsel") {
    Simulation sim(fixture_without_plague());
    const World initial = sim.world();
    step_until(sim, Date{1649, 1, 1});
    // Bis zum Jahreswechsel bleibt die Verwüstung unverändert.
    for (std::size_t i = 0; i < initial.locations.size(); ++i) {
        CHECK(sim.world().locations[i].devastation == initial.locations[i].devastation);
    }
    sim.step_day();
    const Fixed decay = initial.rules.devastation_decay;
    for (std::size_t i = 0; i < initial.locations.size(); ++i) {
        const Fixed old = initial.locations[i].devastation;
        CHECK(sim.world().locations[i].devastation == old - old * decay);
    }

    // Über 100 Jahre: monoton fallend, am Ende unter 5 % des Ausgangswerts (0,97^100 ≈ 0,048).
    std::vector<Fixed> last;
    for (const uhrwerk::Location& l : sim.world().locations) {
        last.push_back(l.devastation);
    }
    for (int year = 1650; year <= 1748; ++year) {
        step_until(sim, Date{year, 1, 2});
        for (std::size_t i = 0; i < last.size(); ++i) {
            if (!initial.locations[i].has_data()) {
                CHECK(sim.world().locations[i].devastation == Fixed::zero()); // Ort ohne Daten
                continue;
            }
            CHECK(sim.world().locations[i].devastation < last[i]);
            last[i] = sim.world().locations[i].devastation;
        }
    }
    for (std::size_t i = 0; i < last.size(); ++i) {
        if (!initial.locations[i].has_data()) {
            continue;
        }
        CHECK(last[i] > Fixed::zero());
        CHECK(last[i] < initial.locations[i].devastation * Fixed::from_ratio(5, 100));
    }
}

// ─── Orte ohne Daten ────────────────────────────────────────────────────────

TEST_CASE("Simulation: Orte ohne Daten ändern die Ergebnisse der übrigen Orte nicht") {
    // Vergleichslauf ohne Orte ohne Daten: Fixture ohne zerbst.
    const testhilfen::TempDir plain(true);
    {
        const std::filesystem::path path = plain / "history/orte/30_anhalt.chron";
        const std::string text = testhilfen::read_file(path);
        const std::size_t cut = text.find("\n# Ort ohne Daten");
        REQUIRE(cut != std::string::npos);
        testhilfen::write_file(path, text.substr(0, cut + 1));
    }
    // Lauf mit 41 Orten ohne Daten hinten (zerbst und 40 weitere: alle 28 Rechentage kommen vor).
    const testhilfen::TempDir extended(true);
    std::string extra;
    for (int i = 0; i < 40; ++i) {
        extra +=
            "ort leer_" + std::to_string(i) + " {\n    name = \"Leer " + std::to_string(i) +
            "\"\n    gelaende = ackerland\n    lage = { 12.5 51.5 }\n    flaeche = 100\n    stadt = nein\n"
            "    kreis = kr_anhalt\n    quelle = \"Testdaten\"\n}\n";
    }
    testhilfen::write_file(extended / "history/orte/90_ohne_daten.chron", extra);
    testhilfen::add_market(extended.path(), "90_ohne_daten");

    const uhrwerk::LoadResult loaded_plain = uhrwerk::load_world(plain.path());
    const uhrwerk::LoadResult loaded_extended = uhrwerk::load_world(extended.path());
    INFO(testhilfen::issues_text(loaded_plain) << testhilfen::issues_text(loaded_extended));
    REQUIRE(loaded_plain.ok());
    REQUIRE(loaded_extended.ok());
    World a = *loaded_plain.world;
    World b = *loaded_extended.world;
    REQUIRE(a.locations.size() == 5);
    REQUIRE(b.locations.size() == 46);
    REQUIRE(b.locations_with_data() == 5);
    // Viele Seuchen, damit der Ereignisstrom stark genutzt wird.
    a.rules.plague_chance = Fixed::from_ratio(1, 2);
    b.rules.plague_chance = Fixed::from_ratio(1, 2);

    Simulation sa(std::move(a));
    Simulation sb(std::move(b));
    sa.submit(set_tax(1, Fixed::from_ratio(1, 5)));
    sb.submit(set_tax(1, Fixed::from_ratio(1, 5)));
    for (int year = 0; year < 30; ++year) {
        sa.step_days(365);
        sb.step_days(365);
    }
    const World& wa = sa.world();
    const World& wb = sb.world();
    REQUIRE(wa.date == wb.date);

    // Bevölkerung Block für Block gleich.
    REQUIRE(wa.pops.size() == wb.pops.size());
    for (std::size_t p = 0; p < wa.pops.size(); ++p) {
        INFO("Block " << p);
        CHECK(wa.pops[p].size() == wb.pops[p].size());
        CHECK(wa.pops[p].location == wb.pops[p].location);
    }
    // Kassen und Monatswerte der Staaten gleich.
    REQUIRE(wa.states.size() == wb.states.size());
    for (std::size_t s = 0; s < wa.states.size(); ++s) {
        INFO("Staat " << wa.states[s].tag);
        CHECK(wa.states[s].treasury == wb.states[s].treasury);
        CHECK(wa.states[s].last_month_income == wb.states[s].last_month_income);
        CHECK(wa.states[s].last_month_expenses == wb.states[s].last_month_expenses);
        CHECK(wa.states[s].tax_rate(TaxKind::Kontribution) == wb.states[s].tax_rate(TaxKind::Kontribution));
        CHECK(wa.state_population(static_cast<Index>(s)) == wb.state_population(static_cast<Index>(s)));
    }
    // Verwüstung der Orte mit Daten gleich; Orte ohne Daten unverändert.
    for (std::size_t i = 0; i < wa.locations.size(); ++i) {
        CHECK(wa.locations[i].devastation == wb.locations[i].devastation);
    }
    for (std::size_t i = wa.locations.size(); i < wb.locations.size(); ++i) {
        const uhrwerk::Location& loc = wb.locations[i];
        CHECK_FALSE(loc.has_data());
        CHECK(loc.owner == kNone);
        CHECK(loc.devastation == Fixed::zero());
        CHECK(sb.location_economy(static_cast<Index>(i)).production == Fixed::zero());
    }
    // Gleiche Chronik: gleiche Seuchen an gleichen Orten (Orte ohne Daten ziehen keine Zufallszahlen).
    REQUIRE(wa.chronicle.size() == wb.chronicle.size());
    CHECK(wa.chronicle.size() > 30);
    for (std::size_t k = 0; k < wa.chronicle.size(); ++k) {
        CHECK(wa.chronicle[k].date == wb.chronicle[k].date);
        CHECK(wa.chronicle[k].text == wb.chronicle[k].text);
        CHECK(wa.chronicle[k].location == wb.chronicle[k].location);
        CHECK(wa.chronicle[k].state == wb.chronicle[k].state);
    }
}

// ─── Bevölkerung (seit M4, docs/technik/bevoelkerung.md) ─────────────────────

TEST_CASE("Simulation: Buchführung der Bevölkerung ist exakt") {
    // Seuchen, Wanderung und Auswanderung eingeschaltet; über 30 Jahre muss jede Person
    // nachgewiesen sein: Änderung = Geburten − Sterbefälle − Seuchentote − Auswanderung (bitgenau),
    // und jeder Wegzug ist irgendwo ein Zuzug.
    World w = testhilfen::load_fixture();
    w.rules.plague_chance = Fixed::from_ratio(1, 2);
    w.rules.migration_rate = Fixed::from_int(3); // viel Wanderung
    Simulation sim(std::move(w));
    const Fixed start = total_population(sim.world());

    uhrwerk::PopulationBalance sum;
    const auto add = [&](const uhrwerk::PopulationBalance& b) {
        sum.births += b.births;
        sum.deaths += b.deaths;
        sum.plague_deaths += b.plague_deaths;
        sum.moved_in += b.moved_in;
        sum.moved_out += b.moved_out;
        sum.emigrated += b.emigrated;
    };
    for (int day = 0; day < 30 * 365; ++day) {
        const Date& d = sim.world().date;
        if (d.month == 1 && d.day == 1) {
            add(sum_balance(sim.world().balance_current)); // wird heute zum Vorjahr
        }
        sim.step_day();
    }
    add(sum_balance(sim.world().balance_current));

    const Fixed end = total_population(sim.world());
    CHECK(end - start == sum.births - sum.deaths - sum.plague_deaths - sum.emigrated);
    CHECK(sum.moved_in == sum.moved_out);
    CHECK(sum.moved_out > Fixed::from_int(100)); // es wurde tatsächlich gewandert
    CHECK(sum.plague_deaths > Fixed::zero());
    CHECK(sum.births > Fixed::zero()); // Plausibilität der Raten prüft der Kalibrierungstest (echte Daten)
    CHECK(sum.deaths > Fixed::zero());

    // Bilanz des Vorjahres: nach dem Jahresabschluss stimmt sie mit der Summe des letzten Jahres
    // überein (Stichprobe: der Jahresabschluss verschiebt, statt zu verlieren).
    CHECK(sum_balance(sim.world().balance_previous).births > Fixed::zero());
}

TEST_CASE("Simulation: Ergebnis unabhängig von der Zahl der Threads") {
    World w = many_locations(120); // 600 Orte mit Daten, etwa 21 je Rechentag
    w.rules.plague_chance = Fixed::from_ratio(1, 10);
    REQUIRE(w.locations.size() == 600);

    std::vector<Simulation> runs;
    for (const unsigned threads : {1u, 2u, 3u, 4u, 8u}) {
        runs.emplace_back(w, SimConfig{1648, threads});
        CHECK(runs.back().threads() == threads);
    }
    const std::uint64_t start = runs[0].checksum();
    for (const Simulation& run : runs) {
        CHECK(run.checksum() == start);
    }
    for (int year = 0; year < 3; ++year) {
        for (Simulation& run : runs) {
            run.step_days(365);
        }
        INFO("Jahr " << year + 1);
        for (std::size_t r = 1; r < runs.size(); ++r) {
            CHECK(runs[r].checksum() == runs[0].checksum());
        }
    }
    CHECK(runs[0].checksum() != start);
    // Es wurde gewandert, auch zwischen Orten, die am selben Tag rechnen.
    CHECK(sum_balance(runs[0].world().balance_current).moved_in > Fixed::zero());

    // Eine Kopie bekommt eigene Threads und rechnet gleich weiter.
    Simulation twin = runs[3];
    twin.step_days(40);
    runs[3].step_days(40);
    CHECK(twin.checksum() == runs[3].checksum());
}

TEST_CASE("Simulation: Wanderungsziele (Abschnitt 4.5)") {
    const World w = testhilfen::load_fixture();
    Simulation sim(w);
    // Potsdam (Land) liegt 26 km von Berlin-Cölln (Stadt): Ziel im Umkreis von 60 km.
    const std::vector<Index>& potsdam = sim.migration_targets(1);
    REQUIRE_FALSE(potsdam.empty());
    CHECK(potsdam[0] == 0);
    // Dessau (Land) hat Leipzig (55 km) im Umkreis; die nächste Stadt ist ohnehin dabei.
    const std::vector<Index>& dessau = sim.migration_targets(4);
    CHECK(std::find(dessau.begin(), dessau.end(), 3) != dessau.end());
    // Dresden (Stadt) hat im Umkreis von 60 km keinen Ort mit Daten.
    CHECK(sim.migration_targets(2).empty());
    // Orte ohne Daten haben keine Ziele, ungültige Indizes auch nicht.
    CHECK(sim.migration_targets(5).empty());
    CHECK(sim.migration_targets(-1).empty());

    // Ein ländlicher Ort bekommt die nächste Stadt auch außerhalb des Umkreises.
    World far = w;
    far.rules.migration_radius_km = Fixed::from_int(1);
    Simulation sim_far(std::move(far));
    CHECK(sim_far.migration_targets(1) == std::vector<Index>{0}); // Potsdam → Berlin-Cölln
    CHECK(sim_far.migration_targets(0).empty());                  // Städte nicht

    World none = w;
    none.rules.migration_targets = 0;
    CHECK(Simulation(std::move(none)).migration_targets(1).empty());
}

TEST_CASE("Simulation: Wanderung in einen besseren Ort und neue Blöcke durch Zuzug") {
    // Zwei benachbarte Orte desselben Staates: arm (0, Land) und reich (1, Stadt, ohne Verwüstung).
    World w = make_small_world();
    w.date = Date{1650, 2, 27};
    w.rules.migration_targets = 4;
    w.rules.migration_radius_km = Fixed::from_int(60);
    w.rules.migration_rate = Fixed::one();
    w.rules.migration_max = Fixed::one();
    w.rules.migration_foreign = Fixed::from_ratio(1, 2);
    w.rules.living_standard_reference = Fixed::from_int(12);
    w.rules.block_min_size = Fixed::one();
    w.rules.migration_min_amount = Fixed::from_ratio(1, 100);
    w.terrains[0].capacity = Fixed::from_int(1000);
    w.pops.pop_back(); // nur Bauern im armen Ort
    w.pops[0].young = Fixed::from_int(300);
    w.locations[0].pop_count = 1;
    w.locations[0].area_km2 = Fixed::from_int(100);
    w.locations[0].longitude = Fixed::from_int(13);
    w.locations[0].latitude = Fixed::from_int(52);
    uhrwerk::Location rich = w.locations[0];
    rich.id = "reich";
    rich.is_city = true;
    rich.devastation = Fixed::zero();
    rich.longitude += Fixed::from_ratio(1, 10);
    rich.first_pop = 1;
    rich.pop_count = 1;
    w.locations.push_back(rich);
    uhrwerk::PopBlock noble = w.pops[0];
    noble.location = 1;
    noble.profession = 1; // Adel: dort gibt es keinen Bauernblock
    w.pops.push_back(noble);
    // Die Konfession der Zuziehenden gibt es im Zielort auch nicht.
    w.confessions.push_back(w.confessions[0]);
    w.confessions[1].id = "ref";
    w.pops[1].confession = 1;
    initialize_population(w);

    Simulation sim(std::move(w));
    CHECK(sim.migration_targets(0) == std::vector<Index>{1});
    step_until(sim, Date{1650, 3, 2}); // Ort 0 rechnet am 1.3.
    const uhrwerk::PopulationBalance& out = sim.world().balance_current[0];
    const uhrwerk::PopulationBalance& in = sim.world().balance_current[1];
    const Fixed first_move = out.moved_out;
    CHECK(first_move > Fixed::one());
    CHECK(in.moved_in == first_move);
    // Für Bauern-Lutheraner gibt es im reichen Ort keinen Block: ein Zugang.
    REQUIRE(sim.world().arrivals[1].size() == 1);
    CHECK(sim.world().arrivals[1][0].location == 1);
    CHECK(sim.world().arrivals[1][0].young == first_move);
    CHECK(sim.world().arrivals[0].empty());
    CHECK(sim.world().location_population(1) == sim.world().pops[1].size() + out.moved_out);

    // Am nächsten Monatsersten wird der Zugang ein Block des reichen Ortes.
    const Fixed population = sim.world().location_population(1);
    step_until(sim, Date{1650, 4, 1});
    sim.step_day(); // 1.4.: Neuordnung (und Ort 0 rechnet wieder)
    CHECK(sim.world().locations[1].pop_count == 2);
    CHECK(sim.world().arrivals[1].empty());
    const uhrwerk::PopBlock& created =
        sim.world().pops[static_cast<std::size_t>(sim.world().locations[1].first_pop + 1)];
    CHECK(created.profession == 0);
    CHECK(created.confession == 0);
    CHECK(created.young >= first_move); // dazu der Zuzug vom 1.4., direkt in den neuen Block
    CHECK(sim.world().location_population(1) > population);
}

TEST_CASE("Simulation: Kalibrierung mit den echten Daten (1648 bis 1700)") {
    const uhrwerk::LoadResult loaded = uhrwerk::load_world(UHRWERK_GAME_DATA_DIR);
    INFO(testhilfen::issues_text(loaded));
    REQUIRE(loaded.ok());
    Simulation sim(*loaded.world);
    const Fixed start = total_population(sim.world());
    step_until(sim, Date{1700, 10, 24});
    const World& w = sim.world();
    const Fixed end = total_population(w);
    // Ziel aus Kapitel 07, Abschnitt 7.3: rund ×1,32 (11 → 14,5 Mio.).
    CHECK(end > start * *Fixed::parse("1.20"));
    CHECK(end < start * *Fixed::parse("1.45"));
    Fixed children;
    Fixed all;
    for (const uhrwerk::PopBlock& p : w.pops) {
        CHECK(p.children >= Fixed::zero());
        CHECK(p.young >= Fixed::zero());
        CHECK(p.working >= Fixed::zero());
        CHECK(p.old >= Fixed::zero());
        CHECK(p.living_standard >= Fixed::zero());
        CHECK(p.living_standard <= Fixed::from_int(100));
        CHECK(p.literacy >= Fixed::zero());
        CHECK(p.literacy <= Fixed::one());
        children += p.children;
        all += p.size();
    }
    CHECK(children > all * *Fixed::parse("0.30"));
    CHECK(children < all * *Fixed::parse("0.40"));
    // Geburten- und Sterberate des letzten vollen Jahres in der Größenordnung der Frühen Neuzeit.
    const uhrwerk::PopulationBalance year = sum_balance(w.balance_previous);
    CHECK(year.births > end * *Fixed::parse("0.030"));
    CHECK(year.births < end * *Fixed::parse("0.045"));
    CHECK(year.deaths > end * *Fixed::parse("0.025"));
    CHECK(year.deaths < end * *Fixed::parse("0.040"));
}
