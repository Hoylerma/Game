#include "uhrwerk/ai.hpp"

#include "uhrwerk/diplomacy.hpp"
#include "uhrwerk/military.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace uhrwerk {

namespace {

std::size_t at(Index index) {
    return static_cast<std::size_t>(index);
}

template <typename T> bool in_range(const std::vector<T>& items, Index index) {
    return index >= 0 && at(index) < items.size();
}

std::size_t office_slot(Index state, PersonRole role) {
    return at(state) * kPersonRoles + static_cast<std::size_t>(role);
}

/// Bezeichnung des Amtes in der Chronik: eigenes 'amt' der Person, sonst nach Rolle.
std::string office_title(const PersonDef& person) {
    if (!person.title.empty()) {
        return person.title;
    }
    return person.role == PersonRole::Ruler ? "Herrscher" : "leitender Minister";
}

void fill_offices(World& world, const EventCache& cache, bool chronicle) {
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        for (std::size_t r = 0; r < kPersonRoles; ++r) {
            const auto state = static_cast<Index>(s);
            const auto role = static_cast<PersonRole>(r);
            const Index next = person_for_office(world, cache, state, role);
            Index& current = world.offices[office_slot(state, role)];
            if (next == current) {
                continue;
            }
            if (chronicle) {
                ChronicleEntry entry;
                entry.date = world.date;
                entry.state = state;
                if (next != kNone) {
                    const PersonDef& person = world.persons[at(next)];
                    entry.text =
                        world.states[s].short_name + ": " + person.name + " wird " + office_title(person);
                } else {
                    const PersonDef& person = world.persons[at(current)];
                    entry.text = world.states[s].short_name + ": " + person.name + " scheidet aus dem Amt";
                }
                world.chronicle.push_back(std::move(entry));
            }
            current = next;
        }
    }
}

} // namespace

bool ai_active(const World& world) {
    return world.ai_rules.active;
}

void ensure_ai_state(World& world, const EventCache& cache) {
    if (!ai_active(world)) {
        return;
    }
    if (world.ai_strategy.size() != world.states.size()) {
        world.ai_strategy.assign(world.states.size(), AiStrategy{});
    }
    const std::size_t slots = world.states.size() * kPersonRoles;
    if (world.offices.size() != slots) {
        world.offices.assign(slots, kNone);
        fill_offices(world, cache, false);
    }
}

Index person_for_office(const World& world, const EventCache& cache, Index state, PersonRole role) {
    Index best = kNone;
    for (std::size_t i = 0; i < world.persons.size(); ++i) {
        const PersonDef& p = world.persons[i];
        if (p.state != state || p.role != role || world.date < p.from || (p.has_to && p.to < world.date)) {
            continue;
        }
        if (p.has_condition && !condition_holds(world, cache, p.condition, state)) {
            continue;
        }
        if (best == kNone || world.persons[at(best)].from < p.from) {
            best = static_cast<Index>(i);
        }
    }
    return best;
}

void update_offices(World& world, const EventCache& cache) {
    if (!ai_active(world)) {
        return;
    }
    ensure_ai_state(world, cache);
    fill_offices(world, cache, true);
}

Fixed ruler_weight(const World& world, Index state) {
    if (in_range(world.states, state) &&
        world.states[at(state)].constitution == Constitution::Constitutional) {
        return world.ai_rules.ruler_weight_constitutional;
    }
    return world.ai_rules.ruler_weight_absolute;
}

