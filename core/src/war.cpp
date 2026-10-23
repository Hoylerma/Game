// Krieg (seit M10, Schritt a3): docs/technik/militaer.md, Abschnitte 4.1–4.4.
//
// Ein Krieg hat zwei Seiten (Hauptpartei zuerst). Stehen gegnerische Heere im selben Ort, kämpfen sie
// täglich, bis eine Seite bricht und sich zurückzieht. Ein gegnerischer Ort ohne Verteidiger wird nach
// einem vollen Tag besetzt, eine Festung erst nach der Belagerung. Kriegsziele, Kriegspunkte und
// Frieden kommen mit Schritt a4.

#include "military_internal.hpp"
#include "uhrwerk/diplomacy.hpp"
#include "uhrwerk/military.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace uhrwerk {

using namespace military_internal;

namespace {

bool contains(const std::vector<Index>& list, Index value) {
    return std::find(list.begin(), list.end(), value) != list.end();
}

const std::string& name_of(const World& world, Index state) {
    return world.states[at(state)].short_name;
}

/// „A“, „A und B“, „A, B und C“.
std::string join(const std::vector<std::string>& names) {
    std::string out;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i > 0) {
            out += i + 1 == names.size() ? " und " : ", ";
        }
        out += names[i];
    }
    return out;
}

bool has_locations(const World& world, Index state) {
    return std::any_of(world.locations.begin(), world.locations.end(),
                       [&](const Location& l) { return l.owner == state; });
}

/// Stehen a und b in einem Krieg auf derselben Seite?
bool comrades(const World& world, Index a, Index b) {
    return std::any_of(world.wars.begin(), world.wars.end(), [&](const War& w) {
        return (contains(w.attackers, a) && contains(w.attackers, b)) ||
               (contains(w.defenders, a) && contains(w.defenders, b));
    });
}

Date days_after(const Date& date, std::int32_t days) {
    return from_day_number(to_day_number(date) + days);
}

void add_prisoners(World& world, Index state, Index captor, Fixed soldiers) {
    if (soldiers <= Fixed::zero()) {
        return;
    }
    for (Prisoners& p : world.prisoners) {
        if (p.state == state && p.captor == captor) {
            p.soldiers += soldiers;
            return;
        }
    }
    world.prisoners.push_back(Prisoners{state, captor, soldiers});
}

/// Verluste eines Heeres in der Schlacht: Gefallene mindern die Bevölkerung, ein Teil der Verwundeten
/// kehrt nach recovery_days zurück, die übrigen sind Invaliden (Abschnitt 4.2).
void casualties(World& world, const Army& army, Fixed losses) {
    const MilitaryRules& rules = world.military_rules;
    const Fixed dead = losses * rules.dead_share;
    add_war_dead(world, army.state, dead);
    const Fixed returning = (losses - dead) * rules.wounded_return;
    if (returning <= Fixed::zero()) {
        return;
    }
    const Date back = days_after(world.date, rules.recovery_days);
    for (Convalescents& c : world.convalescents) {
        if (c.army == army.id && c.returns == back) {
            c.soldiers += returning;
            return;
        }
    }
    world.convalescents.push_back(Convalescents{army.id, army.state, returning, back});
}

/// Geländefaktor des Ortes (wie beim Marsch): schmälert die Front und stärkt den Verteidiger.
Fixed terrain_factor(const World& world, Index location) {
    const Index terrain = world.locations[at(location)].terrain;
    const std::vector<Fixed>& factors = world.military_rules.terrain_march;
    return in_range(factors, terrain) ? factors[at(terrain)] : Fixed::one();
}

/// Kampfkraft eines Heeres mit dem eingesetzten Anteil (Abschnitt 4.2).
Fixed strength(const World& world, const Army& army, Fixed deployed) {
    const Fixed firepower = world.states[at(army.state)].army.firepower;
    return (army.soldiers * firepower + army.guns * world.military_rules.gun_value) * deployed * army.morale;
}

