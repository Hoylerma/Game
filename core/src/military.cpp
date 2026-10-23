// Militär (seit M10): docs/technik/militaer.md, Abschnitte 3.1–3.5.

#include "uhrwerk/military.hpp"

#include "uhrwerk/diplomacy.hpp"
#include "uhrwerk/economy.hpp"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <limits>
#include <queue>
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

void chronicle(World& world, Index state, std::string text) {
    ChronicleEntry entry;
    entry.date = world.date;
    entry.text = std::move(text);
    entry.state = state;
    world.chronicle.push_back(std::move(entry));
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

/// Soldaten setzen; die Geschütze folgen nach dem Heerwesen.
void set_soldiers(const World& world, Army& army, Fixed soldiers) {
    army.soldiers = Fixed::max(Fixed::zero(), soldiers);
    army.guns = guns_for(world, army.state, army.soldiers);
}

Fixed location_distance(const World& world, Index a, Index b) {
    const Location& la = world.locations[at(a)];
    const Location& lb = world.locations[at(b)];
    return distance_km(la.longitude, la.latitude, lb.longitude, lb.latitude);
}

bool section_open(const World& world, const RailSection& section) {
    return section.opened <= world.date && in_range(world.locations, section.from) &&
           in_range(world.locations, section.to);
}

/// Eröffneter Bahnabschnitt von einem Ort aus.
struct RailEdge {
    Index to = kNone;
    Index railway = kNone;
    Fixed days; // Fahrt ohne Ein- und Ausladen
};

/// Bahnnetz für die Heere eines Staates: eröffnete Abschnitte mit zwei zugänglichen Endpunkten.
std::vector<std::vector<RailEdge>> rail_graph(const World& world, Index state) {
    std::vector<std::vector<RailEdge>> graph(world.locations.size());
    const Fixed per_day = world.military_rules.rail_km;
    for (std::size_t r = 0; r < world.railways.size(); ++r) {
        for (const RailSection& s : world.railways[r].sections) {
            if (!section_open(world, s) || s.from == s.to || !accessible(world, state, s.from) ||
                !accessible(world, state, s.to)) {
                continue;
            }
            const Fixed days = location_distance(world, s.from, s.to) / per_day;
            graph[at(s.from)].push_back(RailEdge{s.to, static_cast<Index>(r), days});
            graph[at(s.to)].push_back(RailEdge{s.from, static_cast<Index>(r), days});
        }
    }
    return graph;
}

/// Ist ein Ort für state „eigene Seite“ (eigen oder verbündet)?
bool own_side(const World& world, Index state, Index location) {
    if (!in_range(world.locations, location)) {
        return false;
    }
    const Index owner = world.locations[at(location)].owner;
    return owner != kNone && (owner == state || allied(world, state, owner));
}

/// Die ersten Schritte des Weges, die zu einer Bahnfahrt gehören.
std::size_t rail_block(const Army& army) {
    std::size_t n = 0;
    while (n < army.route.size() && army.route[n].mode == LegMode::Rail) {
        ++n;
    }
    return n;
}

/// Beginnt den nächsten Abschnitt des Weges: Marsch zum Nachbarort oder die ganze Bahnfahrt (Warten auf
/// die belegte Strecke, Einladen, Fahrt, Ausladen). false, wenn der Weg gesperrt ist.
bool start_leg(World& world, Army& army) {
    const MilitaryRules& rules = world.military_rules;
    const RouteStep& next = army.route.front();
    if (next.mode == LegMode::March) {
        const Neighbor* neighbor = find_neighbor(world, army.location, next.location);
        if (neighbor == nullptr || !accessible(world, army.state, next.location)) {
            return false;
        }
        army.days_left += march_days(world, *neighbor);
        army.status = ArmyStatus::March;
        return true;
    }
    const std::size_t count = rail_block(army);
    std::int32_t wait = 0;
    Fixed travel;
    Index from = army.location;
    for (std::size_t i = 0; i < count; ++i) {
        const RouteStep& step = army.route[i];
        if (!accessible(world, army.state, step.location) || !in_range(world.rail_backlog, step.railway)) {
            return false;
        }
        wait = std::max(wait, world.rail_backlog[at(step.railway)]);
        travel += location_distance(world, from, step.location) / rules.rail_km;
        from = step.location;
    }
    const std::int32_t loading = loading_days(world, army.soldiers);
    for (std::size_t i = 0; i < count; ++i) {
        std::int32_t& backlog = world.rail_backlog[at(army.route[i].railway)];
        backlog = std::max(backlog, wait + loading);
    }
    army.days_left += Fixed::from_int(wait + loading + rules.unload_days) + travel;
    army.status = ArmyStatus::Rail;
    return true;
}

void stop(Army& army) {
    army.route.clear();
    army.days_left = Fixed::zero();
    army.status = ArmyStatus::Rest;
}

/// Ein Tag Bewegung: Am Ende eines Abschnitts erreicht das Heer den nächsten Ort (nach einer Bahnfahrt
/// das Ende der Fahrt) und beginnt den folgenden; übrige Zeit wird mitgenommen.
void move(World& world, Army& army) {
    army.days_left -= Fixed::one();
    while (army.days_left <= Fixed::zero() && !army.route.empty()) {
        const std::size_t done = army.route.front().mode == LegMode::Rail ? rail_block(army) : 1;
        army.location = army.route[done - 1].location;
        army.route.erase(army.route.begin(), army.route.begin() + static_cast<std::ptrdiff_t>(done));
        if (!army.route.empty() && !start_leg(world, army)) {
            stop(army);
        }
    }
    if (army.route.empty()) {
        stop(army);
    }
}

void mobilize_armies(World& world, Index state) {
    for (Army& army : world.armies) {
        if (army.state == state && army.status == ArmyStatus::Garrison) {
            army.status = ArmyStatus::Rest;
        }
    }
}

/// Mobilmachung und Abrüstung eines Tages (Abschnitt 3.3).
void run_mobilization(World& world) {
    const std::vector<Fixed> shares = garrison_shares(world);
    for (std::size_t s = 0; s < world.mobilizations.size() && s < world.states.size(); ++s) {
        Mobilization& m = world.mobilizations[s];
        if (m.phase == MobilizationPhase::Peace) {
            continue;
        }
        const auto state = static_cast<Index>(s);
        const State& st = world.states[s];
        ++m.days;
        if (m.phase == MobilizationPhase::Mobilized) {
            continue;
        }
        const Fixed peace = peace_strength(world, state);
        if (m.phase == MobilizationPhase::Mobilizing) {
            const std::int32_t days = std::max(1, st.army.mobilization_days);
            const Fixed growth = (st.army.mobilization - Fixed::one()) / Fixed::from_int(days);
            for (Army& army : world.armies) {
                if (army.state == state && army.status != ArmyStatus::Garrison &&
                    in_range(world.garrisons, army.garrison)) {
                    const Fixed share =
                        garrison_soldiers(world.garrisons[at(army.garrison)], peace, shares[s]);
                    set_soldiers(world, army,
                                 Fixed::min(share * st.army.mobilization, army.soldiers + share * growth));
                }
            }
            if (m.days >= days) {
                m.phase = MobilizationPhase::Mobilized;
                m.days = 0;
                chronicle(world, state,
                          st.short_name +
                              " ist mobil: " + std::to_string(state_soldiers(world, state).round()) +
                              " Soldaten unter Waffen.");
            }
            continue;
        }
        // Abrüstung: Die Feldarmeen schrumpfen gleichmäßig auf die Friedensstärke und kehren am Ende in
        // ihre Garnisonen zurück.
        const std::int32_t days = std::max(1, world.military_rules.demobilization_days);
        const bool done = m.days >= days;
        for (Army& army : world.armies) {
            if (army.state != state || army.status == ArmyStatus::Garrison ||
                !in_range(world.garrisons, army.garrison)) {
                continue;
            }
            const Garrison& garrison = world.garrisons[at(army.garrison)];
            const Fixed share = garrison_soldiers(garrison, peace, shares[s]);
            if (done) {
                set_soldiers(world, army, share);
                army.location = garrison.location;
                army.route.clear();
                army.days_left = Fixed::zero();
                army.status = ArmyStatus::Garrison;
            } else if (army.soldiers > share) {
                const Fixed remaining = Fixed::from_int(days - m.days + 1);
                set_soldiers(world, army, army.soldiers - (army.soldiers - share) / remaining);
            }
        }
        if (done) {
            m = Mobilization{};
            chronicle(world, state,
                      st.short_name + " hat abgerüstet; die Truppen stehen wieder in ihren Garnisonen.");
        }
    }
}

/// Versorgung, Zermürbung und Moral eines Tages (Abschnitt 3.5).
void run_supply(World& world) {
    const MilitaryRules& rules = world.military_rules;
    const bool winter = world.date.month == 12 || world.date.month <= 2;
    const Fixed season = winter ? Fixed::from_int(2) : Fixed::one();
    for (Army& army : world.armies) {
        if (army.status == ArmyStatus::Garrison) {
            continue;
        }
        const bool fed = supplied(world, army);
        const Fixed rate = (fed ? rules.attrition : rules.attrition_unsupplied) * season;
        set_soldiers(world, army, army.soldiers - army.soldiers * rate);
        if (!fed) {
            army.morale = Fixed::max(Fixed::zero(), army.morale - rules.morale_unsupplied * season);
        } else if (army.status == ArmyStatus::Rest) {
            army.morale = Fixed::min(Fixed::one(), army.morale + rules.morale_recovery);
        }
    }
}

/// Dijkstra-Knoten: Ort × (zu Fuß, auf der Bahn).
struct Node {
    std::int64_t cost = 0; // Rohwert der Tage
    std::int32_t id = 0;   // 2 × Ort + Art
    friend bool operator>(const Node& a, const Node& b) {
        return a.cost != b.cost ? a.cost > b.cost : a.id > b.id;
    }
};

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
    if (!military_active(world)) {
        return;
    }
    if (world.mobilizations.size() != world.states.size()) {
        world.mobilizations.assign(world.states.size(), Mobilization{});
    }
    if (world.rail_backlog.size() != world.railways.size()) {
        world.rail_backlog.assign(world.railways.size(), 0);
    }
    if (!world.armies.empty()) {
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

Army* find_army(World& world, std::int64_t id) {
    for (Army& army : world.armies) {
        if (army.id == id) {
            return &army;
        }
    }
    return nullptr;
}

const Army* find_army(const World& world, std::int64_t id) {
    for (const Army& army : world.armies) {
        if (army.id == id) {
            return &army;
        }
    }
    return nullptr;
}

bool accessible(const World& world, Index state, Index location) {
    return own_side(world, state, location);
}

Fixed march_days(const World& world, const Neighbor& neighbor) {
    if (neighbor.ferry_days > 0) {
        return Fixed::from_int(neighbor.ferry_days);
    }
    const MilitaryRules& rules = world.military_rules;
    Fixed days = neighbor.distance_km / rules.march_km;
    if (in_range(world.locations, neighbor.location)) {
        const Index terrain = world.locations[at(neighbor.location)].terrain;
        if (in_range(rules.terrain_march, terrain)) {
            days = days * rules.terrain_march[at(terrain)];
        }
    }
    if (river_crossing(world, neighbor)) {
        days += rules.river_days;
    }
    return days;
}

std::int32_t loading_days(const World& world, Fixed soldiers) {
    const Fixed capacity = world.military_rules.rail_capacity;
    if (capacity <= Fixed::zero() || soldiers <= capacity) {
        return 1;
    }
    const Fixed trips = soldiers / capacity;
    const std::int64_t whole = trips.trunc();
    return static_cast<std::int32_t>(Fixed::from_int(whole) < trips ? whole + 1 : whole);
}

bool is_station(const World& world, Index location) {
    for (const Railway& railway : world.railways) {
        for (const RailSection& s : railway.sections) {
            if (section_open(world, s) && (s.from == location || s.to == location)) {
                return true;
            }
        }
    }
    return false;
}

std::optional<Route> find_route(const World& world, const Army& army, Index target) {
    if (!in_range(world.locations, target) || !in_range(world.locations, army.location) ||
        world.neighbors.size() != world.locations.size() || target == army.location ||
        !accessible(world, army.state, target)) {
        return std::nullopt;
    }
    const MilitaryRules& rules = world.military_rules;
    const std::vector<std::vector<RailEdge>> rail = rail_graph(world, army.state);
    const std::size_t nodes = world.locations.size() * 2;
    constexpr std::int64_t kUnreached = std::numeric_limits<std::int64_t>::max();
    std::vector<std::int64_t> cost(nodes, kUnreached);
    std::vector<std::int32_t> previous(nodes, -1);
    std::vector<Index> previous_railway(nodes, kNone);
    std::priority_queue<Node, std::vector<Node>, std::greater<>> open;
    const auto relax = [&](std::int32_t from, std::int32_t to, Fixed days, Index railway) {
        const std::int64_t c = cost[static_cast<std::size_t>(from)] + days.raw();
        if (c < cost[static_cast<std::size_t>(to)]) {
            cost[static_cast<std::size_t>(to)] = c;
            previous[static_cast<std::size_t>(to)] = from;
            previous_railway[static_cast<std::size_t>(to)] = railway;
            open.push(Node{c, to});
        }
    };
    const auto start = static_cast<std::int32_t>(army.location * 2);
    const auto goal = static_cast<std::int32_t>(target * 2);
    const Fixed loading = Fixed::from_int(loading_days(world, army.soldiers));
    const Fixed unloading = Fixed::from_int(rules.unload_days);
    cost[static_cast<std::size_t>(start)] = 0;
    open.push(Node{0, start});
    while (!open.empty()) {
        const Node node = open.top();
        open.pop();
        if (node.cost != cost[static_cast<std::size_t>(node.id)]) {
            continue;
        }
        if (node.id == goal) {
            break;
        }
        const Index location = node.id / 2;
        const bool on_rail = node.id % 2 == 1;
        const std::vector<RailEdge>& edges = rail[at(location)];
        if (!on_rail) {
            for (const Neighbor& n : world.neighbors[at(location)]) {
                if (accessible(world, army.state, n.location)) {
                    relax(node.id, static_cast<std::int32_t>(n.location * 2), march_days(world, n), kNone);
                }
            }
            if (!edges.empty()) {
                relax(node.id, node.id + 1, loading, kNone);
            }
        } else {
            for (const RailEdge& e : edges) {
                relax(node.id, static_cast<std::int32_t>(e.to * 2 + 1), e.days, e.railway);
            }
            relax(node.id, node.id - 1, unloading, kNone);
        }
    }
    if (cost[static_cast<std::size_t>(goal)] == kUnreached) {
        return std::nullopt;
    }
    Route route;
    route.days = Fixed::from_raw(cost[static_cast<std::size_t>(goal)]);
    for (std::int32_t n = goal; n != start; n = previous[static_cast<std::size_t>(n)]) {
        const std::int32_t p = previous[static_cast<std::size_t>(n)];
        if (p / 2 == n / 2) {
            continue; // Einladen oder Ausladen
        }
        const LegMode mode = n % 2 == 1 ? LegMode::Rail : LegMode::March;
        route.steps.push_back(RouteStep{n / 2, mode, previous_railway[static_cast<std::size_t>(n)]});
    }
    std::reverse(route.steps.begin(), route.steps.end());
    return route;
}

bool order_march(World& world, Index state, std::int64_t id, Index target) {
    if (!military_active(world) || !in_range(world.states, state)) {
        return false;
    }
    Army* army = find_army(world, id);
    if (army == nullptr || army->state != state ||
        (army->status != ArmyStatus::Rest && army->status != ArmyStatus::March)) {
        return false;
    }
    if (target == kNone || target == army->location) {
        stop(*army);
        return true;
    }
    std::optional<Route> route = find_route(world, *army, target);
    if (!route) {
        return false;
    }
    // Führt der neue Weg über denselben nächsten Ort, bleibt der zurückgelegte Teil des Abschnitts.
    const bool same_leg = army->status == ArmyStatus::March && !army->route.empty() &&
                          route->steps.front() == army->route.front();
    army->route = std::move(route->steps);
    if (!same_leg) {
        army->days_left = Fixed::zero();
        if (!start_leg(world, *army)) {
            stop(*army);
            return false;
        }
    }
    return true;
}

MobilizationPhase mobilization_phase(const World& world, Index state) {
    return in_range(world.mobilizations, state) ? world.mobilizations[at(state)].phase
                                                : MobilizationPhase::Peace;
}

bool order_mobilization(World& world, Index state, bool mobilize, bool crisis) {
    if (!military_active(world) || !in_range(world.states, state) || !in_range(world.mobilizations, state)) {
        return false;
    }
    Mobilization& m = world.mobilizations[at(state)];
    const std::string& name = world.states[at(state)].short_name;
    const bool armed = m.phase == MobilizationPhase::Mobilizing || m.phase == MobilizationPhase::Mobilized;
    if (mobilize) {
        if (armed) {
            if (!crisis && m.crisis) {
                m.crisis = false; // die Regierung übernimmt die Mobilmachung der Krise
                return true;
            }
            return false;
        }
        m.phase = MobilizationPhase::Mobilizing;
        m.days = 0;
        m.crisis = crisis;
        mobilize_armies(world, state);
        chronicle(world, state, name + " macht mobil.");
        return true;
    }
    if (!armed) {
        return false;
    }
    m.phase = MobilizationPhase::Demobilizing;
    m.days = 0;
    m.crisis = false;
    chronicle(world, state, name + " rüstet ab.");
    return true;
}

void end_crisis_mobilization(World& world, Index state) {
    if (in_range(world.mobilizations, state) && world.mobilizations[at(state)].crisis) {
        order_mobilization(world, state, false);
    }
}

Fixed army_cost_factor(const World& world, Index state) {
    if (mobilization_phase(world, state) == MobilizationPhase::Peace) {
        return Fixed::one();
    }
    const Fixed peace = peace_strength(world, state);
    return peace > Fixed::zero() ? state_soldiers(world, state) / peace : Fixed::one();
}

bool supplied(const World& world, const Army& army) {
    if (own_side(world, army.state, army.location)) {
        return true;
    }
    if (!in_range(world.locations, army.location) || world.neighbors.size() != world.locations.size()) {
        return false;
    }
    std::vector<char> station(world.locations.size(), 0);
    for (const Railway& railway : world.railways) {
        for (const RailSection& s : railway.sections) {
            if (section_open(world, s)) {
                station[at(s.from)] = 1;
                station[at(s.to)] = 1;
            }
        }
    }
    // Dijkstra über Marschtage bis supply_days zu einem Bahnhof der eigenen Seite.
    const Fixed limit = world.military_rules.supply_days;
    std::vector<std::int64_t> cost(world.locations.size(), std::numeric_limits<std::int64_t>::max());
    std::priority_queue<Node, std::vector<Node>, std::greater<>> open;
    cost[at(army.location)] = 0;
    open.push(Node{0, static_cast<std::int32_t>(army.location)});
    while (!open.empty()) {
        const Node node = open.top();
        open.pop();
        if (node.cost != cost[static_cast<std::size_t>(node.id)]) {
            continue;
        }
        if (station[static_cast<std::size_t>(node.id)] != 0 && own_side(world, army.state, node.id)) {
            return true;
        }
        for (const Neighbor& n : world.neighbors[static_cast<std::size_t>(node.id)]) {
            if (world.locations[at(n.location)].owner == kNone) {
                continue;
            }
            const std::int64_t c = node.cost + march_days(world, n).raw();
            if (c <= limit.raw() && c < cost[at(n.location)]) {
                cost[at(n.location)] = c;
                open.push(Node{c, static_cast<std::int32_t>(n.location)});
            }
        }
    }
    return false;
}

void run_military(World& world) {
    if (!military_active(world)) {
        return;
    }
    run_mobilization(world);
    for (Army& army : world.armies) {
        if (army.status == ArmyStatus::March || army.status == ArmyStatus::Rail) {
            move(world, army);
        }
    }
    run_supply(world);
    for (std::int32_t& backlog : world.rail_backlog) {
        backlog = std::max(0, backlog - 1);
    }
}

} // namespace uhrwerk
