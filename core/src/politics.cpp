// Politik (seit M8): Wahlen nach docs/technik/politik.md, Abschnitt 7.

#include "uhrwerk/politics.hpp"

#include "uhrwerk/economy.hpp"
#include "uhrwerk/parallel.hpp"
#include "uhrwerk/rng.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>

namespace uhrwerk {

namespace {

template <typename T> bool in_range(const std::vector<T>& items, Index index) {
    return index >= 0 && static_cast<std::size_t>(index) < items.size();
}

std::size_t at(Index i) {
    return static_cast<std::size_t>(i);
}

bool contains(const std::vector<Index>& list, Index value) {
    return std::find(list.begin(), list.end(), value) != list.end();
}

Fixed abs(Fixed f) {
    return f < Fixed::zero() ? -f : f;
}

Fixed position_of(const World& world, std::size_t current) {
    return world.currents[current].position;
}

/// Strömung a schlägt b: mehr Stimmen; bei Gleichstand die gemäßigtere (kleinerer Betrag der
/// Position), dann der kleinere Index (Abschnitt 7.5).
bool beats(const World& world, const std::vector<Fixed>& votes, std::size_t a, std::size_t b) {
    if (votes[a] != votes[b]) {
        return votes[a] > votes[b];
    }
    const Fixed pa = abs(position_of(world, a));
    const Fixed pb = abs(position_of(world, b));
    if (pa != pb) {
        return pa < pb;
    }
    return a < b;
}

/// Zufallsgenerator je Wahl und Wahlkreis: Die Folge hängt nicht von der Reihenfolge der Rechnung ab.
Pcg32 district_rng(std::uint64_t seed, Index parliament, Index district) {
    const std::uint64_t mixed = seed + (static_cast<std::uint64_t>(parliament) + 1) * 0x9E3779B97F4A7C15ULL +
                                static_cast<std::uint64_t>(district) * 0xBF58476D1CE4E5B9ULL;
    return Pcg32(mixed, RngStream::Wahlen);
}

/// Ein Block, wie ihn die Wahl sieht.
struct Voter {
    Fixed voters;            // Wähler (Berechtigte × Beteiligung)
    Fixed income;            // Einkommen je Kopf (Steuerleistung für die Dreiklassen)
    Fixed weight;            // Gewicht der Stimme (Dreiklassen), sonst 1
    std::size_t leaning = 0; // erster Eintrag in der Neigungsliste
};

/// Wahlberechtigte eines Blocks (Abschnitt 7.1).
Fixed electorate_of(const World& world, const Parliament& parliament, const Location& loc,
                    const PopBlock& pop, Fixed income) {
    const ElectionRules& r = world.election_rules;
    const Profession& profession = world.professions[at(pop.profession)];
    const Fixed men_25 = r.male_share * (pop.young * r.young_of_age + pop.working + pop.old);
    switch (parliament.law.franchise) {
    case Franchise::Manhood:
        return men_25 * profession.self_employed;
    case Franchise::ThreeClass:
        return r.male_share * (pop.young * r.young_of_age_24 + pop.working + pop.old) *
               profession.self_employed;
    case Franchise::Census: {
        // Steuern je Kopf und Jahr aus dem Einkommen und der Kontribution des Besitzers; Privilegierte
        // (Adel, Klerus) zahlen keine Kontribution und wählen als Stand mit.
        if (!profession.taxable) {
            return men_25 * profession.self_employed;
        }
        const Fixed rate = in_range(world.states, loc.owner)
                               ? world.states[at(loc.owner)].tax_rate(TaxKind::Kontribution)
                               : Fixed{};
        const Fixed tax = Fixed::max(Fixed::zero(), income) * Fixed::from_int(12) * rate;
        return tax >= parliament.law.census_threshold ? men_25 * profession.self_employed : Fixed::zero();
    }
    case Franchise::None:
    case Franchise::Estates:
        break;
    }
    return Fixed::zero();
}

/// Wahlbeteiligung eines Blocks (Abschnitt 7.2).
Fixed turnout_of(const ElectionRules& r, const Location& loc, const PopBlock& pop) {
    const Fixed standard = (pop.living_standard / Fixed::from_int(100)).clamp(Fixed::zero(), Fixed::one());
    Fixed t =
        r.turnout_base + pop.literacy * r.turnout_literacy - r.turnout_poverty * (Fixed::one() - standard);
    if (loc.is_city) {
        t += r.turnout_city;
    }
    return t.clamp(r.turnout_min, r.turnout_max);
}

/// Dreiklassen (Abschnitt 7.4) in einem Ort, wie in den Urwahlbezirken: Die Wähler werden nach der
/// Steuerleistung (Einkommen je Kopf) absteigend geordnet und in drei Klassen mit je einem Drittel der
/// Steuern geteilt; jede besetzte Klasse zählt gleich viel. Ein Block kann über eine Klassengrenze reichen,
/// dann zählt er anteilig in beiden. Setzt Voter::weight.
void three_class_weights(std::vector<Voter>& voters) {
    std::vector<std::size_t> order(voters.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t a, std::size_t b) { return voters[a].income > voters[b].income; });
    Fixed total_tax;
    Fixed total_voters;
    for (const Voter& v : voters) {
        total_tax += v.voters * Fixed::max(Fixed::zero(), v.income);
        total_voters += v.voters;
    }
    for (Voter& v : voters) {
        v.weight = Fixed::one();
    }
    if (total_tax <= Fixed::zero() || total_voters <= Fixed::zero()) {
        return;
    }
    // Anteile jedes Blocks an den drei Klassen.
    const Fixed third = total_tax / Fixed::from_int(3);
    std::vector<std::array<Fixed, 3>> share(voters.size());
    std::array<Fixed, 3> class_voters{};
    Fixed begin;
    for (const std::size_t i : order) {
        const Fixed tax = voters[i].voters * Fixed::max(Fixed::zero(), voters[i].income);
        const Fixed end = begin + tax;
        for (std::size_t k = 0; k < 3; ++k) {
            const Fixed lo = third * Fixed::from_int(static_cast<std::int64_t>(k));
            const Fixed hi = k == 2 ? total_tax : third * Fixed::from_int(static_cast<std::int64_t>(k + 1));
            if (tax <= Fixed::zero()) {
                // Ohne Steuerleistung ans Ende der Ordnung, also in die dritte Klasse.
                share[i][k] = k == 2 ? Fixed::one() : Fixed::zero();
            } else {
                const Fixed overlap = Fixed::min(end, hi) - Fixed::max(begin, lo);
                share[i][k] = overlap > Fixed::zero() ? overlap / tax : Fixed::zero();
            }
            class_voters[k] += voters[i].voters * share[i][k];
        }
        begin = end;
    }
    std::int64_t occupied = 0;
    for (const Fixed v : class_voters) {
        occupied += v > Fixed::zero() ? 1 : 0;
    }
    if (occupied == 0) {
        return; // nur verschwindend kleine Blöcke: ungewichtet
    }
    std::array<Fixed, 3> class_weight{};
    for (std::size_t k = 0; k < 3; ++k) {
        if (class_voters[k] > Fixed::zero()) {
            class_weight[k] = total_voters / Fixed::from_int(occupied) / class_voters[k];
        }
    }
    for (std::size_t i = 0; i < voters.size(); ++i) {
        Fixed weight;
        for (std::size_t k = 0; k < 3; ++k) {
            weight += share[i][k] * class_weight[k];
        }
        voters[i].weight = weight;
    }
}

/// Rechenhilfe je Wahlkreis (parallel je Wahlkreis, eigene Daten).
struct DistrictWork {
    DistrictResult result;
    std::vector<Deputy> deputies;
    Fixed report_key; // Entfernung zum Tagungsort in km plus Zufall
};

DistrictWork elect_district(const World& world, Index parliament_index, Index district_index,
                            std::uint64_t seed) {
    const Parliament& parliament = world.parliaments[at(parliament_index)];
    const ElectoralDistrict& district = world.electoral_districts[at(district_index)];
    const ElectionRules& r = world.election_rules;
    const std::size_t currents = world.currents.size();
    const std::size_t professions = world.professions.size();
    Pcg32 rng = district_rng(seed, parliament_index, district_index);

    DistrictWork work;
    DistrictResult& result = work.result;
    result.district = district_index;
    result.votes.assign(currents, Fixed{});

    // Meldereihenfolge (Abschnitt 8): nahe am Tagungsort zuerst, mit bis zu 150 km Zufall.
    const Fixed jitter = rng.uniform(Fixed::zero(), Fixed::from_int(150));
    if (in_range(world.locations, parliament.seat) && !district.locations.empty()) {
        const Location& seat = world.locations[at(parliament.seat)];
        const Location& first = world.locations[at(district.locations.front())];
        work.report_key =
            distance_km(seat.longitude, seat.latitude, first.longitude, first.latitude) + jitter;
    } else {
        work.report_key = jitter * Fixed::from_int(1000) / Fixed::from_int(150);
    }

    // Boykott: Eine Kultur aus der Liste stellt mehr als die Hälfte der Menschen im Wahlkreis.
    std::vector<Fixed> culture_people(world.cultures.size(), Fixed{});
    Fixed people;
    const auto for_each_block = [&](Index location, const auto& fn) {
        const Location& loc = world.locations[at(location)];
        for (Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
            fn(world.pops[at(p)]);
        }
        if (in_range(world.arrivals, location)) {
            for (const PopBlock& pop : world.arrivals[at(location)]) {
                fn(pop);
            }
        }
    };
    for (const Index location : district.locations) {
        for_each_block(location, [&](const PopBlock& pop) {
            if (in_range(world.cultures, pop.culture)) {
                culture_people[at(pop.culture)] += pop.size();
            }
            people += pop.size();
        });
    }
    const bool boycott_here =
        parliament.boycott_markets.empty() ||
        (!district.locations.empty() &&
         contains(parliament.boycott_markets, world.locations[at(district.locations.front())].market));
    for (const Index culture : parliament.boycott_cultures) {
        if (boycott_here && culture_people[at(culture)] * Fixed::from_int(2) > people) {
            result.boycott = true;
        }
    }

    // Stimmen je Ort (Abschnitt 7.1 bis 7.4).
    std::vector<Voter> voters;
    std::vector<Fixed> leanings;
    for (const Index location : district.locations) {
        const Location& loc = world.locations[at(location)];
        const std::vector<Fixed> region = region_shift(world, location);
        voters.clear();
        leanings.clear();
        for_each_block(location, [&](const PopBlock& pop) {
            if (!in_range(world.professions, pop.profession)) {
                return;
            }
            const std::size_t income_index = at(location) * professions + at(pop.profession);
            const Fixed income =
                income_index < world.location_income.size() ? world.location_income[income_index] : Fixed{};
            const Fixed electorate = electorate_of(world, parliament, loc, pop, income);
            if (electorate <= Fixed::zero()) {
                return;
            }
            const Fixed turnout = turnout_of(r, loc, pop);
            result.electorate += electorate;
            result.voters += electorate * turnout;
            Voter v;
            v.voters = electorate * turnout;
            v.income = income;
            v.weight = Fixed::one();
            v.leaning = leanings.size();
            const std::vector<Fixed> leaning = current_leaning(world, pop, region);
            leanings.insert(leanings.end(), leaning.begin(), leaning.end());
            voters.push_back(v);
        });
        if (parliament.law.franchise == Franchise::ThreeClass) {
            three_class_weights(voters);
        }
        for (const Voter& v : voters) {
            const Fixed counted = v.voters * v.weight;
            for (std::size_t c = 0; c < currents; ++c) {
                result.votes[c] += counted * leanings[v.leaning + c];
            }
        }
    }
    if (parliament.law.indirect) {
        for (std::size_t c = 0; c < currents; ++c) {
            result.votes[c] = result.votes[c] * (c < r.electors.size() ? r.electors[c] : Fixed::one());
        }
    }
    if (result.boycott) {
        // Wer boykottiert, wählt nicht: Berechtigte bleiben stehen, Wähler und Stimmen fallen weg.
        result.voters = Fixed::zero();
        std::fill(result.votes.begin(), result.votes.end(), Fixed{});
        return work;
    }

    // Rauschen je Strömung (Abschnitt 7.3), immer gezogen, damit die Folge nicht von den Stimmen abhängt.
    for (std::size_t c = 0; c < currents; ++c) {
        result.votes[c] = result.votes[c] * (Fixed::one() + rng.uniform(-r.noise, r.noise));
    }
    Fixed total;
    for (const Fixed v : result.votes) {
        total += v;
    }
    if (total <= Fixed::zero() || currents == 0) {
        return work;
    }

    // Mehrheit und Stichwahl (Abschnitt 7.5).
    std::size_t first = 0;
    for (std::size_t c = 1; c < currents; ++c) {
        if (beats(world, result.votes, c, first)) {
            first = c;
        }
    }
    std::size_t winner = first;
    if (parliament.law.system == ElectoralSystem::Runoff &&
        result.votes[first] * Fixed::from_int(2) <= total && currents > 1) {
        std::size_t second = first == 0 ? 1 : 0;
        for (std::size_t c = 0; c < currents; ++c) {
            if (c != first && beats(world, result.votes, c, second)) {
                second = c;
            }
        }
        std::vector<Fixed> final_votes(currents, Fixed{});
        final_votes[first] = result.votes[first];
        final_votes[second] = result.votes[second];
        const Fixed pos_first = position_of(world, first);
        const Fixed pos_second = position_of(world, second);
        for (std::size_t c = 0; c < currents; ++c) {
            if (c == first || c == second) {
                continue;
            }
            // Anteilig nach der Nähe: an den ersten Finalisten Abstand zum zweiten ÷ beide Abstände.
            const Fixed to_first = abs(position_of(world, c) - pos_first);
            const Fixed to_second = abs(position_of(world, c) - pos_second);
            const Fixed distance = to_first + to_second;
            const Fixed share = distance > Fixed::zero() ? to_second / distance : Fixed::from_ratio(1, 2);
            const Fixed part = result.votes[c] * share;
            final_votes[first] += part;
            final_votes[second] += result.votes[c] - part;
        }
        winner = beats(world, final_votes, first, second) ? first : second;
        result.runoff = true;
    }
    result.winner = static_cast<Index>(winner);

    // Abgeordnete (Abschnitt 7.6): Position der Strömung, zur Mitte des Wahlkreises gezogen, mit Rauschen.
    Fixed mean;
    for (std::size_t c = 0; c < currents; ++c) {
        mean += result.votes[c] * position_of(world, c);
    }
    mean = mean / total;
    const Fixed own = position_of(world, winner);
    const Fixed limit = Fixed::from_int(100);
    for (std::int32_t seat = 0; seat < district.seats; ++seat) {
        Deputy deputy;
        deputy.district = district_index;
        deputy.current = result.winner;
        deputy.position =
            (own + r.position_pull * (mean - own) + rng.uniform(-r.position_noise, r.position_noise))
                .clamp(-limit, limit);
        deputy.faction = rng.chance(r.independent_share) ? kNone : faction_of(parliament, deputy.position);
        work.deputies.push_back(deputy);
    }
    return work;
}

} // namespace

