#include "uhrwerk/ai.hpp"

#include <algorithm>
#include <cstddef>
#include <string>

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

void run_ai(World& world, Pcg32& /*rng*/, EventCache& cache) {
    if (!ai_active(world)) {
        return;
    }
    if (world.date.day == 1) {
        update_offices(world, cache);
    }
}

} // namespace uhrwerk
