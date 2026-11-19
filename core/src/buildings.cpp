#include "uhrwerk/buildings.hpp"

#include "uhrwerk/economy.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace uhrwerk {

namespace {

std::size_t at(Index i) {
    return static_cast<std::size_t>(i);
}

template <typename T> bool in_range(const std::vector<T>& v, Index i) {
    return i >= 0 && static_cast<std::size_t>(i) < v.size();
}

std::size_t slot(const World& world, Index region, Index type) {
    return at(region) * world.building_types.size() + at(type);
}

bool available(const World& world, const BuildingType& type) {
    return type.since <= world.date;
}

/// Ort, der zum Gebäude passt: ohne Betrieb jeder, sonst wo der Betrieb erlaubt ist.
bool suits(const World& world, const Location& loc, const BuildingType& type) {
    return type.business == kNone || !in_range(world.businesses, type.business) ||
           business_allowed(world, loc, world.businesses[at(type.business)]);
}

/// Köpfe eines Berufs im Ort (Blöcke und Zugänge).
Fixed heads(const World& world, Index location, Index profession) {
    Fixed total;
    const Location& loc = world.locations[at(location)];
    for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
        if (world.pops[at(p)].profession == profession) {
            total += world.pops[at(p)].size();
        }
    }
    if (in_range(world.arrivals, location)) {
        for (const PopBlock& pop : world.arrivals[at(location)]) {
            if (pop.profession == profession) {
                total += pop.size();
            }
        }
    }
    return total;
}

const BuildOrder* find_order(const World& world, Index region, Index type) {
    for (const BuildOrder& order : world.build_orders) {
        if (order.region == region && order.type == type) {
            return &order;
        }
    }
    return nullptr;
}

bool owns_part(const World& world, Index state, const Region& region) {
    return std::any_of(region.locations.begin(), region.locations.end(),
                       [&](Index li) { return world.locations[at(li)].owner == state; });
}

void chronicle(World& world, Index state, const Region& region, std::string text) {
    ChronicleEntry entry;
    entry.date = world.date;
    entry.text = std::move(text);
    entry.state = state;
    entry.location = region.locations.empty() ? kNone : region.locations.front();
    world.chronicle.push_back(std::move(entry));
}

/// Fertige Stufe: Melioration verteilt ihren Landesausbau nach Fläche auf die Orte des Landesteils.
void complete(World& world, const BuildOrder& order) {
    const Region& region = world.regions[at(order.region)];
    const BuildingType& type = world.building_types[at(order.type)];
    std::int32_t& level = world.building_levels[slot(world, order.region, order.type)];
    ++level;
    if (type.development > Fixed::zero() && world.location_development.size() == world.locations.size()) {
        for (const Index li : region.locations) {
            world.location_development[at(li)] += type.development * world.locations[at(li)].area_km2;
        }
    }
    rebuild_building_effects(world);
    chronicle(world, order.state, region,
              region.name + ": " + type.name + " fertig (Stufe " + std::to_string(level) + ").");
}

} // namespace

void build_regions(World& world) {
    world.regions.clear();
    world.location_region.assign(world.locations.size(), kNone);
    std::map<std::string, Index> by_id;
    for (std::size_t li = 0; li < world.locations.size(); ++li) {
        const Location& loc = world.locations[li];
        if (!loc.has_data()) {
            continue;
        }
        std::string id;
        std::string name;
        if (in_range(world.admin_units, loc.admin)) {
            Index top = loc.admin;
            // Nach oben bis zur obersten Einheit (Schutz gegen Kreise im Baum).
            for (std::size_t guard = 0; guard < world.admin_units.size(); ++guard) {
                const Index parent = world.admin_units[at(top)].parent;
                if (!in_range(world.admin_units, parent)) {
                    break;
                }
                top = parent;
            }
            id = "verwaltung:" + world.admin_units[at(top)].id;
            name = world.admin_units[at(top)].name;
        } else if (in_range(world.districts, loc.district)) {
            id = "kreis:" + world.districts[at(loc.district)].id;
            name = world.districts[at(loc.district)].name;
        } else {
            id = "ort:" + loc.id;
            name = loc.name;
        }
        auto [it, inserted] = by_id.try_emplace(id, static_cast<Index>(world.regions.size()));
        if (inserted) {
            Region region;
            region.id = id;
            region.name = name;
            world.regions.push_back(std::move(region));
        }
        Region& region = world.regions[at(it->second)];
        region.locations.push_back(static_cast<Index>(li));
        region.area_km2 += loc.area_km2;
        world.location_region[li] = it->second;
    }
}

void ensure_building_state(World& world) {
    if (!world.building_rules.active) {
        world.building_levels.clear();
        world.build_orders.clear();
        world.building_yield.clear();
        world.building_literacy.clear();
        return;
    }
    const std::size_t size = world.regions.size() * world.building_types.size();
    if (world.building_levels.size() != size) {
        world.building_levels.assign(size, 0);
    }
    rebuild_building_effects(world);
}

