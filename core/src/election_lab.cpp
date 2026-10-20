// Wahllabor (seit M8, docs/technik/politik.md, Abschnitt 7.8).

#include "uhrwerk/election_lab.hpp"

#include "target_reader.hpp"
#include "uhrwerk/politics.hpp"
#include "uhrwerk/script.hpp"

#include <algorithm>

namespace uhrwerk {

namespace {

std::size_t at(Index index) {
    return static_cast<std::size_t>(index);
}

template <typename T> bool in_range(const std::vector<T>& items, Index index) {
    return index >= 0 && static_cast<std::size_t>(index) < items.size();
}

/// Quantile über ganze Zahlen.
Quantiles int_quantiles(const std::vector<std::int32_t>& values) {
    std::vector<Fixed> fixed;
    fixed.reserve(values.size());
    for (const std::int32_t v : values) {
        fixed.push_back(Fixed::from_int(v));
    }
    return quantiles(fixed);
}

ElectionTargetsResult targets_from(const ParseResult& parsed, const World& world,
                                   const std::string& file_name) {
    ElectionTargetsResult result;
    for (const ParseError& e : parsed.errors) {
        result.errors.push_back(e.to_string());
    }
    if (!parsed.ok()) {
        return result;
    }
    TargetReader r{file_name, {}};
    r.only(parsed.root, {"wahl"});
    for (const ScriptNode* w : parsed.root.find_all("wahl")) {
        const Index parliament = world.find_parliament(w->value);
        if (!w->is_block || w->value.empty()) {
            r.error(*w, "'wahl' erwartet: wahl <parlament> { … }");
            continue;
        }
        if (parliament == kNone) {
            r.error(*w, "unbekanntes Parlament '" + w->value +
                            "' (nicht im Szenario oder nicht in "
                            "common/parlamente.chron)");
            continue;
        }
        if (std::any_of(result.targets.begin(), result.targets.end(),
                        [&](const ElectionTargets& t) { return t.parliament == parliament; })) {
            r.error(*w, "Zielwerte für parlament " + w->value + " sind mehrfach angegeben");
            continue;
        }
        const Parliament& p = world.parliaments[at(parliament)];
        r.only(*w, {"besetzt", "beteiligung", "block"});
        ElectionTargets t;
        t.parliament = parliament;
        if (w->find("besetzt") != nullptr) {
            t.seated = r.range(*w, "besetzt");
        }
        if (w->find("beteiligung") != nullptr) {
            t.turnout = r.range(*w, "beteiligung");
        }
        for (const ScriptNode* b : w->find_all("block")) {
            if (!b->is_block || b->value.empty()) {
                r.error(*b, "'block' erwartet: block <kennung> { fraktionen = { … } anteil = { von bis } }");
                continue;
            }
            r.only(*b, {"fraktionen", "stroemungen", "anteil"});
            ElectionBlockTarget block;
            block.id = b->value;
            const ScriptNode* factions = b->find("fraktionen");
            const ScriptNode* currents = b->find("stroemungen");
            if ((factions == nullptr) == (currents == nullptr)) {
                r.error(*b, "block " + b->value + " braucht genau eines von 'fraktionen' und 'stroemungen'");
                continue;
            }
            const ScriptNode& list = factions != nullptr ? *factions : *currents;
            if (!list.is_block || list.bare_values().empty()) {
                r.error(list, "'" + list.key + "' erwartet eine Liste { … }");
                continue;
            }
            for (const std::string_view id : list.bare_values()) {
                if (factions != nullptr) {
                    if (id == "fraktionslos") {
                        block.factions.push_back(kNone);
                        continue;
                    }
                    const auto it = std::find_if(p.factions.begin(), p.factions.end(),
                                                 [&](const Faction& f) { return f.id == id; });
                    if (it == p.factions.end()) {
                        r.error(list, "unbekannte Fraktion '" + std::string(id) + "' in parlament " + p.id);
                        continue;
                    }
                    block.factions.push_back(static_cast<Index>(it - p.factions.begin()));
                } else {
                    const Index current = world.find_current(id);
                    if (current == kNone) {
                        r.error(list, "unbekannte Strömung '" + std::string(id) + "'");
                        continue;
                    }
                    block.currents.push_back(current);
                }
            }
            if (const std::optional<ValueRange> share = r.range(*b, "anteil")) {
                if (share->min < Fixed::zero() || share->max > Fixed::one()) {
                    r.error(*b->find("anteil"), "'anteil' erwartet einen Bereich innerhalb von 0 bis 1");
                }
                block.share = *share;
            }
            t.blocks.push_back(std::move(block));
        }
        result.targets.push_back(std::move(t));
    }
    result.errors.insert(result.errors.end(), r.errors.begin(), r.errors.end());
    if (!result.errors.empty()) {
        result.targets.clear();
    }
    return result;
}

/// Sitze eines Blocks in einem Lauf.
std::int32_t block_seats(const ElectionBlockTarget& block, const ElectionRun& run) {
    std::int32_t seats = 0;
    for (const Index f : block.factions) {
        seats += f == kNone ? run.by_faction.back() : run.by_faction[at(f)];
    }
    for (const Index c : block.currents) {
        seats += run.by_current[at(c)];
    }
    return seats;
}

} // namespace

const ElectionTargets* ElectionTargetsResult::find(Index parliament) const {
    for (const ElectionTargets& t : targets) {
        if (t.parliament == parliament) {
            return &t;
        }
    }
    return nullptr;
}

ElectionTargetsResult parse_election_targets(std::string_view text, const World& world,
                                             const std::string& file_name) {
    return targets_from(parse_script(text, file_name), world, file_name);
}

ElectionTargetsResult load_election_targets(const std::filesystem::path& file, const World& world) {
    return targets_from(parse_script_file(file), world, path_to_utf8(file));
}

std::optional<ElectionResult> run_to_election(Simulation& sim, Index parliament) {
    const auto held = [&]() -> const ElectionResult* {
        for (const ElectionResult& e : sim.world().elections) {
            if (e.parliament == parliament) {
                return &e;
            }
        }
        return nullptr;
    };
    if (!in_range(sim.world().parliaments, parliament)) {
        return std::nullopt;
    }
    const Date day = sim.world().parliaments[at(parliament)].election_day;
    while (held() == nullptr && sim.world().date <= day) {
        sim.step_day();
    }
    if (const ElectionResult* e = held()) {
        return *e;
    }
    return std::nullopt;
}

ElectionRun measure_election(const World& world, const ElectionResult& election) {
    ElectionRun run;
    run.by_current = seats_by_current(world, election);
    run.by_faction = seats_by_faction(world, election);
    run.seated = static_cast<std::int32_t>(election.deputies.size());
    const std::size_t states = world.states.size();
    run.state_electorate.assign(states, Fixed{});
    run.state_voters.assign(states, Fixed{});
    run.state_seats.assign(states, 0);
    run.state_by_current.assign(states, std::vector<std::int32_t>(world.currents.size(), 0));
    std::vector<Fixed> turnouts;
    for (const DistrictResult& d : election.districts) {
        const ElectoralDistrict& district = world.electoral_districts[at(d.district)];
        const std::size_t s = at(district.state);
        run.state_electorate[s] += d.electorate;
        run.state_voters[s] += d.voters;
        run.state_seats[s] += district.seats;
        if (!d.boycott && d.electorate > Fixed::zero()) {
            turnouts.push_back(d.voters / d.electorate);
        }
    }
    for (const Deputy& deputy : election.deputies) {
        const std::size_t s = at(world.electoral_districts[at(deputy.district)].state);
        ++run.state_by_current[s][at(deputy.current)];
    }
    run.turnout = quantile(turnouts, 1, 2);
    return run;
}

ElectionSummary summarize_elections(const World& world, Index parliament,
                                    const std::vector<ElectionRun>& runs, const ElectionTargets* targets) {
    ElectionSummary summary;
    summary.runs = static_cast<std::int64_t>(runs.size());
    if (runs.empty() || !in_range(world.parliaments, parliament)) {
        return summary;
    }
    const auto over_runs = [&](auto value) {
        std::vector<std::int32_t> values;
        values.reserve(runs.size());
        for (const ElectionRun& run : runs) {
            values.push_back(value(run));
        }
        return int_quantiles(values);
    };
    const auto fixed_over_runs = [&](auto value) {
        std::vector<Fixed> values;
        values.reserve(runs.size());
        for (const ElectionRun& run : runs) {
            values.push_back(value(run));
        }
        return quantiles(values);
    };
    summary.seated = over_runs([](const ElectionRun& r) { return r.seated; });
    summary.turnout = fixed_over_runs([](const ElectionRun& r) { return r.turnout; });
    for (std::size_t c = 0; c < world.currents.size(); ++c) {
        summary.by_current.push_back(over_runs([&](const ElectionRun& r) { return r.by_current[c]; }));
    }
    for (std::size_t f = 0; f < runs.front().by_faction.size(); ++f) {
        summary.by_faction.push_back(over_runs([&](const ElectionRun& r) { return r.by_faction[f]; }));
    }
    summary.state_seats = runs.front().state_seats;
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        summary.state_electorate.push_back(
            fixed_over_runs([&](const ElectionRun& r) { return r.state_electorate[s]; }));
        summary.state_turnout.push_back(fixed_over_runs([&](const ElectionRun& r) {
            return r.state_electorate[s] > Fixed::zero() ? r.state_voters[s] / r.state_electorate[s]
                                                         : Fixed{};
        }));
        std::vector<Quantiles> by_current;
        for (std::size_t c = 0; c < world.currents.size(); ++c) {
            by_current.push_back(over_runs([&](const ElectionRun& r) { return r.state_by_current[s][c]; }));
        }
        summary.state_by_current.push_back(std::move(by_current));
    }
    if (targets == nullptr) {
        return summary;
    }
    for (const ElectionBlockTarget& block : targets->blocks) {
        summary.block_share.push_back(fixed_over_runs([&](const ElectionRun& r) {
            return r.seated > 0 ? Fixed::from_int(block_seats(block, r)) / Fixed::from_int(r.seated)
                                : Fixed{};
        }));
    }
    if (targets->seated && !targets->seated->contains(summary.seated.median)) {
        summary.violations.push_back({ElectionCheck::Seated, 0, summary.seated.median});
    }
    if (targets->turnout && !targets->turnout->contains(summary.turnout.median)) {
        summary.violations.push_back({ElectionCheck::Turnout, 0, summary.turnout.median});
    }
    for (std::size_t b = 0; b < targets->blocks.size(); ++b) {
        if (!targets->blocks[b].share.contains(summary.block_share[b].median)) {
            summary.violations.push_back({ElectionCheck::Block, b, summary.block_share[b].median});
        }
    }
    return summary;
}

} // namespace uhrwerk
