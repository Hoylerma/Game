#include "uhrwerk/economy.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <utility>
#include <vector>

namespace uhrwerk {

namespace {

template <typename T> bool in_range(const std::vector<T>& items, Index index) {
    return index >= 0 && static_cast<std::size_t>(index) < items.size();
}

std::size_t at(Index index) {
    return static_cast<std::size_t>(index);
}

/// Höchstens dieser Anteil des Angebots (Ausfuhr) bzw. der Nachfrage (Einfuhr) des Vormonats wird je
/// Monat gehandelt (Abschnitt 4.1).
constexpr Fixed kTradeCap = Fixed::from_ratio(4, 5);

/// Anteil der möglichen Handelsmenge bei einer Preislücke (Anteil am höheren Preis): rate × Lücke,
/// höchstens 1.
Fixed trade_share(Fixed rate, Fixed gap, Fixed higher_price) {
    return Fixed::min(Fixed::one(), rate * gap / higher_price);
}

/// Ganzzahlige Quadratwurzel, abgerundet (bitweise, deterministisch).
std::uint64_t isqrt(std::uint64_t value) {
    std::uint64_t result = 0;
    std::uint64_t bit = std::uint64_t{1} << 62;
    while (bit > value) {
        bit >>= 2;
    }
    while (bit != 0) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else {
            result >>= 1;
        }
        bit >>= 2;
    }
    return result;
}

/// Abstand in km wie bei der Wanderung: Grad, Länge mit cos 51° gestaucht, 111,2 km je Grad.
Fixed distance_km(Fixed lon_a, Fixed lat_a, Fixed lon_b, Fixed lat_b) {
    const Fixed cos_reference = Fixed::from_ratio(6293, 10000);
    const std::int64_t dx = ((lon_b - lon_a) * cos_reference).raw();
    const std::int64_t dy = (lat_b - lat_a).raw();
    const auto squared = static_cast<std::uint64_t>(dx * dx + dy * dy); // Rohwert² = Q32
    const Fixed degrees = Fixed::from_raw(static_cast<std::int64_t>(isqrt(squared)));
    return degrees * Fixed::from_ratio(1112, 10);
}

/// Alle Bedürfnisse einer Schicht als Liste (Gut, Menge je Kopf und Jahr), Bedürfnisse in Rangfolge.
Fixed basket_cost(const World& world, Stratum stratum, const std::vector<Fixed>& prices) {
    Fixed cost;
    for (const Need& need : world.needs) {
        for (const GoodAmount& item : need.basket[static_cast<std::size_t>(stratum)]) {
            cost += item.amount * prices[at(item.good)];
        }
    }
    return cost;
}

} // namespace

bool EconomyTopology::active(Index market) const {
    return in_range(market_locations, market) && !market_locations[at(market)].empty();
}

bool business_allowed(const World& world, const Location& location, const BusinessType& business) {
    if (business.siting == Siting::City && !location.is_city) {
        return false;
    }
    if (business.siting == Siting::Rural && location.is_city) {
        return false;
    }
    if (business.deposit != kNone) {
        if (!in_range(world.deposits, business.deposit) ||
            ((location.deposits >> static_cast<unsigned>(business.deposit)) & 1u) == 0) {
            return false;
        }
    }
    return true;
}

