// Krieg (seit M10, Schritt a3): docs/technik/militaer.md, Abschnitte 4.1–4.4.
//
// Ein Krieg hat zwei Seiten (Hauptpartei zuerst). Stehen gegnerische Heere im selben Ort, kämpfen sie
// täglich, bis eine Seite bricht und sich zurückzieht. Ein gegnerischer Ort ohne Verteidiger wird nach
// einem vollen Tag besetzt, eine Festung erst nach der Belagerung. Seit Schritt a4: Kriegsziele,
// Kriegspunkte, Kriegsmüdigkeit und Frieden mit Besitzwechsel und Untergang (Abschnitt 4.5).

#include "military_internal.hpp"
#include "uhrwerk/customs.hpp"
#include "uhrwerk/date.hpp"
#include "uhrwerk/diplomacy.hpp"
#include "uhrwerk/military.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <optional>
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

/// Bilanz für die Kriegspunkte (seit a4): by fügt on Verluste zu und vernichtet ein Heer mit army
/// Soldaten (null: keines); die Bewegung setzt den Stillstand zurück. Seit b2 zählt ein vernichtetes Heer
/// nur, wenn es mindestens punkte_heer_anteil der Kriegsstärke seiner Seite hatte.
void credit(World& world, Index by, Index on, Fixed inflicted, Fixed army) {
    for (War& w : world.wars) {
        const bool attacking = contains(w.attackers, by) && contains(w.defenders, on);
        if (!attacking && !(contains(w.defenders, by) && contains(w.attackers, on))) {
            continue;
        }
        (attacking ? w.attacker_inflicted : w.defender_inflicted) += inflicted;
        const Fixed strength = attacking ? w.defender_strength : w.attacker_strength;
        if (army > Fixed::zero() && army >= strength * world.military_rules.points_army_share) {
            ++(attacking ? w.attacker_destroyed : w.defender_destroyed);
        }
        w.last_change = world.date;
        return;
    }
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
    // Führung des Befehlshabers (seit b1): 1 ohne Befehlshaber, dann ist der Faktor genau 1.
    const Fixed command = Fixed::one() + world.military_rules.leadership * (army.leadership - Fixed::one());
    return (army.soldiers * firepower + army.guns * world.military_rules.gun_value) * deployed * army.morale *
           command;
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
    // Seit b2: Der Sieg gibt den Siegern Moral zurück.
    for (const std::size_t i : winners) {
        world.armies[i].morale =
            Fixed::min(Fixed::one(), world.armies[i].morale + world.military_rules.victory_morale);
    }
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
            credit(world, captor, army.state, taken + army.soldiers, taken + army.soldiers);
            set_soldiers(world, army, Fixed::zero());
            surrendered.push_back(army.name);
            destroyed.push_back(army.id);
            continue;
        }
        credit(world, captor, army.state, taken, Fixed::zero());
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
    // Beendete Schlacht im Krieg zwischen den Hauptbeteiligten (seit a5).
    const Index loser_state = world.armies[losers.front()].state;
    for (War& w : world.wars) {
        if ((contains(w.attackers, captor) && contains(w.defenders, loser_state)) ||
            (contains(w.defenders, captor) && contains(w.attackers, loser_state))) {
            ++w.battles;
            break;
        }
    }
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
    const Fixed cover = one + rules.fortress_defense * Fixed::from_int(level);
    if (level > 0) {
        defense = defense * cover;
    }
    if (attackers > Fixed::zero()) {
        defense = defense * (one + (rules.river_defense - one) * crossing / attackers);
    }
    const Fixed low = Fixed::from_ratio(8, 10);
    const Fixed high = Fixed::from_ratio(12, 10);
    const Fixed attacker_luck = rng.uniform(low, high);
    const Fixed defender_luck = rng.uniform(low, high);
    // Seit b2 schützt die Festung die Verteidiger auch: ihre Verluste ÷ denselben Faktor.
    const Fixed defender_losses = Fixed::min(defenders, rules.loss_rate * attack * attacker_luck / cover);
    const Fixed attacker_losses = Fixed::min(attackers, rules.loss_rate * defense * defender_luck);
    apply_losses(world, sides.attackers, attackers, attacker_losses);
    apply_losses(world, sides.defenders, defenders, defender_losses);
    battle.attacker_losses += attacker_losses;
    battle.defender_losses += defender_losses;
    const Index attacker_state = world.armies[sides.attackers.front()].state;
    const Index defender_state = world.armies[sides.defenders.front()].state;
    credit(world, attacker_state, defender_state, defender_losses, Fixed::zero());
    credit(world, defender_state, attacker_state, attacker_losses, Fixed::zero());

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

