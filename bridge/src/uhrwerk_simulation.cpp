#include "uhrwerk_simulation.h"

#include "uhrwerk/date.hpp"
#include "uhrwerk/loader.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/color.hpp>
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
const Color kLivingLow(0.66f, 0.22f, 0.16f);  // Lebensstandard 0
const Color kLivingHigh(0.24f, 0.52f, 0.30f); // Lebensstandard 100
constexpr double kDensityMax = 120.0;         // Einwohner je km², ab hier dunkelste Stufe

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
    for (const uhrwerk::Location& loc : w->locations) {
        if (valid_index(kreis_location_counts_, loc.district)) {
            ++kreis_location_counts_[static_cast<std::size_t>(loc.district)];
        }
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
    return w ? w->find_state(w->scenario.player_tag) : uhrwerk::kNone;
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
    result["treasury"] = state.treasury.to_double();
    result["tax_rate_percent"] = to_percent(state.tax_rate);
    result["income_month"] = state.last_month_income.to_double();
    result["expenses_month"] = state.last_month_expenses.to_double();
    result["population"] = w->state_population(state_index).round();
    const std::vector<uhrwerk::Index> locations = w->state_locations(state_index);
    result["location_count"] = static_cast<std::int64_t>(locations.size());
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
    result["taxes_month"] = economy.taxes.to_double();
    result["source"] = to_godot(loc.source);
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
    ERR_FAIL_COND_V_MSG(mode < MAP_POLITICAL || mode > MAP_KULTUR, colors,
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
        default:
            break;
        }
        // Orte ohne Daten: in den datenabhängigen Modi Alpha 0, die Godot-Seite zeichnet dort
        // eine Schraffur „keine Daten“. Gelände und Kreise sind auch ohne Daten bekannt.
        if (!loc.has_data() && mode != MAP_TERRAIN && mode != MAP_KREISE) {
            color.a = 0.0f;
        }
        colors.set(i, color);
    }
    return colors;
}

bool UhrwerkSimulation::set_tax_rate_percent(std::int64_t state_index, std::int64_t percent) {
    const uhrwerk::World* w = world();
    if (!w || !valid_index(w->states, state_index)) {
        return false;
    }
    if (percent < 0 || percent > get_max_tax_rate_percent()) {
        return false;
    }
    uhrwerk::Command command;
    command.type = uhrwerk::CommandType::SetTaxRate;
    command.state = static_cast<uhrwerk::Index>(state_index);
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
    static constexpr std::array<char, 16> kHex = {'0', '1', '2', '3', '4', '5', '6', '7',
                                                  '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::uint64_t value = simulation_->checksum();
    std::string text(16, '0');
    for (int i = 15; i >= 0; --i) {
        text[static_cast<std::size_t>(i)] = kHex[value & 0xF];
        value >>= 4;
    }
    return to_godot(text);
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

    ClassDB::bind_method(D_METHOD("get_kreis_count"), &UhrwerkSimulation::get_kreis_count);
    ClassDB::bind_method(D_METHOD("get_kreis_index", "id"), &UhrwerkSimulation::get_kreis_index);
    ClassDB::bind_method(D_METHOD("get_kreis", "index"), &UhrwerkSimulation::get_kreis);

    ClassDB::bind_method(D_METHOD("get_map_colors", "mode"), &UhrwerkSimulation::get_map_colors);

    ClassDB::bind_method(D_METHOD("set_tax_rate_percent", "state_index", "percent"),
                         &UhrwerkSimulation::set_tax_rate_percent);
    ClassDB::bind_method(D_METHOD("get_max_tax_rate_percent"), &UhrwerkSimulation::get_max_tax_rate_percent);

    ClassDB::bind_method(D_METHOD("get_chronicle_size"), &UhrwerkSimulation::get_chronicle_size);
    ClassDB::bind_method(D_METHOD("get_chronicle", "from_index"), &UhrwerkSimulation::get_chronicle);

    ClassDB::bind_method(D_METHOD("get_checksum"), &UhrwerkSimulation::get_checksum);

    BIND_ENUM_CONSTANT(MAP_POLITICAL);
    BIND_ENUM_CONSTANT(MAP_DENSITY);
    BIND_ENUM_CONSTANT(MAP_CONFESSION);
    BIND_ENUM_CONSTANT(MAP_TERRAIN);
    BIND_ENUM_CONSTANT(MAP_DEVASTATION);
    BIND_ENUM_CONSTANT(MAP_KREISE);
    BIND_ENUM_CONSTANT(MAP_LEBENSSTANDARD);
    BIND_ENUM_CONSTANT(MAP_KULTUR);
}

} // namespace uhrwerk_bridge
