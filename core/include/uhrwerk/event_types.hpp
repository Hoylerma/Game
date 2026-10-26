#pragma once

// Typen der Ereignisse, Gesetze und Journale (seit M11, docs/technik/ereignisse.md). Nur Daten; die
// Regeln stehen in events.hpp. world.hpp bindet diese Datei ein.

#include "uhrwerk/date.hpp"
#include "uhrwerk/fixed.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace uhrwerk {

using Index = std::int32_t; // wie in world.hpp

// ─── Gesetze (Abschnitt 4.1) ────────────────────────────────────────────────

/// Option einer Gesetzesgruppe, z. B. „Pressefreiheit“.
struct LawOption {
    std::string id; // z. B. "pressefreiheit"
    std::string name;
};

/// Gesetzesgruppe (data/common/gesetze.chron), z. B. Presse mit Vorzensur, Pressegesetz und Pressefreiheit.
struct LawGroup {
    std::string id; // z. B. "presse"
    std::string name;
    std::vector<LawOption> options;
    Index standard = 0; // Option, solange die Historie des Staates keine nennt
    std::string source;
};

/// Wunsch einer Interessengruppe: Zustimmung, die eine Gesetzesoption auf Dauer bringt (−10 … +10).
struct LawWish {
    Index law = 0;
    Index option = 0;
    Fixed approval;
};

// ─── Bedingungen und Wirkungen (Abschnitte 3.2 und 3.3) ─────────────────────

enum class CompareOp : std::uint8_t { Eq, Ne, Lt, Le, Gt, Ge };

/// Art einer Prüfung. a, b und value tragen je nach Art Verweise und Vergleichswert.
enum class ConditionKind : std::uint8_t {
    All,            // alle Kinder (Block, bedingung { … })
    Any,            // oder { … }
    Not,            // nicht { … }: keines der Kinder
    Date,           // datum <op> date
    State,          // staat = a
    StateIn,        // staat_in = { … }: list
    Happened,       // geschehen = Ereignis a (Option b oder kNone) für diesen Staat bzw. die Welt
    Flag,           // merkmal = name
    WorldFlag,      // welt_merkmal = name
    Law,            // gesetz <a> <op> Option b (op Eq/Ne)
    Constitution,   // verfassung <op> a (Constitution)
    Franchise,      // wahlrecht <op> a (Franchise)
    Approval,       // zustimmung <Gruppe a> <op> value
    Share,          // anteil <Gruppe a> <op> value
    Influence,      // einfluss <Gruppe a> <op> value
    Unrest,         // unruhe <op> value
    NeighborUnrest, // nachbar_unruhe <op> value: ein Nachbarstaat
    LivingLower,    // lebensstandard_unten <op> value
    Price,          // preis <Gut a> <op> value (Markt der Hauptstadt)
    AtWar,          // im_krieg
    WarWith,        // krieg_mit = a
    Mobilized,      // mobil
    Federal,        // bund
    CustomsUnion,   // zollunion = a
    Relation,       // beziehung <Staat a> <op> value
    Seats,          // sitze <Strömung a> <op> value in Parlament b (letzte Wahl)
    Soldiers,       // soldaten <op> value
    Treasury,       // schatz <op> value
    DebtRatio,      // schulden_quote <op> value (Schulden ÷ Jahreseinnahmen)
    Player,         // spieler
    OfState,        // staat <TAG> { … }: die Kinder für den Staat a (kNone: gibt es nicht, nie wahr)
    AnyState,       // ein_staat { … }: die Kinder für mindestens einen Staat
    Never,          // Verweis auf etwas, das es zum Startdatum nicht gibt (späterer Inhalt): nie wahr
};

struct Condition {
    ConditionKind kind = ConditionKind::All;
    CompareOp op = CompareOp::Eq;
    Index a = -1;
    Index b = -1;
    Fixed value;
    Date date;
    std::string name;        // Flag
    std::vector<Index> list; // StateIn
    std::vector<Condition> children;
};

