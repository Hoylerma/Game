#pragma once

// Weltmodell des Simulationskerns (seit M1; Bevölkerung seit M4, docs/technik/bevoelkerung.md).
//
// Daten sind als flache Vektoren mit Indizes organisiert (datenorientiert);
// Querverweise sind Indizes, keine Zeiger. Die Reihenfolge aller Vektoren ist
// durch die Ladereihenfolge festgelegt und damit deterministisch.

#include "uhrwerk/date.hpp"
#include "uhrwerk/fixed.hpp"

#include <cstddef>
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
    Fixed yield_factor = Fixed::one(); // Faktor auf Produktion
    Fixed capacity;                    // Tragfähigkeit: Einwohner je km² (seit M4)
};

/// Konfession (data/common/konfessionen.chron).
struct Confession {
    std::string id;
    std::string name;
    Color color;
    Fixed literacy_factor = Fixed::one(); // Faktor auf die Alphabetisierung (seit M4)
};

/// Kultur bzw. Landsmannschaft (data/common/kulturen.chron, seit M4).
struct Culture {
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
    Fixed yield_per_capita;        // Reichstaler pro Kopf und Jahr
    bool taxable = true;           // privilegierte Stände (Adel, Klerus) zahlen keine Kontribution
    Fixed share_city;              // Anteil in Städten (0..1)
    Fixed share_rural;             // Anteil auf dem Land (0..1)
    bool agrarian = false;         // Ertrag hängt am Boden (Tragfähigkeit), seit M4
    Fixed literacy;                // Alphabetisierung, Grundwert 0..1 (seit M4)
    Index city_profession = kNone; // Beruf nach dem Zuzug in eine Stadt, kNone = derselbe (seit M4)
};

/// Bevölkerungsblock: gleichartige Menschen eines Ortes (seit M4 mit Altersaufbau).
///
/// Ein Block ist durch (Ort, Beruf, Konfession, Kultur) bestimmt; in einem Ort gibt es jeden
/// Schlüssel höchstens einmal. Personenzahlen sind Festkomma, damit langsames Wachstum nicht
/// verloren geht. 64 Byte, eine Cache-Zeile.
struct PopBlock {
    Index location = kNone;
    Index profession = kNone;
    Index confession = kNone;
    Index culture = kNone;
    Fixed children;        // 0–14 Jahre
    Fixed young;           // 15–29
    Fixed working;         // 30–64
    Fixed old;             // 65+
    Fixed living_standard; // Lebensstandard 0..100
    Fixed literacy;        // Alphabetisierung 0..1

    /// Personen insgesamt (Summe der Altersgruppen).
    Fixed size() const { return children + young + working + old; }
};
static_assert(sizeof(PopBlock) == 64, "ein Bevölkerungsblock soll genau eine Cache-Zeile füllen");

/// Ort – kleinste Karteneinheit (data/history/orte/*.chron).
///
/// Orte ohne Daten (seit M3, chronik-skript.md Abschnitt 3.6) erscheinen nur auf der Karte:
/// Besitzer kNone, keine Bevölkerungsblöcke, Verwüstung 0. Sie nehmen an der Simulation nicht teil.
struct Location {
    std::string id;
    std::string name;
    Index owner = kNone; // kNone bei Orten ohne Daten
    Index terrain = kNone;
    Index district = kNone; // Kreis (seit M3), kNone ohne Kreis; unveränderlich
    Fixed longitude;        // Grad Ost (WGS84)
    Fixed latitude;         // Grad Nord (WGS84)
    Fixed area_km2;
    bool is_city = false;
    Fixed devastation;   // Kriegsverwüstung 0..1
    std::string source;  // Quellenangabe oder "Testdaten"
    Index first_pop = 0; // Bevölkerungsblöcke liegen zusammenhängend: [first_pop, first_pop + pop_count)
    Index pop_count = 0;

    /// Ort mit Simulationsdaten? Orte ohne Daten haben keine Bevölkerungsblöcke.
    bool has_data() const { return pop_count > 0; }
};

/// Kreis (data/history/kreise/*.chron, seit M3): Gruppe von 4–12 benachbarten Orten für die
/// Kartendarstellung, später für Verwaltung und Wahlkreise. In M3 nicht datiert.
struct District {
    std::string id; // z. B. "kr_berlin"
    std::string name;
    std::string source;
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
    Fixed devastation_decay;      // jährlicher relativer Abbau der Verwüstung, z. B. 0.03
    Fixed devastation_yield_loss; // Produktionsverlust je Verwüstungsgrad, z. B. 0.5
    Fixed plague_chance;          // jährliche Seuchenwahrscheinlichkeit je Ort
    Fixed plague_loss_min;        // Bevölkerungsverlust bei Seuche, Untergrenze
    Fixed plague_loss_max;        // … Obergrenze
    Fixed tax_rate_max;           // höchster erlaubter Steuersatz

