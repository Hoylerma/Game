#include "uhrwerk/simulation.hpp"

#include "state_access.hpp"
#include "uhrwerk/customs.hpp"
#include "uhrwerk/diplomacy.hpp"
#include "uhrwerk/economy.hpp"
#include "uhrwerk/politics.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace uhrwerk {

namespace {

constexpr int kStaggerDays = 28;

/// Orte je Paket der parallelen Phase (bevoelkerung.md, Abschnitt 5.2). Fest, damit die Aufteilung
/// nicht von der Zahl der Threads abhängt (das Ergebnis tut es ohnehin nicht).
constexpr std::size_t kChunkLocations = 8;

/// Zielorte je Paket beim Buchen der Wanderung, und die Mindestzahl an Strömen, ab der sich das
/// parallele Buchen lohnt (darunter kostet das Wecken der Threads mehr, als es bringt). Beides
/// beeinflusst nur die Laufzeit, nicht das Ergebnis.
constexpr std::size_t kChunkTargets = 16;
constexpr std::size_t kMinParallelBookings = 2048;

/// Höchstens wanderung_ziele (≤ 16) Ziele plus die nächste Stadt.
constexpr std::size_t kMaxMigrationTargets = 17;

/// Rechentag im Monat (1..28) für Ort bzw. Staat mit diesem Index.
constexpr std::int32_t compute_day(std::size_t index) {
    return static_cast<std::int32_t>(index % kStaggerDays) + 1;
}

template <typename T> bool in_range(const std::vector<T>& items, Index index) {
    return index >= 0 && static_cast<std::size_t>(index) < items.size();
}

std::int64_t to_percent(Fixed rate) {
    return (rate * Fixed::from_int(100)).round();
}

bool same_key(const PopBlock& pop, Index profession, Index confession, Index culture) {
    return pop.profession == profession && pop.confession == confession && pop.culture == culture;
}

} // namespace

Simulation::Simulation(World world, SimConfig config)
    : world_(std::move(world)), config_(config), rates_(derive_rates(world_.rules)),
      rng_demography_(config.seed, RngStream::Demografie), rng_economy_(config.seed, RngStream::Wirtschaft),
      rng_events_(config.seed, RngStream::Ereignisse), topology_(build_economy_topology(world_)),
      pool_(ThreadPool::resolve_threads(config.threads)) {
    build_schedule();
    build_migration_targets();
    prepare_population_state();
    start_checksum_ = checksum();
}

void Simulation::rebuild_derived() {
    topology_ = build_economy_topology(world_);
    build_schedule();
    build_migration_targets();
}

void Simulation::build_schedule() {
    for (std::vector<Index>& day : locations_by_day_) {
        day.clear();
    }
    state_locations_.assign(world_.states.size(), {});
    for (std::size_t i = 0; i < world_.locations.size(); ++i) {
        const Location& loc = world_.locations[i];
        // Orte ohne Blöcke tragen zu Bevölkerung und Wirtschaft nichts bei.
        if (loc.pop_count > 0) {
            locations_by_day_[static_cast<std::size_t>(compute_day(i) - 1)].push_back(static_cast<Index>(i));
        }
        if (in_range(world_.states, loc.owner)) {
            state_locations_[static_cast<std::size_t>(loc.owner)].push_back(static_cast<Index>(i));
        }
    }
}

