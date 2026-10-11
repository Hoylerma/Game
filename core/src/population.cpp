#include "uhrwerk/population.hpp"

#include "uhrwerk/economy.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace uhrwerk {

namespace {

template <typename T> bool in_range(const std::vector<T>& items, Index index) {
    return index >= 0 && static_cast<std::size_t>(index) < items.size();
}

bool same_key(const PopBlock& a, const PopBlock& b) {
    return a.profession == b.profession && a.confession == b.confession && a.culture == b.culture;
}

} // namespace

void compute_location_factors(const World& world, Index location_index, LocationFactors& out) {
    const Rules& rules = world.rules;
    const Location& location = world.locations[static_cast<std::size_t>(location_index)];
    Fixed n;
    for (Index p = location.first_pop; p < location.first_pop + location.pop_count; ++p) {
        n += world.pops[static_cast<std::size_t>(p)].size();
    }
    out.population = n;

    const TerrainType* terrain = in_range(world.terrains, location.terrain)
                                     ? &world.terrains[static_cast<std::size_t>(location.terrain)]
                                     : nullptr;
    Fixed capacity = terrain != nullptr ? location.area_km2 * terrain->capacity : Fixed::zero();
    if (location.is_city) {
        capacity = capacity * rules.capacity_city;
    }
    capacity += capacity * development_bonus(world, location_index);
    out.capacity = capacity;
    out.reserve = capacity > Fixed::zero() && n < capacity ? Fixed::one() - n / capacity : Fixed::zero();
    out.condition = Fixed::one() - location.devastation * rules.devastation_yield_loss;
    // Abnehmender Ertrag: Ist der Ort übervölkert, teilen sich die Agrarbetriebe den Ertrag der
    // Tragfähigkeit.
    out.agrarian = n > capacity && n > Fixed::zero() ? capacity / n : Fixed::one();
}

Fixed literacy_target(const World& world, const Location& location, const PopBlock& pop,
                      Fixed living_standard) {
    if (!in_range(world.professions, pop.profession) || !in_range(world.confessions, pop.confession)) {
        return Fixed::zero();
    }
    const Profession& profession = world.professions[static_cast<std::size_t>(pop.profession)];
    const Confession& confession = world.confessions[static_cast<std::size_t>(pop.confession)];
    Fixed target = profession.literacy * confession.literacy_factor;
    if (location.is_city) {
        target = target * world.rules.literacy_city;
    }
    target = target * (Fixed::from_int(50) + living_standard) / Fixed::from_int(100);
    return target.clamp(Fixed::zero(), Fixed::one());
}

void absorb(PopBlock& into, const PopBlock& from) {
    const Fixed n_into = into.size();
    const Fixed n_from = from.size();
    const Fixed total = n_into + n_from;
    if (total > Fixed::zero()) {
        into.living_standard = (into.living_standard * n_into + from.living_standard * n_from) / total;
        into.literacy = (into.literacy * n_into + from.literacy * n_from) / total;
    }
    into.children += from.children;
    into.young += from.young;
    into.working += from.working;
    into.old += from.old;
}

Index most_similar_block(const std::vector<PopBlock>& blocks, const std::vector<bool>& keep, Index first,
                         Index count, const PopBlock& pop) {
    Index best = kNone;
    int best_tier = 5;
    for (Index i = first; i < first + count; ++i) {
        const auto index = static_cast<std::size_t>(i);
        if (index >= blocks.size() || (index < keep.size() && !keep[index])) {
            continue;
        }
        const PopBlock& candidate = blocks[index];
        const bool profession = candidate.profession == pop.profession;
        const bool confession = candidate.confession == pop.confession;
        const bool culture = candidate.culture == pop.culture;
        int tier = 4;
        if (profession && confession) {
            tier = 0;
        } else if (profession && culture) {
            tier = 1;
        } else if (profession) {
            tier = 2;
        } else if (confession) {
            tier = 3;
        }
        if (tier < best_tier) {
            best = i;
            best_tier = tier;
        } else if (tier == best_tier && tier == 4 &&
                   candidate.size() > blocks[static_cast<std::size_t>(best)].size()) {
            best = i; // Stufe 4: der größte Block; bei Gleichstand bleibt der kleinere Index
        }
    }
    return best;
}

PopulationRates derive_rates(const Rules& rules) {
    PopulationRates r;
    r.aging_children = Fixed::from_ratio(1, 15 * 12);
    r.aging_young = Fixed::from_ratio(1, 15 * 12);
    r.aging_working = Fixed::from_ratio(1, 35 * 12);
    r.literacy_month = rules.literacy_adjustment / Fixed::from_int(12);
    r.fifty = Fixed::from_int(50);
    r.hundred = Fixed::from_int(100);
    r.twelve = Fixed::from_int(12);
    r.twelve_hundred = Fixed::from_int(1200);
    return r;
}

