// Tests für den Balancing-Bericht (seit M5): Zielwerte, Messung eines Laufs, Auswertung.

#include "test_hilfen.hpp"
#include "uhrwerk/balance.hpp"
#include "uhrwerk/simulation.hpp"

#include <doctest/doctest.h>
#include <string>
#include <vector>

using uhrwerk::BalanceCheck;
using uhrwerk::BalanceRun;
using uhrwerk::BalanceTargets;
using uhrwerk::Date;
using uhrwerk::Fixed;
using uhrwerk::Index;
using uhrwerk::SimConfig;
using uhrwerk::Simulation;
using uhrwerk::World;

namespace {

Fixed fx(const char* text) {
    const auto value = Fixed::parse(text);
    REQUIRE(value.has_value());
    return *value;
}

const char* const kTargets = R"(
vermoegen {
    bereich = { -3 3 }
    anteil  = 0.95
    ende    = { 0 1.5 }
}
bankrotte {
    median = 0
    p95    = 2
}
steuerquote {
    bereich = { 0.6 1.4 }
}
preise {
    bereich = { 0.4 2.5 }
    anteil  = 0.95
    ausnahme kolonialwaren {
        bereich = { 0.4 5 }
    }
}
bevoelkerung {
    jahr   = 1700
    faktor = { 1.2 1.45 }
}
lebensstandard {
    unten  = { 25 55 }
    mittel = { 45 75 }
    oben   = { 70 95 }
}
)";

BalanceTargets parse_targets(const World& world, const std::string& text = kTargets) {
    const uhrwerk::BalanceTargetsResult result = uhrwerk::parse_balance_targets(text, world, "ziele.chron");
    for (const std::string& error : result.errors) {
        INFO(error);
    }
    REQUIRE(result.errors.empty());
    REQUIRE(result.targets.has_value());
    return *result.targets;
}

/// Fehlermeldungen für einen veränderten Zielwerte-Text (ersetzt from durch to).
std::vector<std::string> errors_for(const World& world, const std::string& from, const std::string& to) {
    std::string text = kTargets;
    const std::size_t pos = text.find(from);
    REQUIRE(pos != std::string::npos);
    text.replace(pos, from.size(), to);
    const uhrwerk::BalanceTargetsResult result = uhrwerk::parse_balance_targets(text, world, "ziele.chron");
    CHECK_FALSE(result.targets.has_value());
    return result.errors;
}

