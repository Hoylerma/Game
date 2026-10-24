// Generalstab nach Regel (seit M10, Schritt a4): docs/technik/militaer.md, Abschnitt 5.
//
// Täglich entscheidet der Generalstab für jedes seiner Heere, das seit mindestens einem Tag ruht, nicht
// eingeschlossen ist und dessen Staat im Krieg steht: Rückzug vor doppelter Übermacht im Umkreis (oder
// mit gebrochener Moral), aus einer Festung schon vor der Einschließung; Angriff auf das nächste
// gegnerische Heer in Reichweite, das die eigenen Heere im Umkreis nehmen können; sonst Vormarsch auf
// ein Ziel nach den Kriegszielen (Gebiet, Hauptstadt, Befreiung eigener Orte), wenn dort keine Übermacht
// steht; ein Heer, dem die Kraft im Ort fehlt, sammelt sich erst beim stärksten eigenen Heer. Seit b2
// rechnet er mit Kampfkraft (Abschnitt 4.2) statt Soldaten. Die Regeln sind deterministisch;
// Unterschiede zwischen Läufen kommen aus den Schlachten.

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

/// Streitmacht im Umkreis eines Ortes (seit b2): Kampfkraft und Soldaten.
struct Force {
    Fixed power;
    Fixed soldiers;
};

/// Streitmacht der eigenen Seite (enemies false) oder der Gegner von state im Umkreis eines Ortes; die
/// Kampfkraft der Gegner in einer Festung, die sie halten, mal deren Faktor.
Force force_near(const World& world, Index location, Index state, bool enemies, Fixed radius) {
    Force total;
    for (const Army& a : world.armies) {
        if (!engaged(a) || km(world, a.location, location) > radius) {
            continue;
        }
        if (enemies ? at_war(world, state, a.state) : friendly(world, state, a.state)) {
            Fixed power = combat_power(world, a);
            const std::int32_t level = fortress_level(world, a.location);
            if (enemies && level > 0 && at_war(world, state, controller(world, a.location))) {
                power =
                    power * (Fixed::one() + world.military_rules.fortress_defense * Fixed::from_int(level));
            }
            total.power += power;
            total.soldiers += a.soldiers;
        }
    }
    return total;
}

/// Kann die Streitmacht own den Ort nehmen (seit b2)? Im Sturm, wenn ihre Kampfkraft die der Gegner im
/// Umkreis um generalstab_angriff übertrifft; eine Festung der Gegner auch durch Einschließung, wenn ihre
/// Soldaten die der Gegner um so viel übertreffen.
bool can_take(const World& world, Index state, const Force& own, Index location) {
    const MilitaryRules& rules = world.military_rules;
    const Force enemy = force_near(world, location, state, true, rules.staff_radius_km);
    if (own.power >= enemy.power * rules.staff_attack) {
        return true;
    }
    return fortress_level(world, location) > 0 && at_war(world, state, controller(world, location)) &&
           own.soldiers >= enemy.soldiers * rules.staff_attack;
}

