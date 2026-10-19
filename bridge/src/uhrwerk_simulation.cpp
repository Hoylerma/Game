#include "uhrwerk_simulation.h"

#include "uhrwerk/date.hpp"
#include "uhrwerk/economy.hpp"
#include "uhrwerk/loader.hpp"
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
constexpr double kDensityMax = 120.0;              // Einwohner je km², ab hier dunkelste Stufe
constexpr double kEconomyMax = 16.0;               // Reichstaler je Einwohner und Jahr, dunkelste Stufe

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
    // Nur die Wahl der Oberfläche (Spielstand, Panels); die Simulation kennt keinen Spieler.
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
    result["zollverein"] = state.customs_union; // seit M7
    result["treasury"] = state.treasury.to_double();
    result["tax_rate_percent"] = to_percent(state.tax_rate(uhrwerk::TaxKind::Kontribution));
    // Haushalt des letzten Rechentags (gleich last_month_*); vor dem ersten Rechentag die Schätzung
    // aus dem Start (estimate_budgets) statt 0.
    result["income_month"] = state.budget.income().to_double();
    result["expenses_month"] = state.budget.expenses().to_double();
    result["population"] = w->state_population(state_index).round();
    const std::vector<uhrwerk::Index> locations = w->state_locations(state_index);
    result["location_count"] = static_cast<std::int64_t>(locations.size());
    // Mitglied des Deutschen Bundes (seit M7): mindestens ein Ort im Bundesgebiet.
    bool federal = false;
    for (const uhrwerk::Index li : locations) {
        federal = federal || at(w->locations, li).federal;
    }
    result["bund"] = federal;
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
    result["bundesgebiet"] = loc.federal;
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
            to_godot(uhrwerk::format_date_long(at(w->parliaments, parliament).election_day));
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
            const PackedFloat64Array shares = vote_shares(*r);
            Dictionary by_name;
            Dictionary colors;
            for (std::size_t c = 0; c < w->currents.size() && c < static_cast<std::size_t>(shares.size());
                 ++c) {
                const String name = to_godot(w->currents[c].name);
                by_name[name] = shares[static_cast<std::int64_t>(c)] * 100.0;
                colors[name] = to_godot(w->currents[c].color);
            }
            election["shares"] = by_name; // Strömung → Prozent
            election["colors"] = colors;  // Strömung → Farbe
        }
        result["wahl"] = election;
    }
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
    ERR_FAIL_COND_V_MSG(mode < MAP_POLITICAL || mode > MAP_WAHLEN, colors,
                        "get_map_colors: unbekannter Kartenmodus.");

    const auto count = static_cast<std::int64_t>(w->locations.size());
    const std::vector<std::uint8_t> rail = mode == MAP_EISENBAHNEN
                                               ? rail_access(*w, simulation_->economy_topology())
                                               : std::vector<std::uint8_t>{};
    // Wahlen (seit M8): das angezeigte Parlament und seine letzte Wahl.
    const std::int64_t parliament = mode == MAP_WAHLEN ? get_shown_parliament() : -1;
    const uhrwerk::ElectionResult* election = parliament >= 0 ? latest_election(parliament) : nullptr;
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
        entry["election_date"] = date_dictionary(parliament.election_day);
        entry["election_date_text"] = to_godot(uhrwerk::format_date_long(parliament.election_day));
        entry["faction_date"] = date_dictionary(parliament.faction_day);
        entry["faction_date_text"] = to_godot(uhrwerk::format_date_long(parliament.faction_day));
        entry["seat_index"] = parliament.seat;
        entry["franchise"] = franchise_name(parliament.law.franchise);
        entry["indirect"] = parliament.law.indirect;
        std::int64_t districts = 0;
        for (const uhrwerk::ElectoralDistrict& d : w->electoral_districts) {
            districts += d.parliament == static_cast<uhrwerk::Index>(p) ? 1 : 0;
        }
        entry["district_count"] = districts;
        std::int64_t latest = -1;
        for (std::size_t e = 0; e < w->elections.size(); ++e) {
            if (w->elections[e].parliament == static_cast<uhrwerk::Index>(p)) {
                latest = static_cast<std::int64_t>(e);
            }
        }
        entry["election_index"] = latest;
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
    std::int64_t next = 0;
    for (std::size_t p = 1; p < w->parliaments.size(); ++p) {
        if (w->parliaments[p].election_day < at(w->parliaments, next).election_day) {
            next = static_cast<std::int64_t>(p);
        }
    }
    return next;
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

