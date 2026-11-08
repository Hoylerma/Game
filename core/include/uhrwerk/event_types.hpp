#pragma once

// Typen der Ereignisse, Gesetze und Journale (seit M11, docs/technik/ereignisse.md). Nur Daten; die
// Regeln stehen in events.hpp. world.hpp bindet diese Datei ein.

#include "uhrwerk/date.hpp"
#include "uhrwerk/fixed.hpp"

#include <array>
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
    Fixed unrest; // seit a3: Beitrag zum Zielwert der Unruhe (Zensur, Abschnitt 4.3)
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
    All,             // alle Kinder (Block, bedingung { … })
    Any,             // oder { … }
    Not,             // nicht { … }: keines der Kinder
    Date,            // datum <op> date
    State,           // staat = a
    StateIn,         // staat_in = { … }: list
    Happened,        // geschehen = Ereignis a (Option b oder kNone) für diesen Staat bzw. die Welt
    Flag,            // merkmal = name
    WorldFlag,       // welt_merkmal = name
    Law,             // gesetz <a> <op> Option b (op Eq/Ne)
    Constitution,    // verfassung <op> a (Constitution)
    Franchise,       // wahlrecht <op> a (Franchise)
    Approval,        // zustimmung <Gruppe a> <op> value
    Share,           // anteil <Gruppe a> <op> value
    Influence,       // einfluss <Gruppe a> <op> value
    Unrest,          // unruhe <op> value
    NeighborUnrest,  // nachbar_unruhe <op> value: ein Nachbarstaat
    LivingLower,     // lebensstandard_unten <op> value
    Price,           // preis <Gut a> <op> value (Markt der Hauptstadt)
    AtWar,           // im_krieg
    WarWith,         // krieg_mit = a
    Mobilized,       // mobil
    Federal,         // bund
    CustomsUnion,    // zollunion = a
    Relation,        // beziehung <Staat a> <op> value
    Seats,           // sitze <Strömung a> <op> value in Parlament b (letzte Wahl)
    Soldiers,        // soldaten <op> value
    Treasury,        // schatz <op> value
    DebtRatio,       // schulden_quote <op> value (Schulden ÷ Jahreseinnahmen)
    Player,          // spieler
    OfState,         // staat <TAG> { … }: die Kinder für den Staat a (kNone: gibt es nicht, nie wahr)
    AnyState,        // ein_staat { … }: die Kinder für mindestens einen Staat
    Never,           // Verweis auf etwas, das es zum Startdatum nicht gibt (späterer Inhalt): nie wahr
    InRevolt,        // aufstand = ja (seit a3): im Staat läuft ein Aufstand
    Journal,         // journal_<a> <op> Stufe b (seit a4, Eq/Ne)
    Vote,            // abstimmung_<a> = angenommen (b = 1) | abgelehnt (b = 0): letzte Abstimmung (seit a4)
    PopulationShare, // bevoelkerungsanteil { wenn { … } unter { … } mindestens = value } (seit a4)
    CountStates,     // staaten { wenn { … } mindestens = value } (seit a4)
    DietSuspended,   // bundesversammlung = ruht (seit a4)
    WarWon,          // krieg_gewonnen = a (seit b3): ein beendeter Krieg der Hauptparteien gegen a gewonnen
    WarLost,         // krieg_verloren = a (seit b3)
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
    Mobilize,       // mobilmachung = ja (a = 1) | nein (a = 0, seit b3: Abrüstung, nicht im Krieg)
    Follow,         // folge { ereignis = a tage = days staat = b }
    Chronicle,      // chronik = text
    Random,         // zufall { <gewicht> = { … } … }: genau ein Zweig
    OfState,        // staat <TAG> { … }: die Kinder für den Staat a
    AllStates,      // alle_staaten { … }: die Kinder für jeden Staat
    Timed,          // befristet { tage = days text = name … }: Werte in timed (seit a2, Abschnitt 3.7)
    Revolt,         // aufstand = <art> (seit a3): a = RevoltKind
    Intervention,   // intervention = TAG (seit a3): Truppen gegen den Aufstand im Staat a
    Journal,        // journal_<a> = Stufe b (seit a4): das Journal springt in die Stufe
    Vote,           // abstimmung = <a> (seit a4): Abstimmung im Parlament der Abstimmung
    Diet,           // bundesversammlung = ruht (a = 1) | tagt (a = 0) (seit a4)
    Motion,         // antrag { art = a ziel = b } (seit a4): Antrag in der Bundesversammlung (MotionKind)
    War,            // krieg = TAG (seit a4): Kriegserklärung an den Staat a (Militär, mit Bündnisfall);
                    // seit b3 auch krieg { gegner = TAG … } mit Kriegszielen in goals
    // Seit M13a2 (vertical_slice.md 3.3):
    Federation,     // bund { vorlage = a … }: gründet den Bund; b Bits 1 bundesgebiet, 2 bisherige, 4 der
                    // Staat des Ereignisses; name = merkmal; states dazu, excluded ohne
    FederationJoin, // bund_beitritt = ja: der Staat tritt dem Bund zur Laufzeit bei
};