std::vector<Fixed> interest_memberships(const World& world, const PopBlock& pop) {
    std::vector<Fixed> out(world.interest_groups.size(), Fixed{});
    for (std::size_t g = 0; g < world.interest_groups.size(); ++g) {
        const InterestGroup& group = world.interest_groups[g];
        Fixed m =
            in_range(group.professions, pop.profession) ? group.professions[at(pop.profession)] : Fixed{};
        if (!group.confessions.empty()) {
            m = m * (in_range(group.confessions, pop.confession) ? group.confessions[at(pop.confession)]
                                                                 : Fixed{});
        }
        if (group.by_literacy) {
            m = m * pop.literacy;
        }
        out[g] = m.clamp(Fixed::zero(), Fixed::one());
    }
    return out;
}

std::vector<Fixed> region_shift(const World& world, Index location) {
    std::vector<Fixed> out(world.currents.size(), Fixed{});
    if (!in_range(world.locations, location)) {
        return out;
    }
    const Location& loc = world.locations[at(location)];
    for (const PoliticalRegion& region : world.political_regions) {
        if (!contains(region.states, loc.owner) && !contains(region.markets, loc.market)) {
            continue;
        }
        for (std::size_t c = 0; c < out.size() && c < region.leaning.size(); ++c) {
            out[c] += region.leaning[c];
        }
    }
    return out;
}

