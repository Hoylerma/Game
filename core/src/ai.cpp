#include "uhrwerk/ai.hpp"

#include "uhrwerk/customs.hpp"
#include "uhrwerk/diplomacy.hpp"
#include "uhrwerk/military.hpp"
#include "uhrwerk/politics.hpp"

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

Index ai_difficulty(const World& world) {
    const Index chosen = world.game_settings.difficulty;
    if (in_range(world.ai_difficulties, chosen)) {
        return chosen;
    }
    return in_range(world.ai_difficulties, world.ai_rules.default_difficulty)
               ? world.ai_rules.default_difficulty
               : kNone;
}

AiRules effective_ai_rules(const World& world) {
    AiRules r = world.ai_rules;
    r.open_agenda = r.open_agenda || world.game_settings.open;
    const Index difficulty = ai_difficulty(world);
    if (difficulty == kNone) {
        return r;
    }
    const DifficultyDef& d = world.ai_difficulties[at(difficulty)];
    const auto months = [&](std::int32_t value) {
        return static_cast<std::int32_t>((Fixed::from_int(value) * d.cooldown).round());
    };
    r.threshold = r.threshold * d.threshold;
    r.noise = r.noise * d.noise;
    r.cooldown_months = months(r.cooldown_months);
    r.tax_cooldown_months = months(r.tax_cooldown_months);
    r.caution = d.caution;
    r.player_rival = d.player_rival;
    return r;
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
    if (!world.ai_rules.open_agenda && !world.game_settings.open) {
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

namespace {

/// Bedrohung aus den Stärken beider Staaten (ai_threat; der Planer reicht seine einmal je Monatserstem
/// berechneten Stärken herein).
Fixed threat_from(const World& world, const EventCache& cache, Index state, Index other, Fixed own,
                  Fixed theirs) {
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

} // namespace

Fixed ai_threat(const World& world, const EventCache& cache, Index state, Index other) {
    if (state == other || !in_range(world.states, state) || !in_range(world.states, other)) {
        return Fixed::zero();
    }
    return threat_from(world, cache, state, other, ai_strength(world, state), ai_strength(world, other));
}

// ─── Gründe und Protokoll (Abschnitt 5) ─────────────────────────────────────

std::string ai_reason_text(const World& world, const AiReason& reason) {
    static constexpr std::array<std::string_view, kAiFactors> names = {
        "Defizit",      "Schuldenquote",   "Überschuss",   "Unruhe",         "Sparsamkeit", "Reform",
        "Risiko",       "Bedrohung durch", "mobil:",       "Frieden",        "Agenda",      "Rivale",
        "Beziehung zu", "Stärke gegen",    "Nachbar",      "verbündet mit",  "Expansion",   "Legitimität",
        "Vermittlung",  "Bündnisse",       "Kriegspunkte", "Kriegsmüdigkeit"};
    std::string text(names[static_cast<std::size_t>(reason.factor)]);
    if (in_range(world.states, reason.other)) {
        text += " " + world.states[at(reason.other)].short_name;
    }
    const std::int64_t value = reason.value.round();
    text += value >= 0 ? " +" : " −";
    text += std::to_string(value >= 0 ? value : -value);
    return text;
}

namespace {

std::string other_name(const World& world, Index state) {
    return in_range(world.states, state) ? world.states[at(state)].short_name : std::string("?");
}

} // namespace

std::string ai_decision_text(const World& world, const AiDecision& decision) {
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
    case AiDecisionKind::SendEnvoy:
        return "entsendet einen Gesandten nach " + other_name(world, decision.target);
    case AiDecisionKind::OfferAlliance:
        return "bietet " + other_name(world, decision.target) + " ein Bündnis an";
    case AiDecisionKind::OfferGuarantee:
        return "garantiert " + other_name(world, decision.target);
    case AiDecisionKind::Demand: {
        static constexpr std::array<std::string_view, 3> demands = {
            "die Führung im Bund", "den Beitritt zur Zollunion", "das Lösen seiner Bündnisse"};
        const auto kind = static_cast<std::size_t>(decision.value.trunc());
        return "fordert von " + other_name(world, decision.target) + " " +
               std::string(kind < demands.size() ? demands[kind] : "etwas");
    }
    case AiDecisionKind::Manoeuvre: {
        static constexpr std::array<std::string_view, 3> moves = {
            "gibt in der Krise nach", "bietet in der Krise Vermittlung an", "verschärft die Krise"};
        const auto kind = static_cast<std::size_t>(decision.value.trunc());
        return std::string(kind < moves.size() ? moves[kind] : "handelt in der Krise");
    }
    case AiDecisionKind::Motion: {
        static constexpr std::array<std::string_view, 4> motions = {
            "die Bundesreform", "die Zolleinigung", "eine Bundesintervention", "das Präsidium"};
        const auto kind = static_cast<std::size_t>(decision.value.trunc());
        return "beantragt " + std::string(kind < motions.size() ? motions[kind] : "etwas");
    }
    case AiDecisionKind::DeclareWar:
        return "erklärt " + other_name(world, decision.target) + " den Krieg";
    case AiDecisionKind::AddWarGoal:
        return "stellt ein weiteres Kriegsziel gegen " + other_name(world, decision.target);
    case AiDecisionKind::OfferPeace: {
        static constexpr std::array<std::string_view, 3> terms = {
            "einen weißen Frieden", "Frieden zu den eigenen Zielen", "die Kapitulation"};
        const auto kind = static_cast<std::size_t>(decision.value.trunc());
        return "bietet " + other_name(world, decision.target) + " " +
               std::string(kind < terms.size() ? terms[kind] : "Frieden") + " an";
    }
    case AiDecisionKind::JoinUnion:
        return "beantragt den Beitritt zu " + (in_range(world.customs_unions, decision.target)
                                                   ? world.customs_unions[at(decision.target)].name
                                                   : std::string("einer Zollunion"));
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
        : w_(world), rng_(rng), cache_(cache), out_(out), r_(effective_ai_rules(world)) {}

    Fixed strength(Index state) const {
        return in_range(strength_, state) ? strength_[at(state)] : Fixed::zero();
    }

    Fixed threat(Index state, Index other) const {
        if (state == other || !in_range(strength_, state) || !in_range(strength_, other)) {
            return Fixed::zero();
        }
        return threat_from(w_, cache_, state, other, strength(state), strength(other));
    }

    void run() {
        const std::size_t states = w_.states.size();
        expire_cooldowns();
        // Stärken ändern sich während der Planung nicht (Befehle wirken am nächsten Tag): einmal berechnen.
        strength_.resize(states);
        for (std::size_t s = 0; s < states; ++s) {
            strength_[s] = ai_strength(w_, static_cast<Index>(s));
        }
        for (std::size_t s = 0; s < states; ++s) {
            const auto state = static_cast<Index>(s);
            if (state == w_.event_player || !in_range(cache_.alive, state) || cache_.alive[s] == 0) {
                continue;
            }
            inclinations_ = state_inclinations(w_, state);
            update_strategy(state);
            plan_budget(state);
            plan_mobilization(state);
            if (diplomacy_active(w_)) {
                plan_envoy(state);
                plan_alliance(state);
                plan_guarantee(state);
                plan_demand(state);
                plan_manoeuvre(state);
                plan_motion(state);
            }
            if (!w_.customs_unions.empty()) {
                plan_customs(state);
            }
            if (military_active(w_)) {
                plan_war(state);
            }
        }
        if (!w_.parliaments.empty()) {
            plan_campaigns();
        }
    }

private:
    Fixed inclination(Inclination i) const { return inclinations_[static_cast<std::size_t>(i)]; }

    /// Nutzen mit Rauschen aus dem Strom Ki (nur für Optionen, die ihre Bedingung erfüllen).
    void rate(Option& option) {
        const Fixed noise = r_.noise;
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
        const auto limit = static_cast<std::size_t>(std::max(r_.log_size, 1));
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
                const Fixed threat = this->threat(state, other);
                if (threat > most) {
                    most = threat;
                    rival = other;
                }
            }
            // Stufe Meister (seit M13a1): der Spielerstaat ist Rivale, wenn er mindestens drei Viertel so
            // bedrohlich ist wie die größte Bedrohung (koordinierte Bündnisse gegen den Spieler).
            const Index player = w_.event_player;
            if (r_.player_rival && rival != kNone && rival != player && player != state && alive(player) &&
                this->threat(state, player) * Fixed::from_int(4) >= most * Fixed::from_int(3)) {
                rival = player;
            }
        }
        strategy.rival = rival;
    }

    // ── Haushalt und Steuern (Abschnitt 6.1) ──

    void plan_budget(Index state) {
        const State& s = w_.states[at(state)];
        const AiRules& r = r_;
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
        cool(state, AiDecisionKind::RaiseTax, kNone, r_.tax_cooldown_months);
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
            if (strategy.quiet_months >= r_.quiet_months) {
                Option quiet;
                quiet.add(AiFactor::Quiet, Fixed::from_int(10) * Fixed::from_int(strategy.quiet_months));
                quiet.add(AiFactor::Thrift, Fixed::from_int(5) * inclination(Inclination::Thrift));
                quiet.add(AiFactor::Risk, Fixed::from_int(-5) * inclination(Inclination::Risk));
                rate(quiet);
                if (quiet.utility >= r_.threshold) {
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
            const Fixed threat = this->threat(state, other);
            if (threat > r_.threat_mobilize && relation(w_, state, other) < Fixed::zero() && threat > most) {
                most = threat;
                source = other;
            }
        }
        if (source == kNone) {
            return;
        }
        Option mobilize;
        mobilize.add(AiFactor::Threat,
                     Fixed::from_int(20) * (most - r_.threat_mobilize) + Fixed::from_int(10), source);
        mobilize.add(AiFactor::Risk, Fixed::from_int(5) * inclination(Inclination::Risk));
        mobilize.add(AiFactor::Thrift, Fixed::from_int(-5) * inclination(Inclination::Thrift));
        rate(mobilize);
        if (mobilize.utility >= r_.threshold) {
            order_mobilization(state, true, AiDecisionKind::Mobilize, source, mobilize);
            cool(state, AiDecisionKind::Mobilize, kNone, r_.tax_cooldown_months);
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

    // ── Diplomatie (Abschnitt 6.2) ──

    bool alive(Index other) const { return in_range(cache_.alive, other) && cache_.alive[at(other)] != 0; }

    bool neighbor(Index state, Index other) const {
        const auto& list = cache_.neighbors[at(state)];
        return std::find(list.begin(), list.end(), other) != list.end();
    }

    /// Aktive Ziele der Agenda einer Art (mit Gegenstand target, kNone: jeder).
    std::vector<const AgendaGoal*> goals(Index state, AgendaKind kind, Index target = kNone) const {
        std::vector<const AgendaGoal*> result;
        if (!in_range(w_.agendas, state)) {
            return result;
        }
        for (const AgendaGoal& g : w_.agendas[at(state)].goals) {
            if (g.kind == kind && (target == kNone || g.target == target) &&
                agenda_goal_active(w_, cache_, state, g)) {
                result.push_back(&g);
            }
        }
        return result;
    }

    Fixed goal_weight(Index state, AgendaKind kind, Index target) const {
        Fixed best;
        for (const AgendaGoal* g : goals(state, kind, target)) {
            best = std::max(best, g->weight);
        }
        return best;
    }

    Fixed ratio(Fixed own, Fixed other) const {
        if (other <= Fixed::zero()) {
            return Fixed::from_int(10);
        }
        return std::min(own / other, Fixed::from_int(10));
    }

    std::int64_t alliance_count(Index state) const {
        std::int64_t n = 0;
        for (const Treaty& t : w_.treaties) {
            n += t.kind == TreatyKind::Alliance && (t.from == state || t.to == state) ? 1 : 0;
        }
        return n;
    }

    /// Wichtigkeit eines anderen Staates für Gesandte: Agenda, Rivale, Verbündete, Nachbarschaft, Stärke.
    Option importance(Index state, Index other, Fixed strongest) const {
        Option o;
        Fixed agenda;
        for (const AgendaGoal& g :
             in_range(w_.agendas, state) ? w_.agendas[at(state)].goals : std::vector<AgendaGoal>{}) {
            if (g.target == other && g.kind != AgendaKind::CustomsUnion && g.kind != AgendaKind::Territory &&
                agenda_goal_active(w_, cache_, state, g)) {
                agenda = std::max(agenda, Fixed::from_int(30));
            }
        }
        o.add(AiFactor::Agenda, agenda, other);
        if (w_.ai_strategy[at(state)].rival == other) {
            o.add(AiFactor::Rival, Fixed::from_int(20), other);
        }
        if (allied(w_, state, other) || guarantees(w_, state, other) || guarantees(w_, other, state)) {
            o.add(AiFactor::Ally, Fixed::from_int(15), other);
        }
        if (neighbor(state, other)) {
            o.add(AiFactor::Neighbor, Fixed::from_int(10), other);
        }
        if (strongest > Fixed::zero()) {
            o.add(AiFactor::Strength, Fixed::from_int(10) * strength(other) / strongest, other);
        }
        return o;
    }

    void plan_envoy(Index state) {
        if (w_.relations.empty() || envoy_count(w_, state) >= envoy_capacity(w_, state) ||
            cooling(state, AiDecisionKind::SendEnvoy, kNone)) {
            return;
        }
        Fixed strongest;
        for (std::size_t o = 0; o < w_.states.size(); ++o) {
            strongest = std::max(strongest, strength(static_cast<Index>(o)));
        }
        Index best = kNone;
        Option best_option;
        for (std::size_t o = 0; o < w_.states.size(); ++o) {
            const auto other = static_cast<Index>(o);
            // Wie evaluate_envoy: Kapazität ist oben geprüft und ändert sich in der Schleife nicht.
            if (other == state || !alive(other) || has_envoy(w_, state, other)) {
                continue;
            }
            Option option = importance(state, other, strongest);
            if (best == kNone || option.sum() > best_option.sum()) {
                best = other;
                best_option = std::move(option);
            }
        }
        if (best == kNone) {
            return;
        }
        best_option.add(AiFactor::Thrift, Fixed::from_int(-5) * inclination(Inclination::Thrift));
        rate(best_option);
        if (best_option.utility >= r_.threshold) {
            Command c;
            c.type = CommandType::Envoy;
            c.state = state;
            c.target = best;
            c.value = Fixed::one();
            out_.push_back(c);
            record(state, AiDecisionKind::SendEnvoy, best, Fixed::zero(), best_option);
            cool(state, AiDecisionKind::SendEnvoy, kNone, 1); // höchstens ein Gesandter je Monat
        }
    }

    /// Gemeinsamer Rivale von state und other (beide Strategien nennen denselben Staat, der keiner von
    /// beiden ist).
    Index common_rival(Index state, Index other) const {
        const Index a = w_.ai_strategy[at(state)].rival;
        const Index b = in_range(w_.ai_strategy, other) ? w_.ai_strategy[at(other)].rival : kNone;
        return a != kNone && a == b && a != state && a != other ? a : kNone;
    }

    /// Mitten in einem Krieg schließt die KI keine Verträge (ein Bündnis machte den Partner zur eigenen
    /// Seite: Durchmarsch und Versorgung, militaer.md 4.1).
    bool at_war_any(Index state) const { return military_active(w_) && in_war(w_, state); }

    void plan_alliance(Index state) {
        if (alliance_count(state) >= r_.max_alliances ||
            cooling(state, AiDecisionKind::OfferAlliance, kNone) || at_war_any(state)) {
            return;
        }
        const Fixed own = strength(state);
        Index best = kNone;
        Option best_option;
        for (std::size_t o = 0; o < w_.states.size(); ++o) {
            const auto other = static_cast<Index>(o);
            if (other == state || !alive(other) || other == w_.event_player || allied(w_, state, other) ||
                at_war_any(other)) {
                continue;
            }
            // Der gemeinsame Rivale zählt nur, wenn er für den Staat ein Gegner ist (Beziehung unter
            // rival_below; seit b2: sonst verbündete sich fast jeder Bundesstaat gegen die nächste
            // Großmacht).
            Index rival = common_rival(state, other);
            if (rival != kNone && relation(w_, state, rival) >= w_.diplomacy_rules.rival_below) {
                rival = kNone;
            }
            const Fixed agenda = goal_weight(state, AgendaKind::Alliance, other);
            if (rival == kNone && agenda <= Fixed::zero()) {
                continue; // Bündnisse nur gegen einen gemeinsamen Gegner oder aus der Agenda
            }
            if (evaluate_treaty(w_, state, other, TreatyAction::Alliance) != DiplomacyDecision::Accepted) {
                continue;
            }
            Option option;
            option.add(AiFactor::Agenda, agenda / Fixed::from_int(2), other);
            if (rival != kNone) {
                option.add(AiFactor::Rival, Fixed::from_int(20), rival);
            }
            option.add(AiFactor::Strength,
                       std::min(Fixed::from_int(10) * ratio(strength(other), own), Fixed::from_int(20)),
                       other);
            option.add(AiFactor::Relation, relation(w_, state, other) / Fixed::from_int(20), other);
            option.add(AiFactor::Alliances, Fixed::from_int(-5) * Fixed::from_int(alliance_count(state)));
            if (best == kNone || option.sum() > best_option.sum()) {
                best = other;
                best_option = std::move(option);
            }
        }
        if (best == kNone) {
            return;
        }
        rate(best_option);
        if (best_option.utility >= r_.threshold) {
            order_treaty(state, best, TreatyAction::Alliance, AiDecisionKind::OfferAlliance, best_option);
        }
    }

    void plan_guarantee(Index state) {
        if (cooling(state, AiDecisionKind::OfferGuarantee, kNone) || at_war_any(state)) {
            return;
        }
        // Eine Großmacht (dreifache Stärke) garantiert einen kleinen Nachbarn im eigenen Lager.
        const Fixed own = strength(state);
        Index best = kNone;
        Option best_option;
        for (const Index other : cache_.neighbors[at(state)]) {
            if (!alive(other) || other == w_.event_player || at_war_any(other) ||
                ratio(own, strength(other)) < Fixed::from_int(3) ||
                evaluate_treaty(w_, state, other, TreatyAction::Guarantee) != DiplomacyDecision::Accepted) {
                continue;
            }
            Option option;
            option.add(AiFactor::Neighbor, Fixed::from_int(5), other);
            option.add(AiFactor::Relation, relation(w_, state, other) / Fixed::from_int(10), other);
            option.add(AiFactor::Legitimacy, Fixed::from_int(5) * inclination(Inclination::Legitimacy));
            option.add(AiFactor::Agenda,
                       goal_weight(state, AgendaKind::StatusQuo, kNone) / Fixed::from_int(4));
            if (best == kNone || option.sum() > best_option.sum()) {
                best = other;
                best_option = std::move(option);
            }
        }
        if (best == kNone) {
            return;
        }
        rate(best_option);
        if (best_option.utility >= r_.threshold) {
            order_treaty(state, best, TreatyAction::Guarantee, AiDecisionKind::OfferGuarantee, best_option);
        }
    }

    void order_treaty(Index state, Index target, TreatyAction action, AiDecisionKind decision,
                      const Option& option) {
        Command c;
        c.type = CommandType::Treaty;
        c.state = state;
        c.target = target;
        c.value = Fixed::from_int(static_cast<std::int64_t>(action));
        out_.push_back(c);
        record(state, decision, target, Fixed::zero(), option);
        cool(state, decision, kNone, r_.tax_cooldown_months);
    }

    /// Bündnisstärke: eigene Stärke und die der Verbündeten.
    Fixed alliance_strength(Index state) const {
        Fixed total = strength(state);
        for (const Treaty& t : w_.treaties) {
            if (t.kind == TreatyKind::Alliance && (t.from == state || t.to == state)) {
                total += strength(t.from == state ? t.to : t.from);
            }
        }
        return total;
    }

    void plan_demand(Index state) {
        // Höchstens eine Forderung je Abkühlung (seit b1: sonst wechselte der Staat nur das Ziel).
        if (play_of(w_, state) != kNone || (military_active(w_) && in_war(w_, state)) ||
            cooling(state, AiDecisionKind::Demand, kNone)) {
            return;
        }
        struct Candidate {
            Index target;
            DemandKind kind;
            Fixed weight;
        };
        std::vector<Candidate> candidates;
        // Führung im Bund: gegen den Rivalen bzw. die Präsidialmacht.
        for (const AgendaGoal* g : goals(state, AgendaKind::LeadFederation)) {
            const Index rival = w_.ai_strategy[at(state)].rival;
            const Index target = rival != kNone ? rival : w_.diet_presidency;
            if (target != kNone && target != state) {
                candidates.push_back({target, DemandKind::Leadership, g->weight});
            }
        }
        // Beitritt zur eigenen Zollunion: Nachbarn außerhalb, wenn die Agenda die Union nennt.
        const Index union_own = w_.states[at(state)].customs_union;
        if (union_own != kNone && in_range(w_.customs_unions, union_own) &&
            w_.customs_unions[at(union_own)].leader == state) {
            for (const AgendaGoal* g : goals(state, AgendaKind::CustomsUnion, union_own)) {
                for (const Index other : cache_.neighbors[at(state)]) {
                    // Seit b2 nur Mitglieder einer anderen Union (Union gegen Union, etwa Zollverein gegen
                    // Steuerverein): Staaten ohne Union blieben historisch draußen.
                    const Index other_union = w_.states[at(other)].customs_union;
                    if (alive(other) && federal_state(w_, other) && other_union != kNone &&
                        other_union != union_own) {
                        candidates.push_back({other, DemandKind::CustomsUnion, g->weight});
                    }
                }
            }
        }
        // Schwächen: Führung im Bund gegen den Gegner.
        for (const AgendaGoal* g : goals(state, AgendaKind::Weaken)) {
            candidates.push_back({g->target, DemandKind::Leadership, g->weight});
        }
        const Fixed own = alliance_strength(state);
        Index best = kNone;
        DemandKind best_kind = DemandKind::Leadership;
        Option best_option;
        for (const Candidate& c : candidates) {
            if (!alive(c.target) || cooling(state, AiDecisionKind::Demand, c.target) ||
                evaluate_demand(w_, state, c.target, c.kind) != DiplomacyDecision::Accepted) {
                continue;
            }
            const Fixed r = ratio(own, alliance_strength(c.target));
            const Fixed needed =
                Fixed::one() - inclination(Inclination::Risk) * Fixed::from_ratio(15, 100) + r_.caution;
            if (r < needed) {
                continue;
            }
            Option option;
            option.add(AiFactor::Agenda, c.weight / Fixed::from_int(2), c.target);
            option.add(AiFactor::Strength, Fixed::from_int(20) * (r - needed), c.target);
            option.add(AiFactor::Expansion, Fixed::from_int(5) * inclination(Inclination::Expansion));
            option.add(AiFactor::Risk, Fixed::from_int(5) * inclination(Inclination::Risk));
            if (best == kNone || option.sum() > best_option.sum()) {
                best = c.target;
                best_kind = c.kind;
                best_option = std::move(option);
            }
        }
        if (best == kNone) {
            return;
        }
        rate(best_option);
        if (best_option.utility >= r_.threshold) {
            Command c;
            c.type = CommandType::Demand;
            c.state = state;
            c.target = best;
            c.value = Fixed::from_int(static_cast<std::int64_t>(best_kind));
            out_.push_back(c);
            record(state, AiDecisionKind::Demand, best, c.value, best_option);
            chronicle_reasons(state); // seit b3: Gründe auch in der Chronik (ki.md 9)
            cool(state, AiDecisionKind::Demand, best, r_.cooldown_months);
            cool(state, AiDecisionKind::Demand, kNone, r_.cooldown_months);
        }
    }

    void plan_manoeuvre(Index state) {
        const Index index = play_of(w_, state);
        if (index == kNone || cooling(state, AiDecisionKind::Manoeuvre, kNone)) {
            return;
        }
        const DiplomaticPlay& play = w_.plays[at(index)];
        const bool demander = play.demander == state;
        const Index other = demander ? play.target : play.demander;
        const Fixed r = ratio(side_strength(w_, play, demander), side_strength(w_, play, !demander));
        const Fixed risk = inclination(Inclination::Risk);
        const Fixed yield_below = w_.diplomacy_rules.play_yield - risk * Fixed::from_ratio(1, 10);
        Option option;
        Manoeuvre move = Manoeuvre::Mediate;
        if (r < yield_below) {
            move = Manoeuvre::Yield;
            option.add(AiFactor::Strength, Fixed::from_int(10) + Fixed::from_int(20) * (yield_below - r),
                       other);
            option.add(AiFactor::Risk, Fixed::from_int(-5) * risk);
        } else if (r >= Fixed::one() - risk * Fixed::from_ratio(15, 100)) {
            move = Manoeuvre::Escalate;
            option.add(AiFactor::Strength, Fixed::from_int(10) + Fixed::from_int(10) * (r - Fixed::one()),
                       other);
            option.add(AiFactor::Risk, Fixed::from_int(5) * risk);
            option.add(AiFactor::Expansion, Fixed::from_int(5) * inclination(Inclination::Expansion));
        } else if (inclination(Inclination::Mediation) > Fixed::zero()) {
            option.add(AiFactor::Mediation, Fixed::from_int(10) * inclination(Inclination::Mediation));
        } else {
            return; // die Regel der Stufe entscheidet (diplomatie.md 4.5)
        }
        if (evaluate_manoeuvre(w_, state, move) == DiplomacyDecision::Invalid) {
            return;
        }
        rate(option);
        if (option.utility >= r_.threshold) {
            Command c;
            c.type = CommandType::Manoeuvre;
            c.state = state;
            c.value = Fixed::from_int(static_cast<std::int64_t>(move));
            out_.push_back(c);
            record(state, AiDecisionKind::Manoeuvre, other, c.value, option);
            cool(state, AiDecisionKind::Manoeuvre, kNone, 1);
        }
    }

    void plan_motion(Index state) {
        if (!has_diet(w_) || w_.diet_suspended != 0) {
            return;
        }
        struct Candidate {
            MotionKind kind;
            Index target;
            Fixed weight;
        };
        std::vector<Candidate> candidates;
        for (const AgendaGoal* g : goals(state, AgendaKind::LeadFederation)) {
            candidates.push_back({w_.diet_presidency == state ? MotionKind::Reform : MotionKind::Presidency,
                                  kNone, g->weight});
        }
        // Die Zolleinigung des Bundes beantragt die KI nicht (seit b2): Der Beschluss führte alle
        // Bundesstaaten samt Österreich in die Union der Leitmacht, die das historisch gerade verhinderte.
        // Ziele der Art zollunion wirken über Forderungen und Beitritte.
        for (const Candidate& c : candidates) {
            const auto key = static_cast<Index>(c.kind);
            if (cooling(state, AiDecisionKind::Motion, key) ||
                evaluate_motion(w_, state, c.kind, c.target) != DiplomacyDecision::Accepted ||
                !tally_motion(w_, Motion{c.kind, state, c.target, w_.date}).accepted) {
                continue;
            }
            Option option;
            option.add(AiFactor::Agenda, c.weight / Fixed::from_int(2));
            option.add(AiFactor::Legitimacy, Fixed::from_int(3) * inclination(Inclination::Legitimacy));
            rate(option);
            if (option.utility >= r_.threshold) {
                Command cmd;
                cmd.type = CommandType::Motion;
                cmd.state = state;
                cmd.target = c.target;
                cmd.value = Fixed::from_int(static_cast<std::int64_t>(c.kind));
                out_.push_back(cmd);
                record(state, AiDecisionKind::Motion, c.target, cmd.value, option);
                cool(state, AiDecisionKind::Motion, key, r_.cooldown_months);
                return; // höchstens ein Antrag je Monat
            }
        }
    }

    void plan_customs(Index state) {
        const Index current = w_.states[at(state)].customs_union;
        Index best = kNone;
        Option best_option;
        for (std::size_t u = 0; u < w_.customs_unions.size(); ++u) {
            const auto union_index = static_cast<Index>(u);
            const Fixed weight = goal_weight(state, AgendaKind::CustomsUnion, union_index);
            if (union_index == current || weight <= Fixed::zero() ||
                cooling(state, AiDecisionKind::JoinUnion, union_index) ||
                evaluate_customs_change(w_, state, union_index) != CustomsDecision::Accepted) {
                continue;
            }
            const Index leader = w_.customs_unions[u].leader;
            Option option;
            option.add(AiFactor::Agenda, weight / Fixed::from_int(2));
            if (in_range(w_.states, leader) && leader != state) {
                option.add(AiFactor::Relation, relation(w_, state, leader) / Fixed::from_int(10), leader);
                if (w_.ai_strategy[at(state)].rival == leader) {
                    option.add(AiFactor::Rival, Fixed::from_int(-30), leader);
                }
            }
            if (best == kNone || option.sum() > best_option.sum()) {
                best = union_index;
                best_option = std::move(option);
            }
        }
        if (best == kNone) {
            return;
        }
        rate(best_option);
        if (best_option.utility >= r_.threshold) {
            Command c;
            c.type = CommandType::CustomsUnion;
            c.state = state;
            c.target = best;
            out_.push_back(c);
            record(state, AiDecisionKind::JoinUnion, best, Fixed::zero(), best_option);
            cool(state, AiDecisionKind::JoinUnion, best, r_.cooldown_months);
        }
    }

    // ── Krieg (Abschnitt 6.3) ──

    /// Staat, dem die meisten Orte einer Verwaltungseinheit gehören (auch über untere Ebenen), sonst kNone.
    Index unit_owner(Index unit) const {
        std::vector<std::int64_t> counts(w_.states.size(), 0);
        for (const Location& l : w_.locations) {
            Index u = l.admin;
            while (u != kNone && u != unit && in_range(w_.admin_units, u)) {
                u = w_.admin_units[at(u)].parent;
            }
            if (u == unit && in_range(w_.states, l.owner)) {
                ++counts[at(l.owner)];
            }
        }
        const auto best = std::max_element(counts.begin(), counts.end());
        return best != counts.end() && *best > 0 ? static_cast<Index>(best - counts.begin()) : kNone;
    }

    bool has_trait(Index state, std::string_view id) const {
        const Index trait = w_.find_trait(id);
        if (trait == kNone) {
            return false;
        }
        for (std::size_t r = 0; r < kPersonRoles; ++r) {
            const Index person = w_.office(state, static_cast<PersonRole>(r));
            if (in_range(w_.persons, person)) {
                const auto& traits = w_.persons[at(person)].traits;
                if (std::find(traits.begin(), traits.end(), trait) != traits.end()) {
                    return true;
                }
            }
        }
        return false;
    }

    Fixed war_side_strength(const std::vector<Index>& side) const {
        Fixed total;
        for (const Index s : side) {
            total += strength(s);
        }
        return total;
    }

    void plan_war(Index state) {
        const War* own = nullptr;
        for (const War& war : w_.wars) {
            if (!war.attackers.empty() && !war.defenders.empty() &&
                (war.attackers.front() == state || war.defenders.front() == state)) {
                own = &war;
                break;
            }
        }
        if (own != nullptr) {
            plan_peace(state, *own);
            return;
        }
        if (in_war(w_, state)) {
            return; // als Verbündeter im Krieg: die Hauptpartei entscheidet
        }
        plan_declaration(state);
    }

    void plan_declaration(Index state) {
        const State& s = w_.states[at(state)];
        const Fixed risk = inclination(Inclination::Risk);
        const Fixed needed = r_.war_ratio - risk * Fixed::from_ratio(15, 100) + r_.caution;
        const Fixed debt_ratio = s.debt / std::max(s.yearly_income, Fixed::one());
        const Fixed unrest = in_range(w_.unrest, state) ? w_.unrest[at(state)] : Fixed::zero();
        const bool tactician = has_trait(state, "taktiker");
        Index best = kNone;
        std::int64_t best_goal = 0;
        Option best_option;
        if (!in_range(w_.agendas, state)) {
            return;
        }
        for (const AgendaGoal& g : w_.agendas[at(state)].goals) {
            if ((g.kind != AgendaKind::Territory && g.kind != AgendaKind::Annexation &&
                 g.kind != AgendaKind::Weaken) ||
                !agenda_goal_active(w_, cache_, state, g)) {
                continue;
            }
            const Index enemy = g.kind == AgendaKind::Territory ? unit_owner(g.target) : g.target;
            if (enemy == kNone || enemy == state || !alive(enemy) ||
                cooling(state, AiDecisionKind::DeclareWar, enemy) || !can_declare_war(w_, state, enemy)) {
                continue;
            }
            std::vector<Index> attackers;
            std::vector<Index> defenders;
            war_sides(w_, state, enemy, attackers, defenders);
            if (tactician && defenders.size() > attackers.size()) {
                continue; // Taktiker warten auf einen günstigen Bündnisfall (Gegner mit höchstens so vielen
                          // Staaten)
            }
            const Fixed r = ratio(war_side_strength(attackers), war_side_strength(defenders));
            if (r < needed) {
                continue;
            }
            std::int64_t goal = 0;
            switch (g.kind) {
            case AgendaKind::Territory:
                goal = encode_war_goal(WarGoalKind::Territory, g.target);
                break;
            case AgendaKind::Annexation:
                goal = encode_war_goal(WarGoalKind::Annexation, enemy);
                break;
            default:
                goal = federal_state(w_, state) && federal_state(w_, enemy)
                           ? encode_war_goal(WarGoalKind::Leadership, 0)
                           : encode_war_goal(WarGoalKind::Indemnity, 6);
                break;
            }
            Option option;
            option.add(AiFactor::Agenda, g.weight, enemy);
            option.add(AiFactor::Strength, std::min(Fixed::from_int(20) * (r - needed), Fixed::from_int(40)),
                       enemy);
            option.add(AiFactor::Expansion, Fixed::from_int(5) * inclination(Inclination::Expansion));
            option.add(AiFactor::Risk, Fixed::from_int(5) * risk);
            option.add(AiFactor::DebtRatio,
                       -Fixed::from_int(10) * std::max(debt_ratio - r_.debt_limit, Fixed::zero()));
            option.add(AiFactor::Unrest,
                       -std::max(unrest - Fixed::from_int(40), Fixed::zero()) / Fixed::from_int(2));
            if (federal_state(w_, state) && federal_state(w_, enemy)) {
                option.add(AiFactor::Legitimacy, Fixed::from_int(-10) * inclination(Inclination::Legitimacy));
            }
            if (best == kNone || option.sum() > best_option.sum()) {
                best = enemy;
                best_goal = goal;
                best_option = std::move(option);
            }
        }
        if (best == kNone) {
            return;
        }
        rate(best_option);
        // Krieg verlangt die doppelte Schwelle.
        if (best_option.utility >= r_.threshold * Fixed::from_int(2)) {
            Command c;
            c.type = CommandType::DeclareWar;
            c.state = state;
            c.target = best;
            c.value = Fixed::from_int(best_goal);
            out_.push_back(c);
            record(state, AiDecisionKind::DeclareWar, best, c.value, best_option);
            chronicle_reasons(state);
            cool(state, AiDecisionKind::DeclareWar, best, r_.cooldown_months);
        }
    }

    /// Bedeutende Entscheidung: die Gründe des letzten Protokolleintrags (record) in der Chronik
    /// („Preußen – Gründe: Agenda +40, …“).
    void chronicle_reasons(Index state) {
        const AiDecision& d = w_.ai_log.back();
        std::string text = w_.states[at(state)].short_name + " – Gründe: ";
        for (std::size_t i = 0; i < d.reason_count; ++i) {
            text += (i > 0 ? ", " : "") + ai_reason_text(w_, d.reasons[i]);
        }
        ChronicleEntry entry;
        entry.date = w_.date;
        entry.state = state;
        entry.text = std::move(text);
        w_.chronicle.push_back(std::move(entry));
    }

    void plan_peace(Index state, const War& war) {
        if (cooling(state, AiDecisionKind::OfferPeace, kNone)) {
            return;
        }
        const bool attacker = war.attackers.front() == state;
        const Index enemy = attacker ? war.defenders.front() : war.attackers.front();
        const Fixed score = war_score(w_, war, attacker);
        const Fixed cost = war_goals_cost(w_, war, attacker);
        const Fixed weariness = war_weariness(w_, war, attacker);
        const Fixed enemy_score = war_score(w_, war, !attacker);
        const Fixed enemy_cost = war_goals_cost(w_, war, !attacker);
        // Weiteres Kriegsziel, wenn die Punkte die Kosten der eigenen Ziele um die Hälfte übersteigen.
        if (cost > Fixed::zero() && score >= cost * Fixed::from_ratio(3, 2) &&
            plan_more_goals(state, war, attacker, enemy)) {
            return;
        }
        Option option;
        PeaceTerms terms = PeaceTerms::White;
        if (cost > Fixed::zero() && score >= cost) {
            terms = PeaceTerms::OwnGoals;
            option.add(AiFactor::WarScore, Fixed::from_int(10) + score - cost, enemy);
        } else if (enemy_cost > Fixed::zero() && enemy_score >= enemy_cost &&
                   weariness >= Fixed::from_int(80) - Fixed::from_int(10) * inclination(Inclination::Risk)) {
            terms = PeaceTerms::Capitulation;
            option.add(AiFactor::Weariness, weariness - Fixed::from_int(60), enemy);
        } else if (weariness >= Fixed::from_int(50) - Fixed::from_int(10) * inclination(Inclination::Risk) &&
                   score < Fixed::from_int(25) && enemy_score <= score + Fixed::from_int(10) &&
                   war_side_strength(attacker ? war.attackers : war.defenders) <
                       war_side_strength(attacker ? war.defenders : war.attackers) * r_.war_ratio) {
            // Weißer Frieden nur bei ausgeglichenem Krieg: Wer zurückliegt, kämpft weiter oder unterwirft
            // sich; wer klar überlegen ist (Stärke ab kriegs_staerke), setzt auf den Sieg.
            terms = PeaceTerms::White;
            option.add(AiFactor::Weariness, weariness - Fixed::from_int(40), enemy);
            option.add(AiFactor::Mediation, Fixed::from_int(5) * inclination(Inclination::Mediation));
        } else {
            return; // die Regel des Monatsersten entscheidet (militaer.md 4.5)
        }
        const std::optional<bool> accepted = peace_acceptance(w_, state, war.id, terms);
        if (!accepted || !*accepted) {
            return;
        }
        rate(option);
        if (option.utility >= r_.threshold) {
            Command c;
            c.type = CommandType::PeaceOffer;
            c.state = state;
            c.target = static_cast<Index>(war.id);
            c.value = Fixed::from_int(static_cast<std::int64_t>(terms));
            out_.push_back(c);
            record(state, AiDecisionKind::OfferPeace, enemy, c.value, option);
            cool(state, AiDecisionKind::OfferPeace, kNone, 1);
        }
    }

    /// Ein weiteres Ziel der Agenda gegen die Gegenseite (Gebiet mit deren Orten, Annexion eines ihrer
    /// Staaten); true, wenn ein Befehl entstand.
    bool plan_more_goals(Index state, const War& war, bool attacker, Index enemy) {
        if (!in_range(w_.agendas, state) || cooling(state, AiDecisionKind::AddWarGoal, kNone)) {
            return false;
        }
        for (const AgendaGoal& g : w_.agendas[at(state)].goals) {
            if ((g.kind != AgendaKind::Territory && g.kind != AgendaKind::Annexation) ||
                !agenda_goal_active(w_, cache_, state, g)) {
                continue;
            }
            const WarGoalKind kind =
                g.kind == AgendaKind::Territory ? WarGoalKind::Territory : WarGoalKind::Annexation;
            const WarGoal goal{kind, attacker, g.target};
            if (!valid_war_goal(w_, war, goal)) {
                continue;
            }
            Option option;
            option.add(AiFactor::Agenda, g.weight, enemy);
            option.add(AiFactor::WarScore, war_score(w_, war, attacker) - war_goals_cost(w_, war, attacker),
                       enemy);
            option.add(AiFactor::Expansion, Fixed::from_int(5) * inclination(Inclination::Expansion));
            rate(option);
            if (option.utility < r_.threshold) {
                continue;
            }
            Command c;
            c.type = CommandType::WarGoal;
            c.state = state;
            c.target = static_cast<Index>(war.id);
            c.value = Fixed::from_int(encode_war_goal(kind, g.target));
            out_.push_back(c);
            record(state, AiDecisionKind::AddWarGoal, enemy, c.value, option);
            cool(state, AiDecisionKind::AddWarGoal, kNone, 1);
            return true;
        }
        return false;
    }

    // ── Wahlkampf der Strömungen (Abschnitt 6.4) ──

    /// Je Strömung außer der des Spielers in den campaign_months Monaten vor einer Wahl: Versammlungen in
    /// den Wahlkreisen mit der knappsten Lage, in denen sie erste oder zweite ist, bis die Mittel bis auf die
    /// Rücklage für die verbleibenden Monate verteilt sind. Lage mit Rauschen ±8 Punkte (wie der
    /// Stimmungsbericht des Spielers).
    void plan_campaigns() {
        const std::size_t currents = w_.currents.size();
        if (currents == 0 || w_.party_funds.size() != currents) {
            return;
        }
        const auto rally = static_cast<std::size_t>(CampaignAction::Rally);
        const Fixed cost = w_.election_rules.campaign[rally].cost;
        if (cost <= Fixed::zero()) {
            return;
        }
        for (std::size_t p = 0; p < w_.parliaments.size(); ++p) {
            const Parliament& parliament = w_.parliaments[p];
            const std::int64_t days = to_day_number(parliament.election_day) - to_day_number(w_.date);
            const std::int64_t months = days / 30 + 1;
            if (days < 0 || months > r_.campaign_months ||
                std::any_of(w_.elections.begin(), w_.elections.end(),
                            [&](const ElectionResult& e) { return e.parliament == static_cast<Index>(p); })) {
                continue;
            }
            // Lage je Wahlkreis einmal für alle Strömungen.
            struct Seat {
                Index district;
                std::vector<Fixed> shares;
            };
            std::vector<Seat> seats;
            for (std::size_t d = 0; d < w_.electoral_districts.size(); ++d) {
                if (w_.electoral_districts[d].parliament != static_cast<Index>(p)) {
                    continue;
                }
                const DistrictEstimate estimate = estimate_district(w_, static_cast<Index>(d));
                if (!estimate.boycott && estimate.shares.size() == currents) {
                    seats.push_back({static_cast<Index>(d), estimate.shares});
                }
            }
            for (std::size_t c = 0; c < currents; ++c) {
                const auto current = static_cast<Index>(c);
                if (current == w_.player_current) {
                    continue;
                }
                const Fixed budget =
                    (w_.party_funds[c] * (Fixed::one() - r_.campaign_reserve)) / Fixed::from_int(months);
                std::int64_t actions = (budget / cost).trunc();
                if (actions <= 0) {
                    continue;
                }
                // Knappheit: Abstand zum Ersten bzw. Vorsprung vor dem Zweiten, mit Rauschen.
                std::vector<std::pair<Fixed, Index>> close;
                for (const Seat& seat : seats) {
                    Fixed best_other;
                    std::size_t ahead = 0;
                    for (std::size_t o = 0; o < currents; ++o) {
                        if (o != c) {
                            best_other = std::max(best_other, seat.shares[o]);
                            ahead += seat.shares[o] > seat.shares[c] ? 1 : 0;
                        }
                    }
                    if (ahead > 1) {
                        continue; // nur wo die Strömung erste oder zweite ist
                    }
                    Fixed margin = seat.shares[c] - best_other;
                    margin = margin < Fixed::zero() ? -margin : margin;
                    margin += rng_.uniform(Fixed::from_ratio(-8, 100), Fixed::from_ratio(8, 100));
                    close.emplace_back(margin, seat.district);
                }
                std::stable_sort(close.begin(), close.end());
                for (std::size_t k = 0; k < close.size() && actions > 0; ++k, --actions) {
                    Command cmd;
                    cmd.type = CommandType::Campaign;
                    cmd.state = current;
                    cmd.target = close[k].second;
                    cmd.action = CampaignAction::Rally;
                    out_.push_back(cmd);
                }
            }
        }
    }

    World& w_;
    Pcg32& rng_;
    const EventCache& cache_;
    std::vector<Command>& out_;
    const AiRules r_;             // Regeln mit Stufe und Verlauf der Partie (seit M13a1)
    std::vector<Fixed> strength_; // je Staat, einmal je Monatserstem (run)
    Inclinations inclinations_{};
};

} // namespace

bool ai_accepts_alliance(const World& world, Index acceptor, Index proposer) {
    if (!in_range(world.ai_strategy, acceptor) || !in_range(world.ai_strategy, proposer)) {
        return false;
    }
    // Höchstens 'buendnisse' je Staat, für beide Seiten (Angebote eines Tages werden nacheinander wirksam).
    for (const Index s : {acceptor, proposer}) {
        const auto count = std::count_if(world.treaties.begin(), world.treaties.end(), [&](const Treaty& t) {
            return t.kind == TreatyKind::Alliance && (t.from == s || t.to == s);
        });
        if (count >= world.ai_rules.max_alliances) {
            return false;
        }
    }
    Fixed utility = (relation(world, acceptor, proposer) - Fixed::from_int(50)) / Fixed::from_int(5);
    const Index rival = world.ai_strategy[at(acceptor)].rival;
    if (rival != kNone && rival == world.ai_strategy[at(proposer)].rival && rival != acceptor &&
        rival != proposer) {
        utility += Fixed::from_int(20);
    }
    if (rival == proposer) {
        utility -= Fixed::from_int(30);
    }
    return utility >= effective_ai_rules(world).threshold;
}

bool ai_accepts_white_peace(const World& world, const War& war, bool attacker_side) {
    // Die Zeit arbeitet für die Seite, deren Gegner müder ist; eine klar überlegene Seite setzt auf den Sieg
    // (den Krieg beendet sonst die Regel aus militaer.md 4.5, spätestens bei Kriegsmüdigkeit 100).
    if (war_weariness(world, war, !attacker_side) > war_weariness(world, war, attacker_side)) {
        return false;
    }
    const auto strength = [&](const std::vector<Index>& side) {
        Fixed total;
        for (const Index s : side) {
            total += ai_strength(world, s);
        }
        return total;
    };
    const Fixed own = strength(attacker_side ? war.attackers : war.defenders);
    const Fixed other = strength(attacker_side ? war.defenders : war.attackers);
    return own < other * world.ai_rules.war_ratio;
}

Fixed ai_option_factor(const World& world, const EventOption& option, Index state) {
    if (!option.has_ai || !ai_active(world) || !in_range(world.states, state) ||
        state == world.event_player) {
        return Fixed::one();
    }
    const Inclinations values = state_inclinations(world, state);
    Fixed sum;
    for (std::size_t k = 0; k < kInclinationCount; ++k) {
        sum += option.ai_lean[k] * values[k];
    }
    return std::max(Fixed::one() + sum / Fixed::from_int(4), Fixed::from_ratio(1, 10));
}

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