Inclinations state_inclinations(const World& world, Index state) {
    Inclinations result{};
    if (!ai_active(world) || !in_range(world.states, state)) {
        return result;
    }
    Index personality = world.ai_rules.default_personality;
    if (in_range(world.agendas, state) && world.agendas[at(state)].personality != kNone) {
        personality = world.agendas[at(state)].personality;
    }
    if (in_range(world.ai_personalities, personality)) {
        result = world.ai_personalities[at(personality)].inclinations;
    }
    const Fixed ruler = ruler_weight(world, state);
    for (std::size_t r = 0; r < kPersonRoles; ++r) {
        const Index person = world.office(state, static_cast<PersonRole>(r));
        if (!in_range(world.persons, person)) {
            continue;
        }
        const Fixed weight = r == static_cast<std::size_t>(PersonRole::Ruler) ? ruler : Fixed::one() - ruler;
        for (const Index trait : world.persons[at(person)].traits) {
            for (std::size_t k = 0; k < kInclinationCount; ++k) {
                result[k] += weight * world.ai_traits[at(trait)].inclinations[k];
            }
        }
    }
    const Fixed limit = Fixed::from_int(2);
    for (Fixed& value : result) {
        value = std::clamp(value, -limit, limit);
    }
    return result;
}

bool agenda_goal_active(const World& world, const EventCache& cache, Index state, const AgendaGoal& goal) {
    if (!world.ai_rules.open_agenda) {
        if ((goal.has_from && world.date < goal.from) || (goal.has_to && goal.to < world.date)) {
            return false;
        }
    }
    return !goal.has_condition || condition_holds(world, cache, goal.condition, state);
}

// ─── Lagebild (Abschnitt 4.3) ───────────────────────────────────────────────

Fixed ai_strength(const World& world, Index state) {
    if (!military_active(world) || !in_range(world.states, state)) {
        return Fixed::zero();
    }
    const Fixed reserve = peace_strength(world, state) * world.states[at(state)].army.mobilization;
    return std::max(state_soldiers(world, state), reserve);
}

Fixed ai_threat(const World& world, const EventCache& cache, Index state, Index other) {
    if (state == other || !in_range(world.states, state) || !in_range(world.states, other)) {
        return Fixed::zero();
    }
    const Fixed own = ai_strength(world, state);
    const Fixed theirs = ai_strength(world, other);
    if (own <= Fixed::zero() || theirs <= Fixed::zero()) {
        return Fixed::zero();
    }
    // Feindschaft aus der Beziehung (−200 … +200): unter 0 wächst sie, darüber sinkt sie bis auf ½.
    const Fixed r = relation(world, state, other);
    const Fixed hostility =
        r < Fixed::zero() ? Fixed::one() - r / Fixed::from_int(100) : Fixed::one() - r / Fixed::from_int(400);
    // Nähe: Nachbarn 1, im selben Bund 0,6, sonst 0,3.
    Fixed proximity = Fixed::from_ratio(3, 10);
    const auto& neighbors =
        in_range(cache.neighbors, state) ? cache.neighbors[at(state)] : std::vector<Index>{};
    if (std::find(neighbors.begin(), neighbors.end(), other) != neighbors.end()) {
        proximity = Fixed::one();
    } else if (federal_state(world, state) && federal_state(world, other)) {
        proximity = Fixed::from_ratio(3, 5);
    }
    return theirs / own * hostility * proximity;
}

// ─── Gründe und Protokoll (Abschnitt 5) ─────────────────────────────────────

std::string ai_reason_text(const World& world, const AiReason& reason) {
    static constexpr std::array<std::string_view, kAiFactors> names = {
        "Defizit", "Schuldenquote", "Überschuss",      "Unruhe", "Sparsamkeit",
        "Reform",  "Risiko",        "Bedrohung durch", "mobil:", "Frieden"};
    std::string text(names[static_cast<std::size_t>(reason.factor)]);
    if (in_range(world.states, reason.other)) {
        text += " " + world.states[at(reason.other)].short_name;
    }
    const std::int64_t value = reason.value.round();
    text += value >= 0 ? " +" : " −";
    text += std::to_string(value >= 0 ? value : -value);
    return text;
}

std::string ai_decision_text(const World& /*world*/, const AiDecision& decision) {
    static constexpr std::array<std::string_view, kTaxKinds> taxes = {"die Kontribution", "die Akzise",
                                                                      "den Zoll"};
    switch (decision.kind) {
    case AiDecisionKind::RaiseTax:
    case AiDecisionKind::LowerTax: {
        const std::string_view tax = decision.target >= 0 && at(decision.target) < kTaxKinds
                                         ? taxes[at(decision.target)]
                                         : "die Steuer";
        return std::string(decision.kind == AiDecisionKind::RaiseTax ? "erhöht " : "senkt ") +
               std::string(tax) + " auf " + std::to_string((decision.value * Fixed::from_int(100)).round()) +
               " %";
    }
    case AiDecisionKind::Mobilize:
        return "macht mobil";
    case AiDecisionKind::Demobilize:
        return "rüstet ab";
    }
    return {};
}