EconomyTopology build_economy_topology(const World& world) {
    EconomyTopology t;
    const std::size_t markets = world.markets.size();
    t.market_locations.assign(markets, {});
    t.neighbors.assign(markets, {});
    t.port.assign(markets, false);

    // Mittelpunkt je Markt: nach Fläche gewichtetes Mittel seiner Orte mit Daten (Abschnitt 2.4); Orte
    // ohne Daten ändern nichts an der Rechnung.
    std::vector<Fixed> lon_sum(markets);
    std::vector<Fixed> lat_sum(markets);
    std::vector<Fixed> area(markets);
    for (std::size_t i = 0; i < world.locations.size(); ++i) {
        const Location& loc = world.locations[i];
        if (!in_range(world.markets, loc.market) || loc.pop_count == 0 ||
            !in_range(world.states, loc.owner)) {
            continue;
        }
        const std::size_t m = at(loc.market);
        lon_sum[m] += loc.longitude * loc.area_km2;
        lat_sum[m] += loc.latitude * loc.area_km2;
        area[m] += loc.area_km2;
        t.market_locations[m].push_back(static_cast<Index>(i));
        if (loc.is_port) {
            t.port[m] = true;
        }
    }
    std::vector<Fixed> lon(markets);
    std::vector<Fixed> lat(markets);
    for (std::size_t m = 0; m < markets; ++m) {
        if (area[m] > Fixed::zero()) {
            lon[m] = lon_sum[m] / area[m];
            lat[m] = lat_sum[m] / area[m];
        }
    }

    // Nachbarn: die nächsten aktiven Märkte im Umkreis (Abstand, dann Index).
    const Rules& rules = world.rules;
    const auto max_neighbors = static_cast<std::size_t>(std::max(0, rules.trade_neighbors));
    std::vector<std::pair<Fixed, Index>> near;
    for (std::size_t m = 0; m < markets; ++m) {
        if (t.market_locations[m].empty()) {
            continue;
        }
        near.clear();
        for (std::size_t n = 0; n < markets; ++n) {
            if (n == m || t.market_locations[n].empty()) {
                continue;
            }
            const Fixed d = distance_km(lon[m], lat[m], lon[n], lat[n]);
            if (d <= rules.trade_radius_km) {
                near.emplace_back(d, static_cast<Index>(n));
            }
        }
        std::sort(near.begin(), near.end());
        if (near.size() > max_neighbors) {
            near.resize(max_neighbors);
        }
        for (const auto& [d, n] : near) {
            t.neighbors[m].push_back(MarketLink{n, rules.freight_land * d / Fixed::from_int(100)});
        }
    }

    t.business_by_profession.assign(world.professions.size(), {});
    for (std::size_t b = 0; b < world.businesses.size(); ++b) {
        const Index p = world.businesses[b].profession;
        if (in_range(world.professions, p)) {
            t.business_by_profession[at(p)].push_back(static_cast<Index>(b));
        }
    }

    std::vector<Fixed> base(world.goods.size());
    for (std::size_t g = 0; g < world.goods.size(); ++g) {
        base[g] = world.goods[g].base_price;
    }
    for (std::size_t s = 0; s < 3; ++s) {
        t.base_basket_cost[s] = basket_cost(world, static_cast<Stratum>(s), base);
    }
    return t;
}

void ensure_economy_state(World& world, const EconomyTopology& topology) {
    const std::size_t locations = world.locations.size();
    const std::size_t businesses = world.businesses.size();
    const std::size_t goods = world.goods.size();
    const std::size_t professions = world.professions.size();
    const std::size_t needs = world.needs.size();

    if (world.economy.size() != locations) {
        world.economy.assign(locations, {});
    }
    if (world.location_supply.size() != locations * goods) {
        world.location_supply.assign(locations * goods, Fixed{});
    }
    if (world.location_demand.size() != locations * goods) {
        world.location_demand.assign(locations * goods, Fixed{});
    }
    if (world.location_income.size() != locations * professions) {
        world.location_income.assign(locations * professions, Fixed{});
    }
    if (world.location_coverage.size() != locations * professions * needs) {
        world.location_coverage.assign(locations * professions * needs, Fixed::one());
    }
    if (world.location_development.size() != locations) {
        world.location_development.assign(locations, Fixed{});
    }

    // Betriebsanteile aus den Gewichten (Abschnitt 4.8): je Beruf über die im Ort erlaubten Betriebe.
    if (world.business_shares.size() != locations * businesses) {
        world.business_shares.assign(locations * businesses, Fixed{});
        for (std::size_t i = 0; i < locations; ++i) {
            const Location& loc = world.locations[i];
            if (loc.pop_count == 0 || !in_range(world.terrains, loc.terrain)) {
                continue;
            }
            for (const std::vector<Index>& list : topology.business_by_profession) {
                Fixed total;
                for (const Index b : list) {
                    const BusinessType& business = world.businesses[at(b)];
                    if (business_allowed(world, loc, business)) {
                        total += business.weight * business.terrain_factors[at(loc.terrain)];
                    }
                }
                if (total <= Fixed::zero()) {
                    continue;
                }
                for (const Index b : list) {
                    const BusinessType& business = world.businesses[at(b)];
                    if (business_allowed(world, loc, business)) {
                        world.business_shares[i * businesses + at(b)] =
                            business.weight * business.terrain_factors[at(loc.terrain)] / total;
                    }
                }
            }
        }
    }

    bool markets_fit = world.market_states.size() == world.markets.size();
    for (std::size_t m = 0; markets_fit && m < world.market_states.size(); ++m) {
        markets_fit = world.market_states[m].price.size() == goods;
    }
    if (!markets_fit) {
        world.market_states.assign(world.markets.size(), {});
        for (MarketState& state : world.market_states) {
            state.price.resize(goods);
            for (std::size_t g = 0; g < goods; ++g) {
                state.price[g] = world.goods[g].base_price;
            }
            state.buy_ratio.assign(goods, Fixed::one());
            state.sell_ratio.assign(goods, Fixed::one());
            state.import_share.assign(goods, Fixed{});
            state.supply.assign(goods, Fixed{});
            state.demand.assign(goods, Fixed{});
            state.imports.assign(goods, Fixed{});
            state.exports.assign(goods, Fixed{});
        }
    }
}

