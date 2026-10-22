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

} // namespace uhrwerk
