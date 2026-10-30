#pragma once

// Datentypen der KI der Staaten (seit M12, docs/technik/ki.md Abschnitte 3 und 4): Regeln, Eigenschaften
// und Staatspersönlichkeiten mit ihren Neigungen, Personen mit Amtszeit, Agenden mit Zielen. Gelesen aus
// common/ki.chron, common/persoenlichkeiten.chron und common/agenden.chron (chronik-skript.md 3.37–3.39).

#include "uhrwerk/date.hpp"
#include "uhrwerk/event_types.hpp"
#include "uhrwerk/fixed.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace uhrwerk {

/// Die sechs Neigungen eines Staates (Abschnitt 3.1), je −2 … +2.
enum class Inclination : std::uint8_t { Risk, Expansion, Legitimacy, Reform, Thrift, Mediation };
inline constexpr std::size_t kInclinationCount = 6;
/// Schlüssel im Chronik-Skript in der Reihenfolge von Inclination.
inline constexpr std::array<std::string_view, kInclinationCount> kInclinationKeys = {
    "risiko", "expansion", "legitimitaet", "reform", "sparsamkeit", "vermittlung"};
using Inclinations = std::array<Fixed, kInclinationCount>;

/// Regeln der KI (common/ki.chron, Block ki { … }); Spielwerte.
struct AiRules {
    bool active = false;                   // ab 'seit' und nur mit Ereignissen (1648 bitgleich)
    Fixed noise = Fixed::from_int(10);     // Rauschen je Nutzen (± gleichverteilt, Strom Ki)
    std::int32_t cooldown_months = 12;     // Abkühlung nach Forderung, Krieg, Ablehnung
    Fixed debt_limit = Fixed::from_int(3); // Schuldenquote, ab der Steuern steigen
    bool open_agenda = false;              // offener Modus (ab M13 wählbar)
    Fixed ruler_weight_absolute = Fixed::from_ratio(3, 5);       // Gewicht des Herrschers ohne Verfassung
    Fixed ruler_weight_constitutional = Fixed::from_ratio(2, 5); // … in konstitutionellen Staaten
    Index default_personality = -1;                              // Staatspersönlichkeit ohne Agenda
};

/// Eigenschaft einer Person (Kapitel 13, Abschnitt 1.2) mit ihrem Beitrag zu den Neigungen.
struct TraitDef {
    std::string id;
    std::string name;
    Inclinations inclinations{};
};

/// Staatspersönlichkeit (Kapitel 21, Abschnitt 3.1) mit ihren Neigungen.
struct PersonalityDef {
    std::string id;
    std::string name;
    Inclinations inclinations{};
};

enum class PersonRole : std::uint8_t { Ruler, Minister };
inline constexpr std::size_t kPersonRoles = 2;

/// Grundwerte einer Person (Kapitel 13, Abschnitt 1.1), je 1 … 10.
enum class PersonValue : std::uint8_t { Statecraft, Diplomacy, Warfare, Appeal };
inline constexpr std::size_t kPersonValues = 4;
inline constexpr std::array<std::string_view, kPersonValues> kPersonValueKeys = {"staatskunst", "diplomatie",
                                                                                 "kriegskunst", "wirkung"};

/// Eine Person mit Amt (common/persoenlichkeiten.chron). Im Amt ist sie, wenn das Datum in von … bis liegt
/// und ihre Bedingung (im Staat) gilt; von mehreren die zuletzt beginnende.
struct PersonDef {
    std::string id;
    std::string name;
    std::string title; // Amt in der Chronik ("Ministerpräsident"); leer: nach Rolle
    Index state = -1;
    PersonRole role = PersonRole::Ruler;
    Date from;
    bool has_to = false;
    Date to;
    bool has_condition = false;
    Condition condition;
    std::array<std::int32_t, kPersonValues> values{5, 5, 5, 5};
    std::vector<Index> traits; // Indizes in World::ai_traits
    std::string source;
};

/// Arten der Ziele einer Agenda (Abschnitt 3.4).
enum class AgendaKind : std::uint8_t {
    LeadFederation, // fuehrung_bund
    CustomsUnion,   // zollunion (Ziel: Zollunion)
    Alliance,       // buendnis (Ziel: Staat)
    Territory,      // gebiet (Ziel: Verwaltungseinheit)
    Annexation,     // annexion (Ziel: Staat)
    Weaken,         // schwaechen (Ziel: Staat)
    StatusQuo,      // status_quo
    Calm,           // ruhe
};
inline constexpr std::size_t kAgendaKinds = 8;
inline constexpr std::array<std::string_view, kAgendaKinds> kAgendaKindKeys = {
    "fuehrung_bund", "zollunion", "buendnis", "gebiet", "annexion", "schwaechen", "status_quo", "ruhe"};

struct AgendaGoal {
    AgendaKind kind = AgendaKind::StatusQuo;
    Fixed weight;
    Index target = -1; // Staat, Zollunion oder Verwaltungseinheit je nach Art; -1 ohne
    bool has_from = false;
    Date from;
    bool has_to = false;
    Date to;
    bool has_condition = false;
    Condition condition;
};

/// Agenda eines Staates (common/agenden.chron): Staatspersönlichkeit und Ziele.
struct AgendaDef {
    Index personality = -1; // -1: AiRules::default_personality
    std::vector<AgendaGoal> goals;
};

} // namespace uhrwerk
