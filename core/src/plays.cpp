// Diplomatische Spiele (seit M9, Schritt b2): docs/technik/diplomatie.md, Abschnitt 4.5.
//
// Ein Staat stellt eine Forderung; die übrigen Bundesstaaten nehmen Partei; das Spiel durchläuft drei
// Stufen zu je play_stage_days Tagen (Protest, Ultimatum, Mobilmachung). Am Ende jeder Stufe entscheidet
// jede Hauptpartei nach Regel – es sei denn, sie hat in dieser Stufe „eskalieren“ befohlen: Die stärkere
// Seite eskaliert, die schwächere bietet Vermittlung an, die angenommen wird, wenn sie mindestens
// play_yield der anderen stark ist (Kompromiss). Nach der dritten Stufe gibt die schwächere Seite nach,
// wenn sie darunter liegt, sonst endet das Spiel mit einem Kompromiss. Der Kern kennt keinen Spieler;
// Manöver sind Befehle wie alle anderen.

#include "uhrwerk/customs.hpp"
#include "uhrwerk/diplomacy.hpp"
#include "uhrwerk/military.hpp"

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

const std::string& name_of(const World& world, Index state) {
    return world.states[at(state)].short_name;
}

/// Ganze Zahl mit Tausenderpunkten (Chronik).
std::string grouped(std::int64_t value) {
    std::string digits = std::to_string(value < 0 ? -value : value);
    for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(digits.size()) - 3; i > 0; i -= 3) {
        digits.insert(static_cast<std::size_t>(i), ".");
    }
    return value < 0 ? "-" + digits : digits;
}

std::string currency(const World& world) {
    return world.scenario.currency.empty() ? "Reichstaler" : world.scenario.currency;
}

bool union_active(const World& world, Index customs_union) {
    if (!in_range(world.customs_unions, customs_union)) {
        return false;
    }
    const Index leader = world.customs_unions[at(customs_union)].leader;
    return in_range(world.states, leader) && world.states[at(leader)].customs_union == customs_union;
}

/// Verbündete des Ziels, die Gegner des Fordernden sind (Forderung „Bündnis lösen“).
std::vector<Index> allied_rivals(const World& world, Index demander, Index target) {
    std::vector<Index> result;
    for (const Treaty& t : world.treaties) {
        if (t.kind != TreatyKind::Alliance) {
            continue;
        }
        const Index ally = t.from == target ? t.to : t.to == target ? t.from : kNone;
        if (ally != kNone && ally != demander && rivals(world, demander, ally) &&
            std::find(result.begin(), result.end(), ally) == result.end()) {
            result.push_back(ally);
        }
    }
    return result;
}

std::string list_names(const World& world, const std::vector<Index>& states) {
    std::string text;
    for (std::size_t i = 0; i < states.size(); ++i) {
        text += (i == 0 ? "" : i + 1 == states.size() ? " und " : ", ") + name_of(world, states[i]);
    }
    return text;
}

std::string demand_text(const World& world, DemandKind kind, Index demander, Index target,
                        Index customs_union) {
    switch (kind) {
    case DemandKind::Leadership:
        return "die Führung im Bund";
    case DemandKind::CustomsUnion:
        return "den Beitritt zum " + world.customs_unions[at(customs_union)].name;
    case DemandKind::BreakAlliance:
        return "die Kündigung der Bündnisse mit " + list_names(world, allied_rivals(world, demander, target));
    }
    return {};
}

std::string crisis(const World& world, const DiplomaticPlay& play) {
    return "Krise zwischen " + name_of(world, play.demander) + " und " + name_of(world, play.target);
}

/// Kasse um amount verringern; was fehlt, wird Schuld (wie im Haushalt).
void pay(State& state, Fixed amount) {
    state.treasury -= amount;
    if (state.treasury < Fixed::zero()) {
        state.debt -= state.treasury;
        state.treasury = Fixed::zero();
    }
}