    // Bevölkerung (seit M4, docs/technik/bevoelkerung.md, Abschnitt 4.9). Raten sind Jahreswerte.
    Fixed share_children;               // Altersaufbau beim Start: Anteil 0–14 Jahre
    Fixed share_young;                  // … 15–29
    Fixed share_working;                // … 30–64 (Rest: 65+)
    Fixed fertility;                    // Geburten je Person im fruchtbaren Alter
    Fixed fertile_share_working;        // Anteil der 30–64-Jährigen im fruchtbaren Alter
    Fixed fertility_reserve;            // Zuschlag bei voller Landreserve
    Fixed fertility_wealth;             // Zu-/Abschlag je 50 Punkte Lebensstandard
    Fixed mortality_children;           // jährliche Sterblichkeit 0–14
    Fixed mortality_young;              // … 15–29
    Fixed mortality_working;            // … 30–64
    Fixed mortality_old;                // … 65+
    Fixed mortality_poverty;            // Zu-/Abschlag je 50 Punkte Lebensstandard
    Fixed capacity_city;                // Faktor der Tragfähigkeit für Städte
    Fixed living_standard_reference;    // Netto-Einkommen für Lebensstandard 50
    Fixed living_standard_adjustment;   // Anteil der Lücke zum Ziel je Monat
    Fixed literacy_city;                // Faktor der Alphabetisierung in Städten
    Fixed literacy_adjustment;          // Anteil der Lücke zum Ziel je Jahr
    std::int32_t migration_targets = 0; // Wanderungsziele je Ort
    Fixed migration_radius_km;          // Umkreis der Wanderungsziele
    Fixed migration_rate;               // Anteil der Jungen je Jahr bei 100 Punkten Unterschied
    Fixed migration_max;                // Obergrenze der Abwanderung je Jahr (Anteil der Jungen)
    Fixed migration_reserve;            // Punkte Anziehung für freies Land (Agrarberufe)
    Fixed migration_foreign;            // Faktor für Wanderung über Landesgrenzen
    Fixed emigration_threshold;         // Lebensstandard, unter dem ausgewandert wird
    Fixed emigration_rate;              // Anteil der Jungen je Jahr und 100 Punkte unter der Schwelle
    Fixed block_min_size;               // kleinere Blöcke werden aufgelöst (Personen)
    Fixed migration_min_amount;         // kleinere Wanderungsströme entfallen (Personen)
};

/// Bevölkerungsbilanz eines Ortes über ein Jahr (seit M4), in Personen.
struct PopulationBalance {
    Fixed births;
    Fixed deaths;        // natürliche Sterbefälle
    Fixed plague_deaths; // Seuchentote
    Fixed moved_in;      // Zuzug aus anderen Orten
    Fixed moved_out;     // Wegzug in andere Orte
    Fixed emigrated;     // Auswanderung (verlässt die Simulation)
};

/// Momentaufnahme eines Ortes vom letzten Rechentag (seit M4, bevoelkerung.md Abschnitt 2.4):
/// Andere Orte lesen nur diese Werte, nie den laufenden Zustand.
struct LocationSnapshot {
    Fixed population;
    Fixed reserve; // Landreserve 0..1
};

/// Produktion und Steuern eines Ortes je Monat, festgestellt am Rechentag des Ortes.
struct LocationEconomy {
    Fixed production; // Reichstaler pro Monat
    Fixed taxes;      // davon an den Staat
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
    std::vector<Culture> cultures;
    std::vector<Profession> professions;
    std::vector<State> states;
    std::vector<District> districts;
    std::vector<Location> locations;
    std::vector<PopBlock> pops;
    std::vector<ChronicleEntry> chronicle;

    // Zustand der Bevölkerung je Ort (seit M4), parallel zu locations. Der Lader lässt die Vektoren
    // leer; die Simulation füllt sie beim Start, wenn ihre Größe nicht passt.
    std::vector<LocationSnapshot> snapshots;
    std::vector<Fixed> snapshot_targets; // Ziel-Lebensstandard je Ort und Beruf: [ort × berufe + beruf]
    std::vector<PopulationBalance> balance_current;  // laufendes Jahr
    std::vector<PopulationBalance> balance_previous; // Vorjahr
    std::vector<LocationEconomy> economy;            // am letzten Rechentag
    std::vector<std::vector<PopBlock>> arrivals; // je Ort: Zugezogene ohne eigenen Block, bis zur Neuordnung

    Index find_state(std::string_view tag) const;
    Index find_location(std::string_view id) const;
    Index find_district(std::string_view id) const;
    Index find_terrain(std::string_view id) const;
    Index find_confession(std::string_view id) const;
    Index find_culture(std::string_view id) const;
    Index find_profession(std::string_view id) const;

    /// Personen in den Blöcken des Ortes und in seinen Zugängen (arrivals).
    Fixed location_population(Index location) const;
    Fixed state_population(Index state) const;
    /// Indizes aller Orte eines Staates, aufsteigend.
    std::vector<Index> state_locations(Index state) const;
    /// Indizes aller Orte eines Kreises, aufsteigend.
    std::vector<Index> district_locations(Index district) const;
    /// Anzahl der Orte mit Simulationsdaten (Location::has_data).
    std::size_t locations_with_data() const;
    /// Konfession mit den meisten Angehörigen im Ort (bei Gleichstand der kleinere Index).
    Index location_majority_confession(Index location) const;
    /// Kultur mit den meisten Angehörigen im Ort (bei Gleichstand der kleinere Index).
    Index location_majority_culture(Index location) const;

private:
    template <typename Key> Index majority(Index location, std::size_t count, Key key) const;
};

} // namespace uhrwerk
