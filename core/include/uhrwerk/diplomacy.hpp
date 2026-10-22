#pragma once

// Diplomatie (seit M9, Schritt b1): docs/technik/diplomatie.md, Abschnitte 4.1–4.3.
//
// Jedes Staatenpaar hat eine Beziehung von −200 bis +200, die sich monatlich auf einen Zielwert zu
// bewegt (Grundwert aus den Daten, Zollunion, Bund, Konfession, Verträge); Gesandte heben sie darüber
// hinaus. Bündnisse und Garantien nimmt das Ziel nach Regeln an (bis zur KI in M12). Alles nur in
// Welten mit Deutschem Bund: Ohne ihn bleibt der Zustand leer, und 1648 rechnet wie bisher.

#include "uhrwerk/fixed.hpp"
#include "uhrwerk/world.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace uhrwerk {

/// Diplomatie gilt in Welten mit Deutschem Bund (mindestens ein Ort im Bundesgebiet).
bool diplomacy_active(const World& world);

/// Zahl der Staatenpaare und Platz eines Paares (a ≠ b, beide gültig) in World::relations.
std::size_t relation_count(std::size_t states);
std::size_t relation_slot(std::size_t states, Index a, Index b);

/// Beziehung zweier Staaten; 0 ohne Diplomatie, bei ungültigem Index oder a = b.
Fixed relation(const World& world, Index a, Index b);
/// Zielwert der Beziehung (Abschnitt 4.1) ohne Gesandte; 0 ohne Diplomatie.
Fixed relation_target(const World& world, Index a, Index b);
/// Gegner: Beziehung unter rival_below.
bool rivals(const World& world, Index a, Index b);

bool allied(const World& world, Index a, Index b);
/// from garantiert den Bestand von to.
bool guarantees(const World& world, Index from, Index to);
/// from hat einen Gesandten bei to.
bool has_envoy(const World& world, Index from, Index to);

/// Gesandte, die ein Staat entsenden darf: 1 + Einwohner / envoy_people (abgerundet).
std::int64_t envoy_capacity(const World& world, Index state);
std::int64_t envoy_count(const World& world, Index state);
/// Kosten der Gesandten eines Staates im Monat bei diesen Monatseinnahmen.
Fixed envoy_costs(const World& world, Index state, Fixed income);

/// Legt die Beziehungen zum Start auf ihre Zielwerte, wenn Diplomatie gilt und sie noch fehlen
/// (Lader, von Hand gebaute Welt). Ein Spielstand bringt sie selbst mit.
void ensure_diplomacy_state(World& world);
/// Monatliche Drift auf den Zielwert und Wirkung der Gesandten (Abschnitte 4.1 und 4.2).
void update_relations(World& world);

/// Ergebnis eines diplomatischen Befehls: ungültig (ohne Wirkung und Chronik), vom Ziel abgelehnt
/// (Chronik) oder ausgeführt (Chronik).
enum class DiplomacyDecision { Invalid, Rejected, Accepted };

/// Befehl Vertrag (Command::value): Art der Handlung.
enum class TreatyAction : std::int32_t {
    Alliance = 0,
    Guarantee = 1,
    CancelAlliance = 2,
    CancelGuarantee = 3
};

/// Vorab-Prüfung ohne Wirkung (Anzeige) und Ausführung mit Chronik. send: entsenden, sonst abberufen.
DiplomacyDecision evaluate_envoy(const World& world, Index state, Index target, bool send);
DiplomacyDecision request_envoy(World& world, Index state, Index target, bool send);
DiplomacyDecision evaluate_treaty(const World& world, Index state, Index target, TreatyAction action);
DiplomacyDecision request_treaty(World& world, Index state, Index target, TreatyAction action);

/// Bundesstaat: mindestens ein Ort im Bundesgebiet.
bool federal_state(const World& world, Index state);
/// Beziehung einmalig ändern (Bundesintervention, diplomatische Spiele), begrenzt auf −200..200.
void adjust_relation(World& world, Index a, Index b, Fixed delta);