/// Beendet das Spiel mit dem Ergebnis, wendet die Wirkung an und schreibt Chronik und Ergebnis.
void finish(World& world, std::size_t index, PlayOutcome outcome) {
    const DiplomaticPlay play = world.plays[index];
    world.plays.erase(world.plays.begin() + static_cast<std::ptrdiff_t>(index));
    // Mit Militär (seit M10, militaer.md 3.3) rüsten die Hauptparteien ab, die die Krise mobil gemacht hat.
    end_crisis_mobilization(world, play.demander);
    end_crisis_mobilization(world, play.target);
    world.play_results.push_back(PlayResult{play.kind, play.demander, play.target, play.customs_union,
                                            play.started, world.date, play.stage, outcome});
    const std::string& demander = name_of(world, play.demander);
    const std::string& target = name_of(world, play.target);
    switch (outcome) {
    case PlayOutcome::DemanderWon:
        chronicle(world, play.target,
                  target + " gibt nach: " + demander + " setzt " +
                      demand_text(world, play.kind, play.demander, play.target, play.customs_union) +
                      " durch.");
        break;
    case PlayOutcome::TargetWon:
        chronicle(world, play.demander,
                  demander + " gibt nach; die Forderung an " + target + " ist gescheitert.");
        return;
    case PlayOutcome::Compromise:
        chronicle(world, play.demander, "Die " + crisis(world, play) + " endet mit einem Kompromiss.");
        break;
    case PlayOutcome::War:
        // Seit M10 (militaer.md 4.1): Der Krieg hat schon begonnen; keine Wirkung der Forderung.
        chronicle(world, play.demander, "Die " + crisis(world, play) + " wird zum Krieg.");
        return;
    }
    const bool full = outcome == PlayOutcome::DemanderWon;
    switch (play.kind) {
    case DemandKind::Leadership:
        break; // Anzeige „Die deutsche Frage“ (Abschnitt 4.6)
    case DemandKind::CustomsUnion:
        if (full) {
            // Beitritt zum nächsten 1. Januar, ohne Vorbehalt der Leitmacht.
            std::erase_if(world.customs_requests,
                          [&](const CustomsRequest& r) { return r.state == play.target; });
            world.customs_requests.push_back(CustomsRequest{play.target, play.customs_union});
            chronicle(world, play.target,
                      target + " tritt zum " + format_date_long(customs_forced_date(world.date)) + " dem " +
                          world.customs_unions[at(play.customs_union)].name + " bei.");
        } else {
            // Kompromiss: eine Zahlung von einem halben Monatseinkommen.
            const Fixed amount = world.states[at(play.target)].last_month_income / Fixed::from_int(2);
            if (amount > Fixed::zero()) {
                pay(world.states[at(play.target)], amount);
                world.states[at(play.demander)].treasury += amount;
                chronicle(world, play.target,
                          target + " zahlt " + demander + " " + grouped(amount.round()) + " " +
                              currency(world) + ".");
            }
        }
        break;
    case DemandKind::BreakAlliance:
        for (const Index ally : allied_rivals(world, play.demander, play.target)) {
            if (full) {
                request_treaty(world, play.target, ally, TreatyAction::CancelAlliance);
            } else {
                // Kompromiss: Das Bündnis bleibt, aber es ist belastet.
                adjust_relation(world, play.target, ally, world.diplomacy_rules.play_break);
            }
        }
        break;
    }
}

/// Mediation durch die Seite des Staates offered: angenommen, wenn sie mindestens play_yield der
/// anderen stark ist.
bool mediation_accepted(const World& world, const DiplomaticPlay& play, bool by_demander) {
    const Fixed own = side_strength(world, play, by_demander);
    const Fixed other = side_strength(world, play, !by_demander);
    return own >= other * world.diplomacy_rules.play_yield;
}

} // namespace

Index play_of(const World& world, Index state) {
    for (std::size_t i = 0; i < world.plays.size(); ++i) {
        if (world.plays[i].demander == state || world.plays[i].target == state) {
            return static_cast<Index>(i);
        }
    }
    return kNone;
}

Fixed state_strength(const World& world, Index state) {
    if (!in_range(world.states, state)) {
        return Fixed::zero();
    }
    return world.states[at(state)].army_per_mille * world.state_population(state) / Fixed::from_int(1000);
}

Fixed side_strength(const World& world, const DiplomaticPlay& play, bool demander_side) {
    Fixed strength = state_strength(world, demander_side ? play.demander : play.target);
    for (const Index s : demander_side ? play.demander_side : play.target_side) {
        strength += state_strength(world, s);
    }
    return strength;
}