/// Werte einer befristeten Wirkung (Abschnitt 3.7).
struct TimedValues {
    Fixed tax;      // Steuerertrag ±x (Anteil)
    Fixed morale;   // Kampfkraft der Heere über die Moral ±x (Anteil)
    Fixed unrest;   // Unruhe je Monat ±n
    Fixed relation; // Beziehung zu allen Bundesstaaten je Monat ±n

    friend bool operator==(const TimedValues&, const TimedValues&) = default;
};

struct Effect {
    EffectKind kind = EffectKind::Chronicle;
    Index a = -1;
    Index b = -1;
    Fixed value;
    std::int32_t days = 0;
    std::string name;             // Flag, Chroniktext oder Text der Befristung
    std::vector<Effect> children; // OfState, AllStates
    TimedValues timed;            // Timed
    /// Random: je Zweig ein Gewicht (weights[i]) und seine Wirkungen (branches[i]).
    std::vector<Fixed> weights;
    std::vector<std::vector<Effect>> branches;
    std::vector<std::int64_t>
        goals;                   // War (seit b3): Kriegsziele (encode_war_goal), das erste mit der Erklärung
    std::vector<Index> states;   // Federation (seit M13a2): Mitglieder zusätzlich
    std::vector<Index> excluded; // Federation: keine Mitglieder
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
    // KI (seit M12, ki.md Abschnitt 6.4): Bezug der Option zu den sechs Neigungen (risiko … vermittlung),
    // je −2 … +2; das Gewicht eines Staates ohne Spieler wird mit max(0,1; 1 + ¼ × Σ Bezug × Neigung)
    // multipliziert. Ohne Block (has_ai false) bleibt das Gewicht wie in M11.
    bool has_ai = false;
    std::array<Fixed, 6> ai_lean{};
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
    bool follow_only = false;  // nur_folge: tritt nur als Folgeereignis ein (seit a2)
    std::int32_t spacing = 0;  // Mindestabstand in Tagen bei wiederholbaren Ereignissen
    std::int32_t deadline = 0; // Frist für die Wahl des Spielers; 0 = Standard der Regeln
    std::vector<EventOption> options;
    std::string file; // Quelldatei (Meldungen)
    int line = 0;
};

/// Regeln der Ereignisse (data/common/ereignisse.chron).
/// Art eines Aufstands (Abschnitt 4.4): wer sich erhebt.
enum class RevoltKind : std::uint8_t { Barricades, Agrarian, Uprising }; // Städter, Bauern, alle
inline constexpr std::size_t kRevoltKinds = 3;

/// Ausgang eines Aufstands (Abschnitt 4.4).
enum class RevoltOutcome : std::uint8_t { Concession, Suppression, Victory };

struct EventRules {
    bool active = false;        // ab 'seit'; sonst gibt es keine Ereignisse (1648 bitgleich)
    std::int32_t deadline = 30; // Standardfrist der Wahl des Spielers in Tagen

