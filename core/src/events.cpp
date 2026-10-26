#include "uhrwerk/events.hpp"

#include "uhrwerk/diplomacy.hpp"
#include "uhrwerk/military.hpp"
#include "uhrwerk/politics.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace uhrwerk {

namespace {

constexpr int kMaxFollowsPerDay = 100000; // Schutz vor Endlosschleifen (der Lader prüft Kreise)

std::size_t at(Index index) {
    return static_cast<std::size_t>(index);
}

template <typename T> bool in_range(const std::vector<T>& items, Index index) {
    return index >= 0 && static_cast<std::size_t>(index) < items.size();
}

template <typename T> bool compare(CompareOp op, const T& a, const T& b) {
    switch (op) {
    case CompareOp::Eq:
        return a == b;
    case CompareOp::Ne:
        return a != b;
    case CompareOp::Lt:
        return a < b;
    case CompareOp::Le:
        return a <= b;
    case CompareOp::Gt:
        return a > b;
    case CompareOp::Ge:
        return a >= b;
    }
    return false;
}

Date add_days(const Date& date, std::int32_t days) {
    return from_day_number(to_day_number(date) + days);
}

std::size_t column(const EventCache& cache, const EventDef& event, Index state) {
    return event.scope == EventScope::World || state == kNone ? cache.columns - 1 : at(state);
}

bool has_flag(const World& world, Index state, const std::string& name) {
    return std::binary_search(world.event_flags.begin(), world.event_flags.end(), EventFlag{state, name},
                              [](const EventFlag& a, const EventFlag& b) {
                                  return a.state != b.state ? a.state < b.state : a.name < b.name;
                              });
}

/// Bedingungen mit Zwischenergebnissen je Staat, die teuer sind (Stand der Gruppen, Lebensstandard).
class Evaluator {
public:
    Evaluator(const World& world, const EventCache& cache)
        : world_(world), cache_(cache), standings_(world.states.size()), living_(world.states.size()) {}

    bool holds(const Condition& c, Index state) {
        const World& w = world_;
        const bool own = in_range(w.states, state);
        switch (c.kind) {
        case ConditionKind::All:
            return std::all_of(c.children.begin(), c.children.end(),
                               [&](const Condition& child) { return holds(child, state); });
        case ConditionKind::Any:
            return std::any_of(c.children.begin(), c.children.end(),
                               [&](const Condition& child) { return holds(child, state); });
        case ConditionKind::Not:
            return std::none_of(c.children.begin(), c.children.end(),
                                [&](const Condition& child) { return holds(child, state); });
        case ConditionKind::Date:
            return compare(c.op, w.date, c.date);
        case ConditionKind::State:
            return own && compare(c.op, state, c.a);
        case ConditionKind::StateIn:
            return own && std::binary_search(c.list.begin(), c.list.end(), state);
        case ConditionKind::Happened: {
            if (!in_range(w.events, c.a)) {
                return false;
            }
            const EventDef& event = w.events[at(c.a)];
            if (event.scope == EventScope::State && !own) {
                return false;
            }
            const std::size_t slot = at(c.a) * cache_.columns + column(cache_, event, state);
            const std::int32_t record = slot < cache_.last.size() ? cache_.last[slot] : -1;
            return record >= 0 &&
                   (c.b == kNone || w.event_log[static_cast<std::size_t>(record)].option == c.b);
        }
        case ConditionKind::Flag:
            return own && has_flag(w, state, c.name);
        case ConditionKind::WorldFlag:
            return has_flag(w, kNone, c.name);
        case ConditionKind::Never:
            return false;
        case ConditionKind::Journal: {
            const Index stage = in_range(w.journal_states, c.a) ? w.journal_states[at(c.a)].stage : kNone;
            return compare(c.op, stage, c.b);
        }
        case ConditionKind::Vote:
            for (auto it = w.vote_results.rbegin(); it != w.vote_results.rend(); ++it) {
                if (it->vote == c.a) {
                    return it->accepted == (c.b == 1);
                }
            }
            return false;
        case ConditionKind::DietSuspended:
            return w.diet_suspended != 0;
        case ConditionKind::PopulationShare:
        case ConditionKind::CountStates: {
            Fixed matching;
            Fixed among;
            for (std::size_t s = 0; s < w.states.size(); ++s) {
                const auto other = static_cast<Index>(s);
                if (!alive(other) || !holds(c.children[1], other)) {
                    continue;
                }
                const Fixed weight =
                    c.kind == ConditionKind::PopulationShare ? w.state_population(other) : Fixed::one();
                among += weight;
                if (holds(c.children[0], other)) {
                    matching += weight;
                }
            }
            if (c.kind == ConditionKind::CountStates) {
                return compare(c.op, matching, c.value);
            }
            return among > Fixed::zero() && compare(c.op, matching / among, c.value);
        }
        case ConditionKind::OfState:
            return in_range(w.states, c.a) && alive(c.a) && all_children(c, c.a);
        case ConditionKind::AnyState:
            for (std::size_t s = 0; s < w.states.size(); ++s) {
                if (alive(static_cast<Index>(s)) && all_children(c, static_cast<Index>(s))) {
                    return true;
                }
            }
            return false;
        default:
            break;
        }
        if (!own) {
            return false; // Prüfungen auf den Staat (der Lader lässt sie in der Welt nicht zu)
        }
        const State& s = w.states[at(state)];
        switch (c.kind) {
        case ConditionKind::Law:
            return in_range(s.laws, c.a) && compare(c.op, s.laws[at(c.a)], c.b);
        case ConditionKind::Constitution:
            return compare(c.op, static_cast<Index>(s.constitution), c.a);
        case ConditionKind::Franchise:
            return compare(c.op, static_cast<Index>(s.law.franchise), c.a);
        case ConditionKind::Approval:
            return compare(c.op, approval(state, c.a), c.value);
        case ConditionKind::Share:
        case ConditionKind::Influence: {
            const std::vector<InterestGroupStanding>& standing = standing_of(state);
            if (!in_range(standing, c.a)) {
                return false;
            }
            const InterestGroupStanding& g = standing[at(c.a)];
            return compare(c.op, c.kind == ConditionKind::Share ? g.share : g.influence, c.value);
        }
        case ConditionKind::Unrest:
            return compare(c.op, unrest(state), c.value);
        case ConditionKind::NeighborUnrest:
            if (!in_range(cache_.neighbors, state)) {
                return false;
            }
            return std::any_of(cache_.neighbors[at(state)].begin(), cache_.neighbors[at(state)].end(),
                               [&](Index n) { return alive(n) && compare(c.op, unrest(n), c.value); });
        case ConditionKind::LivingLower:
            return compare(c.op, living_lower(state), c.value);
        case ConditionKind::Price:
            return compare(c.op, price_factor(s, c.a), c.value);
        case ConditionKind::AtWar:
            return military_active(w) && in_war(w, state);
        case ConditionKind::WarWith:
            return military_active(w) && at_war(w, state, c.a);
        case ConditionKind::Mobilized:
            return military_active(w) ? mobilization_phase(w, state) != MobilizationPhase::Peace
                                      : mobilized(w, state);
        case ConditionKind::Federal:
            return federal_state(w, state);
        case ConditionKind::CustomsUnion:
            return compare(c.op, s.customs_union, c.a);
        case ConditionKind::Relation:
            return compare(c.op, w.relations.empty() ? Fixed::zero() : relation(w, state, c.a), c.value);
        case ConditionKind::Seats:
            return compare(c.op, Fixed::from_int(seats(c.b, c.a)), c.value);
        case ConditionKind::Soldiers:
            return compare(c.op, soldiers(state), c.value);
        case ConditionKind::Treasury:
            return compare(c.op, s.treasury, c.value);
        case ConditionKind::DebtRatio: {
            const Fixed ratio = s.yearly_income > Fixed::zero() ? s.debt / s.yearly_income
                                : s.debt > Fixed::zero()        ? Fixed::from_int(1000)
                                                                : Fixed::zero();
            return compare(c.op, ratio, c.value);
        }
        case ConditionKind::Player:
            return w.event_player == state;
        case ConditionKind::InRevolt:
            return std::any_of(w.revolts.begin(), w.revolts.end(),
                               [&](const Revolt& r) { return r.state == state; });
        default:
            return false;
        }
    }