void play_sides(const World& world, Index demander, Index target, std::vector<Index>& demander_side,
                std::vector<Index>& target_side) {
    demander_side.clear();
    target_side.clear();
    const Fixed leaning = world.diplomacy_rules.play_leaning;
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        const auto state = static_cast<Index>(s);
        if (state == demander || state == target || !federal_state(world, state)) {
            continue;
        }
        int side = 0; // 1 Fordernder, −1 Ziel, 0 neutral
        const bool ally_d = allied(world, state, demander);
        const bool ally_t = allied(world, state, target);
        const bool guard_d = guarantees(world, state, demander);
        const bool guard_t = guarantees(world, state, target);
        if (ally_d != ally_t) {
            side = ally_d ? 1 : -1;
        } else if (!ally_d && guard_d != guard_t) {
            side = guard_d ? 1 : -1;
        } else if (!ally_d && !guard_d) {
            const Fixed difference = relation(world, state, demander) - relation(world, state, target);
            side = difference > leaning ? 1 : difference < -leaning ? -1 : 0;
        }
        if (side > 0) {
            demander_side.push_back(state);
        } else if (side < 0) {
            target_side.push_back(state);
        }
    }
}

DiplomacyDecision evaluate_demand(const World& world, Index demander, Index target, DemandKind kind) {
    if (world.relations.empty() || !in_range(world.states, demander) || !in_range(world.states, target) ||
        demander == target || !federal_state(world, demander) || !federal_state(world, target) ||
        play_of(world, demander) != kNone || play_of(world, target) != kNone) {
        return DiplomacyDecision::Invalid;
    }
    switch (kind) {
    case DemandKind::Leadership:
        return DiplomacyDecision::Accepted;
    case DemandKind::CustomsUnion: {
        const Index own = world.states[at(demander)].customs_union;
        return own != kNone && union_active(world, own) && world.states[at(target)].customs_union != own
                   ? DiplomacyDecision::Accepted
                   : DiplomacyDecision::Invalid;
    }
    case DemandKind::BreakAlliance:
        return allied_rivals(world, demander, target).empty() ? DiplomacyDecision::Invalid
                                                              : DiplomacyDecision::Accepted;
    }
    return DiplomacyDecision::Invalid;
}

DiplomacyDecision start_play(World& world, Index demander, Index target, DemandKind kind) {
    const DiplomacyDecision decision = evaluate_demand(world, demander, target, kind);
    if (decision != DiplomacyDecision::Accepted) {
        return decision;
    }
    DiplomaticPlay play;
    play.kind = kind;
    play.demander = demander;
    play.target = target;
    play.customs_union = kind == DemandKind::CustomsUnion ? world.states[at(demander)].customs_union : kNone;
    play.started = world.date;
    play.stage_started = world.date;
    play_sides(world, demander, target, play.demander_side, play.target_side);
    chronicle(world, demander,
              name_of(world, demander) + " fordert von " + name_of(world, target) + " " +
                  demand_text(world, kind, demander, target, play.customs_union) +
                  " (Protest). Auf seiner Seite " + std::to_string(play.demander_side.size()) +
                  ", auf der Gegenseite " + std::to_string(play.target_side.size()) + " Bundesstaaten.");
    // Einmalig: Staaten auf verschiedenen Seiten entfremden sich (Abschnitt 4.1).
    std::vector<Index> one = play.demander_side;
    one.push_back(demander);
    std::vector<Index> other = play.target_side;
    other.push_back(target);
    for (const Index a : one) {
        for (const Index b : other) {
            adjust_relation(world, a, b, world.diplomacy_rules.play_break);
        }
    }
    world.plays.push_back(std::move(play));
    return decision;
}

DiplomacyDecision evaluate_manoeuvre(const World& world, Index state, Manoeuvre manoeuvre) {
    const Index index = play_of(world, state);
    if (index == kNone) {
        return DiplomacyDecision::Invalid;
    }
    const DiplomaticPlay& play = world.plays[at(index)];
    const bool demander = play.demander == state;
    switch (manoeuvre) {
    case Manoeuvre::Yield:
        return DiplomacyDecision::Accepted;
    case Manoeuvre::Mediate:
        return mediation_accepted(world, play, demander) ? DiplomacyDecision::Accepted
                                                         : DiplomacyDecision::Rejected;
    case Manoeuvre::Escalate:
        return (demander ? play.demander_escalates : play.target_escalates) ? DiplomacyDecision::Invalid
                                                                            : DiplomacyDecision::Accepted;
    }
    return DiplomacyDecision::Invalid;
}

