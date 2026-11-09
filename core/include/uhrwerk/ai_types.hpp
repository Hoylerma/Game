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
    // Seit Schritt a2 (Abschnitte 5 und 6.1, 6.3).
    Fixed threshold = Fixed::from_int(10);         // Nutzen, ab dem ein Planer handelt (schwelle)
    std::int32_t log_size = 20;                    // Einträge im KI-Protokoll je Staat (protokoll)
    std::int32_t tax_cooldown_months = 3;          // Monate nach einer Steueränderung (abkuehlung_steuern)
    Fixed surplus_from = Fixed::from_ratio(3, 20); // Überschussanteil, ab dem Steuern sinken (ueberschuss_ab)
    Fixed unrest_tax_from = Fixed::from_int(50);   // Unruhe, ab der Steuern sinken (unruhe_steuern_ab)
    Fixed reserve_from = Fixed::one(); // Schatz ÷ Jahreseinnahmen, ab dem ein Überschuss Steuern senkt
                                       // (ruecklage_ab)
    Fixed threat_mobilize = Fixed::from_ratio(3, 2); // Bedrohung, ab der gegen einen mobilen Nachbarn
                                                     // mobil gemacht wird (bedrohung_mobil)
    std::int32_t quiet_months = 2; // Monatserste ohne Krieg und Krise bis zur Abrüstung (abruestung_monate)
    // Seit Schritt a3 (Abschnitte 6.2 und 6.4).
    std::int32_t max_alliances = 3;                   // Bündnisse, über die hinaus keine angeboten werden
    std::int32_t campaign_months = 4;                 // Wahlkampf der Strömungen in den Monaten vor der Wahl
    Fixed campaign_reserve = Fixed::from_ratio(1, 5); // Rücklage der Mittel einer Strömung
    // Seit Schritt a4 (Abschnitt 6.3).
    Fixed war_ratio = Fixed::from_ratio(13, 10); // Stärkeverhältnis der Seiten für eine Kriegserklärung
                                                 // (kriegs_staerke; minus 0,15 je Punkt Risiko)
    // Seit M13a1 (vertical_slice.md 3.2): Schwierigkeitsstufe ohne Spieleinstellung (standard_stufe).
    Index default_difficulty = -1;
    // Nur in effective_ai_rules gesetzt (aus der Stufe, kein Datenfeld des Blocks ki): Zuschlag auf das
    // nötige Stärkeverhältnis für Forderung und Krieg und ob der Spielerstaat als Rivale zählt.
    Fixed caution;
    bool player_rival = false;
};

/// Schwierigkeitsstufe (seit M13a1, vertical_slice.md 3.2; Kapitel 21, Abschnitt 8): Faktoren auf die
/// Regeln derselben Planer, keine Boni.
struct DifficultyDef {
    std::string id;
    std::string name;
    std::string text;               // Kurztext für die Auswahl
    Fixed threshold = Fixed::one(); // × schwelle
    Fixed noise = Fixed::one();     // × rauschen
    Fixed cooldown = Fixed::one();  // × abkuehlung und abkuehlung_steuern
    Fixed caution;                  // + nötiges Stärkeverhältnis für Forderung und Kriegserklärung
    bool player_rival = false; // der Spielerstaat ist Rivale, wenn er fast so bedrohlich ist wie der größte
};

/// Spieleinstellungen einer Partie (seit M13a1, Befehl Spieleinstellung am ersten Tag; im Zustand nur mit
/// KI-Daten).
struct GameSettings {
    Index difficulty = -1; // Stufe (-1: standard_stufe)
    bool open = false;     // Geschichtsverlauf offen (wie agenda_offen)
};

