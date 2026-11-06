// KI-Labor (seit M12, Schritt a5; docs/technik/ki.md, Abschnitt 7).

#include "uhrwerk/ai_lab.hpp"

#include "target_reader.hpp"
#include "uhrwerk/ai.hpp"
#include "uhrwerk/script.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <optional>
#include <thread>
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

/// Ausgang (Stufe mit ausgang) des Journals; kNone, wenn es ihn nicht gibt.
Index find_outcome(const World& world, Index journal, std::string_view id) {
    const JournalDef& j = world.journals[at(journal)];
    for (std::size_t i = 0; i < j.stages.size(); ++i) {
        if (j.stages[i].id == id && j.stages[i].outcome) {
            return static_cast<Index>(i);
        }
    }
    return kNone;
}

Index read_outcome_id(TargetReader& r, const ScriptNode& node, std::string_view id, const World& world,
                      Index journal) {
    if (journal == kNone) {
        r.error(node, "Ausgang " + std::string(id) + ": im Block 'labor' fehlt 'journal'");
        return kNone;
    }
    const Index stage = find_outcome(world, journal, id);
    if (stage == kNone) {
        r.error(node, "unbekannter Ausgang '" + std::string(id) + "' des Journals " +
                          world.journals[at(journal)].id);
    }
    return stage;
}

Index read_state(TargetReader& r, const ScriptNode& node, std::string_view tag, const World& world) {
    const Index state = world.find_state(tag);
    if (state == kNone) {
        r.error(node, "unbekannter Staat '" + std::string(tag) + "'");
    }
    return state;
}

void read_lab(TargetReader& r, const ScriptNode& lab, const World& world, AiTargets& t) {
    r.only(lab, {"bis", "journal", "hoechstens"});
    const ScriptNode* until = lab.find("bis");
    const std::optional<Date> date =
        until != nullptr && !until->is_block ? parse_date(until->value) : std::nullopt;
    if (!date) {
        r.error(until != nullptr ? *until : lab, "'bis' erwartet ein Datum JJJJ.MM.TT");
    } else if (*date < world.date) {
        r.error(*until, "'bis' liegt vor dem Startdatum");
    } else {
        t.until = *date;
    }
    if (const ScriptNode* journal = lab.find("journal")) {
        t.journal = world.find_journal(journal->value);
        if (t.journal == kNone) {
            r.error(*journal, "unbekanntes Journal '" + journal->value + "'");
        }
    }
    if (lab.find("hoechstens") != nullptr) {
        if (const auto value = r.number(lab, "hoechstens", ValueRange{Fixed::zero(), Fixed::one()})) {
            t.has_max_outcome = true;
            t.max_outcome = *value;
        }
    }
}

