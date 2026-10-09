#include "uhrwerk/world.hpp"

#include <cstddef>
#include <string_view>
#include <vector>

namespace uhrwerk {

namespace {

template <typename T, typename Key> Index find_by(const std::vector<T>& items, std::string_view id, Key key) {
    if (id.empty()) {
        return kNone;
    }
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (key(items[i]) == id) {
            return static_cast<Index>(i);
        }
    }
    return kNone;
}

template <typename T> bool in_range(const std::vector<T>& items, Index index) {
    return index >= 0 && static_cast<std::size_t>(index) < items.size();
}

} // namespace

Index World::find_state(std::string_view tag) const {
    return find_by(states, tag, [](const State& s) -> std::string_view { return s.tag; });
}

Index World::find_location(std::string_view id) const {
    return find_by(locations, id, [](const Location& l) -> std::string_view { return l.id; });
}

Index World::find_terrain(std::string_view id) const {
    return find_by(terrains, id, [](const TerrainType& t) -> std::string_view { return t.id; });
}

Index World::find_confession(std::string_view id) const {
    return find_by(confessions, id, [](const Confession& c) -> std::string_view { return c.id; });
}

Index World::find_profession(std::string_view id) const {
    return find_by(professions, id, [](const Profession& p) -> std::string_view { return p.id; });
}

Fixed World::location_population(Index location) const {
    if (!in_range(locations, location)) {
        return Fixed::zero();
    }
    const Location& loc = locations[static_cast<std::size_t>(location)];
    Fixed total;
    for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
        if (in_range(pops, p)) {
            total += pops[static_cast<std::size_t>(p)].size;
        }
    }
    return total;
}

Fixed World::state_population(Index state) const {
    Fixed total;
    for (const Index location : state_locations(state)) {
        total += location_population(location);
    }
    return total;
}

std::vector<Index> World::state_locations(Index state) const {
    std::vector<Index> result;
    if (!in_range(states, state)) {
        return result;
    }
    for (std::size_t i = 0; i < locations.size(); ++i) {
        if (locations[i].owner == state) {
            result.push_back(static_cast<Index>(i));
        }
    }
    return result;
}

Index World::location_majority_confession(Index location) const {
    if (!in_range(locations, location)) {
        return kNone;
    }
    const Location& loc = locations[static_cast<std::size_t>(location)];
    std::vector<Fixed> sums(confessions.size());
    std::vector<bool> present(confessions.size(), false);
    for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
        if (!in_range(pops, p)) {
            continue;
        }
        const PopBlock& pop = pops[static_cast<std::size_t>(p)];
        if (!in_range(confessions, pop.confession)) {
            continue;
        }
        sums[static_cast<std::size_t>(pop.confession)] += pop.size;
        present[static_cast<std::size_t>(pop.confession)] = true;
    }
    Index best = kNone;
    for (std::size_t c = 0; c < confessions.size(); ++c) {
        if (!present[c]) {
            continue;
        }
        // Strikt größer: bei Gleichstand bleibt der kleinere Index.
        if (best == kNone || sums[c] > sums[static_cast<std::size_t>(best)]) {
            best = static_cast<Index>(c);
        }
    }
    return best;
}

} // namespace uhrwerk
