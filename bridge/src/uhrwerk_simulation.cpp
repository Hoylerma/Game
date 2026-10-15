#include "uhrwerk_simulation.h"

#include "uhrwerk/date.hpp"
#include "uhrwerk/economy.hpp"
#include "uhrwerk/loader.hpp"

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
using godot::PackedColorArray;
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
const Color kLivingLow(0.66f, 0.22f, 0.16f);   // Lebensstandard 0
const Color kLivingHigh(0.24f, 0.52f, 0.30f);  // Lebensstandard 100
const Color kEconomyLow(0.95f, 0.92f, 0.80f);  // Wirtschaftsleistung 0
const Color kEconomyHigh(0.55f, 0.36f, 0.08f); // Wirtschaftsleistung ab kEconomyMax
constexpr double kDensityMax = 120.0;          // Einwohner je km², ab hier dunkelste Stufe
constexpr double kEconomyMax = 16.0;           // Reichstaler je Einwohner und Jahr, dunkelste Stufe

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
    ERR_FAIL_COND_V_MSG(mode < MAP_POLITICAL || mode > MAP_VERWALTUNG, colors,
                        "get_map_colors: unbekannter Kartenmodus.");

    const auto count = static_cast<std::int64_t>(w->locations.size());
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
    if (!uhrwerk::write_file(file, uhrwerk::save_game(*simulation_, uhrwerk::SaveMeta{tag}), &error)) {
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
    // Kurzname für Listen (z. B. „Brandenburg“), sonst der volle Name oder die Kennung.
    String player_name = to_godot(info->player_tag);
    if (const uhrwerk::World* w = world()) {
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

    ClassDB::bind_method(D_METHOD("get_kreis_count"), &UhrwerkSimulation::get_kreis_count);
    ClassDB::bind_method(D_METHOD("get_kreis_index", "id"), &UhrwerkSimulation::get_kreis_index);
    ClassDB::bind_method(D_METHOD("get_kreis", "index"), &UhrwerkSimulation::get_kreis);

    ClassDB::bind_method(D_METHOD("get_market_count"), &UhrwerkSimulation::get_market_count);
    ClassDB::bind_method(D_METHOD("get_market", "index"), &UhrwerkSimulation::get_market);

    ClassDB::bind_method(D_METHOD("get_map_colors", "mode"), &UhrwerkSimulation::get_map_colors);

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
    BIND_ENUM_CONSTANT(TAX_KONTRIBUTION);
    BIND_ENUM_CONSTANT(TAX_AKZISE);
    BIND_ENUM_CONSTANT(TAX_ZOLL);
}

} // namespace uhrwerk_bridge
