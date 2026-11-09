#include "uhrwerk_simulation.h"

#include "uhrwerk/ai.hpp"
#include "uhrwerk/customs.hpp"
#include "uhrwerk/date.hpp"
#include "uhrwerk/diplomacy.hpp"
#include "uhrwerk/economy.hpp"
#include "uhrwerk/events.hpp"
#include "uhrwerk/legacy.hpp"
#include "uhrwerk/loader.hpp"
#include "uhrwerk/military.hpp"
#include "uhrwerk/politics.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/color.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace uhrwerk_bridge {

using godot::Array;
using godot::Color;
using godot::D_METHOD;
using godot::Dictionary;
using godot::PackedByteArray;
using godot::PackedColorArray;
using godot::PackedFloat32Array;
using godot::PackedFloat64Array;
using godot::PackedInt32Array;
using godot::PackedStringArray;
using godot::String;

namespace {

String to_godot(std::string_view text) {
    return String::utf8(text.data(), static_cast<int>(text.size()));
}

std::string to_std(const String& text) {
    const godot::CharString utf8 = text.utf8();
    return std::string(utf8.get_data(), static_cast<std::size_t>(utf8.length()));
}

// Pfade als UTF-8 übergeben, damit Umlaute auch unter Windows ankommen.
std::filesystem::path to_path(const String& text) {
    const std::string utf8 = to_std(text);
    return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

Color to_godot(const uhrwerk::Color& c) {
    return Color(static_cast<float>(c.r) / 255.0f, static_cast<float>(c.g) / 255.0f,
                 static_cast<float>(c.b) / 255.0f);
}

template <typename T> bool valid_index(const std::vector<T>& items, std::int64_t index) {
    return index >= 0 && index < static_cast<std::int64_t>(items.size());
}

template <typename T> const T& at(const std::vector<T>& items, std::int64_t index) {
    return items[static_cast<std::size_t>(index)];
}

// Verlauf zwischen zwei Farben, t in [0, 1].
Color blend(const Color& from, const Color& to, double t) {
    const float f = static_cast<float>(std::clamp(t, 0.0, 1.0));
    return from.lerp(to, f);
}

const Color kUnknown(0.62f, 0.60f, 0.56f);
const Color kDensityLow(0.96f, 0.93f, 0.84f);
const Color kDensityHigh(0.36f, 0.16f, 0.12f);
const Color kDevastationNone(0.90f, 0.88f, 0.80f);
const Color kDevastationFull(0.72f, 0.08f, 0.06f);
const Color kLivingLow(0.66f, 0.22f, 0.16f);       // Lebensstandard 0
const Color kLivingHigh(0.24f, 0.52f, 0.30f);      // Lebensstandard 100
const Color kEconomyLow(0.95f, 0.92f, 0.80f);      // Wirtschaftsleistung 0
const Color kEconomyHigh(0.55f, 0.36f, 0.08f);     // Wirtschaftsleistung ab kEconomyMax
const Color kRailStation(0.45f, 0.13f, 0.08f);     // Bahnhof: Endpunkt eines eröffneten Abschnitts (seit M7)
const Color kRailMarket(0.85f, 0.70f, 0.50f);      // Ort in einem Markt mit Bahnhof
const Color kRailNone(0.94f, 0.91f, 0.83f);        // ohne Bahn
const Color kElectionPaper(0.95f, 0.93f, 0.86f);   // Wahlen (seit M8): knapper Sieg hell …
const Color kElectionOutside(0.90f, 0.88f, 0.82f); // … außerhalb des Wahlgebiets
const Color kElectionBoycott(0.76f, 0.79f, 0.84f); // … Boykott (kühles Grau): der Wahlkreis wählte nicht
constexpr double kCustomsAlone = 0.62;             // Zollgebiete (seit M9): Staaten ohne Union blass
const Color kRelationNeutral(0.94f, 0.91f, 0.83f); // Beziehungen (seit M9, b3): 0 …
const Color kRelationFriend(0.24f, 0.52f, 0.30f);  // … +100 und mehr
const Color kRelationFoe(0.70f, 0.16f, 0.10f);     // … −100 und weniger
const Color kRevoltColor(0.42f, 0.16f, 0.48f);     // Unruhe (seit M11, b4): Staat mit Aufstand
constexpr double kRelationScale = 100.0;
constexpr double kWarNeutral =
    0.6; // Krieg (seit M10, b3): Staaten ohne Krieg blass           // volle Farbe ab diesem Betrag
constexpr double kDensityMax = 120.0; // Einwohner je km², ab hier dunkelste Stufe
constexpr double kEconomyMax = 16.0;  // Reichstaler je Einwohner und Jahr, dunkelste Stufe

// Feste Palette für MAP_KREISE: palette[kreisindex % n]. Gedämpfte, gut unterscheidbare Töne;
// aufeinanderfolgende Einträge liegen weit auseinander (CIELAB ΔE ≥ 37), alle mindestens
// ΔE 25 vom Grau für Orte ohne Kreis (kUnknown) entfernt.
const std::array<Color, 12> kKreisPalette = {
    Color(0.80f, 0.50f, 0.40f), // Terrakotta
    Color(0.44f, 0.62f, 0.80f), // Stahlblau
    Color(0.88f, 0.77f, 0.40f), // Ocker
    Color(0.52f, 0.72f, 0.46f), // Salbeigrün
    Color(0.72f, 0.54f, 0.80f), // Lavendel
    Color(0.93f, 0.60f, 0.28f), // Orange
    Color(0.36f, 0.68f, 0.66f), // Petrol
    Color(0.90f, 0.60f, 0.70f), // Rosa
    Color(0.64f, 0.68f, 0.30f), // Oliv
    Color(0.42f, 0.46f, 0.74f), // Indigo
    Color(0.74f, 0.58f, 0.40f), // Hellbraun
    Color(0.62f, 0.84f, 0.88f), // Eisblau
};

/// Bahnanschluss zum aktuellen Datum (seit M7): 2 = Bahnhof (Endpunkt eines eröffneten Abschnitts),
/// 1 = Ort in einem Markt, der mit einem Nachbarmarkt zur Bahnfracht handelt (wie im Kern,
/// wirtschaft.md 4.1), 0 = ohne Bahn.
std::vector<std::uint8_t> rail_access(const uhrwerk::World& world, const uhrwerk::EconomyTopology& topology) {
    std::vector<std::uint8_t> access(world.locations.size(), 0);
    for (const uhrwerk::Railway& railway : world.railways) {
        for (const uhrwerk::RailSection& section : railway.sections) {
            if (section.opened > world.date) {
                continue;
            }
            for (const uhrwerk::Index end : {section.from, section.to}) {
                if (valid_index(world.locations, end)) {
                    access[static_cast<std::size_t>(end)] = 2;
                }
            }
        }
    }
    std::vector<bool> market_on_rail(world.markets.size(), false);
    for (std::size_t m = 0; m < topology.neighbors.size() && m < market_on_rail.size(); ++m) {
        for (const uhrwerk::MarketLink& link : topology.neighbors[m]) {
            if (link.rail_from && *link.rail_from <= world.date) {
                market_on_rail[m] = true;
            }
        }
    }
    for (std::size_t i = 0; i < world.locations.size(); ++i) {
        const uhrwerk::Index market = world.locations[i].market;
        if (access[i] == 0 && valid_index(world.markets, market) &&
            market_on_rail[static_cast<std::size_t>(market)]) {
            access[i] = 1;
        }
    }
    return access;
}

double density_of(const uhrwerk::World& world, std::int64_t location) {
    const uhrwerk::Location& loc = at(world.locations, location);
    const double area = loc.area_km2.to_double();
    if (area <= 0.0) {
        return 0.0;
    }
    return world.location_population(static_cast<uhrwerk::Index>(location)).to_double() / area;
}

/// Bevölkerung eines Ortes aus Blöcken und Zugängen: Altersgruppen, Mittelwerte und Anteile.
struct People {
    uhrwerk::Fixed total;
    uhrwerk::Fixed children;
    uhrwerk::Fixed young;
    uhrwerk::Fixed working;
    uhrwerk::Fixed old;
    double living_standard = 0.0; // nach Personen gemittelt, 0..100
    double literacy = 0.0;        // nach Personen gemittelt, 0..1
    std::vector<uhrwerk::Fixed> per_confession;
    std::vector<uhrwerk::Fixed> per_culture;
};

People people_of(const uhrwerk::World& world, std::int64_t location) {
    People result;
    result.per_confession.resize(world.confessions.size());
    result.per_culture.resize(world.cultures.size());
    double living = 0.0;
    double literacy = 0.0;
    const auto add = [&](const uhrwerk::PopBlock& pop) {
        const uhrwerk::Fixed size = pop.size();
        result.total += size;
        result.children += pop.children;
        result.young += pop.young;
        result.working += pop.working;
        result.old += pop.old;
        living += pop.living_standard.to_double() * size.to_double();
        literacy += pop.literacy.to_double() * size.to_double();
        if (valid_index(result.per_confession, pop.confession)) {
            result.per_confession[static_cast<std::size_t>(pop.confession)] += size;
        }
        if (valid_index(result.per_culture, pop.culture)) {
            result.per_culture[static_cast<std::size_t>(pop.culture)] += size;
        }
    };
    const uhrwerk::Location& loc = at(world.locations, location);
    for (uhrwerk::Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
        if (valid_index(world.pops, p)) {
            add(at(world.pops, p));
        }
    }
    if (valid_index(world.arrivals, location)) {
        for (const uhrwerk::PopBlock& arrival : at(world.arrivals, location)) {
            add(arrival);
        }
    }
    const double total = result.total.to_double();
    if (total > 0.0) {
        result.living_standard = living / total;
        result.literacy = literacy / total;
    }
    return result;
}

/// Anteile in Prozent je Name (nur Einträge mit Personen), z. B. für Konfessionen und Kulturen.
template <typename T>
Dictionary shares_of(const std::vector<uhrwerk::Fixed>& amounts, const std::vector<T>& items, double total) {
    Dictionary result;
    for (std::size_t i = 0; i < amounts.size() && i < items.size(); ++i) {
        if (amounts[i] > uhrwerk::Fixed::zero() && total > 0.0) {
            result[to_godot(items[i].name)] = amounts[i].to_double() * 100.0 / total;
        }
    }
    return result;
}

Dictionary balance_to_godot(const uhrwerk::PopulationBalance& b) {
    Dictionary result;
    result["births"] = b.births.round();
    result["deaths"] = b.deaths.round();
    result["plague_deaths"] = b.plague_deaths.round();
    result["moved_in"] = b.moved_in.round();
    result["moved_out"] = b.moved_out.round();
    result["emigrated"] = b.emigrated.round();
    return result;
}

void add_balance(uhrwerk::PopulationBalance& sum, const uhrwerk::PopulationBalance& b) {
    sum.births += b.births;
    sum.deaths += b.deaths;
    sum.plague_deaths += b.plague_deaths;
    sum.moved_in += b.moved_in;
    sum.moved_out += b.moved_out;
    sum.emigrated += b.emigrated;
}

std::int64_t to_percent(uhrwerk::Fixed share) {
    return (share * uhrwerk::Fixed::from_int(100)).round();
}

/// Köpfe je Beruf in einem Ort (Blöcke und Zugänge).
std::vector<uhrwerk::Fixed> people_by_profession(const uhrwerk::World& world, std::int64_t location) {
    std::vector<uhrwerk::Fixed> result(world.professions.size());
    const auto add = [&](const uhrwerk::PopBlock& pop) {
        if (valid_index(result, pop.profession)) {
            result[static_cast<std::size_t>(pop.profession)] += pop.size();
        }
    };
    const uhrwerk::Location& loc = at(world.locations, location);
    for (uhrwerk::Index p = loc.first_pop; p < loc.first_pop + loc.pop_count; ++p) {
        if (valid_index(world.pops, p)) {
            add(at(world.pops, p));
        }
    }
    if (valid_index(world.arrivals, location)) {
        for (const uhrwerk::PopBlock& arrival : at(world.arrivals, location)) {
            add(arrival);
        }
    }
    return result;
}

/// Wirtschaftsleistung je Einwohner und Jahr (Reichstaler); 0 ohne Einwohner.
double production_per_capita(const uhrwerk::World& world, std::int64_t location) {
    const double people = world.location_population(static_cast<uhrwerk::Index>(location)).to_double();
    if (people <= 0.0 || !valid_index(world.economy, location)) {
        return 0.0;
    }
    return at(world.economy, location).production.to_double() * 12.0 / people;
}

/// Wirtschaft eines Ortes für get_location (seit M5): Berufe, Betriebe und Güter.
void add_location_economy(const uhrwerk::World& w, std::int64_t index, Dictionary& result) {
    const uhrwerk::Location& loc = at(w.locations, index);
    const auto li = static_cast<std::size_t>(index);
    result["market_index"] = valid_index(w.markets, loc.market) ? loc.market : uhrwerk::kNone;
    result["market_name"] =
        valid_index(w.markets, loc.market) ? to_godot(at(w.markets, loc.market).name) : String();
    result["is_port"] = loc.is_port;
    result["development_percent"] =
        uhrwerk::development_bonus(w, static_cast<uhrwerk::Index>(index)).to_double() * 100.0;

    const std::vector<uhrwerk::Fixed> people = people_by_profession(w, index);
    const std::size_t professions = w.professions.size();
    const std::size_t needs = w.needs.size();
    Array profession_rows;
    for (std::size_t p = 0; p < professions && loc.has_data(); ++p) {
        if (people[p] <= uhrwerk::Fixed::zero()) {
            continue;
        }
        Dictionary row;
        row["name"] = to_godot(w.professions[p].name);
        row["stratum"] = static_cast<std::int64_t>(w.professions[p].stratum);
        row["people"] = people[p].round();
        const std::size_t slot = li * professions + p;
        row["income_year"] =
            slot < w.location_income.size() ? w.location_income[slot].to_double() * 12.0 : 0.0;
        double coverage = 0.0;
        double food = 1.0;
        bool any_food = false;
        for (std::size_t n = 0; n < needs; ++n) {
            const std::size_t c = slot * needs + n;
            const double value = c < w.location_coverage.size() ? w.location_coverage[c].to_double() : 0.0;
            coverage += value;
            if (w.needs[n].hunger) {
                food = any_food ? std::min(food, value) : value;
                any_food = true;
            }
        }
        row["coverage_percent"] = needs > 0 ? coverage * 100.0 / static_cast<double>(needs) : 0.0;
        row["food_percent"] = food * 100.0;
        profession_rows.push_back(row);
    }
    result["professions"] = profession_rows;

    Array business_rows;
    const std::size_t businesses = w.businesses.size();
    for (std::size_t b = 0; b < businesses && loc.has_data(); ++b) {
        const uhrwerk::BusinessType& business = w.businesses[b];
        const std::size_t slot = li * businesses + b;
        if (slot >= w.business_shares.size() || !valid_index(people, business.profession)) {
            continue;
        }
        const uhrwerk::Fixed heads = w.business_shares[slot] * at(people, business.profession);
        if (heads.round() <= 0) {
            continue;
        }
        Dictionary row;
        row["name"] = to_godot(business.name);
        row["profession"] = to_godot(at(w.professions, business.profession).name);
        row["people"] = heads.round();
        business_rows.push_back(row);
    }
    result["businesses"] = business_rows;

    Array good_rows;
    const std::size_t goods = w.goods.size();
    const uhrwerk::MarketState* market =
        valid_index(w.market_states, loc.market) ? &at(w.market_states, loc.market) : nullptr;
    for (std::size_t g = 0; g < goods && loc.has_data(); ++g) {
        const std::size_t slot = li * goods + g;
        const double supply = slot < w.location_supply.size() ? w.location_supply[slot].to_double() : 0.0;
        const double demand = slot < w.location_demand.size() ? w.location_demand[slot].to_double() : 0.0;
        if (supply <= 0.0 && demand <= 0.0) {
            continue;
        }
        const uhrwerk::Good& good = w.goods[g];
        const double price = market && g < market->price.size() ? market->price[g].to_double() : 0.0;
        const double base = good.base_price.to_double();
        Dictionary row;
        row["name"] = to_godot(good.name);
        row["color"] = to_godot(good.color);
        row["supply"] = supply;
        row["demand"] = demand;
        row["price"] = price;
        row["price_factor"] = base > 0.0 ? price / base : 0.0;
        good_rows.push_back(row);
    }
    result["goods"] = good_rows;
}

std::int64_t find_in(const std::unordered_map<std::string, std::int64_t>& lookup, const String& id) {
    const auto it = lookup.find(to_std(id));
    return it == lookup.end() ? uhrwerk::kNone : it->second;
}

// Politik (seit M8)
Dictionary date_dictionary(const uhrwerk::Date& date) {
    Dictionary result;
    result["year"] = date.year;
    result["month"] = date.month;
    result["day"] = date.day;
    return result;
}

String franchise_name(uhrwerk::Franchise franchise) {
    switch (franchise) {
    case uhrwerk::Franchise::Manhood:
        return to_godot(std::string_view("allgemeines Männerwahlrecht"));
    case uhrwerk::Franchise::ThreeClass:
        return to_godot(std::string_view("Dreiklassenwahlrecht"));
    case uhrwerk::Franchise::Census:
        return to_godot(std::string_view("Zensuswahlrecht"));
    case uhrwerk::Franchise::Estates:
        return to_godot(std::string_view("ständisch"));
    case uhrwerk::Franchise::None:
        break;
    }
    return to_godot(std::string_view("kein Wahlrecht"));
}

String constitution_name(uhrwerk::Constitution constitution) {
    switch (constitution) {
    case uhrwerk::Constitution::Constitutional:
        return to_godot(std::string_view("konstitutionell"));
    case uhrwerk::Constitution::Estates:
        return to_godot(std::string_view("ständisch"));
    case uhrwerk::Constitution::None:
        break;
    }
    return to_godot(std::string_view("keine Verfassung"));
}

/// Anteile der Stimmen je Strömung (Summe 1, alle 0 ohne Stimmen).
PackedFloat64Array vote_shares(const uhrwerk::DistrictResult& r) {
    PackedFloat64Array shares;
    double total = 0.0;
    for (const uhrwerk::Fixed v : r.votes) {
        total += v.to_double();
    }
    for (const uhrwerk::Fixed v : r.votes) {
        shares.push_back(total > 0.0 ? v.to_double() / total : 0.0);
    }
    return shares;
}

} // namespace

UhrwerkSimulation::UhrwerkSimulation() = default;
UhrwerkSimulation::~UhrwerkSimulation() = default;

const uhrwerk::World* UhrwerkSimulation::world() const {
    return simulation_ ? &simulation_->world() : nullptr;
}

void UhrwerkSimulation::build_lookups() {
    location_lookup_.clear();
    kreis_lookup_.clear();
    kreis_location_counts_.clear();
    market_location_counts_.clear();
    market_ports_.clear();
    market_colors_.clear();
    const uhrwerk::World* w = world();
    if (!w) {
        return;
    }
    location_lookup_.reserve(w->locations.size());
    for (std::size_t i = 0; i < w->locations.size(); ++i) {
        location_lookup_.emplace(w->locations[i].id, static_cast<std::int64_t>(i));
    }
    kreis_lookup_.reserve(w->districts.size());
    for (std::size_t i = 0; i < w->districts.size(); ++i) {
        kreis_lookup_.emplace(w->districts[i].id, static_cast<std::int64_t>(i));
    }
    kreis_location_counts_.assign(w->districts.size(), 0);
    market_location_counts_.assign(w->markets.size(), 0);
    market_ports_.assign(w->markets.size(), false);
    for (const uhrwerk::Location& loc : w->locations) {
        if (valid_index(kreis_location_counts_, loc.district)) {
            ++kreis_location_counts_[static_cast<std::size_t>(loc.district)];
        }
        // Märkte (seit M5): Nur Orte mit Daten rechnen; ein Hafen öffnet den Weltmarkt.
        if (loc.has_data() && valid_index(market_location_counts_, loc.market)) {
            ++market_location_counts_[static_cast<std::size_t>(loc.market)];
            if (loc.is_port) {
                market_ports_[static_cast<std::size_t>(loc.market)] = true;
            }
        }
    }
    // Farben für MAP_MAERKTE: die aktiven Märkte (mit Orten mit Daten) nacheinander durch die
    // Palette, damit sie sich unterscheiden; Märkte ohne Daten erscheinen ohnehin schraffiert.
    market_colors_.assign(w->markets.size(), 0);
    std::size_t active = 0;
    for (std::size_t m = 0; m < w->markets.size(); ++m) {
        market_colors_[m] = (market_location_counts_[m] > 0 ? active++ : m) % kKreisPalette.size();
    }
    // Wahlkreise je Parlament (seit M8): Inhalt, während der Partie unveränderlich.
    location_districts_.assign(w->parliaments.size(), std::vector<std::int32_t>(w->locations.size(), -1));
    district_positions_.assign(w->electoral_districts.size(), -1);
    std::vector<std::int64_t> next(w->parliaments.size(), 0);
    for (std::size_t d = 0; d < w->electoral_districts.size(); ++d) {
        const uhrwerk::ElectoralDistrict& district = w->electoral_districts[d];
        if (!valid_index(w->parliaments, district.parliament)) {
            continue;
        }
        const auto p = static_cast<std::size_t>(district.parliament);
        district_positions_[d] = next[p]++;
        for (const uhrwerk::Index location : district.locations) {
            if (valid_index(w->locations, location)) {
                location_districts_[p][static_cast<std::size_t>(location)] = static_cast<std::int32_t>(d);
            }
        }
    }
}

const uhrwerk::DistrictResult* UhrwerkSimulation::district_result(const uhrwerk::ElectionResult& election,
                                                                  std::int64_t district) const {
    if (!valid_index(district_positions_, district)) {
        return nullptr;
    }
    const std::int64_t position = at(district_positions_, district);
    if (!valid_index(election.districts, position) || at(election.districts, position).district != district) {
        return nullptr;
    }
    return &at(election.districts, position);
}

const uhrwerk::ElectionResult* UhrwerkSimulation::latest_election(std::int64_t parliament) const {
    const uhrwerk::World* w = world();
    if (!w) {
        return nullptr;
    }
    for (auto it = w->elections.rbegin(); it != w->elections.rend(); ++it) {
        if (it->parliament == parliament) {
            return &*it;
        }
    }
    return nullptr;
}

namespace {

/// Tag der Wahl, die die Anzeige meint (seit M13a3): die nächste ausstehende, sonst die letzte gehaltene,
/// sonst der erste Wahltag.
uhrwerk::Date shown_election_day(const uhrwerk::World& w, std::int64_t parliament) {
    const auto index = static_cast<uhrwerk::Index>(parliament);
    if (const std::optional<uhrwerk::Date> next = uhrwerk::next_election_day(w, index)) {
        return *next;
    }
    for (auto it = w.elections.rbegin(); it != w.elections.rend(); ++it) {
        if (it->parliament == index) {
            return it->day;
        }
    }
    return w.parliaments[static_cast<std::size_t>(parliament)].election_day;
}

} // namespace

std::int64_t UhrwerkSimulation::next_parliament() const {
    const uhrwerk::World* w = world();
    std::int64_t next = -1;
    std::optional<uhrwerk::Date> next_day;
    if (!w) {
        return next;
    }
    for (std::size_t p = 0; p < w->parliaments.size(); ++p) {
        // Seit M13a3 der nächste ausstehende Wahltag (weitere Wahlen, Wahlen eines Bundes).
        const std::optional<uhrwerk::Date> day =
            uhrwerk::next_election_day(*w, static_cast<uhrwerk::Index>(p));
        if (day && (!next_day || *day < *next_day)) {
            next = static_cast<std::int64_t>(p);
            next_day = day;
        }
    }
    return next;
}

bool UhrwerkSimulation::campaign_possible(std::int64_t current, std::int64_t district,
                                          uhrwerk::CampaignAction action) const {
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->currents, current) || !valid_index(w->electoral_districts, district) ||
        !uhrwerk::campaign_allowed(*w, static_cast<uhrwerk::Index>(current),
                                   static_cast<uhrwerk::Index>(district), action)) {
        return false;
    }
    // Die Kosten eingereihter Befehle sinken erst am nächsten Tag von der Kasse (Review M8).
    const uhrwerk::Fixed reserved =
        simulation_->reserved_campaign_funds(static_cast<uhrwerk::Index>(current));
    return at(w->party_funds, current) - reserved >=
           w->election_rules.campaign[static_cast<std::size_t>(action)].cost;
}

