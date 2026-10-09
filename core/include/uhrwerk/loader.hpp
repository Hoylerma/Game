#pragma once

// Lädt Spieldaten aus dem Datenordner (data/) in ein World-Objekt.
// Dateiformate: docs/technik/chronik-skript.md

#include "uhrwerk/world.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uhrwerk {

struct LoadIssue {
    std::string file;
    int line = 0;
    std::string message;
    bool fatal = true; // false = Warnung

    /// "datei.chron:12: Fehler: Nachricht" bzw. "…: Warnung: …"
    std::string to_string() const;
};

struct LoadResult {
    std::optional<World> world; // gesetzt, wenn keine fatalen Probleme auftraten
    std::vector<LoadIssue> issues;

    bool ok() const { return world.has_value(); }
};

inline constexpr std::string_view kDefaultScenario = "westfaelischer_frieden";

/// Lädt alle Inhalte und setzt den Startzustand des Szenarios.
///
/// Reihenfolge (jeweils alle *.chron-Dateien eines Ordners, nach Dateiname sortiert):
///   common/regeln.chron, common/gelaende.chron, common/konfessionen.chron, common/berufe.chron,
///   history/staaten/, history/orte/, szenarien/
LoadResult load_world(const std::filesystem::path& data_dir, std::string_view scenario_id = kDefaultScenario);

/// Sucht den Datenordner: Umgebungsvariable UHRWERK_DATEN, sonst aufwärts ab
/// start nach einem Ordner "data" mit Unterordner "szenarien".
std::optional<std::filesystem::path> find_data_dir(const std::filesystem::path& start);

} // namespace uhrwerk
