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

/// Stufe der Partie (Spieleinstellung, sonst standard_stufe) oder -1 ohne Stufen (seit M13a1).
Index ai_difficulty(const World& world);

/// Regeln der Planer mit den Faktoren der Stufe und dem Geschichtsverlauf der Partie (seit M13a1,
/// vertical_slice.md 3.2): schwelle, rauschen und Abkühlungen × Faktor, caution und player_rival aus der
/// Stufe, open_agenda auch bei offenem Verlauf.
AiRules effective_ai_rules(const World& world);

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

/// Annahme eines Bündnisangebots durch einen Staat ohne Spieler (Abschnitt 6.2), ohne Rauschen:
/// (Beziehung − 50) ÷ 5, gemeinsamer Rivale +20, der Anbieter ist der eigene Rivale −30; angenommen ab der
/// Schwelle, wenn keiner der beiden schon 'buendnisse' Bündnisse hat. Ersetzt die Regel „ab Beziehung
/// bündnis_ab“, wenn die KI rechnet.
bool ai_accepts_alliance(const World& world, Index acceptor, Index proposer);

/// Annahme eines weißen Friedens durch die Hauptpartei einer Seite ohne Spieler (Abschnitt 6.3): abgelehnt,
/// wenn die Gegenseite kriegsmüder ist oder die eigene Seite um kriegs_staerke überlegen. Gilt
/// zusätzlich zur Regel aus militaer.md 4.5, wenn die KI rechnet.
bool ai_accepts_white_peace(const World& world, const War& war, bool attacker_side);

/// Faktor der KI auf das Gewicht einer Ereignisoption (Abschnitt 6.4): max(0,1; 1 + ¼ × Σ Bezug × Neigung)
/// für Staaten ohne Spieler mit Bezug; sonst 1.
Fixed ai_option_factor(const World& world, const EventOption& option, Index state);

/// Text eines Grundes, z. B. „Defizit +34“ oder „Bedrohung durch Österreich +12“.
std::string ai_reason_text(const World& world, const AiReason& reason);

/// Text einer Entscheidung, z. B. „erhöht die Akzise auf 7 %“.
std::string ai_decision_text(const World& world, const AiDecision& decision);

/// Der Tag der KI (Abschnitt 4.2): am Monatsersten Ämter, Strategie und Planer (Haushalt, Mobilmachung,
/// Diplomatie) für jeden bestehenden Staat außer dem Spielerstaat, danach der Wahlkampf der Strömungen
/// außer der des Spielers. Liefert die Befehle der KI; die Simulation reiht sie
/// ohne Protokoll ein, sie wirken am nächsten Tag.
std::vector<Command> run_ai(World& world, Pcg32& rng, EventCache& cache);

} // namespace uhrwerk