String UhrwerkSimulation::load_scenario(const String& data_path, const String& scenario_id,
                                        std::int64_t seed) {
    simulation_.reset();
    load_warnings_.clear();
    build_lookups();

    uhrwerk::LoadResult result = uhrwerk::load_world(to_path(data_path), to_std(scenario_id));

    String errors;
    for (const uhrwerk::LoadIssue& issue : result.issues) {
        if (issue.fatal) {
            if (!errors.is_empty()) {
                errors += "\n";
            }
            errors += to_godot(issue.to_string());
        } else {
            load_warnings_.push_back(to_godot(issue.to_string()));
        }
    }
    if (!result.ok()) {
        if (errors.is_empty()) {
            errors = "Die Daten konnten nicht geladen werden.";
        }
        return errors;
    }

    uhrwerk::SimConfig config;
    config.seed = static_cast<std::uint64_t>(seed);
    simulation_ = std::make_unique<uhrwerk::Simulation>(std::move(*result.world), config);
    player_tag_ = simulation_->world().scenario.player_tag;
    player_current_.clear();
    content_ = uhrwerk::content_fingerprint(simulation_->world());
    build_lookups();
    return String();
}

bool UhrwerkSimulation::is_loaded() const {
    return simulation_ != nullptr;
}

PackedStringArray UhrwerkSimulation::get_load_warnings() const {
    return load_warnings_;
}

void UhrwerkSimulation::advance_days(std::int64_t days) {
    ERR_FAIL_COND_MSG(!simulation_, "Keine Simulation geladen.");
    ERR_FAIL_COND_MSG(days < 0 || days > kMaxAdvanceDays,
                      "advance_days: Tage müssen zwischen 0 und 3660 liegen.");
    simulation_->step_days(days);
}

Dictionary UhrwerkSimulation::get_date() const {
    Dictionary result;
    if (const uhrwerk::World* w = world()) {
        result["year"] = w->date.year;
        result["month"] = w->date.month;
        result["day"] = w->date.day;
    }
    return result;
}

String UhrwerkSimulation::get_date_text() const {
    const uhrwerk::World* w = world();
    return w ? to_godot(uhrwerk::format_date_long(w->date)) : String();
}

std::int64_t UhrwerkSimulation::get_days_elapsed() const {
    return simulation_ ? simulation_->days_elapsed() : 0;
}

String UhrwerkSimulation::get_scenario_name() const {
    const uhrwerk::World* w = world();
    return w ? to_godot(w->scenario.name) : String();
}

std::int64_t UhrwerkSimulation::get_player_state_index() const {
    const uhrwerk::World* w = world();
    return w ? w->find_state(player_tag_) : uhrwerk::kNone;
}

namespace {

/// Phase der Mobilmachung als Text (seit M10, b3).
String mobilization_text(uhrwerk::MobilizationPhase phase) {
    switch (phase) {
    case uhrwerk::MobilizationPhase::Peace:
        return to_godot("Frieden");
    case uhrwerk::MobilizationPhase::Mobilizing:
        return to_godot("macht mobil");
    case uhrwerk::MobilizationPhase::Mobilized:
        return to_godot("mobil");
    case uhrwerk::MobilizationPhase::Demobilizing:
        return to_godot("rüstet ab");
    }
    return String();
}

Dictionary scenario_to_godot(const uhrwerk::Scenario& scenario) {
    Dictionary result;
    result["id"] = to_godot(scenario.id);
    result["name"] = to_godot(scenario.name);
    result["date_text"] = to_godot(uhrwerk::format_date_long(scenario.start));
    result["year"] = scenario.start.year;
    result["player_tag"] = to_godot(scenario.player_tag);
    PackedStringArray playable;
    for (const std::string& tag : scenario.playable) {
        playable.push_back(to_godot(tag));
    }
    result["playable"] = playable;
    result["currency"] = to_godot(scenario.currency.empty() ? std::string("Reichstaler") : scenario.currency);
    return result;
}

} // namespace

Array UhrwerkSimulation::get_scenarios(const String& data_path) const {
    std::vector<uhrwerk::LoadIssue> issues;
    Array result;
    for (const uhrwerk::Scenario& scenario : uhrwerk::list_scenarios(to_path(data_path), issues)) {
        result.push_back(scenario_to_godot(scenario));
    }
    return result;
}

Dictionary UhrwerkSimulation::get_scenario() const {
    const uhrwerk::World* w = world();
    return w ? scenario_to_godot(w->scenario) : Dictionary();
}

bool UhrwerkSimulation::set_player_state(std::int64_t state_index) {
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->states, state_index)) {
        return false;
    }
    // Nur die Wahl der Oberfläche (Spielstand, Panels); den Spielerstaat im Kern meldet main.gd über
    // set_event_player (Befehl Spielerstaat, seit M11; im Parteienmodus keiner).
    player_tag_ = at(w->states, state_index).tag;
    return true;
}

std::int64_t UhrwerkSimulation::get_state_count() const {
    const uhrwerk::World* w = world();
    return w ? static_cast<std::int64_t>(w->states.size()) : 0;
}

std::int64_t UhrwerkSimulation::get_state_index(const String& tag) const {
    const uhrwerk::World* w = world();
    return w ? w->find_state(to_std(tag)) : uhrwerk::kNone;
}

Dictionary UhrwerkSimulation::get_state(std::int64_t index) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->states, index)) {
        return result;
    }
    const uhrwerk::State& state = at(w->states, index);
    const auto state_index = static_cast<uhrwerk::Index>(index);
    result["tag"] = to_godot(state.tag);
    result["name"] = to_godot(state.name);
    result["short_name"] = to_godot(state.short_name);
    result["color"] = to_godot(state.color);
    result["ruler"] = to_godot(state.ruler);
    result["government"] = to_godot(state.government);
    result["capital_index"] = state.capital;
    // Zollunion (seit M9): Kennung und Name, leer für ein eigenes Zollgebiet; seit Schritt a4 Index,
    // Leitmacht, Tarif des Zollgebiets, Anteil an der Vereinskasse und offener Antrag.
    const bool in_union = state.customs_union != uhrwerk::kNone;
    result["customs_union"] = in_union ? to_godot(at(w->customs_unions, state.customs_union).id) : String();
    result["customs_union_name"] =
        in_union ? to_godot(at(w->customs_unions, state.customs_union).name) : String();
    result["customs_union_index"] = state.customs_union;
    result["customs_leader"] = in_union && at(w->customs_unions, state.customs_union).leader == state_index;
    const bool zones = !w->customs_unions.empty();
    result["customs_tariff_percent"] =
        (zones ? uhrwerk::zone_tariff(*w, uhrwerk::customs_zone(*w, state_index))
               : state.tax_rate(uhrwerk::TaxKind::Zoll))
            .to_double() *
        100.0;
    result["customs_locked"] = zones && in_union;
    result["customs_income"] = state.customs_income.to_double();
    Dictionary request;
    for (const uhrwerk::CustomsRequest& r : w->customs_requests) {
        if (r.state == state_index) {
            request["customs_union_index"] = r.customs_union;
            request["customs_union_name"] = r.customs_union != uhrwerk::kNone
                                                ? to_godot(at(w->customs_unions, r.customs_union).name)
                                                : String();
            request["effective_text"] =
                to_godot(uhrwerk::format_date_long(uhrwerk::customs_effective_date(w->date)));
            // Verbindlich nach einem Bundesbeschluss oder einer verlorenen Krise (Review M9).
            request["binding"] =
                uhrwerk::customs_request_binding(*w, static_cast<uhrwerk::Index>(state_index));
        }
    }
    result["customs_request"] = request;
    // Diplomatie (seit M9, b3): gilt sie, und was kosten die Gesandten (Haushalt)?
    result["diplomacy"] = !w->relations.empty();
    result["envoy_cost_month"] = state.budget.diplomacy.to_double();
    result["treasury"] = state.treasury.to_double();
    result["tax_rate_percent"] = to_percent(state.tax_rate(uhrwerk::TaxKind::Kontribution));
    // Haushalt des letzten Rechentags (gleich last_month_*); vor dem ersten Rechentag die Schätzung
    // aus dem Start (estimate_budgets) statt 0.
    result["income_month"] = state.budget.income().to_double();
    result["hinterland_month"] = state.budget.hinterland.to_double(); // seit dem Review M10
    result["expenses_month"] = state.budget.expenses().to_double();
    result["population"] = w->state_population(state_index).round();
    const std::vector<uhrwerk::Index> locations = w->state_locations(state_index);
    result["location_count"] = static_cast<std::int64_t>(locations.size());
    // Mitglied des Deutschen Bundes (seit M7): mindestens ein Ort im Bundesgebiet; seit M13a2 Mitglied
    // des Bundes, der gilt (auch eines Bundes zur Laufzeit).
    result["bund"] = uhrwerk::federal_state(*w, static_cast<uhrwerk::Index>(state_index));
    // Bilanz des Vorjahres und mittlerer Lebensstandard (seit M4).
    uhrwerk::PopulationBalance balance;
    double living = 0.0;
    double people = 0.0;
    for (const uhrwerk::Index li : locations) {
        if (valid_index(w->balance_previous, li)) {
            add_balance(balance, at(w->balance_previous, li));
        }
        const People p = people_of(*w, li);
        living += p.living_standard * p.total.to_double();
        people += p.total.to_double();
    }
    result["balance_previous"] = balance_to_godot(balance);
    result["living_standard"] = people > 0.0 ? living / people : 0.0;

    // Haushalt (seit M5): Sätze und Steuern je Art, Ausgaben je Posten (Monat), Schulden, Zins, Heer.
    const uhrwerk::StateBudget& budget = state.budget;
    Array rates;
    Array taxes;
    for (std::size_t k = 0; k < uhrwerk::kTaxKinds; ++k) {
        rates.push_back(to_percent(state.tax_rates[k]));
        taxes.push_back(budget.taxes[k].to_double());
    }
    result["tax_rates_percent"] = rates;
    result["taxes_month"] = taxes;
    result["court_month"] = budget.court.to_double();
    result["administration_month"] = budget.administration.to_double();
    result["army_month"] = budget.army.to_double();
    result["interest_month"] = budget.interest.to_double();
    result["building_month"] = budget.building.to_double();
    result["debt"] = state.debt.to_double();
    result["interest_percent"] = state.interest_rate.to_double() * 100.0;
    result["yearly_income"] = state.yearly_income.to_double();
    result["army_per_mille"] = state.army_per_mille.to_double();
    result["soldiers"] =
        (state.army_per_mille * w->state_population(state_index) / uhrwerk::Fixed::from_int(1000)).round();
    result["bankruptcies"] = static_cast<std::int64_t>(state.bankruptcies);
    result["premium_months"] = static_cast<std::int64_t>(state.premium_months);
    result["politik"] = !w->parliaments.empty(); // seit M8: get_state_politics hat Angaben
    // Militär (seit M10, b3): Mobilmachung, Krieg und Soldaten unter Waffen.
    result["military"] = uhrwerk::military_active(*w);
    const uhrwerk::MobilizationPhase phase = uhrwerk::mobilization_phase(*w, state_index);
    result["mobilization"] = static_cast<std::int64_t>(phase);
    result["mobilization_text"] = mobilization_text(phase);
    result["in_war"] = uhrwerk::in_war(*w, state_index);
    result["soldiers_under_arms"] = uhrwerk::state_soldiers(*w, state_index).round();
    result["peace_strength"] = uhrwerk::peace_strength(*w, state_index).round();
    return result;
}

std::int64_t UhrwerkSimulation::get_location_count() const {
    const uhrwerk::World* w = world();
    return w ? static_cast<std::int64_t>(w->locations.size()) : 0;
}

std::int64_t UhrwerkSimulation::get_location_index(const String& id) const {
    return world() ? find_in(location_lookup_, id) : uhrwerk::kNone;
}

Dictionary UhrwerkSimulation::get_location(std::int64_t index) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->locations, index)) {
        return result;
    }
    const uhrwerk::Location& loc = at(w->locations, index);
    const auto location_index = static_cast<uhrwerk::Index>(index);
    const uhrwerk::Fixed population = w->location_population(location_index);

    result["id"] = to_godot(loc.id);
    result["name"] = to_godot(loc.name);
    // Orte ohne Daten (seit M3): kein Besitzer, keine Blöcke, Simulationszahlen 0; Kartenfelder
    // (Gelände, Stadt, Fläche, Kreis) wie in den Daten.
    result["has_data"] = loc.has_data();
    result["kreis_index"] = valid_index(w->districts, loc.district) ? loc.district : uhrwerk::kNone;
    result["kreis_name"] =
        valid_index(w->districts, loc.district) ? to_godot(at(w->districts, loc.district).name) : String();
    result["owner_index"] = loc.owner;
    result["owner_name"] =
        valid_index(w->states, loc.owner) ? to_godot(at(w->states, loc.owner).name) : String();
    // Verwaltung (seit M7): Kette der Einheiten von oben nach unten, z. B. Provinz, Regierungsbezirk,
    // Kreis; leer ohne Zuordnung. Bundesgebiet: liegt im Deutschen Bund.
    Array admin_chain;
    for (uhrwerk::Index a = loc.admin; valid_index(w->admin_units, a); a = at(w->admin_units, a).parent) {
        const uhrwerk::AdminUnit& unit = at(w->admin_units, a);
        Dictionary entry;
        entry["index"] = a;
        entry["id"] = to_godot(unit.id);
        entry["name"] = to_godot(unit.name);
        entry["kind"] = to_godot(unit.kind);
        admin_chain.push_front(entry);
        if (admin_chain.size() > static_cast<std::int64_t>(w->admin_units.size())) {
            break; // Der Lader schließt Zyklen aus; Schutz gegen eine Endlosschleife.
        }
    }
    result["verwaltung"] = admin_chain;
    result["bundesgebiet"] = uhrwerk::in_federal_area(*w, static_cast<uhrwerk::Index>(location_index));
    // Strecken mit einem eröffneten Abschnitt an diesem Ort (seit M7), mit dem Jahr der Eröffnung.
    PackedStringArray railways;
    for (const uhrwerk::Railway& railway : w->railways) {
        const uhrwerk::RailSection* first = nullptr;
        for (const uhrwerk::RailSection& section : railway.sections) {
            if ((section.from == index || section.to == index) && section.opened <= w->date &&
                (first == nullptr || section.opened < first->opened)) {
                first = &section;
            }
        }
        if (first != nullptr) {
            // „Name (1839)“; trägt der Name schon eine Klammer, kommt das Jahr hinein: „Ludwigsbahn
            // (Nürnberg–Fürth, 1835)“.
            const std::string year = std::to_string(first->opened.year);
            const std::string& name = railway.name;
            railways.append(to_godot(!name.empty() && name.back() == ')'
                                         ? name.substr(0, name.size() - 1) + ", " + year + ")"
                                         : name + " (" + year + ")"));
        }
    }
    result["eisenbahnen"] = railways;
    // Zollgebiet des Ortes (seit M9): Union oder Besitzer, mit Tarif.
    String zone_name;
    double zone_tariff = 0.0;
    if (valid_index(w->states, loc.owner)) {
        const uhrwerk::State& owner = at(w->states, loc.owner);
        zone_name = to_godot(valid_index(w->customs_unions, owner.customs_union)
                                 ? at(w->customs_unions, owner.customs_union).name
                                 : owner.short_name);
        zone_tariff =
            (w->customs_unions.empty() ? owner.tax_rate(uhrwerk::TaxKind::Zoll)
                                       : uhrwerk::zone_tariff(*w, uhrwerk::customs_zone(*w, loc.owner)))
                .to_double() *
            100.0;
    }
    result["customs_zone_name"] = zone_name;
    result["customs_tariff_percent"] = zone_tariff;
    // Wahlkreis des angezeigten Parlaments mit dem Ergebnis seiner letzten Wahl (seit M8).
    const std::int64_t parliament = get_shown_parliament();
    if (valid_index(location_districts_, parliament) &&
        at(location_districts_, parliament)[static_cast<std::size_t>(index)] >= 0) {
        const std::int32_t d = at(location_districts_, parliament)[static_cast<std::size_t>(index)];
        const uhrwerk::ElectoralDistrict& district = at(w->electoral_districts, d);
        Dictionary election;
        election["parliament_index"] = parliament;
        election["parliament_name"] = to_godot(at(w->parliaments, parliament).name);
        election["district_index"] = d;
        election["district_name"] = to_godot(district.name);
        election["seats"] = district.seats;
        election["election_date_text"] =
            to_godot(uhrwerk::format_date_long(shown_election_day(*w, parliament)));
        // Seit M13a3: ob der Wahlkreis bei der nächsten Wahl mitwählt (Landtag: Staat; Bund: Bundesgebiet).
        election["pending"] =
            uhrwerk::next_election_day(*w, static_cast<uhrwerk::Index>(parliament)).has_value() &&
            uhrwerk::district_votes(*w, static_cast<uhrwerk::Index>(d));
        const uhrwerk::ElectionResult* latest = latest_election(parliament);
        const uhrwerk::DistrictResult* r = latest != nullptr ? district_result(*latest, d) : nullptr;
        election["held"] = r != nullptr;
        if (r != nullptr) {
            election["boycott"] = r->boycott;
            election["runoff"] = r->runoff;
            election["electorate"] = r->electorate.round();
            election["turnout_percent"] = r->electorate > uhrwerk::Fixed::zero()
                                              ? (r->voters / r->electorate).to_double() * 100.0
                                              : 0.0;
            election["winner"] = r->winner;
            election["winner_name"] =
                valid_index(w->currents, r->winner) ? to_godot(at(w->currents, r->winner).name) : String();
            election["winner_short_name"] = valid_index(w->currents, r->winner)
                                                ? to_godot(at(w->currents, r->winner).short_name)
                                                : String();
            const PackedFloat64Array shares = vote_shares(*r);
            Dictionary by_name;
            Dictionary colors;
            for (std::size_t c = 0; c < w->currents.size() && c < static_cast<std::size_t>(shares.size());
                 ++c) {
                const String name = to_godot(w->currents[c].short_name); // Balken der Ortsinfo
                by_name[name] = shares[static_cast<std::int64_t>(c)] * 100.0;
                colors[name] = to_godot(w->currents[c].color);
            }
            election["shares"] = by_name; // Kurzname der Strömung → Prozent
            election["colors"] = colors;  // Strömung → Farbe
        }
        result["wahl"] = election;
    }
    // Wahlkreis der nächsten ausstehenden Wahl (Parteienmodus), -1 ohne (Review M8: nach der
    // Nationalversammlung führt man Wahlkampf zur Preußischen Zweiten Kammer).
    const std::int64_t next = next_parliament();
    result["campaign_district"] = valid_index(location_districts_, next)
                                      ? at(location_districts_, next)[static_cast<std::size_t>(index)]
                                      : -1;
    result["terrain"] =
        valid_index(w->terrains, loc.terrain) ? to_godot(at(w->terrains, loc.terrain).name) : String();
    result["is_city"] = loc.is_city;
    result["population"] = population.round();
    result["area_km2"] = loc.area_km2.to_double();
    result["density"] = density_of(*w, index);
    result["devastation_percent"] = to_percent(loc.devastation);

    const uhrwerk::Index majority = w->location_majority_confession(location_index);
    result["majority_confession"] =
        valid_index(w->confessions, majority) ? to_godot(at(w->confessions, majority).name) : String();

    // Anteile je Konfession und Kultur in Prozent (Blöcke und Zugänge).
    const People people = people_of(*w, index);
    const double total = people.total.to_double();
    result["confessions"] = shares_of(people.per_confession, w->confessions, total);

    // Bevölkerung (seit M4): Kultur, Altersaufbau, Lebensstandard, Bildung, Bilanz des Vorjahres.
    const uhrwerk::Index culture = w->location_majority_culture(location_index);
    result["majority_culture"] =
        valid_index(w->cultures, culture) ? to_godot(at(w->cultures, culture).name) : String();
    result["cultures"] = shares_of(people.per_culture, w->cultures, total);
    result["children"] = people.children.round();
    result["young"] = people.young.round();
    result["working"] = people.working.round();
    result["old"] = people.old.round();
    result["living_standard"] = people.living_standard;
    result["literacy_percent"] = people.literacy * 100.0;
    result["balance_previous"] = valid_index(w->balance_previous, index)
                                     ? balance_to_godot(at(w->balance_previous, index))
                                     : balance_to_godot(uhrwerk::PopulationBalance{});

    const uhrwerk::LocationEconomy economy = simulation_->location_economy(location_index);
    result["production_month"] = economy.production.to_double();
    result["taxes_month"] = economy.total_taxes().to_double();
    result["production_per_capita"] = production_per_capita(*w, index);
    result["source"] = to_godot(loc.source);
    add_location_economy(*w, index, result);
    return result;
}

PackedInt32Array UhrwerkSimulation::get_location_owner_indices() const {
    PackedInt32Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    const auto count = static_cast<std::int64_t>(w->locations.size());
    result.resize(count);
    for (std::int64_t i = 0; i < count; ++i) {
        const uhrwerk::Index owner = at(w->locations, i).owner;
        result.set(i, valid_index(w->states, owner) ? owner : uhrwerk::kNone);
    }
    return result;
}

PackedInt32Array UhrwerkSimulation::get_location_kreis_indices() const {
    PackedInt32Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    const auto count = static_cast<std::int64_t>(w->locations.size());
    result.resize(count);
    for (std::int64_t i = 0; i < count; ++i) {
        const uhrwerk::Index district = at(w->locations, i).district;
        result.set(i, valid_index(w->districts, district) ? district : uhrwerk::kNone);
    }
    return result;
}

PackedInt32Array UhrwerkSimulation::get_location_admin_indices() const {
    PackedInt32Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    const auto count = static_cast<std::int64_t>(w->locations.size());
    result.resize(count);
    for (std::int64_t i = 0; i < count; ++i) {
        const uhrwerk::Index admin = at(w->locations, i).admin;
        result.set(i, valid_index(w->admin_units, admin) ? admin : uhrwerk::kNone);
    }
    return result;
}

std::int64_t UhrwerkSimulation::get_admin_unit_count() const {
    const uhrwerk::World* w = world();
    return w ? static_cast<std::int64_t>(w->admin_units.size()) : 0;
}

Dictionary UhrwerkSimulation::get_admin_unit(std::int64_t index) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->admin_units, index)) {
        return result;
    }
    const uhrwerk::AdminUnit& unit = at(w->admin_units, index);
    result["id"] = to_godot(unit.id);
    result["name"] = to_godot(unit.name);
    result["kind"] = to_godot(unit.kind);
    result["parent_index"] = valid_index(w->admin_units, unit.parent) ? unit.parent : uhrwerk::kNone;
    result["source"] = to_godot(unit.source);
    std::int64_t locations = 0;
    for (const uhrwerk::Location& loc : w->locations) {
        locations += loc.admin == static_cast<uhrwerk::Index>(index) ? 1 : 0;
    }
    result["location_count"] = locations;
    return result;
}

PackedByteArray UhrwerkSimulation::get_rail_access() const {
    PackedByteArray result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    const std::vector<std::uint8_t> access = rail_access(*w, simulation_->economy_topology());
    result.resize(static_cast<std::int64_t>(access.size()));
    for (std::size_t i = 0; i < access.size(); ++i) {
        result[static_cast<std::int64_t>(i)] = access[i];
    }
    return result;
}

Array UhrwerkSimulation::get_railways() const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    for (const uhrwerk::Railway& railway : w->railways) {
        Dictionary line;
        line["id"] = to_godot(railway.id);
        line["name"] = to_godot(railway.name);
        line["source"] = to_godot(railway.source);
        Array sections;
        for (const uhrwerk::RailSection& section : railway.sections) {
            Dictionary entry;
            entry["from"] = section.from;
            entry["to"] = section.to;
            Dictionary opened;
            opened["year"] = section.opened.year;
            opened["month"] = section.opened.month;
            opened["day"] = section.opened.day;
            entry["opened"] = opened;
            entry["open"] = section.opened <= w->date;
            sections.append(entry);
        }
        line["sections"] = sections;
        result.append(line);
    }
    return result;
}

PackedInt32Array UhrwerkSimulation::get_location_market_indices() const {
    PackedInt32Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    const auto count = static_cast<std::int64_t>(w->locations.size());
    result.resize(count);
    for (std::int64_t i = 0; i < count; ++i) {
        const uhrwerk::Index market = at(w->locations, i).market;
        result.set(i, valid_index(w->markets, market) ? market : uhrwerk::kNone);
    }
    return result;
}

std::int64_t UhrwerkSimulation::get_market_count() const {
    const uhrwerk::World* w = world();
    return w ? static_cast<std::int64_t>(w->markets.size()) : 0;
}