const std::vector<Index>& side_of(const War& war, bool attacker) {
    return attacker ? war.attackers : war.defenders;
}

/// Liegt der Ort in der Verwaltungseinheit (auch über übergeordnete Einheiten)?
bool in_unit(const World& world, Index location, Index unit) {
    for (Index u = world.locations[at(location)].admin; in_range(world.admin_units, u);
         u = world.admin_units[at(u)].parent) {
        if (u == unit) {
            return true;
        }
    }
    return false;
}

/// Bevölkerung der Orte, die Staaten der Seite besitzen; dazu, wie viel davon die andere Seite hält.
struct Holdings {
    Fixed population;
    Fixed occupied;
};

Holdings holdings(const World& world, const std::vector<Index>& owners, const std::vector<Index>& holders) {
    Holdings h;
    for (std::size_t l = 0; l < world.locations.size(); ++l) {
        const Location& loc = world.locations[l];
        if (!contains(owners, loc.owner)) {
            continue;
        }
        const Fixed people = world.location_population(static_cast<Index>(l));
        h.population += people;
        if (contains(holders, controller(world, static_cast<Index>(l)))) {
            h.occupied += people;
        }
    }
    // Das Hinterland außerhalb der Karte (seit b1) zählt mit und kann nicht besetzt werden.
    for (const Hinterland& hinterland : world.hinterlands) {
        if (contains(owners, hinterland.state)) {
            h.population += hinterland.population;
        }
    }
    return h;
}

std::string goal_text(const World& world, const War& war, const WarGoal& goal) {
    const Index enemy = side_of(war, !goal.attacker).front();
    switch (goal.kind) {
    case WarGoalKind::Territory:
        return in_range(world.admin_units, goal.param) ? world.admin_units[at(goal.param)].name : "Gebiet";
    case WarGoalKind::Annexation:
        return "Annexion von " + (in_range(world.states, goal.param) ? name_of(world, goal.param) : "?");
    case WarGoalKind::Leadership:
        return "Führung im Bund";
    case WarGoalKind::Customs:
        return "Beitritt von " + name_of(world, enemy) + " zur Zollunion";
    case WarGoalKind::Indemnity:
        return "Entschädigung von " + std::to_string(goal.param) + " Monatseinnahmen";
    }
    return {};
}

/// Kasse um amount verringern; was fehlt, wird Schuld (wie im Haushalt).
void pay(State& state, Fixed amount) {
    state.treasury -= amount;
    if (state.treasury < Fixed::zero()) {
        state.debt -= state.treasury;
        state.treasury = Fixed::zero();
    }
}

/// Ein Staat ohne Orte geht unter (Abschnitt 4.5): Heere aufgelöst, Verträge und Gesandte enden, Kasse und
/// Schulden gehen an den Erwerber, er verlässt den Bund und alle Kriege. Ein Staat mit Hinterland
/// (seit b2) verlässt nur die Karte: Kasse und Schulden gehen mit ihm.
void extinguish(World& world, Index state, Index heir, std::string& text) {
    State& gone = world.states[at(state)];
    State& gains = world.states[at(heir)];
    const bool beyond = std::any_of(world.hinterlands.begin(), world.hinterlands.end(),
                                    [&](const Hinterland& h) { return h.state == state; });
    if (!beyond) {
        gains.treasury += gone.treasury;
        gains.debt += gone.debt;
    }
    gone.treasury = Fixed::zero();
    gone.debt = Fixed::zero();
    gone.capital = kNone;
    gone.customs_union = kNone;
    std::erase_if(world.armies, [&](const Army& a) { return a.state == state; });
    std::erase_if(world.treaties, [&](const Treaty& t) { return t.from == state || t.to == state; });
    std::erase_if(world.envoys, [&](const Envoy& e) { return e.from == state || e.to == state; });
    std::erase_if(world.convalescents, [&](const Convalescents& c) { return c.state == state; });
    std::erase_if(world.prisoners, [&](const Prisoners& p) { return p.state == state || p.captor == state; });
    std::erase_if(world.plays,
                  [&](const DiplomaticPlay& p) { return p.demander == state || p.target == state; });
    if (in_range(world.mobilizations, state)) {
        world.mobilizations[at(state)] = Mobilization{};
    }
    if (in_range(world.left_federation, state)) {
        world.left_federation[at(state)] = 1;
    }
    if (in_range(world.war_dead, state)) {
        world.war_dead[at(state)] = Fixed::zero();
    }
    for (War& w : world.wars) {
        std::erase(w.attackers, state);
        std::erase(w.defenders, state);
    }
    std::erase_if(world.wars, [](const War& w) { return w.attackers.empty() || w.defenders.empty(); });
    text += " " + gone.short_name + (beyond ? " verlässt die Karte." : " geht unter.");
}