void Simulation::build_migration_targets() {
    const Rules& rules = world_.rules;
    const std::size_t count = world_.locations.size();
    migration_targets_.assign(count, {});
    const auto max_targets = static_cast<std::size_t>(std::max(0, rules.migration_targets));
    if (max_targets == 0) {
        return;
    }

    std::vector<Index> with_data;
    for (std::size_t i = 0; i < count; ++i) {
        const Location& loc = world_.locations[i];
        if (loc.pop_count > 0 && in_range(world_.states, loc.owner)) {
            with_data.push_back(static_cast<Index>(i));
        }
    }

    // Abstand in Grad, Länge mit cos 51° gestaucht; Vergleich der Quadrate ohne Wurzel, exakt in
    // Ganzzahlen (Rohwerte im Festkomma).
    const Fixed cos_reference = Fixed::from_ratio(6293, 10000);
    const Fixed radius = rules.migration_radius_km / Fixed::from_ratio(1112, 10); // 111,2 km je Grad
    const std::int64_t radius_squared = radius.raw() * radius.raw();
    const auto distance = [&](const Location& a, const Location& b) {
        const std::int64_t dx = ((b.longitude - a.longitude) * cos_reference).raw();
        const std::int64_t dy = (b.latitude - a.latitude).raw();
        return dx * dx + dy * dy;
    };

    using Candidate = std::pair<std::int64_t, Index>; // Abstandsquadrat, Ort
    std::vector<Candidate> near;
    for (const Index i : with_data) {
        const Location& here = world_.locations[static_cast<std::size_t>(i)];
        near.clear();
        Candidate city{0, kNone};
        for (const Index j : with_data) {
            if (j == i) {
                continue;
            }
            const Location& there = world_.locations[static_cast<std::size_t>(j)];
            const Candidate candidate{distance(here, there), j};
            if (candidate.first <= radius_squared) {
                near.push_back(candidate);
            }
            if (!here.is_city && there.is_city && (city.second == kNone || candidate < city)) {
                city = candidate;
            }
        }
        const std::size_t keep = std::min(max_targets, near.size());
        std::partial_sort(near.begin(), near.begin() + static_cast<std::ptrdiff_t>(keep), near.end());
        near.resize(keep);
        if (city.second != kNone && std::find(near.begin(), near.end(), city) == near.end()) {
            near.push_back(city); // die nächste Stadt, auch außerhalb des Umkreises
        }
        std::sort(near.begin(), near.end());
        std::vector<Index>& targets = migration_targets_[static_cast<std::size_t>(i)];
        for (const Candidate& c : near) {
            targets.push_back(c.second);
        }
    }
}

void Simulation::prepare_population_state() {
    const std::size_t count = world_.locations.size();
    const std::size_t professions = world_.professions.size();
    outbox_.assign(count, {});
    inbox_.assign(count, {});
    touched_.clear();
    next_snapshots_.assign(count, {});
    world_.arrivals.resize(count);
    next_targets_.assign(count * professions, Fixed{});
    ensure_economy_state(world_, topology_);
    ensure_politics_state(world_);  // Wahlkampf (seit M8), nur mit Parlamenten
    ensure_diplomacy_state(world_); // Beziehungen (seit M9), nur mit Deutschem Bund

    const bool fits = world_.snapshots.size() == count &&
                      world_.snapshot_targets.size() == count * professions &&
                      world_.balance_current.size() == count && world_.balance_previous.size() == count;
    if (world_.balance_current.size() != count || world_.balance_previous.size() != count) {
        world_.balance_current.assign(count, {});
        world_.balance_previous.assign(count, {});
    }
    if (fits) {
        return; // Fortsetzung einer laufenden Partie oder Start aus dem Lader (eingeschwungen)
    }
    // Welt ohne Startzustand (z. B. von Hand gebaut): Wirtschaft jedes Ortes einmal rechnen, damit
    // Momentaufnahmen und Ziel-Lebensstandards stehen.
    world_.snapshots.assign(count, {});
    world_.snapshot_targets.assign(count * professions, Fixed{});
    LocationFactors factors;
    EconomyScratch scratch;
    for (const std::vector<Index>& list : topology_.market_locations) {
        for (const Index location : list) {
            const auto i = static_cast<std::size_t>(location);
            compute_location_factors(world_, location, factors);
            run_location_economy(world_, topology_, location, factors, scratch);
            world_.snapshots[i] = LocationSnapshot{factors.population, factors.reserve};
            std::copy(factors.target.begin(), factors.target.end(),
                      world_.snapshot_targets.begin() + static_cast<std::ptrdiff_t>(i * professions));
        }
    }
    estimate_budgets(world_);
}

void Simulation::submit(const Command& command) {
    // Ein eingereihter Befehl wird beim nächsten Schritt ausgeführt, also am heutigen days_elapsed.
    command_log_.push_back(LoggedCommand{days_elapsed_, command});
    pending_.push_back(command);
}

