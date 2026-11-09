#include "uhrwerk/legacy.hpp"

#include "uhrwerk/diplomacy.hpp"
#include "uhrwerk/military.hpp"
#include "uhrwerk/politics.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <utility>

namespace uhrwerk {

namespace {

constexpr std::size_t at(Index i) {
    return static_cast<std::size_t>(i);
}

template <typename T> bool in_range(const std::vector<T>& v, Index i) {
    return i >= 0 && static_cast<std::size_t>(i) < v.size();
}

struct MeasureText {
    LegacyMeasure measure;
    std::string_view id;
    std::string_view name;
};

constexpr std::array<MeasureText, kLegacyMeasures> kMeasureTexts{{
    {LegacyMeasure::None, "", ""},
    {LegacyMeasure::Population, "bevoelkerung", "Bevölkerung (Mio.)"},
    {LegacyMeasure::Income, "einnahmen", "Jahreseinnahmen (Mio. Taler)"},
    {LegacyMeasure::Debt, "schulden", "Schulden (Mio. Taler)"},
    {LegacyMeasure::Treasury, "schatz", "Staatskasse (Mio. Taler)"},
    {LegacyMeasure::DebtRatio, "schuldenquote", "Schuldenquote"},
    {LegacyMeasure::LivingStandard, "lebensstandard", "Lebensstandard"},
    {LegacyMeasure::Literacy, "alphabetisierung", "Alphabetisierung (%)"},
    {LegacyMeasure::Emigration, "auswanderung", "Auswanderung (‰)"},
    {LegacyMeasure::Unrest, "unruhe", "Unruhe"},
    {LegacyMeasure::Approval, "zustimmung", "Zustimmung"},
    {LegacyMeasure::Soldiers, "soldaten", "Soldaten (Tsd.)"},
    {LegacyMeasure::Strength, "staerke", "Stärke"},
    {LegacyMeasure::Locations, "orte", "Orte"},
    {LegacyMeasure::Alliances, "buendnisse", "Bündnisse"},
    {LegacyMeasure::WarsWon, "siege", "gewonnene Kriege"},
    {LegacyMeasure::WarsLost, "niederlagen", "verlorene Kriege"},
    {LegacyMeasure::Bankruptcies, "bankrotte", "Staatsbankrotte"},
    {LegacyMeasure::Revolts, "aufstaende", "Aufstände"},
    {LegacyMeasure::Elections, "wahlen", "Wahlen (zehn Jahre)"},
    {LegacyMeasure::Presidency, "praesidium", "Präsidium im Bund"},
    {LegacyMeasure::FederationMember, "bundesmitglied", "Mitglied des Bundes"},
}};

Fixed soldiers_of(const World& world, Index state) {
    if (military_active(world)) {
        return state_soldiers(world, state);
    }
    const State& s = world.states[at(state)];
    return s.army_per_mille * world.state_population(state) / Fixed::from_int(1000);
}

const Fixed kMillion = Fixed::from_int(1'000'000);

} // namespace

std::string_view legacy_measure_name(LegacyMeasure measure) {
    return kMeasureTexts[static_cast<std::size_t>(measure)].name;
}

std::string_view legacy_measure_id(LegacyMeasure measure) {
    return kMeasureTexts[static_cast<std::size_t>(measure)].id;
}

std::array<Fixed, kLegacyMeasures> legacy_measures(const World& world, Index state) {
    std::array<Fixed, kLegacyMeasures> m{};
    if (!in_range(world.states, state)) {
        return m;
    }
    const auto set = [&](LegacyMeasure k, Fixed v) { m[static_cast<std::size_t>(k)] = v; };
    const State& s = world.states[at(state)];

    // Ein Gang über die Orte: Einwohner, Lebensstandard und Alphabetisierung der Blöcke, Auswanderung.
    Fixed population;
    Fixed weighted_living;
    Fixed weighted_literacy;
    Fixed block_people;
    Fixed emigrated;
    std::int64_t locations = 0;
    for (std::size_t l = 0; l < world.locations.size(); ++l) {
        const Location& loc = world.locations[l];
        if (loc.owner != state) {
            continue;
        }
        if (loc.has_data()) {
            ++locations;
        }
        population += world.location_population(static_cast<Index>(l));
        for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
            const PopBlock& pop = world.pops[at(p)];
            const Fixed size = pop.size();
            block_people += size;
            weighted_living += pop.living_standard * size;
            weighted_literacy += pop.literacy * size;
        }
        if (l < world.balance_previous.size()) {
            emigrated += world.balance_previous[l].emigrated;
        }
    }
    set(LegacyMeasure::Population, population / kMillion);
    set(LegacyMeasure::Income, s.yearly_income / kMillion);
    set(LegacyMeasure::Debt, s.debt / kMillion);
    set(LegacyMeasure::Treasury, s.treasury / kMillion);
    set(LegacyMeasure::DebtRatio, s.yearly_income > Fixed::zero() ? s.debt / s.yearly_income : Fixed::zero());
    if (block_people > Fixed::zero()) {
        set(LegacyMeasure::LivingStandard, weighted_living / block_people);
        set(LegacyMeasure::Literacy, weighted_literacy / block_people * Fixed::from_int(100));
    }
    if (population > Fixed::zero()) {
        set(LegacyMeasure::Emigration, emigrated / population * Fixed::from_int(1000));
    }
    set(LegacyMeasure::Locations, Fixed::from_int(locations));
    if (in_range(world.unrest, state)) {
        set(LegacyMeasure::Unrest, world.unrest[at(state)]);
    }
    // Zustimmung: nach dem Anteil der Gruppen im Staat gemittelt.
    const std::size_t groups = world.interest_groups.size();
    if (groups > 0 && world.approval.size() == world.states.size() * groups) {
        const std::vector<InterestGroupStanding> standing = interest_group_standing(world, state);
        Fixed shares;
        Fixed sum;
        for (std::size_t g = 0; g < standing.size() && g < groups; ++g) {
            shares += standing[g].share;
            sum += standing[g].share * world.approval[at(state) * groups + g];
        }
        if (shares > Fixed::zero()) {
            set(LegacyMeasure::Approval, sum / shares);
        }
    }
    const Fixed soldiers = soldiers_of(world, state);
    set(LegacyMeasure::Soldiers, soldiers / Fixed::from_int(1000));
    Fixed strongest;
    for (std::size_t o = 0; o < world.states.size(); ++o) {
        strongest = Fixed::max(strongest, soldiers_of(world, static_cast<Index>(o)));
    }
    if (strongest > Fixed::zero()) {
        set(LegacyMeasure::Strength, soldiers / strongest);
    }
    std::int64_t alliances = 0;
    for (const Treaty& t : world.treaties) {
        if (t.kind == TreatyKind::Alliance && (t.from == state || t.to == state)) {
            ++alliances;
        }
    }
    set(LegacyMeasure::Alliances, Fixed::from_int(alliances));
    std::int64_t won = 0;
    std::int64_t lost = 0;
    for (const WarResult& r : world.war_results) {
        if ((r.attacker == state && r.winner > 0) || (r.defender == state && r.winner < 0)) {
            ++won;
        } else if ((r.attacker == state && r.winner < 0) || (r.defender == state && r.winner > 0)) {
            ++lost;
        }
    }
    set(LegacyMeasure::WarsWon, Fixed::from_int(won));
    set(LegacyMeasure::WarsLost, Fixed::from_int(lost));
    set(LegacyMeasure::Bankruptcies, Fixed::from_int(s.bankruptcies));
    const auto revolts = std::count_if(world.revolts.begin(), world.revolts.end(),
                                       [&](const Revolt& r) { return r.state == state; }) +
                         std::count_if(world.revolt_history.begin(), world.revolt_history.end(),
                                       [&](const RevoltRecord& r) { return r.state == state; });
    set(LegacyMeasure::Revolts, Fixed::from_int(static_cast<std::int64_t>(revolts)));
    // Wahlen der letzten zehn Jahre mit mindestens einem Wahlkreis des Staates.
    std::int64_t elections = 0;
    const std::int64_t today = to_day_number(world.date);
    for (const ElectionResult& e : world.elections) {
        if (today - to_day_number(e.day) > 3652) {
            continue;
        }
        const bool own = std::any_of(e.districts.begin(), e.districts.end(), [&](const DistrictResult& d) {
            return in_range(world.electoral_districts, d.district) &&
                   world.electoral_districts[at(d.district)].state == state;
        });
        elections += own ? 1 : 0;
    }
    set(LegacyMeasure::Elections, Fixed::from_int(elections));
    set(LegacyMeasure::Presidency,
        world.diet_presidency == state && has_diet(world) ? Fixed::one() : Fixed::zero());
    const bool member = federation_founded(world) && in_range(world.federation.members, state) &&
                        world.federation.members[at(state)] != 0 &&
                        !(in_range(world.left_federation, state) && world.left_federation[at(state)] != 0);
    set(LegacyMeasure::FederationMember, member ? Fixed::one() : Fixed::zero());
    return m;
}

