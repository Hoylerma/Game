// Bundesversammlung (seit M9, Schritt b2): docs/technik/diplomatie.md, Abschnitt 4.4.

#include "uhrwerk/customs.hpp"
#include "uhrwerk/diplomacy.hpp"

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

/// Bevölkerungsreichster Staat mit Plenarstimme außer except, der accept erfüllt.
template <typename Accept> Index most_populous(const World& world, Index except, Accept accept) {
    Index best = kNone;
    Fixed best_people;
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        const auto state = static_cast<Index>(s);
        if (state == except || plenum_votes(world, state) <= 0 || !accept(state)) {
            continue;
        }
        const Fixed people = world.state_population(state);
        if (best == kNone || people > best_people) {
            best = state;
            best_people = people;
        }
    }
    return best;
}

bool union_active(const World& world, Index customs_union) {
    if (!in_range(world.customs_unions, customs_union)) {
        return false;
    }
    const Index leader = world.customs_unions[at(customs_union)].leader;
    return in_range(world.states, leader) && world.states[at(leader)].customs_union == customs_union;
}

std::string motion_text(const World& world, MotionKind kind, Index target) {
    switch (kind) {
    case MotionKind::Reform:
        return "eine Bundesreform";
    case MotionKind::CustomsUnification:
        return "die Zolleinigung des Bundes im " + world.customs_unions[at(target)].name;
    case MotionKind::Intervention:
        return "eine Bundesintervention gegen " + world.states[at(target)].short_name;
    case MotionKind::Presidency:
        return "den Vorsitz im Bund";
    }
    return {};
}

/// Plenum: Bundesreform und Präsidium einstimmig, Zolleinigung zwei Drittel; Engerer Rat: Mehrheit.
bool plenum_motion(MotionKind kind) {
    return kind != MotionKind::Intervention;
}

} // namespace

bool has_diet(const World& world) {
    return !world.diet.id.empty() && !world.relations.empty();
}

std::int32_t plenum_votes(const World& world, Index state) {
    return in_range(world.diet.plenum, state) ? world.diet.plenum[at(state)] : 0;
}

Index motion_opponent(const World& world, MotionKind kind, Index applicant, Index target) {
    Index opponent = kNone;
    switch (kind) {
    case MotionKind::Reform:
    case MotionKind::Presidency:
        opponent = world.diet_presidency;
        break;
    case MotionKind::Intervention:
        opponent = target;
        break;
    case MotionKind::CustomsUnification:
        return most_populous(world, applicant, [&](Index s) {
            const Index membership = world.states[at(s)].customs_union;
            if (membership == target) {
                return false;
            }
            return membership == kNone || world.customs_unions[at(membership)].leader == s ||
                   !union_active(world, membership);
        });
    }
    if (opponent == applicant || !in_range(world.states, opponent)) {
        return most_populous(world, applicant, [](Index) { return true; });
    }
    return opponent;
}

bool votes_yes(const World& world, Index state, Index applicant, Index opponent) {
    if (state == applicant) {
        return true;
    }
    if (state == opponent) {
        return false;
    }
    if (opponent == kNone) {
        return relation(world, state, applicant) > Fixed::zero();
    }
    return relation(world, state, applicant) > relation(world, state, opponent);
}

DiplomacyDecision evaluate_motion(const World& world, Index applicant, MotionKind kind, Index target) {
    if (!has_diet(world) || plenum_votes(world, applicant) <= 0 || !world.motions.empty()) {
        return DiplomacyDecision::Invalid;
    }
    switch (kind) {
    case MotionKind::Reform:
        return target == kNone ? DiplomacyDecision::Accepted : DiplomacyDecision::Invalid;
    case MotionKind::Presidency:
        return target == kNone && applicant != world.diet_presidency ? DiplomacyDecision::Accepted
                                                                     : DiplomacyDecision::Invalid;
    case MotionKind::Intervention:
        return plenum_votes(world, target) > 0 && target != applicant ? DiplomacyDecision::Accepted
                                                                      : DiplomacyDecision::Invalid;
    case MotionKind::CustomsUnification:
        if (!in_range(world.customs_unions, target)) {
            return DiplomacyDecision::Invalid;
        }
        // Nur, wenn ein Bundesstaat außerhalb der Union bleibt.
        for (std::size_t s = 0; s < world.states.size(); ++s) {
            if (plenum_votes(world, static_cast<Index>(s)) > 0 && world.states[s].customs_union != target) {
                return DiplomacyDecision::Accepted;
            }
        }
        return DiplomacyDecision::Invalid;
    }
    return DiplomacyDecision::Invalid;
}

DiplomacyDecision file_motion(World& world, Index applicant, MotionKind kind, Index target) {
    const DiplomacyDecision decision = evaluate_motion(world, applicant, kind, target);
    if (decision != DiplomacyDecision::Accepted) {
        return decision;
    }
    world.motions.push_back(Motion{kind, applicant, target, world.date});
    Date vote = world.date;
    vote.day = 1;
    vote.month = vote.month == 12 ? 1 : vote.month + 1;
    vote.year = world.date.month == 12 ? world.date.year + 1 : world.date.year;
    chronicle(world, applicant,
              world.states[at(applicant)].short_name + " beantragt in der " + world.diet.name + " " +
                  motion_text(world, kind, target) + "; abgestimmt wird am " + format_date_long(vote) + ".");
    return decision;
}

