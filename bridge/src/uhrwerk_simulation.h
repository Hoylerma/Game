#pragma once

// UhrwerkSimulation – die einzige Klasse, die die Brücke Godot bereitstellt.
// Schnittstelle: docs/technik/bridge-api.md
//
// Die Klasse besitzt einen uhrwerk::Simulation-Kern. Godot liest nur und
// schickt Befehle; Werte in die Simulation gehen als Ganzzahlen hinein,
// Werte aus der Simulation dürfen float sein (nur Anzeige).

#include "uhrwerk/savegame.hpp"
#include "uhrwerk/simulation.hpp"

#include <cstdint>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
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
        MAP_WIRTSCHAFT = 8,     // seit M5
        MAP_MAERKTE = 9,        // seit M5
        MAP_VERWALTUNG = 10,    // seit M7
        MAP_EISENBAHNEN = 11,   // seit M7
        MAP_WAHLEN = 12,        // seit M8
    };

    /// Steuerarten für set_tax_percent (seit M5, wie uhrwerk::TaxKind).
    enum TaxKind : std::int64_t {
        TAX_KONTRIBUTION = 0,
        TAX_AKZISE = 1,
        TAX_ZOLL = 2,
    };

    /// Arten des Wahlkampfs für campaign (seit M8, wie uhrwerk::CampaignAction).
    enum CampaignKind : std::int64_t {
        CAMPAIGN_VERSAMMLUNG = 0,
        CAMPAIGN_FLUGBLAETTER = 1,
        CAMPAIGN_VEREIN = 2,
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
    // Szenarien und Staatswahl (seit M7)
    godot::Array get_scenarios(const godot::String& data_path) const;
    godot::Dictionary get_scenario() const;
    bool set_player_state(std::int64_t state_index);

    std::int64_t get_state_count() const;
    std::int64_t get_state_index(const godot::String& tag) const;
    godot::Dictionary get_state(std::int64_t index) const;

    std::int64_t get_location_count() const;
    std::int64_t get_location_index(const godot::String& id) const;
    godot::Dictionary get_location(std::int64_t index) const;
    godot::PackedInt32Array get_location_owner_indices() const;
    godot::PackedInt32Array get_location_kreis_indices() const;
    godot::PackedInt32Array get_location_market_indices() const;
    godot::PackedInt32Array get_location_admin_indices() const;

    std::int64_t get_kreis_count() const;
    std::int64_t get_kreis_index(const godot::String& id) const;
    godot::Dictionary get_kreis(std::int64_t index) const;

    std::int64_t get_admin_unit_count() const;
    godot::Dictionary get_admin_unit(std::int64_t index) const;
    godot::Array get_railways() const;
    godot::PackedByteArray get_rail_access() const;

    std::int64_t get_market_count() const;
    godot::Dictionary get_market(std::int64_t index) const;

    godot::PackedColorArray get_map_colors(std::int64_t mode) const;

    // Politik (seit M8, docs/technik/politik.md Abschnitt 10)
    godot::Array get_currents() const;
    godot::Array get_parliaments() const;
    std::int64_t get_election_count() const;
    godot::Dictionary get_election(std::int64_t index) const;
    godot::PackedInt32Array get_location_electoral_district_indices(std::int64_t parliament) const;
    std::int64_t get_shown_parliament() const;
    godot::Dictionary get_state_politics(std::int64_t state_index) const;

    // Parteienmodus (seit M8, Schritt b2, politik.md Abschnitt 9)
    bool campaign(std::int64_t current, std::int64_t district, std::int64_t action);
    godot::Dictionary get_party(std::int64_t current) const;
    godot::Dictionary get_campaign(std::int64_t district, std::int64_t current) const;
    bool set_player_current(std::int64_t current);
    std::int64_t get_player_current() const;

    bool set_tax_rate_percent(std::int64_t state_index, std::int64_t percent);
    bool set_tax_percent(std::int64_t state_index, std::int64_t kind, std::int64_t percent);
    std::int64_t get_max_tax_rate_percent() const;

    std::int64_t get_chronicle_size() const;
    godot::Array get_chronicle(std::int64_t from_index) const;

    godot::String get_checksum() const;

    // Spielstände (seit M6, docs/technik/speicherstaende.md Abschnitt 9)
    godot::String save_game(const godot::String& path, std::int64_t player_state_index) const;
    godot::String load_game(const godot::String& data_path, const godot::String& path);
    godot::Dictionary get_save_info(const godot::String& path) const;
    std::int64_t get_save_format_version() const;

protected:
    static void _bind_methods();

private:
    const uhrwerk::World* world() const;
    /// Ergebnis eines Wahlkreises in einer Wahl oder nullptr (seit M8).
    const uhrwerk::DistrictResult* district_result(const uhrwerk::ElectionResult& election,
                                                   std::int64_t district) const;
    /// Letzte Wahl eines Parlaments oder nullptr (seit M8).
    const uhrwerk::ElectionResult* latest_election(std::int64_t parliament) const;
    // Parlament mit der nächsten ausstehenden Wahl (Parteienmodus), -1 ohne.
    std::int64_t next_parliament() const;
    // Wahlkampf erlaubt und bezahlbar, auch nach den schon eingereihten Befehlen der Strömung.
    bool campaign_possible(std::int64_t current, std::int64_t district, uhrwerk::CampaignAction action) const;
    /// Nachschlagetabellen für Kennungen und Orte je Kreis; Orte und Kreise ändern sich
    /// während einer Partie nicht. Nur Anzeige, ohne Einfluss auf die Simulation.
    void build_lookups();

    std::unique_ptr<uhrwerk::Simulation> simulation_;
    godot::PackedStringArray load_warnings_;
    std::string player_tag_;              // Spielerstaat: aus dem Szenario oder dem Spielstand (seit M6)
    std::string player_current_;          // Strömung im Parteienmodus (seit M8), leer = Staat
    uhrwerk::ContentFingerprint content_; // Inhalt der geladenen Spieldaten (Vergleich mit Spielständen)
    std::unordered_map<std::string, std::int64_t> location_lookup_;
    std::unordered_map<std::string, std::int64_t> kreis_lookup_;
    std::vector<std::int64_t> kreis_location_counts_;
    std::vector<std::int64_t> market_location_counts_;          // Orte mit Daten je Markt
    std::vector<bool> market_ports_;                            // Markt mit Hafenort (mit Daten)
    std::vector<std::size_t> market_colors_;                    // Farbe je Markt (Index in die Palette)
    std::vector<std::vector<std::int32_t>> location_districts_; // je Parlament: Wahlkreis je Ort, -1 = keiner
    std::vector<std::int64_t>
        district_positions_; // je Wahlkreis: Stelle unter den Wahlkreisen seines Parlaments
};

} // namespace uhrwerk_bridge

VARIANT_ENUM_CAST(uhrwerk_bridge::UhrwerkSimulation::MapMode);
VARIANT_ENUM_CAST(uhrwerk_bridge::UhrwerkSimulation::TaxKind);
VARIANT_ENUM_CAST(uhrwerk_bridge::UhrwerkSimulation::CampaignKind);
