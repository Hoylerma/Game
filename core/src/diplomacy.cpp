#include "uhrwerk/diplomacy.hpp"

#include "uhrwerk/ai.hpp"

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

void chronicle(World& world, Index state, std::string text) {
    ChronicleEntry entry;
    entry.date = world.date;
    entry.text = std::move(text);
    entry.state = state;
    world.chronicle.push_back(std::move(entry));
}

const Fixed kRelationMin = Fixed::from_int(-200);
const Fixed kRelationMax = Fixed::from_int(200);

/// Was der Zielwert eines Paares braucht, einmal für alle Paare gerechnet.
class TargetContext {
public:
    explicit TargetContext(const World& world)
        : world_(world), states_(world.states.size()), federal_(states_, false), base_(states_ * states_),
          alliance_(states_ * states_, false), guarantee_(states_ * states_, false),
          envoy_(states_ * states_, false) {
        for (const Location& loc : world.locations) {
            if (loc.federal && in_range(world.states, loc.owner)) {
                federal_[at(loc.owner)] = true;
            }
        }
        for (const BaseRelation& r : world.base_relations) {
            if (in_range(world.states, r.a) && in_range(world.states, r.b)) {
                base_[pair(r.a, r.b)] = r.value;
                base_[pair(r.b, r.a)] = r.value;
            }
        }
        for (const Treaty& t : world.treaties) {
            if (!in_range(world.states, t.from) || !in_range(world.states, t.to)) {
                continue;
            }
            if (t.kind == TreatyKind::Alliance) {
                alliance_[pair(t.from, t.to)] = true;
                alliance_[pair(t.to, t.from)] = true;
            } else {
                guarantee_[pair(t.from, t.to)] = true;
            }
        }
        for (const Envoy& e : world.envoys) {
            if (in_range(world.states, e.from) && in_range(world.states, e.to)) {
                envoy_[pair(e.from, e.to)] = true;
            }
        }
    }

    Fixed target(Index a, Index b) const {
        const DiplomacyRules& rules = world_.diplomacy_rules;
        const State& sa = world_.states[at(a)];
        const State& sb = world_.states[at(b)];
        Fixed value = base_[pair(a, b)];
        if (!world_.customs_unions.empty() && sa.customs_union != kNone &&
            sa.customs_union == sb.customs_union) {
            value += rules.customs_union;
        }
        if (federal_[at(a)] && federal_[at(b)]) {
            value += rules.federation;
        }
        if (sa.confession != kNone && group(sa.confession) == group(sb.confession)) {
            value += rules.confession;
        }
        if (alliance_[pair(a, b)]) {
            value += rules.alliance;
        }
        if (guarantee_[pair(a, b)]) {
            value += rules.guarantee;
        }
        if (guarantee_[pair(b, a)]) {
            value += rules.guarantee;
        }
        return value;
    }

    bool envoy(Index from, Index to) const { return envoy_[pair(from, to)]; }

private:
    std::size_t pair(Index a, Index b) const { return at(a) * states_ + at(b); }

    Index group(Index confession) const {
        const std::vector<Index>& groups = world_.diplomacy_rules.confession_group;
        return in_range(groups, confession) ? groups[at(confession)] : confession;
    }

    const World& world_;
    std::size_t states_;
    std::vector<bool> federal_;
    std::vector<Fixed> base_;
    std::vector<bool> alliance_;
    std::vector<bool> guarantee_;
    std::vector<bool> envoy_;
};

bool valid_pair(const World& world, Index a, Index b) {
    return !world.relations.empty() && in_range(world.states, a) && in_range(world.states, b) && a != b;
}

const std::string& name_of(const World& world, Index state) {
    return world.states[at(state)].short_name;
}

} // namespace

bool diplomacy_active(const World& world) {
    return std::any_of(world.locations.begin(), world.locations.end(),
                       [&](const Location& loc) { return loc.federal && in_range(world.states, loc.owner); });
}