AiTargetsResult targets_from(const ParseResult& parsed, const World& world, const std::string& file_name) {
    AiTargetsResult result;
    for (const ParseError& e : parsed.errors) {
        result.errors.push_back(e.to_string());
    }
    if (!parsed.ok()) {
        return result;
    }
    AiTargets& t = result.targets;
    TargetReader r{file_name, {}};
    r.only(parsed.root, {"labor", "einigung", "ausgang", "krieg", "bankrott"});
    if (const ScriptNode* lab = r.block(parsed.root, "labor")) {
        read_lab(r, *lab, world, t);
    }
    if (const ScriptNode* unification = parsed.root.find("einigung")) {
        if (!unification->is_block) {
            r.error(*unification, "'einigung' erwartet: einigung { ausgaenge = { … } anteil = { von bis } }");
        } else {
            r.only(*unification, {"ausgaenge", "anteil"});
            const ScriptNode* list = unification->find("ausgaenge");
            if (list == nullptr || !list->is_block || list->bare_values().empty()) {
                r.error(list != nullptr ? *list : *unification,
                        "'ausgaenge' erwartet eine Liste { … } von Ausgängen");
            } else {
                for (const std::string_view id : list->bare_values()) {
                    const Index stage = read_outcome_id(r, *list, id, world, t.journal);
                    if (stage != kNone) {
                        t.unification.push_back(stage);
                    }
                }
            }
            if (const auto share = share_range(r, *unification, "anteil")) {
                t.has_unification = true;
                t.unification_share = *share;
            }
        }
    }
    for (const ScriptNode* node : parsed.root.find_all("ausgang")) {
        if (!node->is_block || node->value.empty()) {
            r.error(*node, "'ausgang' erwartet: ausgang <kennung> { anteil = { von bis } }");
            continue;
        }
        r.only(*node, {"anteil"});
        const Index stage = read_outcome_id(r, *node, node->value, world, t.journal);
        const auto share = share_range(r, *node, "anteil");
        if (stage != kNone && share) {
            t.outcomes.push_back(OutcomeCorridor{stage, *share});
        }
    }
    for (const ScriptNode* node : parsed.root.find_all("krieg")) {
        const ScriptNode* other = node->is_block ? node->find("gegen") : nullptr;
        if (!node->is_block || node->value.empty() || other == nullptr || other->is_block) {
            r.error(*node, "'krieg' erwartet: krieg TAG { gegen = TAG anteil = { von bis } }");
            continue;
        }
        r.only(*node, {"gegen", "anteil"});
        AiWarCorridor c;
        c.first = read_state(r, *node, node->value, world);
        c.second = read_state(r, *other, other->value, world);
        const auto share = share_range(r, *node, "anteil");
        if (c.first != kNone && c.second != kNone && c.first == c.second) {
            r.error(*other, "krieg " + node->value + ": 'gegen' nennt denselben Staat");
        } else if (c.first != kNone && c.second != kNone && share) {
            c.share = *share;
            t.wars.push_back(c);
        }
    }
    for (const ScriptNode* node : parsed.root.find_all("bankrott")) {
        if (!node->is_block) {
            r.error(*node, "'bankrott' erwartet: bankrott { staaten = { TAG … } anteil = { von bis } }");
            continue;
        }
        r.only(*node, {"staaten", "anteil"});
        AiBankruptcyCorridor c;
        const ScriptNode* list = node->find("staaten");
        if (list == nullptr || !list->is_block || list->bare_values().empty()) {
            r.error(list != nullptr ? *list : *node, "'staaten' erwartet eine Liste { TAG … }");
            continue;
        }
        for (const std::string_view tag : list->bare_values()) {
            const Index state = read_state(r, *list, tag, world);
            if (state != kNone) {
                c.states.push_back(state);
            }
        }
        if (const auto share = share_range(r, *node, "anteil")) {
            c.share = *share;
            t.bankruptcies.push_back(std::move(c));
        }
    }
    result.errors.insert(result.errors.end(), r.errors.begin(), r.errors.end());
    return result;
}

bool alive(const World& w, Index state) {
    return !w.state_locations(state).empty();
}

AiLabWar lab_war(const WarResult& r) {
    return AiLabWar{r.attacker, r.defender, r.started, true, r.ended, r.winner};
}

} // namespace

AiTargetsResult parse_ai_targets(std::string_view text, const World& world, const std::string& file_name) {
    return targets_from(parse_script(text, file_name), world, file_name);
}

AiTargetsResult load_ai_targets(const std::filesystem::path& file, const World& world) {
    return targets_from(parse_script_file(file), world, path_to_utf8(file));
}