// ─── Bundesversammlung (seit Schritt b2, Abschnitt 4.4) ─────────────────────

/// Es gibt eine Bundesversammlung, und Diplomatie gilt.
bool has_diet(const World& world);
std::int32_t plenum_votes(const World& world, Index state);
/// Stärkster Gegner eines Antrags: Bundesreform die Präsidialmacht, Zolleinigung der
/// bevölkerungsreichste Bundesstaat außerhalb der Union, der keiner anderen Union bloß als Mitglied
/// angehört (eigenes Zollgebiet oder Leitmacht), Intervention ihr Ziel, Präsidium die Präsidialmacht.
/// Ist der Antragsteller selbst dieser Staat, der bevölkerungsreichste übrige Bundesstaat.
Index motion_opponent(const World& world, MotionKind kind, Index applicant, Index target);
/// Stimme eines Staates nach Regel: Ja, wenn seine Beziehung zum Antragsteller höher ist als zum Gegner.
/// Der Antragsteller stimmt mit Ja, der Gegner mit Nein.
bool votes_yes(const World& world, Index state, Index applicant, Index opponent);
/// Antrag stellen (Accepted) oder ungültig: ohne Bundesversammlung, ohne Plenarstimme, wenn schon ein
/// Antrag offen ist oder das Ziel nicht passt.
DiplomacyDecision evaluate_motion(const World& world, Index applicant, MotionKind kind, Index target);
DiplomacyDecision file_motion(World& world, Index applicant, MotionKind kind, Index target);
/// Abstimmung ohne Wirkung: Stimmen in Plenum und Engerem Rat und Mehrheit nach Art des Antrags.
MotionResult tally_motion(const World& world, const Motion& motion);
/// Monatserster: den offenen Antrag abstimmen (nicht am Tag seiner Stellung), Wirkung und Chronik.
void run_diet(World& world);

// ─── Diplomatische Spiele (seit Schritt b2, Abschnitt 4.5) ──────────────────

/// Befehl Manöver (Command::value).
enum class Manoeuvre : std::int32_t { Yield = 0, Mediate = 1, Escalate = 2 };

/// Index des laufenden Spiels, in dem der Staat Hauptpartei ist, sonst kNone.
Index play_of(const World& world, Index state);
/// Soldaten eines Staates (Heer je 1.000 Einwohner × Einwohner).
Fixed state_strength(const World& world, Index state);
/// Stärke einer Seite: Hauptpartei und Unterstützer.
Fixed side_strength(const World& world, const DiplomaticPlay& play, bool demander_side);
/// Parteinahme der übrigen Bundesstaaten: Bündnis, dann Garantie, sonst Neigung über play_leaning.
void play_sides(const World& world, Index demander, Index target, std::vector<Index>& demander_side,
                std::vector<Index>& target_side);
/// Forderung stellen (Accepted, das Spiel beginnt) oder ungültig.
DiplomacyDecision evaluate_demand(const World& world, Index demander, Index target, DemandKind kind);
DiplomacyDecision start_play(World& world, Index demander, Index target, DemandKind kind);
/// Manöver einer Hauptpartei: Nachgeben (sofort), Vermittlung (angenommen, wenn die eigene Seite
/// mindestens play_yield der anderen stark ist, sonst Rejected), Eskalieren (für die laufende Stufe).
DiplomacyDecision evaluate_manoeuvre(const World& world, Index state, Manoeuvre manoeuvre);
DiplomacyDecision apply_manoeuvre(World& world, Index state, Manoeuvre manoeuvre);
/// Täglich: Ende einer Stufe mit Entscheidung der Seiten, Eskalation und Auflösung.
void run_plays(World& world);
/// Hauptpartei eines Spiels auf der Stufe Mobilmachung (Heereskosten × mobilization).
bool mobilized(const World& world, Index state);

} // namespace uhrwerk