bool federal_state(const World& world, Index state) {
    // Wer den Bund nach einem Krieg verlassen hat (seit M10, militaer.md 4.5), gehört nicht mehr dazu.
    if (in_range(world.left_federation, state) && world.left_federation[at(state)] != 0) {
        return false;
    }
    // Seit M13a6 zuerst die Hauptstadt: Sie entscheidet fast immer, ohne alle Orte zu durchsuchen (das
    // kostete 1848 ein Zehntel der Rechenzeit); gleiches Ergebnis wie die Suche.
    const Index capital = in_range(world.states, state) ? world.states[at(state)].capital : kNone;
    const Location* capital_loc =
        in_range(world.locations, capital) && world.locations[at(capital)].owner == state
            ? &world.locations[at(capital)]
            : nullptr;
    if (federation_founded(world)) {
        // Seit M13a2: die Mitglieder des Bundes zur Laufzeit, solange sie Orte besitzen.
        return in_range(world.federation.members, state) && world.federation.members[at(state)] != 0 &&
               (capital_loc != nullptr ||
                std::any_of(world.locations.begin(), world.locations.end(),
                            [&](const Location& loc) { return loc.owner == state; }));
    }
    if (capital_loc != nullptr && capital_loc->federal) {
        return true;
    }
    return in_range(world.states, state) &&
           std::any_of(world.locations.begin(), world.locations.end(),
                       [&](const Location& loc) { return loc.federal && loc.owner == state; });
}

bool federation_founded(const World& world) {
    return in_range(world.federation_templates, world.federation.diet);
}

const FederalDiet& active_diet(const World& world) {
    return federation_founded(world) ? world.federation_templates[at(world.federation.diet)] : world.diet;
}

bool in_federal_area(const World& world, Index location) {
    if (!in_range(world.locations, location)) {
        return false;
    }
    const Location& loc = world.locations[at(location)];
    if (!federation_founded(world)) {
        return loc.federal;
    }
    return in_range(world.federation.members, loc.owner) && world.federation.members[at(loc.owner)] != 0 &&
           !(in_range(world.left_federation, loc.owner) && world.left_federation[at(loc.owner)] != 0);
}

void found_federation(World& world, Index template_index, const std::vector<std::uint8_t>& members) {
    if (!in_range(world.federation_templates, template_index) || members.size() != world.states.size()) {
        return;
    }
    const FederalDiet& diet = world.federation_templates[at(template_index)];
    world.federation.diet = template_index;
    world.federation.members = members;
    world.federation.founded = world.date;
    // Wer Mitglied ist, gehört wieder zum Bund (auch nach der Auflösung des alten, militaer.md 4.5).
    if (world.left_federation.size() == world.states.size()) {
        for (std::size_t s = 0; s < members.size(); ++s) {
            world.left_federation[s] = members[s] != 0 ? 0 : 1;
        }
    }
    world.diet_presidency = diet.presidency;
    world.diet_suspended = 0;
    world.motions.clear();
    std::string names;
    std::int64_t count = 0;
    for (std::size_t s = 0; s < members.size(); ++s) {
        if (members[s] != 0) {
            ++count;
        }
    }
    names = std::to_string(count) + " Staaten";
    ChronicleEntry entry;
    entry.date = world.date;
    entry.state = diet.presidency;
    entry.text = "Gründung: " + (diet.federation_name.empty() ? diet.name : diet.federation_name) + " mit " +
                 names +
                 (in_range(world.states, diet.presidency)
                      ? ", Präsidium " + world.states[at(diet.presidency)].short_name
                      : std::string()) +
                 ".";
    world.chronicle.push_back(std::move(entry));
}

bool join_federation(World& world, Index state) {
    if (!federation_founded(world) || !in_range(world.federation.members, state) ||
        world.federation.members[at(state)] != 0) {
        return false;
    }
    world.federation.members[at(state)] = 1;
    if (in_range(world.left_federation, state)) {
        world.left_federation[at(state)] = 0;
    }
    ChronicleEntry entry;
    entry.date = world.date;
    entry.state = state;
    const FederalDiet& diet = active_diet(world);
    entry.text = world.states[at(state)].short_name +
                 " tritt bei: " + (diet.federation_name.empty() ? diet.name : diet.federation_name) + ".";
    world.chronicle.push_back(std::move(entry));
    return true;
}

void adjust_relation(World& world, Index a, Index b, Fixed delta) {
    if (!valid_pair(world, a, b)) {
        return;
    }
    Fixed& r = world.relations[relation_slot(world.states.size(), a, b)];
    r = (r + delta).clamp(kRelationMin, kRelationMax);
}

std::size_t relation_count(std::size_t states) {
    return states < 2 ? 0 : states * (states - 1) / 2;
}

std::size_t relation_slot(std::size_t states, Index a, Index b) {
    const std::size_t i = at(std::min(a, b));
    const std::size_t j = at(std::max(a, b));
    return i * (2 * states - i - 1) / 2 + (j - i - 1);
}

Fixed relation(const World& world, Index a, Index b) {
    if (!valid_pair(world, a, b)) {
        return Fixed::zero();
    }
    const std::size_t slot = relation_slot(world.states.size(), a, b);
    return slot < world.relations.size() ? world.relations[slot] : Fixed::zero();
}