MotionResult tally_motion(const World& world, const Motion& motion) {
    MotionResult result;
    result.kind = motion.kind;
    result.applicant = motion.applicant;
    result.target = motion.target;
    result.date = world.date;
    result.opponent = motion_opponent(world, motion.kind, motion.applicant, motion.target);
    std::vector<bool> yes(world.states.size(), false);
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        const auto state = static_cast<Index>(s);
        const std::int32_t votes = plenum_votes(world, state);
        if (votes <= 0) {
            continue;
        }
        yes[s] = votes_yes(world, state, motion.applicant, result.opponent);
        result.plenum_total += votes;
        result.plenum_yes += yes[s] ? votes : 0;
    }
    // Engerer Rat: eine Stimme je Eintrag; in einer Kurie entscheidet die Mehrheit ihrer Plenarstimmen,
    // bei Gleichstand Nein.
    for (const CouncilVote& vote : world.diet.council) {
        std::int32_t for_votes = 0;
        std::int32_t against = 0;
        for (const Index state : vote.states) {
            const std::int32_t weight = std::max<std::int32_t>(plenum_votes(world, state), 1);
            (yes[at(state)] ? for_votes : against) += weight;
        }
        if (for_votes + against == 0) {
            continue;
        }
        ++result.council_total;
        result.council_yes += for_votes > against ? 1 : 0;
    }
    switch (motion.kind) {
    case MotionKind::Reform:
    case MotionKind::Presidency:
        result.accepted = result.plenum_total > 0 && result.plenum_yes == result.plenum_total;
        break;
    case MotionKind::CustomsUnification:
        result.accepted = result.plenum_total > 0 && result.plenum_yes * 3 >= result.plenum_total * 2;
        break;
    case MotionKind::Intervention:
        result.accepted = result.council_yes * 2 > result.council_total;
        break;
    }
    return result;
}

void run_diet(World& world) {
    if (world.motions.empty() || !(world.motions.front().filed < world.date)) {
        return;
    }
    const Motion motion = world.motions.front();
    world.motions.clear();
    if (!in_range(world.states, motion.applicant)) {
        return;
    }
    const MotionResult result = tally_motion(world, motion);
    world.motion_results.push_back(result);
    const std::string applicant = world.states[at(motion.applicant)].short_name;
    std::string text = "Die " + world.diet.name +
                       (result.accepted ? " nimmt den Antrag von " : " lehnt den Antrag von ") + applicant +
                       " auf " + motion_text(world, motion.kind, motion.target) +
                       (result.accepted ? " an" : " ab");
    if (plenum_motion(motion.kind)) {
        text += " (Plenum: " + std::to_string(result.plenum_yes) + " von " +
                std::to_string(result.plenum_total) + " Stimmen dafür, " +
                (motion.kind == MotionKind::CustomsUnification ? "zwei Drittel" : "Einstimmigkeit") +
                " nötig).";
    } else {
        text += " (Engerer Rat: " + std::to_string(result.council_yes) + " von " +
                std::to_string(result.council_total) + " Stimmen dafür).";
    }
    chronicle(world, motion.applicant, std::move(text));
    if (!result.accepted) {
        return;
    }
    switch (motion.kind) {
    case MotionKind::Reform:
        break; // Anzeige „Die deutsche Frage“ (Abschnitt 4.6); M11 knüpft daran an.
    case MotionKind::Presidency:
        world.diet_presidency = motion.applicant;
        break;
    case MotionKind::Intervention: {
        // Wirkung in M9: das Ziel verliert an Beziehung zu jedem, der dafür stimmte (Vollzug mit M10).
        std::vector<Index> supporters;
        for (std::size_t s = 0; s < world.states.size(); ++s) {
            const auto state = static_cast<Index>(s);
            if (plenum_votes(world, state) > 0 && state != motion.target &&
                votes_yes(world, state, motion.applicant, result.opponent)) {
                supporters.push_back(state);
            }
        }
        for (const Index state : supporters) {
            adjust_relation(world, state, motion.target, world.diplomacy_rules.intervention);
        }
        break;
    }
    case MotionKind::CustomsUnification: {
        // Alle Bundesstaaten werden zum nächsten 1. Januar Mitglied der Union (ohne Vorbehalt der Leitmacht).
        for (std::size_t s = 0; s < world.states.size(); ++s) {
            const auto state = static_cast<Index>(s);
            if (plenum_votes(world, state) <= 0 || world.states[s].customs_union == motion.target) {
                continue;
            }
            std::erase_if(world.customs_requests, [&](const CustomsRequest& r) { return r.state == state; });
            world.customs_requests.push_back(CustomsRequest{state, motion.target});
        }
        chronicle(world, motion.applicant,
                  "Alle Bundesstaaten treten zum " + format_date_long(customs_effective_date(world.date)) +
                      " dem " + world.customs_unions[at(motion.target)].name + " bei.");
        break;
    }
    }
}

} // namespace uhrwerk
