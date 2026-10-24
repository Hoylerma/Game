// Kriegslabor (seit M10, Schritt a5; docs/technik/militaer.md, Abschnitt 6.3).

#include "uhrwerk/war_lab.hpp"

#include "target_reader.hpp"
#include "uhrwerk/date.hpp"
#include "uhrwerk/military.hpp"
#include "uhrwerk/script.hpp"

#include <algorithm>
#include <tuple>
#include <utility>

namespace uhrwerk {

namespace {

std::size_t at(Index index) {
    return static_cast<std::size_t>(index);
}

/// Ziele des Angreifers aus `ziel = { annexion = TAG gebiet = einheit fuehrung = ja zollunion = ja
/// entschaedigung = N }`; mehrere Einträge sind erlaubt.
void read_goals(TargetReader& r, const ScriptNode& node, const World& world, std::vector<WarGoal>& goals) {
    if (!node.is_block) {
        r.error(node, "'ziel' muss ein Block { … } sein");
        return;
    }
    r.only(node, {"annexion", "gebiet", "fuehrung", "zollunion", "entschaedigung"});
    for (const ScriptNode& child : node.children) {
        if (child.is_block) {
            continue; // only() meldet unbekannte Felder; Blöcke sind hier immer falsch
        }
        if (child.key == "annexion") {
            const Index state = world.find_state(child.value);
            if (state == kNone) {
                r.error(child, "unbekannter Staat '" + child.value + "' (besteht nicht zum Startdatum)");
                continue;
            }
            goals.push_back(WarGoal{WarGoalKind::Annexation, true, state});
        } else if (child.key == "gebiet") {
            const Index unit = world.find_admin_unit(child.value);
            if (unit == kNone) {
                r.error(child, "unbekannte Verwaltungseinheit '" + child.value + "'");
                continue;
            }
            goals.push_back(WarGoal{WarGoalKind::Territory, true, unit});
        } else if (child.key == "fuehrung" || child.key == "zollunion") {
            if (child.value != "ja") {
                r.error(child, "'" + child.key + "' erwartet ja");
                continue;
            }
            goals.push_back(
                WarGoal{child.key == "fuehrung" ? WarGoalKind::Leadership : WarGoalKind::Customs, true, 0});
        } else if (child.key == "entschaedigung") {
            const std::optional<Fixed> months = Fixed::parse(child.value);
            if (!months || *months < Fixed::one() || *months > Fixed::from_int(24) ||
                months->trunc() != months->round()) {
                r.error(child, "'entschaedigung' erwartet ganze Monatseinnahmen von 1 bis 24");
                continue;
            }
            goals.push_back(WarGoal{WarGoalKind::Indemnity, true, static_cast<Index>(months->trunc())});
        }
    }
    for (const ScriptNode& child : node.children) {
        if (child.is_block) {
            r.error(child, "'" + child.key + "' erwartet einen Wert, keinen Block");
        }
    }
}

std::optional<ValueRange> share_range(TargetReader& r, const ScriptNode& block, std::string_view key) {
    if (block.find(key) == nullptr) {
        return std::nullopt;
    }
    const std::optional<ValueRange> range = r.range(block, key);
    if (range && (range->min < Fixed::zero() || range->max > Fixed::one())) {
        r.error(*block.find(key), "'" + std::string(key) + "' erwartet einen Bereich innerhalb von 0 bis 1");
        return std::nullopt;
    }
    return range;
}

WarTargetsResult targets_from(const ParseResult& parsed, const World& world, const std::string& file_name) {
    WarTargetsResult result;
    for (const ParseError& e : parsed.errors) {
        result.errors.push_back(e.to_string());
    }
    if (!parsed.ok()) {
        return result;
    }
    TargetReader r{file_name, {}};
    r.only(parsed.root, {"krieg"});
    for (const ScriptNode* k : parsed.root.find_all("krieg")) {
        if (!k->is_block || k->value.empty()) {
            r.error(*k, "'krieg' erwartet: krieg <kennung> { angreifer verteidiger … }");
            continue;
        }
        if (std::any_of(result.targets.wars.begin(), result.targets.wars.end(),
                        [&](const WarScenario& w) { return w.id == k->value; })) {
            r.error(*k, "krieg " + k->value + " ist mehrfach angegeben");
            continue;
        }
        r.only(*k,
               {"angreifer", "verteidiger", "ziel", "tage", "sieg", "niederlage", "weiss", "dauer", "offen"});
        WarScenario war;
        war.id = k->value;
        const ScriptNode* attacker = k->find("angreifer");
        const ScriptNode* defender = k->find("verteidiger");
        if (attacker == nullptr || defender == nullptr || attacker->is_block || defender->is_block) {
            r.error(*k, "krieg " + k->value + " braucht 'angreifer' und 'verteidiger' (Staatstags)");
            continue;
        }
        war.attacker = world.find_state(attacker->value);
        war.defender = world.find_state(defender->value);
        bool valid = true;
        for (const auto& [node, state] :
             {std::pair{attacker, war.attacker}, std::pair{defender, war.defender}}) {
            if (state == kNone) {
                r.error(*node, "unbekannter Staat '" + node->value + "' (besteht nicht zum Startdatum)");
                valid = false;
            }
        }
        if (!valid) {
            continue;
        }
        if (war.attacker == war.defender) {
            r.error(*k, "krieg " + k->value + ": Angreifer und Verteidiger sind derselbe Staat");
            continue;
        }
        for (const ScriptNode* goal : k->find_all("ziel")) {
            read_goals(r, *goal, world, war.goals);
        }
        if (k->find("tage") != nullptr) {
            if (const std::optional<Fixed> days =
                    r.number(*k, "tage", ValueRange{Fixed::one(), Fixed::from_int(3650)})) {
                war.days = static_cast<std::int32_t>(days->trunc());
            }
        }
        war.victory = share_range(r, *k, "sieg");
        war.defeat = share_range(r, *k, "niederlage");
        war.white = share_range(r, *k, "weiss");
        if (k->find("dauer") != nullptr) {
            war.duration = r.range(*k, "dauer");
        }
        if (k->find("offen") != nullptr) {
            const std::optional<bool> open = k->get_bool("offen");
            if (!open) {
                r.error(*k->find("offen"), "'offen' erwartet ja oder nein");
            }
            war.open = open.value_or(false);
        }
        result.targets.wars.push_back(std::move(war));
    }
    result.errors.insert(result.errors.end(), r.errors.begin(), r.errors.end());
    if (!result.errors.empty()) {
        result.targets = {};
    }
    return result;
}

const War* war_between(const World& world, Index attacker, Index defender) {
    for (const War& w : world.wars) {
        if (w.attackers.front() == attacker && w.defenders.front() == defender) {
            return &w;
        }
    }
    return nullptr;
}

Command order(CommandType type, Index state, std::int64_t value, Index target) {
    Command c;
    c.type = type;
    c.state = state;
    c.value = Fixed::from_int(value);
    c.target = target;
    return c;
}

std::int32_t days_between(const Date& from, const Date& to) {
    return static_cast<std::int32_t>(to_day_number(to) - to_day_number(from));
}

Quantiles quantiles_of(const std::vector<WarLabRun>& runs, Fixed WarLabRun::*field) {
    std::vector<Fixed> values;
    for (const WarLabRun& run : runs) {
        if (run.started) {
            values.push_back(run.*field);
        }
    }
    return quantiles(values);
}

Quantiles quantiles_of(const std::vector<WarLabRun>& runs, std::int32_t WarLabRun::*field, bool peace_only) {
    std::vector<Fixed> values;
    for (const WarLabRun& run : runs) {
        if (run.started && (!peace_only || run.outcome != WarOutcome::Open)) {
            values.push_back(Fixed::from_int(run.*field));
        }
    }
    return quantiles(values);
}

} // namespace

WarTargetsResult parse_war_targets(std::string_view text, const World& world, const std::string& file_name) {
    return targets_from(parse_script(text, file_name), world, file_name);
}

WarTargetsResult load_war_targets(const std::filesystem::path& file, const World& world) {
    return targets_from(parse_script_file(file), world, path_to_utf8(file));
}

WarLabRun run_war_lab(const World& world, const SimConfig& config, const WarScenario& scenario) {
    WarLabRun run;
    Simulation sim(world, config);
    const World& w = sim.world();
    const Fixed attacker_debt = w.states[at(scenario.attacker)].debt;
    const Fixed defender_debt = w.states[at(scenario.defender)].debt;
    const War* war = war_between(w, scenario.attacker, scenario.defender);
    if (war == nullptr) {
        const std::int64_t first =
            scenario.goals.empty() ? 0 : encode_war_goal(scenario.goals[0].kind, scenario.goals[0].param);
        sim.submit(order(CommandType::DeclareWar, scenario.attacker, first, scenario.defender));
        sim.step_day();
        war = war_between(w, scenario.attacker, scenario.defender);
        if (war == nullptr) {
            return run;
        }
        for (std::size_t g = 1; g < scenario.goals.size(); ++g) {
            sim.submit(order(CommandType::WarGoal, scenario.attacker,
                             encode_war_goal(scenario.goals[g].kind, scenario.goals[g].param),
                             static_cast<Index>(war->id)));
        }
    }
    run.started = true;
    const std::int64_t id = war->id;
    const Date started = war->started;
    const auto result = [&]() -> const WarResult* {
        for (const WarResult& r : w.war_results) {
            if (r.id == id) {
                return &r;
            }
        }
        return nullptr;
    };
    while (result() == nullptr && find_war(w, id) != nullptr &&
           days_between(started, w.date) < scenario.days) {
        sim.step_day();
    }
    run.attacker_debt = w.states[at(scenario.attacker)].debt - attacker_debt;
    run.defender_debt = w.states[at(scenario.defender)].debt - defender_debt;
    run.days = days_between(started, w.date);
    if (const WarResult* r = result()) {
        run.days = days_between(started, r->ended);
        run.outcome = r->winner > 0   ? WarOutcome::Attacker
                      : r->winner < 0 ? WarOutcome::Defender
                                      : WarOutcome::White;
        run.battles = r->battles;
        run.attacker_losses = r->attacker_losses;
        run.defender_losses = r->defender_losses;
        run.transfers = r->transfers;
        for (auto e = w.chronicle.rbegin(); e != w.chronicle.rend(); ++e) {
            if (e->date == r->ended && e->text.starts_with("Frieden zwischen")) {
                run.peace = e->text;
                break;
            }
        }
    } else if (const War* open = find_war(w, id)) {
        run.battles = open->battles;
        run.attacker_losses = open->defender_inflicted;
        run.defender_losses = open->attacker_inflicted;
    } else {
        // Der Krieg erlosch mit dem Untergang einer Hauptpartei in einem anderen Krieg.
        run.outcome =
            w.states[at(scenario.attacker)].capital == kNone ? WarOutcome::Defender : WarOutcome::Attacker;
        run.peace = "Krieg erloschen: Untergang einer Hauptpartei";
    }
    return run;
}

WarLabSummary summarize_war_lab(const std::vector<WarScenario>& wars,
                                const std::vector<std::vector<WarLabRun>>& runs) {
    WarLabSummary summary;
    for (std::size_t i = 0; i < wars.size(); ++i) {
        const WarScenario& war = wars[i];
        const std::vector<WarLabRun>& all = runs[i];
        WarScenarioSummary s;
        for (const WarLabRun& run : all) {
            if (run.started) {
                ++s.outcomes[static_cast<std::size_t>(run.outcome)];
            } else {
                ++s.not_started;
            }
        }
        s.days = quantiles_of(all, &WarLabRun::days, true);
        s.battles = quantiles_of(all, &WarLabRun::battles, false);
        s.transfers = quantiles_of(all, &WarLabRun::transfers, false);
        s.attacker_losses = quantiles_of(all, &WarLabRun::attacker_losses);
        s.defender_losses = quantiles_of(all, &WarLabRun::defender_losses);
        s.attacker_debt = quantiles_of(all, &WarLabRun::attacker_debt);
        s.defender_debt = quantiles_of(all, &WarLabRun::defender_debt);

        if (s.not_started > 0) {
            summary.violations.push_back(
                WarViolation{WarCheck::NotStarted, i, Fixed::from_int(s.not_started)});
        }
        const std::int64_t total = std::max<std::int64_t>(1, static_cast<std::int64_t>(all.size()));
        const auto share = [&](WarOutcome outcome) {
            return Fixed::from_ratio(s.outcomes[static_cast<std::size_t>(outcome)], total);
        };
        for (const auto& [range, outcome, check] :
             {std::tuple{&war.victory, WarOutcome::Attacker, WarCheck::Victory},
              std::tuple{&war.defeat, WarOutcome::Defender, WarCheck::Defeat},
              std::tuple{&war.white, WarOutcome::White, WarCheck::White}}) {
            if (*range && !(*range)->contains(share(outcome))) {
                summary.violations.push_back(WarViolation{check, i, share(outcome)});
            }
        }
        const std::int64_t peace = static_cast<std::int64_t>(all.size()) - s.not_started -
                                   s.outcomes[static_cast<std::size_t>(WarOutcome::Open)];
        if (war.duration) {
            const Fixed median = peace > 0 ? s.days.median : Fixed::from_int(war.days);
            if (!war.duration->contains(median)) {
                summary.violations.push_back(WarViolation{WarCheck::Duration, i, median});
            }
        }
        if (war.open && all.size() > 1) {
            const auto most = *std::max_element(s.outcomes.begin(), s.outcomes.end());
            if (most == static_cast<std::int64_t>(all.size())) {
                summary.violations.push_back(WarViolation{WarCheck::Fixed, i, Fixed::from_int(most)});
            }
        }
        summary.wars.push_back(s);
    }
    return summary;
}

} // namespace uhrwerk