std::vector<StateSpending> state_spending(const World& world) {
    std::vector<StateSpending> result;
    for (const State& state : world.states) {
        if (!in_range(world.locations, state.capital)) {
            continue;
        }
        const Index market = world.locations[at(state.capital)].market;
        const StateBudget& b = state.budget;
        result.push_back(StateSpending{market, Stratum::Oben, b.court});
        result.push_back(StateSpending{market, Stratum::Mittel, b.administration + b.building});
        result.push_back(StateSpending{market, Stratum::Unten, b.army});
    }
    return result;
}

Fixed development_bonus(const World& world, Index location) {
    if (!in_range(world.location_development, location) || !in_range(world.locations, location)) {
        return Fixed::zero();
    }
    const Location& loc = world.locations[at(location)];
    const Fixed invested = world.location_development[at(location)];
    if (invested <= Fixed::zero() || loc.area_km2 <= Fixed::zero()) {
        return Fixed::zero();
    }
    const Rules& rules = world.rules;
    const Fixed per_km2 = invested / loc.area_km2;
    return rules.development_max * per_km2 / (per_km2 + rules.development_half);
}

namespace {

/// Bevölkerung eines Staates: Summe seiner Orte (Blöcke und Zugänge).
Fixed population_of(const World& world, const std::vector<Index>& locations) {
    Fixed people;
    for (const Index li : locations) {
        people += world.location_population(li);
    }
    return people;
}

/// Feste Ausgaben eines Monats nach Bevölkerung (Hof, Verwaltung, Heer).
void base_expenses(const World& world, const State& state, Fixed people, StateBudget& budget) {
    const Rules& rules = world.rules;
    const Fixed twelve = Fixed::from_int(12);
    budget.court = state.court_expenses / twelve;
    budget.administration = rules.administration_per_capita * people / twelve;
    budget.army = state.army_per_mille * people / Fixed::from_int(1000) * rules.soldier_pay / twelve;
}

/// Zins für die Schulden eines Staates (Abschnitt 4.6).
Fixed interest_rate_of(const World& world, const State& state) {
    const Rules& rules = world.rules;
    Fixed rate = rules.interest_base;
    if (state.debt > Fixed::zero()) {
        if (state.yearly_income <= Fixed::zero()) {
            return rules.interest_max;
        }
        const Fixed excess = state.debt / state.yearly_income - Fixed::one();
        if (excess > Fixed::zero()) {
            rate += rules.interest_risk * excess;
        }
    }
    if (state.premium_months > 0) {
        rate += rules.bankruptcy_premium;
    }
    return Fixed::min(rate, rules.interest_max);
}

std::vector<Index> locations_of(const World& world, Index state) {
    std::vector<Index> result;
    for (std::size_t i = 0; i < world.locations.size(); ++i) {
        if (world.locations[i].owner == state && world.locations[i].pop_count > 0) {
            result.push_back(static_cast<Index>(i));
        }
    }
    return result;
}

} // namespace