DiplomacyDecision apply_manoeuvre(World& world, Index state, Manoeuvre manoeuvre) {
    const DiplomacyDecision decision = evaluate_manoeuvre(world, state, manoeuvre);
    if (decision == DiplomacyDecision::Invalid) {
        return decision;
    }
    const auto index = at(play_of(world, state));
    DiplomaticPlay& play = world.plays[index];
    const bool demander = play.demander == state;
    const Index other = demander ? play.target : play.demander;
    switch (manoeuvre) {
    case Manoeuvre::Yield:
        finish(world, index, demander ? PlayOutcome::TargetWon : PlayOutcome::DemanderWon);
        break;
    case Manoeuvre::Mediate:
        chronicle(world, state,
                  name_of(world, state) + " bietet Vermittlung an; " + name_of(world, other) +
                      (decision == DiplomacyDecision::Accepted ? " nimmt an." : " lehnt ab."));
        if (decision == DiplomacyDecision::Accepted) {
            finish(world, index, PlayOutcome::Compromise);
        }
        break;
    case Manoeuvre::Escalate:
        (demander ? play.demander_escalates : play.target_escalates) = true;
        break;
    }
    return decision;
}

void run_plays(World& world) {
    const std::int64_t today = to_day_number(world.date);
    const DiplomacyRules& rules = world.diplomacy_rules;
    for (std::size_t i = 0; i < world.plays.size();) {
        DiplomaticPlay& play = world.plays[i];
        if (today - to_day_number(play.stage_started) < rules.play_stage_days) {
            ++i;
            continue;
        }
        const Fixed demander = side_strength(world, play, true);
        const Fixed target = side_strength(world, play, false);
        // Die schwächere Seite bietet Vermittlung an, wenn sie nicht „eskalieren“ befohlen hat.
        const bool demander_offers = !play.demander_escalates && demander < target;
        const bool target_offers = !play.target_escalates && target < demander;
        bool ended = false;
        for (const bool by_demander : {true, false}) {
            if (ended || !(by_demander ? demander_offers : target_offers)) {
                continue;
            }
            const bool accepted = mediation_accepted(world, play, by_demander);
            const Index offering = by_demander ? play.demander : play.target;
            chronicle(world, offering,
                      name_of(world, offering) + " bietet Vermittlung an; " +
                          name_of(world, by_demander ? play.target : play.demander) +
                          (accepted ? " nimmt an." : " lehnt ab."));
            if (accepted) {
                finish(world, i, PlayOutcome::Compromise);
                ended = true;
            }
        }
        if (ended) {
            continue;
        }
        if (play.stage < 3) {
            ++play.stage;
            play.stage_started = world.date;
            play.demander_escalates = false;
            play.target_escalates = false;
            adjust_relation(world, play.demander, play.target, rules.play_stage);
            chronicle(world, play.demander,
                      crisis(world, play) + (play.stage == 2 ? ": Ultimatum." : ": Mobilmachung."));
            // Mit Militär (seit M10, militaer.md 3.3) machen beide Hauptparteien wirklich mobil.
            if (play.stage == 3 && military_active(world)) {
                const Index demander_state = play.demander;
                const Index target_state = play.target;
                order_mobilization(world, demander_state, true, true);
                order_mobilization(world, target_state, true, true);
            }
            ++i;
            continue;
        }
        // Auflösung nach der dritten Stufe. Gibt keine Seite nach, wird die Krise mit Militär zum Krieg
        // (seit M10, militaer.md 4.1, Entscheidung 9), sonst endet sie mit einem Kompromiss.
        if (demander < target * rules.play_yield) {
            finish(world, i, PlayOutcome::TargetWon);
        } else if (target < demander * rules.play_yield) {
            finish(world, i, PlayOutcome::DemanderWon);
        } else if (military_active(world)) {
            std::vector<Index> attackers{play.demander};
            attackers.insert(attackers.end(), play.demander_side.begin(), play.demander_side.end());
            std::vector<Index> defenders{play.target};
            defenders.insert(defenders.end(), play.target_side.begin(), play.target_side.end());
            const bool war = start_war(world, std::move(attackers), std::move(defenders), WarCause::Crisis);
            finish(world, i, war ? PlayOutcome::War : PlayOutcome::Compromise);
        } else {
            finish(world, i, PlayOutcome::Compromise);
        }
    }
}

bool mobilized(const World& world, Index state) {
    return std::any_of(world.plays.begin(), world.plays.end(), [&](const DiplomaticPlay& p) {
        return p.stage >= 3 && (p.demander == state || p.target == state);
    });
}

} // namespace uhrwerk
