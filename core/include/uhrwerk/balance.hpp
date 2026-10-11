#pragma once

// Balancing-Bericht (seit M5, docs/technik/wirtschaft.md, Abschnitt 7).
//
// record_run rechnet einen Lauf und misst dabei Jahreswerte je Staat (am Jahrestag des Starts),
// Monatspreise je Gut und Markt, Bevölkerung und Lebensstandard. summarize fasst viele Läufe
// zusammen (Median, 5- und 95-%-Wert) und prüft sie gegen die Zielwerte aus
// data/common/zielwerte.chron. Alles rechnet in Festkomma, der Bericht ist daher auf jeder
// Plattform gleich. Der Bericht ist kein Teil der Simulation: kein Zustand, keine Prüfsumme.

#include "uhrwerk/fixed.hpp"
#include "uhrwerk/simulation.hpp"
#include "uhrwerk/world.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uhrwerk {

/// Geschlossener Bereich [min, max].
struct ValueRange {
    Fixed min;
    Fixed max;

    bool contains(Fixed value) const { return value >= min && value <= max; }
};

/// Zielwerte des Berichts (Format: docs/technik/chronik-skript.md, Abschnitt 3.15).
struct BalanceTargets {
    ValueRange wealth;                         // Vermögen ÷ Jahreseinnahmen, jedes Staatsjahr
    Fixed wealth_share;                        // Mindestanteil der Staatsjahre im Bereich (je Staat)
    ValueRange wealth_end;                     // Median des Vermögens am Ende (je Staat)
    Fixed bankruptcies_median;                 // höchstens, je Staat und Jahrhundert
    Fixed bankruptcies_p95;                    // höchstens (95-%-Wert über die Läufe)
    ValueRange tax_ratio;                      // Steuerquote am Ende ÷ am Start (Median je Staat)
    ValueRange price;                          // Preis ÷ Basispreis
    Fixed price_share;                         // Mindestanteil der Markt-Monate im Bereich (je Gut)
    std::vector<ValueRange> price_by_good;     // je Gut: price oder die Ausnahme
    std::int32_t population_year = 0;          // Jahrestag des Starts in diesem Jahr …
    ValueRange population;                     // … Bevölkerung gegen den Start (Median)
    std::array<ValueRange, 3> living_standard; // je Schicht: Median über alle Jahre und Läufe
};

struct BalanceTargetsResult {
    std::optional<BalanceTargets> targets;
    std::vector<std::string> errors; // "datei:zeile:spalte: Meldung"
};

/// Liest die Zielwerte; Güter der Ausnahmen werden in world gesucht.
BalanceTargetsResult parse_balance_targets(std::string_view text, const World& world,
                                           const std::string& file_name = "<text>");
BalanceTargetsResult load_balance_targets(const std::filesystem::path& file, const World& world);

/// Messwerte eines Laufs.
struct BalanceRun {
    std::int64_t years = 0;
    std::vector<std::vector<Fixed>> wealth;            // je Staat: Vermögen ÷ Jahreseinnahmen je Jahr
    std::vector<std::int32_t> bankruptcies;            // je Staat: Staatsbankrotte im Lauf
    std::vector<std::optional<Fixed>> tax_ratio;       // je Staat: Steuerquote Ende ÷ Start
    std::vector<std::int64_t> price_months;            // je Gut: Markt-Monate
    std::vector<std::int64_t> price_in_range;          // je Gut: davon im Zielbereich
    std::vector<Fixed> price_min;                      // je Gut: kleinster Faktor auf den Basispreis
    std::vector<Fixed> price_max;                      // je Gut: größter Faktor
    std::optional<Fixed> population_factor;            // Bevölkerung im Zieljahr ÷ Start
    std::array<std::vector<Fixed>, 3> living_standard; // je Schicht: Jahreswerte (gewichtet)
};

/// Rechnet years Jahre ab dem aktuellen Datum der Simulation und misst dabei.
BalanceRun record_run(Simulation& sim, std::int64_t years, const BalanceTargets& targets);

/// Median, 5- und 95-%-Wert (nächster Rang).
struct Quantiles {
    Fixed p5;
    Fixed median;
    Fixed p95;
};

/// Quantil p = num/den (0..1) nach dem nächsten Rang: sortierter Wert an Stelle ⌈p·n⌉ (ab 1, ganzzahlig
/// gerechnet); 0 ohne Werte.
Fixed quantile(std::vector<Fixed> values, std::int64_t num, std::int64_t den);
Quantiles quantiles(const std::vector<Fixed>& values);

struct BalanceStateSummary {
    Quantiles wealth;       // alle Staatsjahre aller Läufe
    Fixed wealth_share;     // Anteil der Staatsjahre im Zielbereich
    Fixed wealth_min;       // tiefster Wert
    Quantiles wealth_end;   // am Ende je Lauf
    Quantiles bankruptcies; // je Jahrhundert
    std::optional<Quantiles> tax_ratio;
};

struct BalanceGoodSummary {
    std::int64_t months = 0;
    Fixed share; // Anteil im Zielbereich
    Fixed min;
    Fixed max;
};

/// Art eines Verstoßes; subject ist Staat, Gut oder Schicht (sonst kNone).
enum class BalanceCheck {
    WealthShare,
    WealthEnd,
    BankruptciesMedian,
    BankruptciesP95,
    TaxRatio,
    PriceShare,
    Population,
    LivingStandard,
};

struct BalanceViolation {
    BalanceCheck check = BalanceCheck::WealthShare;
    Index subject = kNone;
    Fixed value;
};

struct BalanceSummary {
    std::int64_t runs = 0;
    std::int64_t years = 0;
    std::vector<BalanceStateSummary> states;
    std::vector<BalanceGoodSummary> goods;
    std::optional<Quantiles> population;
    std::array<std::optional<Quantiles>, 3> living_standard;
    std::vector<BalanceViolation> violations;
};

BalanceSummary summarize(const World& world, const std::vector<BalanceRun>& runs,
                         const BalanceTargets& targets);

} // namespace uhrwerk