Fixed Simulation::reserved_campaign_funds(Index current) const {
    Fixed reserved;
    for (const Command& command : pending_) {
        const auto action = static_cast<std::size_t>(command.action);
        if (command.type == CommandType::Campaign && command.state == current && action < kCampaignActions) {
            reserved += world_.election_rules.campaign[action].cost;
        }
    }
    return reserved;
}

void Simulation::step_day() {
    // 1. Befehle in Eingangsreihenfolge. Geänderte Steuersätze werden je Staat
    //    einmal pro Tag in der Chronik vermerkt (nicht je Befehl).
    if (!pending_.empty()) {
        std::vector<std::array<Fixed, kTaxKinds>> previous_rates;
        previous_rates.reserve(world_.states.size());
        for (const State& state : world_.states) {
            previous_rates.push_back(state.tax_rates);
        }
        while (!pending_.empty()) {
            const Command command = pending_.front();
            pending_.pop_front();
            execute(command);
        }
        constexpr std::array<std::string_view, kTaxKinds> names = {"die Kontribution", "die Akzise",
                                                                   "den Zoll"};
        for (std::size_t i = 0; i < world_.states.size(); ++i) {
            const State& state = world_.states[i];
            for (std::size_t t = 0; t < kTaxKinds; ++t) {
                if (state.tax_rates[t] == previous_rates[i][t]) {
                    continue;
                }
                ChronicleEntry entry;
                entry.date = world_.date;
                entry.text = state.short_name + " legt " + std::string(names[t]) + " auf " +
                             std::to_string(to_percent(state.tax_rates[t])) + " % fest.";
                entry.state = static_cast<Index>(i);
                world_.chronicle.push_back(std::move(entry));
            }
        }
    }

    const std::int32_t day = world_.date.day;
    const bool new_year = world_.date.month == 1 && day == 1;

    // 2. Jahresabschluss und Ernte des neuen Jahres (wirtschaft.md, Abschnitt 4.7).
    if (new_year) {
        world_.balance_previous = world_.balance_current;
        std::fill(world_.balance_current.begin(), world_.balance_current.end(), PopulationBalance{});
        draw_harvest(world_, topology_, rng_economy_);
        // Beantragte Wechsel der Zollunion werden wirksam (seit M9, diplomatie.md Abschnitt 3.5).
        if (!world_.customs_unions.empty()) {
            apply_customs_changes(world_);
        }
    }

    // 3. Neuordnung am Monatsersten (nur wenn es Zugänge oder zu kleine Blöcke gibt), dann die Märkte
    //    (wirtschaft.md, Abschnitt 4.1).
    if (day == 1) {
        reorganize(world_);
        update_markets(world_, topology_, state_spending(world_), record_trade_ ? &trade_flows_ : nullptr);
        // Vereinskassen (seit M9, diplomatie.md Abschnitt 3.4): Zoll der Mitglieder, nach Bevölkerung
        // verteilt.
        if (!world_.customs_unions.empty()) {
            distribute_customs(world_);
        }
        // Spenden an die Strömungen (seit M8, politik.md Abschnitt 9), nur mit Parlamenten.
        if (!world_.parliaments.empty()) {
            collect_donations(world_);
        }
        // Beziehungen (seit M9, diplomatie.md Abschnitte 4.1 und 4.2) und Abstimmung in der
        // Bundesversammlung (Abschnitt 4.4), nur mit Deutschem Bund.
        if (!world_.relations.empty()) {
            update_relations(world_);
            run_diet(world_);
        }
    }

    // 4. und 5. Bevölkerung der Orte mit heutigem Rechentag (Tage 29–31 haben keine): parallel
    //    rechnen, dann nacheinander zusammenführen.
    if (day <= kStaggerDays) {
        const std::vector<Index>& today = locations_by_day_[static_cast<std::size_t>(day - 1)];
        pool_.get().run(today.size(), kChunkLocations, [&](std::size_t begin, std::size_t end) {
            Scratch scratch;
            for (std::size_t k = begin; k < end; ++k) {
                run_population(today[k], scratch);
            }
        });
        // 5. Zusammenführen (Abschnitt 4.7): Momentaufnahmen veröffentlichen, dann die Ströme je
        //    Zielort sammeln – Herkunftsorte aufsteigend, Einträge in Reihenfolge – und buchen.
        //    Verschiedene Zielorte sind unabhängig voneinander und werden parallel gebucht.
        touched_.clear();
        std::size_t bookings = 0;
        for (const Index source : today) {
            publish_snapshot(source);
            const std::vector<Migration>& out = outbox_[static_cast<std::size_t>(source)];
            for (std::size_t e = 0; e < out.size(); ++e) {
                auto& in = inbox_[static_cast<std::size_t>(out[e].target)];
                if (in.empty()) {
                    touched_.push_back(out[e].target);
                }
                in.emplace_back(source, static_cast<std::uint32_t>(e));
            }
            bookings += out.size();
        }
        std::sort(touched_.begin(), touched_.end());
        const std::size_t chunk = bookings < kMinParallelBookings ? touched_.size() : kChunkTargets;
        pool_.get().run(touched_.size(), chunk, [&](std::size_t begin, std::size_t end) {
            for (std::size_t k = begin; k < end; ++k) {
                auto& in = inbox_[static_cast<std::size_t>(touched_[k])];
                for (const auto& [source, entry] : in) {
                    book_migration(outbox_[static_cast<std::size_t>(source)][entry]);
                }
                in.clear();
            }
        });
        for (const Index source : today) {
            outbox_[static_cast<std::size_t>(source)].clear();
        }
    }

    // 6. Wirtschaft der Staaten mit heutigem Rechentag.
    for (std::size_t i = 0; i < world_.states.size(); ++i) {
        if (compute_day(i) == day) {
            run_economy(static_cast<Index>(i));
        }
    }

    // 6b. Wahlen am Wahltag (seit M8, politik.md Abschnitt 7).
    if (!world_.parliaments.empty()) {
        run_elections();
    }

    // 6c. Diplomatische Spiele: Ende einer Stufe, Auflösung (seit M9, diplomatie.md Abschnitt 4.5).
    if (!world_.plays.empty()) {
        run_plays(world_);
    }

    // 7. Jahreswechsel.
    if (new_year) {
        run_yearly();
    }

    // 8. Datum weiterschalten.
    world_.date = next_day(world_.date);
    ++days_elapsed_;
}