bool run_state_budget(World& world, Index state_index, const std::vector<Index>& locations) {
    const Rules& rules = world.rules;
    State& state = world.states[at(state_index)];
    const Fixed twelve = Fixed::from_int(12);
    StateBudget budget;
    for (const Index li : locations) {
        const LocationEconomy& e = world.economy[at(li)];
        for (std::size_t t = 0; t < kTaxKinds; ++t) {
            budget.taxes[t] += e.taxes[t];
        }
    }
    base_expenses(world, state, population_of(world, locations), budget);
    state.interest_rate = interest_rate_of(world, state);
    budget.interest = state.debt * state.interest_rate / twelve;

    // Bauten: Überschuss über der Rücklage, nur ohne Schulden.
    const Fixed income = budget.income();
    const Fixed available =
        state.treasury + income - budget.court - budget.administration - budget.army - budget.interest;
    const Fixed reserve = rules.reserve_years * (budget.court + budget.administration + budget.army) * twelve;
    if (state.debt <= Fixed::zero() && available > reserve) {
        budget.building = (available - reserve) * rules.building_share;
    }

    // Kasse, Kredit und Tilgung.
    state.treasury += income - budget.expenses();
    if (state.treasury < Fixed::zero()) {
        state.debt -= state.treasury;
        state.treasury = Fixed::zero();
    } else if (state.debt > Fixed::zero()) {
        const Fixed repay = Fixed::min(state.debt, state.treasury);
        state.debt -= repay;
        state.treasury -= repay;
    }

    // Landesausbau: die Bauten verteilen sich nach Fläche auf die Orte des Staates.
    if (budget.building > Fixed::zero()) {
        Fixed area;
        for (const Index li : locations) {
            area += world.locations[at(li)].area_km2;
        }
        if (area > Fixed::zero() && world.location_development.size() == world.locations.size()) {
            for (const Index li : locations) {
                world.location_development[at(li)] +=
                    budget.building * world.locations[at(li)].area_km2 / area;
            }
        }
    }

    state.yearly_income += (income * twelve - state.yearly_income) / twelve;
    state.budget = budget;
    state.last_month_income = income;
    state.last_month_expenses = budget.expenses();
    if (state.premium_months > 0) {
        --state.premium_months;
    }

    // Staatsbankrott: Schulden über bankrott_quote Jahreseinnahmen.
    if (state.yearly_income > Fixed::zero() && state.debt > rules.bankruptcy_ratio * state.yearly_income) {
        state.debt -= state.debt * rules.bankruptcy_haircut;
        state.premium_months = rules.bankruptcy_years * 12;
        ++state.bankruptcies;
        return true;
    }
    return false;
}

void estimate_budgets(World& world) {
    const Fixed twelve = Fixed::from_int(12);
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        State& state = world.states[s];
        const std::vector<Index> locations = locations_of(world, static_cast<Index>(s));
        StateBudget budget;
        for (const Index li : locations) {
            if (at(li) < world.economy.size()) {
                for (std::size_t t = 0; t < kTaxKinds; ++t) {
                    budget.taxes[t] += world.economy[at(li)].taxes[t];
                }
            }
        }
        base_expenses(world, state, population_of(world, locations), budget);
        state.interest_rate = interest_rate_of(world, state);
        budget.interest = state.debt * state.interest_rate / twelve;
        state.budget = budget;
        if (state.yearly_income <= Fixed::zero()) {
            state.yearly_income = budget.income() * twelve;
        }
    }
}