AiLabRun run_ai_lab(const World& world, const SimConfig& config, const AiTargets& targets) {
    const auto t0 = std::chrono::steady_clock::now();
    Simulation sim(world, config);
    const World& w = sim.world();
    AiLabRun run;
    run.seed = config.seed;
    std::vector<bool> alive_at_start(w.states.size());
    std::vector<std::int32_t> bankruptcies_at_start(w.states.size());
    for (std::size_t s = 0; s < w.states.size(); ++s) {
        alive_at_start[s] = alive(w, static_cast<Index>(s));
        bankruptcies_at_start[s] = w.states[s].bankruptcies;
    }
    const std::int64_t states_alive = std::count(alive_at_start.begin(), alive_at_start.end(), true);
    while (w.date <= targets.until) {
        const Date today = w.date;
        sim.step_day();
        if (today.day != 1) {
            continue;
        }
        // Entscheidungen von heute stehen am Ende des KI-Protokolls (je Staat begrenzt, aber nie unter die
        // Zahl eines Monatsersten).
        for (auto it = w.ai_log.rbegin(); it != w.ai_log.rend() && it->date == today; ++it) {
            run.decisions[static_cast<std::size_t>(it->kind)] += 1;
        }
        if (ai_active(w)) {
            run.ai_state_months += states_alive;
        }
        for (std::size_t s = 0; s < w.unrest.size(); ++s) {
            if (w.unrest[s] > run.unrest_max) {
                run.unrest_max = w.unrest[s];
                run.unrest_state = static_cast<Index>(s);
            }
        }
    }

    if (targets.journal != kNone) {
        const JournalState& js = w.journal_states[at(targets.journal)];
        if (js.stage != kNone && w.journals[at(targets.journal)].stages[at(js.stage)].outcome) {
            run.outcome = js.stage;
            run.outcome_date = js.since;
        }
    }
    for (const WarResult& r : w.war_results) {
        run.wars.push_back(lab_war(r));
    }
    for (const War& war : w.wars) {
        if (!war.attackers.empty() && !war.defenders.empty()) {
            run.wars.push_back(
                AiLabWar{war.attackers.front(), war.defenders.front(), war.started, false, {}, 0});
        }
    }
    for (std::size_t s = 0; s < w.states.size(); ++s) {
        const auto state = static_cast<Index>(s);
        if (alive_at_start[s] && !alive(w, state)) {
            run.fallen.push_back(state);
        }
        if (w.states[s].bankruptcies > bankruptcies_at_start[s]) {
            run.bankrupt.emplace_back(state, w.states[s].bankruptcies - bankruptcies_at_start[s]);
        }
    }
    run.alliances = std::count_if(w.treaties.begin(), w.treaties.end(),
                                  [](const Treaty& t) { return t.kind == TreatyKind::Alliance; });
    run.ai_seconds = sim.ai_seconds();
    run.checksum = sim.checksum();
    run.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return run;
}

std::vector<AiLabRun> run_ai_lab_seeds(const World& world, const AiTargets& targets,
                                       const AiLabOptions& options) {
    const World* base = &world;
    std::optional<World> changed;
    if (!options.ai || options.difficulty != kNone || options.open) {
        changed.emplace(world);
        changed->ai_rules.active = options.ai;
        // Spieleinstellung wie der Befehl am ersten Tag (vertical_slice.md 3.2).
        changed->game_settings.difficulty = options.difficulty;
        changed->game_settings.open = options.open;
        base = &*changed;
    }
    const auto count = static_cast<std::size_t>(std::max<std::int64_t>(options.seeds, 0));
    std::vector<AiLabRun> runs(count);
    const unsigned requested = options.parallel == 0 ? std::thread::hardware_concurrency() : options.parallel;
    const auto workers =
        static_cast<unsigned>(std::clamp<std::size_t>(requested, 1, std::max<std::size_t>(count, 1)));
    const unsigned threads = workers > 1 ? 1 : options.threads;
    std::atomic<std::size_t> next{0};
    const auto work = [&] {
        for (std::size_t i = next++; i < count; i = next++) {
            const std::uint64_t seed = options.seed + i;
            try {
                runs[i] = run_ai_lab(*base, SimConfig{seed, threads}, targets);
            } catch (const std::exception& e) {
                runs[i] = AiLabRun{};
                runs[i].seed = seed;
                runs[i].crashed = true;
                runs[i].error = e.what();
            } catch (...) {
                runs[i] = AiLabRun{};
                runs[i].seed = seed;
                runs[i].crashed = true;
                runs[i].error = "unbekannte Ausnahme";
            }
        }
    };
    std::vector<std::thread> pool;
    for (unsigned k = 1; k < workers; ++k) {
        pool.emplace_back(work);
    }
    work();
    for (std::thread& t : pool) {
        t.join();
    }
    return runs;
}

