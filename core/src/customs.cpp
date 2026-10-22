#include "uhrwerk/customs.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
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

/// Datum, zu dem ein heute angenommener Wechsel wirksam wird: der nächste 1. Januar (heute, wenn heute
/// der 1. Januar ist, denn die Befehle laufen vor dem Jahreswechsel).
Date effective_date(const Date& today) {
    return today.month == 1 && today.day == 1 ? today : Date{today.year + 1, 1, 1};
}

void chronicle(World& world, Index state, std::string text) {
    ChronicleEntry entry;
    entry.date = world.date;
    entry.text = std::move(text);
    entry.state = state;
    world.chronicle.push_back(std::move(entry));
}

} // namespace

Index customs_zone(const World& world, Index state) {
    const Index membership = world.states[at(state)].customs_union;
    return membership != kNone ? membership : static_cast<Index>(world.customs_unions.size()) + state;
}

std::size_t customs_zone_count(const World& world) {
    return world.customs_unions.size() + world.states.size();
}

Fixed zone_tariff(const World& world, Index zone) {
    const auto unions = static_cast<Index>(world.customs_unions.size());
    if (zone < unions) {
        return world.customs_unions[at(zone)].tariff;
    }
    return world.states[at(zone - unions)].tax_rate(TaxKind::Zoll);
}

Index zone_slot(const MarketState& market, Index zone) {
    const auto it = std::lower_bound(market.zones.begin(), market.zones.end(), zone);
    return it != market.zones.end() && *it == zone ? static_cast<Index>(it - market.zones.begin()) : kNone;
}

Fixed zone_share(const MarketState& market, Index zone) {
    const Index slot = zone_slot(market, zone);
    return slot != kNone && at(slot) < market.zone_shares.size() ? market.zone_shares[at(slot)]
                                                                 : Fixed::zero();
}

void update_customs_zones(World& world, const EconomyTopology& topology) {
    std::vector<Fixed> people(customs_zone_count(world));
    for (std::size_t m = 0; m < world.market_states.size(); ++m) {
        MarketState& market = world.market_states[m];
        market.zones.clear();
        market.zone_shares.clear();
        if (m >= topology.market_locations.size()) {
            continue;
        }
        std::fill(people.begin(), people.end(), Fixed{});
        Fixed total;
        for (const Index o : topology.market_locations[m]) {
            const Index owner = world.locations[at(o)].owner;
            if (!in_range(world.states, owner)) {
                continue;
            }
            const Fixed population = world.location_population(o);
            people[at(customs_zone(world, owner))] += population;
            total += population;
        }
        if (total <= Fixed::zero()) {
            continue;
        }
        for (std::size_t z = 0; z < people.size(); ++z) {
            if (people[z] > Fixed::zero()) {
                market.zones.push_back(static_cast<Index>(z));
                market.zone_shares.push_back(people[z] / total);
            }
        }
    }
}

Fixed customs_tariff(const MarketState* from, const MarketState& to, const std::vector<Fixed>& tariffs) {
    Fixed result;
    for (std::size_t i = 0; i < to.zones.size(); ++i) {
        const Index zone = to.zones[i];
        const Fixed outside = from != nullptr ? Fixed::one() - zone_share(*from, zone) : Fixed::one();
        result += to.zone_shares[i] * outside * tariffs[at(zone)];
    }
    return result;
}

void distribute_customs(World& world) {
    const std::size_t unions = world.customs_unions.size();
    world.customs_revenue.assign(unions, Fixed{});
    std::vector<Fixed> people(world.states.size());
    for (std::size_t i = 0; i < world.locations.size(); ++i) {
        const Location& loc = world.locations[i];
        if (loc.pop_count == 0 || !in_range(world.states, loc.owner)) {
            continue;
        }
        const Index membership = world.states[at(loc.owner)].customs_union;
        if (membership == kNone) {
            continue;
        }
        if (i < world.economy.size()) {
            world.customs_revenue[at(membership)] +=
                world.economy[i].taxes[static_cast<std::size_t>(TaxKind::Zoll)];
        }
        people[at(loc.owner)] += world.location_population(static_cast<Index>(i));
    }
    // Bevölkerung in Tausend, damit das Produkt aus Kasse und Bevölkerung sicher im Wertebereich bleibt.
    const Fixed thousand = Fixed::from_int(1000);
    std::vector<Fixed> members(unions);
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        const Index membership = world.states[s].customs_union;
        if (membership != kNone) {
            members[at(membership)] += people[s] / thousand;
        }
    }
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        State& state = world.states[s];
        state.customs_income = Fixed{};
        const Index membership = state.customs_union;
        if (membership != kNone && members[at(membership)] > Fixed::zero()) {
            state.customs_income =
                world.customs_revenue[at(membership)] * (people[s] / thousand) / members[at(membership)];
        }
    }
}

CustomsDecision request_customs_change(World& world, Index state_index, Index target) {
    if (world.customs_unions.empty() || !in_range(world.states, state_index) ||
        (target != kNone && !in_range(world.customs_unions, target))) {
        return CustomsDecision::Invalid;
    }
    const State& state = world.states[at(state_index)];
    const Index current = state.customs_union;
    const auto pending = std::find_if(world.customs_requests.begin(), world.customs_requests.end(),
                                      [&](const CustomsRequest& r) { return r.state == state_index; });
    if (target == current) {
        if (pending == world.customs_requests.end()) {
            return CustomsDecision::Invalid;
        }
        world.customs_requests.erase(pending);
        return CustomsDecision::Withdrawn;
    }
    if (current != kNone && world.customs_unions[at(current)].leader == state_index) {
        return CustomsDecision::Invalid; // die Leitmacht bleibt in ihrer Union
    }
    const std::string effective = format_date_long(effective_date(world.date));
    if (target != kNone) {
        const CustomsUnion& joined = world.customs_unions[at(target)];
        if (std::find(joined.excluded.begin(), joined.excluded.end(), state_index) != joined.excluded.end()) {
            const std::string leader = in_range(world.states, joined.leader)
                                           ? world.states[at(joined.leader)].short_name
                                           : joined.name;
            chronicle(world, state_index,
                      leader + " lehnt den Beitritt von " + state.short_name + " zum " + joined.name +
                          " ab.");
            return CustomsDecision::Rejected;
        }
    }
    std::string text = state.short_name;
    if (target == kNone) {
        text += " tritt zum " + effective + " aus dem " + world.customs_unions[at(current)].name + " aus.";
    } else if (current == kNone) {
        text += " tritt zum " + effective + " dem " + world.customs_unions[at(target)].name + " bei.";
    } else {
        text += " verlässt zum " + effective + " den " + world.customs_unions[at(current)].name +
                " und tritt dem " + world.customs_unions[at(target)].name + " bei.";
    }
    if (pending != world.customs_requests.end()) {
        pending->customs_union = target;
    } else {
        world.customs_requests.push_back(CustomsRequest{state_index, target});
    }
    chronicle(world, state_index, std::move(text));
    return CustomsDecision::Accepted;
}

void apply_customs_changes(World& world) {
    for (const CustomsRequest& request : world.customs_requests) {
        if (in_range(world.states, request.state) &&
            (request.customs_union == kNone || in_range(world.customs_unions, request.customs_union))) {
            world.states[at(request.state)].customs_union = request.customs_union;
        }
    }
    world.customs_requests.clear();
}

} // namespace uhrwerk