/// Rückzug in einen Nachbarort: zugänglich, ohne Gegner, bevorzugt von der eigenen Seite gehalten, dann
/// der nächste. kNone, wenn es keinen gibt (das Heer ergibt sich).
Index retreat_target(const World& world, const Army& army) {
    Index best = kNone;
    bool best_own = false;
    Fixed best_days;
    for (const Neighbor& n : world.neighbors[at(army.location)]) {
        if (!accessible(world, army.state, n.location) || hostile_army_at(world, army.state, n.location)) {
            continue;
        }
        const Index holder = controller(world, n.location);
        const bool own = holder != kNone && friendly(world, army.state, holder);
        const Fixed days = march_days(world, n);
        if (best == kNone || (own && !best_own) || (own == best_own && days < best_days)) {
            best = n.location;
            best_own = own;
            best_days = days;
        }
    }
    return best;
}

struct Sides {
    std::vector<std::size_t> attackers; // Indizes in world.armies
    std::vector<std::size_t> defenders;
};

/// Seiten im Ort: Verteidiger ist, wer zum Herrn des Ortes hält, Angreifer, wer mit ihm im Krieg ist.
Sides sides_at(const World& world, Index location, const std::vector<std::size_t>& armies) {
    Sides sides;
    const Index holder = controller(world, location);
    if (holder == kNone) {
        return sides;
    }
    for (const std::size_t i : armies) {
        if (at_war(world, world.armies[i].state, holder)) {
            sides.attackers.push_back(i);
        }
    }
    for (const std::size_t i : armies) {
        const Index state = world.armies[i].state;
        if (friendly(world, state, holder) &&
            std::any_of(sides.attackers.begin(), sides.attackers.end(),
                        [&](std::size_t a) { return at_war(world, state, world.armies[a].state); })) {
            sides.defenders.push_back(i);
        }
    }
    return sides;
}

Fixed soldiers_of(const World& world, const std::vector<std::size_t>& side) {
    Fixed total;
    for (const std::size_t i : side) {
        total += world.armies[i].soldiers;
    }
    return total;
}

/// Durchschnittliche Moral einer Seite (nach Soldaten gewichtet).
Fixed morale_of(const World& world, const std::vector<std::size_t>& side) {
    Fixed weighted;
    Fixed total;
    for (const std::size_t i : side) {
        weighted += world.armies[i].morale * world.armies[i].soldiers;
        total += world.armies[i].soldiers;
    }
    return total > Fixed::zero() ? weighted / total : Fixed::zero();
}

/// Verteilt die Verluste einer Seite nach Soldaten auf ihre Heere; Moral sinkt mit dem Anteil.
void apply_losses(World& world, const std::vector<std::size_t>& side, Fixed before, Fixed losses) {
    if (before <= Fixed::zero()) {
        return;
    }
    const Fixed morale_drop = losses / before * world.military_rules.morale_loss;
    for (const std::size_t i : side) {
        Army& army = world.armies[i];
        const Fixed share = losses * army.soldiers / before;
        set_soldiers(world, army, army.soldiers - share);
        army.morale = Fixed::max(Fixed::zero(), army.morale - morale_drop);
        casualties(world, army, share);
    }
}

std::vector<std::string> army_names(const World& world, const std::vector<std::size_t>& side) {
    std::vector<std::string> names;
    for (const std::size_t i : side) {
        names.push_back(world.armies[i].name);
    }
    return names;
}

