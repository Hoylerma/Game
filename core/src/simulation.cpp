#include "uhrwerk/simulation.hpp"

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

/// Produktion und Steuern eines Ortes je Monat aus seinen Kennzahlen (Abschnitt 4.6). Dieselbe
/// Rechnung wie in run_population, dort in der Blockschleife.
LocationEconomy economy_of(const World& world, const Location& loc, const LocationFactors& factors,
                           Fixed tax_rate) {
    LocationEconomy result;
    const Fixed twelve = Fixed::from_int(12);
    for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
        const PopBlock& pop = world.pops[static_cast<std::size_t>(p)];
        if (!in_range(world.professions, pop.profession)) {
            continue;
        }
        const auto b = static_cast<std::size_t>(pop.profession);
        const Fixed produced = pop.size() * (factors.yield[b] / twelve);
        result.production += produced;
        if (world.professions[b].taxable) {
            result.taxes += produced * tax_rate;
        }
    }
    return result;
}

/// FNV-1a, 64 Bit. Ganzzahlen werden bytegenau in Little-Endian-Reihenfolge
/// eingespeist, damit die Prüfsumme plattformunabhängig ist.
class Fnv1a {
public:
    void byte(std::uint8_t b) {
        hash_ ^= b;
        hash_ *= 0x100000001b3ULL;
    }
    void u64(std::uint64_t v) {
        for (int i = 0; i < 8; ++i) {
            byte(static_cast<std::uint8_t>(v >> (8 * i)));
        }
    }
    void i64(std::int64_t v) { u64(static_cast<std::uint64_t>(v)); }
    void fixed(Fixed v) { i64(v.raw()); }
    void date(const Date& d) {
        i64(d.year);
        i64(d.month);
        i64(d.day);
    }
    void text(std::string_view s) {
        u64(s.size());
        for (const char c : s) {
            byte(static_cast<std::uint8_t>(c));
        }
    }
    void rng(const Pcg32& r) {
        u64(r.state());
        u64(r.increment());
    }
    void pop(const PopBlock& p) {
        i64(p.location);
        i64(p.profession);
        i64(p.confession);
        i64(p.culture);
        fixed(p.children);
        fixed(p.young);
        fixed(p.working);
        fixed(p.old);
        fixed(p.living_standard);
        fixed(p.literacy);
    }
    void balance(const PopulationBalance& b) {
        fixed(b.births);
        fixed(b.deaths);
        fixed(b.plague_deaths);
        fixed(b.moved_in);
        fixed(b.moved_out);
        fixed(b.emigrated);
    }
    std::uint64_t value() const { return hash_; }

private:
    std::uint64_t hash_ = 0xcbf29ce484222325ULL;
};

} // namespace

Simulation::Simulation(World world, SimConfig config)
    : world_(std::move(world)), config_(config), rates_(derive_rates(world_.rules)),
      rng_demography_(config.seed, RngStream::Demografie), rng_economy_(config.seed, RngStream::Wirtschaft),
      rng_events_(config.seed, RngStream::Ereignisse), pool_(ThreadPool::resolve_threads(config.threads)) {
    build_schedule();
    build_migration_targets();
    prepare_population_state();
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

    const bool fits = world_.snapshots.size() == count &&
                      world_.snapshot_targets.size() == count * professions &&
                      world_.balance_current.size() == count && world_.balance_previous.size() == count &&
                      world_.economy.size() == count;
    if (fits) {
        return; // Fortsetzung einer laufenden Partie
    }
    world_.snapshots.assign(count, {});
    world_.snapshot_targets.assign(count * professions, Fixed{});
    world_.balance_current.assign(count, {});
    world_.balance_previous.assign(count, {});
    world_.economy.assign(count, {});

    LocationFactors factors;
    for (std::size_t i = 0; i < count; ++i) {
        const Location& loc = world_.locations[i];
        if (loc.pop_count == 0 || !in_range(world_.states, loc.owner) ||
            !in_range(world_.terrains, loc.terrain)) {
            continue;
        }
        const Fixed tax_rate =
            world_.states[static_cast<std::size_t>(loc.owner)].tax_rate(TaxKind::Kontribution);
        compute_location_factors(world_, loc, tax_rate, factors);
        world_.snapshots[i] = LocationSnapshot{factors.population, factors.reserve};
        std::copy(factors.target.begin(), factors.target.end(),
                  world_.snapshot_targets.begin() + static_cast<std::ptrdiff_t>(i * professions));
        world_.economy[i] = economy_of(world_, loc, factors, tax_rate);
    }
}

