#include "uhrwerk/simulation.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace uhrwerk {

namespace {

constexpr int kStaggerDays = 28;

/// Rechentag im Monat (1..28) für Ort bzw. Staat mit diesem Index.
constexpr std::int32_t compute_day(std::size_t index) {
    return static_cast<std::int32_t>(index % kStaggerDays) + 1;
}

template <typename T> bool in_range(const std::vector<T>& items, Index index) {
    return index >= 0 && static_cast<std::size_t>(index) < items.size();
}

/// Produktion eines Bevölkerungsblocks pro Monat (Formel aus chronik-skript.md, Abschnitt 5).
Fixed block_production(const World& world, const Location& location, const PopBlock& pop) {
    if (!in_range(world.professions, pop.profession) || !in_range(world.terrains, location.terrain)) {
        return Fixed::zero();
    }
    const Profession& profession = world.professions[static_cast<std::size_t>(pop.profession)];
    const TerrainType& terrain = world.terrains[static_cast<std::size_t>(location.terrain)];
    const Fixed condition = Fixed::one() - location.devastation * world.rules.devastation_yield_loss;
    return pop.size * profession.yield_per_capita * terrain.yield_factor * condition / Fixed::from_int(12);
}

bool is_taxable(const World& world, const PopBlock& pop) {
    return in_range(world.professions, pop.profession) &&
           world.professions[static_cast<std::size_t>(pop.profession)].taxable;
}

std::int64_t to_percent(Fixed rate) {
    return (rate * Fixed::from_int(100)).round();
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
    std::uint64_t value() const { return hash_; }

private:
    std::uint64_t hash_ = 0xcbf29ce484222325ULL;
};

} // namespace

Simulation::Simulation(World world, SimConfig config)
    : world_(std::move(world)), config_(config), rng_demography_(config.seed, RngStream::Demografie),
      rng_economy_(config.seed, RngStream::Wirtschaft), rng_events_(config.seed, RngStream::Ereignisse) {}

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
            previous_rates.push_back(state.tax_rate);
        }
        while (!pending_.empty()) {
            const Command command = pending_.front();
            pending_.pop_front();
            execute(command);
        }
        for (std::size_t i = 0; i < world_.states.size(); ++i) {
            const State& state = world_.states[i];
            if (state.tax_rate != previous_rates[i]) {
                ChronicleEntry entry;
                entry.date = world_.date;
                entry.text = state.short_name + " legt den Steuersatz auf " +
                             std::to_string(to_percent(state.tax_rate)) + " % fest.";
                entry.state = static_cast<Index>(i);
                world_.chronicle.push_back(std::move(entry));
            }
        }
    }

    const std::int32_t day = world_.date.day;

    // 2. Demografie der Orte mit heutigem Rechentag.
    for (std::size_t i = 0; i < world_.locations.size(); ++i) {
        if (compute_day(i) == day) {
            run_demography(static_cast<Index>(i));
        }
    }

    // 3. Wirtschaft der Staaten mit heutigem Rechentag.
    for (std::size_t i = 0; i < world_.states.size(); ++i) {
        if (compute_day(i) == day) {
            run_economy(static_cast<Index>(i));
        }
    }

    // 4. Jahreswechsel.
    if (world_.date.month == 1 && world_.date.day == 1) {
        run_yearly();
    }

    // 5. Datum weiterschalten.
    world_.date = next_day(world_.date);
    ++days_elapsed_;
}

void Simulation::step_days(std::int64_t days) {
    for (std::int64_t i = 0; i < days; ++i) {
        step_day();
    }
}

LocationEconomy Simulation::location_economy(Index location) const {
    LocationEconomy result;
    if (!in_range(world_.locations, location)) {
        return result;
    }
    const Location& loc = world_.locations[static_cast<std::size_t>(location)];
    const Fixed tax_rate = in_range(world_.states, loc.owner)
                               ? world_.states[static_cast<std::size_t>(loc.owner)].tax_rate
                               : Fixed{};
    for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
        if (!in_range(world_.pops, p)) {
            continue;
        }
        const PopBlock& pop = world_.pops[static_cast<std::size_t>(p)];
        const Fixed production = block_production(world_, loc, pop);
        result.production += production;
        if (is_taxable(world_, pop)) {
            result.taxes += production * tax_rate;
        }
    }
    return result;
}

std::uint64_t Simulation::checksum() const {
    Fnv1a h;
    h.date(world_.date);
    h.i64(days_elapsed_);
    h.u64(config_.seed);

    h.u64(world_.states.size());
    for (const State& s : world_.states) {
        h.fixed(s.treasury);
        h.fixed(s.tax_rate);
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
        h.i64(p.location);
        h.i64(p.profession);
        h.i64(p.confession);
        h.fixed(p.size);
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
        world_.states[static_cast<std::size_t>(command.state)].tax_rate = command.value;
        return;
    }
}

void Simulation::run_demography(Index location) {
    Location& loc = world_.locations[static_cast<std::size_t>(location)];
    if (!in_range(world_.terrains, loc.terrain) || !in_range(world_.states, loc.owner)) {
        return;
    }
    const Rules& rules = world_.rules;
    const TerrainType& terrain = world_.terrains[static_cast<std::size_t>(loc.terrain)];
    const State& owner = world_.states[static_cast<std::size_t>(loc.owner)];

    const Fixed rate =
        (rules.growth_base + rules.growth_reconstruction * loc.devastation) * terrain.growth_factor -
        rules.growth_tax_pressure * owner.tax_rate;
    const Fixed twelve = Fixed::from_int(12);
    for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
        PopBlock& pop = world_.pops[static_cast<std::size_t>(p)];
        Fixed size = pop.size + pop.size * rate / twelve;
        if (size < Fixed::zero()) {
            size = Fixed::zero();
        }
        pop.size = size;
    }
}

void Simulation::run_economy(Index state) {
    State& s = world_.states[static_cast<std::size_t>(state)];
    Fixed income;
    for (const Index location : world_.state_locations(state)) {
        const Location& loc = world_.locations[static_cast<std::size_t>(location)];
        for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
            const PopBlock& pop = world_.pops[static_cast<std::size_t>(p)];
            if (is_taxable(world_, pop)) {
                income += block_production(world_, loc, pop) * s.tax_rate;
            }
        }
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

        // 1. Seuche
        if (rng_events_.chance(rules.plague_chance)) {
            const Fixed loss = rng_events_.uniform(rules.plague_loss_min, rules.plague_loss_max);
            for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
                PopBlock& pop = world_.pops[static_cast<std::size_t>(p)];
                Fixed size = pop.size - pop.size * loss;
                if (size < Fixed::zero()) {
                    size = Fixed::zero();
                }
                pop.size = size;
            }
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
