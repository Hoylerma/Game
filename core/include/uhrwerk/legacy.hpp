#pragma once

// Vermächtnis, Jahrbuch und Ende der Partie (seit M13a4, docs/technik/vertical_slice.md 3.5 und 4.3).
//
// Das Vermächtnis eines Staates ist abgeleitet: fünf Dimensionen von 0 bis 100 aus den Faktoren in
// common/vermaechtnis.chron, jederzeit aus dem Zustand berechnet. Zustand sind nur die Jahresreihen
// (World::yearbook): Kennzahlen des Jahrbuchs und Vermächtnis je Staat am Start, an jedem 1. Januar und
// am Ende der Partie.

#include "uhrwerk/events.hpp"
#include "uhrwerk/fixed.hpp"
#include "uhrwerk/world.hpp"

#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace uhrwerk {

inline constexpr std::size_t kLegacyMeasures = static_cast<std::size_t>(LegacyMeasure::Count);

/// Name einer Kennzahl für Anzeige und Jahrbuch (z. B. „Lebensstandard“).
std::string_view legacy_measure_name(LegacyMeasure measure);
/// Kennung in den Daten (z. B. "lebensstandard").
std::string_view legacy_measure_id(LegacyMeasure measure);

/// Alle Kennzahlen eines Staates (Index nach LegacyMeasure; None bleibt 0).
std::array<Fixed, kLegacyMeasures> legacy_measures(const World& world, Index state);

/// Beitrag eines Faktors zu einer Dimension.
struct LegacyContribution {
    std::string name;
    Fixed value;
    bool active = false; // Bedingung erfüllt (ohne Bedingung immer)
};

/// Wert einer Dimension (0 … 100) mit Grundwert und Beiträgen.
struct LegacyValue {
    Fixed value;
    Fixed base;
    std::vector<LegacyContribution> factors;
};

/// Vermächtnis eines Staates je Dimension (parallel zu legacy_rules.dimensions); leer ohne Regeln.
std::vector<LegacyValue> legacy(const World& world, const EventCache& cache, Index state);

/// Satz zur Dimension für den Wert (der erste, dessen 'ab' erreicht ist); leer ohne passenden.
std::string legacy_sentence(const LegacyDimension& dimension, Fixed value);

/// Kennzahlen des Jahrbuchs (Spalten vor den Dimensionen).
inline constexpr std::array<LegacyMeasure, 12> kYearbookMeasures{
    LegacyMeasure::Population, LegacyMeasure::Income,         LegacyMeasure::Debt,
    LegacyMeasure::Treasury,   LegacyMeasure::LivingStandard, LegacyMeasure::Literacy,
    LegacyMeasure::Emigration, LegacyMeasure::Unrest,         LegacyMeasure::Approval,
    LegacyMeasure::Soldiers,   LegacyMeasure::Locations,      LegacyMeasure::Alliances};

/// Spalten einer Jahresreihe: Kennzahlen, dann Dimensionen.
std::size_t yearbook_columns(const World& world);

/// Jahresreihe zum heutigen Datum.
YearRecord year_record(const World& world, const EventCache& cache);

/// Die Partie ist zu Ende: Das Szenario hat ein 'ende', und dieser letzte Tag ist gerechnet (an ihm treten
/// keine Ereignisse mehr ein, die Journale wählen ihren Ausgang). Weiterspielen bleibt möglich.
bool game_over(const World& world);

} // namespace uhrwerk