void Simulation::step_days(std::int64_t days) {
    for (std::int64_t i = 0; i < days; ++i) {
        step_day();
    }
}

LocationEconomy Simulation::location_economy(Index location) const {
    return in_range(world_.economy, location) ? world_.economy[static_cast<std::size_t>(location)]
                                              : LocationEconomy{};
}

const std::vector<Index>& Simulation::migration_targets(Index location) const {
    static const std::vector<Index> none;
    return in_range(migration_targets_, location) ? migration_targets_[static_cast<std::size_t>(location)]
                                                  : none;
}

std::uint64_t Simulation::checksum() const {
    HashVisitor h;
    StateAccess::visit(*this, h);
    return h.value();
}

std::string Simulation::state_text() const {
    TextVisitor t;
    StateAccess::visit(*this, t);
    return t.text();
}

void Simulation::execute(const Command& command) {
    switch (command.type) {
    case CommandType::SetTaxRate:
        if (!in_range(world_.states, command.state)) {
            return;
        }
        if (command.value < Fixed::zero() || command.value > world_.rules.tax_rate_max ||
            static_cast<std::size_t>(command.kind) >= kTaxKinds) {
            return;
        }
        // Mitglieder einer Zollunion erheben den Vereinstarif (seit M9, diplomatie.md Abschnitt 3.5).
        if (command.kind == TaxKind::Zoll && !world_.customs_unions.empty() &&
            world_.states[static_cast<std::size_t>(command.state)].customs_union != kNone) {
            return;
        }
        world_.states[static_cast<std::size_t>(command.state)]
            .tax_rates[static_cast<std::size_t>(command.kind)] = command.value;
        return;
    case CommandType::Campaign:
        // Ungültige oder zu teure Befehle bleiben ohne Wirkung (wie beim Steuersatz).
        apply_campaign(world_, command.state, command.target, command.action);
        return;
    case CommandType::CustomsUnion:
        // Entscheidung und Chronik in request_customs_change; wirksam zum nächsten 1. Januar.
        request_customs_change(world_, command.state, command.target);
        return;
    case CommandType::Envoy:
        // Diplomatie (seit M9, diplomatie.md 4.2): ungültige Befehle bleiben ohne Wirkung.
        request_envoy(world_, command.state, command.target, command.value != Fixed::zero());
        return;
    case CommandType::Treaty: {
        // Annahme nach Regel und Chronik in request_treaty (Abschnitt 4.3).
        const std::int64_t action = command.value.trunc();
        if (command.value == Fixed::from_int(action) && action >= 0 &&
            action <= static_cast<std::int64_t>(TreatyAction::CancelGuarantee)) {
            request_treaty(world_, command.state, command.target, static_cast<TreatyAction>(action));
        }
        return;
    }
    case CommandType::Motion:
    case CommandType::Demand:
    case CommandType::Manoeuvre: {
        // Bundesversammlung und diplomatische Spiele (seit M9, Abschnitte 4.4 und 4.5): Art im Wert.
        const std::int64_t kind = command.value.trunc();
        if (command.value != Fixed::from_int(kind) || kind < 0 || kind > 3) {
            return;
        }
        if (command.type == CommandType::Motion) {
            file_motion(world_, command.state, static_cast<MotionKind>(kind), command.target);
        } else if (command.type == CommandType::Demand) {
            if (kind <= static_cast<std::int64_t>(DemandKind::BreakAlliance)) {
                start_play(world_, command.state, command.target, static_cast<DemandKind>(kind));
            }
        } else if (kind <= static_cast<std::int64_t>(Manoeuvre::Escalate) && command.target == kNone) {
            apply_manoeuvre(world_, command.state, static_cast<Manoeuvre>(kind));
        }
        return;
    }
    }
}