/// Ende einer Schlacht: Die Verlierer ziehen sich zurück (Verfolgung kostet Gefangene); wer nicht
/// ausweichen kann, ergibt sich. Chronik und Verwüstung (Abschnitt 4.2).
void end_battle(World& world, const Battle& battle, const std::vector<std::size_t>& winners,
                const std::vector<std::size_t>& losers, Fixed winner_losses, Fixed loser_losses,
                std::vector<std::int64_t>& destroyed) {
    const MilitaryRules& rules = world.military_rules;
    std::size_t strongest = winners.front();
    for (const std::size_t i : winners) {
        if (world.armies[i].soldiers > world.armies[strongest].soldiers) {
            strongest = i;
        }
        world.armies[i].status = ArmyStatus::Rest;
    }
    const Index captor = world.armies[strongest].state;
    const std::vector<std::string> winner_names = army_names(world, winners);
    const std::vector<std::string> loser_names = army_names(world, losers);
    std::vector<std::string> surrendered;
    Fixed lost = loser_losses;
    for (const std::size_t i : losers) {
        Army& army = world.armies[i];
        const Fixed taken = army.soldiers * rules.pursuit_prisoners;
        set_soldiers(world, army, army.soldiers - taken);
        add_prisoners(world, army.state, captor, taken);
        lost += taken;
        const Index target = army.soldiers >= Fixed::one() ? retreat_target(world, army) : kNone;
        if (target == kNone) {
            add_prisoners(world, army.state, captor, army.soldiers);
            lost += army.soldiers;
            set_soldiers(world, army, Fixed::zero());
            surrendered.push_back(army.name);
            destroyed.push_back(army.id);
            continue;
        }
        stop(army);
        army.route.push_back(RouteStep{target, LegMode::March, kNone});
        army.days_left = march_days(world, *find_neighbor(world, army.location, target));
        army.status = ArmyStatus::Retreat;
    }
    Location& place = world.locations[at(battle.location)];
    place.devastation =
        Fixed::min(Fixed::one(), place.devastation + (winner_losses + lost) / rules.devastation_losses);
    std::string text = "Schlacht bei " + place.name + ": " + join(winner_names) +
                       (winners.size() == 1 ? " schlägt " : " schlagen ") + join(loser_names) +
                       "; Verluste " + grouped(winner_losses.round()) + " gegen " + grouped(lost.round()) +
                       ".";
    if (!surrendered.empty()) {
        text += " " + join(surrendered) + (surrendered.size() == 1 ? " ergibt sich." : " ergeben sich.");
    }
    chronicle(world, captor, std::move(text), battle.location);
}

/// Ein Kampftag in einem Ort (Abschnitt 4.2). true, wenn die Schlacht endet.
bool fight(World& world, Battle& battle, const Sides& sides, Pcg32& rng,
           std::vector<std::int64_t>& destroyed) {
    const MilitaryRules& rules = world.military_rules;
    const Index location = battle.location;
    ++battle.days;
    const Fixed attackers = soldiers_of(world, sides.attackers);
    const Fixed defenders = soldiers_of(world, sides.defenders);
    const Fixed terrain = terrain_factor(world, location);
    const Fixed front = rules.frontage / terrain;
    const auto deployed = [&](Fixed soldiers) { return soldiers > front ? front / soldiers : Fixed::one(); };
    Fixed attack;
    Fixed crossing;
    for (const std::size_t i : sides.attackers) {
        const Army& army = world.armies[i];
        attack += strength(world, army, deployed(attackers));
        const Neighbor* n = army.previous == kNone ? nullptr : find_neighbor(world, army.previous, location);
        if (n != nullptr && river_crossing(world, *n)) {
            crossing += army.soldiers;
        }
    }
    Fixed defense;
    for (const std::size_t i : sides.defenders) {
        defense += strength(world, world.armies[i], deployed(defenders));
    }
    const Fixed one = Fixed::one();
    defense = defense * (one + (terrain - one) * rules.terrain_defense);
    const std::int32_t level = fortress_level(world, location);
    if (level > 0) {
        defense = defense * (one + rules.fortress_defense * Fixed::from_int(level));
    }
    if (attackers > Fixed::zero()) {
        defense = defense * (one + (rules.river_defense - one) * crossing / attackers);
    }
    const Fixed low = Fixed::from_ratio(8, 10);
    const Fixed high = Fixed::from_ratio(12, 10);
    const Fixed attacker_luck = rng.uniform(low, high);
    const Fixed defender_luck = rng.uniform(low, high);
    const Fixed defender_losses = Fixed::min(defenders, rules.loss_rate * attack * attacker_luck);
    const Fixed attacker_losses = Fixed::min(attackers, rules.loss_rate * defense * defender_luck);
    apply_losses(world, sides.attackers, attackers, attacker_losses);
    apply_losses(world, sides.defenders, defenders, defender_losses);
    battle.attacker_losses += attacker_losses;
    battle.defender_losses += defender_losses;

    const Fixed attacker_morale = morale_of(world, sides.attackers);
    const Fixed defender_morale = morale_of(world, sides.defenders);
    const bool attacker_breaks = attacker_morale < rules.break_morale ||
                                 attacker_losses > attackers * rules.break_losses ||
                                 soldiers_of(world, sides.attackers) < one;
    const bool defender_breaks = defender_morale < rules.break_morale ||
                                 defender_losses > defenders * rules.break_losses ||
                                 soldiers_of(world, sides.defenders) < one;
    if (!attacker_breaks && !defender_breaks) {
        return false;
    }
    // Brechen beide, weicht die Seite mit der geringeren Moral, bei Gleichstand der Angreifer.
    const bool attacker_lost = attacker_breaks && (!defender_breaks || attacker_morale <= defender_morale);
    if (attacker_lost) {
        end_battle(world, battle, sides.defenders, sides.attackers, battle.defender_losses,
                   battle.attacker_losses, destroyed);
    } else {
        end_battle(world, battle, sides.attackers, sides.defenders, battle.attacker_losses,
                   battle.defender_losses, destroyed);
    }
    return true;
}

