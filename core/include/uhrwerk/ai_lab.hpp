#pragma once

// KI-Labor (seit M12, Schritt a5; docs/technik/ki.md, Abschnitt 7).
//
// Rechnet je Seed eine Partie ohne Spieler bis zu einem Datum, mit der KI oder zum Vergleich ohne sie, und
// berichtet je Lauf den Ausgang eines Journals (der deutschen Frage) mit Datum, die Kriege, untergegangene
// Staaten, Bankrotte, die höchste Unruhe, die Entscheidungen der KI je Art und die Laufzeit mit dem Anteil
// der KI; über alle Läufe die Verteilung gegen die Korridore in common/kiziele_<szenario>.chron (Format:
// docs/technik/chronik-skript.md, Abschnitt 3.40). Die Läufe rechnen auf Wunsch gleichzeitig (je Lauf ein
// Thread); eine Ausnahme in einem Lauf zählt als Absturz, die übrigen rechnen weiter. Wie die übrigen
// Labore kein Teil der Simulation.

#include "uhrwerk/ai_types.hpp"
#include "uhrwerk/balance.hpp"
#include "uhrwerk/event_lab.hpp"
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

/// Korridor des Anteils der Läufe, in denen beide Staaten als Hauptparteien gegeneinander Krieg führen
/// (gleich, wer erklärt; auch ein Krieg, der am Ende noch läuft).
struct AiWarCorridor {
    Index first = kNone;
    Index second = kNone;
    ValueRange share;
};

/// Korridor des Anteils der Läufe mit mindestens einem Staatsbankrott eines der Staaten.
struct AiBankruptcyCorridor {
    std::vector<Index> states;
    ValueRange share;
};

struct AiTargets {
    Date until;                     // Ende der Läufe (labor.bis)
    Index journal = kNone;          // Journal, dessen Ausgänge zählen
    bool has_max_outcome = false;   // labor.hoechstens: kein Ausgang in mehr Läufen als …
    Fixed max_outcome;              // … diesem Anteil
    std::vector<Index> unification; // Ausgänge, die als Einigung zählen (einigung.ausgaenge)
    bool has_unification = false;
    ValueRange unification_share;
    std::vector<OutcomeCorridor> outcomes;
    std::vector<AiWarCorridor> wars;
    std::vector<AiBankruptcyCorridor> bankruptcies;
};

struct AiTargetsResult {
    AiTargets targets;
    std::vector<std::string> errors; // "datei:zeile:spalte: Meldung"

    bool ok() const { return errors.empty(); }
};

AiTargetsResult parse_ai_targets(std::string_view text, const World& world,
                                 const std::string& file_name = "<text>");
AiTargetsResult load_ai_targets(const std::filesystem::path& file, const World& world);

/// Ein Krieg eines Laufs zwischen den Hauptparteien.
struct AiLabWar {
    Index attacker = kNone;
    Index defender = kNone;
    Date started;
    bool ended = false;
    Date end;
    std::int32_t winner = 0; // 1 Angreifer, −1 Verteidiger, 0 ohne Gewinner (nur mit ended)
};

/// Ergebnis eines Laufs.
struct AiLabRun {
    std::uint64_t seed = 0;
    bool crashed = false;
    std::string error; // Meldung der Ausnahme bei einem Absturz
    Index outcome = kNone;
    Date outcome_date;
    std::vector<AiLabWar> wars;
    std::vector<Index> fallen;                            // beim Start bestehende Staaten ohne Orte am Ende
    std::vector<std::pair<Index, std::int32_t>> bankrupt; // Staat und Zahl der Bankrotte im Lauf
    Fixed unrest_max;                                     // höchste Unruhe an einem Monatsersten
    Index unrest_state = kNone;
    std::array<std::int64_t, kAiDecisionKinds> decisions{}; // Entscheidungen der KI je Art
    std::int64_t alliances = 0;                             // Bündnisse am Ende
    std::int64_t ai_state_months = 0;                       // Monatserste × bestehende Staaten
    double seconds = 0;
    double ai_seconds = 0;
    std::uint64_t checksum = 0; // Prüfsumme am Ende (gleich mit jeder Zahl von Threads)
    std::vector<Fixed> legacy;  // seit M13a4: Vermächtnis am Ende [staat × dimensionen], leer ohne
};

/// Rechnet einen Lauf mit config.seed bis targets.until, ohne Spieler.
AiLabRun run_ai_lab(const World& world, const SimConfig& config, const AiTargets& targets);

struct AiLabOptions {
    std::uint64_t seed = 1648; // erster Seed
    std::int64_t seeds = 1;
    unsigned parallel = 1;    // gleichzeitige Läufe, 0 = alle Kerne
    unsigned threads = 0;     // Threads je Lauf; mit mehr als einem gleichzeitigen Lauf je Lauf einer
    bool ai = true;           // false: ohne KI wie in M11 (--ohne-ki)
    Index difficulty = kNone; // Stufe der Spieleinstellung (--stufe, seit M13a1; kNone: standard_stufe)
    bool open = false;        // offener Geschichtsverlauf (--offen, seit M13a1)
};

/// Rechnet die Läufe seed … seed + seeds − 1, Ergebnisse in der Reihenfolge der Seeds.
std::vector<AiLabRun> run_ai_lab_seeds(const World& world, const AiTargets& targets,
                                       const AiLabOptions& options);

enum class AiCheck : std::uint8_t { Unification, Outcome, MaxOutcome, War, Bankruptcy, Crash };

struct AiViolation {
    AiCheck check = AiCheck::Outcome;
    std::size_t index = 0; // Korridor bzw. Stufe des Journals (MaxOutcome)
    Fixed value;           // gemessener Anteil bzw. Zahl der Abstürze
};

struct AiLabSummary {
    std::vector<std::int64_t> outcomes; // Läufe je Stufe des Journals (nur Ausgänge)
    std::int64_t open = 0;              // Läufe ohne Ausgang
    std::int64_t unified = 0;           // Läufe mit einem Ausgang aus einigung.ausgaenge
    std::int64_t crashed = 0;
    std::vector<Fixed> wars;         // je AiWarCorridor: Anteil der Läufe
    std::vector<Fixed> bankruptcies; // je AiBankruptcyCorridor: Anteil der Läufe
    std::array<std::int64_t, kAiDecisionKinds> decisions{};
    std::vector<AiViolation> violations;
};

/// Fasst die Läufe zusammen (Abstürze zählen nur als Abstürze) und prüft gegen die Korridore.
AiLabSummary summarize_ai_lab(const World& world, const AiTargets& targets,
                              const std::vector<AiLabRun>& runs);

} // namespace uhrwerk