enum class EffectKind : std::uint8_t {
    Law,            // gesetz <a> = Option b
    Constitution,   // verfassung = a
    Franchise,      // wahlrecht = a
    Approval,       // zustimmung <Gruppe a> = ±value
    Unrest,         // unruhe = ±value
    SetFlag,        // merke = name
    ClearFlag,      // vergiss = name
    SetWorldFlag,   // welt_merke = name
    ClearWorldFlag, // welt_vergiss = name
    Treasury,       // schatz = ±value
    MonthlyIncome,  // monatseinnahmen = ±value (Vielfaches der Monatseinnahmen in die Kasse)
    Relation,       // beziehung <Staat a> = ±value
    Emigration,     // auswanderung = value (Anteil der unteren Schicht)
    Devastation,    // verwuestung = value in Ort a (kNone: Hauptstadt)
    Mobilize,       // mobilmachung
    Follow,         // folge { ereignis = a tage = days staat = b }
    Chronicle,      // chronik = text
    Random,         // zufall { <gewicht> = { … } … }: genau ein Zweig
    OfState,        // staat <TAG> { … }: die Kinder für den Staat a
    AllStates,      // alle_staaten { … }: die Kinder für jeden Staat
};

struct Effect {
    EffectKind kind = EffectKind::Chronicle;
    Index a = -1;
    Index b = -1;
    Fixed value;
    std::int32_t days = 0;
    std::string name;             // Flag oder Chroniktext
    std::vector<Effect> children; // OfState, AllStates
    /// Random: je Zweig ein Gewicht (weights[i]) und seine Wirkungen (branches[i]).
    std::vector<Fixed> weights;
    std::vector<std::vector<Effect>> branches;
};

// ─── Ereignisse (Abschnitt 3.1) ─────────────────────────────────────────────

enum class EventKind : std::uint8_t { Historical, Dynamic, Atmospheric };
enum class EventScope : std::uint8_t { State, World };

/// Zuschlag auf das Gewicht einer Option, solange die Bedingung gilt (gewicht_wenn).
struct WeightModifier {
    Condition condition;
    Fixed plus;
};

struct EventOption {
    std::string id;
    std::string text;
    bool historical = false;
    Fixed weight;
    std::vector<WeightModifier> weight_modifiers;
    Condition condition; // Verfügbarkeit (leer: immer)
    std::vector<Effect> effects;
};

struct EventDef {
    std::string id; // z. B. "maerz.forderungen"
    std::string title;
    std::string text;      // Absätze getrennt durch "\n\n"
    bool has_date = false; // historisches Datum (Anzeige, Kompass)
    Date date;
    std::string source;
    std::string certainty; // z. B. "ungeprueft"
    EventKind kind = EventKind::Historical;
    bool turning_point = false;
    EventScope scope = EventScope::State;
    Condition condition;
    std::int32_t wait = 0;       // mittlere Wartezeit in Tagen; 0 = sofort
    bool has_fixed_date = false; // termin
    Date fixed_date;
    bool repeatable = false;
    std::int32_t spacing = 0;  // Mindestabstand in Tagen bei wiederholbaren Ereignissen
    std::int32_t deadline = 0; // Frist für die Wahl des Spielers; 0 = Standard der Regeln
    std::vector<EventOption> options;
    std::string file; // Quelldatei (Meldungen)
    int line = 0;
};

/// Regeln der Ereignisse (data/common/ereignisse.chron).
struct EventRules {
    bool active = false;        // ab 'seit'; sonst gibt es keine Ereignisse (1648 bitgleich)
    std::int32_t deadline = 30; // Standardfrist der Wahl des Spielers in Tagen
};

// ─── Zustand ────────────────────────────────────────────────────────────────

/// Eingetretenes Ereignis mit gewählter Option (Zustand).
struct EventRecord {
    Index event = -1;
    Index state = -1; // -1 für Welt-Ereignisse
    Index option = -1;
    Date date;

    friend bool operator==(const EventRecord&, const EventRecord&) = default;
};

/// Ereignis des Spielers, das auf seine Wahl wartet (Zustand).
struct PendingEvent {
    Index event = -1;
    Index state = -1;
    Date since;
    Date due; // ohne Wahl gilt an diesem Tag die Option mit dem größten Gewicht

    friend bool operator==(const PendingEvent&, const PendingEvent&) = default;
};

/// Geplantes Folgeereignis (Zustand).
struct ScheduledEvent {
    Index event = -1;
    Index state = -1;
    Date due;

    friend bool operator==(const ScheduledEvent&, const ScheduledEvent&) = default;
};

/// Merkmal (Flag) eines Staates oder der Welt (state = -1), Zustand.
struct EventFlag {
    Index state = -1;
    std::string name;

    friend bool operator==(const EventFlag&, const EventFlag&) = default;
};

} // namespace uhrwerk