Dictionary UhrwerkSimulation::get_market(std::int64_t index) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->markets, index)) {
        return result;
    }
    const uhrwerk::Market& market = at(w->markets, index);
    result["id"] = to_godot(market.id);
    result["name"] = to_godot(market.name);
    const std::int64_t locations =
        valid_index(market_location_counts_, index) ? at(market_location_counts_, index) : std::int64_t{0};
    result["location_count"] = locations;
    result["active"] = locations > 0;
    result["is_port"] = valid_index(market_ports_, index) && market_ports_[static_cast<std::size_t>(index)];
    Array goods;
    if (valid_index(w->market_states, index)) {
        const uhrwerk::MarketState& state = at(w->market_states, index);
        result["harvest"] = state.harvest.to_double();
        for (std::size_t g = 0; g < w->goods.size() && g < state.price.size(); ++g) {
            const uhrwerk::Good& good = w->goods[g];
            const double base = good.base_price.to_double();
            const double price = state.price[g].to_double();
            Dictionary row;
            row["id"] = to_godot(good.id);
            row["name"] = to_godot(good.name);
            row["color"] = to_godot(good.color);
            row["base_price"] = base;
            row["price"] = price;
            row["price_factor"] = base > 0.0 ? price / base : 0.0;
            row["supply"] = state.supply[g].to_double();
            row["demand"] = state.demand[g].to_double();
            row["imports"] = state.imports[g].to_double();
            row["exports"] = state.exports[g].to_double();
            row["buy_percent"] = state.buy_ratio[g].to_double() * 100.0;
            goods.push_back(row);
        }
    } else {
        result["harvest"] = 1.0;
    }
    result["goods"] = goods;
    return result;
}

std::int64_t UhrwerkSimulation::get_kreis_count() const {
    const uhrwerk::World* w = world();
    return w ? static_cast<std::int64_t>(w->districts.size()) : 0;
}

std::int64_t UhrwerkSimulation::get_kreis_index(const String& id) const {
    return world() ? find_in(kreis_lookup_, id) : uhrwerk::kNone;
}

Dictionary UhrwerkSimulation::get_kreis(std::int64_t index) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->districts, index)) {
        return result;
    }
    const uhrwerk::District& district = at(w->districts, index);
    result["id"] = to_godot(district.id);
    result["name"] = to_godot(district.name);
    result["location_count"] =
        valid_index(kreis_location_counts_, index) ? at(kreis_location_counts_, index) : std::int64_t{0};
    result["source"] = to_godot(district.source);
    return result;
}

PackedColorArray UhrwerkSimulation::get_map_colors(std::int64_t mode) const {
    PackedColorArray colors;
    const uhrwerk::World* w = world();
    if (!w) {
        return colors;
    }
    ERR_FAIL_COND_V_MSG(mode < MAP_POLITICAL || mode > MAP_BUND, colors,
                        "get_map_colors: unbekannter Kartenmodus.");

    const auto count = static_cast<std::int64_t>(w->locations.size());
    const std::vector<std::uint8_t> rail = mode == MAP_EISENBAHNEN
                                               ? rail_access(*w, simulation_->economy_topology())
                                               : std::vector<std::uint8_t>{};
    // Wahlen (seit M8): das angezeigte Parlament und seine letzte Wahl.
    const std::int64_t parliament = mode == MAP_WAHLEN ? get_shown_parliament() : -1;
    const uhrwerk::ElectionResult* election = parliament >= 0 ? latest_election(parliament) : nullptr;
    // Beziehungen (seit M9, b3): zum Spielerstaat.
    const std::int64_t player = mode == MAP_BEZIEHUNGEN ? get_player_state_index() : -1;
    colors.resize(count);
    for (std::int64_t i = 0; i < count; ++i) {
        const uhrwerk::Location& loc = at(w->locations, i);
        Color color = kUnknown;
        switch (mode) {
        case MAP_POLITICAL:
            if (valid_index(w->states, loc.owner)) {
                color = to_godot(at(w->states, loc.owner).color);
            }
            break;
        case MAP_DENSITY:
            color = blend(kDensityLow, kDensityHigh, density_of(*w, i) / kDensityMax);
            break;
        case MAP_CONFESSION: {
            const uhrwerk::Index majority = w->location_majority_confession(static_cast<uhrwerk::Index>(i));
            if (valid_index(w->confessions, majority)) {
                color = to_godot(at(w->confessions, majority).color);
            }
            break;
        }
        case MAP_TERRAIN:
            if (valid_index(w->terrains, loc.terrain)) {
                color = to_godot(at(w->terrains, loc.terrain).color);
            }
            break;
        case MAP_DEVASTATION:
            color = blend(kDevastationNone, kDevastationFull, loc.devastation.to_double());
            break;
        case MAP_KREISE:
            if (valid_index(w->districts, loc.district)) {
                color = kKreisPalette[static_cast<std::size_t>(loc.district) % kKreisPalette.size()];
            }
            break;
        case MAP_LEBENSSTANDARD:
            if (loc.has_data()) {
                color = blend(kLivingLow, kLivingHigh, people_of(*w, i).living_standard / 100.0);
            }
            break;
        case MAP_KULTUR: {
            const uhrwerk::Index majority = w->location_majority_culture(static_cast<uhrwerk::Index>(i));
            if (valid_index(w->cultures, majority)) {
                color = to_godot(at(w->cultures, majority).color);
            }
            break;
        }
        case MAP_WIRTSCHAFT:
            if (loc.has_data()) {
                color = blend(kEconomyLow, kEconomyHigh, production_per_capita(*w, i) / kEconomyMax);
            }
            break;
        case MAP_MAERKTE:
            if (valid_index(market_colors_, loc.market)) {
                color = kKreisPalette[at(market_colors_, loc.market)];
            }
            break;
        case MAP_VERWALTUNG:
            // Oberste Verwaltungseinheit (Provinz, Kronland …) aus der Palette, die Grenzen der
            // untersten zeichnet Godot; Staaten ohne Gliederung in aufgehellter Staatsfarbe (seit M7).
            if (valid_index(w->admin_units, loc.admin)) {
                uhrwerk::Index top = loc.admin;
                for (std::size_t step = 0; step < w->admin_units.size(); ++step) {
                    const uhrwerk::Index parent = at(w->admin_units, top).parent;
                    if (!valid_index(w->admin_units, parent)) {
                        break;
                    }
                    top = parent;
                }
                color = kKreisPalette[static_cast<std::size_t>(top) % kKreisPalette.size()];
            } else if (valid_index(w->states, loc.owner)) {
                color = blend(to_godot(at(w->states, loc.owner).color), Color(1.0f, 1.0f, 1.0f), 0.45);
            }
            break;
        case MAP_EISENBAHNEN: {
            const std::uint8_t access = rail[static_cast<std::size_t>(i)];
            color = access == 2 ? kRailStation : (access == 1 ? kRailMarket : kRailNone);
            break;
        }
        case MAP_WAHLEN: {
            // Farbe der siegreichen Strömung, kräftiger bei klarerem Sieg (Anteil 25 % hell bis 70 %
            // voll); vor der Wahl die Wahlkreise aus der Palette, außerhalb des Wahlgebiets Papier.
            const std::int32_t district =
                valid_index(location_districts_, parliament)
                    ? at(location_districts_, parliament)[static_cast<std::size_t>(i)]
                    : -1;
            if (district < 0) {
                color = kElectionOutside;
            } else if (election == nullptr) {
                color = blend(kKreisPalette[static_cast<std::size_t>(district) % kKreisPalette.size()],
                              Color(1.0f, 1.0f, 1.0f), 0.45);
            } else if (const uhrwerk::DistrictResult* r = district_result(*election, district)) {
                if (r->boycott) {
                    color = kElectionBoycott;
                } else if (valid_index(w->currents, r->winner)) {
                    double total = 0.0;
                    for (const uhrwerk::Fixed v : r->votes) {
                        total += v.to_double();
                    }
                    const double share = total > 0.0 ? at(r->votes, r->winner).to_double() / total : 0.0;
                    const double strength = std::clamp((share - 0.25) / 0.45, 0.0, 1.0);
                    color = blend(kElectionPaper, to_godot(at(w->currents, r->winner).color),
                                  0.35 + 0.65 * strength);
                }
            }
            break;
        }
        case MAP_ZOLLGEBIETE:
            // Zollunionen in ihrer Farbe, Staaten ohne Union blass in der Staatsfarbe (seit M9).
            if (valid_index(w->states, loc.owner)) {
                const uhrwerk::State& owner = at(w->states, loc.owner);
                color = valid_index(w->customs_unions, owner.customs_union)
                            ? to_godot(at(w->customs_unions, owner.customs_union).color)
                            : blend(to_godot(owner.color), Color(1.0f, 1.0f, 1.0f), kCustomsAlone);
            }
            break;
        case MAP_BEZIEHUNGEN:
            // Beziehung des Besitzers zum Spielerstaat: rot über Papier bis grün; der Spielerstaat in
            // seiner Farbe. Ohne Diplomatie (1648) alles Papier.
            if (valid_index(w->states, loc.owner)) {
                if (loc.owner == player) {
                    color = to_godot(at(w->states, loc.owner).color);
                } else {
                    const double r =
                        uhrwerk::relation(*w, static_cast<uhrwerk::Index>(player), loc.owner).to_double();
                    color = r >= 0.0 ? blend(kRelationNeutral, kRelationFriend, r / kRelationScale)
                                     : blend(kRelationNeutral, kRelationFoe, -r / kRelationScale);
                }
            }
            break;
        case MAP_KRIEG: {
            // Krieg (seit M10, b3): Farbe dessen, der den Ort hält; Staaten im Krieg kräftig, die übrigen
            // blass. Besetzte Orte schraffiert Godot nach get_location_control, die Fronten sind die
            // Grenzen der Kontrolle.
            const uhrwerk::Index holder = uhrwerk::controller(*w, static_cast<uhrwerk::Index>(i));
            if (valid_index(w->states, holder)) {
                color = to_godot(at(w->states, holder).color);
                if (!uhrwerk::in_war(*w, holder)) {
                    color = blend(color, kRelationNeutral, kWarNeutral);
                }
            }
            break;
        }
        case MAP_BUND:
            // Bund (seit M13a2, vertical_slice.md 3.3): das Bundesgebiet, das gilt, in der Farbe der
            // Präsidialmacht (blass), sie selbst kräftig; außerhalb Papier.
            if (uhrwerk::in_federal_area(*w, static_cast<uhrwerk::Index>(i)) &&
                valid_index(w->states, loc.owner) && uhrwerk::federal_state(*w, loc.owner)) {
                const uhrwerk::Index presidency = w->diet_presidency;
                const Color lead = valid_index(w->states, presidency)
                                       ? to_godot(at(w->states, presidency).color)
                                       : kRelationFriend;
                color = loc.owner == presidency ? lead : blend(lead, kRelationNeutral, 0.55);
            }
            break;
        case MAP_UNRUHE:
            // Unruhe (seit M11, b4) des Besitzers von Papier (0) bis Rot (100); Staaten mit laufendem
            // Aufstand in Violett. Ohne Ereignisse (1648) alles Papier.
            if (valid_index(w->states, loc.owner)) {
                const double u =
                    valid_index(w->unrest, loc.owner) ? at(w->unrest, loc.owner).to_double() / 100.0 : 0.0;
                color = blend(kRelationNeutral, kRelationFoe, std::clamp(u, 0.0, 1.0));
                if (std::any_of(w->revolts.begin(), w->revolts.end(),
                                [&](const uhrwerk::Revolt& r) { return r.state == loc.owner; })) {
                    color = blend(color, kRevoltColor, 0.6);
                }
            }
            break;
        default:
            break;
        }
        // Orte ohne Daten: in den datenabhängigen Modi Alpha 0, die Godot-Seite zeichnet dort
        // eine Schraffur „keine Daten“. Gelände und Kreise sind auch ohne Daten bekannt, Märkte
        // dort, wo der Markt Orte mit Daten hat (sonst rechnet er nicht).
        bool known = mode == MAP_TERRAIN || mode == MAP_KREISE;
        if (mode == MAP_MAERKTE) {
            known = valid_index(market_location_counts_, loc.market) &&
                    at(market_location_counts_, loc.market) > 0;
        }
        if (!loc.has_data() && !known) {
            color.a = 0.0f;
        }
        colors.set(i, color);
    }
    return colors;
}

// ─── Zollunionen (seit M9) ──────────────────────────────────────────────────

Array UhrwerkSimulation::get_customs_unions() const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    for (std::size_t u = 0; u < w->customs_unions.size(); ++u) {
        const uhrwerk::CustomsUnion& c = w->customs_unions[u];
        PackedInt32Array members;
        uhrwerk::Fixed population;
        for (std::size_t s = 0; s < w->states.size(); ++s) {
            if (w->states[s].customs_union == static_cast<uhrwerk::Index>(u)) {
                members.append(static_cast<std::int32_t>(s));
                population += w->state_population(static_cast<uhrwerk::Index>(s));
            }
        }
        Dictionary entry;
        entry["index"] = static_cast<std::int64_t>(u);
        entry["id"] = to_godot(c.id);
        entry["name"] = to_godot(c.name);
        entry["color"] = to_godot(c.color);
        entry["leader_index"] = c.leader;
        entry["leader_name"] =
            valid_index(w->states, c.leader) ? to_godot(at(w->states, c.leader).short_name) : String();
        entry["tariff_percent"] = c.tariff.to_double() * 100.0;
        entry["member_indices"] = members;
        entry["member_count"] = static_cast<std::int64_t>(members.size());
        entry["population"] = population.round();
        entry["revenue_month"] =
            u < w->customs_revenue.size() ? w->customs_revenue[u].to_double() : 0.0; // Vereinskasse
        // Aufgelöst, wenn die Leitmacht ihr nicht mehr angehört (diplomatie.md Abschnitt 3.5).
        entry["dissolved"] = !valid_index(w->states, c.leader) ||
                             at(w->states, c.leader).customs_union != static_cast<uhrwerk::Index>(u);
        result.append(entry);
    }
    return result;
}

Dictionary UhrwerkSimulation::get_customs_change(std::int64_t state_index, std::int64_t customs_union) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->states, state_index)) {
        return result;
    }
    const auto state = static_cast<uhrwerk::Index>(state_index);
    const auto target = customs_union < 0 ? uhrwerk::kNone : static_cast<uhrwerk::Index>(customs_union);
    if (target != uhrwerk::kNone && !valid_index(w->customs_unions, target)) {
        return result;
    }
    constexpr std::array<const char*, 4> kDecisions = {"ungueltig", "zurueckgezogen", "abgelehnt",
                                                       "angenommen"};
    const uhrwerk::CustomsDecision decision = uhrwerk::evaluate_customs_change(*w, state, target);
    result["decision"] = String(kDecisions[static_cast<std::size_t>(decision)]);
    result["effective_text"] = to_godot(uhrwerk::format_date_long(uhrwerk::customs_effective_date(w->date)));
    const uhrwerk::Index current = at(w->states, state_index).customs_union;
    result["current_name"] =
        valid_index(w->customs_unions, current) ? to_godot(at(w->customs_unions, current).name) : String();
    result["target_name"] =
        target != uhrwerk::kNone ? to_godot(at(w->customs_unions, target).name) : String();
    result["dissolves"] = valid_index(w->customs_unions, current) && target != current &&
                          at(w->customs_unions, current).leader == state;
    if (target != uhrwerk::kNone) {
        const uhrwerk::Index leader = at(w->customs_unions, target).leader;
        result["leader_name"] =
            valid_index(w->states, leader) ? to_godot(at(w->states, leader).short_name) : String();
    }
    return result;
}

bool UhrwerkSimulation::request_customs_union(std::int64_t state_index, std::int64_t customs_union) {
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->states, state_index)) {
        return false;
    }
    const auto target = customs_union < 0 ? uhrwerk::kNone : static_cast<uhrwerk::Index>(customs_union);
    if (target != uhrwerk::kNone && !valid_index(w->customs_unions, target)) {
        return false;
    }
    if (uhrwerk::evaluate_customs_change(*w, static_cast<uhrwerk::Index>(state_index), target) ==
        uhrwerk::CustomsDecision::Invalid) {
        return false;
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::CustomsUnion;
    command.state = static_cast<uhrwerk::Index>(state_index);
    command.target = target;
    simulation_->submit(command);
    return true;
}

// ─── Diplomatie (seit M9, Schritt b3) ───────────────────────────────────────

namespace {

String decision_text(uhrwerk::DiplomacyDecision decision) {
    switch (decision) {
    case uhrwerk::DiplomacyDecision::Accepted:
        return "angenommen";
    case uhrwerk::DiplomacyDecision::Rejected:
        return "abgelehnt";
    case uhrwerk::DiplomacyDecision::Invalid:
        break;
    }
    return "ungueltig";
}

const uhrwerk::Fixed kCampLeaning = uhrwerk::Fixed::from_int(20); // Lager ab ±20 (Abschnitt 4.1)

/// Lager eines Staates zwischen Preußen und Österreich: 1 Preußen, −1 Österreich, 0 unentschieden.
int camp_of(const uhrwerk::World& w, uhrwerk::Index state) {
    const uhrwerk::Index prussia = w.find_state("PRU");
    const uhrwerk::Index austria = w.find_state("AUT");
    if (prussia == uhrwerk::kNone || austria == uhrwerk::kNone) {
        return 0;
    }
    if (state == prussia) {
        return 1;
    }
    if (state == austria) {
        return -1;
    }
    const uhrwerk::Fixed leaning =
        uhrwerk::relation(w, state, prussia) - uhrwerk::relation(w, state, austria);
    return leaning > kCampLeaning ? 1 : leaning < -kCampLeaning ? -1 : 0;
}

String camp_text(int camp) {
    return to_godot(camp > 0   ? std::string_view("Preußen")
                    : camp < 0 ? std::string_view("Österreich")
                               : std::string_view("unentschieden"));
}

String short_name(const uhrwerk::World& w, std::int64_t state) {
    return valid_index(w.states, state) ? to_godot(at(w.states, state).short_name) : String();
}

String motion_kind_text(const uhrwerk::World& w, uhrwerk::MotionKind kind, uhrwerk::Index target) {
    switch (kind) {
    case uhrwerk::MotionKind::Reform:
        return "Bundesreform";
    case uhrwerk::MotionKind::CustomsUnification:
        return valid_index(w.customs_unions, target)
                   ? "Zolleinigung im " + to_godot(at(w.customs_unions, target).name)
                   : String("Zolleinigung");
    case uhrwerk::MotionKind::Intervention:
        return "Bundesintervention gegen " + short_name(w, target);
    case uhrwerk::MotionKind::Presidency:
        return "Vorsitz im Bund";
    }
    return {};
}

String majority_text(uhrwerk::MotionKind kind) {
    switch (kind) {
    case uhrwerk::MotionKind::Intervention:
        return "Mehrheit im Engeren Rat";
    case uhrwerk::MotionKind::CustomsUnification:
        return "zwei Drittel im Plenum";
    case uhrwerk::MotionKind::Reform:
    case uhrwerk::MotionKind::Presidency:
        break;
    }
    return "Einstimmigkeit im Plenum";
}

String demand_kind_text(const uhrwerk::World& w, uhrwerk::DemandKind kind, uhrwerk::Index customs_union) {
    switch (kind) {
    case uhrwerk::DemandKind::Leadership:
        return to_godot(std::string_view("Führung im Bund"));
    case uhrwerk::DemandKind::CustomsUnion:
        return valid_index(w.customs_unions, customs_union)
                   ? "Beitritt zum " + to_godot(at(w.customs_unions, customs_union).name)
                   : String("Beitritt zur Zollunion");
    case uhrwerk::DemandKind::BreakAlliance:
        return to_godot(std::string_view("Bündnisse lösen"));
    }
    return {};
}

String outcome_text(uhrwerk::PlayOutcome outcome) {
    switch (outcome) {
    case uhrwerk::PlayOutcome::DemanderWon:
        return "durchgesetzt";
    case uhrwerk::PlayOutcome::TargetWon:
        return "gescheitert";
    case uhrwerk::PlayOutcome::War:
        return "Krieg"; // seit M10
    case uhrwerk::PlayOutcome::Compromise:
        break;
    }
    return "Kompromiss";
}

Dictionary motion_result_to_godot(const uhrwerk::World& w, const uhrwerk::MotionResult& r) {
    Dictionary entry;
    entry["kind"] = static_cast<std::int64_t>(r.kind);
    entry["text"] = motion_kind_text(w, r.kind, r.target);
    entry["applicant_index"] = r.applicant;
    entry["applicant_name"] = short_name(w, r.applicant);
    entry["opponent_name"] = short_name(w, r.opponent);
    entry["date_text"] = to_godot(uhrwerk::format_date_long(r.date));
    entry["plenum_yes"] = r.plenum_yes;
    entry["plenum_total"] = r.plenum_total;
    entry["council_yes"] = r.council_yes;
    entry["council_total"] = r.council_total;
    entry["council_body"] = r.kind == uhrwerk::MotionKind::Intervention;
    entry["majority_text"] = majority_text(r.kind);
    entry["accepted"] = r.accepted;
    return entry;
}

PackedStringArray names_of(const uhrwerk::World& w, const std::vector<uhrwerk::Index>& states) {
    PackedStringArray names;
    for (const uhrwerk::Index s : states) {
        names.push_back(short_name(w, s));
    }
    return names;
}

} // namespace

Array UhrwerkSimulation::get_relations(std::int64_t state_index) const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w || w->relations.empty() || !valid_index(w->states, state_index)) {
        return result;
    }
    const auto self = static_cast<uhrwerk::Index>(state_index);
    // Zielwerte und Bundesstaaten einmal für alle (die Liste wird im Tooltip je Bild gelesen).
    const std::vector<uhrwerk::Fixed> targets = uhrwerk::relation_targets(*w, self);
    std::vector<bool> federal(w->states.size(), false);
    for (std::size_t s = 0; s < w->states.size(); ++s) {
        federal[s] =
            uhrwerk::federal_state(*w, static_cast<uhrwerk::Index>(s)); // seit M13a2 auch Bund zur Laufzeit
    }
    for (std::size_t s = 0; s < w->states.size(); ++s) {
        const auto other = static_cast<uhrwerk::Index>(s);
        if (other == self) {
            continue;
        }
        const uhrwerk::State& state = w->states[s];
        Dictionary entry;
        entry["index"] = other;
        entry["tag"] = to_godot(state.tag);
        entry["name"] = to_godot(state.name);
        entry["short_name"] = to_godot(state.short_name);
        entry["color"] = to_godot(state.color);
        entry["relation"] = uhrwerk::relation(*w, self, other).to_double();
        entry["target"] = targets[s].to_double();
        entry["rival"] = uhrwerk::rivals(*w, self, other);
        entry["allied"] = uhrwerk::allied(*w, self, other);
        entry["guarantees"] = uhrwerk::guarantees(*w, self, other); // der Staat schützt other
        entry["guaranteed"] = uhrwerk::guarantees(*w, other, self); // other schützt den Staat
        entry["envoy"] = uhrwerk::has_envoy(*w, self, other);       // Gesandter des Staates bei other
        entry["envoy_from"] = uhrwerk::has_envoy(*w, other, self);  // Gesandter von other beim Staat
        entry["federal"] = static_cast<bool>(federal[s]);
        entry["plenum_votes"] = uhrwerk::plenum_votes(*w, other);
        const int camp = camp_of(*w, other);
        entry["camp"] = camp;
        entry["camp_text"] = camp_text(camp);
        entry["in_play"] = uhrwerk::play_of(*w, other) != uhrwerk::kNone;
        entry["customs_union_name"] = valid_index(w->customs_unions, state.customs_union)
                                          ? to_godot(at(w->customs_unions, state.customs_union).name)
                                          : String();
        result.append(entry);
    }
    return result;
}

Dictionary UhrwerkSimulation::get_diplomacy(std::int64_t state_index) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->states, state_index)) {
        return result;
    }
    const auto self = static_cast<uhrwerk::Index>(state_index);
    result["active"] = !w->relations.empty();
    result["diet"] = uhrwerk::has_diet(*w);
    result["envoy_capacity"] = uhrwerk::envoy_capacity(*w, self);
    result["envoy_count"] = uhrwerk::envoy_count(*w, self);
    result["envoy_cost_month"] = at(w->states, state_index).budget.diplomacy.to_double();
    result["envoy_cost_percent"] = w->diplomacy_rules.envoy_cost.to_double() * 100.0;
    result["alliance_min"] = w->diplomacy_rules.alliance_min.to_double();
    result["guarantee_min"] = w->diplomacy_rules.guarantee_min.to_double();
    result["rival_below"] = w->diplomacy_rules.rival_below.to_double();
    result["play_index"] = uhrwerk::play_of(*w, self);
    // Mit Militär (seit M10) zählt die Mobilmachung des Staates, die Kosten folgen den Soldaten; sonst die
    // Mobilmachung der Krise mit dem Faktor der Diplomatie.
    if (uhrwerk::military_active(*w)) {
        result["mobilized"] = uhrwerk::mobilization_phase(*w, self) != uhrwerk::MobilizationPhase::Peace;
        result["army_cost_factor"] = uhrwerk::army_cost_factor(*w, self).to_double();
    } else {
        const bool mobilized = uhrwerk::mobilized(*w, self);
        result["mobilized"] = mobilized;
        result["army_cost_factor"] = mobilized ? w->diplomacy_rules.mobilization.to_double() : 1.0;
    }
    result["camp"] = camp_of(*w, self);
    result["federal"] = uhrwerk::federal_state(*w, self);
    return result;
}