void update_markets(World& world, const EconomyTopology& topology,
                    const std::vector<StateSpending>& spending) {
    const Rules& rules = world.rules;
    const std::size_t goods = world.goods.size();
    const std::size_t markets = world.markets.size();

    // 1. Lokale Mengen aus den Meldungen der Orte, dazu die Staatsnachfrage.
    std::vector<Fixed> local_supply(markets * goods);
    std::vector<Fixed> local_demand(markets * goods);
    std::vector<Fixed> traders(markets);
    for (std::size_t m = 0; m < markets; ++m) {
        for (const Index o : topology.market_locations[m]) {
            for (std::size_t g = 0; g < goods; ++g) {
                local_supply[m * goods + g] += world.location_supply[at(o) * goods + g];
                local_demand[m * goods + g] += world.location_demand[at(o) * goods + g];
            }
            traders[m] += world.economy[at(o)].traders;
        }
    }
    for (const StateSpending& item : spending) {
        if (!topology.active(item.market) || item.amount <= Fixed::zero()) {
            continue;
        }
        const std::size_t m = at(item.market);
        const MarketState& state = world.market_states[m];
        const Fixed cost = basket_cost(world, item.stratum, state.price); // ein Warenkorb (Jahr)
        if (cost <= Fixed::zero()) {
            continue;
        }
        const Fixed baskets = item.amount / cost;
        for (const Need& need : world.needs) {
            for (const GoodAmount& entry : need.basket[static_cast<std::size_t>(item.stratum)]) {
                local_demand[m * goods + at(entry.good)] += entry.amount * baskets;
            }
        }
    }

    // 2. Handel aus den Preisen und Mengen des Vormonats (unabhängig von der Reihenfolge).
    struct Flow {
        Index from = kNone; // kNone = Weltmarkt
        Index to = kNone;
        std::size_t good = 0;
        Fixed amount;
    };
    std::vector<Flow> flows;
    const Fixed one = Fixed::one();
    for (std::size_t a = 0; a < markets; ++a) {
        if (!topology.active(static_cast<Index>(a))) {
            continue;
        }
        for (const MarketLink& link : topology.neighbors[a]) {
            const auto b = at(link.market);
            // Jedes Paar einmal: vom kleineren Index aus, oder wenn der andere Markt a nicht kennt.
            const auto& back = topology.neighbors[b];
            const bool mutual =
                std::any_of(back.begin(), back.end(), [&](const MarketLink& l) { return at(l.market) == a; });
            if (mutual && b < a) {
                continue;
            }
            const MarketState& sa = world.market_states[a];
            const MarketState& sb = world.market_states[b];
            for (std::size_t g = 0; g < goods; ++g) {
                const Fixed pa = sa.price[g];
                const Fixed pb = sb.price[g];
                if (pb > pa * (one + link.freight)) {
                    const Fixed gap = pb - pa * (one + link.freight);
                    const Fixed amount =
                        trade_share(rules.trade_rate, gap, pb) * Fixed::min(sa.supply[g], sb.demand[g]);
                    if (amount > Fixed::zero()) {
                        flows.push_back(Flow{static_cast<Index>(a), static_cast<Index>(b), g, amount});
                    }
                } else if (pa > pb * (one + link.freight)) {
                    const Fixed gap = pa - pb * (one + link.freight);
                    const Fixed amount =
                        trade_share(rules.trade_rate, gap, pa) * Fixed::min(sb.supply[g], sa.demand[g]);
                    if (amount > Fixed::zero()) {
                        flows.push_back(Flow{static_cast<Index>(b), static_cast<Index>(a), g, amount});
                    }
                }
            }
        }
        if (!topology.port[a]) {
            continue;
        }
        const MarketState& sa = world.market_states[a];
        for (std::size_t g = 0; g < goods; ++g) {
            const Good& good = world.goods[g];
            if (good.world_factor <= Fixed::zero()) {
                continue;
            }
            const Fixed world_price = good.base_price * good.world_factor;
            const Fixed p = sa.price[g];
            if (world_price > p * (one + rules.freight_sea)) {
                const Fixed amount =
                    trade_share(rules.trade_rate, world_price - p * (one + rules.freight_sea), world_price) *
                    sa.supply[g];
                if (amount > Fixed::zero()) {
                    flows.push_back(Flow{static_cast<Index>(a), kNone, g, amount});
                }
            } else if (p > world_price * (one + rules.freight_sea)) {
                const Fixed amount =
                    trade_share(rules.trade_rate, p - world_price * (one + rules.freight_sea), p) *
                    sa.demand[g];
                if (amount > Fixed::zero()) {
                    flows.push_back(Flow{kNone, static_cast<Index>(a), g, amount});
                }
            }
        }
    }
    // Obergrenze: höchstens kTradeCap des Angebots bzw. der Nachfrage des Vormonats.
    std::vector<Fixed> out_total(markets * goods);
    std::vector<Fixed> in_total(markets * goods);
    for (const Flow& f : flows) {
        if (f.from != kNone) {
            out_total[at(f.from) * goods + f.good] += f.amount;
        }
        if (f.to != kNone) {
            in_total[at(f.to) * goods + f.good] += f.amount;
        }
    }
    const auto scale = [](Fixed total, Fixed limit) {
        return total > limit && total > Fixed::zero() ? limit / total : Fixed::one();
    };
    std::vector<Fixed> imports(markets * goods);
    std::vector<Fixed> exports(markets * goods);
    for (const Flow& f : flows) {
        Fixed factor = one;
        if (f.from != kNone) {
            const std::size_t k = at(f.from) * goods + f.good;
            factor = Fixed::min(
                factor, scale(out_total[k], kTradeCap * world.market_states[at(f.from)].supply[f.good]));
        }
        if (f.to != kNone) {
            const std::size_t k = at(f.to) * goods + f.good;
            factor = Fixed::min(factor,
                                scale(in_total[k], kTradeCap * world.market_states[at(f.to)].demand[f.good]));
        }
        const Fixed amount = f.amount * factor;
        if (f.from != kNone) {
            exports[at(f.from) * goods + f.good] += amount;
        }
        if (f.to != kNone) {
            imports[at(f.to) * goods + f.good] += amount;
        }
    }

    // 3.–5. Preise, Quoten, Handelsspanne.
    for (std::size_t m = 0; m < markets; ++m) {
        if (!topology.active(static_cast<Index>(m))) {
            continue;
        }
        MarketState& state = world.market_states[m];
        Fixed sales;
        for (std::size_t g = 0; g < goods; ++g) {
            const std::size_t k = m * goods + g;
            const Fixed supply = local_supply[k] + imports[k];
            const Fixed demand = local_demand[k] + exports[k];
            const Fixed base = world.goods[g].base_price;
            Fixed target = base;
            if (supply <= Fixed::zero()) {
                target = demand > Fixed::zero() ? base * rules.price_max : base;
            } else if (demand <= Fixed::zero()) {
                target = base * rules.price_min;
            } else {
                target = base * (demand / supply).clamp(rules.price_min, rules.price_max);
            }
            state.price[g] += (target - state.price[g]) * rules.price_adjustment;
            state.buy_ratio[g] = supply >= demand ? one : supply / demand;
            state.sell_ratio[g] = demand >= supply ? one : demand / supply;
            state.import_share[g] = supply > Fixed::zero() ? imports[k] / supply : Fixed::zero();
            state.supply[g] = supply;
            state.demand[g] = demand;
            state.imports[g] = imports[k];
            state.exports[g] = exports[k];
            sales += state.price[g] * Fixed::min(supply, demand);
        }
        state.margin_per_trader =
            traders[m] > Fixed::zero() ? rules.trade_margin * sales / traders[m] : Fixed::zero();
    }
}

