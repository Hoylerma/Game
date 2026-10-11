#pragma once

// Die Simulation: Tick-Planer, Systeme, Befehle und Prüfsumme.
//
// Zeitmodell (docs/02_engine_und_technik.md, Abschnitt 5.2):
//   1 Tick = 1 Tag. Monatliche Systeme sind gestaffelt: Ort i rechnet am
//   Monatstag (i % 28) + 1, Staat j am Monatstag (j % 28) + 1. Jährliche
//   Ereignisse laufen am 1. Januar.
//
// Ablauf eines Tages (step_day), in fester Reihenfolge:
//   1. Befehle der Warteschlange ausführen (in Eingangsreihenfolge)
//   2. Demografie der Orte, die heute ihren Rechentag haben
//   3. Wirtschaft der Staaten, die heute ihren Rechentag haben
//   4. Am 1. Januar: Jahresereignisse (Seuchen) und Abbau der Verwüstung
//      (Orte ohne Bevölkerungsblöcke werden dabei übersprungen)
//   5. Datum um einen Tag weiterschalten

#include "uhrwerk/rng.hpp"
#include "uhrwerk/world.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <vector>

namespace uhrwerk {

struct SimConfig {
    std::uint64_t seed = 1648;
};

enum class CommandType : std::uint8_t {
    SetTaxRate, // state, value = neuer Steuersatz (0..rules.tax_rate_max)
};

struct Command {
    CommandType type = CommandType::SetTaxRate;
    Index state = kNone;
    Fixed value;
};

/// Monatliche Kennzahlen eines Ortes (für Anzeige und Tests).
struct LocationEconomy {
    Fixed production; // Reichstaler pro Monat
    Fixed taxes;      // davon an den Staat
};

class Simulation {
public:
    explicit Simulation(World world, SimConfig config = {});

    /// Befehl für den nächsten Tag einreihen. Ungültige Befehle werden bei
    /// der Ausführung verworfen und in der Chronik nicht vermerkt.
    void submit(const Command& command);

    void step_day();
    void step_days(std::int64_t days);

    const World& world() const { return world_; }
    const SimConfig& config() const { return config_; }
    std::int64_t days_elapsed() const { return days_elapsed_; }

    /// Wirtschaftskennzahlen eines Ortes nach aktuellem Stand (rein berechnet, ändert nichts).
    LocationEconomy location_economy(Index location) const;

    /// FNV-1a-64 über den gesamten veränderlichen Zustand (Datum, Bevölkerung,
    /// Kassen, Steuersätze, Verwüstung, Zufallsströme, Chronik).
    ///
    /// Orte ohne Daten (seit M3) sind über Besitzer (kNone) und Blockzahl (0) erfasst. Die
    /// Kreiszugehörigkeit ist wie Gelände, Lage und Name unveränderlicher Inhalt und gehört
    /// nicht dazu; so ändern neue Kreise in den Daten die Referenzprüfsumme nicht.
    std::uint64_t checksum() const;

private:
    void execute(const Command& command);
    void run_demography(Index location);
    void run_economy(Index state);
    void run_yearly();

    /// Hilfslisten aus dem unveränderlichen Inhalt und den Besitzverhältnissen (seit M4).
    /// Kein Zustand: nicht in der Prüfsumme, im Konstruktor aus der Welt aufgebaut.
    void build_schedule();

    World world_;
    SimConfig config_;
    std::array<std::vector<Index>, 28> locations_by_day_; // Orte mit Blöcken je Rechentag, aufsteigend
    std::vector<std::vector<Index>> state_locations_;     // Orte je Staat, aufsteigend
    std::deque<Command> pending_;
    Pcg32 rng_demography_;
    Pcg32 rng_economy_;
    Pcg32 rng_events_;
    std::int64_t days_elapsed_ = 0;
};

} // namespace uhrwerk