void rebuild_building_effects(World& world) {
    const std::size_t businesses = world.businesses.size();
    world.building_yield.assign(world.locations.size() * businesses, Fixed{});
    world.building_literacy.assign(world.locations.size(), Fixed{});
    if (!world.building_rules.active) {
        return;
    }
    for (std::size_t r = 0; r < world.regions.size(); ++r) {
        for (std::size_t t = 0; t < world.building_types.size(); ++t) {
            const std::int32_t level = world.building_levels[r * world.building_types.size() + t];
            if (level <= 0) {
                continue;
            }
            const BuildingType& type = world.building_types[t];
            const Fixed levels = Fixed::from_int(level);
            for (const Index li : world.regions[r].locations) {
                if (in_range(world.businesses, type.business) && type.yield > Fixed::zero()) {
                    world.building_yield[at(li) * businesses + at(type.business)] += type.yield * levels;
                }
                if (type.literacy > Fixed::zero()) {
                    world.building_literacy[at(li)] += type.literacy * levels;
                }
            }
        }
    }
}

std::int32_t building_level(const World& world, Index region, Index type) {
    if (!world.building_rules.active || !in_range(world.regions, region) ||
        !in_range(world.building_types, type)) {
        return 0;
    }
    const std::size_t k = slot(world, region, type);
    return k < world.building_levels.size() ? world.building_levels[k] : 0;
}

Fixed region_population(const World& world, Index region) {
    Fixed total;
    if (in_range(world.regions, region)) {
        for (const Index li : world.regions[at(region)].locations) {
            total += world.location_population(li);
        }
    }
    return total;
}

Index region_owner(const World& world, Index region) {
    if (!in_range(world.regions, region) || world.regions[at(region)].locations.empty()) {
        return kNone;
    }
    return world.locations[at(world.regions[at(region)].locations.front())].owner;
}

Fixed build_cost(const World& world, Index region, Index type) {
    if (!in_range(world.building_types, type)) {
        return Fixed{};
    }
    return world.building_types[at(type)].cost * region_population(world, region) / Fixed::from_int(1000);
}

std::int32_t order_capacity(const World& world, Index state) {
    if (!in_range(world.states, state)) {
        return 0;
    }
    Fixed people;
    for (std::size_t li = 0; li < world.locations.size(); ++li) {
        if (world.locations[li].owner == state) {
            people += world.location_population(static_cast<Index>(li));
        }
    }
    const BuildingRules& r = world.building_rules;
    const Fixed capacity = r.orders_base + r.orders_per_million * people / Fixed::from_int(1'000'000);
    return std::max<std::int32_t>(1, static_cast<std::int32_t>(capacity.trunc()));
}

std::int32_t orders_of(const World& world, Index state) {
    return static_cast<std::int32_t>(std::count_if(world.build_orders.begin(), world.build_orders.end(),
                                                   [&](const BuildOrder& o) { return o.state == state; }));
}

std::string build_problem(const World& world, Index state, Index region, Index type) {
    if (!world.building_rules.active) {
        return "In diesem Szenario wird nicht gebaut.";
    }
    if (!in_range(world.states, state) || !in_range(world.regions, region) ||
        !in_range(world.building_types, type)) {
        return "Unbekannter Staat, Landesteil oder Gebäude.";
    }
    const BuildingType& t = world.building_types[at(type)];
    const Region& r = world.regions[at(region)];
    if (!available(world, t)) {
        return t.name + " gibt es erst später.";
    }
    if (!owns_part(world, state, r)) {
        return r.name + " gehört nicht zum Staat.";
    }
    if (find_order(world, region, type) != nullptr) {
        return t.name + " wird in " + r.name + " schon gebaut.";
    }
    if (building_level(world, region, type) >= t.max_level) {
        return t.name + " hat in " + r.name + " schon die höchste Stufe.";
    }
    if (!std::any_of(r.locations.begin(), r.locations.end(),
                     [&](Index li) { return suits(world, world.locations[at(li)], t); })) {
        return t.name + " passt in " + r.name + " an keinen Ort.";
    }
    if (orders_of(world, state) >= order_capacity(world, state)) {
        return "Das Bauamt ist ausgelastet (" + std::to_string(order_capacity(world, state)) + " Aufträge).";
    }
    return {};
}

bool order_building(World& world, Index state, Index region, Index type) {
    if (!build_problem(world, state, region, type).empty()) {
        return false;
    }
    const BuildingType& t = world.building_types[at(type)];
    BuildOrder order;
    order.state = state;
    order.region = region;
    order.type = type;
    order.months_left = std::max<std::int32_t>(1, t.months);
    order.monthly_cost = build_cost(world, region, type) / Fixed::from_int(order.months_left);
    world.build_orders.push_back(order);
    const Region& r = world.regions[at(region)];
    chronicle(world, state, r,
              world.states[at(state)].short_name + " baut in " + r.name + ": " + t.name + " (Stufe " +
                  std::to_string(building_level(world, region, type) + 1) + ", " +
                  std::to_string(order.months_left) + " Monate).");
    return true;
}

