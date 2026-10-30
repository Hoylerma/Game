#pragma once

// KI der Staaten (seit M12, docs/technik/ki.md). Jeder Staat ohne Spieler erhält Neigungen aus seiner
// Staatspersönlichkeit und den Eigenschaften der Personen im Amt (Abschnitt 3) und verfolgt die Ziele
// seiner Agenda. Ämter wechseln am Monatsersten nach Datum und Bedingung; ein Wechsel schreibt die Chronik.
// Alles nur in Welten mit KI-Daten (World::ai_rules.active), damit 1648 bitgleich bleibt.

#include "uhrwerk/ai_types.hpp"
#include "uhrwerk/events.hpp"
#include "uhrwerk/rng.hpp"
#include "uhrwerk/simulation.hpp"
#include "uhrwerk/world.hpp"

#include <string>
#include <vector>

namespace uhrwerk {

bool ai_active(const World& world);

/// Legt die Ämter an, wenn ihre Größe nicht zur Welt passt, und besetzt sie ohne Eintrag in der Chronik
/// (Start einer Partie).
void ensure_ai_state(World& world, const EventCache& cache);

/// Person, die das Amt zum Datum innehätte (Zeitraum und Bedingung; von mehreren die zuletzt beginnende),
/// oder kNone.
Index person_for_office(const World& world, const EventCache& cache, Index state, PersonRole role);

/// Besetzt die Ämter neu; jeder Wechsel schreibt die Chronik („Preußen: Otto von Bismarck wird
/// Ministerpräsident“).
void update_offices(World& world, const EventCache& cache);

/// Gewicht des Herrschers nach der Verfassung des Staates (Abschnitt 3.3); der Minister erhält den Rest.
Fixed ruler_weight(const World& world, Index state);

/// Neigungen eines Staates (Abschnitt 3.1): Staatspersönlichkeit + Σ Gewicht × Eigenschaften der Personen
/// im Amt, je auf −2 … +2 begrenzt.
Inclinations state_inclinations(const World& world, Index state);

/// Ist ein Ziel der Agenda aktiv (Zeitfenster und Bedingung im Staat)?
bool agenda_goal_active(const World& world, const EventCache& cache, Index state, const AgendaGoal& goal);

/// Stärke eines Staates im Lagebild (Abschnitt 4.3): Soldaten unter Waffen, mindestens die Friedensstärke ×
/// Faktor der Mobilmachung; 0 ohne Militär.
Fixed ai_strength(const World& world, Index state);

/// Bedrohung des Staates state durch other (Abschnitt 4.3): Stärke von other ÷ eigene Stärke × Feindschaft
/// × Nähe; 0 ohne Militär oder für sich selbst.
Fixed ai_threat(const World& world, const EventCache& cache, Index state, Index other);

/// Text eines Grundes, z. B. „Defizit +34“ oder „Bedrohung durch Österreich +12“.
std::string ai_reason_text(const World& world, const AiReason& reason);

/// Text einer Entscheidung, z. B. „erhöht die Akzise auf 7 %“.
std::string ai_decision_text(const World& world, const AiDecision& decision);

/// Der Tag der KI (Abschnitt 4.2): am Monatsersten Ämter, Strategie und Planer (Haushalt, Mobilmachung)
/// für jeden bestehenden Staat außer dem Spielerstaat. Liefert die Befehle der KI; die Simulation reiht sie
/// ohne Protokoll ein, sie wirken am nächsten Tag.
std::vector<Command> run_ai(World& world, Pcg32& rng, EventCache& cache);

} // namespace uhrwerk