// Bevölkerung eines Ortes an seinem Rechentag (bevoelkerung.md, Abschnitt 4). Läuft parallel zu
// anderen Orten: schreibt nur Daten dieses Ortes (Blöcke, Bilanz, Steuern, Ausgang, neue
// Momentaufnahme) und liest andere Orte nur über world_.snapshots und world_.snapshot_targets.
void Simulation::run_population(Index location, Scratch& scratch) {
    const auto li = static_cast<std::size_t>(location);
    const Location& loc = world_.locations[li];
    std::vector<Migration>& out = outbox_[li];
    out.clear();
    if (!in_range(world_.states, loc.owner) || !in_range(world_.terrains, loc.terrain)) {
        return;
    }
    const Rules& rules = world_.rules;
    const std::size_t professions = world_.professions.size();
    const Fixed one = Fixed::one();

    // 4.1 Kennzahlen des Ortes, dann seine Wirtschaft (wirtschaft.md, Abschnitte 4.2–4.5); der
    // Ziel-Lebensstandard je Beruf kommt aus dem Realeinkommen. Beides wird die neue Momentaufnahme.
    LocationFactors& factors = scratch.factors;
    compute_location_factors(world_, location, factors);
    run_location_economy(world_, topology_, location, factors, scratch.economy);
    next_snapshots_[li] = LocationSnapshot{factors.population, factors.reserve};
    std::copy(factors.target.begin(), factors.target.end(),
              next_targets_.begin() + static_cast<std::ptrdiff_t>(li * professions));

    scratch.attraction.resize(professions);
    scratch.hunger.resize(professions);
    for (std::size_t b = 0; b < professions; ++b) {
        scratch.attraction[b] = factors.target[b];
        if (world_.professions[b].agrarian) {
            scratch.attraction[b] += rules.migration_reserve * factors.reserve;
        }
        // Hunger (wirtschaft.md, Abschnitt 4.4): fehlt Grundnahrung, steigt die Sterblichkeit –
        // quadratisch, damit kleine Lücken (etwas Salz, kein Fleisch) kaum, echte Not stark wirkt.
        const Fixed gap = one - food_coverage(world_, location, static_cast<Index>(b));
        scratch.hunger[b] = one + rules.mortality_hunger * gap * gap;
    }
    const Fixed fertility_location = rules.fertility * (one + rules.fertility_reserve * factors.reserve);
    const std::vector<Index>& targets = migration_targets_[li];
    const std::size_t target_count = std::min(targets.size(), kMaxMigrationTargets);
    std::array<Fixed, kMaxMigrationTargets> flows{};
    std::array<Index, kMaxMigrationTargets> flow_professions{};

    PopulationBalance& balance = world_.balance_current[li];

    for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
        PopBlock& pop = world_.pops[static_cast<std::size_t>(p)];
        if (!in_range(world_.professions, pop.profession)) {
            continue;
        }
        const auto b = static_cast<std::size_t>(pop.profession);
        const Profession& profession = world_.professions[b];

        // 4.2 Lebensstandard.
        pop.living_standard += (factors.target[b] - pop.living_standard) * rules.living_standard_adjustment;
        const Fixed living = pop.living_standard;

        // 4.3 Geburten, Sterbefälle, Altern – aus dem Stand vor dem Schritt.
        const Fixed deviation = (living - rates_.fifty) / rates_.fifty; // −1 … +1
        const Fixed mortality =
            (one - rules.mortality_poverty * deviation).clamp(Fixed::from_ratio(1, 2), Fixed::from_int(2));
        const Fixed mortality_month = mortality * scratch.hunger[b] / rates_.twelve;
        const Fixed wealth = Fixed::max(Fixed::zero(), one + rules.fertility_wealth * deviation);
        const Fixed fertility_month = fertility_location * wealth / rates_.twelve;

        const Fixed k = pop.children;
        const Fixed j = pop.young;
        const Fixed e = pop.working;
        const Fixed a = pop.old;
        const Fixed births = (j + e * rules.fertile_share_working) * fertility_month;
        const Fixed dead_k = k * rules.mortality_children * mortality_month;
        const Fixed dead_j = j * rules.mortality_young * mortality_month;
        const Fixed dead_e = e * rules.mortality_working * mortality_month;
        const Fixed dead_a = a * rules.mortality_old * mortality_month;
        const Fixed aged_k = k * rates_.aging_children;
        const Fixed aged_j = j * rates_.aging_young;
        const Fixed aged_e = e * rates_.aging_working;
        // Kein Wert wird negativ: Die Abgänge jeder Altersgruppe liegen weit unter 100 % und
        // die Multiplikation rundet ab (bevoelkerung.md, Abschnitt 4.3).
        pop.children = k + births - dead_k - aged_k;
        pop.young = j + aged_k - dead_j - aged_j;
        pop.working = e + aged_j - dead_e - aged_e;
        pop.old = a + aged_e - dead_a;
        balance.births += births;
        balance.deaths += dead_k + dead_j + dead_e + dead_a;

        // 4.4 Bildung.
        pop.literacy += (literacy_target(world_, loc, pop, living) - pop.literacy) * rates_.literacy_month;

        // 4.5 Wanderung und Auswanderung der Jungen.
        const Fixed young = pop.young;
        if (young <= Fixed::zero()) {
            continue;
        }
        Fixed sum;
        for (std::size_t t = 0; t < target_count; ++t) {
            flows[t] = Fixed::zero();
            const Index target = targets[t];
            const Location& there = world_.locations[static_cast<std::size_t>(target)];
            const Index target_profession = there.is_city && profession.city_profession != kNone
                                                ? profession.city_profession
                                                : pop.profession;
            const auto tb = static_cast<std::size_t>(target_profession);
            Fixed attraction = world_.snapshot_targets[static_cast<std::size_t>(target) * professions + tb];
            if (world_.professions[tb].agrarian) {
                attraction +=
                    rules.migration_reserve * world_.snapshots[static_cast<std::size_t>(target)].reserve;
            }
            const Fixed difference = attraction - scratch.attraction[b];
            if (difference <= Fixed::zero()) {
                continue;
            }
            Fixed flow = young * rules.migration_rate * difference / rates_.twelve_hundred;
            if (there.owner != loc.owner) {
                flow = flow * rules.migration_foreign;
            }
            flows[t] = flow;
            flow_professions[t] = target_profession;
            sum += flow;
        }
        Fixed moved;
        if (sum > Fixed::zero()) {
            const Fixed cap = young * rules.migration_max / rates_.twelve;
            for (std::size_t t = 0; t < target_count; ++t) {
                Fixed flow = flows[t];
                if (sum > cap) {
                    flow = flow * cap / sum;
                }
                if (flow <= Fixed::zero() || flow < rules.migration_min_amount) {
                    continue;
                }
                out.push_back(Migration{targets[t], flow_professions[t], pop.confession, pop.culture, flow,
                                        living, pop.literacy});
                moved += flow;
            }
        }
        Fixed emigrated;
        if (living < rules.emigration_threshold) {
            emigrated =
                young * rules.emigration_rate * (rules.emigration_threshold - living) / rates_.twelve_hundred;
        }
        pop.young = young - moved - emigrated;
        balance.moved_out += moved;
        balance.emigrated += emigrated;
    }
}

