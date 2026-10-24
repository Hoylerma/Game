#pragma once

// Kriegslabor (seit M10, Schritt a5; docs/technik/militaer.md, Abschnitt 6.3).
//
// Rechnet je Seed einen Krieg ohne Spieler bis zum Frieden (höchstens tage Tage, Standard zwei Jahre):
// Der Angreifer erklärt ihn am ersten Tag mit seinen Kriegszielen, sofern das Szenario ihn nicht schon
// führt; die Heere führt der Generalstab nach Regel. Berichtet je Lauf Ausgang, Dauer, Friedensschluss,
// Schlachten, Verluste und Schulden, über alle Läufe die Verteilung gegen die Kriegsziele in
// common/kriegsziele_<szenario>.chron (Format: docs/technik/chronik-skript.md, Abschnitt 3.31). Wie
// Zoll- und Wahllabor kein Teil der Simulation.

#include "uhrwerk/balance.hpp"
#include "uhrwerk/fixed.hpp"
#include "uhrwerk/simulation.hpp"
#include "uhrwerk/world.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uhrwerk {

/// Ein Krieg des Labors (Block `krieg <kennung> { … }`) mit seinen Zielwerten: Anteile der Ausgänge
/// über die Läufe, Median der Dauer in Tagen über die Läufe mit Frieden.
struct WarScenario {
    std::string id;
    Index attacker = kNone;
    Index defender = kNone;
    std::vector<WarGoal> goals;        // Ziele des Angreifers; das erste mit der Erklärung
    std::int32_t days = 730;           // höchstens so viele Tage
    std::optional<ValueRange> victory; // Anteil: der Angreifer siegt
    std::optional<ValueRange> defeat;  // Anteil: der Verteidiger siegt
    std::optional<ValueRange> white;   // Anteil: Frieden ohne Gewinner
    std::optional<ValueRange> duration;
    bool open = false; // kein Ausgang in allen Läufen
};

struct WarTargets {
    std::vector<WarScenario> wars;
};

struct WarTargetsResult {
    WarTargets targets;
    std::vector<std::string> errors; // "datei:zeile:spalte: Meldung"

    bool ok() const { return errors.empty(); }
};

/// Liest die Kriegsziele; Staaten und Verwaltungseinheiten werden in world gesucht.
WarTargetsResult parse_war_targets(std::string_view text, const World& world,
                                   const std::string& file_name = "<text>");
WarTargetsResult load_war_targets(const std::filesystem::path& file, const World& world);

enum class WarOutcome : std::uint8_t {
    Attacker = 0, // der Angreifer siegt
    Defender = 1, // der Verteidiger siegt
    White = 2,    // Frieden ohne Gewinner
    Open = 3,     // kein Frieden bis zum Ende des Laufs
};
inline constexpr std::size_t kWarOutcomes = 4;

/// Ergebnis eines Laufs.
struct WarLabRun {
    bool started = false; // Krieg erklärt oder schon im Szenario
    WarOutcome outcome = WarOutcome::Open;
    std::int32_t days = 0; // vom Kriegsbeginn bis zum Frieden oder zum Ende des Laufs
    std::int32_t battles = 0;
    Fixed attacker_losses; // Verluste der Angreiferseite (Gefallene, Verwundete, Gefangene)
    Fixed defender_losses;
    std::int32_t transfers = 0; // Orte, die den Besitzer wechselten
    Fixed attacker_debt;        // Schulden der Hauptpartei am Ende minus am Start
    Fixed defender_debt;
    std::string peace; // Chronik des Friedensschlusses, leer ohne
};

/// Rechnet einen Lauf mit config.seed.
WarLabRun run_war_lab(const World& world, const SimConfig& config, const WarScenario& war);

struct WarScenarioSummary {
    std::array<std::int64_t, kWarOutcomes> outcomes{}; // Läufe je Ausgang
    std::int64_t not_started = 0;                      // Läufe ohne gültige Kriegserklärung
    Quantiles days;                                    // Läufe mit Frieden
    Quantiles battles;
    Quantiles attacker_losses;
    Quantiles defender_losses;
    Quantiles transfers;
    Quantiles attacker_debt;
    Quantiles defender_debt;
};

enum class WarCheck : std::uint8_t { NotStarted, Victory, Defeat, White, Duration, Fixed };

struct WarViolation {
    WarCheck check = WarCheck::NotStarted;
    std::size_t war = 0;
    Fixed value; // Anteil, Median der Tage, Zahl der Läufe
};

struct WarLabSummary {
    std::vector<WarScenarioSummary> wars;
    std::vector<WarViolation> violations;
};

/// Fasst die Läufe zusammen (runs[w]: alle Läufe des Krieges w) und prüft gegen die Ziele der Kriege.
WarLabSummary summarize_war_lab(const std::vector<WarScenario>& wars,
                                const std::vector<std::vector<WarLabRun>>& runs);

} // namespace uhrwerk