std::vector<Fixed> current_leaning(const World& world, const PopBlock& pop,
                                   const std::vector<Fixed>& region) {
    const ElectionRules& r = world.election_rules;
    const std::size_t currents = world.currents.size();
    std::vector<Fixed> out(currents, Fixed{});
    if (currents == 0) {
        return out;
    }
    const auto value = [](const std::vector<Fixed>& v, std::size_t c) {
        return c < v.size() ? v[c] : Fixed{};
    };

    // Interessengruppen; wer keiner (ganz) angehört, neigt mit dem Rest wie die Grundneigung.
    const std::vector<Fixed> memberships = interest_memberships(world, pop);
    Fixed members;
    for (std::size_t g = 0; g < memberships.size(); ++g) {
        members += memberships[g];
        for (std::size_t c = 0; c < currents; ++c) {
            out[c] += memberships[g] * value(world.interest_groups[g].leaning, c);
        }
    }
    if (members > Fixed::one()) {
        for (Fixed& v : out) {
            v = v / members;
        }
    }
    const Fixed rest = Fixed::max(Fixed::zero(), Fixed::one() - members);

    // Bildung, Not, Stadt und Region verschieben.
    Fixed need;
    if (r.poverty_below > Fixed::zero() && pop.living_standard < r.poverty_below) {
        need = ((r.poverty_below - pop.living_standard) / r.poverty_below).clamp(Fixed::zero(), Fixed::one());
    }
    const bool city = in_range(world.locations, pop.location) && world.locations[at(pop.location)].is_city;
    Fixed sum;
    for (std::size_t c = 0; c < currents; ++c) {
        Fixed v = out[c] + rest * value(r.base, c) + pop.literacy * value(r.literacy, c) +
                  need * value(r.poverty, c) + value(region, c);
        if (city) {
            v += value(r.city, c);
        }
        out[c] = Fixed::max(Fixed::zero(), v);
        sum += out[c];
    }
    if (sum <= Fixed::zero()) {
        std::fill(out.begin(), out.end(),
                  Fixed::one() / Fixed::from_int(static_cast<std::int64_t>(currents)));
        return out;
    }
    for (Fixed& v : out) {
        v = v / sum;
    }
    return out;
}

