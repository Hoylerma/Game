#pragma once

// Ereignislabor (seit M11, Schritt a5; docs/technik/ereignisse.md, Abschnitt 7).
//
// Rechnet je Seed ein Lesezeichen ohne Spieler (alle Staaten wählen nach Gewicht) oder mit einer
// Strategie für einen Staat bis zu einem Datum und berichtet je Lauf den Ausgang eines Journals, seinen
// Weg, Ereignisse je Art, die Wahl der Wendepunkte, Aufstände, Unruhe und Gesetze; über alle Läufe die
// Verteilung gegen die Korridore in common/ereignisziele_<szenario>.chron (Format:
// docs/technik/chronik-skript.md, Abschnitt 3.36). Wie die übrigen Labore kein Teil der Simulation.

#include "uhrwerk/balance.hpp"
#include "uhrwerk/fixed.hpp"
#include "uhrwerk/simulation.hpp"
#include "uhrwerk/world.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace uhrwerk {

/// Korridor des Anteils der Läufe, die in einem Ausgang (Stufe des Journals) enden.
struct OutcomeCorridor {
    Index stage = kNone;
    ValueRange share;
};

/// Korridor für ein Ereignis: Anteil der Läufe, in denen es (mit der Option) bis zum Datum eintritt, oder
/// mit staaten = ja der mittlere Anteil der Staaten, für die es eintritt.
struct EventCorridor {
    Index event = kNone;
    Index option = kNone;
    bool has_until = false;
    Date until;
    bool states = false;
    ValueRange share;
};

/// Strategie eines Staates: bevorzugte Option je Ereignis; sonst die Option mit dem größten Gewicht.
struct EventStrategy {
    std::string id;
    Index player = kNone;
    std::vector<std::pair<Index, Index>> choices; // (Ereignis, Option)
    std::vector<OutcomeCorridor> outcomes;        // Korridore mit dieser Strategie
};

struct EventTargets {
    Date until;                // Ende der Läufe (labor.bis)
    Index journal = kNone;     // Journal, dessen Ausgänge zählen
    bool undetermined = false; // kein Ausgang in allen Läufen (ohne Strategie)
    std::vector<OutcomeCorridor> outcomes;
    std::vector<EventCorridor> events;
    std::vector<EventStrategy> strategies;
};

struct EventTargetsResult {
    EventTargets targets;
    std::vector<std::string> errors; // "datei:zeile:spalte: Meldung"

    bool ok() const { return errors.empty(); }
};

EventTargetsResult parse_event_targets(std::string_view text, const World& world,
                                       const std::string& file_name = "<text>");
EventTargetsResult load_event_targets(const std::filesystem::path& file, const World& world);

/// Ergebnis eines Laufs.
struct EventLabRun {
    Index outcome = kNone; // erreichter Ausgang des Journals, kNone: offen
    Date outcome_date;
    std::vector<JournalStep> path;         // Weg des Journals
    std::array<std::int64_t, 3> kinds{};   // eingetretene Ereignisse je EventKind
    std::vector<EventRecord> turning;      // Wendepunkte mit gewählter Option
    std::array<std::int64_t, 3> revolts{}; // beendete Aufstände je RevoltOutcome
    std::int64_t revolts_running = 0;
    Fixed unrest_median;                         // Unruhe der Staaten am Ende
    Fixed unrest_max;                            // höchste Unruhe eines Staates an einem Monatsersten
    std::vector<std::vector<std::int64_t>> laws; // [gruppe][option]: Staaten am Ende
    std::vector<Fixed> events;                   // je EventCorridor: 0/1 bzw. Anteil der Staaten
};

/// Rechnet einen Lauf mit config.seed; mit strategy wartet der Staat der Strategie und wählt nach ihr.
EventLabRun run_event_lab(const World& world, const SimConfig& config, const EventTargets& targets,
                          const EventStrategy* strategy = nullptr);

enum class EventCheck : std::uint8_t { Outcome, Determined, Event };

struct EventViolation {
    EventCheck check = EventCheck::Outcome;
    std::size_t index = 0; // Korridor
    Fixed value;           // gemessener Anteil
};

struct EventLabSummary {
    std::vector<std::int64_t> outcomes; // Läufe je Stufe des Journals (nur Ausgänge)
    std::int64_t open = 0;              // Läufe ohne Ausgang
    std::vector<Fixed> events;          // je EventCorridor: Mittel über die Läufe
    std::vector<EventViolation> violations;
};

/// Fasst die Läufe zusammen und prüft gegen die Korridore (mit Strategie deren Ausgänge).
EventLabSummary summarize_event_lab(const World& world, const EventTargets& targets,
                                    const EventStrategy* strategy, const std::vector<EventLabRun>& runs);

} // namespace uhrwerk
