// Ereignislabor (seit M11, Schritt a5; docs/technik/ereignisse.md, Abschnitt 7).

#include "uhrwerk/event_lab.hpp"

#include "target_reader.hpp"
#include "uhrwerk/events.hpp"
#include "uhrwerk/script.hpp"

#include <algorithm>
#include <utility>

namespace uhrwerk {

namespace {

std::size_t at(Index index) {
    return static_cast<std::size_t>(index);
}

std::optional<ValueRange> share_range(TargetReader& r, const ScriptNode& block, std::string_view key) {
    const std::optional<ValueRange> range = r.range(block, key);
    if (range && (range->min < Fixed::zero() || range->max > Fixed::one())) {
        r.error(*block.find(key), "'" + std::string(key) + "' erwartet einen Bereich innerhalb von 0 bis 1");
        return std::nullopt;
    }
    return range;
}

Index find_stage(const World& world, Index journal, const std::string& id) {
    if (journal == kNone) {
        return kNone;
    }
    const JournalDef& j = world.journals[at(journal)];
    for (std::size_t i = 0; i < j.stages.size(); ++i) {
        if (j.stages[i].id == id && j.stages[i].outcome) {
            return static_cast<Index>(i);
        }
    }
    return kNone;
}

Index find_option(const World& world, Index event, const std::string& id) {
    const EventDef& e = world.events[at(event)];
    for (std::size_t i = 0; i < e.options.size(); ++i) {
        if (e.options[i].id == id) {
            return static_cast<Index>(i);
        }
    }
    return kNone;
}

/// ausgang <kennung> { anteil = { von bis } }
void read_outcome(TargetReader& r, const ScriptNode& node, const World& world, Index journal,
                  std::vector<OutcomeCorridor>& out) {
    if (!node.is_block || node.value.empty()) {
        r.error(node, "'ausgang' erwartet: ausgang <kennung> { anteil = { von bis } }");
        return;
    }
    if (journal == kNone) {
        r.error(node, "ausgang " + node.value + ": im Block 'labor' fehlt 'journal'");
        return;
    }
    const Index stage = find_stage(world, journal, node.value);
    if (stage == kNone) {
        r.error(node,
                "unbekannter Ausgang '" + node.value + "' des Journals " + world.journals[at(journal)].id);
        return;
    }
    r.only(node, {"anteil"});
    if (const auto share = share_range(r, node, "anteil")) {
        out.push_back(OutcomeCorridor{stage, *share});
    }
}

EventTargetsResult targets_from(const ParseResult& parsed, const World& world, const std::string& file_name) {
    EventTargetsResult result;
    for (const ParseError& e : parsed.errors) {
        result.errors.push_back(e.to_string());
    }
    if (!parsed.ok()) {
        return result;
    }
    EventTargets& t = result.targets;
    TargetReader r{file_name, {}};
    r.only(parsed.root, {"labor", "ausgang", "ereignis", "strategie"});
    const ScriptNode* lab = r.block(parsed.root, "labor");
    if (lab != nullptr) {
        r.only(*lab, {"bis", "journal", "nicht_festgelegt"});
        const ScriptNode* until = lab->find("bis");
        const std::optional<Date> date =
            until != nullptr && !until->is_block ? parse_date(until->value) : std::nullopt;
        if (!date) {
            r.error(until != nullptr ? *until : *lab, "'bis' erwartet ein Datum JJJJ.MM.TT");
        } else if (*date < world.date) {
            r.error(*until, "'bis' liegt vor dem Startdatum");
        } else {
            t.until = *date;
        }
        if (const ScriptNode* journal = lab->find("journal")) {
            t.journal = world.find_journal(journal->value);
            if (t.journal == kNone) {
                r.error(*journal, "unbekanntes Journal '" + journal->value + "'");
            }
        }
        if (const ScriptNode* flag = lab->find("nicht_festgelegt")) {
            t.undetermined = flag->value == "ja";
            if (flag->value != "ja" && flag->value != "nein") {
                r.error(*flag, "'nicht_festgelegt' erwartet ja oder nein");
            }
        }
    }
    for (const ScriptNode* node : parsed.root.find_all("ausgang")) {
        read_outcome(r, *node, world, t.journal, t.outcomes);
    }
    for (const ScriptNode* node : parsed.root.find_all("ereignis")) {
        if (!node->is_block || node->value.empty()) {
            r.error(*node, "'ereignis' erwartet: ereignis <kennung> { anteil = { von bis } }");
            continue;
        }
        EventCorridor c;
        c.event = world.find_event(node->value);
        if (c.event == kNone) {
            r.error(*node, "unbekanntes Ereignis '" + node->value + "'");
            continue;
        }
        r.only(*node, {"option", "bis", "anteil", "staaten"});
        if (const ScriptNode* option = node->find("option")) {
            c.option = find_option(world, c.event, option->value);
            if (c.option == kNone) {
                r.error(*option, "unbekannte Option '" + option->value + "' von Ereignis " + node->value);
                continue;
            }
        }
        if (const ScriptNode* until = node->find("bis")) {
            const std::optional<Date> date = until->is_block ? std::nullopt : parse_date(until->value);
            if (!date) {
                r.error(*until, "'bis' erwartet ein Datum JJJJ.MM.TT");
                continue;
            }
            c.has_until = true;
            c.until = *date;
        }
        c.states = node->find("staaten") != nullptr;
        if (c.states == (node->find("anteil") != nullptr)) {
            r.error(*node, "ereignis " + node->value + " braucht genau eines: anteil (Läufe) oder staaten");
            continue;
        }
        if (const auto share = share_range(r, *node, c.states ? "staaten" : "anteil")) {
            c.share = *share;
            t.events.push_back(c);
        }
    }
    for (const ScriptNode* node : parsed.root.find_all("strategie")) {
        if (!node->is_block || node->value.empty()) {
            r.error(*node, "'strategie' erwartet: strategie <kennung> { spieler = TAG wahl = { … } }");
            continue;
        }
        r.only(*node, {"spieler", "wahl", "ausgang"});
        EventStrategy s;
        s.id = node->value;
        const ScriptNode* player = node->find("spieler");
        s.player = player != nullptr ? world.find_state(player->value) : kNone;
        if (s.player == kNone) {
            r.error(player != nullptr ? *player : *node, "strategie " + s.id +
                                                             " braucht einen bestehenden Staat "
                                                             "als 'spieler'");
            continue;
        }
        if (const ScriptNode* choices = node->find("wahl")) {
            for (const ScriptNode& choice : choices->children) {
                const Index event = world.find_event(choice.key);
                const Index option = event == kNone ? kNone : find_option(world, event, choice.value);
                if (option == kNone) {
                    r.error(choice, "unbekanntes Ereignis oder unbekannte Option '" + choice.key + " = " +
                                        choice.value + "'");
                    continue;
                }
                s.choices.emplace_back(event, option);
            }
        }
        for (const ScriptNode* outcome : node->find_all("ausgang")) {
            read_outcome(r, *outcome, world, t.journal, s.outcomes);
        }
        t.strategies.push_back(std::move(s));
    }
    result.errors.insert(result.errors.end(), r.errors.begin(), r.errors.end());
    return result;
}

} // namespace

EventTargetsResult parse_event_targets(std::string_view text, const World& world,
                                       const std::string& file_name) {
    return targets_from(parse_script(text, file_name), world, file_name);
}

EventTargetsResult load_event_targets(const std::filesystem::path& file, const World& world) {
    return targets_from(parse_script_file(file), world, path_to_utf8(file));
}

EventLabRun run_event_lab(const World& world, const SimConfig& config, const EventTargets& targets,
                          const EventStrategy* strategy) {
    Simulation sim(world, config);
    if (strategy != nullptr) {
        Command player;
        player.type = CommandType::EventPlayer;
        player.state = strategy->player;
        sim.submit(player);
    }
    const World& w = sim.world();
    EventLabRun run;
    EventCache cache;
    while (w.date <= targets.until) {
        // Offene Ereignisse des Strategiestaates: bevorzugte Option, sonst die mit dem größten Gewicht.
        if (strategy != nullptr && !w.pending_events.empty()) {
            rebuild_event_cache(w, cache);
            for (const PendingEvent& p : w.pending_events) {
                if (p.state != strategy->player) {
                    continue;
                }
                const EventDef& e = w.events[at(p.event)];
                const std::vector<Fixed> weights = option_weights(w, cache, e, p.state);
                Index option = default_option(w, cache, e, p.state);
                for (const auto& [event, preferred] : strategy->choices) {
                    if (event == p.event && weights[at(preferred)] >= Fixed::zero()) {
                        option = preferred;
                    }
                }
                Command choice;
                choice.type = CommandType::EventChoice;
                choice.state = p.state;
                choice.value = Fixed::from_int(p.event);
                choice.target = option;
                sim.submit(choice);
            }
        }
        sim.step_day();
        if (w.date.day == 1) {
            for (const Fixed u : w.unrest) {
                run.unrest_max = Fixed::max(run.unrest_max, u);
            }
        }
    }

    if (targets.journal != kNone) {
        const JournalState& js = w.journal_states[at(targets.journal)];
        if (js.stage != kNone && w.journals[at(targets.journal)].stages[at(js.stage)].outcome) {
            run.outcome = js.stage;
            run.outcome_date = js.since;
        }
        for (const JournalStep& step : w.journal_history) {
            if (step.journal == targets.journal) {
                run.path.push_back(step);
            }
        }
    }
    for (const EventRecord& r : w.event_log) {
        const EventDef& e = w.events[at(r.event)];
        run.kinds[static_cast<std::size_t>(e.kind)] += 1;
        if (e.turning_point) {
            run.turning.push_back(r);
        }
    }
    for (const RevoltRecord& r : w.revolt_history) {
        run.revolts[static_cast<std::size_t>(r.outcome)] += 1;
    }
    run.revolts_running = static_cast<std::int64_t>(w.revolts.size());
    std::vector<Fixed> unrest;
    std::vector<Index> alive;
    for (std::size_t s = 0; s < w.states.size(); ++s) {
        const auto state = static_cast<Index>(s);
        if (!w.state_locations(state).empty()) {
            alive.push_back(state);
            if (s < w.unrest.size()) {
                unrest.push_back(w.unrest[s]);
            }
        }
    }
    run.unrest_median = quantile(unrest, 1, 2);
    run.laws.assign(w.law_groups.size(), {});
    for (std::size_t g = 0; g < w.law_groups.size(); ++g) {
        run.laws[g].assign(w.law_groups[g].options.size(), 0);
        for (const Index s : alive) {
            const State& st = w.states[at(s)];
            if (g < st.laws.size()) {
                run.laws[g][at(st.laws[g])] += 1;
            }
        }
    }
    for (const EventCorridor& c : targets.events) {
        std::vector<Index> hit;
        for (const EventRecord& r : w.event_log) {
            if (r.event == c.event && (c.option == kNone || r.option == c.option) &&
                (!c.has_until || r.date <= c.until) &&
                std::find(hit.begin(), hit.end(), r.state) == hit.end()) {
                hit.push_back(r.state);
            }
        }
        if (!c.states) {
            run.events.push_back(hit.empty() ? Fixed::zero() : Fixed::one());
        } else {
            std::int64_t count = 0;
            for (const Index s : alive) {
                count += std::find(hit.begin(), hit.end(), s) != hit.end() ? 1 : 0;
            }
            run.events.push_back(alive.empty()
                                     ? Fixed::zero()
                                     : Fixed::from_ratio(count, static_cast<std::int64_t>(alive.size())));
        }
    }
    return run;
}

EventLabSummary summarize_event_lab(const World& world, const EventTargets& targets,
                                    const EventStrategy* strategy, const std::vector<EventLabRun>& runs) {
    EventLabSummary summary;
    const std::size_t stages =
        targets.journal != kNone ? world.journals[at(targets.journal)].stages.size() : 0;
    summary.outcomes.assign(stages, 0);
    for (const EventLabRun& run : runs) {
        if (run.outcome == kNone) {
            ++summary.open;
        } else {
            summary.outcomes[at(run.outcome)] += 1;
        }
    }
    const auto n = static_cast<std::int64_t>(runs.size());
    if (n == 0) {
        return summary;
    }
    const std::vector<OutcomeCorridor>& outcomes =
        strategy != nullptr ? strategy->outcomes : targets.outcomes;
    for (std::size_t i = 0; i < outcomes.size(); ++i) {
        const Fixed share = Fixed::from_ratio(summary.outcomes[at(outcomes[i].stage)], n);
        if (!outcomes[i].share.contains(share)) {
            summary.violations.push_back(EventViolation{EventCheck::Outcome, i, share});
        }
    }
    if (strategy == nullptr && targets.undetermined) {
        for (std::size_t s = 0; s < summary.outcomes.size(); ++s) {
            if (summary.outcomes[s] == n) {
                summary.violations.push_back(EventViolation{EventCheck::Determined, s, Fixed::one()});
            }
        }
    }
    summary.events.assign(targets.events.size(), Fixed::zero());
    for (std::size_t c = 0; c < targets.events.size(); ++c) {
        Fixed total;
        for (const EventLabRun& run : runs) {
            total += run.events[c];
        }
        summary.events[c] = total / Fixed::from_int(n);
        if (strategy == nullptr && !targets.events[c].share.contains(summary.events[c])) {
            summary.violations.push_back(EventViolation{EventCheck::Event, c, summary.events[c]});
        }
    }
    return summary;
}

} // namespace uhrwerk