/// Heere je Ort (Indizes in world.armies, aufsteigend), die im Ort stehen.
std::map<Index, std::vector<std::size_t>> armies_by_location(const World& world, bool stationary) {
    std::map<Index, std::vector<std::size_t>> result;
    for (std::size_t i = 0; i < world.armies.size(); ++i) {
        const Army& army = world.armies[i];
        const bool keep = stationary ? (army.status == ArmyStatus::Rest || army.status == ArmyStatus::Siege)
                                     : engaged(army);
        if (keep) {
            result[army.location].push_back(i);
        }
    }
    return result;
}

void remove_armies(World& world, const std::vector<std::int64_t>& ids) {
    if (ids.empty()) {
        return;
    }
    std::erase_if(world.armies,
                  [&](const Army& a) { return std::find(ids.begin(), ids.end(), a.id) != ids.end(); });
}

} // namespace

namespace military_internal {

void run_battles(World& world, Pcg32& rng) {
    if (world.wars.empty() && world.battles.empty()) {
        return;
    }
    std::vector<std::int64_t> destroyed;
    std::vector<Index> fought;
    for (const auto& [location, armies] : armies_by_location(world, false)) {
        const Sides sides = sides_at(world, location, armies);
        if (sides.attackers.empty() || sides.defenders.empty()) {
            continue;
        }
        auto it = std::find_if(world.battles.begin(), world.battles.end(),
                               [&](const Battle& b) { return b.location == location; });
        if (it == world.battles.end()) {
            world.battles.push_back(Battle{location, world.date, 0, Fixed{}, Fixed{}});
            it = world.battles.end() - 1;
        }
        for (const std::vector<std::size_t>* side : {&sides.attackers, &sides.defenders}) {
            for (const std::size_t i : *side) {
                stop(world.armies[i]);
                world.armies[i].status = ArmyStatus::Battle;
            }
        }
        fought.push_back(location);
        Battle battle = *it;
        if (fight(world, battle, sides, rng, destroyed)) {
            world.battles.erase(world.battles.begin() + (it - world.battles.begin()));
        } else {
            *it = battle;
        }
    }
    // Schlachten ohne Gegner mehr im Ort enden ohne Ergebnis; ihre Heere stehen wieder in Ruhe.
    std::erase_if(world.battles, [&](const Battle& b) { return !contains(fought, b.location); });
    for (Army& army : world.armies) {
        if (army.status == ArmyStatus::Battle &&
            !std::any_of(world.battles.begin(), world.battles.end(),
                         [&](const Battle& b) { return b.location == army.location; })) {
            army.status = ArmyStatus::Rest;
        }
    }
    remove_armies(world, destroyed);
}

void run_occupation(World& world) {
    if (world.wars.empty() && world.sieges.empty()) {
        return;
    }
    const MilitaryRules& rules = world.military_rules;
    const std::map<Index, std::vector<std::size_t>> engaged_here = armies_by_location(world, false);
    std::vector<Index> besieged;
    for (const auto& [location, armies] : armies_by_location(world, true)) {
        if (std::any_of(world.battles.begin(), world.battles.end(),
                        [&](const Battle& b) { return b.location == location; })) {
            continue;
        }
        const Index holder = controller(world, location);
        if (holder == kNone) {
            continue;
        }
        std::vector<std::size_t> takers;
        for (const std::size_t i : armies) {
            if (world.armies[i].days_here >= 1 && at_war(world, world.armies[i].state, holder)) {
                takers.push_back(i);
            }
        }
        const auto present = engaged_here.find(location);
        const bool defended = present != engaged_here.end() &&
                              std::any_of(present->second.begin(), present->second.end(), [&](std::size_t i) {
                                  return friendly(world, world.armies[i].state, holder);
                              });
        if (takers.empty() || defended) {
            continue;
        }
        const Index taker = world.armies[takers.front()].state;
        const Location& place = world.locations[at(location)];
        // Ein Verbündeter des Besitzers befreit den Ort für ihn.
        const Index next = friendly(world, taker, place.owner) ? kNone : taker;
        const std::int32_t level = fortress_level(world, location);
        if (level > 0) {
            auto siege = std::find_if(world.sieges.begin(), world.sieges.end(),
                                      [&](const Siege& s) { return s.location == location; });
            if (siege == world.sieges.end()) {
                world.sieges.push_back(Siege{location, taker, Fixed{}, world.date});
                siege = world.sieges.end() - 1;
            }
            Fixed besiegers;
            for (const std::size_t i : takers) {
                besiegers += world.armies[i].soldiers + world.armies[i].guns * rules.gun_value;
                world.armies[i].status = ArmyStatus::Siege;
            }
            const Fixed garrison = rules.fortress_garrison * Fixed::from_int(level);
            Fixed required = rules.siege_min_days;
            if (besiegers > Fixed::zero()) {
                required =
                    Fixed::max(required, rules.siege_days * Fixed::from_int(level) * garrison / besiegers);
            }
            // Aufgerundet, damit die Festung nach required Tagen fällt und nicht am Abrunden scheitert.
            Fixed step = Fixed::one() / required;
            if (step * required < Fixed::one()) {
                step = Fixed::from_raw(step.raw() + 1);
            }
            siege->progress = Fixed::min(Fixed::one(), siege->progress + step);
            besieged.push_back(location);
            if (siege->progress < Fixed::one()) {
                continue;
            }
            for (const std::size_t i : takers) {
                world.armies[i].status = ArmyStatus::Rest;
            }
            std::string name = "Festung";
            for (const Fortress& f : world.fortresses) {
                if (f.location == location) {
                    name = f.name;
                }
            }
            chronicle(world, taker, "Die " + name + " ergibt sich " + name_of(world, taker) + ".", location);
        }
        if (world.control[at(location)] == next) {
            continue;
        }
        world.control[at(location)] = next;
        if (place.owner != kNone && world.states[at(place.owner)].capital == location) {
            chronicle(world, taker,
                      next == kNone ? name_of(world, taker) + " befreit " + place.name +
                                          ", die Hauptstadt von " + name_of(world, place.owner) + "."
                                    : name_of(world, taker) + " besetzt " + place.name +
                                          ", die Hauptstadt von " + name_of(world, place.owner) + ".",
                      location);
        }
    }
    // Belagerungen ohne Belagerer heute (abgezogen, gefallen, Schlacht) enden; der Fortschritt verfällt.
    std::erase_if(world.sieges, [&](const Siege& s) {
        return !contains(besieged, s.location) || s.progress >= Fixed::one();
    });
    for (Army& army : world.armies) {
        if (army.status == ArmyStatus::Siege && !contains(besieged, army.location)) {
            army.status = ArmyStatus::Rest;
        }
    }
}

void return_convalescents(World& world) {
    if (world.convalescents.empty()) {
        return;
    }
    std::vector<Convalescents> waiting;
    for (const Convalescents& c : world.convalescents) {
        if (world.date < c.returns) {
            waiting.push_back(c);
            continue;
        }
        Army* army = find_army(world, c.army);
        if (army == nullptr) {
            // Das Heer besteht nicht mehr: zum ersten Heer des Staates, sonst nach Hause.
            for (Army& other : world.armies) {
                if (other.state == c.state) {
                    army = &other;
                    break;
                }
            }
        }
        if (army != nullptr) {
            set_soldiers(world, *army, army->soldiers + c.soldiers);
        }
    }
    world.convalescents = std::move(waiting);
}

void apply_war_dead(World& world) {
    for (std::size_t s = 0; s < world.war_dead.size() && s < world.states.size(); ++s) {
        const Fixed dead = world.war_dead[s];
        if (dead <= Fixed::zero()) {
            continue;
        }
        world.war_dead[s] = Fixed::zero();
        const auto state = static_cast<Index>(s);
        Fixed eligible;
        for (const Location& l : world.locations) {
            if (l.owner != state) {
                continue;
            }
            for (Index p = l.first_pop; p < l.first_pop + l.pop_count; ++p) {
                eligible += world.pops[at(p)].young + world.pops[at(p)].working;
            }
        }
        if (eligible <= Fixed::zero()) {
            continue;
        }
        const Fixed fraction = Fixed::min(Fixed::one(), dead / eligible);
        for (std::size_t l = 0; l < world.locations.size(); ++l) {
            const Location& loc = world.locations[l];
            if (loc.owner != state) {
                continue;
            }
            Fixed lost;
            for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
                PopBlock& pop = world.pops[at(p)];
                const Fixed young = pop.young * fraction;
                const Fixed working = pop.working * fraction;
                pop.young -= young;
                pop.working -= working;
                lost += young + working;
            }
            if (l < world.balance_current.size()) {
                world.balance_current[l].deaths += lost;
            }
        }
    }
}

} // namespace military_internal

