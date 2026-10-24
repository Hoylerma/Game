// Generalstab nach Regel (seit M10, Schritt a4): docs/technik/militaer.md, Abschnitt 5.
//
// Täglich entscheidet der Generalstab für jedes seiner Heere, das seit mindestens einem Tag ruht und
// dessen Staat im Krieg steht: Rückzug in die Hauptstadt vor doppelter Übermacht im Umkreis; Angriff auf
// das nächste gegnerische Heer in Reichweite, wenn die eigenen Heere im Umkreis es um das 1,2-Fache
// übertreffen; sonst Vormarsch auf ein Ziel nach den Kriegszielen (Gebiet, Hauptstadt, Befreiung
// eigener Orte), wenn dort keine Übermacht steht. Die Regeln sind deterministisch; Unterschiede zwischen
// Läufen kommen aus den Schlachten.

#include "military_internal.hpp"
#include "uhrwerk/economy.hpp"
#include "uhrwerk/military.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace uhrwerk::military_internal {

namespace {

bool contains(const std::vector<Index>& list, Index value) {
    return std::find(list.begin(), list.end(), value) != list.end();
}

Fixed km(const World& world, Index a, Index b) {
    const Location& la = world.locations[at(a)];
    const Location& lb = world.locations[at(b)];
    return distance_km(la.longitude, la.latitude, lb.longitude, lb.latitude);
}

bool in_unit(const World& world, Index location, Index unit) {
    for (Index u = world.locations[at(location)].admin; in_range(world.admin_units, u);
         u = world.admin_units[at(u)].parent) {
        if (u == unit) {
            return true;
        }
    }
    return false;
}

/// Soldaten der eigenen Seite (enemies false) oder der Gegner von state im Umkreis eines Ortes.
Fixed strength_near(const World& world, Index location, Index state, bool enemies, Fixed radius) {
    Fixed total;
    for (const Army& a : world.armies) {
        if (!engaged(a) || km(world, a.location, location) > radius) {
            continue;
        }
        if (enemies ? at_war(world, state, a.state) : friendly(world, state, a.state)) {
            total += a.soldiers;
        }
    }
    return total;
}

/// Ziel des Vormarschs: ein Ort der Gegenseite, den die eigene Seite noch nicht hält – zuerst in einem
/// Gebiet der Kriegsziele, dann die Hauptstadt der gegnerischen Hauptpartei, dann der nächste; eine Seite
/// ohne Kriegsziele, die verteidigt, befreit nur eigene Orte. Je Stufe der nächste Ort.
Index objective(const World& world, const War& war, bool attacker, Index from) {
    const std::vector<Index>& own = attacker ? war.attackers : war.defenders;
    const std::vector<Index>& enemies = attacker ? war.defenders : war.attackers;
    const bool goals = std::any_of(war.goals.begin(), war.goals.end(),
                                   [&](const WarGoal& g) { return g.attacker == attacker; });
    const Index capital = world.states[at(enemies.front())].capital;
    Index best = kNone;
    int best_rank = 99;
    Fixed best_km;
    for (std::size_t l = 0; l < world.locations.size(); ++l) {
        const auto location = static_cast<Index>(l);
        const Location& loc = world.locations[l];
        if (!loc.has_data()) {
            continue;
        }
        const Index holder = controller(world, location);
        int rank = 99;
        if (contains(own, loc.owner) && contains(enemies, holder)) {
            rank = 0; // eigene Orte befreien
        } else if (contains(enemies, loc.owner) && !contains(own, holder) && (attacker || goals)) {
            rank = 3;
            for (const WarGoal& g : war.goals) {
                if (g.attacker == attacker && g.kind == WarGoalKind::Territory &&
                    in_unit(world, location, g.param)) {
                    rank = 1;
                }
            }
            if (rank == 3 && location == capital) {
                rank = 2;
            }
        }
        if (rank == 99) {
            continue;
        }
        const Fixed distance = km(world, from, location);
        if (rank < best_rank || (rank == best_rank && distance < best_km)) {
            best = location;
            best_rank = rank;
            best_km = distance;
        }
    }
    return best;
}

} // namespace

void run_staff(World& world) {
    if (world.wars.empty()) {
        return;
    }
    const MilitaryRules& rules = world.military_rules;
    for (std::size_t i = 0; i < world.armies.size(); ++i) {
        const Army& army = world.armies[i];
        if (!army.staff || army.status != ArmyStatus::Rest || army.days_here < 1) {
            continue;
        }
        const auto war = std::find_if(world.wars.begin(), world.wars.end(), [&](const War& w) {
            return contains(w.attackers, army.state) || contains(w.defenders, army.state);
        });
        if (war == world.wars.end()) {
            continue;
        }
        const bool attacker = contains(war->attackers, army.state);
        const Index state = army.state;
        const std::int64_t id = army.id;
        const Index here = army.location;
        const Fixed own = strength_near(world, here, state, false, rules.staff_radius_km);
        const Fixed threat = strength_near(world, here, state, true, rules.staff_radius_km);
        Index target = kNone;
        if (threat > Fixed::zero() && threat >= own * rules.staff_retreat) {
            target = world.states[at(state)].capital; // Rückzug vor doppelter Übermacht
        } else {
            // Nächstes gegnerisches Heer in Reichweite.
            Index enemy = kNone;
            Fixed nearest;
            for (const Army& other : world.armies) {
                if (!engaged(other) || !at_war(world, state, other.state)) {
                    continue;
                }
                const Fixed distance = km(world, here, other.location);
                if (distance <= rules.staff_reach_km && (enemy == kNone || distance < nearest)) {
                    enemy = other.location;
                    nearest = distance;
                }
            }
            if (enemy != kNone &&
                own >= strength_near(world, enemy, state, true, rules.staff_radius_km) * rules.staff_attack) {
                target = enemy;
            } else {
                const Index goal = objective(world, *war, attacker, here);
                if (goal != kNone && own >= strength_near(world, goal, state, true, rules.staff_radius_km) *
                                                rules.staff_attack) {
                    target = goal;
                }
            }
        }
        if (target != kNone && target != here) {
            order_march(world, state, id, target);
        }
    }
}

} // namespace uhrwerk::military_internal
