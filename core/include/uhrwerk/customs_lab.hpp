#pragma once

// Zolllabor (seit M9, docs/technik/diplomatie.md, Abschnitt 3.6).
//
// Vergleicht je Seed Läufe gleichen Startzustands: ohne Wechsel und mit dem Wechsel eines Staates in
// eine Zollunion (oder aus ihr heraus) zum ersten 1. Januar. Gemessen wird im letzten Jahr, je Monat
// gemittelt: der Handel über die frühere Zollgrenze, der Handel der Märkte des Staates, seine
// Zolleinnahme und das Preisniveau seiner Märkte; dazu im Lauf ohne Wechsel der Anteil des Zolls an
// den Einnahmen der Mitglieder jeder Union. Zielwerte: common/zollziele_<szenario>.chron (Format:
// docs/technik/chronik-skript.md, Abschnitt 3.26). Wie der Balancing-Bericht kein Teil der Simulation.

#include "uhrwerk/balance.hpp"
#include "uhrwerk/fixed.hpp"
#include "uhrwerk/simulation.hpp"
#include "uhrwerk/world.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uhrwerk {

/// Ein Wechsel (Block `wechsel <kennung> { … }` oder --beitritt/--austritt) mit seinen Zielwerten: je
/// Messgröße der Faktor „mit ÷ ohne Wechsel“, Median über die Läufe.
struct CustomsChange {
    std::string id;
    Index state = kNone;
    Index customs_union = kNone; // kNone: Austritt
    std::optional<ValueRange> border_trade;
    std::optional<ValueRange> trade;
    std::optional<ValueRange> revenue;
};

/// Anteil des Zolls an den Einnahmen der Mitglieder einer Union (Block `anteil <union> { zoll }`).
struct CustomsShareTarget {
    Index customs_union = kNone;
    ValueRange share;
};

struct CustomsTargets {
    std::vector<CustomsChange> changes;
    std::vector<CustomsShareTarget> shares;
};

struct CustomsTargetsResult {
    CustomsTargets targets;
    std::vector<std::string> errors; // "datei:zeile:spalte: Meldung"

    bool ok() const { return errors.empty(); }
};

/// Liest die Zielwerte; Staaten und Unionen werden in world gesucht.
CustomsTargetsResult parse_customs_targets(std::string_view text, const World& world,
                                           const std::string& file_name = "<text>");
CustomsTargetsResult load_customs_targets(const std::filesystem::path& file, const World& world);

/// Messwerte eines Laufs für einen Wechsel, je Monat im letzten Jahr gemittelt (Taler bzw. Faktor).
struct CustomsMeasure {
    Fixed border_trade; // Handel zwischen den Märkten des Staates und denen der Union (Wert)
    Fixed trade;        // Handel der Märkte des Staates mit allen anderen und dem Weltmarkt (Wert)
    Fixed revenue;      // Zolleinnahme des Staates im Haushalt
    Fixed price_level;  // Preis ÷ Basispreis, gemittelt über die Güter und seine Märkte
};

/// Ergebnis eines Seeds.
struct CustomsLabRun {
    std::vector<Fixed> share;            // je Union: Zoll ÷ Einnahmen der Mitglieder (ohne Wechsel)
    std::vector<CustomsMeasure> without; // je Wechsel: Lauf ohne Wechsel
    std::vector<CustomsMeasure> with;    // je Wechsel: Lauf mit Wechsel
    std::vector<bool> accepted;          // je Wechsel: Antrag angenommen
};

/// Rechnet einen Seed: einen Lauf ohne Wechsel und je Wechsel einen Lauf mit ihm, jeweils bis zum
/// 1. Januar years Jahre nach dem ersten 1. Januar.
CustomsLabRun run_customs_lab(const World& world, const SimConfig& config, std::int64_t years,
                              const std::vector<CustomsChange>& changes);

struct CustomsChangeSummary {
    Quantiles border_trade; // Faktoren mit ÷ ohne
    Quantiles trade;
    Quantiles revenue;
    Quantiles price_level;
    Quantiles revenue_without; // Zolleinnahme ohne Wechsel (Taler je Monat)
    std::int64_t rejected = 0; // Läufe, in denen der Antrag abgelehnt wurde
};

enum class CustomsCheck { Share, BorderTrade, Trade, Revenue, Rejected };

struct CustomsViolation {
    CustomsCheck check = CustomsCheck::Share;
    std::size_t index = 0; // Union (Share) bzw. Wechsel
    Fixed value;           // Median
};

struct CustomsLabSummary {
    std::vector<Quantiles> share; // je Union
    std::vector<CustomsChangeSummary> changes;
    std::vector<CustomsViolation> violations;
};

/// Fasst die Seeds zusammen und prüft gegen targets (falls angegeben; die Ziele der Wechsel stehen in
/// changes selbst).
CustomsLabSummary summarize_customs_lab(const World& world, const std::vector<CustomsChange>& changes,
                                        const std::vector<CustomsLabRun>& runs,
                                        const CustomsTargets* targets);

} // namespace uhrwerk
