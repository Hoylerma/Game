#include "uhrwerk/balance.hpp"

#include "target_reader.hpp"
#include "uhrwerk/economy.hpp"
#include "uhrwerk/script.hpp"

#include <algorithm>
#include <initializer_list>

namespace uhrwerk {

namespace {

std::size_t at(Index index) {
    return static_cast<std::size_t>(index);
}

const ValueRange kShare{Fixed::zero(), Fixed::one()};

} // namespace

namespace {

BalanceTargetsResult targets_from(const ParseResult& parsed, const World& world,
                                  const std::string& file_name) {
    BalanceTargetsResult result;
    for (const ParseError& e : parsed.errors) {
        result.errors.push_back(e.to_string());
    }
    if (!parsed.ok()) {
        return result;
    }
    TargetReader r{file_name, {}};
    const ScriptNode& root = parsed.root;
    r.only(root, {"vermoegen", "bankrotte", "steuerquote", "preise", "bevoelkerung", "lebensstandard"});

    BalanceTargets t;
    if (const ScriptNode* b = r.block(root, "vermoegen")) {
        r.only(*b, {"bereich", "anteil", "ende"});
        t.wealth = r.range(*b, "bereich").value_or(ValueRange{});
        t.wealth_share = r.number(*b, "anteil", kShare).value_or(Fixed{});
        t.wealth_end = r.range(*b, "ende").value_or(ValueRange{});
    }
    if (const ScriptNode* b = r.block(root, "bankrotte")) {
        r.only(*b, {"median", "p95"});
        const ValueRange count{Fixed::zero(), Fixed::from_int(100)};
        t.bankruptcies_median = r.number(*b, "median", count).value_or(Fixed{});
        t.bankruptcies_p95 = r.number(*b, "p95", count).value_or(Fixed{});
    }
    if (const ScriptNode* b = r.block(root, "steuerquote")) {
        r.only(*b, {"bereich"});
        t.tax_ratio = r.range(*b, "bereich").value_or(ValueRange{});
    }
    if (const ScriptNode* b = r.block(root, "preise")) {
        r.only(*b, {"bereich", "anteil", "ausnahme"});
        t.price = r.range(*b, "bereich").value_or(ValueRange{});
        t.price_share = r.number(*b, "anteil", kShare).value_or(Fixed{});
        t.price_by_good.assign(world.goods.size(), t.price);
        for (const ScriptNode* e : b->find_all("ausnahme")) {
            const Index good = world.find_good(e->value);
            if (!e->is_block || e->value.empty()) {
                r.error(*e, "'ausnahme' erwartet: ausnahme <gut> { bereich = { von bis } }");
            } else if (good == kNone) {
                r.error(*e, "unbekanntes Gut '" + e->value + "'");
            } else {
                r.only(*e, {"bereich"});
                if (const std::optional<ValueRange> range = r.range(*e, "bereich")) {
                    t.price_by_good[at(good)] = *range;
                }
            }
        }
    }
    if (const ScriptNode* b = r.block(root, "bevoelkerung")) {
        r.only(*b, {"jahr", "faktor"});
        const std::optional<Fixed> year =
            r.number(*b, "jahr", ValueRange{Fixed::one(), Fixed::from_int(9999)});
        t.population_year = year ? static_cast<std::int32_t>(year->trunc()) : 0;
        t.population = r.range(*b, "faktor").value_or(ValueRange{});
    }
    if (const ScriptNode* b = r.block(root, "lebensstandard")) {
        r.only(*b, {"unten", "mittel", "oben"});
        t.living_standard[0] = r.range(*b, "unten").value_or(ValueRange{});
        t.living_standard[1] = r.range(*b, "mittel").value_or(ValueRange{});
        t.living_standard[2] = r.range(*b, "oben").value_or(ValueRange{});
    }
    result.errors.insert(result.errors.end(), r.errors.begin(), r.errors.end());
    if (result.errors.empty()) {
        result.targets = t;
    }
    return result;
}

} // namespace

BalanceTargetsResult parse_balance_targets(std::string_view text, const World& world,
                                           const std::string& file_name) {
    return targets_from(parse_script(text, file_name), world, file_name);
}

BalanceTargetsResult load_balance_targets(const std::filesystem::path& file, const World& world) {
    return targets_from(parse_script_file(file), world, path_to_utf8(file));
}

// ─── Messung eines Laufs ────────────────────────────────────────────────────

namespace {

Date add_years(Date date, std::int64_t years) {
    date.year = static_cast<std::int32_t>(date.year + years);
    if (date.month == 2 && date.day == 29 && !is_leap_year(date.year)) {
        date.day = 28;
    }
    return date;
}

/// Steuern ÷ Wirtschaftsleistung der Orte eines Staates (letzter Rechentag); leer ohne Leistung.
std::optional<Fixed> tax_share(const World& world, Index state) {
    Fixed taxes;
    Fixed production;
    for (std::size_t i = 0; i < world.locations.size() && i < world.economy.size(); ++i) {
        if (world.locations[i].owner == state && world.locations[i].pop_count > 0) {
            taxes += world.economy[i].total_taxes();
            production += world.economy[i].production;
        }
    }
    if (production <= Fixed::zero()) {
        return std::nullopt;
    }
    return taxes / production;
}

Fixed total_population(const World& world) {
    Fixed total;
    for (std::size_t i = 0; i < world.locations.size(); ++i) {
        total += world.location_population(static_cast<Index>(i));
    }
    return total;
}

void sample_prices(const World& world, const std::vector<bool>& active, const BalanceTargets& targets,
                   BalanceRun& run) {
    for (std::size_t m = 0; m < world.market_states.size() && m < active.size(); ++m) {
        if (!active[m]) {
            continue;
        }
        const MarketState& market = world.market_states[m];
        for (std::size_t g = 0; g < world.goods.size() && g < market.price.size(); ++g) {
            const Fixed base = world.goods[g].base_price;
            if (base <= Fixed::zero()) {
                continue;
            }
            const Fixed factor = market.price[g] / base;
            const ValueRange& range =
                g < targets.price_by_good.size() ? targets.price_by_good[g] : targets.price;
            if (run.price_months[g] == 0) {
                run.price_min[g] = factor;
                run.price_max[g] = factor;
            }
            run.price_min[g] = Fixed::min(run.price_min[g], factor);
            run.price_max[g] = Fixed::max(run.price_max[g], factor);
            ++run.price_months[g];
            if (range.contains(factor)) {
                ++run.price_in_range[g];
            }
        }
    }
}

void sample_year(const World& world, Fixed population_start, const BalanceTargets& targets, BalanceRun& run) {
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        const State& state = world.states[s];
        if (state.yearly_income > Fixed::zero()) {
            run.wealth[s].push_back((state.treasury - state.debt) / state.yearly_income);
        }
    }
    if (world.date.year == targets.population_year && !run.population_factor &&
        population_start > Fixed::zero()) {
        run.population_factor = total_population(world) / population_start;
    }
    std::array<Fixed, 3> weighted{};
    std::array<Fixed, 3> people{};
    for (const PopBlock& block : world.pops) {
        if (!(block.profession >= 0 && at(block.profession) < world.professions.size())) {
            continue;
        }
        const auto stratum = static_cast<std::size_t>(world.professions[at(block.profession)].stratum);
        const Fixed size = block.size();
        weighted[stratum] += block.living_standard * size;
        people[stratum] += size;
    }
    for (std::size_t k = 0; k < 3; ++k) {
        if (people[k] > Fixed::zero()) {
            run.living_standard[k].push_back(weighted[k] / people[k]);
        }
    }
}

} // namespace