/// Friedensschluss (Abschnitt 4.5): Der Krieg endet; winner 1 Angreifer, −1 Verteidiger, 0 ohne Gewinner.
/// true, wenn Orte den Besitzer wechselten.
bool conclude_peace(World& world, std::size_t index, int winner) {
    const War war = world.wars[index];
    world.wars.erase(world.wars.begin() + static_cast<std::ptrdiff_t>(index));
    const Index attacker = war.attackers.front();
    const Index defender = war.defenders.front();
    std::string text = "Frieden zwischen " + name_of(world, attacker) + " und " + name_of(world, defender) +
                       (winner == 0 ? " ohne Gewinner."
                                    : ": " + name_of(world, winner > 0 ? attacker : defender) + " siegt.");
    bool changed = false;
    std::int32_t transfers = 0;
    std::vector<Index> heirs(world.states.size(), kNone);
    if (winner != 0) {
        const bool attacker_won = winner > 0;
        const std::vector<Index>& losers = side_of(war, !attacker_won);
        const Index main = side_of(war, attacker_won).front();
        const Index loser = losers.front();
        for (const WarGoal& goal : war.goals) {
            if (goal.attacker != attacker_won) {
                continue;
            }
            switch (goal.kind) {
            case WarGoalKind::Territory:
            case WarGoalKind::Annexation: {
                std::int32_t count = 0;
                for (std::size_t l = 0; l < world.locations.size(); ++l) {
                    Location& loc = world.locations[l];
                    const bool taken =
                        goal.kind == WarGoalKind::Territory
                            ? contains(losers, loc.owner) && in_unit(world, static_cast<Index>(l), goal.param)
                            : loc.owner == goal.param;
                    if (taken) {
                        heirs[at(loc.owner)] = main;
                        loc.owner = main;
                        ++count;
                    }
                }
                if (count > 0) {
                    changed = true;
                    transfers += count;
                    const std::string places =
                        " (" + std::to_string(count) + (count == 1 ? " Ort)." : " Orte).");
                    text += " " + name_of(world, main) +
                            (goal.kind == WarGoalKind::Annexation
                                 ? " annektiert " + name_of(world, goal.param) + places
                                 : " erhält " + goal_text(world, war, goal) + places);
                }
                break;
            }
            case WarGoalKind::Leadership:
                if (in_range(world.left_federation, loser) && loser == world.diet_presidency) {
                    std::fill(world.left_federation.begin(), world.left_federation.end(), std::uint8_t{1});
                    text += " Der Deutsche Bund löst sich auf.";
                } else if (in_range(world.left_federation, loser)) {
                    world.left_federation[at(loser)] = 1;
                    world.diet_presidency = main;
                    text += " " + name_of(world, loser) + " verlässt den Deutschen Bund.";
                }
                break;
            case WarGoalKind::Customs: {
                const Index union_index = world.states[at(main)].customs_union;
                if (union_index != kNone) {
                    std::erase_if(world.customs_requests,
                                  [&](const CustomsRequest& r) { return r.state == loser; });
                    world.customs_requests.push_back(CustomsRequest{loser, union_index});
                    text += " " + name_of(world, loser) + " tritt zum " +
                            format_date_long(customs_forced_date(world.date)) + " der Zollunion bei.";
                }
                break;
            }
            case WarGoalKind::Indemnity: {
                const Fixed amount = world.states[at(loser)].last_month_income * Fixed::from_int(goal.param);
                if (amount > Fixed::zero()) {
                    pay(world.states[at(loser)], amount);
                    world.states[at(main)].treasury += amount;
                    text += " " + name_of(world, loser) + " zahlt " + grouped(amount.round()) + ".";
                }
                break;
            }
            }
        }
    }
    // Gefangene zwischen den Seiten kehren heim; besetzte Orte gehen an ihre Besitzer zurück.
    const auto across = [&](Index a, Index b) {
        return (contains(war.attackers, a) && contains(war.defenders, b)) ||
               (contains(war.defenders, a) && contains(war.attackers, b));
    };
    std::vector<Prisoners> kept;
    for (const Prisoners& p : world.prisoners) {
        if (!across(p.state, p.captor)) {
            kept.push_back(p);
            continue;
        }
        for (Army& army : world.armies) {
            if (army.state == p.state) {
                set_soldiers(world, army, army.soldiers + p.soldiers);
                break;
            }
        }
    }
    world.prisoners = std::move(kept);
    for (std::size_t l = 0; l < world.control.size(); ++l) {
        const Index holder = world.control[l];
        if (holder != kNone && !at_war(world, holder, world.locations[l].owner)) {
            world.control[l] = kNone;
        }
    }
    // Untergang und neue Hauptstädte.
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        const auto state = static_cast<Index>(s);
        State& st = world.states[s];
        if (st.capital == kNone) {
            continue;
        }
        if (!has_locations(world, state)) {
            extinguish(world, state, heirs[s] != kNone ? heirs[s] : side_of(war, winner >= 0).front(), text);
            continue;
        }
        if (world.locations[at(st.capital)].owner != state) {
            Index best = kNone;
            Fixed most;
            for (std::size_t l = 0; l < world.locations.size(); ++l) {
                const Fixed people = world.location_population(static_cast<Index>(l));
                if (world.locations[l].owner == state && (best == kNone || people > most)) {
                    best = static_cast<Index>(l);
                    most = people;
                }
            }
            st.capital = best;
        }
    }
    // Heere in Orten, die ihnen nicht mehr offenstehen, kehren in ihre Garnison oder Hauptstadt zurück.
    for (Army& army : world.armies) {
        if (accessible(world, army.state, army.location)) {
            continue;
        }
        Index home = world.states[at(army.state)].capital;
        if (in_range(world.garrisons, army.garrison) &&
            world.locations[at(world.garrisons[at(army.garrison)].location)].owner == army.state) {
            home = world.garrisons[at(army.garrison)].location;
        }
        stop(army);
        army.location = home;
        army.previous = kNone;
        army.days_here = 0;
    }
    chronicle(world, attacker, std::move(text));
    world.war_results.push_back(WarResult{war.id, attacker, defender, war.cause, war.started, world.date,
                                          winner, war.battles, war.defender_inflicted, war.attacker_inflicted,
                                          transfers});
    // Wer in keinem Krieg mehr steht, rüstet ab.
    for (const std::vector<Index>* side : {&war.attackers, &war.defenders}) {
        for (const Index s : *side) {
            if (has_locations(world, s) && !in_war(world, s)) {
                order_mobilization(world, s, false);
            }
        }
    }
    return changed;
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
        // Seit b2: Ein Heer in der Festung wird eingeschlossen statt gestürmt (Belagerung), wenn die
        // Angreifer es an Soldaten um generalstab_angriff übertreffen, an Kampfkraft aber nicht um so viel
        // über die Festung hinaus.
        if (it == world.battles.end()) {
            const std::int32_t level = fortress_level(world, location);
            if (level > 0 && soldiers_of(world, sides.attackers) >=
                                 soldiers_of(world, sides.defenders) * world.military_rules.staff_attack) {
                Fixed attack;
                Fixed defense;
                for (const std::size_t i : sides.attackers) {
                    attack += combat_power(world, world.armies[i]);
                }
                for (const std::size_t i : sides.defenders) {
                    defense += combat_power(world, world.armies[i]);
                }
                const Fixed cover =
                    Fixed::one() + world.military_rules.fortress_defense * Fixed::from_int(level);
                if (attack < defense * cover * world.military_rules.staff_attack) {
                    continue;
                }
            }
        }
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

