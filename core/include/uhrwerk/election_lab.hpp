#pragma once

// Wahllabor (seit M8, docs/technik/politik.md, Abschnitt 7.8).
//
// Rechnet eine Wahl über viele Seeds und fasst sie zusammen: Sitze je Strömung und Fraktion, besetzte
// Sitze, Beteiligung, Werte je Staat. Die Zielwerte (common/wahlziele_<szenario>.chron, Format:
// docs/technik/chronik-skript.md, Abschnitt 3.24) prüfen Anteile von Blöcken aus Fraktionen oder
// Strömungen am Median über die Seeds. Wie der Balancing-Bericht kein Teil der Simulation.

#include "uhrwerk/balance.hpp"
#include "uhrwerk/fixed.hpp"
#include "uhrwerk/simulation.hpp"
#include "uhrwerk/world.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uhrwerk {

/// Block der Zielwerte: Anteil seiner Sitze an den besetzten Sitzen.
struct ElectionBlockTarget {
    std::string id;
    std::vector<Index> factions; // Fraktionen des Parlaments; kNone steht für die Fraktionslosen
    std::vector<Index> currents; // oder Strömungen (genau eines von beiden ist gefüllt)
    ValueRange share;
};

/// Zielwerte einer Wahl (Block `wahl <parlament> { … }`).
struct ElectionTargets {
    Index parliament = kNone;
    std::optional<ValueRange> seated;  // besetzte Sitze, Median
    std::optional<ValueRange> turnout; // Median der Beteiligung der gewählten Wahlkreise, Median
    std::vector<ElectionBlockTarget> blocks;
};

struct ElectionTargetsResult {
    std::vector<ElectionTargets> targets; // je Parlament höchstens einer
    std::vector<std::string> errors;      // "datei:zeile:spalte: Meldung"

    bool ok() const { return errors.empty(); }
    const ElectionTargets* find(Index parliament) const;
};

/// Liest die Zielwerte; Parlamente, Fraktionen und Strömungen werden in world gesucht.
ElectionTargetsResult parse_election_targets(std::string_view text, const World& world,
                                             const std::string& file_name = "<text>");
ElectionTargetsResult load_election_targets(const std::filesystem::path& file, const World& world);

/// Rechnet die Simulation bis zur Wahl des Parlaments und gibt deren Ergebnis zurück; nullopt, wenn der
/// Wahltag vor dem aktuellen Datum liegt und die Wahl nicht schon stattfand.
std::optional<ElectionResult> run_to_election(Simulation& sim, Index parliament);

/// Messwerte einer Wahl.
struct ElectionRun {
    std::int32_t seated = 0;
    Fixed turnout;                                           // Median über die gewählten Wahlkreise
    std::vector<std::int32_t> by_current;                    // je Strömung
    std::vector<std::int32_t> by_faction;                    // je Fraktion, zuletzt die Fraktionslosen
    std::vector<Fixed> state_electorate;                     // je Staat
    std::vector<Fixed> state_voters;                         // je Staat
    std::vector<std::int32_t> state_seats;                   // je Staat: Sollzahl seiner Wahlkreise
    std::vector<std::vector<std::int32_t>> state_by_current; // je Staat und Strömung
};

ElectionRun measure_election(const World& world, const ElectionResult& election);

enum class ElectionCheck { Seated, Turnout, Block };

struct ElectionViolation {
    ElectionCheck check = ElectionCheck::Seated;
    std::size_t block = 0; // bei Block: Index in ElectionTargets::blocks
    Fixed value;           // Median
};

struct ElectionSummary {
    std::int64_t runs = 0;
    Quantiles seated;
    Quantiles turnout;
    std::vector<Quantiles> by_current;
    std::vector<Quantiles> by_faction;
    std::vector<Quantiles> block_share;      // je Block der Zielwerte (Anteil an den besetzten Sitzen)
    std::vector<Quantiles> state_electorate; // je Staat
    std::vector<Quantiles> state_turnout;    // je Staat: Wähler ÷ Berechtigte
    std::vector<std::int32_t> state_seats;   // je Staat (in allen Läufen gleich)
    std::vector<std::vector<Quantiles>> state_by_current;
    std::vector<ElectionViolation> violations;
};

/// Fasst die Läufe zusammen und prüft sie gegen targets (falls angegeben).
ElectionSummary summarize_elections(const World& world, Index parliament,
                                    const std::vector<ElectionRun>& runs, const ElectionTargets* targets);

} // namespace uhrwerk