void Simulation::submit(const Command& command) {
    pending_.push_back(command);
}

void Simulation::step_day() {
    // 1. Befehle in Eingangsreihenfolge. Geänderte Steuersätze werden je Staat
    //    einmal pro Tag in der Chronik vermerkt (nicht je Befehl).
    if (!pending_.empty()) {
        std::vector<Fixed> previous_rates;
        previous_rates.reserve(world_.states.size());
        for (const State& state : world_.states) {
            previous_rates.push_back(state.tax_rate(TaxKind::Kontribution));
        }
        while (!pending_.empty()) {
            const Command command = pending_.front();
            pending_.pop_front();
            execute(command);
        }
        for (std::size_t i = 0; i < world_.states.size(); ++i) {
            const State& state = world_.states[i];
            if (state.tax_rate(TaxKind::Kontribution) != previous_rates[i]) {
                ChronicleEntry entry;
                entry.date = world_.date;
                entry.text = state.short_name + " legt den Steuersatz auf " +
                             std::to_string(to_percent(state.tax_rate(TaxKind::Kontribution))) + " % fest.";
                entry.state = static_cast<Index>(i);
                world_.chronicle.push_back(std::move(entry));
            }
        }
    }

    const std::int32_t day = world_.date.day;
    const bool new_year = world_.date.month == 1 && day == 1;

    // 2. Jahresabschluss.
    if (new_year) {
        world_.balance_previous = world_.balance_current;
        std::fill(world_.balance_current.begin(), world_.balance_current.end(), PopulationBalance{});
    }

    // 3. Neuordnung am Monatsersten (nur wenn es Zugänge oder zu kleine Blöcke gibt).
    if (day == 1) {
        reorganize(world_);
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
    Fnv1a h;
    h.date(world_.date);
    h.i64(days_elapsed_);
    h.u64(config_.seed);

    h.u64(world_.states.size());
    for (const State& s : world_.states) {
        h.fixed(s.treasury);
        h.fixed(s.tax_rate(TaxKind::Kontribution));
        h.fixed(s.court_expenses);
        h.fixed(s.last_month_income);
        h.fixed(s.last_month_expenses);
        h.i64(s.capital);
    }

    h.u64(world_.locations.size());
    for (const Location& l : world_.locations) {
        h.i64(l.owner);
        h.fixed(l.devastation);
        h.i64(l.first_pop);
        h.i64(l.pop_count);
    }

    h.u64(world_.pops.size());
    for (const PopBlock& p : world_.pops) {
        h.pop(p);
    }
    h.u64(world_.arrivals.size());
    for (const std::vector<PopBlock>& list : world_.arrivals) {
        h.u64(list.size());
        for (const PopBlock& p : list) {
            h.pop(p);
        }
    }

    h.u64(world_.snapshots.size());
    for (const LocationSnapshot& s : world_.snapshots) {
        h.fixed(s.population);
        h.fixed(s.reserve);
    }
    h.u64(world_.snapshot_targets.size());
    for (const Fixed target : world_.snapshot_targets) {
        h.fixed(target);
    }
    h.u64(world_.balance_current.size());
    for (const PopulationBalance& b : world_.balance_current) {
        h.balance(b);
    }
    h.u64(world_.balance_previous.size());
    for (const PopulationBalance& b : world_.balance_previous) {
        h.balance(b);
    }
    h.u64(world_.economy.size());
    for (const LocationEconomy& e : world_.economy) {
        h.fixed(e.production);
        h.fixed(e.taxes);
    }

    h.rng(rng_demography_);
    h.rng(rng_economy_);
    h.rng(rng_events_);

    h.u64(world_.chronicle.size());
    for (const ChronicleEntry& e : world_.chronicle) {
        h.date(e.date);
        h.text(e.text);
        h.i64(e.location);
        h.i64(e.state);
    }

    h.u64(pending_.size());
    for (const Command& c : pending_) {
        h.u64(static_cast<std::uint64_t>(c.type));
        h.i64(c.state);
        h.fixed(c.value);
    }
    return h.value();
}

void Simulation::execute(const Command& command) {
    switch (command.type) {
    case CommandType::SetTaxRate:
        if (!in_range(world_.states, command.state)) {
            return;
        }
        if (command.value < Fixed::zero() || command.value > world_.rules.tax_rate_max) {
            return;
        }
        world_.states[static_cast<std::size_t>(command.state)]
            .tax_rates[static_cast<std::size_t>(command.kind)] = command.value;
        return;
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
    const State& owner = world_.states[static_cast<std::size_t>(loc.owner)];
    const std::size_t professions = world_.professions.size();
    const Fixed one = Fixed::one();

    // 4.1 Kennzahlen des Ortes; sie werden auch die neue Momentaufnahme.
    LocationFactors& factors = scratch.factors;
    compute_location_factors(world_, loc, owner.tax_rate(TaxKind::Kontribution), factors);
    next_snapshots_[li] = LocationSnapshot{factors.population, factors.reserve};
    std::copy(factors.target.begin(), factors.target.end(),
              next_targets_.begin() + static_cast<std::ptrdiff_t>(li * professions));

    scratch.yield_month.resize(professions);
    scratch.attraction.resize(professions);
    for (std::size_t b = 0; b < professions; ++b) {
        scratch.yield_month[b] = factors.yield[b] / rates_.twelve;
        scratch.attraction[b] = factors.target[b];
        if (world_.professions[b].agrarian) {
            scratch.attraction[b] += rules.migration_reserve * factors.reserve;
        }
    }
    const Fixed fertility_location = rules.fertility * (one + rules.fertility_reserve * factors.reserve);
    const std::vector<Index>& targets = migration_targets_[li];
    const std::size_t target_count = std::min(targets.size(), kMaxMigrationTargets);
    std::array<Fixed, kMaxMigrationTargets> flows{};
    std::array<Index, kMaxMigrationTargets> flow_professions{};

    PopulationBalance& balance = world_.balance_current[li];
    LocationEconomy economy;

    for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
        PopBlock& pop = world_.pops[static_cast<std::size_t>(p)];
        if (!in_range(world_.professions, pop.profession)) {
            continue;
        }
        const auto b = static_cast<std::size_t>(pop.profession);
        const Profession& profession = world_.professions[b];

        // 4.6 Produktion und Steuern mit der Größe vor diesem Schritt.
        const Fixed produced = pop.size() * scratch.yield_month[b];
        economy.production += produced;
        if (profession.taxable) {
            economy.taxes += produced * owner.tax_rate(TaxKind::Kontribution);
        }

        // 4.2 Lebensstandard.
        pop.living_standard += (factors.target[b] - pop.living_standard) * rules.living_standard_adjustment;
        const Fixed living = pop.living_standard;

        // 4.3 Geburten, Sterbefälle, Altern – aus dem Stand vor dem Schritt.
        const Fixed deviation = (living - rates_.fifty) / rates_.fifty; // −1 … +1
        const Fixed mortality =
            (one - rules.mortality_poverty * deviation).clamp(Fixed::from_ratio(1, 2), Fixed::from_int(2));
        const Fixed mortality_month = mortality / rates_.twelve;
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
    world_.economy[li] = economy;
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

void Simulation::run_economy(Index state) {
    State& s = world_.states[static_cast<std::size_t>(state)];
    Fixed income;
    for (const Index location : state_locations_[static_cast<std::size_t>(state)]) {
        income += world_.economy[static_cast<std::size_t>(location)].taxes;
    }
    const Fixed expenses = s.court_expenses / Fixed::from_int(12);
    s.treasury += income - expenses;
    s.last_month_income = income;
    s.last_month_expenses = expenses;
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