BalanceRun record_run(Simulation& sim, std::int64_t years, const BalanceTargets& targets) {
    const World& start_world = sim.world();
    const std::size_t states = start_world.states.size();
    const std::size_t goods = start_world.goods.size();
    BalanceRun run;
    run.years = years;
    run.wealth.assign(states, {});
    run.bankruptcies.assign(states, 0);
    run.tax_ratio.assign(states, std::nullopt);
    run.price_months.assign(goods, 0);
    run.price_in_range.assign(goods, 0);
    run.price_min.assign(goods, Fixed::zero());
    run.price_max.assign(goods, Fixed::zero());

    const EconomyTopology topology = build_economy_topology(start_world);
    std::vector<bool> active(start_world.markets.size(), false);
    for (std::size_t m = 0; m < active.size(); ++m) {
        active[m] = topology.active(static_cast<Index>(m));
    }
    std::vector<std::int32_t> bankruptcies_start(states);
    std::vector<std::optional<Fixed>> share_start(states);
    for (std::size_t s = 0; s < states; ++s) {
        bankruptcies_start[s] = start_world.states[s].bankruptcies;
        share_start[s] = tax_share(start_world, static_cast<Index>(s));
    }
    const Fixed population_start = total_population(start_world);
    const Date start = start_world.date;

    std::int64_t year = 0;
    Date next = add_years(start, 1);
    while (year < years) {
        const bool first_of_month = sim.world().date.day == 1;
        sim.step_day();
        const World& now = sim.world();
        if (first_of_month) {
            sample_prices(now, active, targets, run); // Preise des neuen Monats
        }
        if (now.date == next) {
            ++year;
            sample_year(now, population_start, targets, run);
            next = add_years(start, year + 1);
        }
    }

    const World& end_world = sim.world();
    for (std::size_t s = 0; s < states; ++s) {
        run.bankruptcies[s] = end_world.states[s].bankruptcies - bankruptcies_start[s];
        const std::optional<Fixed> share_end = tax_share(end_world, static_cast<Index>(s));
        if (share_start[s] && share_end && *share_start[s] > Fixed::zero()) {
            run.tax_ratio[s] = *share_end / *share_start[s];
        }
    }
    return run;
}