namespace {

/// Eine bewertete Option: Faktoren und Nutzen (Summe + Rauschen).
struct Option {
    std::vector<AiReason> factors;
    Fixed utility;

    void add(AiFactor factor, Fixed value, Index other = kNone) {
        if (value != Fixed::zero()) {
            factors.push_back(AiReason{factor, other, value});
        }
    }

    Fixed sum() const {
        Fixed total;
        for (const AiReason& r : factors) {
            total += r.value;
        }
        return total;
    }
};

/// Planer eines Monatsersten: liest das Lagebild, schreibt Protokoll, Abkühlung und Befehle.
class Planner {
public:
    Planner(World& world, Pcg32& rng, const EventCache& cache, std::vector<Command>& out)
        : w_(world), rng_(rng), cache_(cache), out_(out) {}

    void run() {
        const std::size_t states = w_.states.size();
        expire_cooldowns();
        for (std::size_t s = 0; s < states; ++s) {
            const auto state = static_cast<Index>(s);
            if (state == w_.event_player || !in_range(cache_.alive, state) || cache_.alive[s] == 0) {
                continue;
            }
            inclinations_ = state_inclinations(w_, state);
            update_strategy(state);
            plan_budget(state);
            plan_mobilization(state);
        }
    }

private:
    Fixed inclination(Inclination i) const { return inclinations_[static_cast<std::size_t>(i)]; }

    /// Nutzen mit Rauschen aus dem Strom Ki (nur für Optionen, die ihre Bedingung erfüllen).
    void rate(Option& option) {
        const Fixed noise = w_.ai_rules.noise;
        option.utility = option.sum() + (noise > Fixed::zero() ? rng_.uniform(-noise, noise) : Fixed::zero());
    }

    bool cooling(Index state, AiDecisionKind kind, Index target) const {
        return std::any_of(w_.ai_cooldowns.begin(), w_.ai_cooldowns.end(), [&](const AiCooldown& c) {
            return c.state == state && c.kind == kind && c.target == target;
        });
    }

    void cool(Index state, AiDecisionKind kind, Index target, std::int32_t months) {
        if (months <= 0) {
            return;
        }
        // Bis zum Monatsersten months Monate später.
        const std::int32_t total = w_.date.year * 12 + (w_.date.month - 1) + months;
        w_.ai_cooldowns.push_back(AiCooldown{state, kind, target, Date{total / 12, total % 12 + 1, 1}});
    }

    void expire_cooldowns() {
        std::erase_if(w_.ai_cooldowns, [&](const AiCooldown& c) { return c.until <= w_.date; });
    }

    /// Eintrag im Protokoll mit den drei Faktoren mit dem größten Betrag; je Staat höchstens log_size.
    void record(Index state, AiDecisionKind kind, Index target, Fixed value, const Option& option) {
        AiDecision d;
        d.date = w_.date;
        d.state = state;
        d.kind = kind;
        d.target = target;
        d.value = value;
        d.utility = option.utility;
        std::vector<AiReason> sorted = option.factors;
        std::stable_sort(sorted.begin(), sorted.end(), [](const AiReason& a, const AiReason& b) {
            return (a.value < Fixed::zero() ? -a.value : a.value) >
                   (b.value < Fixed::zero() ? -b.value : b.value);
        });
        d.reason_count = static_cast<std::uint8_t>(std::min(sorted.size(), kAiReasons));
        for (std::size_t i = 0; i < d.reason_count; ++i) {
            d.reasons[i] = sorted[i];
        }
        w_.ai_log.push_back(d);
        const auto limit = static_cast<std::size_t>(std::max(w_.ai_rules.log_size, 1));
        std::size_t count = 0;
        for (const AiDecision& e : w_.ai_log) {
            count += e.state == state ? 1 : 0;
        }
        if (count > limit) {
            const auto oldest = std::find_if(w_.ai_log.begin(), w_.ai_log.end(),
                                             [&](const AiDecision& e) { return e.state == state; });
            w_.ai_log.erase(oldest);
        }
    }

