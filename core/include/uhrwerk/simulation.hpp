#pragma once

// Die Simulation: Tick-Planer, Systeme, Befehle und Prüfsumme.
//
// Zeitmodell (docs/02_engine_und_technik.md, Abschnitt 5.2):
//   1 Tick = 1 Tag. Monatliche Systeme sind gestaffelt: Ort i rechnet am
//   Monatstag (i % 28) + 1, Staat j am Monatstag (j % 28) + 1. Jährliche
//   Ereignisse laufen am 1. Januar.
//
// Ablauf eines Tages (step_day), in fester Reihenfolge (docs/technik/bevoelkerung.md, Abschnitt 3):
//   1. Befehle der Warteschlange ausführen (in Eingangsreihenfolge)
//   2. Am 1. Januar: Jahresabschluss (Bilanz des Jahres wird Bilanz des Vorjahres)
//   3. Am Monatsersten: Neuordnung der Blöcke (Zugänge, zu kleine Blöcke), wenn nötig
//   4. Bevölkerung der Orte, die heute ihren Rechentag haben – parallel
//   5. Zusammenführen, nacheinander: Momentaufnahmen veröffentlichen, Wanderung buchen
//   6. Wirtschaft der Staaten, die heute ihren Rechentag haben
//   7. Am 1. Januar: Seuchen und Abbau der Verwüstung (Orte ohne Blöcke werden übersprungen)
//   8. Datum um einen Tag weiterschalten
//
// Mehrkernberechnung (Abschnitt 5): Jede Ortsrechnung in Schritt 4 schreibt nur Daten ihres Ortes
// und liest Daten anderer Orte nur aus den veröffentlichten Momentaufnahmen. Das Ergebnis hängt
// daher nicht von der Zahl der Threads ab.

#include "uhrwerk/economy.hpp"
#include "uhrwerk/parallel.hpp"
#include "uhrwerk/population.hpp"
#include "uhrwerk/rng.hpp"
#include "uhrwerk/world.hpp"

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <utility>
#include <vector>

namespace uhrwerk {

struct SimConfig {
    std::uint64_t seed = 1648;
    /// Rechnende Threads; 0 = alle Kerne. Kein Teil des Zustands: Das Ergebnis ist bei jeder
    /// Zahl gleich.
    unsigned threads = 0;
};

enum class CommandType : std::uint8_t {
    SetTaxRate, // state, kind, value = neuer Steuersatz (0..rules.tax_rate_max)
};

struct Command {
    CommandType type = CommandType::SetTaxRate;
    Index state = kNone;
    Fixed value;
    TaxKind kind = TaxKind::Kontribution; // SetTaxRate: welche Steuer (seit M5)
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
    /// Zahl der rechnenden Threads (aufgelöst, mindestens 1).
    unsigned threads() const { return pool_.threads(); }

    /// Produktion und Steuern eines Ortes je Monat, festgestellt an seinem letzten Rechentag
    /// (beim Start aus dem Anfangszustand). Ändert nichts.
    LocationEconomy location_economy(Index location) const;

    /// Wanderungsziele eines Ortes (Abschnitt 4.5), nach Abstand aufsteigend; leer für Orte ohne
    /// Daten.
    const std::vector<Index>& migration_targets(Index location) const;

    /// FNV-1a-64 über den gesamten veränderlichen Zustand (Datum, Bevölkerung mit allen Feldern,
    /// Zugänge, Momentaufnahmen, Bilanzen, Ortssteuern, Kassen, Steuersätze, Verwüstung,
    /// Zufallsströme, Chronik, wartende Befehle).
    ///
    /// Orte ohne Daten (seit M3) sind über Besitzer (kNone) und Blockzahl (0) erfasst. Die
    /// Kreiszugehörigkeit ist wie Gelände, Lage und Name unveränderlicher Inhalt und gehört
    /// nicht dazu; ebenso nicht die Zahl der Threads und abgeleitete Hilfsdaten (Rechentag-Listen,
    /// Wanderungsziele).
    std::uint64_t checksum() const;

private:
    /// Ein Wanderungsstrom aus dem Ausgang eines Ortes (Abschnitt 4.5).
    struct Migration {
        Index target = kNone;
        Index profession = kNone; // Beruf am Ziel
        Index confession = kNone;
        Index culture = kNone;
        Fixed amount;
        Fixed living_standard;
        Fixed literacy;
    };

    /// Arbeitsspeicher einer parallelen Aufgabe (wird je Paket wiederverwendet).
    struct Scratch {
        LocationFactors factors;
        EconomyScratch economy;
        std::vector<Fixed> attraction; // je Beruf: Anziehung des eigenen Ortes
        std::vector<Fixed> hunger;     // je Beruf: Faktor auf die Sterblichkeit (wirtschaft.md 4.4)
    };

    /// Thread-Pool, der beim Kopieren einer Simulation neu angelegt wird (Threads sind kein Zustand).
    class Pool {
    public:
        explicit Pool(unsigned threads) : pool_(std::make_unique<ThreadPool>(threads)) {}
        Pool(const Pool& other) : Pool(other.threads()) {}
        Pool& operator=(const Pool& other) {
            if (this != &other) {
                pool_ = std::make_unique<ThreadPool>(other.threads());
            }
            return *this;
        }
        Pool(Pool&&) noexcept = default;
        Pool& operator=(Pool&&) noexcept = default;
        ~Pool() = default;

        unsigned threads() const { return pool_->threads(); }
        ThreadPool& get() { return *pool_; }

    private:
        std::unique_ptr<ThreadPool> pool_;
    };

    void execute(const Command& command);
    void run_population(Index location, Scratch& scratch);
    void publish_snapshot(Index location);
    void book_migration(const Migration& migration);
    void run_economy(Index state);
    void run_yearly();

    /// Hilfslisten aus dem unveränderlichen Inhalt und den Besitzverhältnissen (seit M4).
    /// Kein Zustand: nicht in der Prüfsumme, im Konstruktor aus der Welt aufgebaut.
    void build_schedule();
    void build_migration_targets();
    /// Füllt Momentaufnahmen, Bilanzen und Ortssteuern, wenn sie nicht zur Welt passen (Start
    /// aus dem Lader).
    void prepare_population_state();

    World world_;
    SimConfig config_;
    PopulationRates rates_;
    std::deque<Command> pending_;
    Pcg32 rng_demography_;
    Pcg32 rng_economy_;
    Pcg32 rng_events_;
    std::int64_t days_elapsed_ = 0;
    EconomyTopology topology_; // abgeleitet: Märkte, Nachbarn, Betriebe je Beruf (seit M5)

    std::array<std::vector<Index>, 28> locations_by_day_; // Orte mit Blöcken je Rechentag, aufsteigend
    std::vector<std::vector<Index>> state_locations_;     // Orte je Staat, aufsteigend
    std::vector<std::vector<Index>> migration_targets_;   // je Ort, nach Abstand aufsteigend

    // Ergebnisse der parallelen Phase, die erst beim Zusammenführen sichtbar werden (kein
    // Zustand zwischen zwei Tagen).
    std::vector<std::vector<Migration>> outbox_;
    std::vector<std::vector<std::pair<Index, std::uint32_t>>> inbox_; // je Zielort: (Herkunft, Eintrag)
    std::vector<Index> touched_;                                      // Zielorte mit Zugängen heute
    std::vector<LocationSnapshot> next_snapshots_;
    std::vector<Fixed> next_targets_;
    Pool pool_;
};

} // namespace uhrwerk