/// Ziel des Vormarschs: ein Ort der Gegenseite, den die eigene Seite noch nicht hält – zuerst in einem
/// Gebiet der Kriegsziele, dann die Hauptstadt der gegnerischen Hauptpartei, dann der nächste; eine Seite
/// ohne Kriegsziele, die verteidigt, befreit nur eigene Orte. Je Stufe der nächste Ort, an dem keine
/// Übermacht steht (seit b2; vorher nur der nächste, auch wenn ihn eine Übermacht deckte). Seit b2 nur
/// Orte in Reichweite; liegt keiner darin, der nächste überhaupt (Vormarsch an die Front).
Index objective(const World& world, const War& war, bool attacker, Index from, Index state,
                const Force& own) {
    const MilitaryRules& rules = world.military_rules;
    const std::vector<Index>& mine = attacker ? war.attackers : war.defenders;
    const std::vector<Index>& enemies = attacker ? war.defenders : war.attackers;
    const bool goals = std::any_of(war.goals.begin(), war.goals.end(),
                                   [&](const WarGoal& g) { return g.attacker == attacker; });
    const Index capital = world.states[at(enemies.front())].capital;
    struct Candidate {
        int rank;
        Fixed distance;
        Index location;
    };
    std::vector<Candidate> candidates;
    for (std::size_t l = 0; l < world.locations.size(); ++l) {
        const auto location = static_cast<Index>(l);
        const Location& loc = world.locations[l];
        if (!loc.has_data()) {
            continue;
        }
        const Index holder = controller(world, location);
        int rank = 99;
        if (contains(mine, loc.owner) && contains(enemies, holder)) {
            rank = 0; // eigene Orte befreien
        } else if (contains(enemies, loc.owner) && !contains(mine, holder) && (attacker || goals)) {
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
        if (rank != 99) {
            candidates.push_back({rank, km(world, from, location), location});
        }
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        if (a.rank != b.rank) {
            return a.rank < b.rank;
        }
        if (a.distance != b.distance) {
            return a.distance < b.distance;
        }
        return a.location < b.location;
    });
    const auto open = [&](const Candidate& c) { return can_take(world, state, own, c.location); };
    Index front = kNone;
    Fixed front_km;
    for (const Candidate& c : candidates) {
        if (c.distance <= rules.staff_reach_km) {
            if (open(c)) {
                return c.location;
            }
        } else if ((front == kNone || c.distance < front_km) && open(c)) {
            front = c.location;
            front_km = c.distance;
        }
    }
    const bool any_in_reach = std::any_of(candidates.begin(), candidates.end(), [&](const Candidate& c) {
        return c.distance <= rules.staff_reach_km;
    });
    return any_in_reach ? kNone : front;
}

/// Nächste eigene Festung in Reichweite mit höherer Stufe als level, die weiter vom nächsten Gegner liegt
/// als from; kNone ohne (seit b2).
Index safer_fortress(const World& world, Index state, Index from, std::int32_t level) {
    const MilitaryRules& rules = world.military_rules;
    Index enemy_at = kNone;
    Fixed enemy_km;
    for (const Army& other : world.armies) {
        if (engaged(other) && at_war(world, state, other.state)) {
            const Fixed distance = km(world, from, other.location);
            if (enemy_at == kNone || distance < enemy_km) {
                enemy_at = other.location;
                enemy_km = distance;
            }
        }
    }
    Index best = kNone;
    Fixed best_km;
    for (const Fortress& f : world.fortresses) {
        if (f.location == from || f.level <= level ||
            !friendly(world, state, controller(world, f.location)) ||
            (enemy_at != kNone && km(world, enemy_at, f.location) <= enemy_km)) {
            continue;
        }
        const Fixed distance = km(world, from, f.location);
        if (distance <= rules.staff_reach_km && (best == kNone || distance < best_km)) {
            best = f.location;
            best_km = distance;
        }
    }
    return best;
}

/// Fordert die Gegenseite den Ort als Kriegsziel (Gebiet oder Annexion seines Besitzers)? Seit b2.
bool enemy_goal(const World& world, const War& war, bool attacker, Index location) {
    return std::any_of(war.goals.begin(), war.goals.end(), [&](const WarGoal& g) {
        return g.attacker != attacker &&
               ((g.kind == WarGoalKind::Territory && in_unit(world, location, g.param)) ||
                (g.kind == WarGoalKind::Annexation && g.param == world.locations[at(location)].owner));
    });
}

} // namespace