Dictionary UhrwerkSimulation::get_diplomacy_actions(std::int64_t state_index, std::int64_t target) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || w->relations.empty() || !valid_index(w->states, state_index) ||
        !valid_index(w->states, target) || state_index == target) {
        return result;
    }
    const auto self = static_cast<uhrwerk::Index>(state_index);
    const auto other = static_cast<uhrwerk::Index>(target);
    using uhrwerk::TreatyAction;
    result["envoy_send"] = decision_text(uhrwerk::evaluate_envoy(*w, self, other, true));
    result["envoy_recall"] = decision_text(uhrwerk::evaluate_envoy(*w, self, other, false));
    result["alliance"] = decision_text(uhrwerk::evaluate_treaty(*w, self, other, TreatyAction::Alliance));
    result["guarantee"] = decision_text(uhrwerk::evaluate_treaty(*w, self, other, TreatyAction::Guarantee));
    result["cancel_alliance"] =
        decision_text(uhrwerk::evaluate_treaty(*w, self, other, TreatyAction::CancelAlliance));
    result["cancel_guarantee"] =
        decision_text(uhrwerk::evaluate_treaty(*w, self, other, TreatyAction::CancelGuarantee));
    result["demand_leadership"] =
        decision_text(uhrwerk::evaluate_demand(*w, self, other, uhrwerk::DemandKind::Leadership));
    result["demand_customs_union"] =
        decision_text(uhrwerk::evaluate_demand(*w, self, other, uhrwerk::DemandKind::CustomsUnion));
    result["demand_break_alliance"] =
        decision_text(uhrwerk::evaluate_demand(*w, self, other, uhrwerk::DemandKind::BreakAlliance));
    result["motion_intervention"] =
        decision_text(uhrwerk::evaluate_motion(*w, self, uhrwerk::MotionKind::Intervention, other));
    result["customs_union_name"] =
        valid_index(w->customs_unions, at(w->states, state_index).customs_union)
            ? to_godot(at(w->customs_unions, at(w->states, state_index).customs_union).name)
            : String();
    // Seiten eines möglichen Spiels (Parteinahme, Abschnitt 4.5), mit Stärke in Soldaten.
    std::vector<uhrwerk::Index> own;
    std::vector<uhrwerk::Index> theirs;
    uhrwerk::play_sides(*w, self, other, own, theirs);
    uhrwerk::DiplomaticPlay probe;
    probe.demander = self;
    probe.target = other;
    probe.demander_side = own;
    probe.target_side = theirs;
    result["own_side"] = names_of(*w, own);
    result["other_side"] = names_of(*w, theirs);
    result["own_strength"] = uhrwerk::side_strength(*w, probe, true).round();
    result["other_strength"] = uhrwerk::side_strength(*w, probe, false).round();
    return result;
}

bool UhrwerkSimulation::send_envoy(std::int64_t state_index, std::int64_t target, bool send) {
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->states, state_index) || !valid_index(w->states, target) ||
        uhrwerk::evaluate_envoy(*w, static_cast<uhrwerk::Index>(state_index),
                                static_cast<uhrwerk::Index>(target),
                                send) == uhrwerk::DiplomacyDecision::Invalid) {
        return false;
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::Envoy;
    command.state = static_cast<uhrwerk::Index>(state_index);
    command.target = static_cast<uhrwerk::Index>(target);
    command.value = send ? uhrwerk::Fixed::one() : uhrwerk::Fixed::zero();
    simulation_->submit(command);
    return true;
}

bool UhrwerkSimulation::propose_treaty(std::int64_t state_index, std::int64_t target, std::int64_t action) {
    const uhrwerk::World* w = world();
    if (!w || action < TREATY_ALLIANCE || action > TREATY_CANCEL_GUARANTEE ||
        !valid_index(w->states, state_index) || !valid_index(w->states, target) ||
        uhrwerk::evaluate_treaty(
            *w, static_cast<uhrwerk::Index>(state_index), static_cast<uhrwerk::Index>(target),
            static_cast<uhrwerk::TreatyAction>(action)) == uhrwerk::DiplomacyDecision::Invalid) {
        return false;
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::Treaty;
    command.state = static_cast<uhrwerk::Index>(state_index);
    command.target = static_cast<uhrwerk::Index>(target);
    command.value = uhrwerk::Fixed::from_int(action);
    simulation_->submit(command);
    return true;
}

Dictionary UhrwerkSimulation::get_diet() const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !uhrwerk::has_diet(*w)) {
        return result;
    }
    const uhrwerk::FederalDiet& diet = uhrwerk::active_diet(*w); // seit M13a2 auch ein Bund zur Laufzeit
    result["name"] = to_godot(diet.name);
    result["federation_name"] =
        to_godot(diet.federation_name.empty() ? std::string("Deutscher Bund") : diet.federation_name);
    result["founded"] = uhrwerk::federation_founded(*w);
    result["presidency_index"] = w->diet_presidency;
    result["presidency_name"] = short_name(*w, w->diet_presidency);
    result["seat_name"] =
        valid_index(w->locations, diet.seat) ? to_godot(at(w->locations, diet.seat).name) : String();
    Array plenum;
    std::int64_t total = 0;
    for (std::size_t s = 0; s < w->states.size(); ++s) {
        const std::int32_t votes = uhrwerk::plenum_votes(*w, static_cast<uhrwerk::Index>(s));
        if (votes <= 0) {
            continue;
        }
        total += votes;
        Dictionary entry;
        entry["index"] = static_cast<std::int64_t>(s);
        entry["short_name"] = to_godot(w->states[s].short_name);
        entry["color"] = to_godot(w->states[s].color);
        entry["votes"] = votes;
        entry["camp"] = camp_of(*w, static_cast<uhrwerk::Index>(s));
        plenum.append(entry);
    }
    result["plenum"] = plenum;
    result["plenum_total"] = total;
    Array council;
    for (const uhrwerk::CouncilVote& vote : diet.council) {
        Dictionary entry;
        entry["name"] = to_godot(vote.name);
        PackedInt32Array members;
        for (const uhrwerk::Index s : vote.states) {
            members.append(static_cast<std::int32_t>(s));
        }
        entry["state_indices"] = members;
        entry["states"] = names_of(*w, vote.states);
        council.append(entry);
    }
    result["council"] = council;
    Dictionary motion;
    if (!w->motions.empty()) {
        const uhrwerk::Motion& m = w->motions.front();
        uhrwerk::Date vote = m.filed;
        vote.day = 1;
        vote.month = m.filed.month == 12 ? 1 : m.filed.month + 1;
        vote.year = m.filed.month == 12 ? m.filed.year + 1 : m.filed.year;
        motion["kind"] = static_cast<std::int64_t>(m.kind);
        motion["text"] = motion_kind_text(*w, m.kind, m.target);
        motion["applicant_index"] = m.applicant;
        motion["applicant_name"] = short_name(*w, m.applicant);
        motion["filed_text"] = to_godot(uhrwerk::format_date_long(m.filed));
        motion["vote_text"] = to_godot(uhrwerk::format_date_long(vote));
        motion["majority_text"] = majority_text(m.kind);
    }
    result["motion"] = motion;
    Array results;
    std::int64_t reforms = 0;
    std::int64_t reforms_accepted = 0;
    for (const uhrwerk::MotionResult& r : w->motion_results) {
        results.append(motion_result_to_godot(*w, r));
        if (r.kind == uhrwerk::MotionKind::Reform) {
            ++reforms;
            reforms_accepted += r.accepted ? 1 : 0;
        }
    }
    result["results"] = results;
    result["reform_attempts"] = reforms;
    result["reform_accepted"] = reforms_accepted;
    // Seit M11 (b4): Die Bundesversammlung ruht (1848–1851); dann nimmt sie keine Anträge an.
    result["suspended"] = w->diet_suspended != 0;
    return result;
}

Dictionary UhrwerkSimulation::get_motion_preview(std::int64_t applicant, std::int64_t kind,
                                                 std::int64_t target) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || kind < MOTION_REFORM || kind > MOTION_PRESIDENCY || !valid_index(w->states, applicant)) {
        return result;
    }
    const auto motion_kind = static_cast<uhrwerk::MotionKind>(kind);
    const auto target_index = target < 0 ? uhrwerk::kNone : static_cast<uhrwerk::Index>(target);
    const uhrwerk::DiplomacyDecision decision =
        uhrwerk::evaluate_motion(*w, static_cast<uhrwerk::Index>(applicant), motion_kind, target_index);
    result["decision"] = decision_text(decision);
    result["text"] = motion_kind_text(*w, motion_kind, target_index);
    result["majority_text"] = majority_text(motion_kind);
    if (decision == uhrwerk::DiplomacyDecision::Accepted) {
        const uhrwerk::MotionResult tally = uhrwerk::tally_motion(
            *w, uhrwerk::Motion{motion_kind, static_cast<uhrwerk::Index>(applicant), target_index, w->date});
        Dictionary entry = motion_result_to_godot(*w, tally);
        result["plenum_yes"] = entry["plenum_yes"];
        result["plenum_total"] = entry["plenum_total"];
        result["council_yes"] = entry["council_yes"];
        result["council_total"] = entry["council_total"];
        result["council_body"] = entry["council_body"];
        result["opponent_name"] = entry["opponent_name"];
        result["accepted"] = tally.accepted;
    }
    return result;
}

bool UhrwerkSimulation::file_motion(std::int64_t applicant, std::int64_t kind, std::int64_t target) {
    const uhrwerk::World* w = world();
    if (!w || kind < MOTION_REFORM || kind > MOTION_PRESIDENCY || !valid_index(w->states, applicant)) {
        return false;
    }
    const auto target_index = target < 0 ? uhrwerk::kNone : static_cast<uhrwerk::Index>(target);
    if (uhrwerk::evaluate_motion(*w, static_cast<uhrwerk::Index>(applicant),
                                 static_cast<uhrwerk::MotionKind>(kind),
                                 target_index) != uhrwerk::DiplomacyDecision::Accepted) {
        return false;
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::Motion;
    command.state = static_cast<uhrwerk::Index>(applicant);
    command.target = target_index;
    command.value = uhrwerk::Fixed::from_int(kind);
    simulation_->submit(command);
    return true;
}

Array UhrwerkSimulation::get_diplomatic_plays() const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    const std::int64_t today = uhrwerk::to_day_number(w->date);
    for (std::size_t i = 0; i < w->plays.size(); ++i) {
        const uhrwerk::DiplomaticPlay& play = w->plays[i];
        Dictionary entry;
        entry["index"] = static_cast<std::int64_t>(i);
        entry["kind"] = static_cast<std::int64_t>(play.kind);
        entry["kind_text"] = demand_kind_text(*w, play.kind, play.customs_union);
        entry["demander_index"] = play.demander;
        entry["demander_name"] = short_name(*w, play.demander);
        entry["target_index"] = play.target;
        entry["target_name"] = short_name(*w, play.target);
        entry["stage"] = play.stage;
        entry["stage_text"] = play.stage == 1 ? "Protest" : play.stage == 2 ? "Ultimatum" : "Mobilmachung";
        entry["days_left"] = std::max<std::int64_t>(0, uhrwerk::to_day_number(play.stage_started) +
                                                           w->diplomacy_rules.play_stage_days - today);
        entry["started_text"] = to_godot(uhrwerk::format_date_long(play.started));
        entry["demander_strength"] = uhrwerk::side_strength(*w, play, true).round();
        entry["target_strength"] = uhrwerk::side_strength(*w, play, false).round();
        entry["demander_side"] = names_of(*w, play.demander_side);
        entry["target_side"] = names_of(*w, play.target_side);
        entry["demander_escalates"] = play.demander_escalates;
        entry["target_escalates"] = play.target_escalates;
        entry["demander_mediation"] =
            decision_text(uhrwerk::evaluate_manoeuvre(*w, play.demander, uhrwerk::Manoeuvre::Mediate));
        entry["target_mediation"] =
            decision_text(uhrwerk::evaluate_manoeuvre(*w, play.target, uhrwerk::Manoeuvre::Mediate));
        result.append(entry);
    }
    return result;
}

bool UhrwerkSimulation::make_demand(std::int64_t demander, std::int64_t target, std::int64_t kind) {
    const uhrwerk::World* w = world();
    if (!w || kind < DEMAND_LEADERSHIP || kind > DEMAND_BREAK_ALLIANCE || !valid_index(w->states, demander) ||
        !valid_index(w->states, target) ||
        uhrwerk::evaluate_demand(
            *w, static_cast<uhrwerk::Index>(demander), static_cast<uhrwerk::Index>(target),
            static_cast<uhrwerk::DemandKind>(kind)) != uhrwerk::DiplomacyDecision::Accepted) {
        return false;
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::Demand;
    command.state = static_cast<uhrwerk::Index>(demander);
    command.target = static_cast<uhrwerk::Index>(target);
    command.value = uhrwerk::Fixed::from_int(kind);
    simulation_->submit(command);
    return true;
}

bool UhrwerkSimulation::manoeuvre(std::int64_t state_index, std::int64_t kind) {
    const uhrwerk::World* w = world();
    if (!w || kind < MANOEUVRE_YIELD || kind > MANOEUVRE_ESCALATE || !valid_index(w->states, state_index) ||
        uhrwerk::evaluate_manoeuvre(*w, static_cast<uhrwerk::Index>(state_index),
                                    static_cast<uhrwerk::Manoeuvre>(kind)) ==
            uhrwerk::DiplomacyDecision::Invalid) {
        return false;
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::Manoeuvre;
    command.state = static_cast<uhrwerk::Index>(state_index);
    command.value = uhrwerk::Fixed::from_int(kind);
    simulation_->submit(command);
    return true;
}

Dictionary UhrwerkSimulation::get_german_question() const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || w->relations.empty()) {
        return result;
    }
    // Bevölkerung des Bundes: Orte im Bundesgebiet, je Staat.
    std::vector<double> federal(w->states.size(), 0.0);
    double total = 0.0;
    for (std::size_t i = 0; i < w->locations.size(); ++i) {
        const uhrwerk::Location& loc = w->locations[i];
        if (uhrwerk::in_federal_area(*w, static_cast<uhrwerk::Index>(i)) &&
            valid_index(w->states, loc.owner)) {
            const double people = w->location_population(static_cast<uhrwerk::Index>(i)).to_double();
            federal[static_cast<std::size_t>(loc.owner)] += people;
            total += people;
        }
    }
    result["federal_population"] = static_cast<std::int64_t>(total + 0.5);
    // Anteil des Zollvereins (Union mit der Kennung zollverein, sonst der größten Union).
    uhrwerk::Index zollverein = w->find_customs_union("zollverein");
    std::vector<double> union_people(w->customs_unions.size(), 0.0);
    for (std::size_t s = 0; s < w->states.size(); ++s) {
        if (valid_index(w->customs_unions, w->states[s].customs_union)) {
            union_people[static_cast<std::size_t>(w->states[s].customs_union)] += federal[s];
        }
    }
    if (zollverein == uhrwerk::kNone && !union_people.empty()) {
        zollverein = static_cast<uhrwerk::Index>(std::max_element(union_people.begin(), union_people.end()) -
                                                 union_people.begin());
    }
    result["customs_union_name"] = valid_index(w->customs_unions, zollverein)
                                       ? to_godot(at(w->customs_unions, zollverein).name)
                                       : String();
    result["customs_union_percent"] = valid_index(w->customs_unions, zollverein) && total > 0.0
                                          ? union_people[static_cast<std::size_t>(zollverein)] / total * 100.0
                                          : 0.0;
    // Lager: Bevölkerung im Bund nach Neigung zu Preußen oder Österreich.
    double camps[3] = {0.0, 0.0, 0.0};
    std::int64_t camp_states[3] = {0, 0, 0};
    for (std::size_t s = 0; s < w->states.size(); ++s) {
        if (federal[s] <= 0.0) {
            continue;
        }
        const int camp = camp_of(*w, static_cast<uhrwerk::Index>(s));
        const std::size_t slot = camp > 0 ? 0 : camp < 0 ? 1 : 2;
        camps[slot] += federal[s];
        ++camp_states[slot];
    }
    result["prussia_percent"] = total > 0.0 ? camps[0] / total * 100.0 : 0.0;
    result["austria_percent"] = total > 0.0 ? camps[1] / total * 100.0 : 0.0;
    result["undecided_percent"] = total > 0.0 ? camps[2] / total * 100.0 : 0.0;
    result["prussia_states"] = camp_states[0];
    result["austria_states"] = camp_states[1];
    result["undecided_states"] = camp_states[2];
    // Bundesreform und Führung im Bund.
    std::int64_t reforms = 0;
    std::int64_t reforms_accepted = 0;
    for (const uhrwerk::MotionResult& r : w->motion_results) {
        if (r.kind == uhrwerk::MotionKind::Reform) {
            ++reforms;
            reforms_accepted += r.accepted ? 1 : 0;
        }
    }
    result["reform_attempts"] = reforms;
    result["reform_accepted"] = reforms_accepted;
    const uhrwerk::Index prussia = w->find_state("PRU");
    const uhrwerk::Index austria = w->find_state("AUT");
    std::int64_t prussia_wins = 0;
    std::int64_t austria_wins = 0;
    std::int64_t compromises = 0;
    Array plays;
    for (const uhrwerk::PlayResult& r : w->play_results) {
        Dictionary entry;
        entry["text"] = demand_kind_text(*w, r.kind, r.customs_union);
        entry["demander_name"] = short_name(*w, r.demander);
        entry["target_name"] = short_name(*w, r.target);
        entry["date_text"] = to_godot(uhrwerk::format_date_long(r.ended));
        entry["outcome_text"] = outcome_text(r.outcome);
        plays.append(entry);
        if (r.kind != uhrwerk::DemandKind::Leadership) {
            continue;
        }
        if (r.outcome == uhrwerk::PlayOutcome::Compromise) {
            ++compromises;
            continue;
        }
        if (r.outcome == uhrwerk::PlayOutcome::War) {
            continue; // seit M10: entscheidet der Krieg (Frieden mit Schritt a4)
        }
        const uhrwerk::Index winner = r.outcome == uhrwerk::PlayOutcome::DemanderWon ? r.demander : r.target;
        prussia_wins += winner == prussia ? 1 : 0;
        austria_wins += winner == austria ? 1 : 0;
    }
    result["leadership_prussia"] = prussia_wins;
    result["leadership_austria"] = austria_wins;
    result["leadership_compromises"] = compromises;
    result["play_results"] = plays;
    // Bündnisse und Garantien als Netz.
    Array treaties;
    for (const uhrwerk::Treaty& t : w->treaties) {
        Dictionary entry;
        entry["alliance"] = t.kind == uhrwerk::TreatyKind::Alliance;
        entry["from_index"] = t.from;
        entry["to_index"] = t.to;
        entry["from_name"] = short_name(*w, t.from);
        entry["to_name"] = short_name(*w, t.to);
        entry["since_text"] = to_godot(uhrwerk::format_date_long(t.since));
        treaties.append(entry);
    }
    result["treaties"] = treaties;
    return result;
}

// ─── Militär (seit M10, Schritt b3) ─────────────────────────────────────────

namespace {

String army_status_text(uhrwerk::ArmyStatus status) {
    switch (status) {
    case uhrwerk::ArmyStatus::Garrison:
        return to_godot("in der Garnison");
    case uhrwerk::ArmyStatus::Rest:
        return to_godot("in Ruhe");
    case uhrwerk::ArmyStatus::March:
        return to_godot("auf dem Marsch");
    case uhrwerk::ArmyStatus::Rail:
        return to_godot("auf der Bahn");
    case uhrwerk::ArmyStatus::Battle:
        return to_godot("in der Schlacht");
    case uhrwerk::ArmyStatus::Siege:
        return to_godot("belagert eine Festung");
    case uhrwerk::ArmyStatus::Retreat:
        return to_godot("auf dem Rückzug");
    }
    return String();
}

Dictionary army_to_godot(const uhrwerk::World& w, const uhrwerk::Army& army) {
    Dictionary result;
    result["id"] = army.id;
    result["state_index"] = army.state;
    result["name"] = to_godot(army.name);
    result["commander"] = to_godot(army.commander);
    result["leadership"] = army.leadership.to_double();
    result["location_index"] = army.location;
    result["soldiers"] = army.soldiers.round();
    result["guns"] = army.guns.round();
    result["morale"] = army.morale.to_double();
    result["status"] = static_cast<std::int64_t>(army.status);
    result["status_text"] = army_status_text(army.status);
    result["garrison"] = army.status == uhrwerk::ArmyStatus::Garrison;
    result["staff"] = army.staff;
    result["supplied"] = army.status == uhrwerk::ArmyStatus::Garrison || uhrwerk::supplied(w, army);
    PackedInt32Array route;
    bool rail = false;
    for (const uhrwerk::RouteStep& step : army.route) {
        route.push_back(static_cast<std::int32_t>(step.location));
    }
    if (!army.route.empty()) {
        rail = army.route.front().mode != uhrwerk::LegMode::March;
    }
    result["route"] = route;
    result["destination_index"] = army.route.empty() ? std::int64_t{-1} : army.route.back().location;
    result["on_rail"] = rail;
    result["days_left"] = army.days_left.to_double();
    return result;
}

/// Name eines Krieges: der Name aus dem Aufmarsch, wenn er derselbe Krieg ist, sonst „A gegen B“.
String war_name(const uhrwerk::World& w, const uhrwerk::War& war) {
    for (const uhrwerk::OpeningWar& opening : w.opening_wars) {
        if (!opening.attackers.empty() && !opening.defenders.empty() &&
            opening.attackers.front() == war.attackers.front() &&
            opening.defenders.front() == war.defenders.front() && opening.started == war.started) {
            return to_godot(opening.name);
        }
    }
    return to_godot(at(w.states, war.attackers.front()).short_name + " gegen " +
                    at(w.states, war.defenders.front()).short_name);
}

Array state_indices(const std::vector<uhrwerk::Index>& states) {
    Array result;
    for (const uhrwerk::Index s : states) {
        result.push_back(s);
    }
    return result;
}

/// Mögliche Kriegsziele einer Seite: Annexion je Staat der Gegenseite, Gebiete der beiden obersten
/// Ebenen, Führung im Bund, Beitritt zur Zollunion, Entschädigung von 3, 6 und 12 Monatseinnahmen;
/// nur gültige, mit Kosten.
Array goal_options(const uhrwerk::World& w, const uhrwerk::War& war, bool attacker) {
    Array result;
    const auto add = [&](uhrwerk::WarGoalKind kind, uhrwerk::Index param) {
        const uhrwerk::WarGoal goal{kind, attacker, param};
        if (!uhrwerk::valid_war_goal(w, war, goal)) {
            return;
        }
        Dictionary entry;
        entry["code"] = uhrwerk::encode_war_goal(kind, param);
        entry["kind"] = static_cast<std::int64_t>(kind);
        entry["text"] = to_godot(uhrwerk::war_goal_text(w, war, goal));
        entry["cost"] = uhrwerk::war_goal_cost(w, war, goal).to_double();
        result.push_back(entry);
    };
    for (const uhrwerk::Index s : attacker ? war.defenders : war.attackers) {
        add(uhrwerk::WarGoalKind::Annexation, s);
    }
    for (std::size_t u = 0; u < w.admin_units.size(); ++u) {
        const uhrwerk::Index parent = w.admin_units[u].parent;
        if (parent == uhrwerk::kNone || at(w.admin_units, parent).parent == uhrwerk::kNone) {
            add(uhrwerk::WarGoalKind::Territory, static_cast<uhrwerk::Index>(u));
        }
    }
    add(uhrwerk::WarGoalKind::Leadership, 0);
    add(uhrwerk::WarGoalKind::Customs, 0);
    for (const uhrwerk::Index months : {3, 6, 12}) {
        add(uhrwerk::WarGoalKind::Indemnity, months);
    }
    return result;
}

} // namespace