    bool alive(Index state) const { return in_range(cache_.alive, state) && cache_.alive[at(state)] != 0; }

    bool all_children(const Condition& c, Index state) {
        return std::all_of(c.children.begin(), c.children.end(),
                           [&](const Condition& child) { return holds(child, state); });
    }

    Fixed approval(Index state, Index group) const {
        const std::size_t slot = at(state) * world_.interest_groups.size() + at(group);
        return group >= 0 && slot < world_.approval.size() ? world_.approval[slot] : Fixed::zero();
    }

    Fixed unrest(Index state) const {
        return in_range(world_.unrest, state) ? world_.unrest[at(state)] : Fixed::zero();
    }

    const std::vector<InterestGroupStanding>& standing_of(Index state) {
        auto& slot = standings_[at(state)];
        if (!slot) {
            slot = interest_group_standing(world_, state);
        }
        return *slot;
    }

    /// Lebensstandard der unteren Schicht im Staat, nach Köpfen gewichtet.
    Fixed living_lower(Index state) {
        auto& slot = living_[at(state)];
        if (!slot) {
            Fixed weighted;
            Fixed people;
            for (const Location& loc : world_.locations) {
                if (loc.owner != state) {
                    continue;
                }
                for (std::int32_t k = 0; k < loc.pop_count; ++k) {
                    const PopBlock& p = world_.pops[at(loc.first_pop + k)];
                    if (world_.professions[at(p.profession)].stratum != Stratum::Unten) {
                        continue;
                    }
                    weighted += p.living_standard * p.size();
                    people += p.size();
                }
            }
            slot = people > Fixed::zero() ? weighted / people : Fixed::zero();
        }
        return *slot;
    }

    /// Preis des Gutes im Markt der Hauptstadt ÷ Basispreis (1 ohne Markt).
    Fixed price_factor(const State& s, Index good) const {
        if (!in_range(world_.locations, s.capital) || !in_range(world_.goods, good)) {
            return Fixed::one();
        }
        const Index market = world_.locations[at(s.capital)].market;
        if (!in_range(world_.market_states, market)) {
            return Fixed::one();
        }
        const MarketState& m = world_.market_states[at(market)];
        const Fixed base = world_.goods[at(good)].base_price;
        if (!in_range(m.price, good) || base <= Fixed::zero()) {
            return Fixed::one();
        }
        return m.price[at(good)] / base;
    }

    std::int32_t seats(Index parliament, Index current) const {
        for (auto it = world_.elections.rbegin(); it != world_.elections.rend(); ++it) {
            if (it->parliament == parliament) {
                const std::vector<std::int32_t> result = seats_by_current(world_, *it);
                return in_range(result, current) ? result[at(current)] : 0;
            }
        }
        return 0;
    }

    Fixed soldiers(Index state) const {
        if (military_active(world_)) {
            return state_soldiers(world_, state);
        }
        const State& s = world_.states[at(state)];
        return s.army_per_mille * world_.state_population(state) / Fixed::from_int(1000);
    }

private:
    const World& world_;
    const EventCache& cache_;
    std::vector<std::optional<std::vector<InterestGroupStanding>>> standings_;
    std::vector<std::optional<Fixed>> living_;
};

// ─── Wirkungen (Abschnitt 3.3) ──────────────────────────────────────────────

class Executor {
public:
    Executor(World& world, Pcg32& rng, EventCache& cache) : world_(world), rng_(rng), cache_(cache) {}

    /// Ein Ereignis tritt für den Staat (kNone: Welt) ein: wartet beim Spieler, sonst Wahl nach Gewicht.
    void fire(Index event, Index state) {
        const EventDef& e = world_.events[at(event)];
        if (e.scope == EventScope::State && state != kNone && state == world_.event_player) {
            const std::int32_t days = e.deadline > 0 ? e.deadline : world_.event_rules.deadline;
            world_.pending_events.push_back(
                PendingEvent{event, state, world_.date, add_days(world_.date, days)});
            return;
        }
        resolve(event, state, draw_option(e, state));
    }