// Momentaufnahme eines Ortes nach seinem Rechentag veröffentlichen (nacheinander).
void Simulation::publish_snapshot(Index location) {
    const auto li = static_cast<std::size_t>(location);
    const std::size_t professions = world_.professions.size();
    world_.snapshots[li] = next_snapshots_[li];
    const auto first = next_targets_.begin() + static_cast<std::ptrdiff_t>(li * professions);
    std::copy(first, first + static_cast<std::ptrdiff_t>(professions),
              world_.snapshot_targets.begin() + static_cast<std::ptrdiff_t>(li * professions));
}

// Bucht einen Wanderungsstrom im Zielort (Abschnitt 4.7). Läuft parallel zu anderen Zielorten:
// schreibt nur Blöcke, Zugänge und Bilanz des Zielortes.
void Simulation::book_migration(const Migration& migration) {
    PopBlock incoming;
    incoming.location = migration.target;
    incoming.profession = migration.profession;
    incoming.confession = migration.confession;
    incoming.culture = migration.culture;
    incoming.young = migration.amount;
    incoming.living_standard = migration.living_standard;
    incoming.literacy = migration.literacy;
    world_.balance_current[static_cast<std::size_t>(migration.target)].moved_in += migration.amount;

    const Location& there = world_.locations[static_cast<std::size_t>(migration.target)];
    for (Index p = there.first_pop; p < there.first_pop + there.pop_count; ++p) {
        PopBlock& pop = world_.pops[static_cast<std::size_t>(p)];
        if (same_key(pop, migration.profession, migration.confession, migration.culture)) {
            absorb(pop, incoming);
            return;
        }
    }
    if (migration.amount < world_.rules.block_min_size) {
        const Index similar = most_similar_block(world_.pops, {}, there.first_pop, there.pop_count, incoming);
        if (similar != kNone) {
            absorb(world_.pops[static_cast<std::size_t>(similar)], incoming);
            return;
        }
    }
    // Zugang: wird bei der nächsten Neuordnung ein eigener Block.
    std::vector<PopBlock>& arrivals = world_.arrivals[static_cast<std::size_t>(migration.target)];
    for (PopBlock& arrival : arrivals) {
        if (same_key(arrival, migration.profession, migration.confession, migration.culture)) {
            absorb(arrival, incoming);
            return;
        }
    }
    arrivals.push_back(incoming);
}

