// Militär (seit M10, Schritt a1): docs/technik/militaer.md, Abschnitte 3.1 und 3.2.

#include "uhrwerk/military.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace uhrwerk {

namespace {

template <typename T> bool in_range(const std::vector<T>& items, Index index) {
    return index >= 0 && static_cast<std::size_t>(index) < items.size();
}

std::size_t at(Index index) {
    return static_cast<std::size_t>(index);
}

/// Summe der Anteile aller Garnisonen eines Staates (je Staat).
std::vector<Fixed> garrison_shares(const World& world) {
    std::vector<Fixed> sums(world.states.size());
    for (const Garrison& g : world.garrisons) {
        if (in_range(world.states, g.state)) {
            sums[at(g.state)] += g.share;
        }
    }
    return sums;
}

/// Soldaten einer Garnison: ihr Anteil an der Friedensstärke ihres Staates.
Fixed garrison_soldiers(const Garrison& g, Fixed peace, Fixed shares) {
    return shares > Fixed::zero() ? peace * g.share / shares : Fixed::zero();
}

Fixed guns_for(const World& world, Index state, Fixed soldiers) {
    return soldiers * world.states[at(state)].army.guns_per_mille / Fixed::from_int(1000);
}

} // namespace

bool military_active(const World& world) {
    return world.military_rules.active;
}

Fixed peace_strength(const World& world, Index state) {
    if (!in_range(world.states, state)) {
        return Fixed::zero();
    }
    return world.states[at(state)].army_per_mille * world.state_population(state) / Fixed::from_int(1000);
}

Fixed state_soldiers(const World& world, Index state) {
    Fixed soldiers;
    for (const Army& a : world.armies) {
        if (a.state == state) {
            soldiers += a.soldiers;
        }
    }
    return soldiers;
}

std::int64_t state_army_count(const World& world, Index state) {
    return std::count_if(world.armies.begin(), world.armies.end(),
                         [&](const Army& a) { return a.state == state; });
}

const Neighbor* find_neighbor(const World& world, Index a, Index b) {
    if (!in_range(world.neighbors, a)) {
        return nullptr;
    }
    const std::vector<Neighbor>& list = world.neighbors[at(a)];
    const auto it = std::lower_bound(list.begin(), list.end(), b,
                                     [](const Neighbor& n, Index location) { return n.location < location; });
    return it != list.end() && it->location == b ? &*it : nullptr;
}

bool river_crossing(const World& world, const Neighbor& neighbor) {
    return neighbor.river > 0 && neighbor.river <= world.military_rules.river_rank;
}

void ensure_military_state(World& world) {
    if (!military_active(world) || !world.armies.empty()) {
        return;
    }
    const std::vector<Fixed> shares = garrison_shares(world);
    std::vector<Fixed> peace(world.states.size());
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        peace[s] = peace_strength(world, static_cast<Index>(s));
    }
    for (std::size_t g = 0; g < world.garrisons.size(); ++g) {
        const Garrison& garrison = world.garrisons[g];
        Army army;
        army.id = world.next_army_id++;
        army.state = garrison.state;
        army.location = garrison.location;
        army.garrison = static_cast<Index>(g);
        army.name = garrison.name;
        army.soldiers = garrison_soldiers(garrison, peace[at(garrison.state)], shares[at(garrison.state)]);
        army.guns = guns_for(world, garrison.state, army.soldiers);
        army.status = ArmyStatus::Garrison;
        world.armies.push_back(std::move(army));
    }
}

void update_garrisons(World& world) {
    if (!military_active(world)) {
        return;
    }
    const std::vector<Fixed> shares = garrison_shares(world);
    std::vector<Fixed> peace(world.states.size());
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        peace[s] = peace_strength(world, static_cast<Index>(s));
    }
    for (Army& army : world.armies) {
        if (army.status != ArmyStatus::Garrison || !in_range(world.garrisons, army.garrison)) {
            continue;
        }
        const Garrison& garrison = world.garrisons[at(army.garrison)];
        army.soldiers = garrison_soldiers(garrison, peace[at(garrison.state)], shares[at(garrison.state)]);
        army.guns = guns_for(world, garrison.state, army.soldiers);
    }
}

} // namespace uhrwerk