// ─── Auswertung ─────────────────────────────────────────────────────────────

Fixed quantile(std::vector<Fixed> values, std::int64_t num, std::int64_t den) {
    if (values.empty() || den <= 0) {
        return Fixed::zero();
    }
    std::sort(values.begin(), values.end());
    const auto n = static_cast<std::int64_t>(values.size());
    std::int64_t rank = (num * n + den - 1) / den; // ⌈p·n⌉
    rank = std::clamp<std::int64_t>(rank, 1, n);
    return values[static_cast<std::size_t>(rank - 1)];
}

Quantiles quantiles(const std::vector<Fixed>& values) {
    return Quantiles{quantile(values, 1, 20), quantile(values, 1, 2), quantile(values, 19, 20)};
}

BalanceSummary summarize(const World& world, const std::vector<BalanceRun>& runs,
                         const BalanceTargets& targets) {
    BalanceSummary out;
    out.runs = static_cast<std::int64_t>(runs.size());
    out.years = runs.empty() ? 0 : runs.front().years;
    const auto violate = [&out](BalanceCheck check, Index subject, Fixed value) {
        out.violations.push_back(BalanceViolation{check, subject, value});
    };

    for (std::size_t s = 0; s < world.states.size(); ++s) {
        const auto subject = static_cast<Index>(s);
        BalanceStateSummary st;
        std::vector<Fixed> all;
        std::vector<Fixed> end;
        std::vector<Fixed> bankruptcies;
        std::vector<Fixed> ratios;
        std::int64_t in_range = 0;
        for (const BalanceRun& run : runs) {
            if (s < run.wealth.size()) {
                for (const Fixed w : run.wealth[s]) {
                    all.push_back(w);
                    in_range += targets.wealth.contains(w) ? 1 : 0;
                }
                if (!run.wealth[s].empty()) {
                    end.push_back(run.wealth[s].back());
                }
            }
            if (s < run.bankruptcies.size() && run.years > 0) {
                bankruptcies.push_back(Fixed::from_int(std::int64_t{run.bankruptcies[s]} * 100) /
                                       Fixed::from_int(run.years));
            }
            if (s < run.tax_ratio.size() && run.tax_ratio[s]) {
                ratios.push_back(*run.tax_ratio[s]);
            }
        }
        st.wealth = quantiles(all);
        st.wealth_share =
            all.empty() ? Fixed::one()
                        : Fixed::from_int(in_range) / Fixed::from_int(static_cast<std::int64_t>(all.size()));
        st.wealth_min = all.empty() ? Fixed::zero() : *std::min_element(all.begin(), all.end());
        st.wealth_end = quantiles(end);
        st.bankruptcies = quantiles(bankruptcies);
        if (!ratios.empty()) {
            st.tax_ratio = quantiles(ratios);
        }

        if (!all.empty() && st.wealth_share < targets.wealth_share) {
            violate(BalanceCheck::WealthShare, subject, st.wealth_share);
        }
        if (!end.empty() && !targets.wealth_end.contains(st.wealth_end.median)) {
            violate(BalanceCheck::WealthEnd, subject, st.wealth_end.median);
        }
        if (!bankruptcies.empty() && st.bankruptcies.median > targets.bankruptcies_median) {
            violate(BalanceCheck::BankruptciesMedian, subject, st.bankruptcies.median);
        }
        if (!bankruptcies.empty() && st.bankruptcies.p95 > targets.bankruptcies_p95) {
            violate(BalanceCheck::BankruptciesP95, subject, st.bankruptcies.p95);
        }
        if (st.tax_ratio && !targets.tax_ratio.contains(st.tax_ratio->median)) {
            violate(BalanceCheck::TaxRatio, subject, st.tax_ratio->median);
        }
        out.states.push_back(st);
    }

    for (std::size_t g = 0; g < world.goods.size(); ++g) {
        BalanceGoodSummary gs;
        std::int64_t in_range = 0;
        bool first = true;
        for (const BalanceRun& run : runs) {
            if (g >= run.price_months.size() || run.price_months[g] == 0) {
                continue;
            }
            gs.months += run.price_months[g];
            in_range += run.price_in_range[g];
            gs.min = first ? run.price_min[g] : Fixed::min(gs.min, run.price_min[g]);
            gs.max = first ? run.price_max[g] : Fixed::max(gs.max, run.price_max[g]);
            first = false;
        }
        gs.share = gs.months > 0 ? Fixed::from_int(in_range) / Fixed::from_int(gs.months) : Fixed::one();
        if (gs.months > 0 && gs.share < targets.price_share) {
            violate(BalanceCheck::PriceShare, static_cast<Index>(g), gs.share);
        }
        out.goods.push_back(gs);
    }

    std::vector<Fixed> population;
    for (const BalanceRun& run : runs) {
        if (run.population_factor) {
            population.push_back(*run.population_factor);
        }
    }
    if (!population.empty()) {
        out.population = quantiles(population);
        if (!targets.population.contains(out.population->median)) {
            violate(BalanceCheck::Population, kNone, out.population->median);
        }
    }

    for (std::size_t k = 0; k < 3; ++k) {
        std::vector<Fixed> values;
        for (const BalanceRun& run : runs) {
            values.insert(values.end(), run.living_standard[k].begin(), run.living_standard[k].end());
        }
        if (!values.empty()) {
            out.living_standard[k] = quantiles(values);
            if (!targets.living_standard[k].contains(out.living_standard[k]->median)) {
                violate(BalanceCheck::LivingStandard, static_cast<Index>(k), out.living_standard[k]->median);
            }
        }
    }
    return out;
}

} // namespace uhrwerk
