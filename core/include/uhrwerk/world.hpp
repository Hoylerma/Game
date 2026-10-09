#pragma once

// Weltmodell des Simulationskerns (Meilenstein M1: technischer Durchstich).
//
// Daten sind als flache Vektoren mit Indizes organisiert (datenorientiert);
// Querverweise sind Indizes, keine Zeiger. Die Reihenfolge aller Vektoren ist
// durch die Ladereihenfolge festgelegt und damit deterministisch.

#include "uhrwerk/date.hpp"
#include "uhrwerk/fixed.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace uhrwerk {

using Index = std::int32_t;
inline constexpr Index kNone = -1;

struct Color {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    friend constexpr bool operator==(const Color&, const Color&) = default;
};

/// Geländetyp (data/common/gelaende.chron).
struct TerrainType {
    std::string id;
    std::string name;
    Color color;
    Fixed growth_factor = Fixed::one(); // Faktor auf Bevölkerungswachstum
    Fixed yield_factor = Fixed::one();  // Faktor auf Produktion
};

/// Konfession (data/common/konfessionen.chron).
struct Confession {
    std::string id;
    std::string name;
    Color color;
};

enum class Stratum : std::uint8_t { Unten, Mittel, Oben };

/// Berufsgruppe (data/common/berufe.chron).
struct Profession {
    std::string id;
    std::string name;
    Stratum stratum = Stratum::Unten;
    Fixed yield_per_capita; // Reichstaler pro Kopf und Jahr
    bool taxable = true;    // privilegierte Stände (Adel, Klerus) zahlen keine Kontribution
    Fixed share_city;       // Anteil in Städten (0..1)
    Fixed share_rural;      // Anteil auf dem Land (0..1)
};

/// Bevölkerungsblock: gleichartige Menschen eines Ortes.
struct PopBlock {
    Index location = kNone;
    Index profession = kNone;
    Index confession = kNone;
    Fixed size; // Personen (Festkomma, damit langsames Wachstum nicht verloren geht)
};

/// Ort – kleinste Karteneinheit (data/history/orte/*.chron).
struct Location {
    std::string id;
    std::string name;
    Index owner = kNone;
    Index terrain = kNone;
    Fixed longitude; // Grad Ost (WGS84)
    Fixed latitude;  // Grad Nord (WGS84)
    Fixed area_km2;
    bool is_city = false;
    Fixed devastation;   // Kriegsverwüstung 0..1
    std::string source;  // Quellenangabe oder "Testdaten"
    Index first_pop = 0; // Bevölkerungsblöcke liegen zusammenhängend: [first_pop, first_pop + pop_count)
    Index pop_count = 0;
};

/// Staat bzw. Reichsstand (data/history/staaten/*.chron).
struct State {
    std::string tag; // z. B. "BRA"
    std::string name;
    std::string short_name;
    Color color;
    Index capital = kNone;
    std::string government; // z. B. "absolute_monarchie" (in M1 nur Anzeige)
    std::string ruler;
    Index confession = kNone; // Konfession des Herrscherhauses
    Fixed treasury;           // Staatskasse in Reichstalern
    Fixed tax_rate;           // Kontribution als Anteil der steuerpflichtigen Produktion (0..1)
    Fixed court_expenses;     // Hofhaltung und Verwaltung pro Jahr
    Fixed last_month_income;
    Fixed last_month_expenses;
    std::string source;
};

/// Globale Regeln und Balancing-Werte (data/common/regeln.chron).
struct Rules {
    Fixed growth_base;            // jährliches Grundwachstum, z. B. 0.004
    Fixed growth_reconstruction;  // zusätzliches Wachstum je Verwüstungsgrad (Wiederaufbau, Peuplierung)
    Fixed growth_tax_pressure;    // Wachstumsabzug je Steuersatz
    Fixed devastation_decay;      // jährlicher relativer Abbau der Verwüstung, z. B. 0.03
    Fixed devastation_yield_loss; // Produktionsverlust je Verwüstungsgrad, z. B. 0.5
    Fixed plague_chance;          // jährliche Seuchenwahrscheinlichkeit je Ort
    Fixed plague_loss_min;        // Bevölkerungsverlust bei Seuche, Untergrenze
    Fixed plague_loss_max;        // … Obergrenze
    Fixed tax_rate_max;           // höchster erlaubter Steuersatz
};

/// Szenario/Lesezeichen (data/szenarien/*.chron).
struct Scenario {
    std::string id;
    std::string name;
    Date start;
    std::string player_tag;
};

/// Eintrag der Chronik (Geschichtsbuch der Partie).
struct ChronicleEntry {
    Date date;
    std::string text;
    Index location = kNone;
    Index state = kNone;
};

struct World {
    Scenario scenario;
    Date date;
    Rules rules;

    std::vector<TerrainType> terrains;
    std::vector<Confession> confessions;
    std::vector<Profession> professions;
    std::vector<State> states;
    std::vector<Location> locations;
    std::vector<PopBlock> pops;
    std::vector<ChronicleEntry> chronicle;

    Index find_state(std::string_view tag) const;
    Index find_location(std::string_view id) const;
    Index find_terrain(std::string_view id) const;
    Index find_confession(std::string_view id) const;
    Index find_profession(std::string_view id) const;

    Fixed location_population(Index location) const;
    Fixed state_population(Index state) const;
    /// Indizes aller Orte eines Staates, aufsteigend.
    std::vector<Index> state_locations(Index state) const;
    /// Konfession mit den meisten Angehörigen im Ort (bei Gleichstand der kleinere Index).
    Index location_majority_confession(Index location) const;
};

} // namespace uhrwerk