void draw_harvest(World& world, const EconomyTopology& topology, Pcg32& rng) {
    const Rules& rules = world.rules;
    const Fixed land = rng.uniform(-rules.harvest_variation_land, rules.harvest_variation_land);
    for (std::size_t m = 0; m < world.market_states.size(); ++m) {
        if (!topology.active(static_cast<Index>(m))) {
            continue;
        }
        const Fixed local = rng.uniform(-rules.harvest_variation_market, rules.harvest_variation_market);
        world.market_states[m].harvest = Fixed::one() + land + local;
    }
}

Fixed food_coverage(const World& world, Index location, Index profession) {
    const std::size_t needs = world.needs.size();
    const std::size_t base = (at(location) * world.professions.size() + at(profession)) * needs;
    Fixed result = Fixed::one();
    for (std::size_t n = 0; n < needs; ++n) {
        if (world.needs[n].hunger && base + n < world.location_coverage.size()) {
            result = Fixed::min(result, world.location_coverage[base + n]);
        }
    }
    return result;
}

void run_location_economy(World& world, const EconomyTopology& topology, Index location,
                          LocationFactors& factors, EconomyScratch& scratch) {
    const std::size_t li = at(location);
    const Location& loc = world.locations[li];
    const Rules& rules = world.rules;
    const std::size_t goods = world.goods.size();
    const std::size_t professions = world.professions.size();
    const std::size_t businesses = world.businesses.size();
    const std::size_t needs = world.needs.size();
    const Fixed one = Fixed::one();
    const Fixed twelve = Fixed::from_int(12);
    const Fixed hundred = Fixed::from_int(100);

    factors.target.assign(professions, Fixed{});
    Fixed* supply = world.location_supply.data() + li * goods;
    Fixed* demand = world.location_demand.data() + li * goods;
    std::fill(supply, supply + goods, Fixed{});
    std::fill(demand, demand + goods, Fixed{});
    LocationEconomy economy;
    if (!topology.active(loc.market) || !in_range(world.states, loc.owner) ||
        !in_range(world.terrains, loc.terrain)) {
        world.economy[li] = economy;
        return;
    }
    const MarketState& market = world.market_states[at(loc.market)];
    const State& owner = world.states[at(loc.owner)];
    const Fixed kontribution = owner.tax_rate(TaxKind::Kontribution);
    const Fixed akzise = loc.is_city ? owner.tax_rate(TaxKind::Akzise) : Fixed::zero();
    const Fixed zoll = owner.tax_rate(TaxKind::Zoll);

    // Köpfe je Beruf (Blöcke, ohne Zugänge).
    scratch.people.assign(professions, Fixed{});
    for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
        const PopBlock& pop = world.pops[at(p)];
        if (in_range(world.professions, pop.profession)) {
            scratch.people[at(pop.profession)] += pop.size();
        }
    }
    Fixed levy_total; // Anteil der Agrarabgaben (nur Berufe mit Angehörigen im Ort)
    for (std::size_t p = 0; p < professions; ++p) {
        if (scratch.people[p] > Fixed::zero()) {
            levy_total += world.professions[p].agrarian_levy;
        }
    }

    // 4.2 Erzeugung und Einkommen je Kopf und Monat.
    const Fixed terrain_yield = world.terrains[at(loc.terrain)].yield_factor;
    const Fixed agrarian_factor = factors.agrarian * market.harvest;
    scratch.income.assign(professions, Fixed{});
    scratch.agrar_income.assign(professions, Fixed{});
    scratch.profit.assign(businesses, Fixed{});
    Fixed levy_pool;
    for (std::size_t p = 0; p < professions; ++p) {
        const Fixed n = scratch.people[p];
        for (const Index b : topology.business_by_profession[p]) {
            const Fixed share = world.business_shares[li * businesses + at(b)];
            const BusinessType& business = world.businesses[at(b)];
            if (share <= Fixed::zero() || !business_allowed(world, loc, business)) {
                continue;
            }
            Fixed revenue;
            Fixed cost;
            if (business.trade) {
                revenue = share * market.margin_per_trader;
                economy.traders += n * share;
            } else {
                Fixed supplied = one;
                for (const GoodAmount& input : business.inputs) {
                    supplied = Fixed::min(supplied, market.buy_ratio[at(input.good)]);
                }
                Fixed unit = share * terrain_yield * factors.condition * supplied;
                if (business.agrarian) {
                    unit = unit * agrarian_factor;
                }
                for (const GoodAmount& output : business.outputs) {
                    const std::size_t g = at(output.good);
                    const Fixed quantity = unit * output.amount / twelve; // je Kopf des Berufs
                    supply[g] += quantity * n;
                    revenue += quantity * market.price[g] * (one - rules.trade_margin);
                }
                for (const GoodAmount& input : business.inputs) {
                    const std::size_t g = at(input.good);
                    const Fixed quantity = unit * input.amount / twelve;
                    demand[g] += quantity * n;
                    cost += quantity * market.price[g];
                }
            }
            economy.production += (revenue - cost) * n;
            Fixed added = revenue - cost;
            if (business.agrarian) {
                const Fixed levy = revenue * levy_total;
                added -= levy;
                levy_pool += levy * n;
                scratch.agrar_income[p] += added;
            }
            scratch.income[p] += added;
            scratch.profit[at(b)] = added / share;
        }
    }
    for (std::size_t p = 0; p < professions; ++p) {
        const Fixed n = scratch.people[p];
        const Profession& profession = world.professions[p];
        if (n > Fixed::zero() && profession.agrarian_levy > Fixed::zero() && levy_total > Fixed::zero()) {
            scratch.income[p] += levy_pool * profession.agrarian_levy / levy_total / n;
        }
        // Kontribution: auf dem Land das ganze Einkommen, in Städten nur das aus Agrarbetrieben.
        if (profession.taxable) {
            const Fixed base = loc.is_city ? scratch.agrar_income[p] : scratch.income[p];
            if (base > Fixed::zero()) {
                const Fixed tax = base * kontribution;
                scratch.income[p] -= tax;
                economy.taxes[static_cast<std::size_t>(TaxKind::Kontribution)] += tax * n;
            }
        }
    }

    // 4.3 Verbrauch und 4.4 Lebensstandard.
    scratch.consumer.resize(goods);
    for (std::size_t g = 0; g < goods; ++g) {
        scratch.consumer[g] = market.price[g] * (one + akzise) * (one + zoll * market.import_share[g]);
    }
    scratch.bought.assign(goods, Fixed{});
    for (std::size_t p = 0; p < professions; ++p) {
        const Fixed n = scratch.people[p];
        const auto stratum = static_cast<std::size_t>(world.professions[p].stratum);
        const Fixed yearly = Fixed::max(Fixed::zero(), scratch.income[p]) * twelve; // je Kopf und Jahr
        world.location_income[li * professions + p] = scratch.income[p];

        Fixed full;
        Fixed essential; // Bedürfnisse mit hunger = ja
        for (const Need& need : world.needs) {
            for (const GoodAmount& item : need.basket[stratum]) {
                const Fixed cost = item.amount * scratch.consumer[at(item.good)];
                full += cost;
                if (need.hunger) {
                    essential += cost;
                }
            }
        }
        // Reicht das Einkommen für den ganzen Korb, wird vom Rest mehr gekauft – aber nicht mehr
        // Grundnahrung: Der Überschuss geht in die übrigen Bedürfnisse.
        Fixed rest = yearly;
        const bool plenty = full > Fixed::zero() && yearly >= full;
        const bool only_essential = full <= essential;
        const Fixed scale = !plenty          ? Fixed::zero()
                            : only_essential ? yearly / full
                                             : (yearly - essential) / (full - essential);
        for (std::size_t k = 0; k < needs; ++k) {
            const Need& need = world.needs[k];
            Fixed cost;
            Fixed weight;
            Fixed available;
            for (const GoodAmount& item : need.basket[stratum]) {
                cost += item.amount * scratch.consumer[at(item.good)];
                const Fixed w = item.amount * world.goods[at(item.good)].base_price;
                weight += w;
                available += w * market.buy_ratio[at(item.good)];
            }
            Fixed f = one;
            if (plenty) {
                f = need.hunger && !only_essential ? one : scale;
            } else if (cost > Fixed::zero()) {
                f = Fixed::min(one, rest / cost);
                rest -= f * cost;
            }
            for (const GoodAmount& item : need.basket[stratum]) {
                const std::size_t g = at(item.good);
                const Fixed wanted = n * item.amount * f / twelve;
                demand[g] += wanted;
                scratch.bought[g] += wanted * market.buy_ratio[g];
            }
            const Fixed rationed = weight > Fixed::zero() ? available / weight : one;
            world.location_coverage[(li * professions + p) * needs + k] = Fixed::min(one, f) * rationed;
        }

        const Fixed base_cost = topology.base_basket_cost[stratum];
        const Fixed price_level = base_cost > Fixed::zero() ? full / base_cost : one;
        const Fixed real = price_level > Fixed::zero() ? yearly / price_level : Fixed::zero();
        const Fixed denominator = real + rules.living_standard_reference;
        factors.target[p] = denominator > Fixed::zero() ? hundred * real / denominator : Fixed::zero();
    }
    for (std::size_t g = 0; g < goods; ++g) {
        const Fixed value = scratch.bought[g] * market.price[g];
        economy.taxes[static_cast<std::size_t>(TaxKind::Akzise)] += value * akzise;
        economy.taxes[static_cast<std::size_t>(TaxKind::Zoll)] +=
            value * (one + akzise) * zoll * market.import_share[g];
    }
    world.economy[li] = economy;

    // 4.5 Betriebsanteile folgen dem Gewinn.
    for (std::size_t p = 0; p < professions; ++p) {
        const std::vector<Index>& list = topology.business_by_profession[p];
        Fixed average;
        std::size_t allowed = 0;
        for (const Index b : list) {
            if (business_allowed(world, loc, world.businesses[at(b)])) {
                average += world.business_shares[li * businesses + at(b)] * scratch.profit[at(b)];
                ++allowed;
            }
        }
        if (allowed < 2 || average <= Fixed::zero()) {
            continue;
        }
        Fixed total;
        for (const Index b : list) {
            if (!business_allowed(world, loc, world.businesses[at(b)])) {
                continue;
            }
            Fixed& share = world.business_shares[li * businesses + at(b)];
            share = share * (one + rules.business_adjustment * (scratch.profit[at(b)] - average) / average);
            share = Fixed::max(share, rules.business_min_share);
            total += share;
        }
        for (const Index b : list) {
            if (business_allowed(world, loc, world.businesses[at(b)])) {
                Fixed& share = world.business_shares[li * businesses + at(b)];
                share = share / total;
            }
        }
    }
}

void settle_economy(World& world, const EconomyTopology& topology, int months) {
    const std::vector<StateSpending> spending = state_spending(world);
    LocationFactors factors;
    EconomyScratch scratch;
    for (int month = 0; month < months; ++month) {
        update_markets(world, topology, spending);
        for (const std::vector<Index>& list : topology.market_locations) {
            for (const Index o : list) {
                compute_location_factors(world, o, factors);
                run_location_economy(world, topology, o, factors, scratch);
            }
        }
    }
}

} // namespace uhrwerk