    /// Die Option gilt: Eintrag in der Ereignischronik, Chronik, Wirkungen.
    void resolve(Index event, Index state, Index option) {
        const EventDef& e = world_.events[at(event)];
        const Index who = e.scope == EventScope::World ? kNone : state;
        world_.event_log.push_back(EventRecord{event, who, option, world_.date});
        const std::size_t slot = at(event) * cache_.columns + column(cache_, e, who);
        if (slot < cache_.last.size()) {
            cache_.last[slot] = static_cast<std::int32_t>(world_.event_log.size() - 1);
        }
        const EventOption& o = e.options[at(option)];
        const bool own_entry = std::any_of(o.effects.begin(), o.effects.end(),
                                           [](const Effect& x) { return x.kind == EffectKind::Chronicle; });
        if (!own_entry) {
            std::string text = in_range(world_.states, who) ? world_.states[at(who)].short_name + ": " : "";
            text += e.title + " – " + o.text;
            chronicle(std::move(text), who);
        }
        event_ = event;
        apply(o.effects, who);
    }

    /// Wahl nach Gewicht (Abschnitt 3.5) aus dem Strom Ereignisse; ohne Gewicht die Standardoption.
    Index draw_option(const EventDef& e, Index state) {
        const std::vector<Fixed> weights = option_weights(world_, cache_, e, state);
        std::uint64_t total = 0;
        for (const Fixed w : weights) {
            total += w > Fixed::zero() ? static_cast<std::uint64_t>(w.raw()) : 0;
        }
        int shift = 0;
        while ((total >> shift) > 0xFFFFFFFFULL) {
            ++shift;
        }
        std::uint64_t scaled = 0;
        for (const Fixed w : weights) {
            scaled += w > Fixed::zero() ? static_cast<std::uint64_t>(w.raw()) >> shift : 0;
        }
        if (scaled == 0) {
            return default_option(world_, cache_, e, state);
        }
        std::uint64_t r = rng_.below(static_cast<std::uint32_t>(scaled));
        for (std::size_t i = 0; i < weights.size(); ++i) {
            const std::uint64_t w =
                weights[i] > Fixed::zero() ? static_cast<std::uint64_t>(weights[i].raw()) >> shift : 0;
            if (r < w) {
                return static_cast<Index>(i);
            }
            r -= w;
        }
        return default_option(world_, cache_, e, state);
    }

private:
    void chronicle(std::string text, Index state) {
        ChronicleEntry entry;
        entry.date = world_.date;
        entry.text = std::move(text);
        entry.state = state;
        world_.chronicle.push_back(std::move(entry));
    }

    void apply(const std::vector<Effect>& effects, Index state) {
        for (const Effect& e : effects) {
            apply(e, state);
        }
    }

    void apply(const Effect& e, Index state) {
        World& w = world_;
        switch (e.kind) {
        case EffectKind::Chronicle:
            chronicle(e.name, state);
            return;
        case EffectKind::SetWorldFlag:
        case EffectKind::ClearWorldFlag:
            set_flag(kNone, e.name, e.kind == EffectKind::SetWorldFlag);
            return;
        case EffectKind::Follow: {
            const EventDef& target = w.events[at(e.a)];
            const Index who = target.scope == EventScope::World ? kNone : e.b != kNone ? e.b : state;
            const ScheduledEvent next{e.a, who, add_days(w.date, e.days)};
            const auto pos = std::upper_bound(
                w.scheduled_events.begin(), w.scheduled_events.end(), next,
                [](const ScheduledEvent& a, const ScheduledEvent& b) { return a.due < b.due; });
            w.scheduled_events.insert(pos, next);
            return;
        }
        case EffectKind::Random: {
            Fixed total;
            for (const Fixed weight : e.weights) {
                total += weight;
            }
            if (total <= Fixed::zero()) {
                return;
            }
            Fixed r = rng_.uniform(Fixed::zero(), total);
            for (std::size_t i = 0; i < e.weights.size(); ++i) {
                if (r < e.weights[i] || i + 1 == e.weights.size()) {
                    apply(e.branches[i], state);
                    return;
                }
                r -= e.weights[i];
            }
            return;
        }
        case EffectKind::OfState:
            if (alive(e.a)) {
                apply(e.children, e.a);
            }
            return;
        case EffectKind::AllStates:
            for (std::size_t s = 0; s < w.states.size(); ++s) {
                if (alive(static_cast<Index>(s))) {
                    apply(e.children, static_cast<Index>(s));
                }
            }
            return;
        case EffectKind::Journal:
            enter_stage(e.a, e.b);
            return;
        case EffectKind::Vote:
            hold_vote(e.a);
            return;
        case EffectKind::Diet:
            if (w.diet_suspended != static_cast<std::uint8_t>(e.a) && !w.diet.id.empty()) {
                w.diet_suspended = static_cast<std::uint8_t>(e.a);
                chronicle(e.a != 0 ? "Die " + w.diet.name + " ruht."
                                   : "Die " + w.diet.name + " tritt wieder zusammen.",
                          kNone);
            }
            return;
        default:
            break;
        }
        if (!in_range(w.states, state)) {
            return; // Wirkungen auf den Staat (der Lader lässt sie in der Welt nicht zu)
        }
        State& s = w.states[at(state)];
        switch (e.kind) {
        case EffectKind::Law:
            if (in_range(s.laws, e.a)) {
                s.laws[at(e.a)] = e.b;
            }
            return;
        case EffectKind::Constitution:
            s.constitution = static_cast<Constitution>(e.a);
            return;
        case EffectKind::Franchise:
            s.law.franchise = static_cast<Franchise>(e.a);
            return;
        case EffectKind::Approval: {
            const std::size_t slot = at(state) * w.interest_groups.size() + at(e.a);
            if (slot < w.approval.size()) {
                w.approval[slot] =
                    std::clamp(w.approval[slot] + e.value, Fixed::from_int(-10), Fixed::from_int(10));
            }
            return;
        }
        case EffectKind::Unrest:
            add_unrest(state, e.value);
            return;
        case EffectKind::SetFlag:
        case EffectKind::ClearFlag:
            set_flag(state, e.name, e.kind == EffectKind::SetFlag);
            return;
        case EffectKind::Treasury:
            s.treasury += e.value;
            return;
        case EffectKind::MonthlyIncome:
            s.treasury += e.value * s.yearly_income / Fixed::from_int(12);
            return;
        case EffectKind::Relation:
            if (!w.relations.empty() && e.a != state && alive(e.a)) {
                adjust_relation(w, state, e.a, e.value);
            }
            return;
        case EffectKind::Emigration:
            emigrate(state, e.value);
            return;
        case EffectKind::Devastation: {
            const Index place = e.a != kNone ? e.a : s.capital;
            if (in_range(w.locations, place) && w.locations[at(place)].pop_count > 0) {
                Location& loc = w.locations[at(place)];
                loc.devastation = std::clamp(loc.devastation + e.value, Fixed::zero(), Fixed::one());
            }
            return;
        }
        case EffectKind::Mobilize:
            if (military_active(w)) {
                order_mobilization(w, state, true);
            }
            return;
        case EffectKind::Timed:
            w.timed_effects.push_back(TimedEffect{state, event_, add_days(w.date, e.days), e.timed, e.name});
            return;
        case EffectKind::Revolt:
            start_revolt(state, static_cast<RevoltKind>(e.a));
            return;
        case EffectKind::Intervention:
            intervene(state, e.a);
            return;
        case EffectKind::Motion:
            if (!w.relations.empty()) {
                file_motion(w, state, static_cast<MotionKind>(e.a), e.b);
            }
            return;
        case EffectKind::War:
            if (alive(e.a)) {
                declare_war(w, state, e.a);
            }
            return;
        default:
            return;
        }
    }

public:
    /// Ein Journal tritt in eine Stufe oder einen Ausgang (Abschnitt 5.1): Zustand, Weg, Chronik, Wirkungen.
    void enter_stage(Index journal, Index stage) {
        if (!in_range(world_.journals, journal) || !in_range(world_.journal_states, journal)) {
            return;
        }
        const JournalDef& j = world_.journals[at(journal)];
        if (!in_range(j.stages, stage) || world_.journal_states[at(journal)].stage == stage) {
            return;
        }
        world_.journal_states[at(journal)] = JournalState{stage, world_.date};
        world_.journal_history.push_back(JournalStep{journal, stage, world_.date});
        const JournalStage& s = j.stages[at(stage)];
        chronicle(s.outcome ? j.title + " endet: " + s.title + " (" + s.rating + ")."
                            : j.title + ": " + s.title + ".",
                  kNone);
        event_ = kNone;
        apply(s.effects, kNone);
    }

