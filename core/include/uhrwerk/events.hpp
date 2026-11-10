#pragma once

// Ereignisse (seit M11, docs/technik/ereignisse.md Abschnitt 3): Auslösung, Wahl, Wirkungen.
//
// Täglich nach dem Militär prüft der Kern die Ereignisse in der Reihenfolge ihrer Kennung, je Staat in
// der Reihenfolge der Staaten. Gilt die Bedingung, tritt ein Ereignis mit der Wahrscheinlichkeit
// 1 / wartezeit ein (Strom Ereignisse), an einem Termin genau an diesem Tag. Staaten ohne Spieler wählen
// eine Option nach Gewicht; Ereignisse des Spielerstaates (World::event_player) warten auf den Befehl
// Ereigniswahl, ohne Wahl gilt nach der Frist die Option mit dem größten Gewicht. Alles nur in Welten
// mit Ereignissen (World::event_rules.active), damit 1648 bitgleich bleibt.

#include "uhrwerk/fixed.hpp"
#include "uhrwerk/rng.hpp"
#include "uhrwerk/world.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace uhrwerk {

/// Abgeleitete Hilfsdaten der Ereignisse (kein Zustand): letzter Eintritt je Ereignis und Staat,
/// Nachbarstaaten, bestehende Staaten. Aus dem Zustand neu aufgebaut (rebuild_event_cache).
struct EventCache {
    std::size_t columns = 0;                   // Staaten + 1 (Welt)
    std::vector<std::int32_t> last;            // [ereignis × columns + staat]: Index in event_log, −1: nie
    std::vector<std::vector<Index>> neighbors; // je Staat: Nachbarstaaten (über die Nachbarschaft der Orte)
    std::vector<std::uint8_t> alive;           // je Staat: besitzt Orte mit Bevölkerung
};

bool events_active(const World& world);

/// Legt Zustimmung und Unruhe an, wenn ihre Größen nicht zur Welt passen (Start, Lasttest-Welt).
void ensure_events_state(World& world);

void rebuild_event_cache(const World& world, EventCache& cache);

/// Gilt die Bedingung für den Staat (kNone: Welt)?
bool condition_holds(const World& world, const EventCache& cache, const Condition& condition, Index state);

/// Gewicht je Option mit den Zuschlägen (mindestens 0); −1 für Optionen, deren Bedingung nicht gilt.
std::vector<Fixed> option_weights(const World& world, const EventCache& cache, const EventDef& event,
                                  Index state);

/// Option, die ohne Wahl gilt: verfügbar mit dem größten Gewicht (bei Gleichstand die erste); ohne
/// verfügbare Option die erste.
Index default_option(const World& world, const EventCache& cache, const EventDef& event, Index state);

/// Der Tag der Ereignisse (Abschnitte 3.4–3.7): befristete Wirkungen, fällige Fristen, Folgeereignisse,
/// Auslösung. Ohne Ereignisse nichts.
void run_events(World& world, Pcg32& rng, EventCache& cache);

/// Befehl Ereigniswahl: wählt die Option eines offenen Ereignisses des Staates; false, wenn es kein
/// solches Ereignis gibt oder die Option nicht verfügbar ist.
bool choose_event_option(World& world, Pcg32& rng, EventCache& cache, Index state, Index event, Index option);

/// Befehl Ereignisspieler: Staat, dessen Ereignisse auf die Wahl warten (kNone: keiner). Offene
/// Ereignisse eines früheren Spielers bleiben offen und gelten nach ihrer Frist.
bool set_event_player(World& world, Index state);

/// Zielwert der Unruhe eines Staates (Abschnitt 4.3): Not, Teuerung, Verfassung, Gesetze, Ansteckung,
/// Zustimmung; 0 … 100.
Fixed unrest_target(const World& world, const EventCache& cache, Index state);

/// Bestandteile des Zielwerts der Unruhe (seit M13a5c, Tooltip „Unruhe“): Not der Unterschicht, Teuerung des
/// Getreides, Verfassung, Zensur der Gesetze, Ansteckung durch Nachbarn und Zustimmung (negativ, wenn sie
/// beruhigt). unrest_target ist ihre Summe, begrenzt auf 0 … 100.
struct UnrestCauses {
    Fixed misery;
    Fixed dearth;
    Fixed constitution;
    Fixed censorship;
    Fixed contagion;
    Fixed approval;
};
UnrestCauses unrest_causes(const World& world, const EventCache& cache, Index state);

/// Faktor auf den Steuerertrag aus befristeten Wirkungen und Steuerverweigerung bei hoher Unruhe.
Fixed event_tax_factor(const World& world, Index state);
/// Faktor auf die Auswanderung der unteren Schicht bei hoher Unruhe (1 sonst).
Fixed unrest_emigration_factor(const World& world, Index state);

/// Faktor auf den Steuerertrag und auf die Kampfkraft der Heere aus befristeten Wirkungen (1 ohne).
Fixed timed_tax_factor(const World& world, Index state);
Fixed timed_morale_factor(const World& world, Index state);

/// Folgen von Wirkungen als Text (seit b4, Ereignisfenster und Kompass, ereignisse.md Abschnitt 9): je
/// Wirkung eine Zeile („Zustimmung Bildungsbürgertum +3“, „Unruhe −20“, „Krieg gegen Dänemark“).
/// Merkmale und Chronikeinträge sind Buchführung und erscheinen nicht; Wirkungen auf Staaten ohne Orte
/// zum Start sind beim Laden schon entfallen.
std::vector<std::string> describe_effects(const World& world, const std::vector<Effect>& effects);

/// Stand eines Ziels einer Journalstufe (seit b4): erfüllt, und mit zaehle der Fortschritt als Zahl
/// der bestehenden Staaten, für die die Zählbedingung gilt (von goal.target).
struct JournalGoalStatus {
    bool done = false;
    std::int32_t count = 0;
};
JournalGoalStatus journal_goal_status(const World& world, const EventCache& cache, const JournalGoal& goal);

} // namespace uhrwerk