    // ── Strategie (Abschnitt 4.2) ──

    void update_strategy(Index state) {
        AiStrategy& strategy = w_.ai_strategy[at(state)];
        const AgendaDef* agenda = in_range(w_.agendas, state) ? &w_.agendas[at(state)] : nullptr;
        // Hauptziel: aktives Ziel mit dem größten Gewicht × (1 + ¼ × passende Neigung).
        strategy.goal = kNone;
        Index rival = kNone;
        Fixed best;
        Fixed best_weaken;
        if (agenda != nullptr) {
            for (std::size_t g = 0; g < agenda->goals.size(); ++g) {
                const AgendaGoal& goal = agenda->goals[g];
                if (!agenda_goal_active(w_, cache_, state, goal)) {
                    continue;
                }
                Fixed lean;
                switch (goal.kind) {
                case AgendaKind::LeadFederation:
                case AgendaKind::Territory:
                case AgendaKind::Annexation:
                case AgendaKind::Weaken:
                    lean = inclination(Inclination::Expansion);
                    break;
                case AgendaKind::StatusQuo:
                    lean = inclination(Inclination::Legitimacy);
                    break;
                case AgendaKind::Calm:
                    lean = inclination(Inclination::Reform);
                    break;
                case AgendaKind::CustomsUnion:
                case AgendaKind::Alliance:
                    break;
                }
                const Fixed score = goal.weight * (Fixed::one() + lean / Fixed::from_int(4));
                if (strategy.goal == kNone || score > best) {
                    strategy.goal = static_cast<Index>(g);
                    best = score;
                }
                if (goal.kind == AgendaKind::Weaken && (rival == kNone || goal.weight > best_weaken)) {
                    rival = goal.target;
                    best_weaken = goal.weight;
                }
            }
        }
        // Rivale: Ziel von „schwaechen“, sonst die größte Bedrohung über 0.
        if (rival == kNone) {
            Fixed most;
            for (std::size_t o = 0; o < w_.states.size(); ++o) {
                const auto other = static_cast<Index>(o);
                if (other == state || cache_.alive[o] == 0) {
                    continue;
                }
                const Fixed threat = ai_threat(w_, cache_, state, other);
                if (threat > most) {
                    most = threat;
                    rival = other;
                }
            }
        }
        strategy.rival = rival;
    }

    // ── Haushalt und Steuern (Abschnitt 6.1) ──