Fixed combat_power(const World& world, const Army& army) {
    return strength(world, army, Fixed::one());
}

void occupy_on_passage(World& world, const Army& army) {
    const Index location = army.location;
    const Index holder = controller(world, location);
    if (holder == kNone || !at_war(world, army.state, holder) || fortress_level(world, location) > 0) {
        return;
    }
    if (std::any_of(world.armies.begin(), world.armies.end(), [&](const Army& a) {
            return a.location == location && engaged(a) && friendly(world, a.state, holder);
        })) {
        return;
    }
    const Location& place = world.locations[at(location)];
    const Index next = friendly(world, army.state, place.owner) ? kNone : army.state;
    world.control[at(location)] = next;
    credit(world, army.state, holder, Fixed::zero(), Fixed::zero());
    if (place.owner != kNone && world.states[at(place.owner)].capital == location) {
        chronicle(world, army.state,
                  next == kNone ? name_of(world, army.state) + " befreit " + place.name +
                                      ", die Hauptstadt von " + name_of(world, place.owner) + "."
                                : name_of(world, army.state) + " besetzt " + place.name +
                                      ", die Hauptstadt von " + name_of(world, place.owner) + ".",
                  location);
    }
}

void run_occupation(World& world) {
    if (world.wars.empty() && world.sieges.empty()) {
        return;
    }
    const MilitaryRules& rules = world.military_rules;
    // Seit b2: Wer nach einem vollen Tag am Ort abmarschiert, nimmt ihn mit (ohne Festung, wie im
    // Vorbeimarsch).
    for (const Army& army : world.armies) {
        if (army.status == ArmyStatus::March && army.days_here >= 1) {
            occupy_on_passage(world, army);
        }
    }
    const std::map<Index, std::vector<std::size_t>> engaged_here = armies_by_location(world, false);
    std::vector<Index> besieged;
    std::vector<std::int64_t> surrendered; // seit b2: Heere, die sich mit einer Festung ergeben
    for (const auto& [location, armies] : armies_by_location(world, true)) {
        if (std::any_of(world.battles.begin(), world.battles.end(),
                        [&](const Battle& b) { return b.location == location; })) {
            continue;
        }
        const Index holder = controller(world, location);
        if (holder == kNone) {
            continue;
        }
        const std::int32_t level = fortress_level(world, location);
        std::vector<std::size_t> takers;
        for (const std::size_t i : armies) {
            if (world.armies[i].days_here >= 1 && at_war(world, world.armies[i].state, holder)) {
                takers.push_back(i);
            }
        }

        // Heere des Besitzers im Ort: ohne Festung verhindern sie die Besetzung, in einer Festung werden sie
        // seit b2 mit ihr belagert (Einschließung) und ergeben sich mit ihr.
        const auto present = engaged_here.find(location);
        std::vector<std::size_t> inside;
        if (present != engaged_here.end()) {
            for (const std::size_t i : present->second) {
                if (friendly(world, world.armies[i].state, holder)) {
                    inside.push_back(i);
                }
            }
        }
        if (takers.empty() || (!inside.empty() && level == 0)) {
            continue;
        }
        const Index taker = world.armies[takers.front()].state;
        const Location& place = world.locations[at(location)];
        // Ein Verbündeter des Besitzers befreit den Ort für ihn.
        const Index next = friendly(world, taker, place.owner) ? kNone : taker;
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
            Fixed garrison = rules.fortress_garrison * Fixed::from_int(level);
            for (const std::size_t i : inside) {
                garrison += world.armies[i].soldiers;
            }
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
            std::string text = "Die " + name + " ergibt sich " + name_of(world, taker) + ".";
            if (!inside.empty()) {
                std::vector<std::string> names;
                for (const std::size_t i : inside) {
                    Army& army = world.armies[i];
                    names.push_back(army.name);
                    surrendered.push_back(army.id);
                    add_prisoners(world, army.state, taker, army.soldiers);
                    credit(world, taker, army.state, army.soldiers, army.soldiers);
                    set_soldiers(world, army, Fixed::zero());
                }
                text += " Mit ihr " + std::string(names.size() == 1 ? "ergibt sich " : "ergeben sich ") +
                        join(names) + ".";
            }
            chronicle(world, taker, std::move(text), location);
        }
        if (world.control[at(location)] == next) {
            continue;
        }
        world.control[at(location)] = next;
        credit(world, taker, holder, Fixed::zero(), Fixed::zero());
        if (place.owner != kNone && world.states[at(place.owner)].capital == location) {
            chronicle(world, taker,
                      next == kNone ? name_of(world, taker) + " befreit " + place.name +
                                          ", die Hauptstadt von " + name_of(world, place.owner) + "."
                                    : name_of(world, taker) + " besetzt " + place.name +
                                          ", die Hauptstadt von " + name_of(world, place.owner) + ".",
                      location);
        }
    }
    remove_armies(world, surrendered);
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