Array UhrwerkSimulation::get_armies(std::int64_t state_index) const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    for (const uhrwerk::Army& army : w->armies) {
        if (state_index < 0 || army.state == state_index) {
            result.push_back(army_to_godot(*w, army));
        }
    }
    return result;
}

Dictionary UhrwerkSimulation::get_army(std::int64_t army_id) const {
    const uhrwerk::World* w = world();
    const uhrwerk::Army* army = w ? uhrwerk::find_army(*w, army_id) : nullptr;
    return army ? army_to_godot(*w, *army) : Dictionary();
}

Dictionary UhrwerkSimulation::get_route_preview(std::int64_t army_id, std::int64_t location_index) const {
    Dictionary result;
    result["ok"] = false;
    const uhrwerk::World* w = world();
    const uhrwerk::Army* army = w ? uhrwerk::find_army(*w, army_id) : nullptr;
    if (!army || army->status == uhrwerk::ArmyStatus::Garrison ||
        !valid_index(w->locations, location_index)) {
        return result;
    }
    const std::optional<uhrwerk::Route> route =
        uhrwerk::find_route(*w, *army, static_cast<uhrwerk::Index>(location_index));
    if (!route) {
        return result;
    }
    PackedInt32Array path;
    std::int64_t rail_legs = 0;
    for (const uhrwerk::RouteStep& step : route->steps) {
        path.push_back(static_cast<std::int32_t>(step.location));
        rail_legs += step.mode != uhrwerk::LegMode::March ? 1 : 0;
    }
    result["ok"] = true;
    result["days"] = route->days.to_double();
    result["path"] = path;
    result["rail_legs"] = rail_legs;
    const std::int64_t whole = route->days.trunc();
    const std::int64_t days = whole + (route->days > uhrwerk::Fixed::from_int(whole) ? 1 : 0);
    result["arrival_text"] =
        to_godot(uhrwerk::format_date_long(uhrwerk::from_day_number(uhrwerk::to_day_number(w->date) + days)));
    return result;
}

bool UhrwerkSimulation::order_march(std::int64_t army_id, std::int64_t location_index) {
    const uhrwerk::World* w = world();
    const uhrwerk::Army* army = w ? uhrwerk::find_army(*w, army_id) : nullptr;
    if (!army || !bool(get_route_preview(army_id, location_index)["ok"])) {
        return false;
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::March;
    command.state = army->state;
    command.target = static_cast<uhrwerk::Index>(location_index);
    command.value = uhrwerk::Fixed::from_int(army_id);
    simulation_->submit(command);
    return true;
}

bool UhrwerkSimulation::set_army_staff(std::int64_t army_id, bool on) {
    const uhrwerk::World* w = world();
    const uhrwerk::Army* army = w ? uhrwerk::find_army(*w, army_id) : nullptr;
    if (!army || army->staff == on) {
        return false;
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::Staff;
    command.state = army->state;
    command.target = on ? 1 : 0;
    command.value = uhrwerk::Fixed::from_int(army_id);
    simulation_->submit(command);
    return true;
}

bool UhrwerkSimulation::order_mobilization(std::int64_t state_index, bool mobilize) {
    const uhrwerk::World* w = world();
    if (!w || !uhrwerk::military_active(*w) || !valid_index(w->states, state_index)) {
        return false;
    }
    const auto state = static_cast<uhrwerk::Index>(state_index);
    const uhrwerk::MobilizationPhase phase = uhrwerk::mobilization_phase(*w, state);
    const bool armed =
        phase == uhrwerk::MobilizationPhase::Mobilizing || phase == uhrwerk::MobilizationPhase::Mobilized;
    if (mobilize == armed || (!mobilize && uhrwerk::in_war(*w, state))) {
        return false; // schon so, oder im Krieg rüstet niemand ab
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::Mobilization;
    command.state = state;
    command.value = mobilize ? uhrwerk::Fixed::one() : uhrwerk::Fixed::zero();
    simulation_->submit(command);
    return true;
}

PackedInt32Array UhrwerkSimulation::get_location_control() const {
    PackedInt32Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    result.resize(static_cast<std::int64_t>(w->locations.size()));
    for (std::size_t l = 0; l < w->locations.size(); ++l) {
        const uhrwerk::Index holder = uhrwerk::controller(*w, static_cast<uhrwerk::Index>(l));
        result.set(static_cast<std::int64_t>(l),
                   holder == uhrwerk::kNone ? -1 : static_cast<std::int32_t>(holder));
    }
    return result;
}

Array UhrwerkSimulation::get_battles() const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    for (const uhrwerk::Battle& battle : w->battles) {
        Dictionary entry;
        entry["location_index"] = battle.location;
        entry["days"] = static_cast<std::int64_t>(battle.days);
        entry["attacker_losses"] = battle.attacker_losses.round();
        entry["defender_losses"] = battle.defender_losses.round();
        result.push_back(entry);
    }
    return result;
}

Array UhrwerkSimulation::get_sieges() const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    for (const uhrwerk::Siege& siege : w->sieges) {
        Dictionary entry;
        entry["location_index"] = siege.location;
        entry["besieger_index"] = siege.besieger;
        entry["progress"] = siege.progress.to_double();
        result.push_back(entry);
    }
    return result;
}

Array UhrwerkSimulation::get_wars() const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    const std::int64_t player = get_player_state_index();
    for (const uhrwerk::War& war : w->wars) {
        Dictionary entry;
        entry["id"] = war.id;
        entry["name"] = war_name(*w, war);
        entry["attackers"] = state_indices(war.attackers);
        entry["defenders"] = state_indices(war.defenders);
        entry["started_text"] = to_godot(uhrwerk::format_date_long(war.started));
        Array goals;
        for (const uhrwerk::WarGoal& goal : war.goals) {
            Dictionary g;
            g["attacker"] = goal.attacker;
            g["text"] = to_godot(uhrwerk::war_goal_text(*w, war, goal));
            g["cost"] = uhrwerk::war_goal_cost(*w, war, goal).to_double();
            goals.push_back(g);
        }
        entry["goals"] = goals;
        for (const bool attacker : {true, false}) {
            const std::string side = attacker ? "attacker_" : "defender_";
            entry[to_godot(side + "score")] = uhrwerk::war_score(*w, war, attacker).to_double();
            entry[to_godot(side + "weariness")] = uhrwerk::war_weariness(*w, war, attacker).to_double();
            entry[to_godot(side + "cost")] = uhrwerk::war_goals_cost(*w, war, attacker).to_double();
            // Erlittene Verluste der Seite: was die Gegenseite ihr zugefügt hat.
            entry[to_godot(side + "losses")] =
                (attacker ? war.defender_inflicted : war.attacker_inflicted).round();
        }
        entry["battles"] = static_cast<std::int64_t>(war.battles);
        const auto has = [&](const std::vector<uhrwerk::Index>& side) {
            return std::find(side.begin(), side.end(), player) != side.end();
        };
        entry["player_side"] = has(war.attackers) ? 1 : (has(war.defenders) ? -1 : 0);
        entry["player_leads"] = player == war.attackers.front() || player == war.defenders.front();
        result.push_back(entry);
    }
    return result;
}

Array UhrwerkSimulation::get_declaration_goals(std::int64_t state_index, std::int64_t target) const {
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->states, state_index) || !valid_index(w->states, target) ||
        !uhrwerk::can_declare_war(*w, static_cast<uhrwerk::Index>(state_index),
                                  static_cast<uhrwerk::Index>(target))) {
        return Array();
    }
    uhrwerk::War war; // gedachter Krieg mit den Seiten des Bündnisfalls
    uhrwerk::war_sides(*w, static_cast<uhrwerk::Index>(state_index), static_cast<uhrwerk::Index>(target),
                       war.attackers, war.defenders);
    return goal_options(*w, war, true);
}

Dictionary UhrwerkSimulation::get_declaration_sides(std::int64_t state_index, std::int64_t target) const {
    const uhrwerk::World* w = world();
    Dictionary result;
    if (!w || !valid_index(w->states, state_index) || !valid_index(w->states, target) ||
        !uhrwerk::can_declare_war(*w, static_cast<uhrwerk::Index>(state_index),
                                  static_cast<uhrwerk::Index>(target))) {
        return result;
    }
    std::vector<uhrwerk::Index> attackers;
    std::vector<uhrwerk::Index> defenders;
    uhrwerk::war_sides(*w, static_cast<uhrwerk::Index>(state_index), static_cast<uhrwerk::Index>(target),
                       attackers, defenders);
    const auto side = [&](const std::vector<uhrwerk::Index>& states, const char* key, const char* soldiers) {
        PackedInt32Array indices;
        uhrwerk::Fixed sum;
        for (const uhrwerk::Index s : states) {
            indices.push_back(static_cast<std::int32_t>(s));
            sum += uhrwerk::state_soldiers(*w, s);
        }
        result[key] = indices;
        result[soldiers] = sum.round();
    };
    side(attackers, "attackers", "attacker_soldiers");
    side(defenders, "defenders", "defender_soldiers");
    return result;
}

Array UhrwerkSimulation::get_war_goal_options(std::int64_t state_index, std::int64_t war_id) const {
    const uhrwerk::World* w = world();
    const uhrwerk::War* war = w ? uhrwerk::find_war(*w, war_id) : nullptr;
    if (!war || (war->attackers.front() != state_index && war->defenders.front() != state_index)) {
        return Array();
    }
    return goal_options(*w, *war, war->attackers.front() == state_index);
}

bool UhrwerkSimulation::declare_war(std::int64_t state_index, std::int64_t target, std::int64_t goal) {
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->states, state_index) || !valid_index(w->states, target) ||
        !uhrwerk::can_declare_war(*w, static_cast<uhrwerk::Index>(state_index),
                                  static_cast<uhrwerk::Index>(target))) {
        return false;
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::DeclareWar;
    command.state = static_cast<uhrwerk::Index>(state_index);
    command.target = static_cast<uhrwerk::Index>(target);
    command.value = uhrwerk::Fixed::from_int(goal);
    simulation_->submit(command);
    return true;
}

bool UhrwerkSimulation::add_war_goal(std::int64_t state_index, std::int64_t war_id, std::int64_t goal) {
    const uhrwerk::World* w = world();
    const uhrwerk::War* war = w ? uhrwerk::find_war(*w, war_id) : nullptr;
    if (!war || (war->attackers.front() != state_index && war->defenders.front() != state_index)) {
        return false;
    }
    const std::optional<uhrwerk::WarGoal> decoded =
        uhrwerk::decode_war_goal(goal, war->attackers.front() == state_index);
    if (!decoded || !uhrwerk::valid_war_goal(*w, *war, *decoded)) {
        return false;
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::WarGoal;
    command.state = static_cast<uhrwerk::Index>(state_index);
    command.target = static_cast<uhrwerk::Index>(war_id);
    command.value = uhrwerk::Fixed::from_int(goal);
    simulation_->submit(command);
    return true;
}

Dictionary UhrwerkSimulation::get_peace_preview(std::int64_t state_index, std::int64_t war_id,
                                                std::int64_t terms) const {
    Dictionary result;
    result["valid"] = false;
    const uhrwerk::World* w = world();
    if (!w || terms < PEACE_WHITE || terms > PEACE_CAPITULATION || !valid_index(w->states, state_index)) {
        return result;
    }
    const std::optional<bool> accepted = uhrwerk::peace_acceptance(
        *w, static_cast<uhrwerk::Index>(state_index), war_id, static_cast<uhrwerk::PeaceTerms>(terms));
    if (!accepted) {
        return result;
    }
    const uhrwerk::War& war = *uhrwerk::find_war(*w, war_id);
    const bool attacker = war.attackers.front() == state_index;
    const uhrwerk::Index other = attacker ? war.defenders.front() : war.attackers.front();
    result["valid"] = true;
    result["accepted"] = *accepted;
    result["text"] = to_godot(at(w->states, other).short_name + (*accepted ? " nähme an." : " lehnte ab."));
    return result;
}

bool UhrwerkSimulation::offer_peace(std::int64_t state_index, std::int64_t war_id, std::int64_t terms) {
    if (!bool(get_peace_preview(state_index, war_id, terms)["valid"])) {
        return false;
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::PeaceOffer;
    command.state = static_cast<uhrwerk::Index>(state_index);
    command.target = static_cast<uhrwerk::Index>(war_id);
    command.value = uhrwerk::Fixed::from_int(terms);
    simulation_->submit(command);
    return true;
}

// ─── Politik (seit M8) ──────────────────────────────────────────────────────

Array UhrwerkSimulation::get_currents() const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    for (const uhrwerk::PoliticalCurrent& c : w->currents) {
        Dictionary entry;
        entry["id"] = to_godot(c.id);
        entry["name"] = to_godot(c.name);
        entry["short_name"] = to_godot(c.short_name);
        entry["color"] = to_godot(c.color);
        entry["position"] = c.position.to_double();
        result.push_back(entry);
    }
    return result;
}

Array UhrwerkSimulation::get_parliaments() const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    for (std::size_t p = 0; p < w->parliaments.size(); ++p) {
        const uhrwerk::Parliament& parliament = w->parliaments[p];
        Dictionary entry;
        entry["index"] = static_cast<std::int64_t>(p);
        entry["id"] = to_godot(parliament.id);
        entry["name"] = to_godot(parliament.name);
        entry["state_index"] = parliament.state;
        entry["seats"] = parliament.seats;
        // Seit M13a3: der nächste ausstehende Wahltag, sonst der letzte; pending, solange eine Wahl aussteht.
        const uhrwerk::Date shown = shown_election_day(*w, static_cast<std::int64_t>(p));
        entry["election_date"] = date_dictionary(shown);
        entry["election_date_text"] = to_godot(uhrwerk::format_date_long(shown));
        entry["pending"] = uhrwerk::next_election_day(*w, static_cast<uhrwerk::Index>(p)).has_value();
        entry["federal"] = !parliament.federations.empty();
        entry["faction_date"] = date_dictionary(parliament.faction_day);
        entry["faction_date_text"] = to_godot(uhrwerk::format_date_long(parliament.faction_day));
        entry["seat_index"] = parliament.seat;
        entry["franchise"] = franchise_name(parliament.law.franchise);
        entry["indirect"] = parliament.law.indirect;
        // Seit M13a3: beim Parlament eines Bundes nur die Wahlkreise im Bundesgebiet (und deren Sitze).
        std::int64_t districts = 0;
        std::int64_t seats = 0;
        for (std::size_t d = 0; d < w->electoral_districts.size(); ++d) {
            const uhrwerk::ElectoralDistrict& district = w->electoral_districts[d];
            if (district.parliament == static_cast<uhrwerk::Index>(p) &&
                (parliament.federations.empty() ||
                 uhrwerk::district_votes(*w, static_cast<uhrwerk::Index>(d)))) {
                ++districts;
                seats += district.seats;
            }
        }
        entry["district_count"] = districts;
        if (!parliament.federations.empty()) {
            entry["seats"] = seats;
        }
        std::int64_t latest = -1;
        for (std::size_t e = 0; e < w->elections.size(); ++e) {
            if (w->elections[e].parliament == static_cast<uhrwerk::Index>(p)) {
                latest = static_cast<std::int64_t>(e);
            }
        }
        entry["election_index"] = latest;
        if (latest >= 0) {
            entry["last_election_date"] = date_dictionary(w->elections[static_cast<std::size_t>(latest)].day);
        }
        Array factions;
        for (const uhrwerk::Faction& f : parliament.factions) {
            Dictionary faction;
            faction["id"] = to_godot(f.id);
            faction["name"] = to_godot(f.name);
            faction["color"] = to_godot(f.color);
            faction["from"] = f.from.to_double();
            faction["to"] = f.to.to_double();
            factions.push_back(faction);
        }
        entry["factions"] = factions;
        result.push_back(entry);
    }
    return result;
}

std::int64_t UhrwerkSimulation::get_election_count() const {
    const uhrwerk::World* w = world();
    return w ? static_cast<std::int64_t>(w->elections.size()) : 0;
}

Dictionary UhrwerkSimulation::get_election(std::int64_t index) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->elections, index)) {
        return result;
    }
    const uhrwerk::ElectionResult& election = at(w->elections, index);
    if (!valid_index(w->parliaments, election.parliament)) {
        return result;
    }
    const uhrwerk::Parliament& parliament = at(w->parliaments, election.parliament);
    result["index"] = index;
    result["parliament_index"] = election.parliament;
    result["parliament_name"] = to_godot(parliament.name);
    result["date"] = date_dictionary(election.day);
    result["date_text"] = to_godot(uhrwerk::format_date_long(election.day));
    result["seats"] = parliament.seats;
    result["seated"] = static_cast<std::int64_t>(election.deputies.size());
    Array districts;
    double electorate = 0.0;
    double voters = 0.0;
    std::int64_t boycott_seats = 0;
    for (const uhrwerk::DistrictResult& r : election.districts) {
        if (!valid_index(w->electoral_districts, r.district)) {
            continue;
        }
        const uhrwerk::ElectoralDistrict& district = at(w->electoral_districts, r.district);
        Dictionary entry;
        entry["district_index"] = r.district;
        entry["id"] = to_godot(district.id);
        entry["name"] = to_godot(district.name);
        entry["state_index"] = district.state;
        entry["state_name"] = valid_index(w->states, district.state)
                                  ? to_godot(at(w->states, district.state).short_name)
                                  : String();
        entry["seats"] = district.seats;
        entry["location_index"] = district.locations.empty() ? uhrwerk::kNone : district.locations.front();
        entry["electorate"] = r.electorate.to_double();
        entry["voters"] = r.voters.to_double();
        entry["turnout"] =
            r.electorate > uhrwerk::Fixed::zero() ? (r.voters / r.electorate).to_double() : 0.0;
        const PackedFloat64Array shares = vote_shares(r);
        entry["shares"] = shares;
        entry["winner"] = r.winner;
        entry["winner_share"] = valid_index(w->currents, r.winner) ? shares[r.winner] : 0.0;
        entry["runoff"] = r.runoff;
        entry["boycott"] = r.boycott;
        entry["report_rank"] = r.report_rank;
        districts.push_back(entry);
        electorate += r.electorate.to_double();
        voters += r.voters.to_double();
        boycott_seats += r.boycott ? district.seats : 0;
    }
    result["districts"] = districts;
    result["electorate"] = electorate;
    result["voters"] = voters;
    result["boycott_seats"] = boycott_seats;
    PackedInt32Array deputy_district;
    PackedInt32Array deputy_current;
    PackedFloat32Array deputy_position;
    PackedInt32Array deputy_faction;
    for (const uhrwerk::Deputy& d : election.deputies) {
        deputy_district.push_back(d.district);
        deputy_current.push_back(d.current);
        deputy_position.push_back(static_cast<float>(d.position.to_double()));
        deputy_faction.push_back(d.faction);
    }
    result["deputy_district"] = deputy_district;
    result["deputy_current"] = deputy_current;
    result["deputy_position"] = deputy_position;
    result["deputy_faction"] = deputy_faction;
    PackedInt32Array by_current;
    for (const std::int32_t seats : uhrwerk::seats_by_current(*w, election)) {
        by_current.push_back(seats);
    }
    PackedInt32Array by_faction;
    for (const std::int32_t seats : uhrwerk::seats_by_faction(*w, election)) {
        by_faction.push_back(seats);
    }
    result["seats_by_current"] = by_current;
    result["seats_by_faction"] = by_faction; // zuletzt die Fraktionslosen
    return result;
}

PackedInt32Array UhrwerkSimulation::get_location_electoral_district_indices(std::int64_t parliament) const {
    PackedInt32Array result;
    if (!valid_index(location_districts_, parliament)) {
        return result;
    }
    for (const std::int32_t d : at(location_districts_, parliament)) {
        result.push_back(d);
    }
    return result;
}

std::int64_t UhrwerkSimulation::get_shown_parliament() const {
    const uhrwerk::World* w = world();
    if (!w || w->parliaments.empty()) {
        return -1;
    }
    // Das Parlament der letzten Wahl; vorher das mit dem nächsten Wahltag.
    if (!w->elections.empty()) {
        return w->elections.back().parliament;
    }
    const std::int64_t next = next_parliament();
    return next >= 0 ? next : 0;
}

// ─── Parteienmodus (seit M8, Schritt b2) ────────────────────────────────────

namespace {

const std::array<std::string_view, uhrwerk::kCampaignActions> kCampaignNames = {"Versammlung", "Flugblätter",
                                                                                "Verein gründen"};

/// Fester Anzeigefehler des Stimmungsberichts je Wahlkreis, Strömung und Monat in [−1, 1] (keine
/// Simulation, kein Zufallsstrom: die Zahl bleibt einen Monat lang gleich).
double poll_offset(std::int64_t district, std::size_t current, const uhrwerk::Date& date) {
    std::uint64_t x = static_cast<std::uint64_t>(district) * 1'000'003ULL + current * 7919ULL +
                      static_cast<std::uint64_t>(date.year * 12 + date.month) * 104'729ULL;
    x += 0x9E3779B97F4A7C15ULL; // splitmix64
    x = (x ^ (x >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27U)) * 0x94D049BB133111EBULL;
    x ^= x >> 31U;
    return static_cast<double>(x % 2001ULL) / 1000.0 - 1.0;
}

} // namespace

Dictionary UhrwerkSimulation::get_state_politics(std::int64_t state_index) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || w->parliaments.empty() || !valid_index(w->states, state_index)) {
        return result;
    }
    const uhrwerk::State& state = at(w->states, state_index);
    result["constitution"] = constitution_name(state.constitution);
    result["franchise"] = franchise_name(state.law.franchise);
    result["elected"] = state.law.franchise != uhrwerk::Franchise::None &&
                        state.law.franchise != uhrwerk::Franchise::Estates; // Volkswahl
    result["runoff"] = state.law.system == uhrwerk::ElectoralSystem::Runoff;
    result["indirect"] = state.law.indirect;
    result["census_threshold"] = state.law.census_threshold.to_double();
    const std::vector<uhrwerk::InterestGroupStanding> standing =
        uhrwerk::interest_group_standing(*w, static_cast<uhrwerk::Index>(state_index));
    Array groups;
    for (std::size_t g = 0; g < standing.size() && g < w->interest_groups.size(); ++g) {
        const uhrwerk::InterestGroup& group = w->interest_groups[g];
        Dictionary entry;
        entry["id"] = to_godot(group.id);
        entry["name"] = to_godot(group.name);
        entry["color"] = to_godot(group.color);
        entry["share_percent"] = standing[g].share.to_double() * 100.0;
        entry["influence_percent"] = standing[g].influence.to_double() * 100.0;
        groups.push_back(entry);
    }
    result["interest_groups"] = groups;
    return result;
}