std::vector<LegacyValue> legacy(const World& world, const EventCache& cache, Index state) {
    std::vector<LegacyValue> out;
    if (!world.legacy_rules.active || !in_range(world.states, state)) {
        return out;
    }
    const std::array<Fixed, kLegacyMeasures> measures = legacy_measures(world, state);
    out.reserve(world.legacy_rules.dimensions.size());
    for (const LegacyDimension& d : world.legacy_rules.dimensions) {
        LegacyValue v;
        v.base = d.base;
        Fixed total = d.base;
        for (const LegacyFactor& f : d.factors) {
            LegacyContribution c;
            c.name = f.name;
            c.active = !f.has_condition || condition_holds(world, cache, f.condition, state);
            if (c.active) {
                c.value = f.measure == LegacyMeasure::None
                              ? f.weight
                              : f.weight * measures[static_cast<std::size_t>(f.measure)];
                if (f.cap > Fixed::zero()) {
                    c.value = std::clamp(c.value, -f.cap, f.cap);
                }
            }
            total += c.value;
            v.factors.push_back(std::move(c));
        }
        v.value = std::clamp(total, Fixed::zero(), Fixed::from_int(100));
        out.push_back(std::move(v));
    }
    return out;
}

std::string legacy_sentence(const LegacyDimension& dimension, Fixed value) {
    for (const LegacySentence& s : dimension.sentences) {
        if (value >= s.from) {
            return s.text;
        }
    }
    return {};
}

std::size_t yearbook_columns(const World& world) {
    return kYearbookMeasures.size() + world.legacy_rules.dimensions.size();
}

YearRecord year_record(const World& world, const EventCache& cache) {
    YearRecord record;
    record.date = world.date;
    const std::size_t columns = yearbook_columns(world);
    record.values.assign(world.states.size() * columns, Fixed{});
    for (std::size_t s = 0; s < world.states.size(); ++s) {
        const auto state = static_cast<Index>(s);
        const std::array<Fixed, kLegacyMeasures> measures = legacy_measures(world, state);
        for (std::size_t c = 0; c < kYearbookMeasures.size(); ++c) {
            record.values[s * columns + c] = measures[static_cast<std::size_t>(kYearbookMeasures[c])];
        }
        const std::vector<LegacyValue> values = legacy(world, cache, state);
        for (std::size_t d = 0; d < values.size(); ++d) {
            record.values[s * columns + kYearbookMeasures.size() + d] = values[d].value;
        }
    }
    return record;
}

bool game_over(const World& world) {
    return world.scenario.has_end && world.date > world.scenario.end;
}

} // namespace uhrwerk