Fixed relation_target(const World& world, Index a, Index b) {
    if (!valid_pair(world, a, b)) {
        return Fixed::zero();
    }
    return TargetContext(world).target(a, b);
}

std::vector<Fixed> relation_targets(const World& world, Index state) {
    if (world.relations.empty() || !in_range(world.states, state)) {
        return {};
    }
    const TargetContext context(world);
    std::vector<Fixed> targets(world.states.size());
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        if (static_cast<Index>(s) != state) {
            targets[s] = context.target(state, static_cast<Index>(s));
        }
    }
    return targets;
}

bool rivals(const World& world, Index a, Index b) {
    return valid_pair(world, a, b) && relation(world, a, b) < world.diplomacy_rules.rival_below;
}

bool allied(const World& world, Index a, Index b) {
    return std::any_of(world.treaties.begin(), world.treaties.end(), [&](const Treaty& t) {
        return t.kind == TreatyKind::Alliance && ((t.from == a && t.to == b) || (t.from == b && t.to == a));
    });
}

bool guarantees(const World& world, Index from, Index to) {
    return std::any_of(world.treaties.begin(), world.treaties.end(), [&](const Treaty& t) {
        return t.kind == TreatyKind::Guarantee && t.from == from && t.to == to;
    });
}

bool has_envoy(const World& world, Index from, Index to) {
    return std::any_of(world.envoys.begin(), world.envoys.end(),
                       [&](const Envoy& e) { return e.from == from && e.to == to; });
}

std::int64_t envoy_capacity(const World& world, Index state) {
    if (!in_range(world.states, state) || world.diplomacy_rules.envoy_people <= Fixed::zero()) {
        return 1;
    }
    return 1 + (world.state_population(state) / world.diplomacy_rules.envoy_people).trunc();
}

std::int64_t envoy_count(const World& world, Index state) {
    return std::count_if(world.envoys.begin(), world.envoys.end(),
                         [&](const Envoy& e) { return e.from == state; });
}

Fixed envoy_costs(const World& world, Index state, Fixed income) {
    if (world.relations.empty() || income <= Fixed::zero()) {
        return Fixed::zero();
    }
    return Fixed::from_int(envoy_count(world, state)) * world.diplomacy_rules.envoy_cost * income;
}

void ensure_diplomacy_state(World& world) {
    const std::size_t count = relation_count(world.states.size());
    if (!diplomacy_active(world) || count == 0 || world.relations.size() == count) {
        return;
    }
    // Zum Start steht jede Beziehung auf ihrem Zielwert (eingeschwungen wie die Wirtschaft); die
    // Bundesversammlung (seit Schritt b2) hat ihre Präsidialmacht aus den Daten.
    world.diet_presidency = world.diet.presidency;
    world.relations.assign(count, Fixed{});
    const TargetContext context(world);
    const auto states = static_cast<Index>(world.states.size());
    for (Index a = 0; a < states; ++a) {
        for (Index b = a + 1; b < states; ++b) {
            world.relations[relation_slot(world.states.size(), a, b)] =
                context.target(a, b).clamp(kRelationMin, kRelationMax);
        }
    }
}

void update_relations(World& world) {
    const std::size_t count = relation_count(world.states.size());
    if (world.relations.size() != count) {
        return;
    }
    const DiplomacyRules& rules = world.diplomacy_rules;
    const TargetContext context(world);
    const auto states = static_cast<Index>(world.states.size());
    for (Index a = 0; a < states; ++a) {
        for (Index b = a + 1; b < states; ++b) {
            Fixed& r = world.relations[relation_slot(world.states.size(), a, b)];
            const Fixed target = context.target(a, b);
            r += (target - r) * rules.drift;
            // Ein Gesandter hebt die Beziehung über den Zielwert hinaus, aber nicht weiter als envoy_limit;
            // liegt sie schon darüber, senkt er sie nicht.
            for (const bool envoy : {context.envoy(a, b), context.envoy(b, a)}) {
                if (envoy) {
                    r = Fixed::max(r, Fixed::min(r + rules.envoy, target + rules.envoy_limit));
                }
            }
            r = r.clamp(kRelationMin, kRelationMax);
        }
    }
}

DiplomacyDecision evaluate_envoy(const World& world, Index state, Index target, bool send) {
    if (!valid_pair(world, state, target)) {
        return DiplomacyDecision::Invalid;
    }
    if (!send) {
        return has_envoy(world, state, target) ? DiplomacyDecision::Accepted : DiplomacyDecision::Invalid;
    }
    if (has_envoy(world, state, target) || envoy_count(world, state) >= envoy_capacity(world, state)) {
        return DiplomacyDecision::Invalid;
    }
    return DiplomacyDecision::Accepted;
}

