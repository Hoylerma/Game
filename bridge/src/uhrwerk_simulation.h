#pragma once

// UhrwerkSimulation – die einzige Klasse, die die Brücke Godot bereitstellt.
// Schnittstelle: docs/technik/bridge-api.md
//
// Die Klasse besitzt einen uhrwerk::Simulation-Kern. Godot liest nur und
// schickt Befehle; Werte in die Simulation gehen als Ganzzahlen hinein,
// Werte aus der Simulation dürfen float sein (nur Anzeige).

#include "uhrwerk/simulation.hpp"

#include <cstdint>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_color_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace uhrwerk_bridge {

class UhrwerkSimulation : public godot::RefCounted {
    GDCLASS(UhrwerkSimulation, godot::RefCounted)

public:
    /// Kartenmodi für get_map_colors (siehe bridge-api.md).
    enum MapMode : std::int64_t {
        MAP_POLITICAL = 0,
        MAP_DENSITY = 1,
        MAP_CONFESSION = 2,
        MAP_TERRAIN = 3,
        MAP_DEVASTATION = 4,
        MAP_KREISE = 5,         // seit M3
        MAP_LEBENSSTANDARD = 6, // seit M4
        MAP_KULTUR = 7,         // seit M4
    };

    static constexpr std::int64_t kMaxAdvanceDays = 3660;

    UhrwerkSimulation();
    ~UhrwerkSimulation() override;

    godot::String load_scenario(const godot::String& data_path, const godot::String& scenario_id,
                                std::int64_t seed);
    bool is_loaded() const;
    godot::PackedStringArray get_load_warnings() const;

    void advance_days(std::int64_t days);
    godot::Dictionary get_date() const;
    godot::String get_date_text() const;
    std::int64_t get_days_elapsed() const;
    godot::String get_scenario_name() const;
    std::int64_t get_player_state_index() const;

    std::int64_t get_state_count() const;
    std::int64_t get_state_index(const godot::String& tag) const;
    godot::Dictionary get_state(std::int64_t index) const;

    std::int64_t get_location_count() const;
    std::int64_t get_location_index(const godot::String& id) const;
    godot::Dictionary get_location(std::int64_t index) const;
    godot::PackedInt32Array get_location_owner_indices() const;
    godot::PackedInt32Array get_location_kreis_indices() const;

    std::int64_t get_kreis_count() const;
    std::int64_t get_kreis_index(const godot::String& id) const;
    godot::Dictionary get_kreis(std::int64_t index) const;

    godot::PackedColorArray get_map_colors(std::int64_t mode) const;

    bool set_tax_rate_percent(std::int64_t state_index, std::int64_t percent);
    std::int64_t get_max_tax_rate_percent() const;

    std::int64_t get_chronicle_size() const;
    godot::Array get_chronicle(std::int64_t from_index) const;

    godot::String get_checksum() const;

protected:
    static void _bind_methods();

private:
    const uhrwerk::World* world() const;
    /// Nachschlagetabellen für Kennungen und Orte je Kreis; Orte und Kreise ändern sich
    /// während einer Partie nicht. Nur Anzeige, ohne Einfluss auf die Simulation.
    void build_lookups();

    std::unique_ptr<uhrwerk::Simulation> simulation_;
    godot::PackedStringArray load_warnings_;
    std::unordered_map<std::string, std::int64_t> location_lookup_;
    std::unordered_map<std::string, std::int64_t> kreis_lookup_;
    std::vector<std::int64_t> kreis_location_counts_;
};

} // namespace uhrwerk_bridge

VARIANT_ENUM_CAST(uhrwerk_bridge::UhrwerkSimulation::MapMode);