bool UhrwerkSimulation::campaign(std::int64_t current, std::int64_t district, std::int64_t action) {
    const uhrwerk::World* w = world();
    if (!w || action < CAMPAIGN_VERSAMMLUNG || action > CAMPAIGN_VEREIN ||
        !valid_index(w->currents, current) || !valid_index(w->electoral_districts, district)) {
        return false;
    }
    const auto kind = static_cast<uhrwerk::CampaignAction>(action);
    if (!campaign_possible(current, district, kind)) {
        return false;
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::Campaign;
    command.state = static_cast<uhrwerk::Index>(current);
    command.target = static_cast<uhrwerk::Index>(district);
    command.action = kind;
    simulation_->submit(command);
    return true;
}

Dictionary UhrwerkSimulation::get_party(std::int64_t current) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->currents, current)) {
        return result;
    }
    const auto c = static_cast<std::size_t>(current);
    const uhrwerk::PoliticalCurrent& info = w->currents[c];
    result["index"] = current;
    result["id"] = to_godot(info.id);
    result["name"] = to_godot(info.name);
    result["color"] = to_godot(info.color);
    result["funds"] = c < w->party_funds.size() ? w->party_funds[c].to_double() : 0.0;
    result["donations"] = c < w->party_donations.size() ? w->party_donations[c].to_double() : 0.0;
    result["reserved"] =
        simulation_->reserved_campaign_funds(static_cast<uhrwerk::Index>(current)).to_double();
    // Nächste Wahl: Parlament, Tag und Wahlkreise mit eigenem Wahlkampf.
    const std::int64_t next = next_parliament();
    result["open"] = next >= 0;
    if (next >= 0) {
        const uhrwerk::Parliament& p = at(w->parliaments, next);
        result["parliament_name"] = to_godot(p.name);
        result["election_date_text"] = to_godot(uhrwerk::format_date_long(shown_election_day(*w, next)));
        std::int64_t districts = 0;
        const std::size_t currents = w->currents.size();
        for (std::size_t d = 0; d < w->electoral_districts.size() &&
                                w->campaign.size() == w->electoral_districts.size() * currents;
             ++d) {
            const uhrwerk::CampaignState& state = w->campaign[d * currents + c];
            if (w->electoral_districts[d].parliament == next &&
                (state.mobilization > uhrwerk::Fixed::zero() || state.persuasion > uhrwerk::Fixed::zero() ||
                 state.club > uhrwerk::Fixed::zero())) {
                ++districts;
            }
        }
        result["campaign_districts"] = districts;
    }
    // Letzte Wahl: Sitze, Abgeordnete und Fraktion.
    result["held"] = !w->elections.empty();
    if (w->elections.empty() || !valid_index(w->parliaments, w->elections.back().parliament)) {
        return result;
    }
    const uhrwerk::ElectionResult* election = &w->elections.back();
    const uhrwerk::Parliament& p = at(w->parliaments, election->parliament);
    result["last_parliament_name"] = to_godot(p.name);
    result["last_election_date_text"] = to_godot(uhrwerk::format_date_long(election->day));
    result["faction_date_text"] = to_godot(uhrwerk::format_date_long(p.faction_day));
    const std::vector<std::int32_t> seats = uhrwerk::seats_by_current(*w, *election);
    const std::vector<std::int32_t> by_faction = uhrwerk::seats_by_faction(*w, *election);
    result["seats"] = c < seats.size() ? seats[c] : 0;
    result["seated"] = static_cast<std::int64_t>(election->deputies.size());
    // Eigene Abgeordnete je Fraktion; man spielt die Fraktion mit den meisten (Abschnitt 9).
    std::vector<std::int64_t> own(p.factions.size(), 0);
    Array deputies;
    for (const uhrwerk::Deputy& deputy : election->deputies) {
        if (deputy.current != static_cast<uhrwerk::Index>(current) ||
            !valid_index(w->electoral_districts, deputy.district)) {
            continue;
        }
        const uhrwerk::ElectoralDistrict& district = at(w->electoral_districts, deputy.district);
        Dictionary entry;
        entry["district_name"] = to_godot(district.name);
        entry["state_name"] = valid_index(w->states, district.state)
                                  ? to_godot(at(w->states, district.state).short_name)
                                  : String();
        entry["faction_name"] = valid_index(p.factions, deputy.faction)
                                    ? to_godot(at(p.factions, deputy.faction).name)
                                    : to_godot(std::string_view("fraktionslos"));
        entry["position"] = deputy.position.to_double();
        deputies.push_back(entry);
        if (valid_index(p.factions, deputy.faction)) {
            ++own[static_cast<std::size_t>(deputy.faction)];
        }
    }
    result["deputies"] = deputies;
    result["factions_formed"] = w->date >= p.faction_day;
    std::size_t best = 0;
    for (std::size_t f = 1; f < own.size(); ++f) {
        if (own[f] > own[best]) {
            best = f;
        }
    }
    if (!own.empty() && own[best] > 0) {
        result["faction_name"] = to_godot(p.factions[best].name);
        result["faction_own"] = own[best];
        result["faction_seats"] = best < by_faction.size() ? by_faction[best] : 0;
    }
    return result;
}

Dictionary UhrwerkSimulation::get_campaign(std::int64_t district, std::int64_t current) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->electoral_districts, district) || !valid_index(w->currents, current)) {
        return result;
    }
    const auto c = static_cast<std::size_t>(current);
    const std::size_t currents = w->currents.size();
    const uhrwerk::ElectoralDistrict& d = at(w->electoral_districts, district);
    result["district_name"] = to_godot(d.name);
    result["seats"] = d.seats;
    // Seit M13a3: offen, solange eine Wahl aussteht und der Wahlkreis mitwählt.
    bool open = valid_index(w->parliaments, d.parliament) &&
                uhrwerk::next_election_day(*w, d.parliament).has_value() &&
                uhrwerk::district_votes(*w, static_cast<uhrwerk::Index>(district));
    result["open"] = open;
    if (valid_index(w->parliaments, d.parliament)) {
        const uhrwerk::Parliament& p = at(w->parliaments, d.parliament);
        result["parliament_name"] = to_godot(p.name);
        result["election_date_text"] =
            to_godot(uhrwerk::format_date_long(shown_election_day(*w, d.parliament)));
    }
    if (w->campaign.size() == w->electoral_districts.size() * currents) {
        const uhrwerk::CampaignState& state = w->campaign[static_cast<std::size_t>(district) * currents + c];
        result["mobilization"] = state.mobilization.to_double();
        result["persuasion"] = state.persuasion.to_double();
        result["club"] = state.club.to_double();
    }
    Array actions;
    for (std::size_t a = 0; a < uhrwerk::kCampaignActions; ++a) {
        const uhrwerk::CampaignRule& rule = w->election_rules.campaign[a];
        Dictionary entry;
        entry["action"] = static_cast<std::int64_t>(a);
        entry["name"] = to_godot(kCampaignNames[a]);
        entry["cost"] = rule.cost.to_double();
        entry["allowed"] = campaign_possible(current, district, static_cast<uhrwerk::CampaignAction>(a));
        actions.push_back(entry);
    }
    result["actions"] = actions;
    // Stimmungsbericht: Schätzung mit Fehler (Abschnitt 9), Prozent je Strömung, Summe 100.
    if (open) {
        const uhrwerk::DistrictEstimate estimate =
            uhrwerk::estimate_district(*w, static_cast<uhrwerk::Index>(district));
        const double error = w->election_rules.poll_error.to_double();
        result["boycott"] = estimate.boycott;
        result["poll_error"] = error;
        PackedFloat64Array poll;
        double total = 0.0;
        for (std::size_t k = 0; k < estimate.shares.size(); ++k) {
            const double value = std::max(0.0, estimate.shares[k].to_double() * 100.0 +
                                                   poll_offset(district, k, w->date) * error);
            poll.push_back(value);
            total += value;
        }
        for (std::int64_t k = 0; k < poll.size() && total > 0.0; ++k) {
            poll[k] = poll[k] * 100.0 / total;
        }
        result["poll"] = poll;
    }
    return result;
}

bool UhrwerkSimulation::set_player_current(std::int64_t current) {
    const uhrwerk::World* w = world();
    if (!w) {
        return false;
    }
    // Seit M12 (a3) erfährt auch der Kern die Strömung des Spielers (Befehl Spielerströmung): Die KI führt
    // für sie keinen Wahlkampf.
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::PlayerCurrent;
    if (current < 0) {
        player_current_.clear();
        if (w->player_current != uhrwerk::kNone) {
            simulation_->submit(command);
        }
        return true;
    }
    // Parteienmodus nur in Szenarien mit Parlament (Review M8: 1648 hat Strömungen, aber keine Wahl).
    if (!valid_index(w->currents, current) || w->parliaments.empty()) {
        return false;
    }
    player_current_ = at(w->currents, current).id;
    if (uhrwerk::ai_active(*w) && w->player_current != current) {
        command.state = static_cast<uhrwerk::Index>(current);
        simulation_->submit(command);
    }
    return true;
}

std::int64_t UhrwerkSimulation::get_player_current() const {
    const uhrwerk::World* w = world();
    return w && !player_current_.empty() ? w->find_current(player_current_) : uhrwerk::kNone;
}

bool UhrwerkSimulation::set_tax_rate_percent(std::int64_t state_index, std::int64_t percent) {
    return set_tax_percent(state_index, TAX_KONTRIBUTION, percent);
}

bool UhrwerkSimulation::set_tax_percent(std::int64_t state_index, std::int64_t kind, std::int64_t percent) {
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->states, state_index)) {
        return false;
    }
    if (kind < TAX_KONTRIBUTION || kind > TAX_ZOLL || percent < 0 || percent > get_max_tax_rate_percent()) {
        return false;
    }
    // Mitglieder einer Zollunion erheben den Vereinstarif (seit M9); der Kern verwürfe den Befehl.
    if (kind == TAX_ZOLL && !w->customs_unions.empty() &&
        at(w->states, state_index).customs_union != uhrwerk::kNone) {
        return false;
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::SetTaxRate;
    command.state = static_cast<uhrwerk::Index>(state_index);
    command.kind = static_cast<uhrwerk::TaxKind>(kind);
    command.value = uhrwerk::Fixed::from_ratio(percent, 100);
    simulation_->submit(command);
    return true;
}

std::int64_t UhrwerkSimulation::get_max_tax_rate_percent() const {
    const uhrwerk::World* w = world();
    // Abrunden, damit der höchste angebotene Wert nie über dem Regelmaximum liegt.
    return w ? (w->rules.tax_rate_max * uhrwerk::Fixed::from_int(100)).trunc() : 0;
}

std::int64_t UhrwerkSimulation::get_chronicle_size() const {
    const uhrwerk::World* w = world();
    return w ? static_cast<std::int64_t>(w->chronicle.size()) : 0;
}

Array UhrwerkSimulation::get_chronicle(std::int64_t from_index) const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    const auto size = static_cast<std::int64_t>(w->chronicle.size());
    if (std::max<std::int64_t>(from_index, 0) >= size) {
        return result;
    }
    // Wendepunkte (seit M11, Schritt b4): Einträge, die ein Wendepunkt geschrieben hat, je Tag mit dem
    // Standardtext „Staat: Titel – Option“ oder den eigenen Chroniktexten der Option (events.cpp).
    std::unordered_map<std::int32_t, std::vector<std::string>> turning;
    for (const uhrwerk::EventRecord& r : w->event_log) {
        if (!valid_index(w->events, r.event) || !at(w->events, r.event).turning_point) {
            continue;
        }
        const uhrwerk::EventDef& e = at(w->events, r.event);
        if (!valid_index(e.options, r.option)) {
            continue;
        }
        std::vector<std::string>& texts = turning[uhrwerk::to_day_number(r.date)];
        bool own_entry = false;
        for (const uhrwerk::Effect& effect : at(e.options, r.option).effects) {
            if (effect.kind == uhrwerk::EffectKind::Chronicle) {
                texts.push_back(effect.name);
                own_entry = true;
            }
        }
        if (!own_entry) {
            std::string text =
                valid_index(w->states, r.state) ? at(w->states, r.state).short_name + ": " : "";
            texts.push_back(text + e.title + " – " + at(e.options, r.option).text);
        }
    }
    for (std::int64_t i = std::max<std::int64_t>(from_index, 0); i < size; ++i) {
        const uhrwerk::ChronicleEntry& entry = at(w->chronicle, i);
        Dictionary item;
        item["date_text"] = to_godot(uhrwerk::format_date(entry.date));
        item["text"] = to_godot(entry.text);
        item["location_index"] = entry.location;
        item["state_index"] = entry.state;
        const auto day = turning.find(uhrwerk::to_day_number(entry.date));
        item["turning_point"] = day != turning.end() && std::find(day->second.begin(), day->second.end(),
                                                                  entry.text) != day->second.end();
        result.push_back(item);
    }
    return result;
}

String UhrwerkSimulation::get_checksum() const {
    if (!simulation_) {
        return String();
    }
    return to_godot(uhrwerk::hex16(simulation_->checksum()));
}

String UhrwerkSimulation::save_game(const String& path, std::int64_t player_state_index) const {
    const uhrwerk::World* w = world();
    if (!w) {
        return "Keine Simulation geladen.";
    }
    const std::string tag =
        valid_index(w->states, player_state_index) ? at(w->states, player_state_index).tag : player_tag_;
    const std::filesystem::path file = to_path(path);
    std::error_code ignored;
    if (file.has_parent_path()) {
        std::filesystem::create_directories(file.parent_path(), ignored);
    }
    std::string error;
    if (!uhrwerk::write_file(file, uhrwerk::save_game(*simulation_, uhrwerk::SaveMeta{tag, player_current_}),
                             &error)) {
        return to_godot(error);
    }
    return String();
}

String UhrwerkSimulation::load_game(const String& data_path, const String& path) {
    std::string error;
    const auto bytes = uhrwerk::read_file(to_path(path), &error);
    if (!bytes) {
        return to_godot(error);
    }
    const auto info = uhrwerk::read_save_info(*bytes, &error);
    if (!info) {
        return to_godot(error);
    }
    uhrwerk::LoadResult result = uhrwerk::load_world(to_path(data_path), info->scenario);
    if (!result.ok()) {
        String errors = "Die Spieldaten konnten nicht geladen werden.";
        for (const uhrwerk::LoadIssue& issue : result.issues) {
            if (issue.fatal) {
                errors += "\n" + to_godot(issue.to_string());
            }
        }
        return errors;
    }
    PackedStringArray warnings;
    for (const uhrwerk::LoadIssue& issue : result.issues) {
        if (!issue.fatal) {
            warnings.push_back(to_godot(issue.to_string()));
        }
    }
    uhrwerk::LoadedGame game = uhrwerk::load_game(*bytes, std::move(*result.world));
    if (!game.ok()) {
        return to_godot(game.error); // die laufende Simulation bleibt unverändert
    }
    simulation_ = std::make_unique<uhrwerk::Simulation>(std::move(*game.simulation));
    load_warnings_ = warnings;
    const uhrwerk::World& w = simulation_->world();
    player_tag_ = w.find_state(info->player_tag) != uhrwerk::kNone ? info->player_tag : w.scenario.player_tag;
    player_current_ =
        w.find_current(info->player_current) != uhrwerk::kNone ? info->player_current : std::string();
    content_ = info->content;
    build_lookups();
    return String();
}

Dictionary UhrwerkSimulation::get_save_info(const String& path) const {
    Dictionary result;
    std::string error;
    const auto bytes = uhrwerk::read_file(to_path(path), &error);
    const auto info = bytes ? uhrwerk::read_save_info(*bytes, &error) : std::nullopt;
    if (!info) {
        result["ok"] = false;
        result["compatible"] = false;
        result["reason"] = to_godot(error);
        return result;
    }
    result["ok"] = true;
    result["format_version"] = static_cast<std::int64_t>(info->format_version);
    result["game_version"] = to_godot(info->game_version);
    result["scenario"] = to_godot(info->scenario);
    Dictionary date;
    date["year"] = info->date.year;
    date["month"] = info->date.month;
    date["day"] = info->date.day;
    result["date"] = date;
    result["date_text"] = to_godot(uhrwerk::format_date_long(info->date));
    result["days"] = info->days;
    result["player_tag"] = to_godot(info->player_tag);
    result["player_current"] = to_godot(info->player_current); // Parteienmodus (seit M8), leer = Staat
    // Kurzname für Listen (z. B. „Brandenburg“), sonst der volle Name oder die Kennung. Nur im
    // laufenden Szenario nachschlagen: ein anderes kennt andere Staaten (seit M7).
    String player_name = to_godot(info->player_tag);
    if (const uhrwerk::World* w = world(); w != nullptr && w->scenario.id == info->scenario) {
        const uhrwerk::Index state = w->find_state(info->player_tag);
        if (state != uhrwerk::kNone) {
            const uhrwerk::State& s = at(w->states, state);
            player_name = to_godot(s.short_name.empty() ? s.name : s.short_name);
        }
    }
    result["player_name"] = player_name;
    result["checksum"] = to_godot(uhrwerk::hex16(info->checksum));
    if (!simulation_) {
        result["compatible"] = false;
        result["reason"] = "Keine Spieldaten geladen.";
    } else if (info->content != content_) {
        std::string parts;
        for (const std::string_view part : uhrwerk::content_differences(info->content, content_)) {
            parts += parts.empty() ? "" : ", ";
            parts += part;
        }
        result["compatible"] = false;
        result["reason"] = to_godot("Passt nicht zu den Spieldaten (abweichend: " + parts + ").");
    } else {
        result["compatible"] = true;
        result["reason"] = String();
    }
    return result;
}

std::int64_t UhrwerkSimulation::get_save_format_version() const {
    return uhrwerk::kSaveFormatVersion;
}

// ─── Ereignisse (seit M11, Schritt b4, ereignisse.md Abschnitt 9) ──────────────────────────────────────

namespace {

// Texte mit Umlauten über to_godot (UTF-8); ein String aus const char* liest Latin-1.
String event_kind_text(uhrwerk::EventKind kind) {
    switch (kind) {
    case uhrwerk::EventKind::Historical:
        return "historisch";
    case uhrwerk::EventKind::Dynamic:
        return "dynamisch";
    case uhrwerk::EventKind::Atmospheric:
        return to_godot("atmosphärisch");
    }
    return {};
}

String revolt_kind_text(uhrwerk::RevoltKind kind) {
    switch (kind) {
    case uhrwerk::RevoltKind::Barricades:
        return to_godot("Barrikadenkämpfe");
    case uhrwerk::RevoltKind::Agrarian:
        return "Agrarunruhen";
    case uhrwerk::RevoltKind::Uprising:
        return "Erhebung";
    }
    return {};
}

PackedStringArray to_godot_lines(const std::vector<std::string>& lines) {
    PackedStringArray result;
    for (const std::string& line : lines) {
        result.append(to_godot(line));
    }
    return result;
}

} // namespace

Array UhrwerkSimulation::get_open_events() const {
    Array result;
    const uhrwerk::World* w = world();
    // Godot fragt jeden Tag: ohne offene Ereignisse ohne den Aufbau des Zwischenspeichers.
    if (!w || !uhrwerk::events_active(*w) || w->pending_events.empty()) {
        return result;
    }
    uhrwerk::EventCache cache;
    uhrwerk::rebuild_event_cache(*w, cache);
    for (const uhrwerk::PendingEvent& p : w->pending_events) {
        if (!valid_index(w->events, p.event)) {
            continue;
        }
        const uhrwerk::EventDef& e = at(w->events, p.event);
        Dictionary entry;
        entry["event_index"] = p.event;
        entry["id"] = to_godot(e.id);
        entry["title"] = to_godot(e.title);
        entry["text"] = to_godot(e.text);
        entry["state_index"] = p.state;
        entry["state_name"] = valid_index(w->states, p.state) ? short_name(*w, p.state) : String();
        entry["date_text"] = e.has_date ? to_godot(uhrwerk::format_date_long(e.date)) : String();
        entry["since_text"] = to_godot(uhrwerk::format_date_long(p.since));
        entry["due_text"] = to_godot(uhrwerk::format_date_long(p.due));
        entry["days_left"] = uhrwerk::to_day_number(p.due) - uhrwerk::to_day_number(w->date);
        entry["source"] = to_godot(e.source);
        entry["kind_text"] = event_kind_text(e.kind);
        entry["turning_point"] = e.turning_point;
        // Schon gewählt, aber erst am nächsten Tag wirksam (Befehl wartet).
        std::int64_t chosen = -1;
        for (const uhrwerk::LoggedCommand& c : simulation_->command_log()) {
            if (c.day == simulation_->days_elapsed() && c.command.type == uhrwerk::CommandType::EventChoice &&
                c.command.state == p.state && c.command.value == uhrwerk::Fixed::from_int(p.event)) {
                chosen = c.command.target;
            }
        }
        entry["chosen_option"] = chosen;
        const std::vector<uhrwerk::Fixed> weights = uhrwerk::option_weights(*w, cache, e, p.state);
        Array options;
        for (std::size_t o = 0; o < e.options.size(); ++o) {
            const uhrwerk::EventOption& option = e.options[o];
            Dictionary item;
            item["index"] = static_cast<std::int64_t>(o);
            item["id"] = to_godot(option.id);
            item["text"] = to_godot(option.text);
            item["historical"] = option.historical;
            item["available"] = o < weights.size() && weights[o] >= uhrwerk::Fixed::zero();
            item["effects"] = to_godot_lines(uhrwerk::describe_effects(*w, option.effects));
            options.append(item);
        }
        entry["options"] = options;
        entry["default_option"] = uhrwerk::default_option(*w, cache, e, p.state);
        result.append(entry);
    }
    return result;
}

bool UhrwerkSimulation::choose_event_option(std::int64_t event_index, std::int64_t option_index) {
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->events, event_index) ||
        !valid_index(at(w->events, event_index).options, option_index)) {
        return false;
    }
    const auto pending = std::find_if(w->pending_events.begin(), w->pending_events.end(),
                                      [&](const uhrwerk::PendingEvent& p) { return p.event == event_index; });
    if (pending == w->pending_events.end()) {
        return false;
    }
    uhrwerk::EventCache cache;
    uhrwerk::rebuild_event_cache(*w, cache);
    const std::vector<uhrwerk::Fixed> weights =
        uhrwerk::option_weights(*w, cache, at(w->events, event_index), pending->state);
    if (!valid_index(weights, option_index) || at(weights, option_index) < uhrwerk::Fixed::zero()) {
        return false; // nicht verfügbar
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::EventChoice;
    command.state = pending->state;
    command.value = uhrwerk::Fixed::from_int(event_index);
    command.target = static_cast<uhrwerk::Index>(option_index);
    simulation_->submit(command);
    return true;
}

Dictionary UhrwerkSimulation::get_game_settings() const {
    Dictionary result;
    const uhrwerk::World* w = world();
    result["active"] = w != nullptr && uhrwerk::ai_active(*w);
    if (!w || !uhrwerk::ai_active(*w)) {
        return result;
    }
    const uhrwerk::Index chosen = uhrwerk::ai_difficulty(*w);
    Array levels;
    for (std::size_t i = 0; i < w->ai_difficulties.size(); ++i) {
        const uhrwerk::DifficultyDef& d = w->ai_difficulties[i];
        Dictionary level;
        level["id"] = to_godot(d.id);
        level["name"] = to_godot(d.name);
        level["text"] = to_godot(d.text);
        level["default"] = static_cast<uhrwerk::Index>(i) == w->ai_rules.default_difficulty;
        levels.append(level);
    }
    result["difficulties"] = levels;
    result["difficulty_id"] =
        chosen != uhrwerk::kNone ? to_godot(at(w->ai_difficulties, chosen).id) : String();
    result["difficulty_name"] =
        chosen != uhrwerk::kNone ? to_godot(at(w->ai_difficulties, chosen).name) : String();
    result["open"] = w->ai_rules.open_agenda || w->game_settings.open;
    // Nur am ersten Tag einer Partie (der Befehl wirkt beim ersten Schritt).
    result["editable"] = simulation_->days_elapsed() == 0;
    return result;
}

bool UhrwerkSimulation::set_game_settings(const String& difficulty_id, bool open) {
    const uhrwerk::World* w = world();
    if (!w || !uhrwerk::ai_active(*w) || simulation_->days_elapsed() != 0) {
        return false;
    }
    uhrwerk::Index difficulty = uhrwerk::kNone;
    if (!difficulty_id.is_empty()) {
        difficulty = w->find_difficulty(difficulty_id.utf8().get_data());
        if (difficulty == uhrwerk::kNone) {
            return false;
        }
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::GameSettings;
    command.state = difficulty;
    command.value = open ? uhrwerk::Fixed::one() : uhrwerk::Fixed::zero();
    simulation_->submit(command);
    return true;
}

bool UhrwerkSimulation::set_event_player(std::int64_t state_index) {
    const uhrwerk::World* w = world();
    if (!w || !uhrwerk::events_active(*w) || (state_index != -1 && !valid_index(w->states, state_index))) {
        return false;
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::PlayerState;
    command.state = static_cast<uhrwerk::Index>(state_index);
    simulation_->submit(command);
    return true;
}

std::int64_t UhrwerkSimulation::get_event_player() const {
    const uhrwerk::World* w = world();
    return w ? w->event_player : -1;
}

// ─── Fenster der Bereiche (seit M13a5b) ─────────────────────────────────────

Dictionary UhrwerkSimulation::get_state_society(std::int64_t state_index) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->states, state_index)) {
        return result;
    }
    People sum;
    sum.per_confession.resize(w->confessions.size());
    sum.per_culture.resize(w->cultures.size());
    double living = 0.0;
    double literacy = 0.0;
    uhrwerk::PopulationBalance balance;
    std::vector<uhrwerk::Fixed> professions(w->professions.size());
    std::int64_t locations = 0;
    std::int64_t cities = 0;
    for (std::size_t l = 0; l < w->locations.size(); ++l) {
        const uhrwerk::Location& loc = w->locations[l];
        if (loc.owner != state_index || !loc.has_data()) {
            continue;
        }
        ++locations;
        cities += loc.is_city ? 1 : 0;
        const auto index = static_cast<std::int64_t>(l);
        const People people = people_of(*w, index);
        sum.total += people.total;
        sum.children += people.children;
        sum.young += people.young;
        sum.working += people.working;
        sum.old += people.old;
        living += people.living_standard * people.total.to_double();
        literacy += people.literacy * people.total.to_double();
        for (std::size_t c = 0; c < sum.per_confession.size(); ++c) {
            sum.per_confession[c] += people.per_confession[c];
        }
        for (std::size_t c = 0; c < sum.per_culture.size(); ++c) {
            sum.per_culture[c] += people.per_culture[c];
        }
        if (valid_index(w->balance_previous, index)) {
            add_balance(balance, at(w->balance_previous, index));
        }
        const std::vector<uhrwerk::Fixed> by_profession = people_by_profession(*w, index);
        for (std::size_t p = 0; p < professions.size(); ++p) {
            professions[p] += by_profession[p];
        }
    }
    const double total = sum.total.to_double();
    result["population"] = sum.total.round();
    result["locations"] = locations;
    result["cities"] = cities;
    result["children"] = sum.children.round();
    result["young"] = sum.young.round();
    result["working"] = sum.working.round();
    result["old"] = sum.old.round();
    result["living_standard"] = total > 0.0 ? living / total : 0.0;
    result["literacy_percent"] = total > 0.0 ? literacy / total * 100.0 : 0.0;
    result["confessions"] = shares_of(sum.per_confession, w->confessions, total);
    result["cultures"] = shares_of(sum.per_culture, w->cultures, total);
    result["balance_previous"] = balance_to_godot(balance);
    Array rows;
    for (std::size_t p = 0; p < professions.size(); ++p) {
        if (professions[p].round() <= 0) {
            continue;
        }
        Dictionary row;
        row["name"] = to_godot(w->professions[p].name);
        row["stratum"] = static_cast<std::int64_t>(w->professions[p].stratum);
        row["people"] = professions[p].round();
        row["percent"] = total > 0.0 ? professions[p].to_double() * 100.0 / total : 0.0;
        rows.push_back(row);
    }
    result["professions"] = rows;
    return result;
}

