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

Index World::find_district(std::string_view id) const {
    return find_by(districts, id, [](const District& d) -> std::string_view { return d.id; });
}

Index World::find_admin_unit(std::string_view id) const {
    return find_by(admin_units, id, [](const AdminUnit& u) -> std::string_view { return u.id; });
}

Index World::find_terrain(std::string_view id) const {
    return find_by(terrains, id, [](const TerrainType& t) -> std::string_view { return t.id; });
}

Index World::find_confession(std::string_view id) const {
    return find_by(confessions, id, [](const Confession& c) -> std::string_view { return c.id; });
}

Index World::find_culture(std::string_view id) const {
    return find_by(cultures, id, [](const Culture& c) -> std::string_view { return c.id; });
}

Index World::find_profession(std::string_view id) const {
    return find_by(professions, id, [](const Profession& p) -> std::string_view { return p.id; });
}

Index World::find_good(std::string_view id) const {
    return find_by(goods, id, [](const Good& g) -> std::string_view { return g.id; });
}

Index World::find_need(std::string_view id) const {
    return find_by(needs, id, [](const Need& n) -> std::string_view { return n.id; });
}

Index World::find_business(std::string_view id) const {
    return find_by(businesses, id, [](const BusinessType& b) -> std::string_view { return b.id; });
}

Index World::find_current(std::string_view id) const {
    return find_by(currents, id, [](const PoliticalCurrent& c) -> std::string_view { return c.id; });
}

Index World::find_interest_group(std::string_view id) const {
    return find_by(interest_groups, id, [](const InterestGroup& g) -> std::string_view { return g.id; });
}

Index World::find_parliament(std::string_view id) const {
    return find_by(parliaments, id, [](const Parliament& p) -> std::string_view { return p.id; });
}

Index World::find_customs_union(std::string_view id) const {
    return find_by(customs_unions, id, [](const CustomsUnion& u) -> std::string_view { return u.id; });
}

Index World::find_market(std::string_view id) const {
    return find_by(markets, id, [](const Market& m) -> std::string_view { return m.id; });
}

Fixed World::location_population(Index location) const {
    if (!in_range(locations, location)) {
        return Fixed::zero();
    }
    const Location& loc = locations[static_cast<std::size_t>(location)];
    Fixed total;
    for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
        if (in_range(pops, p)) {
            total += pops[static_cast<std::size_t>(p)].size();
        }
    }
    if (in_range(arrivals, location)) {
        for (const PopBlock& arrival : arrivals[static_cast<std::size_t>(location)]) {
            total += arrival.size();
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

std::vector<Index> World::district_locations(Index district) const {
    std::vector<Index> result;
    if (!in_range(districts, district)) {
        return result;
    }
    for (std::size_t i = 0; i < locations.size(); ++i) {
        if (locations[i].district == district) {
            result.push_back(static_cast<Index>(i));
        }
    }
    return result;
}

std::size_t World::locations_with_data() const {
    std::size_t count = 0;
    for (const Location& location : locations) {
        if (location.has_data()) {
            ++count;
        }
    }
    return count;
}

Index World::location_majority_confession(Index location) const {
    return majority(location, confessions.size(), [](const PopBlock& pop) { return pop.confession; });
}

Index World::location_majority_culture(Index location) const {
    return majority(location, cultures.size(), [](const PopBlock& pop) { return pop.culture; });
}

template <typename Key> Index World::majority(Index location, std::size_t count, Key key) const {
    if (!in_range(locations, location)) {
        return kNone;
    }
    const Location& loc = locations[static_cast<std::size_t>(location)];
    std::vector<Fixed> sums(count);
    std::vector<bool> present(count, false);
    for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
        if (!in_range(pops, p)) {
            continue;
        }
        const PopBlock& pop = pops[static_cast<std::size_t>(p)];
        const Index k = key(pop);
        if (k < 0 || static_cast<std::size_t>(k) >= count) {
            continue;
        }
        sums[static_cast<std::size_t>(k)] += pop.size();
        present[static_cast<std::size_t>(k)] = true;
    }
    Index best = kNone;
    for (std::size_t c = 0; c < count; ++c) {
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