void run_staff(World& world) {
    if (world.wars.empty()) {
        return;
    }
    const MilitaryRules& rules = world.military_rules;
    for (std::size_t i = 0; i < world.armies.size(); ++i) {
        const Army& army = world.armies[i];
        if (!army.staff || army.status != ArmyStatus::Rest || army.days_here < 1 ||
            std::any_of(world.sieges.begin(), world.sieges.end(),
                        [&](const Siege& s) { return s.location == army.location; })) {
            continue; // seit b2: ein eingeschlossenes Heer bleibt
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
        const Force own = force_near(world, here, state, false, rules.staff_radius_km);
        const Force threat = force_near(world, here, state, true, rules.staff_radius_km);
        Index target = kNone;
        const std::int32_t level_here =
            friendly(world, state, controller(world, here)) ? fortress_level(world, here) : 0;
        // Rückzug vor doppelter Übermacht, seit b2 auch mit gebrochener Moral und den Gegnern im Umkreis,
        // aus einer eigenen Festung schon vor der Übermacht, die das Heer einschlösse.
        const bool pressed =
            threat.power > Fixed::zero() &&
            (threat.power >= own.power * rules.staff_retreat || army.morale < rules.staff_morale);
        const bool investable = level_here > 0 && threat.soldiers > Fixed::zero() &&
                                threat.soldiers >= own.soldiers * rules.staff_attack;
        if (pressed || investable) {
            // Seit b2: in die nächste stärkere eigene Festung in Reichweite, die weiter vom Gegner liegt
            // (1864: vom Danewerk nach Düppel); sonst hält das Heer eine eigene Festung in einem Gebiet, das
            // der Gegner fordert, oder geht zum stärksten eigenen Heer jenseits des Umkreises in Reichweite,
            // zuletzt in die Hauptstadt.
            target = safer_fortress(world, state, here, level_here);
            if (target == kNone) {
                if (level_here > 0 && enemy_goal(world, *war, attacker, here)) {
                    continue;
                }
                target = world.states[at(state)].capital;
                Fixed strongest;
                for (const Army& other : world.armies) {
                    if (!engaged(other) || !friendly(world, state, other.state)) {
                        continue;
                    }
                    const Fixed distance = km(world, here, other.location);
                    if (distance > rules.staff_radius_km && distance <= rules.staff_reach_km &&
                        other.soldiers > strongest) {
                        target = other.location;
                        strongest = other.soldiers;
                    }
                }
            }
        } else if (army.morale < rules.staff_morale) {
            continue; // seit b2: erst erholen
        } else {
            // Seit b2: das nächste gegnerische Heer in Reichweite, das die eigenen Heere im Umkreis nehmen
            // können, sofern keine Übermacht näher steht; steht in Reichweite eine Übermacht, rückt das Heer
            // nicht auf ein Ziel vor, sondern wartet.
            Index enemy = kNone;
            Fixed nearest;
            bool superior = false;
            Fixed superior_km;
            for (const Army& other : world.armies) {
                if (!engaged(other) || !at_war(world, state, other.state)) {
                    continue;
                }
                const Fixed distance = km(world, here, other.location);
                if (distance > rules.staff_reach_km) {
                    continue;
                }
                if (!can_take(world, state, own, other.location)) {
                    if (!superior || distance < superior_km) {
                        superior_km = distance;
                    }
                    superior = true;
                } else if (enemy == kNone || distance < nearest) {
                    enemy = other.location;
                    nearest = distance;
                }
            }
            if (enemy != kNone && (!superior || nearest < superior_km)) {
                target = enemy;
            } else if (!superior) {
                target = objective(world, *war, attacker, here, state, own);
            }
            // Seit b2: Sammeln. Kann die Streitmacht im Ort das Ziel nicht nehmen oder wartet das Heer vor
            // einer Übermacht, stößt es zum stärksten eigenen Heer in Reichweite, das stärker ist als es
            // selbst; das stärkste wartet.
            if ((target == kNone && superior) ||
                (target != kNone &&
                 !can_take(world, state, force_near(world, here, state, false, Fixed::zero()), target))) {
                Index rally = kNone;
                Fixed most = army.soldiers;
                for (const Army& other : world.armies) {
                    if (other.id != id && engaged(other) && friendly(world, state, other.state) &&
                        other.location != here && km(world, here, other.location) <= rules.staff_reach_km &&
                        (other.soldiers > most || (other.soldiers == most && other.id < id))) {
                        rally = other.location;
                        most = other.soldiers;
                    }
                }
                target = rally;
            }
        }
        if (target != kNone && target != here) {
            order_march(world, state, id, target);
        }
    }
}

} // namespace uhrwerk::military_internal