    /// Abstimmung nach Fraktionen (Abschnitt 5.3) mit den Abgeordneten der letzten Wahl des Parlaments;
    /// ohne Wahl findet sie nicht statt.
    void hold_vote(Index vote) {
        const VoteDef& v = world_.votes[at(vote)];
        const ElectionResult* election = nullptr;
        for (auto it = world_.elections.rbegin(); it != world_.elections.rend(); ++it) {
            if (it->parliament == v.parliament) {
                election = &*it;
                break;
            }
        }
        if (election == nullptr || election->deputies.empty()) {
            return;
        }
        std::vector<std::int64_t> seats(v.leaning.size() + 1, 0); // letzte: fraktionslos
        for (const Deputy& d : election->deputies) {
            seats[in_range(v.leaning, d.faction) ? at(d.faction) : v.leaning.size()] += 1;
        }
        std::int64_t yes = 0;
        std::int64_t total = 0;
        for (std::size_t f = 0; f < seats.size(); ++f) {
            const Fixed share = f < v.leaning.size() ? v.leaning[f] : v.unaligned;
            yes += (Fixed::from_int(seats[f]) * share).round();
            total += seats[f];
        }
        const bool accepted = Fixed::from_int(yes) > v.majority * Fixed::from_int(total);
        world_.vote_results.push_back(VoteResult{vote, world_.date, static_cast<std::int32_t>(yes),
                                                 static_cast<std::int32_t>(total - yes), accepted});
        const std::string& house = world_.parliaments[at(v.parliament)].name;
        chronicle(house + ": " + v.title + (accepted ? " angenommen (" : " abgelehnt (") +
                      std::to_string(yes) + " : " + std::to_string(total - yes) + ").",
                  kNone);
        event_ = kNone;
        apply(accepted ? v.yes : v.no, kNone);
    }

private:
    bool alive(Index state) const { return in_range(cache_.alive, state) && cache_.alive[at(state)] != 0; }

    /// Ein Aufstand beginnt (Abschnitt 4.4); je Staat höchstens einer.
    void start_revolt(Index state, RevoltKind kind) {
        if (std::any_of(world_.revolts.begin(), world_.revolts.end(),
                        [&](const Revolt& r) { return r.state == state; })) {
            return;
        }
        world_.revolts.push_back(Revolt{state, kind, world_.date, Fixed::zero(), kNone});
        static constexpr std::array<std::string_view, kRevoltKinds> kTexts = {
            "Barrikadenkämpfe in der Hauptstadt", "Agrarunruhen auf dem Land",
            "Erhebung gegen die Regierung"};
        chronicle(world_.states[at(state)].short_name + ": " +
                      std::string(kTexts[static_cast<std::size_t>(kind)]),
                  state);
    }