bool at_war(const World& world, Index a, Index b) {
    return std::any_of(world.wars.begin(), world.wars.end(), [&](const War& w) {
        return (contains(w.attackers, a) && contains(w.defenders, b)) ||
               (contains(w.defenders, a) && contains(w.attackers, b));
    });
}

bool in_war(const World& world, Index state) {
    return std::any_of(world.wars.begin(), world.wars.end(), [&](const War& w) {
        return contains(w.attackers, state) || contains(w.defenders, state);
    });
}

bool friendly(const World& world, Index a, Index b) {
    if (!in_range(world.states, a) || !in_range(world.states, b)) {
        return false;
    }
    return a == b || allied(world, a, b) || comrades(world, a, b);
}

Index controller(const World& world, Index location) {
    if (!in_range(world.locations, location)) {
        return kNone;
    }
    if (in_range(world.control, location) && world.control[at(location)] != kNone) {
        return world.control[at(location)];
    }
    return world.locations[at(location)].owner;
}

std::int32_t fortress_level(const World& world, Index location) {
    for (const Fortress& f : world.fortresses) {
        if (f.location == location) {
            return f.level;
        }
    }
    return 0;
}

void war_sides(const World& world, Index attacker, Index target, std::vector<Index>& attackers,
               std::vector<Index>& defenders) {
    attackers.assign(1, attacker);
    defenders.assign(1, target);
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        const auto state = static_cast<Index>(s);
        if (state == attacker || state == target || !has_locations(world, state)) {
            continue;
        }
        const bool with_target = allied(world, state, target) || guarantees(world, state, target);
        const bool with_attacker = allied(world, state, attacker);
        if (with_target && !with_attacker) {
            defenders.push_back(state);
        } else if (with_attacker && !with_target) {
            attackers.push_back(state);
        }
    }
}

