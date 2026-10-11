#include "uhrwerk/lasttest.hpp"

#include "uhrwerk/population.hpp"
#include "uhrwerk/rng.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace uhrwerk {

namespace {

template <typename T> bool in_range(const std::vector<T>& items, Index index) {
    return index >= 0 && static_cast<std::size_t>(index) < items.size();
}

std::int64_t squared_distance(const Location& a, const Location& b) {
    const Fixed cos_reference = Fixed::from_ratio(6293, 10000); // cos 51°
    const std::int64_t dx = ((b.longitude - a.longitude) * cos_reference).raw();
    const std::int64_t dy = (b.latitude - a.latitude).raw();
    return dx * dx + dy * dy;
}

} // namespace

World make_load_test_world(const World& base, const LoadTestOptions& options) {
    World w = base;
    w.pops.clear();
    w.snapshots.clear();
    w.snapshot_targets.clear();
    w.balance_current.clear();
    w.balance_previous.clear();
    w.economy.clear();
    w.arrivals.clear();
    if (w.states.empty() || w.professions.empty() || w.confessions.empty() || w.cultures.empty()) {
        return w;
    }

    Pcg32 rng(options.seed, RngStream::Lasttest);
    const Rules& rules = w.rules;
    const std::size_t count = w.locations.size();
    const auto professions = static_cast<std::int64_t>(w.professions.size());
    const auto confessions = static_cast<std::int64_t>(w.confessions.size());
    const auto cultures = static_cast<std::int64_t>(w.cultures.size());
    const std::int64_t per_culture = professions * confessions; // Blöcke je gewählter Kultur

    std::vector<Index> with_data;
    for (std::size_t i = 0; i < count; ++i) {
        if (base.locations[i].has_data() && in_range(base.states, base.locations[i].owner)) {
            with_data.push_back(static_cast<Index>(i));
        }
    }

    std::int64_t made = 0;
    std::vector<Index> chosen;
    for (std::size_t i = 0; i < count; ++i) {
        Location& loc = w.locations[i];

        // Besitzer
        if (!in_range(w.states, loc.owner)) {
            Index nearest = kNone;
            std::int64_t best = 0;
            for (const Index j : with_data) {
                const std::int64_t d = squared_distance(loc, base.locations[static_cast<std::size_t>(j)]);
                if (nearest == kNone || d < best) {
                    nearest = j;
                    best = d;
                }
            }
            loc.owner = nearest != kNone ? base.locations[static_cast<std::size_t>(nearest)].owner
                                         : static_cast<Index>(i % w.states.size());
        }

        // Zahl der Kulturen: dem Ziel folgen (verbleibende Blöcke / verbleibende Orte).
        const auto left = static_cast<std::int64_t>(count - i);
        const std::int64_t want = std::max<std::int64_t>(0, options.blocks - made) / left;
        const std::int64_t k = std::clamp<std::int64_t>((want + per_culture / 2) / per_culture, 1, cultures);
        const std::int64_t blocks = k * per_culture;

        // Kulturen ohne Wiederholung ziehen (Fisher-Yates auf den ersten k Plätzen).
        chosen.resize(static_cast<std::size_t>(cultures));
        for (std::int64_t c = 0; c < cultures; ++c) {
            chosen[static_cast<std::size_t>(c)] = static_cast<Index>(c);
        }
        for (std::int64_t c = 0; c < k; ++c) {
            const auto pick =
                c + static_cast<std::int64_t>(rng.below(static_cast<std::uint32_t>(cultures - c)));
            std::swap(chosen[static_cast<std::size_t>(c)], chosen[static_cast<std::size_t>(pick)]);
        }
        std::sort(chosen.begin(), chosen.begin() + static_cast<std::ptrdiff_t>(k));

        // Bevölkerung und Verwüstung
        Fixed capacity = in_range(w.terrains, loc.terrain)
                             ? loc.area_km2 * w.terrains[static_cast<std::size_t>(loc.terrain)].capacity
                             : Fixed::zero();
        if (loc.is_city) {
            capacity = capacity * rules.capacity_city;
        }
        Fixed population = capacity * rng.uniform(Fixed::from_ratio(3, 10), Fixed::from_ratio(9, 10));
        population = Fixed::max(population, Fixed::from_int(20 * blocks));
        loc.devastation = rng.uniform(Fixed::zero(), Fixed::from_ratio(6, 10));

        // Blöcke: gleich groß, Beruf → Konfession → Kultur.
        const Fixed size = population / Fixed::from_int(blocks);
        loc.first_pop = static_cast<Index>(w.pops.size());
        for (std::int64_t p = 0; p < professions; ++p) {
            for (std::int64_t c = 0; c < confessions; ++c) {
                for (std::int64_t u = 0; u < k; ++u) {
                    PopBlock pop;
                    pop.location = static_cast<Index>(i);
                    pop.profession = static_cast<Index>(p);
                    pop.confession = static_cast<Index>(c);
                    pop.culture = chosen[static_cast<std::size_t>(u)];
                    pop.children = size * rules.share_children;
                    pop.young = size * rules.share_young;
                    pop.working = size * rules.share_working;
                    pop.old = size - pop.children - pop.young - pop.working;
                    w.pops.push_back(pop);
                }
            }
        }
        loc.pop_count = static_cast<Index>(w.pops.size()) - loc.first_pop;
        made += blocks;
    }

    initialize_population(w);
    return w;
}

} // namespace uhrwerk