    /// Der Staat helper greift gegen den Aufstand in target ein (mit Militär macht er mobil).
    void intervene(Index helper, Index target) {
        const auto it = std::find_if(world_.revolts.begin(), world_.revolts.end(),
                                     [&](const Revolt& r) { return r.state == target; });
        if (it == world_.revolts.end() || helper == target || !alive(helper)) {
            return;
        }
        it->helper = helper;
        if (military_active(world_)) {
            order_mobilization(world_, helper, true);
        }
        chronicle(world_.states[at(helper)].short_name + " schickt Truppen gegen den Aufstand in " +
                      world_.states[at(target)].short_name + ".",
                  helper);
    }

public:
    /// Ende eines Aufstands mit seinem Ausgang: feste Wirkungen, Chronik, Folgeereignis der Regeln.
    void end_revolt(std::size_t index, RevoltOutcome outcome) {
        const Revolt revolt = world_.revolts[index];
        world_.revolts.erase(world_.revolts.begin() + static_cast<std::ptrdiff_t>(index));
        const EventRules& r = world_.event_rules;
        State& s = world_.states[at(revolt.state)];
        switch (outcome) {
        case RevoltOutcome::Concession:
            add_unrest(revolt.state, r.concession_unrest);
            chronicle(s.short_name + ": Die Regierung gibt den Aufständischen nach.", revolt.state);
            break;
        case RevoltOutcome::Suppression:
            add_unrest(revolt.state, r.suppression_unrest);
            if (in_range(world_.locations, s.capital)) {
                Location& capital = world_.locations[at(s.capital)];
                capital.devastation =
                    std::clamp(capital.devastation + r.suppression_devastation, Fixed::zero(), Fixed::one());
            }
            chronicle(s.short_name + ": Der Aufstand wird niedergeschlagen.", revolt.state);
            break;
        case RevoltOutcome::Victory:
            if (!r.victory_flag.empty()) {
                set_flag(revolt.state, r.victory_flag, true);
            }
            chronicle(s.short_name + ": Die Aufständischen siegen, der Landesherr flieht.", revolt.state);
            break;
        }
        const Index event = r.outcome_events[static_cast<std::size_t>(outcome)];
        if (in_range(world_.events, event)) {
            const ScheduledEvent next{event, revolt.state, world_.date};
            const auto pos = std::upper_bound(
                world_.scheduled_events.begin(), world_.scheduled_events.end(), next,
                [](const ScheduledEvent& a, const ScheduledEvent& b) { return a.due < b.due; });
            world_.scheduled_events.insert(pos, next);
        }
    }

private:
    void add_unrest(Index state, Fixed delta) {
        if (in_range(world_.unrest, state)) {
            Fixed& u = world_.unrest[at(state)];
            u = std::clamp(u + delta, Fixed::zero(), Fixed::from_int(100));
        }
    }

    void set_flag(Index state, const std::string& name, bool on) {
        std::vector<EventFlag>& flags = world_.event_flags;
        const EventFlag flag{state, name};
        const auto less = [](const EventFlag& a, const EventFlag& b) {
            return a.state != b.state ? a.state < b.state : a.name < b.name;
        };
        const auto it = std::lower_bound(flags.begin(), flags.end(), flag, less);
        const bool present = it != flags.end() && *it == flag;
        if (on && !present) {
            flags.insert(it, flag);
        } else if (!on && present) {
            flags.erase(it);
        }
    }

    /// Ein Anteil der unteren Schicht wandert aus (Bilanz: Auswanderung).
    void emigrate(Index state, Fixed share) {
        World& w = world_;
        const Fixed keep = Fixed::one() - share;
        for (std::size_t li = 0; li < w.locations.size(); ++li) {
            const Location& loc = w.locations[li];
            if (loc.owner != state) {
                continue;
            }
            Fixed removed;
            for (std::int32_t k = 0; k < loc.pop_count; ++k) {
                PopBlock& p = w.pops[at(loc.first_pop + k)];
                if (w.professions[at(p.profession)].stratum != Stratum::Unten) {
                    continue;
                }
                const Fixed before = p.size();
                p.children = p.children * keep;
                p.young = p.young * keep;
                p.working = p.working * keep;
                p.old = p.old * keep;
                removed += before - p.size();
            }
            if (li < w.balance_current.size()) {
                w.balance_current[li].emigrated += removed;
            }
        }
    }

    World& world_;
    Pcg32& rng_;
    EventCache& cache_;
    Index event_ = kNone;
};

bool pending(const World& world, Index event, Index state) {
    return std::any_of(world.pending_events.begin(), world.pending_events.end(),
                       [&](const PendingEvent& p) { return p.event == event && p.state == state; });
}

/// Darf das Ereignis für den Staat (kNone: Welt) heute aus eigener Bedingung eintreten?
bool eligible(const World& world, const EventCache& cache, Index event, Index state) {
    const EventDef& e = world.events[at(event)];
    const std::size_t slot = at(event) * cache.columns + column(cache, e, state);
    const std::int32_t record = slot < cache.last.size() ? cache.last[slot] : -1;
    if (record >= 0) {
        if (!e.repeatable) {
            return false;
        }
        const Date last = world.event_log[static_cast<std::size_t>(record)].date;
        if (to_day_number(world.date) - to_day_number(last) < e.spacing) {
            return false;
        }
    }
    return !pending(world, event, state);
}

/// Prüfungen des Datums auf oberster Ebene: hängen nicht vom Staat ab und schließen das Fenster.
bool date_window_open(const World& world, const EventDef& e) {
    for (const Condition& c : e.condition.children) {
        if (c.kind == ConditionKind::Date && !compare(c.op, world.date, c.date)) {
            return false;
        }
    }
    return true;
}

/// Tritt das Ereignis heute ein (Wartezeit)? Am Termin und ohne Wartezeit sofort.
bool happens_today(const EventDef& e, Pcg32& rng) {
    if (e.has_fixed_date || e.wait <= 0) {
        return true;
    }
    return rng.below(static_cast<std::uint32_t>(e.wait)) == 0;
}

void run_timed_effects(World& world, const EventCache& cache) {
    auto& list = world.timed_effects;
    list.erase(
        std::remove_if(list.begin(), list.end(), [&](const TimedEffect& t) { return t.until <= world.date; }),
        list.end());
    if (world.date.day != 1) {
        return;
    }
    for (const TimedEffect& t : list) {
        if (t.values.unrest != Fixed::zero() && in_range(world.unrest, t.state)) {
            Fixed& u = world.unrest[at(t.state)];
            u = std::clamp(u + t.values.unrest, Fixed::zero(), Fixed::from_int(100));
        }
        if (t.values.relation != Fixed::zero() && !world.relations.empty()) {
            for (std::size_t s = 0; s < world.states.size(); ++s) {
                const auto other = static_cast<Index>(s);
                if (other != t.state && in_range(cache.alive, other) && cache.alive[s] != 0 &&
                    federal_state(world, other)) {
                    adjust_relation(world, t.state, other, t.values.relation);
                }
            }
        }
    }
}

/// Zielwert der Zustimmung einer Gruppe aus den Gesetzen des Staates (Abschnitt 4.2), −10 … 10.
Fixed approval_target(const World& world, Index state, const InterestGroup& group) {
    const State& s = world.states[at(state)];
    Fixed target;
    for (const LawWish& wish : group.wishes) {
        if (in_range(s.laws, wish.law) && s.laws[at(wish.law)] == wish.option) {
            target += wish.approval;
        }
    }
    return std::clamp(target, Fixed::from_int(-10), Fixed::from_int(10));
}

} // namespace