bool contains(const std::vector<std::string>& errors, const std::string& part) {
    for (const std::string& e : errors) {
        if (e.find(part) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::vector<Fixed> ints(std::initializer_list<int> values) {
    std::vector<Fixed> result;
    for (const int v : values) {
        result.push_back(Fixed::from_int(v));
    }
    return result;
}

} // namespace

TEST_CASE("Bilanz: Quantile nach dem nächsten Rang") {
    CHECK(uhrwerk::quantile({}, 1, 2) == Fixed::zero());
    CHECK(uhrwerk::quantile(ints({7}), 19, 20) == Fixed::from_int(7));
    std::vector<Fixed> values;
    for (int i = 20; i >= 1; --i) {
        values.push_back(Fixed::from_int(i)); // absteigend: quantile sortiert selbst
    }
    const uhrwerk::Quantiles q = uhrwerk::quantiles(values);
    CHECK(q.p5 == Fixed::from_int(1));      // ⌈0,05 × 20⌉ = 1
    CHECK(q.median == Fixed::from_int(10)); // ⌈0,5 × 20⌉ = 10 (unterer Median)
    CHECK(q.p95 == Fixed::from_int(19));    // ⌈0,95 × 20⌉ = 19
    CHECK(uhrwerk::quantile(ints({3, 1, 2}), 1, 2) == Fixed::from_int(2));
    CHECK(uhrwerk::quantile(ints({3, 1, 2}), 1, 1) == Fixed::from_int(3));
    CHECK(uhrwerk::quantile(ints({3, 1, 2}), 0, 1) == Fixed::from_int(1));
}

TEST_CASE("Bilanz: Zielwerte lesen") {
    const World w = testhilfen::load_fixture();
    const BalanceTargets t = parse_targets(w);
    CHECK(t.wealth.min == Fixed::from_int(-3));
    CHECK(t.wealth.max == Fixed::from_int(3));
    CHECK(t.wealth_share == fx("0.95"));
    CHECK(t.wealth_end.max == fx("1.5"));
    CHECK(t.bankruptcies_median == Fixed::zero());
    CHECK(t.bankruptcies_p95 == Fixed::from_int(2));
    CHECK(t.tax_ratio.min == fx("0.6"));
    CHECK(t.population_year == 1700);
    CHECK(t.population.min == fx("1.2"));
    CHECK(t.living_standard[2].min == Fixed::from_int(70));
    REQUIRE(t.price_by_good.size() == w.goods.size());
    const Index kolonial = w.find_good("kolonialwaren");
    const Index getreide = w.find_good("getreide");
    REQUIRE(kolonial != uhrwerk::kNone);
    CHECK(t.price_by_good[static_cast<std::size_t>(kolonial)].max == Fixed::from_int(5));
    CHECK(t.price_by_good[static_cast<std::size_t>(getreide)].max == fx("2.5"));
    CHECK(t.wealth.contains(Fixed::from_int(-3)));
    CHECK_FALSE(t.wealth.contains(fx("3.01")));

    SUBCASE("Datei der Fixture") {
        const uhrwerk::BalanceTargetsResult r =
            uhrwerk::load_balance_targets(testhilfen::fixture_dir() / "common" / "zielwerte.chron", w);
        CHECK(r.errors.empty());
        REQUIRE(r.targets.has_value());
        CHECK(r.targets->wealth.min == Fixed::from_int(-4));
    }
}

TEST_CASE("Bilanz: Fehler in den Zielwerten") {
    const World w = testhilfen::load_fixture();
    CHECK(contains(errors_for(w, "bereich = { -3 3 }", "bereich = { 3 -3 }"), "ziele.chron:3:"));
    CHECK(contains(errors_for(w, "bereich = { -3 3 }", "bereich = { -3 }"), "Bereich { von bis }"));
    CHECK(contains(errors_for(w, "anteil  = 0.95\n    ende", "anteil  = 1.5\n    ende"), "'anteil'"));
    CHECK(contains(errors_for(w, "median = 0", "mittel = 0"), "unbekanntes Feld 'mittel'"));
    CHECK(contains(errors_for(w, "median = 0", "median = 0 p50 = 1"), "unbekanntes Feld 'p50'"));
    CHECK(contains(errors_for(w, "ausnahme kolonialwaren", "ausnahme gold"), "unbekanntes Gut 'gold'"));
    CHECK(contains(errors_for(w, "steuerquote {\n    bereich = { 0.6 1.4 }\n}", ""),
                   "Block 'steuerquote' fehlt"));
    CHECK(contains(errors_for(w, "jahr   = 1700", "jahr   = abc"), "'jahr'"));
    CHECK(contains(errors_for(w, "lebensstandard {", "lebenstandard {"), "unbekanntes Feld 'lebenstandard'"));
    CHECK(contains(errors_for(w, "oben   = { 70 95 }", "oben   = { 70 95 "), "ziele.chron:"));
    const uhrwerk::BalanceTargetsResult missing =
        uhrwerk::load_balance_targets(testhilfen::fixture_dir() / "gibt_es_nicht.chron", w);
    CHECK_FALSE(missing.targets.has_value());
    CHECK_FALSE(missing.errors.empty());
}

TEST_CASE("Bilanz: Messung eines Laufs") {
    World w = testhilfen::load_fixture();
    w.rules.plague_chance = Fixed::zero();
    const BalanceTargets t = parse_targets(w);
    const Date start = w.date;

    Simulation sim(w, SimConfig{1648, 2});
    const BalanceRun run = uhrwerk::record_run(sim, 3, t);
    CHECK(sim.world().date == Date{start.year + 3, start.month, start.day});
    CHECK(run.years == 3);
    REQUIRE(run.wealth.size() == w.states.size());
    REQUIRE(run.bankruptcies.size() == w.states.size());
    for (std::size_t s = 0; s < w.states.size(); ++s) {
        CHECK(run.wealth[s].size() == 3);
        CHECK(run.bankruptcies[s] == 0);
        REQUIRE(run.tax_ratio[s].has_value());
        CHECK(*run.tax_ratio[s] > fx("0.5"));
        CHECK(*run.tax_ratio[s] < Fixed::from_int(2));
    }
    for (std::size_t k = 0; k < 3; ++k) {
        CHECK(run.living_standard[k].size() == 3);
    }
    CHECK_FALSE(run.population_factor.has_value()); // 1700 liegt nach dem Lauf

    // Preise: jeder Monat jedes Marktes mit Orten, für jedes Gut einmal (36 Monate).
    REQUIRE(run.price_months.size() == w.goods.size());
    CHECK(run.price_months[0] > 0);
    CHECK(run.price_months[0] % 36 == 0);
    for (std::size_t g = 0; g < w.goods.size(); ++g) {
        CHECK(run.price_months[g] == run.price_months[0]);
        CHECK(run.price_in_range[g] <= run.price_months[g]);
        CHECK(run.price_min[g] <= run.price_max[g]);
        CHECK(run.price_min[g] > Fixed::zero());
    }

    // Vermögen am ersten Jahrestag, nachgerechnet mit einer zweiten Simulation.
    Simulation check(w, SimConfig{1648, 1});
    while (check.world().date != Date{start.year + 1, start.month, start.day}) {
        check.step_day();
    }
    for (std::size_t s = 0; s < w.states.size(); ++s) {
        const uhrwerk::State& state = check.world().states[s];
        CHECK(run.wealth[s][0] == (state.treasury - state.debt) / state.yearly_income);
    }

    // Gleicher Seed, andere Zahl von Threads: dieselben Messwerte.
    Simulation again(w, SimConfig{1648, 1});
    const BalanceRun run2 = uhrwerk::record_run(again, 3, t);
    CHECK(run2.wealth == run.wealth);
    CHECK(run2.price_in_range == run.price_in_range);
    CHECK(run2.price_max == run.price_max);
    CHECK(run2.living_standard == run.living_standard);
}

TEST_CASE("Bilanz: Bevölkerungsfaktor im Zieljahr") {
    World w = testhilfen::load_fixture();
    w.rules.plague_chance = Fixed::zero();
    BalanceTargets t = parse_targets(w);
    t.population_year = w.date.year + 2;
    Simulation sim(w, SimConfig{1648, 1});
    const Fixed start = [&w] {
        Fixed total;
        for (std::size_t i = 0; i < w.locations.size(); ++i) {
            total += w.location_population(static_cast<Index>(i));
        }
        return total;
    }();
    const BalanceRun run = uhrwerk::record_run(sim, 2, t);
    Fixed end;
    for (std::size_t i = 0; i < sim.world().locations.size(); ++i) {
        end += sim.world().location_population(static_cast<Index>(i));
    }
    REQUIRE(run.population_factor.has_value());
    CHECK(*run.population_factor == end / start);
}

TEST_CASE("Bilanz: Auswertung und Verstöße") {
    const World w = testhilfen::load_fixture(); // 3 Staaten, 4 Güter
    REQUIRE(w.states.size() == 3);
    const BalanceTargets t = parse_targets(w);
    const std::size_t goods = w.goods.size();

    const auto make_run = [&](std::vector<std::vector<Fixed>> wealth,
                              std::vector<std::int32_t> bankruptcies) {
        BalanceRun run;
        run.years = 100;
        run.wealth = std::move(wealth);
        run.bankruptcies = std::move(bankruptcies);
        run.tax_ratio = {Fixed::one(), fx("0.5"), std::nullopt};
        run.price_months.assign(goods, 100);
        run.price_in_range.assign(goods, 100);
        run.price_min.assign(goods, Fixed::one());
        run.price_max.assign(goods, Fixed::from_int(2));
        run.price_in_range[1] = 90; // Gut 1: nur 90 % im Bereich
        run.population_factor = fx("1.3");
        run.living_standard = {ints({40, 42}), ints({60}), ints({60, 65})};
        return run;
    };
    // Staat 0 in Ordnung; Staat 1 endet mit 2 Jahreseinnahmen und hat Bankrotte; Staat 2 mit −4.
    const std::vector<BalanceRun> runs = {
        make_run({ints({0, 1}), ints({1, 2}), ints({-4, -4})}, {0, 1, 0}),
        make_run({ints({1, 1}), ints({2, 2}), ints({0, 1})}, {0, 3, 0}),
    };
    const uhrwerk::BalanceSummary summary = uhrwerk::summarize(w, runs, t);
    CHECK(summary.runs == 2);
    CHECK(summary.years == 100);
    REQUIRE(summary.states.size() == 3);

    CHECK(summary.states[0].wealth_share == Fixed::one());
    CHECK(summary.states[0].wealth_end.median == Fixed::one());
    CHECK(summary.states[2].wealth_share == Fixed::from_ratio(1, 2)); // −4 zweimal außerhalb
    CHECK(summary.states[2].wealth_min == Fixed::from_int(-4));
    CHECK(summary.states[1].bankruptcies.median == Fixed::one()); // 1 und 3 je Jahrhundert
    CHECK(summary.states[1].bankruptcies.p95 == Fixed::from_int(3));
    CHECK_FALSE(summary.states[2].tax_ratio.has_value());
    CHECK(summary.goods[1].share == fx("0.9"));
    CHECK(summary.goods[1].months == 200);
    REQUIRE(summary.population.has_value());
    CHECK(summary.population->median == fx("1.3"));
    REQUIRE(summary.living_standard[1].has_value());
    CHECK(summary.living_standard[1]->median == Fixed::from_int(60));

    const auto has = [&summary](BalanceCheck check, Index subject) {
        for (const uhrwerk::BalanceViolation& v : summary.violations) {
            if (v.check == check && v.subject == subject) {
                return true;
            }
        }
        return false;
    };
    CHECK(has(BalanceCheck::WealthEnd, 1));          // Median am Ende 2 > 1,5
    CHECK(has(BalanceCheck::BankruptciesMedian, 1)); // 1 > 0
    CHECK(has(BalanceCheck::BankruptciesP95, 1));    // 3 > 2
    CHECK(has(BalanceCheck::TaxRatio, 1));           // 0,5 < 0,6
    CHECK(has(BalanceCheck::WealthShare, 2));        // 50 % < 95 %
    CHECK(has(BalanceCheck::WealthEnd, 2));          // −4 (unterer Median) < 0
    CHECK(has(BalanceCheck::PriceShare, 1));         // 90 % < 95 %
    CHECK_FALSE(has(BalanceCheck::WealthEnd, 0));
    CHECK_FALSE(has(BalanceCheck::PriceShare, 0));
    CHECK_FALSE(has(BalanceCheck::Population, uhrwerk::kNone));
    CHECK(has(BalanceCheck::LivingStandard, 2)); // oben 60 < 70
    CHECK_FALSE(has(BalanceCheck::LivingStandard, 0));
    CHECK(summary.violations.size() == 8);

    // Ohne Läufe: keine Werte, keine Verstöße.
    const uhrwerk::BalanceSummary empty = uhrwerk::summarize(w, {}, t);
    CHECK(empty.violations.empty());
    CHECK_FALSE(empty.population.has_value());
}