Index faction_of(const Parliament& parliament, Fixed position) {
    for (std::size_t f = 0; f < parliament.factions.size(); ++f) {
        const Faction& faction = parliament.factions[f];
        const bool top = f + 1 == parliament.factions.size();
        if (position >= faction.from && (position < faction.to || (top && position <= faction.to))) {
            return static_cast<Index>(f);
        }
    }
    return kNone;
}

ElectionResult run_election(const World& world, Index parliament, std::uint64_t seed, ThreadPool* pool) {
    ElectionResult result;
    result.parliament = parliament;
    result.day = world.date;
    if (!in_range(world.parliaments, parliament)) {
        return result;
    }
    std::vector<Index> districts;
    for (std::size_t d = 0; d < world.electoral_districts.size(); ++d) {
        if (world.electoral_districts[d].parliament == parliament) {
            districts.push_back(static_cast<Index>(d));
        }
    }
    std::vector<DistrictWork> work(districts.size());
    const auto compute = [&](std::size_t begin, std::size_t end) {
        for (std::size_t k = begin; k < end; ++k) {
            work[k] = elect_district(world, parliament, districts[k], seed);
        }
    };
    if (pool != nullptr) {
        pool->run(districts.size(), 16, compute);
    } else {
        compute(0, districts.size());
    }

    // Meldereihenfolge: nach Schlüssel, bei Gleichstand nach Wahlkreis.
    std::vector<std::size_t> order(work.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t a, std::size_t b) { return work[a].report_key < work[b].report_key; });
    for (std::size_t rank = 0; rank < order.size(); ++rank) {
        work[order[rank]].result.report_rank = static_cast<std::int32_t>(rank);
    }
    result.districts.reserve(work.size());
    for (DistrictWork& w : work) {
        result.districts.push_back(std::move(w.result));
        result.deputies.insert(result.deputies.end(), w.deputies.begin(), w.deputies.end());
    }
    return result;
}

std::vector<std::int32_t> seats_by_current(const World& world, const ElectionResult& election) {
    std::vector<std::int32_t> seats(world.currents.size(), 0);
    for (const Deputy& deputy : election.deputies) {
        if (in_range(seats, deputy.current)) {
            ++seats[at(deputy.current)];
        }
    }
    return seats;
}

std::vector<std::int32_t> seats_by_faction(const World& world, const ElectionResult& election) {
    std::vector<std::int32_t> seats;
    if (!in_range(world.parliaments, election.parliament)) {
        return seats;
    }
    // Letzter Eintrag: fraktionslos.
    seats.assign(world.parliaments[at(election.parliament)].factions.size() + 1, 0);
    for (const Deputy& deputy : election.deputies) {
        ++seats[in_range(seats, deputy.faction) ? at(deputy.faction) : seats.size() - 1];
    }
    return seats;
}

} // namespace uhrwerk