// Haushalt eines Staates an seinem Rechentag (wirtschaft.md, Abschnitt 4.6).
void Simulation::run_economy(Index state) {
    if (run_state_budget(world_, state, state_locations_[static_cast<std::size_t>(state)])) {
        const State& s = world_.states[static_cast<std::size_t>(state)];
        ChronicleEntry entry;
        entry.date = world_.date;
        entry.text = s.short_name + " erklärt den Staatsbankrott: Die Gläubiger verlieren " +
                     std::to_string(to_percent(world_.rules.bankruptcy_haircut)) + " % ihrer Forderungen.";
        entry.state = state;
        world_.chronicle.push_back(std::move(entry));
    }
}

// Wahlen der Parlamente mit heutigem Wahltag (politik.md, Abschnitt 7.7): Ergebnis und Chronik.
void Simulation::run_elections() {
    for (std::size_t i = 0; i < world_.parliaments.size(); ++i) {
        const Parliament& parliament = world_.parliaments[i];
        if (parliament.election_day != world_.date) {
            continue;
        }
        const auto index = static_cast<Index>(i);
        const bool held =
            std::any_of(world_.elections.begin(), world_.elections.end(), [&](const ElectionResult& e) {
                return e.parliament == index && e.day == world_.date;
            });
        if (held) {
            continue;
        }
        world_.elections.push_back(run_election(world_, index, config_.seed, &pool_.get()));
        end_campaign(world_, index);
        const ElectionResult& election = world_.elections.back();
        const std::vector<std::int32_t> seats = seats_by_current(world_, election);
        std::size_t strongest = 0;
        for (std::size_t c = 1; c < seats.size(); ++c) {
            if (seats[c] > seats[strongest]) {
                strongest = c;
            }
        }
        std::int32_t empty = 0;
        for (const DistrictResult& d : election.districts) {
            if (d.boycott) {
                empty += world_.electoral_districts[static_cast<std::size_t>(d.district)].seats;
            }
        }
        ChronicleEntry entry;
        entry.date = world_.date;
        entry.text = "Wahltag – " + parliament.name + ": " + std::to_string(election.deputies.size()) +
                     " von " + std::to_string(parliament.seats) + " Sitzen besetzt";
        if (empty > 0) {
            entry.text += ", " + std::to_string(empty) + " leer (Boykott)";
        }
        if (!seats.empty() && seats[strongest] > 0) {
            entry.text += "; stärkste Strömung: " + world_.currents[strongest].name + " mit " +
                          std::to_string(seats[strongest]) + " Sitzen";
        }
        entry.text += ".";
        entry.location = parliament.seat;
        entry.state = parliament.state;
        world_.chronicle.push_back(std::move(entry));
    }
}