bool can_declare_war(const World& world, Index state, Index target) {
    return military_active(world) && in_range(world.states, state) && in_range(world.states, target) &&
           state != target && has_locations(world, state) && has_locations(world, target) &&
           !at_war(world, state, target) && !comrades(world, state, target);
}

bool start_war(World& world, std::vector<Index> attackers, std::vector<Index> defenders, WarCause cause) {
    if (!military_active(world)) {
        return false;
    }
    const auto clean = [&](std::vector<Index>& side) {
        std::vector<Index> kept;
        for (const Index s : side) {
            if (in_range(world.states, s) && has_locations(world, s) && !contains(kept, s)) {
                kept.push_back(s);
            }
        }
        side = std::move(kept);
    };
    clean(attackers);
    clean(defenders);
    if (attackers.empty() || defenders.empty() || contains(defenders, attackers.front()) ||
        contains(attackers, defenders.front()) || at_war(world, attackers.front(), defenders.front())) {
        return false;
    }
    // Wer auf beiden Seiten stünde, bleibt neutral.
    const std::vector<Index> both_attackers = attackers;
    std::erase_if(attackers, [&](Index s) { return contains(defenders, s); });
    std::erase_if(defenders, [&](Index s) { return contains(both_attackers, s); });

    War war;
    war.id = world.next_war_id++;
    war.attackers = attackers;
    war.defenders = defenders;
    war.started = world.date;
    war.cause = cause;
    world.wars.push_back(war);
    // Verträge zwischen den Seiten enden.
    std::erase_if(world.treaties, [&](const Treaty& t) {
        return (contains(attackers, t.from) && contains(defenders, t.to)) ||
               (contains(defenders, t.from) && contains(attackers, t.to));
    });

    const Index attacker = attackers.front();
    const Index defender = defenders.front();
    std::string text;
    switch (cause) {
    case WarCause::Declaration:
        text = name_of(world, attacker) + " erklärt " + name_of(world, defender) + " den Krieg.";
        break;
    case WarCause::Crisis:
        text = "Krieg zwischen " + name_of(world, attacker) + " und " + name_of(world, defender) + ".";
        break;
    case WarCause::Execution:
        text = "Bundesexekution gegen " + name_of(world, defender) + ".";
        break;
    }
    for (const auto& [main, side] : {std::pair{attacker, &attackers}, std::pair{defender, &defenders}}) {
        if (side->size() < 2) {
            continue;
        }
        std::vector<std::string> names;
        for (std::size_t i = 1; i < side->size(); ++i) {
            names.push_back(name_of(world, (*side)[i]));
        }
        text += " An der Seite von " + name_of(world, main) + ": " + join(names) + ".";
    }
    chronicle(world, attacker, std::move(text));
    // Alle Kriegführenden machen mobil (wer schon mobil ist, bleibt es auch nach einer Krise).
    for (const std::vector<Index>* side : {&attackers, &defenders}) {
        for (const Index s : *side) {
            order_mobilization(world, s, true);
        }
    }
    return true;
}

bool declare_war(World& world, Index state, Index target) {
    if (!can_declare_war(world, state, target)) {
        return false;
    }
    std::vector<Index> attackers;
    std::vector<Index> defenders;
    war_sides(world, state, target, attackers, defenders);
    // Eine Kriegserklärung kostet Beziehung bei allen Bundesstaaten (Abschnitt 4.1).
    if (diplomacy_active(world)) {
        for (std::size_t s = 0; s < world.states.size(); ++s) {
            const auto other = static_cast<Index>(s);
            if (other != state && federal_state(world, other)) {
                adjust_relation(world, state, other, world.military_rules.declaration_relation);
            }
        }
    }
    return start_war(world, std::move(attackers), std::move(defenders), WarCause::Declaration);
}

} // namespace uhrwerk