    void plan_budget(Index state) {
        const State& s = w_.states[at(state)];
        const AiRules& r = w_.ai_rules;
        const Fixed income = s.last_month_income;
        if (income <= Fixed::zero() || cooling(state, AiDecisionKind::RaiseTax, kNone)) {
            return;
        }
        const Fixed saldo = income - s.last_month_expenses;
        const Fixed debt_ratio = s.debt / std::max(s.yearly_income, Fixed::one());
        const bool war = military_active(w_) && in_war(w_, state);
        const bool armed = military_active(w_) && mobilization_phase(w_, state) != MobilizationPhase::Peace;
        const Fixed unrest = in_range(w_.unrest, state) ? w_.unrest[at(state)] : Fixed::zero();
        const Fixed hundred = Fixed::from_int(100);
        Option raise;
        bool can_raise = false;
        Fixed deficit;
        if (saldo < Fixed::zero() && debt_ratio > r.debt_limit) {
            Fixed share = -saldo / income;
            if (war || armed) {
                share = share / Fixed::from_int(2); // im Krieg zählt das Defizit halb
            }
            deficit = std::min(hundred * share, Fixed::from_int(50));
            raise.add(AiFactor::Deficit, deficit);
            raise.add(AiFactor::DebtRatio,
                      std::min(Fixed::from_int(10) * (debt_ratio - r.debt_limit), Fixed::from_int(30)));
            raise.add(AiFactor::Thrift, Fixed::from_int(10) * inclination(Inclination::Thrift));
            raise.add(AiFactor::Unrest, -unrest / Fixed::from_int(2));
            raise.add(AiFactor::Reform, Fixed::from_int(-5) * inclination(Inclination::Reform));
            rate(raise);
            can_raise = true;
        }
        Option lower;
        bool can_lower = false;
        // Ein Überschuss senkt die Steuern erst, wenn eine Rücklage von reserve_from Jahreseinnahmen da ist
        // (sonst fließt er in Schatz und Tilgung).
        const bool rich = saldo > r.surplus_from * income && debt_ratio < Fixed::one() &&
                          s.treasury >= r.reserve_from * std::max(s.yearly_income, Fixed::one());
        const bool restless = unrest > r.unrest_tax_from && !war;
        if (rich || restless) {
            if (rich) {
                lower.add(AiFactor::Surplus,
                          std::min(hundred * (saldo / income - r.surplus_from), Fixed::from_int(50)));
            }
            if (restless) {
                lower.add(AiFactor::Unrest, (unrest - r.unrest_tax_from) / Fixed::from_int(2));
            }
            lower.add(AiFactor::Thrift, Fixed::from_int(-10) * inclination(Inclination::Thrift));
            lower.add(AiFactor::Reform, Fixed::from_int(5) * inclination(Inclination::Reform));
            rate(lower);
            can_lower = true;
        }
        const Fixed point = Fixed::from_ratio(1, 100);
        const auto k = static_cast<std::size_t>(TaxKind::Kontribution);
        const auto a = static_cast<std::size_t>(TaxKind::Akzise);
        if (can_raise && raise.utility >= r.threshold && (!can_lower || raise.utility >= lower.utility)) {
            // Die niedrigere der beiden Steuern steigt (bei Gleichstand die Akzise), um einen Punkt, bei
            // großem Defizit um zwei.
            const std::size_t kind = s.tax_rates[k] < s.tax_rates[a] ? k : a;
            const Fixed step = deficit >= Fixed::from_int(30) ? point * Fixed::from_int(2) : point;
            const Fixed rate_new = std::min(s.tax_rates[kind] + step, w_.rules.tax_rate_max);
            if (rate_new > s.tax_rates[kind]) {
                order_tax(state, kind, rate_new, AiDecisionKind::RaiseTax, raise);
            }
        } else if (can_lower && lower.utility >= r.threshold) {
            // Die höhere der beiden Steuern sinkt (bei Gleichstand die Kontribution) um einen Punkt.
            const std::size_t kind = s.tax_rates[a] > s.tax_rates[k] ? a : k;
            const Fixed rate_new = std::max(s.tax_rates[kind] - point, Fixed::zero());
            if (rate_new < s.tax_rates[kind]) {
                order_tax(state, kind, rate_new, AiDecisionKind::LowerTax, lower);
            }
        }
    }

    void order_tax(Index state, std::size_t kind, Fixed rate_new, AiDecisionKind decision,
                   const Option& option) {
        Command c;
        c.type = CommandType::SetTaxRate;
        c.state = state;
        c.kind = static_cast<TaxKind>(kind);
        c.value = rate_new;
        out_.push_back(c);
        record(state, decision, static_cast<Index>(kind), rate_new, option);
        // Eine Abkühlung für beide Richtungen (unter der Art RaiseTax ohne Ziel).
        cool(state, AiDecisionKind::RaiseTax, kNone, w_.ai_rules.tax_cooldown_months);
    }

    // ── Mobilmachung und Abrüstung (Abschnitt 6.3) ──