bool UhrwerkSimulation::campaign(std::int64_t current, std::int64_t district, std::int64_t action) {
    const uhrwerk::World* w = world();
    if (!w || action < CAMPAIGN_VERSAMMLUNG || action > CAMPAIGN_VEREIN ||
        !valid_index(w->currents, current) || !valid_index(w->electoral_districts, district)) {
        return false;
    }
    const auto kind = static_cast<uhrwerk::CampaignAction>(action);
    if (!uhrwerk::campaign_allowed(*w, static_cast<uhrwerk::Index>(current),
                                   static_cast<uhrwerk::Index>(district), kind)) {
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
    std::int64_t districts = 0;
    const std::size_t currents = w->currents.size();
    for (std::size_t d = 0;
         d < w->electoral_districts.size() && w->campaign.size() == w->electoral_districts.size() * currents;
         ++d) {
        const uhrwerk::CampaignState& state = w->campaign[d * currents + c];
        if (state.mobilization > uhrwerk::Fixed::zero() || state.persuasion > uhrwerk::Fixed::zero() ||
            state.club > uhrwerk::Fixed::zero()) {
            ++districts;
        }
    }
    result["campaign_districts"] = districts;
    const std::int64_t parliament = get_shown_parliament();
    if (!valid_index(w->parliaments, parliament)) {
        return result;
    }
    const uhrwerk::Parliament& p = at(w->parliaments, parliament);
    result["parliament_name"] = to_godot(p.name);
    result["election_date_text"] = to_godot(uhrwerk::format_date_long(p.election_day));
    result["faction_date_text"] = to_godot(uhrwerk::format_date_long(p.faction_day));
    const uhrwerk::ElectionResult* election = latest_election(parliament);
    result["held"] = election != nullptr;
    if (election == nullptr) {
        return result;
    }
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
    bool open = valid_index(w->parliaments, d.parliament) && latest_election(d.parliament) == nullptr &&
                w->date <= at(w->parliaments, d.parliament).election_day;
    result["open"] = open;
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
        entry["allowed"] = uhrwerk::campaign_allowed(*w, static_cast<uhrwerk::Index>(current),
                                                     static_cast<uhrwerk::Index>(district),
                                                     static_cast<uhrwerk::CampaignAction>(a));
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
    if (current < 0) {
        player_current_.clear();
        return true;
    }
    if (!valid_index(w->currents, current)) {
        return false;
    }
    player_current_ = at(w->currents, current).id;
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
    for (std::int64_t i = std::max<std::int64_t>(from_index, 0); i < size; ++i) {
        const uhrwerk::ChronicleEntry& entry = at(w->chronicle, i);
        Dictionary item;
        item["date_text"] = to_godot(uhrwerk::format_date(entry.date));
        item["text"] = to_godot(entry.text);
        item["location_index"] = entry.location;
        item["state_index"] = entry.state;
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
    ClassDB::bind_method(D_METHOD("campaign", "current", "district", "action"), &UhrwerkSimulation::campaign);
    ClassDB::bind_method(D_METHOD("get_party", "current"), &UhrwerkSimulation::get_party);
    ClassDB::bind_method(D_METHOD("get_campaign", "district", "current"), &UhrwerkSimulation::get_campaign);
    ClassDB::bind_method(D_METHOD("set_player_current", "current"), &UhrwerkSimulation::set_player_current);
    ClassDB::bind_method(D_METHOD("get_player_current"), &UhrwerkSimulation::get_player_current);

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
    BIND_ENUM_CONSTANT(TAX_KONTRIBUTION);
    BIND_ENUM_CONSTANT(TAX_AKZISE);
    BIND_ENUM_CONSTANT(TAX_ZOLL);
    BIND_ENUM_CONSTANT(CAMPAIGN_VERSAMMLUNG);
    BIND_ENUM_CONSTANT(CAMPAIGN_FLUGBLAETTER);
    BIND_ENUM_CONSTANT(CAMPAIGN_VEREIN);
}

} // namespace uhrwerk_bridge