Dictionary UhrwerkSimulation::get_state_economy(std::int64_t state_index) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->states, state_index)) {
        return result;
    }
    const std::size_t goods = w->goods.size();
    const std::size_t businesses = w->businesses.size();
    std::vector<double> supply(goods, 0.0);
    std::vector<double> demand(goods, 0.0);
    std::vector<double> price_weight(goods, 0.0);
    std::vector<double> price_sum(goods, 0.0);
    std::vector<uhrwerk::Fixed> heads(businesses);
    std::vector<std::int64_t> market_places(w->markets.size(), 0);
    double production = 0.0;
    double taxes = 0.0;
    for (std::size_t l = 0; l < w->locations.size(); ++l) {
        const uhrwerk::Location& loc = w->locations[l];
        if (loc.owner != state_index || !loc.has_data()) {
            continue;
        }
        const auto index = static_cast<uhrwerk::Index>(l);
        const uhrwerk::LocationEconomy economy = simulation_->location_economy(index);
        production += economy.production.to_double();
        taxes += economy.total_taxes().to_double();
        if (valid_index(market_places, loc.market)) {
            ++market_places[static_cast<std::size_t>(loc.market)];
        }
        const uhrwerk::MarketState* market =
            valid_index(w->market_states, loc.market) ? &at(w->market_states, loc.market) : nullptr;
        for (std::size_t g = 0; g < goods; ++g) {
            const std::size_t slot = l * goods + g;
            const double s = slot < w->location_supply.size() ? w->location_supply[slot].to_double() : 0.0;
            const double d = slot < w->location_demand.size() ? w->location_demand[slot].to_double() : 0.0;
            supply[g] += s;
            demand[g] += d;
            // Preis nach Umsatz gemittelt (Angebot und Nachfrage des Ortes zum Preis seines Marktes).
            if (market != nullptr && g < market->price.size() && s + d > 0.0) {
                price_sum[g] += market->price[g].to_double() * (s + d);
                price_weight[g] += s + d;
            }
        }
        const std::vector<uhrwerk::Fixed> people = people_by_profession(*w, static_cast<std::int64_t>(l));
        for (std::size_t b = 0; b < businesses; ++b) {
            const std::size_t slot = l * businesses + b;
            const uhrwerk::BusinessType& business = w->businesses[b];
            if (slot < w->business_shares.size() && valid_index(people, business.profession)) {
                heads[b] += w->business_shares[slot] * at(people, business.profession);
            }
        }
    }
    Array good_rows;
    for (std::size_t g = 0; g < goods; ++g) {
        if (supply[g] <= 0.0 && demand[g] <= 0.0) {
            continue;
        }
        const uhrwerk::Good& good = w->goods[g];
        const double price = price_weight[g] > 0.0 ? price_sum[g] / price_weight[g] : 0.0;
        const double base = good.base_price.to_double();
        Dictionary row;
        row["name"] = to_godot(good.name);
        row["color"] = to_godot(good.color);
        row["supply"] = supply[g];
        row["demand"] = demand[g];
        row["price"] = price;
        row["price_factor"] = base > 0.0 ? price / base : 0.0;
        good_rows.push_back(row);
    }
    Array business_rows;
    for (std::size_t b = 0; b < businesses; ++b) {
        if (heads[b].round() <= 0) {
            continue;
        }
        Dictionary row;
        row["name"] = to_godot(w->businesses[b].name);
        row["profession"] = to_godot(at(w->professions, w->businesses[b].profession).name);
        row["people"] = heads[b].round();
        business_rows.push_back(row);
    }
    Array market_rows;
    for (std::size_t m = 0; m < market_places.size(); ++m) {
        if (market_places[m] == 0) {
            continue;
        }
        Dictionary row;
        row["name"] = to_godot(w->markets[m].name);
        row["locations"] = market_places[m];
        market_rows.push_back(row);
    }
    const uhrwerk::State& state = at(w->states, state_index);
    result["goods"] = good_rows;
    result["businesses"] = business_rows;
    result["markets"] = market_rows;
    result["production_month"] = production;
    result["taxes_month"] = taxes;
    result["customs_zone_name"] = to_godot(valid_index(w->customs_unions, state.customs_union)
                                               ? at(w->customs_unions, state.customs_union).name
                                               : state.short_name);
    result["customs_tariff_percent"] =
        (w->customs_unions.empty()
             ? state.tax_rate(uhrwerk::TaxKind::Zoll)
             : uhrwerk::zone_tariff(*w, uhrwerk::customs_zone(*w, static_cast<uhrwerk::Index>(state_index))))
            .to_double() *
        100.0;
    return result;
}

Array UhrwerkSimulation::get_state_laws(std::int64_t state_index) const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->states, state_index)) {
        return result;
    }
    const uhrwerk::State& state = at(w->states, state_index);
    for (std::size_t g = 0; g < w->law_groups.size(); ++g) {
        const uhrwerk::LawGroup& group = w->law_groups[g];
        const uhrwerk::Index current = g < state.laws.size() ? state.laws[g] : group.standard;
        Dictionary entry;
        entry["id"] = to_godot(group.id);
        entry["name"] = to_godot(group.name);
        entry["current"] = current;
        Array options;
        for (std::size_t o = 0; o < group.options.size(); ++o) {
            const uhrwerk::LawOption& option = group.options[o];
            Dictionary item;
            item["id"] = to_godot(option.id);
            item["name"] = to_godot(option.name);
            item["unrest"] = option.unrest.to_double();
            item["current"] = static_cast<uhrwerk::Index>(o) == current;
            // Wünsche der Interessengruppen an diese Option (Zustimmung −10 … +10 auf Dauer).
            Array wishes;
            for (const uhrwerk::InterestGroup& ig : w->interest_groups) {
                for (const uhrwerk::LawWish& wish : ig.wishes) {
                    if (wish.law == static_cast<uhrwerk::Index>(g) &&
                        wish.option == static_cast<uhrwerk::Index>(o)) {
                        Dictionary wd;
                        wd["group"] = to_godot(ig.name);
                        wd["color"] = to_godot(ig.color);
                        wd["value"] = wish.approval.to_double();
                        wishes.append(wd);
                    }
                }
            }
            item["wishes"] = wishes;
            options.append(item);
        }
        entry["options"] = options;
        result.append(entry);
    }
    return result;
}

Array UhrwerkSimulation::get_votes() const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    for (auto it = w->vote_results.rbegin(); it != w->vote_results.rend(); ++it) {
        if (!valid_index(w->votes, it->vote)) {
            continue;
        }
        const uhrwerk::VoteDef& def = at(w->votes, it->vote);
        Dictionary entry;
        entry["title"] = to_godot(def.title);
        entry["parliament_index"] = def.parliament;
        entry["parliament_name"] = valid_index(w->parliaments, def.parliament)
                                       ? to_godot(at(w->parliaments, def.parliament).name)
                                       : String();
        entry["date_text"] = to_godot(uhrwerk::format_date_long(it->date));
        entry["yes"] = it->yes;
        entry["no"] = it->no;
        entry["accepted"] = it->accepted;
        result.append(entry);
    }
    return result;
}

Array UhrwerkSimulation::get_office_history(std::int64_t state_index) const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->states, state_index)) {
        return result;
    }
    for (auto it = w->office_history.rbegin(); it != w->office_history.rend(); ++it) {
        if (it->state != state_index || !valid_index(w->persons, it->person)) {
            continue;
        }
        const uhrwerk::PersonDef& p = at(w->persons, it->person);
        Dictionary entry;
        entry["name"] = to_godot(p.name);
        entry["role"] = it->role == uhrwerk::PersonRole::Ruler ? String("herrscher") : String("minister");
        entry["title"] =
            to_godot(!p.title.empty()                         ? p.title
                     : it->role == uhrwerk::PersonRole::Ruler ? std::string("Herrscher")
                                                              : std::string("leitender Minister"));
        entry["from_text"] = to_godot(uhrwerk::format_date_long(std::max(p.from, w->scenario.start)));
        entry["to_text"] = to_godot(uhrwerk::format_date_long(it->to));
        result.append(entry);
    }
    return result;
}

Dictionary UhrwerkSimulation::get_state_overview(std::int64_t state_index) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->states, state_index)) {
        return result;
    }
    const auto state = static_cast<uhrwerk::Index>(state_index);
    std::vector<std::uint8_t> kreise(w->districts.size(), 0);
    std::int64_t locations = 0;
    std::int64_t kreis_count = 0;
    for (const uhrwerk::Location& loc : w->locations) {
        if (loc.owner != state) {
            continue;
        }
        ++locations;
        if (valid_index(kreise, loc.district) && kreise[static_cast<std::size_t>(loc.district)] == 0) {
            kreise[static_cast<std::size_t>(loc.district)] = 1;
            ++kreis_count;
        }
    }
    const auto measures = uhrwerk::legacy_measures(*w, state);
    result["locations"] = locations;
    result["kreise"] = kreis_count;
    result["legitimacy"] = measures[static_cast<std::size_t>(uhrwerk::LegacyMeasure::Approval)].to_double();
    result["unrest"] = measures[static_cast<std::size_t>(uhrwerk::LegacyMeasure::Unrest)].to_double();
    result["events"] = uhrwerk::events_active(*w);
    return result;
}

// ─── Vermächtnis, Jahrbuch und Ende (seit M13a4) ────────────────────────────

Dictionary UhrwerkSimulation::get_legacy(std::int64_t state_index) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    result["active"] = w != nullptr && w->legacy_rules.active;
    Array dimensions;
    if (w == nullptr || !w->legacy_rules.active || !valid_index(w->states, state_index)) {
        result["dimensions"] = dimensions;
        return result;
    }
    const std::vector<uhrwerk::LegacyValue> values =
        uhrwerk::legacy(*w, simulation_->event_cache(), static_cast<uhrwerk::Index>(state_index));
    for (std::size_t d = 0; d < values.size() && d < w->legacy_rules.dimensions.size(); ++d) {
        const uhrwerk::LegacyDimension& def = w->legacy_rules.dimensions[d];
        Dictionary entry;
        entry["id"] = to_godot(def.id);
        entry["name"] = to_godot(def.name);
        entry["value"] = values[d].value.to_double();
        entry["base"] = values[d].base.to_double();
        entry["sentence"] = to_godot(uhrwerk::legacy_sentence(def, values[d].value));
        Array factors;
        for (const uhrwerk::LegacyContribution& c : values[d].factors) {
            Dictionary f;
            f["name"] = to_godot(c.name);
            f["value"] = c.value.to_double();
            f["active"] = c.active;
            factors.append(f);
        }
        entry["factors"] = factors;
        dimensions.append(entry);
    }
    result["dimensions"] = dimensions;
    return result;
}

Dictionary UhrwerkSimulation::get_yearbook(std::int64_t state_index) const {
    Dictionary result;
    PackedStringArray columns;
    Array rows;
    const uhrwerk::World* w = world();
    if (w != nullptr && w->legacy_rules.active && valid_index(w->states, state_index)) {
        for (const uhrwerk::LegacyMeasure m : uhrwerk::kYearbookMeasures) {
            columns.append(to_godot(uhrwerk::legacy_measure_name(m)));
        }
        for (const uhrwerk::LegacyDimension& d : w->legacy_rules.dimensions) {
            columns.append(to_godot(d.name));
        }
        const std::size_t count = uhrwerk::yearbook_columns(*w);
        for (const uhrwerk::YearRecord& r : w->yearbook) {
            const std::size_t first = static_cast<std::size_t>(state_index) * count;
            if (first + count > r.values.size()) {
                continue;
            }
            Dictionary row;
            row["date"] = date_dictionary(r.date);
            row["year"] = r.date.year;
            row["date_text"] = to_godot(uhrwerk::format_date(r.date));
            PackedFloat64Array values;
            for (std::size_t c = 0; c < count; ++c) {
                values.append(r.values[first + c].to_double());
            }
            row["values"] = values;
            rows.append(row);
        }
    }
    result["columns"] = columns;
    result["measure_count"] = static_cast<std::int64_t>(uhrwerk::kYearbookMeasures.size());
    result["rows"] = rows;
    return result;
}

bool UhrwerkSimulation::is_game_over() const {
    const uhrwerk::World* w = world();
    return w != nullptr && uhrwerk::game_over(*w);
}

Dictionary UhrwerkSimulation::get_verdict() const {
    Dictionary result;
    const uhrwerk::World* w = world();
    result["has_end"] = w != nullptr && w->scenario.has_end;
    result["over"] = is_game_over();
    if (w == nullptr) {
        return result;
    }
    result["active"] = w->legacy_rules.active;
    result["end_date_text"] =
        w->scenario.has_end ? to_godot(uhrwerk::format_date_long(w->scenario.end)) : String();
    if (w->scenario.has_end) {
        result["end_date"] = date_dictionary(w->scenario.end);
    }
    result["start_date_text"] = to_godot(uhrwerk::format_date_long(w->scenario.start));
    // Wendepunkte der Partie in zeitlicher Folge.
    Array turning;
    for (const uhrwerk::EventRecord& r : w->event_log) {
        if (!valid_index(w->events, r.event) || !at(w->events, r.event).turning_point) {
            continue;
        }
        const uhrwerk::EventDef& e = at(w->events, r.event);
        Dictionary entry;
        entry["date_text"] = to_godot(uhrwerk::format_date_long(r.date));
        entry["title"] = to_godot(e.title);
        entry["state_name"] = valid_index(w->states, r.state) ? short_name(*w, r.state) : String();
        entry["option_text"] =
            valid_index(e.options, r.option) ? to_godot(at(e.options, r.option).text) : String();
        turning.append(entry);
    }
    result["turning_points"] = turning;
    // Beendete Kriege der Partie mit Ausgang.
    Array wars;
    for (const uhrwerk::WarResult& r : w->war_results) {
        if (!valid_index(w->states, r.attacker) || !valid_index(w->states, r.defender)) {
            continue;
        }
        Dictionary entry;
        entry["attacker_name"] = short_name(*w, r.attacker);
        entry["defender_name"] = short_name(*w, r.defender);
        entry["started_text"] = to_godot(uhrwerk::format_date_long(r.started));
        entry["ended_text"] = to_godot(uhrwerk::format_date_long(r.ended));
        entry["winner"] = r.winner;
        entry["winner_name"] = r.winner > 0   ? short_name(*w, r.attacker)
                               : r.winner < 0 ? short_name(*w, r.defender)
                                              : String();
        entry["transfers"] = r.transfers;
        wars.append(entry);
    }
    result["wars"] = wars;
    return result;
}

Array UhrwerkSimulation::get_event_log(std::int64_t limit) const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w || limit <= 0) {
        return result;
    }
    // Neueste zuerst.
    for (auto it = w->event_log.rbegin(); it != w->event_log.rend() && result.size() < limit; ++it) {
        if (!valid_index(w->events, it->event)) {
            continue;
        }
        const uhrwerk::EventDef& e = at(w->events, it->event);
        Dictionary entry;
        entry["event_index"] = it->event;
        entry["title"] = to_godot(e.title);
        entry["date_text"] = to_godot(uhrwerk::format_date_long(it->date));
        entry["state_index"] = it->state;
        entry["state_name"] = valid_index(w->states, it->state) ? short_name(*w, it->state) : String();
        entry["turning_point"] = e.turning_point;
        entry["kind_text"] = event_kind_text(e.kind);
        const bool has_option = valid_index(e.options, it->option);
        entry["option_text"] = has_option ? to_godot(at(e.options, it->option).text) : String();
        entry["historical"] = has_option && at(e.options, it->option).historical;
        result.append(entry);
    }
    return result;
}

Array UhrwerkSimulation::get_journals() const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w || !uhrwerk::events_active(*w)) {
        return result;
    }
    uhrwerk::EventCache cache;
    uhrwerk::rebuild_event_cache(*w, cache);
    for (std::size_t j = 0; j < w->journals.size(); ++j) {
        const uhrwerk::JournalDef& def = w->journals[j];
        const uhrwerk::JournalState state =
            j < w->journal_states.size() ? w->journal_states[j] : uhrwerk::JournalState{};
        Dictionary entry;
        entry["index"] = static_cast<std::int64_t>(j);
        entry["id"] = to_godot(def.id);
        entry["title"] = to_godot(def.title);
        entry["text"] = to_godot(def.text);
        entry["end_text"] = def.has_end ? to_godot(uhrwerk::format_date_long(def.end)) : String();
        entry["historical"] = to_godot(def.historical); // seit M13a4: Vergleich im Urteil der Geschichte
        entry["started"] = valid_index(def.stages, state.stage);
        if (valid_index(def.stages, state.stage)) {
            const uhrwerk::JournalStage& stage = at(def.stages, state.stage);
            entry["stage_id"] = to_godot(stage.id);
            entry["stage_title"] = to_godot(stage.title);
            entry["stage_text"] = to_godot(stage.text);
            entry["finished"] = stage.outcome;
            entry["rating"] = to_godot(stage.rating);
            entry["since_text"] = to_godot(uhrwerk::format_date_long(state.since));
            Array goals;
            for (const uhrwerk::JournalGoal& goal : stage.goals) {
                const uhrwerk::JournalGoalStatus status = uhrwerk::journal_goal_status(*w, cache, goal);
                Dictionary g;
                g["text"] = to_godot(goal.text);
                g["done"] = status.done;
                g["counting"] = goal.counting;
                g["count"] = status.count;
                g["target"] = goal.target;
                goals.append(g);
            }
            entry["goals"] = goals;
        } else {
            entry["finished"] = false;
            entry["goals"] = Array();
        }
        Array history;
        for (const uhrwerk::JournalStep& step : w->journal_history) {
            if (step.journal != static_cast<uhrwerk::Index>(j) || !valid_index(def.stages, step.stage)) {
                continue;
            }
            Dictionary h;
            h["title"] = to_godot(at(def.stages, step.stage).title);
            h["date_text"] = to_godot(uhrwerk::format_date_long(step.date));
            h["outcome"] = at(def.stages, step.stage).outcome;
            history.append(h);
        }
        entry["history"] = history;
        Array outcomes;
        for (const uhrwerk::JournalStage& stage : def.stages) {
            if (!stage.outcome) {
                continue;
            }
            Dictionary o;
            o["id"] = to_godot(stage.id);
            o["title"] = to_godot(stage.title);
            o["rating"] = to_godot(stage.rating);
            o["text"] = to_godot(stage.text);
            o["fallback"] = stage.fallback;
            outcomes.append(o);
        }
        entry["outcomes"] = outcomes;
        result.append(entry);
    }
    return result;
}

Dictionary UhrwerkSimulation::get_state_events(std::int64_t state_index) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !uhrwerk::events_active(*w) || !valid_index(w->states, state_index)) {
        return result;
    }
    const auto state = static_cast<uhrwerk::Index>(state_index);
    const uhrwerk::State& s = at(w->states, state_index);
    Array laws;
    for (std::size_t g = 0; g < w->law_groups.size(); ++g) {
        const uhrwerk::LawGroup& group = w->law_groups[g];
        const uhrwerk::Index option = g < s.laws.size() ? s.laws[g] : group.standard;
        Dictionary law;
        law["group"] = to_godot(group.name);
        law["option"] =
            valid_index(group.options, option) ? to_godot(at(group.options, option).name) : String();
        laws.append(law);
    }
    result["laws"] = laws;
    Array approvals;
    const std::size_t groups = w->interest_groups.size();
    for (std::size_t g = 0; g < groups; ++g) {
        const std::size_t slot = static_cast<std::size_t>(state_index) * groups + g;
        Dictionary a;
        a["id"] = to_godot(w->interest_groups[g].id);
        a["name"] = to_godot(w->interest_groups[g].name);
        a["color"] = to_godot(w->interest_groups[g].color);
        a["value"] = slot < w->approval.size() ? w->approval[slot].to_double() : 0.0;
        approvals.append(a);
    }
    result["approvals"] = approvals;
    uhrwerk::EventCache cache;
    uhrwerk::rebuild_event_cache(*w, cache);
    result["unrest"] = valid_index(w->unrest, state_index) ? at(w->unrest, state_index).to_double() : 0.0;
    result["unrest_target"] = uhrwerk::unrest_target(*w, cache, state).to_double();
    // Seit M13a5c: Bestandteile des Zielwerts für den Tooltip „Unruhe“.
    const uhrwerk::UnrestCauses causes = uhrwerk::unrest_causes(*w, cache, state);
    Array cause_rows;
    for (const auto& [name, value] :
         {std::pair<const char*, uhrwerk::Fixed>{"Not der Unterschicht", causes.misery},
          {"Teuerung des Getreides", causes.dearth},
          {"Verfassung", causes.constitution},
          {"Zensur und Gesetze", causes.censorship},
          {"Unruhe der Nachbarn", causes.contagion},
          {"Zustimmung der Gruppen", causes.approval}}) {
        Dictionary row;
        row["name"] = String::utf8(name);
        row["value"] = value.to_double();
        cause_rows.append(row);
    }
    result["unrest_causes"] = cause_rows;
    Dictionary revolt;
    for (const uhrwerk::Revolt& r : w->revolts) {
        if (r.state == state) {
            revolt["kind_text"] = revolt_kind_text(r.kind);
            revolt["strength"] = r.strength.to_double();
            revolt["since_text"] = to_godot(uhrwerk::format_date_long(r.start));
            revolt["helper_index"] = r.helper;
            revolt["helper_name"] = valid_index(w->states, r.helper) ? short_name(*w, r.helper) : String();
        }
    }
    result["revolt"] = revolt;
    Array timed;
    for (const uhrwerk::TimedEffect& t : w->timed_effects) {
        if (t.state != state) {
            continue;
        }
        Dictionary entry;
        entry["text"] = to_godot(t.text);
        entry["until_text"] = to_godot(uhrwerk::format_date_long(t.until));
        timed.append(entry);
    }
    result["timed"] = timed;
    result["pending"] = static_cast<std::int64_t>(
        std::count_if(w->pending_events.begin(), w->pending_events.end(),
                      [&](const uhrwerk::PendingEvent& p) { return p.state == state; }));
    return result;
}

Array UhrwerkSimulation::get_revolts() const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w) {
        return result;
    }
    for (const uhrwerk::Revolt& r : w->revolts) {
        if (!valid_index(w->states, r.state)) {
            continue;
        }
        Dictionary entry;
        entry["state_index"] = r.state;
        entry["state_name"] = short_name(*w, r.state);
        entry["capital_index"] = at(w->states, r.state).capital;
        entry["kind_text"] = revolt_kind_text(r.kind);
        entry["strength"] = r.strength.to_double();
        entry["since_text"] = to_godot(uhrwerk::format_date_long(r.start));
        entry["helper_index"] = r.helper;
        result.append(entry);
    }
    return result;
}

// ─── KI der Staaten (seit M12, Schritt b3; ki.md Abschnitt 9) ──────────────

