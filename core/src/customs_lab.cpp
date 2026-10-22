// Zolllabor (seit M9, docs/technik/diplomatie.md, Abschnitt 3.6).

#include "uhrwerk/customs_lab.hpp"

#include "target_reader.hpp"
#include "uhrwerk/script.hpp"

#include <algorithm>
#include <utility>

namespace uhrwerk {

namespace {

std::size_t at(Index index) {
    return static_cast<std::size_t>(index);
}

template <typename T> bool in_range(const std::vector<T>& items, Index index) {
    return index >= 0 && static_cast<std::size_t>(index) < items.size();
}

CustomsTargetsResult targets_from(const ParseResult& parsed, const World& world,
                                  const std::string& file_name) {
    CustomsTargetsResult result;
    for (const ParseError& e : parsed.errors) {
        result.errors.push_back(e.to_string());
    }
    if (!parsed.ok()) {
        return result;
    }
    TargetReader r{file_name, {}};
    r.only(parsed.root, {"anteil", "wechsel"});
    for (const ScriptNode* a : parsed.root.find_all("anteil")) {
        if (!a->is_block || a->value.empty()) {
            r.error(*a, "'anteil' erwartet: anteil <zollunion> { zoll = { von bis } }");
            continue;
        }
        const Index customs_union = world.find_customs_union(a->value);
        if (customs_union == kNone) {
            r.error(*a, "unbekannte Zollunion '" + a->value +
                            "' (nicht im Szenario oder nicht in "
                            "common/zollunionen.chron)");
            continue;
        }
        if (std::any_of(result.targets.shares.begin(), result.targets.shares.end(),
                        [&](const CustomsShareTarget& t) { return t.customs_union == customs_union; })) {
            r.error(*a, "Zielwerte für anteil " + a->value + " sind mehrfach angegeben");
            continue;
        }
        r.only(*a, {"zoll"});
        if (const std::optional<ValueRange> share = r.range(*a, "zoll")) {
            if (share->min < Fixed::zero() || share->max > Fixed::one()) {
                r.error(*a->find("zoll"), "'zoll' erwartet einen Bereich innerhalb von 0 bis 1");
            }
            result.targets.shares.push_back(CustomsShareTarget{customs_union, *share});
        }
    }
    for (const ScriptNode* w : parsed.root.find_all("wechsel")) {
        if (!w->is_block || w->value.empty()) {
            r.error(*w, "'wechsel' erwartet: wechsel <kennung> { staat union … }");
            continue;
        }
        if (std::any_of(result.targets.changes.begin(), result.targets.changes.end(),
                        [&](const CustomsChange& c) { return c.id == w->value; })) {
            r.error(*w, "wechsel " + w->value + " ist mehrfach angegeben");
            continue;
        }
        r.only(*w, {"staat", "union", "grenzhandel", "handel", "zoll"});
        CustomsChange change;
        change.id = w->value;
        const ScriptNode* state = w->find("staat");
        const ScriptNode* target = w->find("union");
        if (state == nullptr || target == nullptr || state->is_block || target->is_block) {
            r.error(*w, "wechsel " + w->value + " braucht 'staat' und 'union' (Kennung oder keine)");
            continue;
        }
        change.state = world.find_state(state->value);
        if (change.state == kNone) {
            r.error(*state, "unbekannter Staat '" + state->value + "' (besteht nicht zum Startdatum)");
            continue;
        }
        if (target->value != "keine") {
            change.customs_union = world.find_customs_union(target->value);
            if (change.customs_union == kNone) {
                r.error(*target, "unbekannte Zollunion '" + target->value + "'");
                continue;
            }
        }
        if (world.states[at(change.state)].customs_union == change.customs_union) {
            r.error(*w, "wechsel " + w->value + ": " + state->value + " gehört zum Startdatum schon zu " +
                            target->value);
            continue;
        }
        for (const auto& [key, field] :
             {std::pair{"grenzhandel", &CustomsChange::border_trade},
              std::pair{"handel", &CustomsChange::trade}, std::pair{"zoll", &CustomsChange::revenue}}) {
            if (w->find(key) != nullptr) {
                change.*field = r.range(*w, key);
            }
        }
        result.targets.changes.push_back(std::move(change));
    }
    result.errors.insert(result.errors.end(), r.errors.begin(), r.errors.end());
    if (!result.errors.empty()) {
        result.targets = {};
    }
    return result;
}

/// Gewichte eines Wechsels aus dem Startzustand: je Markt der Anteil des Staates und der Anteil der
/// übrigen Mitglieder der Union an seiner Bevölkerung, dazu die Bevölkerung des Staates je Markt.
struct ChangeWeights {
    std::vector<Fixed> state_share;
    std::vector<Fixed> union_share;
    std::vector<Fixed> state_people;
};

ChangeWeights weights_of(const World& world, const CustomsChange& change) {
    const std::size_t markets = world.markets.size();
    // Beim Austritt zählt die bisherige Union, beim Beitritt die neue (jeweils ohne den Staat).
    const Index other =
        change.customs_union != kNone ? change.customs_union : world.states[at(change.state)].customs_union;
    std::vector<Fixed> total(markets);
    ChangeWeights w;
    w.state_share.assign(markets, Fixed{});
    w.union_share.assign(markets, Fixed{});
    w.state_people.assign(markets, Fixed{});
    for (std::size_t i = 0; i < world.locations.size(); ++i) {
        const Location& loc = world.locations[i];
        if (loc.pop_count == 0 || !in_range(world.markets, loc.market) ||
            !in_range(world.states, loc.owner)) {
            continue;
        }
        const Fixed people = world.location_population(static_cast<Index>(i));
        const std::size_t m = at(loc.market);
        total[m] += people;
        if (loc.owner == change.state) {
            w.state_people[m] += people;
        } else if (other != kNone && world.states[at(loc.owner)].customs_union == other) {
            w.union_share[m] += people;
        }
    }
    for (std::size_t m = 0; m < markets; ++m) {
        if (total[m] > Fixed::zero()) {
            w.state_share[m] = w.state_people[m] / total[m];
            w.union_share[m] = w.union_share[m] / total[m];
        }
    }
    return w;
}

/// Summen eines Laufs über die Monate des letzten Jahres.
struct Accumulator {
    std::vector<CustomsMeasure> sums; // je Wechsel
    std::vector<Fixed> union_zoll;    // je Union
    std::vector<Fixed> union_income;  // je Union
    std::int64_t months = 0;
};

void sample(const Simulation& sim, const std::vector<CustomsChange>& changes,
            const std::vector<ChangeWeights>& weights, Accumulator& acc) {
    const World& w = sim.world();
    const std::size_t goods = w.goods.size();
    const auto weight = [](const std::vector<Fixed>& share, Index market) {
        return market == kNone ? Fixed::zero() : share[at(market)];
    };
    for (std::size_t c = 0; c < changes.size(); ++c) {
        const ChangeWeights& cw = weights[c];
        CustomsMeasure& m = acc.sums[c];
        for (const TradeFlow& f : sim.trade_flows()) {
            const Index priced = f.to != kNone ? f.to : f.from;
            const Fixed value = f.amount * w.market_states[at(priced)].price[at(f.good)];
            m.border_trade += value * (weight(cw.state_share, f.from) * weight(cw.union_share, f.to) +
                                       weight(cw.union_share, f.from) * weight(cw.state_share, f.to));
            m.trade += value * (weight(cw.state_share, f.from) + weight(cw.state_share, f.to));
        }
        m.revenue += w.states[at(changes[c].state)].budget.taxes[static_cast<std::size_t>(TaxKind::Zoll)];
        Fixed level;
        Fixed people;
        for (std::size_t k = 0; k < w.market_states.size(); ++k) {
            if (cw.state_people[k] <= Fixed::zero()) {
                continue;
            }
            Fixed mean;
            for (std::size_t g = 0; g < goods; ++g) {
                mean += w.market_states[k].price[g] / w.goods[g].base_price;
            }
            level += mean / Fixed::from_int(static_cast<std::int64_t>(goods)) * cw.state_people[k];
            people += cw.state_people[k];
        }
        if (people > Fixed::zero()) {
            m.price_level += level / people;
        }
    }
    for (const State& s : w.states) {
        if (s.customs_union != kNone) {
            acc.union_zoll[at(s.customs_union)] += s.budget.taxes[static_cast<std::size_t>(TaxKind::Zoll)];
            acc.union_income[at(s.customs_union)] += s.budget.income();
        }
    }
    ++acc.months;
}

/// Ein Lauf bis end; Messung an jedem Monatsersten ab measure_from (dann stehen die Ströme des Monats).
Accumulator run_once(Simulation& sim, const std::vector<CustomsChange>& changes,
                     const std::vector<ChangeWeights>& weights, Date measure_from, Date end) {
    Accumulator acc;
    acc.sums.assign(changes.size(), CustomsMeasure{});
    acc.union_zoll.assign(sim.world().customs_unions.size(), Fixed{});
    acc.union_income.assign(sim.world().customs_unions.size(), Fixed{});
    sim.record_trade(true);
    while (sim.world().date < end) {
        const Date today = sim.world().date;
        sim.step_day();
        if (today.day == 1 && measure_from <= today) {
            sample(sim, changes, weights, acc);
        }
    }
    return acc;
}

CustomsMeasure average(const CustomsMeasure& sum, std::int64_t months) {
    if (months <= 0) {
        return {};
    }
    const Fixed n = Fixed::from_int(months);
    return CustomsMeasure{sum.border_trade / n, sum.trade / n, sum.revenue / n, sum.price_level / n};
}

Fixed ratio(Fixed with, Fixed without) {
    return without > Fixed::zero() ? with / without : Fixed::zero();
}

} // namespace

CustomsTargetsResult parse_customs_targets(std::string_view text, const World& world,
                                           const std::string& file_name) {
    return targets_from(parse_script(text, file_name), world, file_name);
}

CustomsTargetsResult load_customs_targets(const std::filesystem::path& file, const World& world) {
    return targets_from(parse_script_file(file), world, path_to_utf8(file));
}

CustomsLabRun run_customs_lab(const World& world, const SimConfig& config, std::int64_t years,
                              const std::vector<CustomsChange>& changes) {
    const Date start = world.date;
    const std::int32_t first_year = start.month == 1 && start.day == 1 ? start.year : start.year + 1;
    const Date end{first_year + static_cast<std::int32_t>(std::max<std::int64_t>(years, 1)), 1, 1};
    const Date measure_from{end.year - 1, 1, 1};
    std::vector<ChangeWeights> weights;
    for (const CustomsChange& change : changes) {
        weights.push_back(weights_of(world, change));
    }

    CustomsLabRun result;
    Simulation baseline(world, config);
    const Accumulator base = run_once(baseline, changes, weights, measure_from, end);
    for (std::size_t u = 0; u < world.customs_unions.size(); ++u) {
        result.share.push_back(
            base.union_income[u] > Fixed::zero() ? base.union_zoll[u] / base.union_income[u] : Fixed::zero());
    }
    for (std::size_t c = 0; c < changes.size(); ++c) {
        result.without.push_back(average(base.sums[c], base.months));
        Simulation sim(world, config);
        Command command;
        command.type = CommandType::CustomsUnion;
        command.state = changes[c].state;
        command.target = changes[c].customs_union;
        sim.submit(command);
        sim.step_day();
        const World& w = sim.world();
        const bool pending =
            std::any_of(w.customs_requests.begin(), w.customs_requests.end(), [&](const CustomsRequest& r) {
                return r.state == changes[c].state && r.customs_union == changes[c].customs_union;
            });
        result.accepted.push_back(pending ||
                                  w.states[at(changes[c].state)].customs_union == changes[c].customs_union);
        const Accumulator with = run_once(sim, {changes[c]}, {weights[c]}, measure_from, end);
        result.with.push_back(average(with.sums[0], with.months));
    }
    return result;
}

CustomsLabSummary summarize_customs_lab(const World& world, const std::vector<CustomsChange>& changes,
                                        const std::vector<CustomsLabRun>& runs,
                                        const CustomsTargets* targets) {
    CustomsLabSummary summary;
    for (std::size_t u = 0; u < world.customs_unions.size(); ++u) {
        std::vector<Fixed> values;
        for (const CustomsLabRun& run : runs) {
            values.push_back(run.share[u]);
        }
        summary.share.push_back(quantiles(values));
    }
    for (std::size_t c = 0; c < changes.size(); ++c) {
        std::vector<Fixed> border;
        std::vector<Fixed> trade;
        std::vector<Fixed> revenue;
        std::vector<Fixed> price;
        std::vector<Fixed> revenue_without;
        CustomsChangeSummary s;
        for (const CustomsLabRun& run : runs) {
            const CustomsMeasure& a = run.without[c];
            const CustomsMeasure& b = run.with[c];
            border.push_back(ratio(b.border_trade, a.border_trade));
            trade.push_back(ratio(b.trade, a.trade));
            revenue.push_back(ratio(b.revenue, a.revenue));
            price.push_back(ratio(b.price_level, a.price_level));
            revenue_without.push_back(a.revenue);
            s.rejected += run.accepted[c] ? 0 : 1;
        }
        s.border_trade = quantiles(border);
        s.trade = quantiles(trade);
        s.revenue = quantiles(revenue);
        s.price_level = quantiles(price);
        s.revenue_without = quantiles(revenue_without);
        const CustomsChange& change = changes[c];
        const bool has_targets = change.border_trade || change.trade || change.revenue;
        if (has_targets && s.rejected > 0) {
            summary.violations.push_back(
                CustomsViolation{CustomsCheck::Rejected, c, Fixed::from_int(s.rejected)});
        }
        const auto check = [&](const std::optional<ValueRange>& range, const Quantiles& q,
                               CustomsCheck kind) {
            if (range && !range->contains(q.median)) {
                summary.violations.push_back(CustomsViolation{kind, c, q.median});
            }
        };
        check(change.border_trade, s.border_trade, CustomsCheck::BorderTrade);
        check(change.trade, s.trade, CustomsCheck::Trade);
        check(change.revenue, s.revenue, CustomsCheck::Revenue);
        summary.changes.push_back(s);
    }
    if (targets != nullptr) {
        for (const CustomsShareTarget& t : targets->shares) {
            const Fixed median = summary.share[at(t.customs_union)].median;
            if (!t.share.contains(median)) {
                summary.violations.push_back(
                    CustomsViolation{CustomsCheck::Share, at(t.customs_union), median});
            }
        }
    }
    return summary;
}

} // namespace uhrwerk