void Simulation::run_yearly() {
    const Rules& rules = world_.rules;
    for (std::size_t i = 0; i < world_.locations.size(); ++i) {
        Location& loc = world_.locations[i];
        // Orte ohne Bevölkerungsblöcke (Orte ohne Daten) werden übersprungen: keine Zufallszahlen,
        // kein Abbau der Verwüstung (chronik-skript.md, Abschnitt 5). Blöcke mit Größe 0 zählen mit.
        if (loc.pop_count == 0) {
            continue;
        }

        // 1. Seuche: jede Altersgruppe verliert denselben Anteil.
        if (rng_events_.chance(rules.plague_chance)) {
            const Fixed loss = rng_events_.uniform(rules.plague_loss_min, rules.plague_loss_max);
            Fixed dead;
            const auto shrink = [&](Fixed& group) {
                const Fixed lost = group * loss;
                group -= lost;
                dead += lost;
            };
            for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
                PopBlock& pop = world_.pops[static_cast<std::size_t>(p)];
                shrink(pop.children);
                shrink(pop.young);
                shrink(pop.working);
                shrink(pop.old);
            }
            world_.balance_current[i].plague_deaths += dead;
            ChronicleEntry entry;
            entry.date = world_.date;
            entry.text = "Seuche in " + loc.name + ": " + std::to_string(to_percent(loss)) +
                         " % der Bevölkerung sterben.";
            entry.location = static_cast<Index>(i);
            entry.state = loc.owner;
            world_.chronicle.push_back(std::move(entry));
        }

        // 2. Abbau der Verwüstung
        loc.devastation -= loc.devastation * rules.devastation_decay;
    }
}

} // namespace uhrwerk