bool start_war(World& world, std::vector<Index> attackers, std::vector<Index> defenders, WarCause cause,
               const std::vector<WarGoal>& goals) {
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
    war.last_change = world.date;
    // Kriegsstärke beim Beginn (seit a4): Friedensstärke × Mobilmachungsfaktor je Seite.
    // Seit b1 mindestens die Soldaten unter Waffen (Aufmarsch eines Lesezeichens).
    const auto war_strength = [&](Index s) {
        return Fixed::max(peace_strength(world, s) * world.states[at(s)].army.mobilization,
                          state_soldiers(world, s));
    };
    for (const Index s : attackers) {
        war.attacker_strength += war_strength(s);
    }
    for (const Index s : defenders) {
        war.defender_strength += war_strength(s);
    }
    for (const WarGoal& goal : goals) {
        if (valid_war_goal(world, war, goal)) {
            war.goals.push_back(goal);
        }
    }
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
    for (const WarGoal& goal : war.goals) {
        text += " Kriegsziel: " + goal_text(world, war, goal) + ".";
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

bool declare_war(World& world, Index state, Index target, std::int64_t goal) {
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
    std::vector<WarGoal> goals;
    if (const std::optional<WarGoal> g = decode_war_goal(goal, true)) {
        goals.push_back(*g);
    }
    return start_war(world, std::move(attackers), std::move(defenders), WarCause::Declaration, goals);
}

} // namespace uhrwerk

namespace uhrwerk {

std::int64_t encode_war_goal(WarGoalKind kind, Index param) {
    return static_cast<std::int64_t>(kind) + 8 * static_cast<std::int64_t>(std::max<Index>(param, 0));
}

std::optional<WarGoal> decode_war_goal(std::int64_t code, bool attacker) {
    if (code <= 0) {
        return std::nullopt;
    }
    const std::int64_t kind = code % 8;
    if (kind < 1 || kind > static_cast<std::int64_t>(WarGoalKind::Indemnity) ||
        code / 8 > std::numeric_limits<Index>::max()) {
        return std::nullopt;
    }
    return WarGoal{static_cast<WarGoalKind>(kind), attacker, static_cast<Index>(code / 8)};
}

War* find_war(World& world, std::int64_t id) {
    for (War& w : world.wars) {
        if (w.id == id) {
            return &w;
        }
    }
    return nullptr;
}

const War* find_war(const World& world, std::int64_t id) {
    for (const War& w : world.wars) {
        if (w.id == id) {
            return &w;
        }
    }
    return nullptr;
}

bool valid_war_goal(const World& world, const War& war, const WarGoal& goal) {
    if (std::find(war.goals.begin(), war.goals.end(), goal) != war.goals.end()) {
        return false;
    }
    const std::vector<Index>& enemies = side_of(war, !goal.attacker);
    const Index own = side_of(war, goal.attacker).front();
    const Index enemy = enemies.front();
    switch (goal.kind) {
    case WarGoalKind::Territory:
        if (!in_range(world.admin_units, goal.param)) {
            return false;
        }
        for (std::size_t l = 0; l < world.locations.size(); ++l) {
            if (contains(enemies, world.locations[l].owner) &&
                in_unit(world, static_cast<Index>(l), goal.param)) {
                return true;
            }
        }
        return false;
    case WarGoalKind::Annexation:
        return contains(enemies, goal.param);
    case WarGoalKind::Leadership:
        return has_diet(world) && federal_state(world, enemy) && federal_state(world, own);
    case WarGoalKind::Customs:
        return world.states[at(own)].customs_union != kNone &&
               world.states[at(enemy)].customs_union != world.states[at(own)].customs_union;
    case WarGoalKind::Indemnity:
        return goal.param >= 1 && goal.param <= 24;
    }
    return false;
}

Fixed war_goal_cost(const World& world, const War& war, const WarGoal& goal) {
    const MilitaryRules& rules = world.military_rules;
    switch (goal.kind) {
    case WarGoalKind::Territory: {
        const std::vector<Index>& enemies = side_of(war, !goal.attacker);
        Fixed part;
        Fixed whole;
        for (std::size_t l = 0; l < world.locations.size(); ++l) {
            if (!contains(enemies, world.locations[l].owner)) {
                continue;
            }
            const Fixed people = world.location_population(static_cast<Index>(l));
            whole += people;
            if (in_unit(world, static_cast<Index>(l), goal.param)) {
                part += people;
            }
        }
        // Seit b2 zählt das Hinterland mit, wie bei den Kriegspunkten.
        for (const Hinterland& hinterland : world.hinterlands) {
            if (contains(enemies, hinterland.state)) {
                whole += hinterland.population;
            }
        }
        const Fixed cost = whole > Fixed::zero() ? rules.goal_territory * part / whole : rules.goal_territory;
        return Fixed::max(rules.goal_territory_min, cost);
    }
    case WarGoalKind::Annexation: {
        // Seit b2: × Anteil des Staates an der Bevölkerung der Gegenseite (mit Hinterland), mindestens
        // ziel_gebiet_mindestens; die Annexion der ganzen Gegenseite kostet ziel_annexion.
        const std::vector<Index>& enemies = side_of(war, !goal.attacker);
        Fixed part;
        Fixed whole;
        for (std::size_t l = 0; l < world.locations.size(); ++l) {
            const Index owner = world.locations[l].owner;
            if (!contains(enemies, owner)) {
                continue;
            }
            const Fixed people = world.location_population(static_cast<Index>(l));
            whole += people;
            if (owner == goal.param) {
                part += people;
            }
        }
        for (const Hinterland& hinterland : world.hinterlands) {
            if (contains(enemies, hinterland.state)) {
                whole += hinterland.population;
                if (hinterland.state == goal.param) {
                    part += hinterland.population;
                }
            }
        }
        const Fixed cost =
            whole > Fixed::zero() ? rules.goal_annexation * part / whole : rules.goal_annexation;
        return Fixed::max(rules.goal_territory_min, cost);
    }
    case WarGoalKind::Leadership:
        return rules.goal_leadership;
    case WarGoalKind::Customs:
        return rules.goal_customs;
    case WarGoalKind::Indemnity:
        return rules.goal_indemnity * Fixed::from_int(goal.param);
    }
    return Fixed::zero();
}

Fixed war_goals_cost(const World& world, const War& war, bool attacker) {
    Fixed cost;
    for (const WarGoal& goal : war.goals) {
        if (goal.attacker == attacker) {
            cost += war_goal_cost(world, war, goal);
        }
    }
    return cost;
}

Fixed war_score(const World& world, const War& war, bool attacker) {
    const MilitaryRules& rules = world.military_rules;
    const std::vector<Index>& own = side_of(war, attacker);
    const std::vector<Index>& enemies = side_of(war, !attacker);
    Fixed score;
    const Holdings h = holdings(world, enemies, own);
    if (h.population > Fixed::zero()) {
        score += rules.points_occupation * h.occupied / h.population;
    }
    // Liegt die Hauptstadt im Hinterland (seit b1: Paris), zählt die Hauptstadt auf der Karte nicht.
    const Index capital = world.states[at(enemies.front())].capital;
    const bool beyond = std::any_of(world.hinterlands.begin(), world.hinterlands.end(),
                                    [&](const Hinterland& hl) { return hl.state == enemies.front(); });
    if (capital != kNone && !beyond && contains(own, controller(world, capital))) {
        score += rules.points_capital;
    }
    const Fixed inflicted = attacker ? war.attacker_inflicted : war.defender_inflicted;
    const Fixed suffered = attacker ? war.defender_inflicted : war.attacker_inflicted;
    const Fixed strength = attacker ? war.defender_strength : war.attacker_strength;
    if (strength > Fixed::zero()) {
        score += (rules.points_battles * (inflicted - suffered) / strength)
                     .clamp(-rules.points_battles, rules.points_battles);
    }
    score += rules.points_army * Fixed::from_int(attacker ? war.attacker_destroyed : war.defender_destroyed);
    return score.clamp(Fixed::zero(), Fixed::from_int(100));
}

Fixed war_weariness(const World& world, const War& war, bool attacker) {
    const MilitaryRules& rules = world.military_rules;
    Fixed weariness;
    const Fixed suffered = attacker ? war.defender_inflicted : war.attacker_inflicted;
    const Fixed strength = attacker ? war.attacker_strength : war.defender_strength;
    if (strength > Fixed::zero()) {
        weariness += rules.weariness_losses * suffered / strength;
    }
    const Holdings h = holdings(world, side_of(war, attacker), side_of(war, !attacker));
    if (h.population > Fixed::zero()) {
        weariness += rules.weariness_occupation * h.occupied / h.population;
    }
    const std::int64_t months = (to_day_number(world.date) - to_day_number(war.started)) / 30;
    weariness += rules.weariness_month * Fixed::from_int(months);
    return weariness.clamp(Fixed::zero(), Fixed::from_int(100));
}

bool add_war_goal(World& world, Index state, std::int64_t id, std::int64_t code) {
    War* war = find_war(world, id);
    if (war == nullptr || !in_range(world.states, state) ||
        (war->attackers.front() != state && war->defenders.front() != state)) {
        return false;
    }
    const std::optional<WarGoal> goal = decode_war_goal(code, war->attackers.front() == state);
    if (!goal || !valid_war_goal(world, *war, *goal)) {
        return false;
    }
    war->goals.push_back(*goal);
    chronicle(world, state,
              name_of(world, state) + " stellt das Kriegsziel " + goal_text(world, *war, *goal) + ".");
    return true;
}

bool offer_peace(World& world, Index state, std::int64_t id, PeaceTerms terms) {
    const War* war = find_war(world, id);
    if (war == nullptr || !in_range(world.states, state) ||
        (war->attackers.front() != state && war->defenders.front() != state)) {
        return false;
    }
    const bool attacker = war->attackers.front() == state;
    const Index other = side_of(*war, !attacker).front();
    const Fixed own_score = war_score(world, *war, attacker);
    const Fixed other_score = war_score(world, *war, !attacker);
    const Fixed other_weariness = war_weariness(world, *war, !attacker);
    bool accepted = false;
    int winner = 0;
    std::string what;
    switch (terms) {
    case PeaceTerms::White:
        accepted = other_score < Fixed::from_int(25) || other_weariness >= Fixed::from_int(50);
        what = "ohne Gewinner";
        break;
    case PeaceTerms::OwnGoals: {
        const Fixed cost = war_goals_cost(world, *war, attacker);
        if (cost <= Fixed::zero()) {
            return false;
        }
        accepted = own_score >= cost || other_weariness >= cost;
        winner = attacker ? 1 : -1;
        what = "zu seinen Bedingungen";
        break;
    }
    case PeaceTerms::Capitulation:
        accepted = true;
        winner = attacker ? -1 : 1;
        what = "und unterwirft sich";
        break;
    }
    chronicle(world, state,
              name_of(world, state) + " bietet " + name_of(world, other) + " Frieden an (" + what + "); " +
                  name_of(world, other) + (accepted ? " nimmt an." : " lehnt ab."));
    if (!accepted) {
        return false;
    }
    const std::size_t index = static_cast<std::size_t>(war - world.wars.data());
    conclude_peace(world, index, winner);
    return true;
}

namespace military_internal {

bool run_peace(World& world) {
    bool changed = false;
    for (std::size_t i = 0; i < world.wars.size();) {
        const War& war = world.wars[i];
        // Eine Seite setzt ihre Ziele durch, wenn ihre Kriegspunkte deren Kosten decken; seit b2 auch, wenn
        // die Kriegsmüdigkeit der Gegenseite sie erreicht (wie beim Friedensangebot zu eigenen Bedingungen).
        const auto decided = [&](bool attacker) {
            const Fixed cost = war_goals_cost(world, war, attacker);
            return cost > Fixed::zero() &&
                   (war_score(world, war, attacker) >= cost || war_weariness(world, war, !attacker) >= cost);
        };
        int winner = 2; // 2: weiter Krieg
        if (decided(true)) {
            winner = 1;
        } else if (decided(false)) {
            winner = -1;
        } else if (war_weariness(world, war, true) >= Fixed::from_int(100)) {
            winner = war_goals_cost(world, war, false) > Fixed::zero() ? -1 : 0;
        } else if (war_weariness(world, war, false) >= Fixed::from_int(100)) {
            winner = war_goals_cost(world, war, true) > Fixed::zero() ? 1 : 0;
        } else if (to_day_number(world.date) - to_day_number(war.last_change) >=
                   30 * static_cast<std::int64_t>(world.military_rules.stalemate_months)) {
            winner = 0;
        }
        if (winner == 2) {
            ++i;
            continue;
        }
        changed = conclude_peace(world, i, winner) || changed;
        i = 0; // ein Untergang kann weitere Kriege beenden
    }
    return changed;
}

} // namespace military_internal

} // namespace uhrwerk