/// Faktor eines Nutzens (Abschnitt 5); seine Bezeichnung liefert ai_reason_text.
enum class AiFactor : std::uint8_t {
    Deficit,   // Defizit des Vormonats
    DebtRatio, // Schuldenquote über der Grenze
    Surplus,   // Überschuss des Vormonats
    Unrest,    // Unruhe
    Thrift,    // Neigung Sparsamkeit
    Reform,    // Neigung Reform
    Risk,      // Neigung Risiko
    Threat,    // Bedrohung durch einen Staat (other)
    Mobilized, // ein Nachbar ist mobil (other)
    Quiet,     // weder Krieg noch Krise
    // seit a3: Diplomatie und Politik
    Agenda,     // Gewicht eines Ziels der Agenda (other: Gegenstand, wenn ein Staat)
    Rival,      // gemeinsamer Rivale bzw. der Gegner ist der Rivale (other)
    Relation,   // Beziehung zum anderen Staat (other)
    Strength,   // Stärkeverhältnis zum anderen Staat (other)
    Neighbor,   // gemeinsame Grenze (other)
    Ally,       // Verbündeter oder Garant (other)
    Expansion,  // Neigung Expansion
    Legitimacy, // Neigung Legitimität
    Mediation,  // Neigung Vermittlung
    Alliances,  // Zahl der eigenen Bündnisse (dagegen)
    // seit a4: Krieg
    WarScore,  // Kriegspunkte gegen die Kosten der Ziele
    Weariness, // Kriegsmüdigkeit
};
inline constexpr std::size_t kAiFactors = 22;

/// Ein Grund: Faktor, betroffener Staat (oder -1) und Beitrag zum Nutzen.
struct AiReason {
    AiFactor factor = AiFactor::Deficit;
    Index other = -1;
    Fixed value;

    friend bool operator==(const AiReason&, const AiReason&) = default;
};
inline constexpr std::size_t kAiReasons = 3;

/// Art einer Entscheidung im KI-Protokoll.
enum class AiDecisionKind : std::uint8_t {
    RaiseTax,
    LowerTax,
    Mobilize,
    Demobilize,
    // seit a3 (Abschnitt 6.2)
    SendEnvoy,      // Ziel: Staat
    OfferAlliance,  // Ziel: Staat
    OfferGuarantee, // Ziel: Staat
    Demand,         // Ziel: Staat, Wert: DemandKind
    Manoeuvre,      // Ziel: Gegner im Spiel, Wert: Manoeuvre
    Motion,         // Ziel: Zollunion oder -1, Wert: MotionKind
    JoinUnion,      // Ziel: Zollunion
    // seit a4 (Abschnitt 6.3)
    DeclareWar, // Ziel: Staat, Wert: Kriegsziel (encode_war_goal)
    AddWarGoal, // Ziel: Gegner (Hauptpartei), Wert: Kriegsziel
    OfferPeace, // Ziel: Gegner (Hauptpartei), Wert: PeaceTerms
};
inline constexpr std::size_t kAiDecisionKinds = 14;

/// Eintrag im KI-Protokoll: wer, wann, was, mit welchem Nutzen und den drei wichtigsten Gründen.
struct AiDecision {
    Date date;
    Index state = -1;
    AiDecisionKind kind = AiDecisionKind::RaiseTax;
    Index target = -1; // Steuerart bzw. Staat; -1 ohne
    Fixed value;       // neuer Steuersatz bzw. 0
    Fixed utility;     // Nutzen mit Rauschen
    std::uint8_t reason_count = 0;
    std::array<AiReason, kAiReasons> reasons{};

    friend bool operator==(const AiDecision&, const AiDecision&) = default;
};

/// Abkühlung: Bis 'until' plant der Staat diese Art gegen dieses Ziel nicht neu.
struct AiCooldown {
    Index state = -1;
    AiDecisionKind kind = AiDecisionKind::RaiseTax;
    Index target = -1;
    Date until;

    friend bool operator==(const AiCooldown&, const AiCooldown&) = default;
};

/// Strategie eines Staates (Abschnitt 4.2), am Monatsersten neu bestimmt.
struct AiStrategy {
    Index rival = -1;              // Rivale: Ziel von „schwaechen“ oder größte Bedrohung
    Index goal = -1;               // Hauptziel: Index in den Zielen der Agenda, -1 ohne
    std::int32_t quiet_months = 0; // Monatserste mobil ohne Krieg und Krise in Folge

    friend bool operator==(const AiStrategy&, const AiStrategy&) = default;
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

/// Beendete Amtszeit einer Person (seit M13a5b, Zustand): Kabinett und Hof zeigen die früheren Amtsinhaber.
struct OfficeTerm {
    Index state = -1;
    PersonRole role = PersonRole::Ruler;
    Index person = -1;
    Date to; // letzter Tag im Amt (der Tag des Wechsels)

    friend bool operator==(const OfficeTerm&, const OfficeTerm&) = default;
};

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