bool reorganize(World& world) {
    std::vector<std::vector<PopBlock>>& arrivals = world.arrivals;
    const Fixed min_size = world.rules.block_min_size;
    std::size_t arrival_count = 0;
    for (const std::vector<PopBlock>& list : arrivals) {
        arrival_count += list.size();
    }
    bool needed = arrival_count > 0;
    for (std::size_t i = 0; i < world.pops.size() && !needed; ++i) {
        needed = world.pops[i].size() < min_size;
    }
    if (!needed) {
        return false;
    }

    std::vector<PopBlock>& pops = world.pops;
    std::vector<bool> keep(pops.size(), false);
    std::vector<PopBlock> result;
    result.reserve(pops.size() + arrival_count);

    for (std::size_t li = 0; li < world.locations.size(); ++li) {
        Location& loc = world.locations[li];
        const Index first = loc.first_pop;
        const Index count = loc.pop_count;

        // 1. Welche Blöcke bleiben? Der größte bleibt immer.
        bool any_kept = false;
        Index largest = kNone;
        for (Index i = first; i < first + count; ++i) {
            const auto index = static_cast<std::size_t>(i);
            keep[index] = pops[index].size() >= min_size;
            any_kept = any_kept || keep[index];
            if (largest == kNone || pops[index].size() > pops[static_cast<std::size_t>(largest)].size()) {
                largest = i;
            }
        }
        if (!any_kept && largest != kNone) {
            keep[static_cast<std::size_t>(largest)] = true;
        }

        // 2. Zu kleine Blöcke im ähnlichsten verbleibenden Block auflösen (Blöcke aufsteigend).
        for (Index i = first; i < first + count; ++i) {
            const auto index = static_cast<std::size_t>(i);
            if (keep[index]) {
                continue;
            }
            const Index target = most_similar_block(pops, keep, first, count, pops[index]);
            absorb(pops[static_cast<std::size_t>(target)], pops[index]);
        }

        const auto new_first = static_cast<Index>(result.size());
        for (Index i = first; i < first + count; ++i) {
            if (keep[static_cast<std::size_t>(i)]) {
                result.push_back(pops[static_cast<std::size_t>(i)]);
            }
        }

        // 3. Zugänge dieses Ortes in Eingangsreihenfolge.
        if (li < arrivals.size()) {
            for (const PopBlock& arrival : arrivals[li]) {
                const auto begin = result.begin() + new_first;
                const auto same = std::find_if(begin, result.end(),
                                               [&](const PopBlock& b) { return same_key(b, arrival); });
                if (same != result.end()) {
                    absorb(*same, arrival);
                } else if (arrival.size() >= min_size || begin == result.end()) {
                    result.push_back(arrival);
                } else {
                    const Index target = most_similar_block(
                        result, {}, new_first, static_cast<Index>(result.size()) - new_first, arrival);
                    absorb(result[static_cast<std::size_t>(target)], arrival);
                }
            }
            arrivals[li].clear();
        }

        loc.first_pop = new_first;
        loc.pop_count = static_cast<Index>(result.size()) - new_first;
    }

    pops = std::move(result);
    return true;
}

void initialize_population(World& world) {
    reorganize(world);

    // Wirtschaft einschwingen (wirtschaft.md, Abschnitt 4.8); danach stehen die Ziel-Lebensstandards.
    // Der Zustand wird neu angelegt (die Welt kann aus einer anderen abgeleitet sein, z. B. der
    // Lasttest-Welt).
    world.business_shares.clear();
    world.location_supply.clear();
    world.location_demand.clear();
    world.location_income.clear();
    world.location_coverage.clear();
    world.market_states.clear();
    world.economy.clear();
    world.location_development.clear();
    const EconomyTopology topology = build_economy_topology(world);
    ensure_economy_state(world, topology);
    estimate_budgets(world); // Staatsnachfrage schon beim Einschwingen
    settle_economy(world, topology, world.rules.settling_months);

    const std::size_t count = world.locations.size();
    const std::size_t professions = world.professions.size();
    world.snapshots.assign(count, {});
    world.snapshot_targets.assign(count * professions, Fixed{});
    LocationFactors factors;
    EconomyScratch scratch;
    for (const std::vector<Index>& list : topology.market_locations) {
        for (const Index location : list) {
            const auto li = static_cast<std::size_t>(location);
            const Location& loc = world.locations[li];
            compute_location_factors(world, location, factors);
            run_location_economy(world, topology, location, factors, scratch);
            world.snapshots[li] = LocationSnapshot{factors.population, factors.reserve};
            std::copy(factors.target.begin(), factors.target.end(),
                      world.snapshot_targets.begin() + static_cast<std::ptrdiff_t>(li * professions));
            for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
                PopBlock& pop = world.pops[static_cast<std::size_t>(p)];
                pop.living_standard = in_range(world.professions, pop.profession)
                                          ? factors.target[static_cast<std::size_t>(pop.profession)]
                                          : Fixed::zero();
                pop.literacy = literacy_target(world, loc, pop, pop.living_standard);
            }
        }
    }
    estimate_budgets(world); // jetzt mit den Steuern nach dem Einschwingen (Jahreseinnahmen)
}

} // namespace uhrwerk