    void plan_mobilization(Index state) {
        if (!military_active(w_) || !in_range(w_.mobilizations, state)) {
            return;
        }
        AiStrategy& strategy = w_.ai_strategy[at(state)];
        const Mobilization& m = w_.mobilizations[at(state)];
        const bool armed =
            m.phase == MobilizationPhase::Mobilizing || m.phase == MobilizationPhase::Mobilized;
        const bool war = in_war(w_, state);
        const bool crisis = std::any_of(w_.plays.begin(), w_.plays.end(), [&](const DiplomaticPlay& p) {
            return p.stage >= 3 && (p.demander == state || p.target == state);
        });
        const bool helping = std::any_of(w_.revolts.begin(), w_.revolts.end(),
                                         [&](const Revolt& r) { return r.helper == state; });
        if (armed) {
            // Abrüstung nach quiet_months Monatsersten ohne Krieg, Krise und Hilfe gegen einen Aufstand.
            strategy.quiet_months = war || crisis || helping || m.crisis ? 0 : strategy.quiet_months + 1;
            if (strategy.quiet_months >= w_.ai_rules.quiet_months) {
                Option quiet;
                quiet.add(AiFactor::Quiet, Fixed::from_int(10) * Fixed::from_int(strategy.quiet_months));
                quiet.add(AiFactor::Thrift, Fixed::from_int(5) * inclination(Inclination::Thrift));
                quiet.add(AiFactor::Risk, Fixed::from_int(-5) * inclination(Inclination::Risk));
                rate(quiet);
                if (quiet.utility >= w_.ai_rules.threshold) {
                    order_mobilization(state, false, AiDecisionKind::Demobilize, kNone, quiet);
                    strategy.quiet_months = 0;
                }
            }
            return;
        }
        strategy.quiet_months = 0;
        if (war || cooling(state, AiDecisionKind::Mobilize, kNone)) {
            return;
        }
        // Gegen einen mobilen Nachbarn mit Bedrohung über threat_mobilize.
        const auto& neighbors = cache_.neighbors[at(state)];
        Index source = kNone;
        Fixed most;
        for (const Index other : neighbors) {
            if (!in_range(w_.mobilizations, other)) {
                continue;
            }
            const MobilizationPhase phase = w_.mobilizations[at(other)].phase;
            if (phase != MobilizationPhase::Mobilizing && phase != MobilizationPhase::Mobilized) {
                continue;
            }
            const Fixed threat = ai_threat(w_, cache_, state, other);
            if (threat > w_.ai_rules.threat_mobilize && relation(w_, state, other) < Fixed::zero() &&
                threat > most) {
                most = threat;
                source = other;
            }
        }
        if (source == kNone) {
            return;
        }
        Option mobilize;
        mobilize.add(AiFactor::Threat,
                     Fixed::from_int(20) * (most - w_.ai_rules.threat_mobilize) + Fixed::from_int(10),
                     source);
        mobilize.add(AiFactor::Risk, Fixed::from_int(5) * inclination(Inclination::Risk));
        mobilize.add(AiFactor::Thrift, Fixed::from_int(-5) * inclination(Inclination::Thrift));
        rate(mobilize);
        if (mobilize.utility >= w_.ai_rules.threshold) {
            order_mobilization(state, true, AiDecisionKind::Mobilize, source, mobilize);
            cool(state, AiDecisionKind::Mobilize, kNone, w_.ai_rules.tax_cooldown_months);
        }
    }

    void order_mobilization(Index state, bool mobilize, AiDecisionKind decision, Index target,
                            const Option& option) {
        Command c;
        c.type = CommandType::Mobilization;
        c.state = state;
        c.value = mobilize ? Fixed::one() : Fixed::zero();
        out_.push_back(c);
        record(state, decision, target, Fixed::zero(), option);
    }

    World& w_;
    Pcg32& rng_;
    const EventCache& cache_;
    std::vector<Command>& out_;
    Inclinations inclinations_{};
};

} // namespace

std::vector<Command> run_ai(World& world, Pcg32& rng, EventCache& cache) {
    std::vector<Command> commands;
    if (!ai_active(world) || world.date.day != 1) {
        return commands;
    }
    update_offices(world, cache);
    Planner(world, rng, cache, commands).run();
    return commands;
}

} // namespace uhrwerk