Fixed unrest_target(const World& world, const EventCache& cache, Index state) {
    const EventRules& r = world.event_rules;
    if (!in_range(world.states, state)) {
        return Fixed::zero();
    }
    Evaluator evaluator(world, cache);
    const State& s = world.states[at(state)];
    const Fixed two = Fixed::from_int(2);
    const Fixed misery =
        std::clamp((r.misery_from - evaluator.living_lower(state)) / r.misery_span, Fixed::zero(), two);
    Fixed dearth;
    if (in_range(world.goods, r.grain)) {
        dearth = std::clamp((evaluator.price_factor(s, r.grain) - r.dearth_from) / r.dearth_span,
                            Fixed::zero(), two);
    }
    Fixed censor;
    for (std::size_t g = 0; g < s.laws.size() && g < world.law_groups.size(); ++g) {
        const LawGroup& group = world.law_groups[g];
        if (in_range(group.options, s.laws[g])) {
            censor += group.options[at(s.laws[g])].unrest;
        }
    }
    std::int64_t restless = 0;
    std::int64_t total = 0;
    if (in_range(cache.neighbors, state)) {
        for (const Index n : cache.neighbors[at(state)]) {
            if (evaluator.alive(n)) {
                ++total;
                restless += in_range(world.unrest, n) && world.unrest[at(n)] >= r.contagion_from ? 1 : 0;
            }
        }
    }
    if (!r.contagion_flag.empty() && has_flag(world, kNone, r.contagion_flag)) {
        ++total;
        ++restless;
    }
    const Fixed contagion = total > 0 ? r.contagion * Fixed::from_ratio(restless, total) : Fixed::zero();
    Fixed approval;
    const std::vector<InterestGroupStanding>& standing = evaluator.standing_of(state);
    for (std::size_t g = 0; g < standing.size() && g < world.interest_groups.size(); ++g) {
        approval += standing[g].influence * evaluator.approval(state, static_cast<Index>(g));
    }
    const Fixed target = r.misery * misery + r.dearth * dearth +
                         r.constitution[static_cast<std::size_t>(s.constitution)] + censor + contagion -
                         r.approval_weight * approval;
    return std::clamp(target, Fixed::zero(), Fixed::from_int(100));
}

namespace {

/// Am Monatsersten: Zustimmung und Unruhe nähern sich ihren Zielwerten (Abschnitte 4.2 und 4.3). Die
/// Zielwerte der Unruhe entstehen alle aus dem Stand vor der Annäherung (Ansteckung unabhängig von der
/// Reihenfolge der Staaten).
void update_approval_unrest(World& world, const EventCache& cache) {
    const EventRules& r = world.event_rules;
    const std::size_t groups = world.interest_groups.size();
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        if (cache.alive[s] == 0) {
            continue;
        }
        for (std::size_t g = 0; g < groups; ++g) {
            Fixed& a = world.approval[s * groups + g];
            const Fixed target = approval_target(world, static_cast<Index>(s), world.interest_groups[g]);
            a = std::clamp(a + (target - a) * r.approval_rate, Fixed::from_int(-10), Fixed::from_int(10));
        }
    }
    std::vector<Fixed> targets(world.states.size());
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        if (cache.alive[s] != 0) {
            targets[s] = unrest_target(world, cache, static_cast<Index>(s));
        }
    }
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        if (cache.alive[s] != 0) {
            Fixed& u = world.unrest[s];
            u = std::clamp(u + (targets[s] - u) * r.unrest_rate, Fixed::zero(), Fixed::from_int(100));
        }
    }
}

/// Anteil der Bevölkerung des Staates, der sich bei dieser Art erhebt: Städter, Bauern oder alle.
Fixed mobilized_share(const World& world, Index state, RevoltKind kind) {
    if (kind == RevoltKind::Uprising) {
        return Fixed::one();
    }
    Fixed part;
    Fixed all;
    for (const Location& loc : world.locations) {
        if (loc.owner != state) {
            continue;
        }
        for (std::int32_t k = 0; k < loc.pop_count; ++k) {
            const PopBlock& p = world.pops[at(loc.first_pop + k)];
            const Fixed size = p.size();
            all += size;
            const bool counts =
                kind == RevoltKind::Barricades ? loc.is_city : world.professions[at(p.profession)].agrarian;
            part += counts ? size : Fixed::zero();
        }
    }
    return all > Fixed::zero() ? part / all : Fixed::zero();
}

/// Soldaten, mit denen ein Staat eingreift: sein größtes Heer, ohne Militär ein Viertel des Friedensheeres.
Fixed helper_soldiers(const World& world, Index helper) {
    if (military_active(world)) {
        Fixed largest;
        for (const Army& a : world.armies) {
            if (a.state == helper) {
                largest = Fixed::max(largest, a.soldiers);
            }
        }
        return largest;
    }
    const State& s = world.states[at(helper)];
    return s.army_per_mille * world.state_population(helper) / Fixed::from_int(4000);
}

/// Kräfte eines Aufstands (Abschnitt 4.4): Aufständische (Mobilisierung, Elitenspaltung) und Regierung
/// (Loyalität der Truppen, Intervention).
std::pair<Fixed, Fixed> revolt_forces(const World& world, const EventCache& cache, const Revolt& revolt) {
    const EventRules& r = world.event_rules;
    Evaluator evaluator(world, cache);
    const Index state = revolt.state;
    const Fixed half = Fixed::from_ratio(1, 2);
    const Fixed two = Fixed::from_int(2);
    const Fixed unrest = in_range(world.unrest, state) ? world.unrest[at(state)] : Fixed::zero();
    const Fixed mobilization =
        unrest / Fixed::from_int(100) * (half + mobilized_share(world, state, revolt.kind));
    std::int64_t splits = 0;
    for (const Index g : r.split_groups) {
        splits += evaluator.approval(state, g) < Fixed::zero() ? 1 : 0;
    }
    if (!r.split_flag.empty() && has_flag(world, state, r.split_flag)) {
        ++splits;
    }
    const Fixed rebels = mobilization + r.split * Fixed::from_int(splits);

    const Fixed population = world.state_population(state);
    const Fixed per_mille = population > Fixed::zero()
                                ? evaluator.soldiers(state) * Fixed::from_int(1000) / population
                                : Fixed::zero();
    const Fixed loyalty_share =
        in_range(world.interest_groups, r.loyalty_group)
            ? (evaluator.approval(state, r.loyalty_group) + Fixed::from_int(10)) / Fixed::from_int(20)
            : half;
    const Fixed loyalty = loyalty_share * Fixed::min(two, per_mille / r.soldiers_reference);
    Fixed intervention;
    if (evaluator.alive(revolt.helper) && population > Fixed::zero()) {
        const Fixed helper_per_mille =
            helper_soldiers(world, revolt.helper) * Fixed::from_int(1000) / population;
        intervention = Fixed::min(two, helper_per_mille / r.soldiers_reference);
    }
    return {rebels, loyalty + intervention};
}