bool cancel_building(World& world, Index state, Index region, Index type) {
    const auto it =
        std::find_if(world.build_orders.begin(), world.build_orders.end(), [&](const BuildOrder& o) {
            return o.state == state && o.region == region && o.type == type;
        });
    if (it == world.build_orders.end()) {
        return false;
    }
    world.build_orders.erase(it);
    return true;
}

Fixed run_buildings(World& world, Index state) {
    if (!world.building_rules.active) {
        return Fixed{};
    }
    Fixed expenses;
    // Aufträge des Staates in Eingangsreihenfolge; fertige werden danach entfernt.
    std::vector<BuildOrder> finished;
    for (BuildOrder& order : world.build_orders) {
        if (order.state != state) {
            continue;
        }
        expenses += order.monthly_cost;
        --order.months_left;
        if (order.months_left <= 0) {
            finished.push_back(order);
        }
    }
    if (!finished.empty()) {
        world.build_orders.erase(
            std::remove_if(world.build_orders.begin(), world.build_orders.end(),
                           [&](const BuildOrder& o) { return o.state == state && o.months_left <= 0; }),
            world.build_orders.end());
        for (const BuildOrder& order : finished) {
            complete(world, order);
        }
    }
    // Unterhalt der Gebäude in den Landesteilen, deren erster Ort dem Staat gehört.
    const Fixed per_thousand = Fixed::from_int(1000);
    const Fixed twelve = Fixed::from_int(12);
    for (std::size_t r = 0; r < world.regions.size(); ++r) {
        if (region_owner(world, static_cast<Index>(r)) != state) {
            continue;
        }
        Fixed population;
        bool any = false;
        for (std::size_t t = 0; t < world.building_types.size(); ++t) {
            const std::int32_t level = world.building_levels[r * world.building_types.size() + t];
            const BuildingType& type = world.building_types[t];
            if (level <= 0 || type.upkeep <= Fixed::zero()) {
                continue;
            }
            if (!any) {
                population = region_population(world, static_cast<Index>(r));
                any = true;
            }
            expenses += type.upkeep * type.cost * population / per_thousand * Fixed::from_int(level) / twelve;
        }
    }
    return expenses;
}

void recruit_workers(World& world) {
    if (!world.building_rules.active) {
        return;
    }
    const std::size_t types = world.building_types.size();
    for (std::size_t r = 0; r < world.regions.size(); ++r) {
        const Region& region = world.regions[r];
        for (std::size_t t = 0; t < types; ++t) {
            const BuildingType& type = world.building_types[t];
            const std::int32_t level = world.building_levels[r * types + t];
            if (level <= 0 || !in_range(world.professions, type.profession) ||
                !in_range(world.professions, type.recruit_from) || type.jobs <= Fixed::zero()) {
                continue;
            }
            const Fixed capacity =
                type.jobs * Fixed::from_int(level) * region_population(world, static_cast<Index>(r));
            Fixed current;
            Fixed source;
            for (const Index li : region.locations) {
                current += heads(world, li, type.profession);
                const Location& loc = world.locations[at(li)];
                if (suits(world, loc, type)) {
                    for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
                        if (world.pops[at(p)].profession == type.recruit_from) {
                            source += world.pops[at(p)].size();
                        }
                    }
                }
            }
            const Fixed gap = capacity - current;
            if (gap <= Fixed::zero() || source <= Fixed::zero()) {
                continue;
            }
            const Fixed move = Fixed::min(gap, world.building_rules.recruit_rate * source);
            const Fixed f = move / source;
            for (const Index li : region.locations) {
                Location& loc = world.locations[at(li)];
                if (!suits(world, loc, type)) {
                    continue;
                }
                for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
                    PopBlock& from = world.pops[at(p)];
                    if (from.profession != type.recruit_from) {
                        continue;
                    }
                    PopBlock moved = from;
                    moved.profession = type.profession;
                    moved.children = from.children * f;
                    moved.young = from.young * f;
                    moved.working = from.working * f;
                    moved.old = from.old * f;
                    if (moved.size() <= Fixed::zero()) {
                        continue;
                    }
                    from.children -= moved.children;
                    from.young -= moved.young;
                    from.working -= moved.working;
                    from.old -= moved.old;
                    std::vector<PopBlock>& arrivals = world.arrivals[at(li)];
                    const auto same = std::find_if(arrivals.begin(), arrivals.end(), [&](const PopBlock& a) {
                        return a.profession == moved.profession && a.confession == moved.confession &&
                               a.culture == moved.culture;
                    });
                    if (same == arrivals.end()) {
                        arrivals.push_back(moved);
                    } else {
                        const Fixed n_into = same->size();
                        const Fixed n_from = moved.size();
                        const Fixed total = n_into + n_from;
                        same->living_standard =
                            (same->living_standard * n_into + moved.living_standard * n_from) / total;
                        same->literacy = (same->literacy * n_into + moved.literacy * n_from) / total;
                        same->children += moved.children;
                        same->young += moved.young;
                        same->working += moved.working;
                        same->old += moved.old;
                    }
                }
            }
        }
    }
}

} // namespace uhrwerk