AiLabSummary summarize_ai_lab(const World& world, const AiTargets& targets,
                              const std::vector<AiLabRun>& runs) {
    AiLabSummary summary;
    const std::size_t stages =
        targets.journal != kNone ? world.journals[at(targets.journal)].stages.size() : 0;
    summary.outcomes.assign(stages, 0);
    summary.wars.assign(targets.wars.size(), Fixed::zero());
    summary.bankruptcies.assign(targets.bankruptcies.size(), Fixed::zero());
    std::vector<std::int64_t> war_runs(targets.wars.size(), 0);
    std::vector<std::int64_t> bankrupt_runs(targets.bankruptcies.size(), 0);
    for (const AiLabRun& run : runs) {
        if (run.crashed) {
            ++summary.crashed;
            continue;
        }
        if (run.outcome == kNone) {
            ++summary.open;
        } else {
            summary.outcomes[at(run.outcome)] += 1;
            if (std::find(targets.unification.begin(), targets.unification.end(), run.outcome) !=
                targets.unification.end()) {
                ++summary.unified;
            }
        }
        for (std::size_t k = 0; k < kAiDecisionKinds; ++k) {
            summary.decisions[k] += run.decisions[k];
        }
        for (std::size_t c = 0; c < targets.wars.size(); ++c) {
            const AiWarCorridor& corridor = targets.wars[c];
            const bool hit = std::any_of(run.wars.begin(), run.wars.end(), [&](const AiLabWar& war) {
                return (war.attacker == corridor.first && war.defender == corridor.second) ||
                       (war.attacker == corridor.second && war.defender == corridor.first);
            });
            war_runs[c] += hit ? 1 : 0;
        }
        for (std::size_t c = 0; c < targets.bankruptcies.size(); ++c) {
            const std::vector<Index>& states = targets.bankruptcies[c].states;
            const bool hit = std::any_of(run.bankrupt.begin(), run.bankrupt.end(), [&](const auto& entry) {
                return std::find(states.begin(), states.end(), entry.first) != states.end();
            });
            bankrupt_runs[c] += hit ? 1 : 0;
        }
    }
    if (summary.crashed > 0) {
        summary.violations.push_back(AiViolation{AiCheck::Crash, 0, Fixed::from_int(summary.crashed)});
    }
    const auto n = static_cast<std::int64_t>(runs.size());
    if (n == 0) {
        return summary;
    }
    if (targets.has_unification) {
        const Fixed share = Fixed::from_ratio(summary.unified, n);
        if (!targets.unification_share.contains(share)) {
            summary.violations.push_back(AiViolation{AiCheck::Unification, 0, share});
        }
    }
    for (std::size_t i = 0; i < targets.outcomes.size(); ++i) {
        const Fixed share = Fixed::from_ratio(summary.outcomes[at(targets.outcomes[i].stage)], n);
        if (!targets.outcomes[i].share.contains(share)) {
            summary.violations.push_back(AiViolation{AiCheck::Outcome, i, share});
        }
    }
    if (targets.has_max_outcome) {
        for (std::size_t s = 0; s < summary.outcomes.size(); ++s) {
            const Fixed share = Fixed::from_ratio(summary.outcomes[s], n);
            if (share > targets.max_outcome) {
                summary.violations.push_back(AiViolation{AiCheck::MaxOutcome, s, share});
            }
        }
    }
    for (std::size_t c = 0; c < targets.wars.size(); ++c) {
        summary.wars[c] = Fixed::from_ratio(war_runs[c], n);
        if (!targets.wars[c].share.contains(summary.wars[c])) {
            summary.violations.push_back(AiViolation{AiCheck::War, c, summary.wars[c]});
        }
    }
    for (std::size_t c = 0; c < targets.bankruptcies.size(); ++c) {
        summary.bankruptcies[c] = Fixed::from_ratio(bankrupt_runs[c], n);
        if (!targets.bankruptcies[c].share.contains(summary.bankruptcies[c])) {
            summary.violations.push_back(AiViolation{AiCheck::Bankruptcy, c, summary.bankruptcies[c]});
        }
    }
    return summary;
}

} // namespace uhrwerk