DiplomacyDecision request_envoy(World& world, Index state, Index target, bool send) {
    const DiplomacyDecision decision = evaluate_envoy(world, state, target, send);
    if (decision != DiplomacyDecision::Accepted) {
        return decision;
    }
    if (send) {
        world.envoys.push_back(Envoy{state, target, world.date});
        chronicle(world, state,
                  name_of(world, state) + " entsendet einen Gesandten nach " + name_of(world, target) + ".");
    } else {
        std::erase_if(world.envoys, [&](const Envoy& e) { return e.from == state && e.to == target; });
        chronicle(world, state,
                  name_of(world, state) + " ruft seinen Gesandten aus " + name_of(world, target) + " ab.");
    }
    return decision;
}

DiplomacyDecision evaluate_treaty(const World& world, Index state, Index target, TreatyAction action) {
    if (!valid_pair(world, state, target)) {
        return DiplomacyDecision::Invalid;
    }
    const DiplomacyRules& rules = world.diplomacy_rules;
    switch (action) {
    case TreatyAction::Alliance: {
        if (allied(world, state, target)) {
            return DiplomacyDecision::Invalid;
        }
        // Seit M12 (ki.md 6.2) entscheidet ein Staat ohne Spieler nach Nutzen statt nach bündnis_ab.
        if (ai_active(world) && target != world.event_player) {
            if (!ai_accepts_alliance(world, target, state)) {
                return DiplomacyDecision::Rejected;
            }
        } else if (relation(world, state, target) < rules.alliance_min) {
            return DiplomacyDecision::Rejected;
        }
        // Kein Bündnis mit einem Staat, der mit dem eigenen Gegner verbündet ist (in beide Richtungen).
        for (const Treaty& t : world.treaties) {
            if (t.kind != TreatyKind::Alliance) {
                continue;
            }
            for (const auto& [own, other] : {std::pair{state, target}, std::pair{target, state}}) {
                const Index partner = t.from == own ? t.to : t.to == own ? t.from : kNone;
                if (partner != kNone && rivals(world, other, partner)) {
                    return DiplomacyDecision::Rejected;
                }
            }
        }
        return DiplomacyDecision::Accepted;
    }
    case TreatyAction::Guarantee:
        if (guarantees(world, state, target) || allied(world, state, target)) {
            return DiplomacyDecision::Invalid;
        }
        return relation(world, state, target) < rules.guarantee_min ? DiplomacyDecision::Rejected
                                                                    : DiplomacyDecision::Accepted;
    case TreatyAction::CancelAlliance:
        return allied(world, state, target) ? DiplomacyDecision::Accepted : DiplomacyDecision::Invalid;
    case TreatyAction::CancelGuarantee:
        return guarantees(world, state, target) ? DiplomacyDecision::Accepted : DiplomacyDecision::Invalid;
    }
    return DiplomacyDecision::Invalid;
}

DiplomacyDecision request_treaty(World& world, Index state, Index target, TreatyAction action) {
    const DiplomacyDecision decision = evaluate_treaty(world, state, target, action);
    if (decision == DiplomacyDecision::Invalid) {
        return decision;
    }
    const std::string& own = name_of(world, state);
    const std::string& other = name_of(world, target);
    if (decision == DiplomacyDecision::Rejected) {
        chronicle(world, state,
                  action == TreatyAction::Alliance ? other + " lehnt ein Bündnis mit " + own + " ab."
                                                   : other + " lehnt eine Garantie von " + own + " ab.");
        return decision;
    }
    switch (action) {
    case TreatyAction::Alliance:
        world.treaties.push_back(Treaty{TreatyKind::Alliance, state, target, world.date});
        chronicle(world, state, own + " und " + other + " schließen ein Bündnis.");
        break;
    case TreatyAction::Guarantee:
        world.treaties.push_back(Treaty{TreatyKind::Guarantee, state, target, world.date});
        chronicle(world, state, own + " garantiert den Bestand von " + other + ".");
        break;
    case TreatyAction::CancelAlliance:
        std::erase_if(world.treaties, [&](const Treaty& t) {
            return t.kind == TreatyKind::Alliance &&
                   ((t.from == state && t.to == target) || (t.from == target && t.to == state));
        });
        chronicle(world, state, own + " kündigt das Bündnis mit " + other + ".");
        break;
    case TreatyAction::CancelGuarantee:
        std::erase_if(world.treaties, [&](const Treaty& t) {
            return t.kind == TreatyKind::Guarantee && t.from == state && t.to == target;
        });
        chronicle(world, state, own + " kündigt die Garantie für " + other + ".");
        break;
    }
    return decision;
}

} // namespace uhrwerk