namespace {

constexpr std::array<std::string_view, uhrwerk::kInclinationCount> kInclinationNames = {
    "Risiko", "Expansion", "Legitimität", "Reform", "Sparsamkeit", "Vermittlung"};

Dictionary inclinations_to_godot(const uhrwerk::Inclinations& values) {
    Dictionary result;
    for (std::size_t k = 0; k < uhrwerk::kInclinationCount; ++k) {
        result[to_godot(uhrwerk::kInclinationKeys[k])] = values[k].to_double();
    }
    return result;
}

Array inclination_names() {
    Array names;
    for (const std::string_view name : kInclinationNames) {
        names.append(to_godot(name));
    }
    return names;
}

std::string state_short(const uhrwerk::World& w, uhrwerk::Index state) {
    return valid_index(w.states, state) ? at(w.states, state).short_name : std::string();
}

/// Ziel einer Agenda als Text („Führung im Bund“, „Österreich schwächen“, „Gebiet Herzogtum Schleswig“).
std::string goal_text(const uhrwerk::World& w, const uhrwerk::AgendaGoal& g) {
    using uhrwerk::AgendaKind;
    switch (g.kind) {
    case AgendaKind::LeadFederation:
        return "Führung im Bund";
    case AgendaKind::CustomsUnion:
        return "Zollunion " + (valid_index(w.customs_unions, g.target) ? at(w.customs_unions, g.target).name
                                                                       : std::string("?"));
    case AgendaKind::Alliance:
        return "Bündnis mit " + state_short(w, g.target);
    case AgendaKind::Territory:
        return "Gebiet " +
               (valid_index(w.admin_units, g.target) ? at(w.admin_units, g.target).name : std::string("?"));
    case AgendaKind::Annexation:
        return "Annexion: " + state_short(w, g.target);
    case AgendaKind::Weaken:
        return state_short(w, g.target) + " schwächen";
    case AgendaKind::StatusQuo:
        return "Bund und Grenzen erhalten";
    case AgendaKind::Calm:
        return "Ruhe im Land";
    }
    return std::string();
}

} // namespace

Array UhrwerkSimulation::get_state_persons(std::int64_t state_index) const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w || !uhrwerk::ai_active(*w) || !valid_index(w->states, state_index)) {
        return result;
    }
    for (const uhrwerk::PersonRole role : {uhrwerk::PersonRole::Ruler, uhrwerk::PersonRole::Minister}) {
        const uhrwerk::Index person = w->office(static_cast<uhrwerk::Index>(state_index), role);
        if (!valid_index(w->persons, person)) {
            continue;
        }
        const uhrwerk::PersonDef& p = at(w->persons, person);
        Dictionary entry;
        entry["id"] = to_godot(p.id);
        entry["name"] = to_godot(p.name);
        entry["role"] = role == uhrwerk::PersonRole::Ruler ? String("herrscher") : String("minister");
        entry["title"] = to_godot(!p.title.empty()                     ? p.title
                                  : role == uhrwerk::PersonRole::Ruler ? std::string("Herrscher")
                                                                       : std::string("leitender Minister"));
        entry["since_text"] = to_godot(uhrwerk::format_date_long(p.from));
        Dictionary values;
        for (std::size_t v = 0; v < uhrwerk::kPersonValues; ++v) {
            values[to_godot(uhrwerk::kPersonValueKeys[v])] = p.values[v];
        }
        entry["values"] = values;
        Array traits;
        uhrwerk::Inclinations sum{};
        for (const uhrwerk::Index t : p.traits) {
            if (!valid_index(w->ai_traits, t)) {
                continue;
            }
            traits.append(to_godot(at(w->ai_traits, t).name));
            for (std::size_t k = 0; k < uhrwerk::kInclinationCount; ++k) {
                sum[k] += at(w->ai_traits, t).inclinations[k];
            }
        }
        entry["traits"] = traits;
        entry["inclinations"] = inclinations_to_godot(sum);
        const double ruler = uhrwerk::ruler_weight(*w, static_cast<uhrwerk::Index>(state_index)).to_double();
        entry["weight"] = role == uhrwerk::PersonRole::Ruler ? ruler : 1.0 - ruler; // Anteil an den Neigungen
        entry["source"] = to_godot(p.source);
        result.append(entry);
    }
    return result;
}

Dictionary UhrwerkSimulation::get_intentions(std::int64_t state_index) const {
    Dictionary result;
    const uhrwerk::World* w = world();
    if (!w || !uhrwerk::ai_active(*w) || !valid_index(w->states, state_index)) {
        return result;
    }
    const auto state = static_cast<uhrwerk::Index>(state_index);
    const bool has_agenda = valid_index(w->agendas, state_index);
    const uhrwerk::Index personality = has_agenda && at(w->agendas, state_index).personality != uhrwerk::kNone
                                           ? at(w->agendas, state_index).personality
                                           : w->ai_rules.default_personality;
    result["player"] = state == w->event_player;
    result["personality"] = valid_index(w->ai_personalities, personality)
                                ? to_godot(at(w->ai_personalities, personality).name)
                                : String();
    result["inclinations"] = inclinations_to_godot(uhrwerk::state_inclinations(*w, state));
    Array keys;
    for (const std::string_view key : uhrwerk::kInclinationKeys) {
        keys.append(to_godot(key));
    }
    result["inclination_keys"] = keys;
    result["inclination_names"] = inclination_names();
    const uhrwerk::AiStrategy strategy =
        valid_index(w->ai_strategy, state_index) ? at(w->ai_strategy, state_index) : uhrwerk::AiStrategy{};
    result["rival_index"] = strategy.rival;
    result["rival_name"] = to_godot(state_short(*w, strategy.rival));
    result["goal_text"] = String();
    Array goals;
    if (has_agenda) {
        uhrwerk::EventCache cache;
        uhrwerk::rebuild_event_cache(*w, cache);
        const std::vector<uhrwerk::AgendaGoal>& list = at(w->agendas, state_index).goals;
        for (std::size_t i = 0; i < list.size(); ++i) {
            const bool active = uhrwerk::agenda_goal_active(*w, cache, state, list[i]);
            if (!active) {
                continue;
            }
            Dictionary goal;
            goal["kind"] = to_godot(uhrwerk::kAgendaKindKeys[static_cast<std::size_t>(list[i].kind)]);
            goal["text"] = to_godot(goal_text(*w, list[i]));
            goal["weight"] = list[i].weight.to_double();
            goal["main"] = static_cast<uhrwerk::Index>(i) == strategy.goal;
            if (static_cast<uhrwerk::Index>(i) == strategy.goal) {
                result["goal_text"] = to_godot(goal_text(*w, list[i]));
            }
            goals.append(goal);
        }
    }
    result["goals"] = goals;
    return result;
}

Array UhrwerkSimulation::get_ai_decisions(std::int64_t state_index, std::int64_t limit) const {
    Array result;
    const uhrwerk::World* w = world();
    if (!w || !uhrwerk::ai_active(*w) || !valid_index(w->states, state_index)) {
        return result;
    }
    for (auto it = w->ai_log.rbegin(); it != w->ai_log.rend() && (limit <= 0 || result.size() < limit);
         ++it) {
        if (it->state != static_cast<uhrwerk::Index>(state_index)) {
            continue;
        }
        Dictionary entry;
        entry["date_text"] = to_godot(uhrwerk::format_date_long(it->date));
        entry["kind"] = static_cast<std::int64_t>(it->kind);
        entry["text"] = to_godot(uhrwerk::ai_decision_text(*w, *it));
        entry["target_index"] = it->target;
        entry["utility"] = it->utility.to_double();
        Array reasons;
        std::string why;
        for (std::size_t r = 0; r < it->reason_count; ++r) {
            const std::string text = uhrwerk::ai_reason_text(*w, it->reasons[r]);
            reasons.append(to_godot(text));
            why += (r > 0 ? ", " : "") + text;
        }
        entry["reasons"] = reasons;
        entry["reasons_text"] = to_godot(why);
        result.append(entry);
    }
    return result;
}

void UhrwerkSimulation::_bind_methods() {
    using godot::ClassDB;

    ClassDB::bind_method(D_METHOD("load_scenario", "data_path", "scenario_id", "seed"),
                         &UhrwerkSimulation::load_scenario, DEFVAL(String("westfaelischer_frieden")),
                         DEFVAL(1648));
    ClassDB::bind_method(D_METHOD("is_loaded"), &UhrwerkSimulation::is_loaded);
    ClassDB::bind_method(D_METHOD("get_load_warnings"), &UhrwerkSimulation::get_load_warnings);

    ClassDB::bind_method(D_METHOD("advance_days", "days"), &UhrwerkSimulation::advance_days);
    ClassDB::bind_method(D_METHOD("get_date"), &UhrwerkSimulation::get_date);
    ClassDB::bind_method(D_METHOD("get_date_text"), &UhrwerkSimulation::get_date_text);
    ClassDB::bind_method(D_METHOD("get_days_elapsed"), &UhrwerkSimulation::get_days_elapsed);
    ClassDB::bind_method(D_METHOD("get_scenario_name"), &UhrwerkSimulation::get_scenario_name);
    ClassDB::bind_method(D_METHOD("get_player_state_index"), &UhrwerkSimulation::get_player_state_index);
    ClassDB::bind_method(D_METHOD("get_scenarios", "data_path"), &UhrwerkSimulation::get_scenarios);
    ClassDB::bind_method(D_METHOD("get_scenario"), &UhrwerkSimulation::get_scenario);
    ClassDB::bind_method(D_METHOD("set_player_state", "state_index"), &UhrwerkSimulation::set_player_state);

    ClassDB::bind_method(D_METHOD("get_state_count"), &UhrwerkSimulation::get_state_count);
    ClassDB::bind_method(D_METHOD("get_state_index", "tag"), &UhrwerkSimulation::get_state_index);
    ClassDB::bind_method(D_METHOD("get_state", "index"), &UhrwerkSimulation::get_state);

    ClassDB::bind_method(D_METHOD("get_location_count"), &UhrwerkSimulation::get_location_count);
    ClassDB::bind_method(D_METHOD("get_location_index", "id"), &UhrwerkSimulation::get_location_index);
    ClassDB::bind_method(D_METHOD("get_location", "index"), &UhrwerkSimulation::get_location);
    ClassDB::bind_method(D_METHOD("get_location_owner_indices"),
                         &UhrwerkSimulation::get_location_owner_indices);
    ClassDB::bind_method(D_METHOD("get_location_kreis_indices"),
                         &UhrwerkSimulation::get_location_kreis_indices);
    ClassDB::bind_method(D_METHOD("get_location_market_indices"),
                         &UhrwerkSimulation::get_location_market_indices);
    ClassDB::bind_method(D_METHOD("get_location_admin_indices"),
                         &UhrwerkSimulation::get_location_admin_indices);
    ClassDB::bind_method(D_METHOD("get_admin_unit_count"), &UhrwerkSimulation::get_admin_unit_count);
    ClassDB::bind_method(D_METHOD("get_admin_unit", "index"), &UhrwerkSimulation::get_admin_unit);
    ClassDB::bind_method(D_METHOD("get_railways"), &UhrwerkSimulation::get_railways);
    ClassDB::bind_method(D_METHOD("get_rail_access"), &UhrwerkSimulation::get_rail_access);

    ClassDB::bind_method(D_METHOD("get_kreis_count"), &UhrwerkSimulation::get_kreis_count);
    ClassDB::bind_method(D_METHOD("get_kreis_index", "id"), &UhrwerkSimulation::get_kreis_index);
    ClassDB::bind_method(D_METHOD("get_kreis", "index"), &UhrwerkSimulation::get_kreis);

    ClassDB::bind_method(D_METHOD("get_market_count"), &UhrwerkSimulation::get_market_count);
    ClassDB::bind_method(D_METHOD("get_market", "index"), &UhrwerkSimulation::get_market);

    ClassDB::bind_method(D_METHOD("get_map_colors", "mode"), &UhrwerkSimulation::get_map_colors);

    ClassDB::bind_method(D_METHOD("get_currents"), &UhrwerkSimulation::get_currents);
    ClassDB::bind_method(D_METHOD("get_parliaments"), &UhrwerkSimulation::get_parliaments);
    ClassDB::bind_method(D_METHOD("get_election_count"), &UhrwerkSimulation::get_election_count);
    ClassDB::bind_method(D_METHOD("get_election", "index"), &UhrwerkSimulation::get_election);
    ClassDB::bind_method(D_METHOD("get_location_electoral_district_indices", "parliament"),
                         &UhrwerkSimulation::get_location_electoral_district_indices);
    ClassDB::bind_method(D_METHOD("get_shown_parliament"), &UhrwerkSimulation::get_shown_parliament);
    ClassDB::bind_method(D_METHOD("get_state_politics", "state_index"),
                         &UhrwerkSimulation::get_state_politics);
    ClassDB::bind_method(D_METHOD("campaign", "current", "district", "action"), &UhrwerkSimulation::campaign);
    ClassDB::bind_method(D_METHOD("get_party", "current"), &UhrwerkSimulation::get_party);
    ClassDB::bind_method(D_METHOD("get_campaign", "district", "current"), &UhrwerkSimulation::get_campaign);
    ClassDB::bind_method(D_METHOD("set_player_current", "current"), &UhrwerkSimulation::set_player_current);
    ClassDB::bind_method(D_METHOD("get_player_current"), &UhrwerkSimulation::get_player_current);

    ClassDB::bind_method(D_METHOD("get_customs_unions"), &UhrwerkSimulation::get_customs_unions);
    ClassDB::bind_method(D_METHOD("get_customs_change", "state_index", "customs_union"),
                         &UhrwerkSimulation::get_customs_change);
    ClassDB::bind_method(D_METHOD("request_customs_union", "state_index", "customs_union"),
                         &UhrwerkSimulation::request_customs_union);
    ClassDB::bind_method(D_METHOD("get_relations", "state_index"), &UhrwerkSimulation::get_relations);
    ClassDB::bind_method(D_METHOD("get_diplomacy", "state_index"), &UhrwerkSimulation::get_diplomacy);
    ClassDB::bind_method(D_METHOD("get_diplomacy_actions", "state_index", "target"),
                         &UhrwerkSimulation::get_diplomacy_actions);
    ClassDB::bind_method(D_METHOD("send_envoy", "state_index", "target", "send"),
                         &UhrwerkSimulation::send_envoy);
    ClassDB::bind_method(D_METHOD("propose_treaty", "state_index", "target", "action"),
                         &UhrwerkSimulation::propose_treaty);
    ClassDB::bind_method(D_METHOD("get_diet"), &UhrwerkSimulation::get_diet);
    ClassDB::bind_method(D_METHOD("get_motion_preview", "applicant", "kind", "target"),
                         &UhrwerkSimulation::get_motion_preview);
    ClassDB::bind_method(D_METHOD("file_motion", "applicant", "kind", "target"),
                         &UhrwerkSimulation::file_motion);
    ClassDB::bind_method(D_METHOD("get_diplomatic_plays"), &UhrwerkSimulation::get_diplomatic_plays);
    ClassDB::bind_method(D_METHOD("make_demand", "demander", "target", "kind"),
                         &UhrwerkSimulation::make_demand);
    ClassDB::bind_method(D_METHOD("manoeuvre", "state_index", "kind"), &UhrwerkSimulation::manoeuvre);
    ClassDB::bind_method(D_METHOD("get_german_question"), &UhrwerkSimulation::get_german_question);

    ClassDB::bind_method(D_METHOD("get_armies", "state_index"), &UhrwerkSimulation::get_armies, DEFVAL(-1));
    ClassDB::bind_method(D_METHOD("get_army", "army_id"), &UhrwerkSimulation::get_army);
    ClassDB::bind_method(D_METHOD("get_route_preview", "army_id", "location_index"),
                         &UhrwerkSimulation::get_route_preview);
    ClassDB::bind_method(D_METHOD("order_march", "army_id", "location_index"),
                         &UhrwerkSimulation::order_march);
    ClassDB::bind_method(D_METHOD("set_army_staff", "army_id", "on"), &UhrwerkSimulation::set_army_staff);
    ClassDB::bind_method(D_METHOD("order_mobilization", "state_index", "mobilize"),
                         &UhrwerkSimulation::order_mobilization);
    ClassDB::bind_method(D_METHOD("get_location_control"), &UhrwerkSimulation::get_location_control);
    ClassDB::bind_method(D_METHOD("get_battles"), &UhrwerkSimulation::get_battles);
    ClassDB::bind_method(D_METHOD("get_sieges"), &UhrwerkSimulation::get_sieges);
    ClassDB::bind_method(D_METHOD("get_wars"), &UhrwerkSimulation::get_wars);
    ClassDB::bind_method(D_METHOD("get_open_events"), &UhrwerkSimulation::get_open_events);
    ClassDB::bind_method(D_METHOD("choose_event_option", "event_index", "option_index"),
                         &UhrwerkSimulation::choose_event_option);
    ClassDB::bind_method(D_METHOD("get_state_society", "state_index"), &UhrwerkSimulation::get_state_society);
    ClassDB::bind_method(D_METHOD("get_state_economy", "state_index"), &UhrwerkSimulation::get_state_economy);
    ClassDB::bind_method(D_METHOD("get_state_laws", "state_index"), &UhrwerkSimulation::get_state_laws);
    ClassDB::bind_method(D_METHOD("get_votes"), &UhrwerkSimulation::get_votes);
    ClassDB::bind_method(D_METHOD("get_state_overview", "state_index"),
                         &UhrwerkSimulation::get_state_overview);
    ClassDB::bind_method(D_METHOD("get_office_history", "state_index"),
                         &UhrwerkSimulation::get_office_history);
    ClassDB::bind_method(D_METHOD("get_legacy", "state_index"), &UhrwerkSimulation::get_legacy);
    ClassDB::bind_method(D_METHOD("get_yearbook", "state_index"), &UhrwerkSimulation::get_yearbook);
    ClassDB::bind_method(D_METHOD("get_verdict"), &UhrwerkSimulation::get_verdict);
    ClassDB::bind_method(D_METHOD("is_game_over"), &UhrwerkSimulation::is_game_over);
    ClassDB::bind_method(D_METHOD("get_game_settings"), &UhrwerkSimulation::get_game_settings);
    ClassDB::bind_method(D_METHOD("set_game_settings", "difficulty_id", "open"),
                         &UhrwerkSimulation::set_game_settings);
    ClassDB::bind_method(D_METHOD("set_event_player", "state_index"), &UhrwerkSimulation::set_event_player);
    ClassDB::bind_method(D_METHOD("get_event_player"), &UhrwerkSimulation::get_event_player);
    ClassDB::bind_method(D_METHOD("get_state_persons", "state_index"), &UhrwerkSimulation::get_state_persons);
    ClassDB::bind_method(D_METHOD("get_intentions", "state_index"), &UhrwerkSimulation::get_intentions);
    ClassDB::bind_method(D_METHOD("get_ai_decisions", "state_index", "limit"),
                         &UhrwerkSimulation::get_ai_decisions);
    ClassDB::bind_method(D_METHOD("get_event_log", "limit"), &UhrwerkSimulation::get_event_log);
    ClassDB::bind_method(D_METHOD("get_journals"), &UhrwerkSimulation::get_journals);
    ClassDB::bind_method(D_METHOD("get_state_events", "state_index"), &UhrwerkSimulation::get_state_events);
    ClassDB::bind_method(D_METHOD("get_revolts"), &UhrwerkSimulation::get_revolts);
    ClassDB::bind_method(D_METHOD("get_declaration_goals", "state_index", "target"),
                         &UhrwerkSimulation::get_declaration_goals);
    ClassDB::bind_method(D_METHOD("get_declaration_sides", "state_index", "target"),
                         &UhrwerkSimulation::get_declaration_sides);
    ClassDB::bind_method(D_METHOD("get_war_goal_options", "state_index", "war_id"),
                         &UhrwerkSimulation::get_war_goal_options);
    ClassDB::bind_method(D_METHOD("declare_war", "state_index", "target", "goal"),
                         &UhrwerkSimulation::declare_war, DEFVAL(0));
    ClassDB::bind_method(D_METHOD("add_war_goal", "state_index", "war_id", "goal"),
                         &UhrwerkSimulation::add_war_goal);
    ClassDB::bind_method(D_METHOD("get_peace_preview", "state_index", "war_id", "terms"),
                         &UhrwerkSimulation::get_peace_preview);
    ClassDB::bind_method(D_METHOD("offer_peace", "state_index", "war_id", "terms"),
                         &UhrwerkSimulation::offer_peace);

    ClassDB::bind_method(D_METHOD("set_tax_rate_percent", "state_index", "percent"),
                         &UhrwerkSimulation::set_tax_rate_percent);
    ClassDB::bind_method(D_METHOD("set_tax_percent", "state_index", "kind", "percent"),
                         &UhrwerkSimulation::set_tax_percent);
    ClassDB::bind_method(D_METHOD("get_max_tax_rate_percent"), &UhrwerkSimulation::get_max_tax_rate_percent);

    ClassDB::bind_method(D_METHOD("get_chronicle_size"), &UhrwerkSimulation::get_chronicle_size);
    ClassDB::bind_method(D_METHOD("get_chronicle", "from_index"), &UhrwerkSimulation::get_chronicle);

    ClassDB::bind_method(D_METHOD("get_checksum"), &UhrwerkSimulation::get_checksum);

    ClassDB::bind_method(D_METHOD("save_game", "path", "player_state_index"), &UhrwerkSimulation::save_game);
    ClassDB::bind_method(D_METHOD("load_game", "data_path", "path"), &UhrwerkSimulation::load_game);
    ClassDB::bind_method(D_METHOD("get_save_info", "path"), &UhrwerkSimulation::get_save_info);
    ClassDB::bind_method(D_METHOD("get_save_format_version"), &UhrwerkSimulation::get_save_format_version);

    BIND_ENUM_CONSTANT(MAP_POLITICAL);
    BIND_ENUM_CONSTANT(MAP_DENSITY);
    BIND_ENUM_CONSTANT(MAP_CONFESSION);
    BIND_ENUM_CONSTANT(MAP_TERRAIN);
    BIND_ENUM_CONSTANT(MAP_DEVASTATION);
    BIND_ENUM_CONSTANT(MAP_KREISE);
    BIND_ENUM_CONSTANT(MAP_LEBENSSTANDARD);
    BIND_ENUM_CONSTANT(MAP_KULTUR);
    BIND_ENUM_CONSTANT(MAP_WIRTSCHAFT);
    BIND_ENUM_CONSTANT(MAP_MAERKTE);
    BIND_ENUM_CONSTANT(MAP_VERWALTUNG);
    BIND_ENUM_CONSTANT(MAP_EISENBAHNEN);
    BIND_ENUM_CONSTANT(MAP_WAHLEN);
    BIND_ENUM_CONSTANT(MAP_ZOLLGEBIETE);
    BIND_ENUM_CONSTANT(MAP_BEZIEHUNGEN);
    BIND_ENUM_CONSTANT(MAP_KRIEG);
    BIND_ENUM_CONSTANT(MAP_UNRUHE);
    BIND_ENUM_CONSTANT(MAP_BUND);
    BIND_ENUM_CONSTANT(TAX_KONTRIBUTION);
    BIND_ENUM_CONSTANT(TAX_AKZISE);
    BIND_ENUM_CONSTANT(TAX_ZOLL);
    BIND_ENUM_CONSTANT(CAMPAIGN_VERSAMMLUNG);
    BIND_ENUM_CONSTANT(CAMPAIGN_FLUGBLAETTER);
    BIND_ENUM_CONSTANT(CAMPAIGN_VEREIN);
    BIND_ENUM_CONSTANT(TREATY_ALLIANCE);
    BIND_ENUM_CONSTANT(TREATY_GUARANTEE);
    BIND_ENUM_CONSTANT(TREATY_CANCEL_ALLIANCE);
    BIND_ENUM_CONSTANT(TREATY_CANCEL_GUARANTEE);
    BIND_ENUM_CONSTANT(MOTION_REFORM);
    BIND_ENUM_CONSTANT(MOTION_CUSTOMS);
    BIND_ENUM_CONSTANT(MOTION_INTERVENTION);
    BIND_ENUM_CONSTANT(MOTION_PRESIDENCY);
    BIND_ENUM_CONSTANT(DEMAND_LEADERSHIP);
    BIND_ENUM_CONSTANT(DEMAND_CUSTOMS_UNION);
    BIND_ENUM_CONSTANT(DEMAND_BREAK_ALLIANCE);
    BIND_ENUM_CONSTANT(MANOEUVRE_YIELD);
    BIND_ENUM_CONSTANT(MANOEUVRE_MEDIATE);
    BIND_ENUM_CONSTANT(MANOEUVRE_ESCALATE);
    BIND_ENUM_CONSTANT(PEACE_WHITE);
    BIND_ENUM_CONSTANT(PEACE_OWN_GOALS);
    BIND_ENUM_CONSTANT(PEACE_CAPITULATION);
}

} // namespace uhrwerk_bridge