    // Zustimmung und Unruhe (seit a3, Abschnitte 4.2 und 4.3), am Monatsersten; Spielwerte.
    Fixed approval_rate = Fixed::from_ratio(1, 5); // Anteil der Lücke zum Zielwert je Monat
    Fixed unrest_rate = Fixed::from_ratio(1, 4);
    Fixed misery = Fixed::from_int(20);          // je Punkt Not (0 … 2)
    Fixed misery_from = Fixed::from_int(50);     // Lebensstandard der unteren Schicht, ab dem Not beginnt
    Fixed misery_span = Fixed::from_int(25);     // … je Punkt Not
    Fixed dearth = Fixed::from_int(15);          // je Punkt Teuerung (0 … 2)
    Index grain = -1;                            // Gut der Teuerung (Markt der Hauptstadt), −1: keine
    Fixed dearth_from = Fixed::from_ratio(6, 5); // Preisfaktor, ab dem Teuerung beginnt
    Fixed dearth_span = Fixed::from_ratio(1, 2);
    std::array<Fixed, 3> constitution = {Fixed::from_int(15), Fixed::from_int(10), Fixed{}}; // je Verfassung
    Fixed contagion = Fixed::from_int(10); // × Anteil der Nachbarn mit hoher Unruhe
    Fixed contagion_from = Fixed::from_int(60);
    std::string contagion_flag;                   // Welt-Merkmal, das als unruhiger Nachbar zählt
    Fixed approval_weight = Fixed::from_int(2);   // × Σ Einfluss × Zustimmung
    Fixed tax_refusal_from = Fixed::from_int(50); // Unruhe, ab der Steuern verweigert werden
    Fixed tax_refusal = Fixed::from_ratio(1, 10); // … Anteil des Steuerertrags
    Fixed emigration_from = Fixed::from_int(50);  // Unruhe, ab der die untere Schicht mehr auswandert
    Fixed emigration = Fixed::from_ratio(1, 2);   // … zusätzlicher Anteil

    // Aufstände (seit a3, Abschnitt 4.4).
    std::int32_t revolt_min_days = 7;                   // erste Entscheidung nach einer Woche
    std::int32_t revolt_max_days = 60;                  // spätestens dann endet er
    Fixed revolt_step = Fixed::from_ratio(1, 4);        // Stärke je Woche × (Aufständische − Regierung)
    Fixed revolt_noise = Fixed::from_ratio(3, 20);      // Zufall je Woche ±
    Fixed revolt_concession = Fixed::from_ratio(3, 10); // Neigung nachzugeben × Stärke je Woche
    std::vector<Index> split_groups;                    // Elitenspaltung: Gruppen mit Zustimmung < 0 …
    std::string split_flag;                             // … und ein Merkmal am Staat (Märzministerium)
    Fixed split = Fixed::from_ratio(1, 4);              // je Treffer
    Index loyalty_group = -1;                           // Loyalität: Zustimmung dieser Gruppe (Militär)
    Fixed soldiers_reference = Fixed::from_int(10);     // Soldaten je 1.000 Einwohner für Loyalität 1
    Fixed concession_unrest = Fixed::from_int(-30);
    Fixed suppression_unrest = Fixed::from_int(-20);
    Fixed suppression_devastation = Fixed::from_ratio(1, 20); // in der Hauptstadt
    std::string victory_flag = "revolutionsregierung";        // Merkmal nach dem Sieg
    std::array<Index, 3> outcome_events = {-1, -1, -1};       // je RevoltOutcome ein Folgeereignis
};

// ─── Journale und Abstimmungen (Abschnitt 5, seit a4) ───────────────────────

/// Übergang einer Stufe: gilt die Bedingung (Welt), wechselt das Journal in die Stufe target.
struct JournalTransition {
    Index target = -1;
    Condition condition;
};

/// Angezeigtes Ziel einer Stufe: erfüllt, wenn die Bedingung gilt; mit zaehle Fortschritt als Zahl der
/// bestehenden Staaten, für die die Zählbedingung gilt, von soll.
struct JournalGoal {
    std::string text;
    Condition condition;
    bool counting = false;
    Condition count;         // Bedingung je Staat
    std::int32_t target = 0; // soll
};