/// Aufstände: jede Woche ein Kräftevergleich mit Zufall aus dem Strom Ereignisse (Abschnitt 4.4).
void run_revolts(World& world, Pcg32& rng, const EventCache& cache, Executor& executor) {
    const EventRules& r = world.event_rules;
    for (std::size_t i = 0; i < world.revolts.size();) {
        Revolt& revolt = world.revolts[i];
        const std::int64_t days = to_day_number(world.date) - to_day_number(revolt.start);
        if (cache.alive[at(revolt.state)] == 0) {
            world.revolts.erase(world.revolts.begin() + static_cast<std::ptrdiff_t>(i));
            continue;
        }
        if (days >= r.revolt_max_days) {
            executor.end_revolt(i, revolt.strength > Fixed::zero() ? RevoltOutcome::Concession
                                                                   : RevoltOutcome::Suppression);
            continue;
        }
        if (days < r.revolt_min_days || days % 7 != 0) {
            ++i;
            continue;
        }
        const auto [rebels, government] = revolt_forces(world, cache, revolt);
        revolt.strength +=
            r.revolt_step * (rebels - government) + rng.uniform(-r.revolt_noise, r.revolt_noise);
        if (revolt.strength >= Fixed::one()) {
            executor.end_revolt(i, RevoltOutcome::Victory);
        } else if (revolt.strength <= -Fixed::one()) {
            executor.end_revolt(i, RevoltOutcome::Suppression);
        } else if (revolt.strength > Fixed::zero() && rng.chance(revolt.strength * r.revolt_concession)) {
            executor.end_revolt(i, RevoltOutcome::Concession);
        } else {
            ++i;
        }
    }
}

void run_scheduled(World& world, Executor& executor, const EventCache& cache) {
    for (int guard = 0; guard < kMaxFollowsPerDay; ++guard) {
        if (world.scheduled_events.empty() || world.date < world.scheduled_events.front().due) {
            return;
        }
        const ScheduledEvent next = world.scheduled_events.front();
        world.scheduled_events.erase(world.scheduled_events.begin());
        const EventDef& e = world.events[at(next.event)];
        const bool exists = e.scope == EventScope::World ||
                            (in_range(cache.alive, next.state) && cache.alive[at(next.state)] != 0);
        if (!exists || pending(world, next.event, next.state)) {
            continue;
        }
        if (condition_holds(world, cache, e.condition, next.state)) {
            executor.fire(next.event, next.state);
        }
    }
}

} // namespace

bool events_active(const World& world) {
    return world.event_rules.active;
}

void ensure_events_state(World& world) {
    if (!events_active(world)) {
        return;
    }
    if (world.journal_states.size() != world.journals.size()) {
        world.journal_states.assign(world.journals.size(), JournalState{});
    }
    const std::size_t groups = world.states.size() * world.interest_groups.size();
    if (world.approval.size() != groups) {
        world.approval.assign(groups, Fixed::zero());
    }
    if (world.unrest.size() != world.states.size()) {
        world.unrest.assign(world.states.size(), Fixed::zero());
    }
}

void rebuild_event_cache(const World& world, EventCache& cache) {
    const std::size_t states = world.states.size();
    cache.columns = states + 1;
    cache.last.assign(world.events.size() * cache.columns, -1);
    for (std::size_t i = 0; i < world.event_log.size(); ++i) {
        const EventRecord& r = world.event_log[i];
        if (!in_range(world.events, r.event)) {
            continue;
        }
        const std::size_t slot =
            at(r.event) * cache.columns + column(cache, world.events[at(r.event)], r.state);
        if (slot < cache.last.size()) {
            cache.last[slot] = static_cast<std::int32_t>(i);
        }
    }
    cache.alive.assign(states, 0);
    for (const Location& loc : world.locations) {
        if (in_range(world.states, loc.owner) && loc.pop_count > 0) {
            cache.alive[at(loc.owner)] = 1;
        }
    }
    std::vector<std::set<Index>> sets(states);
    for (std::size_t l = 0; l < world.neighbors.size() && l < world.locations.size(); ++l) {
        const Index a = world.locations[l].owner;
        if (!in_range(world.states, a)) {
            continue;
        }
        for (const Neighbor& n : world.neighbors[l]) {
            const Index b =
                in_range(world.locations, n.location) ? world.locations[at(n.location)].owner : kNone;
            if (in_range(world.states, b) && b != a) {
                sets[at(a)].insert(b);
            }
        }
    }
    cache.neighbors.assign(states, {});
    for (std::size_t s = 0; s < states; ++s) {
        cache.neighbors[s].assign(sets[s].begin(), sets[s].end());
    }
}

bool condition_holds(const World& world, const EventCache& cache, const Condition& condition, Index state) {
    Evaluator evaluator(world, cache);
    return evaluator.holds(condition, state);
}

std::vector<Fixed> option_weights(const World& world, const EventCache& cache, const EventDef& event,
                                  Index state) {
    Evaluator evaluator(world, cache);
    std::vector<Fixed> weights;
    weights.reserve(event.options.size());
    for (const EventOption& o : event.options) {
        if (!evaluator.holds(o.condition, state)) {
            weights.push_back(Fixed::from_int(-1));
            continue;
        }
        Fixed weight = o.weight;
        for (const WeightModifier& m : o.weight_modifiers) {
            if (evaluator.holds(m.condition, state)) {
                weight += m.plus;
            }
        }
        weights.push_back(Fixed::max(weight, Fixed::zero()));
    }
    return weights;
}