/// Stufe oder Ausgang eines Journals.
struct JournalStage {
    std::string id;
    std::string title;
    std::string text;
    bool outcome = false;  // Ausgang: Endstufe
    std::string rating;    // Ausgang: gelungen, teilweise, gescheitert
    bool fallback = false; // Ausgang, der am Ende des Journals gilt
    std::vector<JournalGoal> goals;
    std::vector<JournalTransition> transitions;
    std::vector<Effect> effects; // beim Eintritt (Welt)
};

struct JournalDef {
    std::string id;
    std::string title;
    std::string text;
    bool has_begin = false; // ohne beginn: ab dem Start
    Date begin;
    bool has_end = false; // am Ende gilt der Ausgang mit standard = ja
    Date end;
    std::vector<JournalStage> stages; // Stufen und Ausgänge in Dateireihenfolge; die erste Stufe beginnt
    Index fallback = -1;
    std::string file;
    int line = 0;
};

/// Abstimmung in einem Parlament nach Fraktionen (Abschnitt 5.3).
struct VoteDef {
    std::string id;
    std::string title;
    Index parliament = -1;
    std::vector<Fixed> leaning;                // Anteil der Ja-Stimmen je Fraktion des Parlaments
    Fixed unaligned = Fixed::from_ratio(1, 2); // fraktionslose Abgeordnete
    Fixed majority = Fixed::from_ratio(1, 2);  // angenommen mit mehr Ja-Stimmen als majority × abgegebene
    std::vector<Effect> yes;                   // Wirkungen bei Annahme (Welt)
    std::vector<Effect> no;                    // … bei Ablehnung
};

// ─── Zustand ────────────────────────────────────────────────────────────────

/// Stand eines Journals (Zustand, seit a4): Stufe (−1 vor dem Beginn) seit einem Datum.
struct JournalState {
    Index stage = -1;
    Date since;

    friend bool operator==(const JournalState&, const JournalState&) = default;
};

/// Weg eines Journals: jede erreichte Stufe mit Datum (Zustand, seit a4).
struct JournalStep {
    Index journal = -1;
    Index stage = -1;
    Date date;

    friend bool operator==(const JournalStep&, const JournalStep&) = default;
};

/// Ergebnis einer Abstimmung (Zustand, seit a4).
struct VoteResult {
    Index vote = -1;
    Date date;
    std::int32_t yes = 0;
    std::int32_t no = 0;
    bool accepted = false;

    friend bool operator==(const VoteResult&, const VoteResult&) = default;
};

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

/// Befristete Wirkung eines Staates bis zu einem Datum (Zustand, seit a2, Abschnitt 3.7).
struct TimedEffect {
    Index state = -1;
    Index event = -1; // Ereignis, aus dem sie stammt
    Date until;       // gilt bis zum Vortag
    TimedValues values;
    std::string text; // z. B. "Belagerungszustand in Berlin"

    friend bool operator==(const TimedEffect&, const TimedEffect&) = default;
};

/// Laufender Aufstand eines Staates (Zustand, seit a3, Abschnitt 4.4).
struct Revolt {
    Index state = -1;
    RevoltKind kind = RevoltKind::Barricades;
    Date start;
    Fixed strength;    // > 0 für die Aufständischen; ±1 entscheidet
    Index helper = -1; // Staat, der mit Truppen eingreift

    friend bool operator==(const Revolt&, const Revolt&) = default;
};

/// Beendeter Aufstand mit Ausgang (Zustand, seit a5; Labor und Anzeige).
struct RevoltRecord {
    Index state = -1;
    RevoltKind kind = RevoltKind::Barricades;
    Date start;
    Date end;
    RevoltOutcome outcome = RevoltOutcome::Suppression;
    Index helper = -1;

    friend bool operator==(const RevoltRecord&, const RevoltRecord&) = default;
};

/// Merkmal (Flag) eines Staates oder der Welt (state = -1), Zustand.
struct EventFlag {
    Index state = -1;
    std::string name;

    friend bool operator==(const EventFlag&, const EventFlag&) = default;
};

} // namespace uhrwerk