Index default_option(const World& world, const EventCache& cache, const EventDef& event, Index state) {
    const std::vector<Fixed> weights = option_weights(world, cache, event, state);
    Index best = kNone;
    for (std::size_t i = 0; i < weights.size(); ++i) {
        if (weights[i] >= Fixed::zero() && (best == kNone || weights[i] > weights[at(best)])) {
            best = static_cast<Index>(i);
        }
    }
    return best == kNone ? 0 : best;
}

void run_events(World& world, Pcg32& rng, EventCache& cache) {
    if (!events_active(world)) {
        return;
    }
    if (cache.columns != world.states.size() + 1 ||
        cache.last.size() != world.events.size() * cache.columns) {
        rebuild_event_cache(world, cache);
    }
    run_timed_effects(world, cache);
    if (world.date.day == 1) {
        update_approval_unrest(world, cache);
    }
    Executor executor(world, rng, cache);
    run_revolts(world, rng, cache, executor);

    // Fristen des Spielers (Abschnitt 3.6): ohne Wahl gilt die Option mit dem größten Gewicht.
    for (std::size_t i = 0; i < world.pending_events.size();) {
        const PendingEvent p = world.pending_events[i];
        if (world.date < p.due) {
            ++i;
            continue;
        }
        world.pending_events.erase(world.pending_events.begin() + static_cast<std::ptrdiff_t>(i));
        executor.resolve(p.event, p.state, default_option(world, cache, world.events[at(p.event)], p.state));
    }

    run_scheduled(world, executor, cache);

    // Auslösung (Abschnitt 3.4) in der Reihenfolge der Kennungen, je Staat in der Reihenfolge der Staaten.
    for (std::size_t i = 0; i < world.events.size(); ++i) {
        const EventDef& e = world.events[i];
        const auto event = static_cast<Index>(i);
        if (e.follow_only || (e.has_fixed_date && e.fixed_date != world.date) ||
            !date_window_open(world, e)) {
            continue;
        }
        if (e.scope == EventScope::World) {
            if (eligible(world, cache, event, kNone) && condition_holds(world, cache, e.condition, kNone) &&
                happens_today(e, rng)) {
                executor.fire(event, kNone);
            }
            continue;
        }
        for (std::size_t s = 0; s < world.states.size(); ++s) {
            const auto state = static_cast<Index>(s);
            if (cache.alive[s] == 0 || !eligible(world, cache, event, state) ||
                !condition_holds(world, cache, e.condition, state) || !happens_today(e, rng)) {
                continue;
            }
            executor.fire(event, state);
        }
    }

    // Folgeereignisse ohne Frist aus den heutigen Ereignissen.
    run_scheduled(world, executor, cache);

    // Journale nach den Ereignissen (Abschnitt 5.1): Beginn, am Ende der Ausgang mit standard = ja, sonst
    // höchstens ein Übergang je Tag; dann die Folgen ohne Frist ihrer Wirkungen.
    for (std::size_t j = 0; j < world.journals.size() && j < world.journal_states.size(); ++j) {
        const JournalDef& journal = world.journals[j];
        const auto index = static_cast<Index>(j);
        if (world.journal_states[j].stage == kNone) {
            if (journal.has_begin && world.date < journal.begin) {
                continue;
            }
            executor.enter_stage(index, 0);
        }
        const Index stage = world.journal_states[j].stage;
        if (journal.stages[at(stage)].outcome) {
            continue;
        }
        if (journal.has_end && journal.end <= world.date) {
            executor.enter_stage(index, journal.fallback);
            continue;
        }
        for (const JournalTransition& t : journal.stages[at(stage)].transitions) {
            if (condition_holds(world, cache, t.condition, kNone)) {
                executor.enter_stage(index, t.target);
                break;
            }
        }
    }
    run_scheduled(world, executor, cache);
}

bool choose_event_option(World& world, Pcg32& rng, EventCache& cache, Index state, Index event,
                         Index option) {
    if (!events_active(world) || !in_range(world.events, event)) {
        return false;
    }
    const auto it = std::find_if(world.pending_events.begin(), world.pending_events.end(),
                                 [&](const PendingEvent& p) { return p.event == event && p.state == state; });
    const EventDef& e = world.events[at(event)];
    if (it == world.pending_events.end() || !in_range(e.options, option)) {
        return false;
    }
    if (cache.columns != world.states.size() + 1 ||
        cache.last.size() != world.events.size() * cache.columns) {
        rebuild_event_cache(world, cache);
    }
    const std::vector<Fixed> weights = option_weights(world, cache, e, state);
    if (weights[at(option)] < Fixed::zero()) {
        return false;
    }
    world.pending_events.erase(it);
    Executor(world, rng, cache).resolve(event, state, option);
    return true;
}

bool set_event_player(World& world, Index state) {
    if (!events_active(world) || (state != kNone && !in_range(world.states, state))) {
        return false;
    }
    world.event_player = state;
    return true;
}

Fixed event_tax_factor(const World& world, Index state) {
    Fixed factor = timed_tax_factor(world, state);
    if (in_range(world.unrest, state) && world.unrest[at(state)] >= world.event_rules.tax_refusal_from) {
        factor = factor * (Fixed::one() - world.event_rules.tax_refusal);
    }
    return factor;
}

Fixed unrest_emigration_factor(const World& world, Index state) {
    if (in_range(world.unrest, state) && world.unrest[at(state)] >= world.event_rules.emigration_from) {
        return Fixed::one() + world.event_rules.emigration;
    }
    return Fixed::one();
}

Fixed timed_tax_factor(const World& world, Index state) {
    Fixed factor = Fixed::one();
    for (const TimedEffect& t : world.timed_effects) {
        if (t.state == state) {
            factor += t.values.tax;
        }
    }
    return Fixed::max(factor, Fixed::zero());
}

Fixed timed_morale_factor(const World& world, Index state) {
    Fixed factor = Fixed::one();
    for (const TimedEffect& t : world.timed_effects) {
        if (t.state == state) {
            factor += t.values.morale;
        }
    }
    return Fixed::max(factor, Fixed::zero());
}

} // namespace uhrwerk
