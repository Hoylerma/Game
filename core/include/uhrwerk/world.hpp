#pragma once

// Weltmodell des Simulationskerns (seit M1; Bevölkerung seit M4, docs/technik/bevoelkerung.md).
//
// Daten sind als flache Vektoren mit Indizes organisiert (datenorientiert);
// Querverweise sind Indizes, keine Zeiger. Die Reihenfolge aller Vektoren ist
// durch die Ladereihenfolge festgelegt und damit deterministisch.

#include "uhrwerk/date.hpp"
#include "uhrwerk/fixed.hpp"

#include <array>
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
    bool taxable = true;           // privilegierte Stände (Adel, Klerus) zahlen keine Kontribution
    Fixed share_city;              // Anteil in Städten (0..1)
    Fixed share_rural;             // Anteil auf dem Land (0..1)
    bool agrarian = false;         // Ertrag hängt am Boden (Tragfähigkeit), seit M4
    Fixed literacy;                // Alphabetisierung, Grundwert 0..1 (seit M4)
    Index city_profession = kNone; // Beruf nach dem Zuzug in eine Stadt, kNone = derselbe (seit M4)
    Fixed agrarian_levy;           // Anteil am Agrarertrag des Ortes (Grundrente, Zehnt), seit M5
};

/// Menge eines Gutes je Kopf und Jahr (Warenkorb, Erzeugung, Vorleistung), seit M5.
struct GoodAmount {
    Index good = kNone;
    Fixed amount;
};

/// Gut (data/common/gueter.chron, seit M5, docs/technik/wirtschaft.md Abschnitt 2.1).
struct Good {
    std::string id;
    std::string name;
    Color color;
    Fixed base_price;   // Reichstaler je Einheit
    Fixed world_factor; // Preis am Weltmarkt als Faktor auf den Basispreis; 0 = nicht am Weltmarkt
};

/// Bedürfnis mit Warenkorb je Schicht (data/common/beduerfnisse.chron, seit M5). Die Reihenfolge
/// der Bedürfnisse ist der Rang beim Einkauf.
struct Need {
    std::string id;
    std::string name;
    bool hunger = false;                           // fehlt es, steigt die Sterblichkeit
    std::array<std::vector<GoodAmount>, 3> basket; // je Schicht (Stratum als Index)
};

/// Wo ein Betrieb stehen darf.
enum class Siting : std::uint8_t { All, City, Rural };

/// Betriebstyp: Arbeitsstätte eines Berufs (data/common/betriebe.chron, seit M5, Abschnitt 2.3).
struct BusinessType {
    std::string id;
    std::string name;
    Index profession = kNone;
    std::vector<GoodAmount> outputs; // je Kopf und Jahr
    std::vector<GoodAmount> inputs;  // Vorleistungen je Kopf und Jahr
    bool agrarian = false;           // Ertrag hängt an Tragfähigkeit und Ernte
    Siting siting = Siting::All;
    Index deposit = kNone;              // nur in Orten mit diesem Vorkommen (World::deposits)
    bool trade = false;                 // Einkommen aus der Handelsspanne statt aus Gütern
    Fixed weight;                       // Anteil beim Start (relativ)
    std::vector<Fixed> terrain_factors; // Faktor auf das Gewicht je Geländetyp (Standard 1)
};

/// Markt (data/common/maerkte.chron, seit M5, Abschnitt 2.4): eine oder mehrere Landschaften.
struct Market {
    std::string id;
    std::string name;
    std::vector<std::string> landscapes; // Dateien in history/orte/ ohne Endung
};

/// Steuerarten (seit M5, Abschnitt 4.2 und 4.3).
enum class TaxKind : std::uint8_t { Kontribution, Akzise, Zoll };
inline constexpr std::size_t kTaxKinds = 3;

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
    Index market = kNone;       // Markt (seit M5), aus der Landschaft des Ortes; unveränderlich
    std::uint32_t deposits = 0; // Vorkommen als Bitmaske über World::deposits (seit M5)
    bool is_port = false;       // Hafen mit Zugang zum Weltmarkt (seit M5)
    Index admin = kNone;        // Verwaltungseinheit der untersten Ebene (seit M7), kNone ohne
    bool federal = false;       // liegt im Gebiet des Deutschen Bundes (seit M7)
    Fixed devastation;          // Kriegsverwüstung 0..1
    std::string source;         // Quellenangabe oder "Testdaten"
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

/// Verwaltungseinheit (data/history/verwaltung/*.chron, seit M7, docs/technik/lesezeichen_1848.md
/// Abschnitt 6): Provinz, Regierungsbezirk, Kreis, Kronland … eines Staates. Die Einheiten bilden
/// je Staat einen Baum über parent; ein Ort verweist auf die Einheit der untersten Ebene
/// (Location::admin). In M7 Inhalt (zum Startdatum aus den Daten), keine Simulationsgröße.
struct AdminUnit {
    std::string id; // z. B. "pr_rb_potsdam"
    std::string name;
    std::string kind;     // Bezeichner, z. B. "regierungsbezirk"
    Index parent = kNone; // übergeordnete Einheit, kNone für die oberste
    std::string source;
};

/// Abschnitt einer Eisenbahnstrecke von Ort zu Ort mit Eröffnungstag (seit M7, chronik-skript.md 3.17).
struct RailSection {
    Index from = kNone;
    Index to = kNone;
    Date opened;
};

/// Eisenbahnstrecke (seit M7): Inhalt, kein Zustand; ob ein Abschnitt befahren wird, folgt aus dem Datum.
struct Railway {
    std::string id; // z. B. "leipzig_dresden"
    std::string name;
    std::vector<RailSection> sections; // in der Reihenfolge der Datei
    std::string source;
};

/// Haushalt eines Staates in einem Monat, festgestellt an seinem Rechentag (seit M5,
/// docs/technik/wirtschaft.md Abschnitt 4.6). Beträge in Reichstalern.
struct StateBudget {
    std::array<Fixed, kTaxKinds> taxes{}; // Einnahmen je TaxKind
    Fixed court;                          // Hof
    Fixed administration;                 // Verwaltung
    Fixed army;                           // Heer
    Fixed interest;                       // Zinsen
    Fixed building;                       // Bauten (Landesausbau)

    Fixed income() const { return taxes[0] + taxes[1] + taxes[2]; }
    Fixed expenses() const { return court + administration + army + interest + building; }
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
    Index confession = kNone;                 // Konfession des Herrscherhauses
    Fixed treasury;                           // Staatskasse in Reichstalern
    std::array<Fixed, kTaxKinds> tax_rates{}; // Steuersätze je TaxKind (seit M5; vorher nur Kontribution)
    Fixed court_expenses;                     // Hofhaltung pro Jahr
    Fixed army_per_mille;                     // Soldaten je 1.000 Einwohner (seit M5)
    Fixed debt;                               // Staatsschulden in Reichstalern (seit M5)
    Fixed last_month_income;
    Fixed last_month_expenses;
    std::string source;

    // Haushalt (seit M5, wirtschaft.md Abschnitt 4.6)
    StateBudget budget;              // Vormonat (am letzten Rechentag)
    Fixed yearly_income;             // gleitende Jahreseinnahmen
    Fixed interest_rate;             // Zins des Vormonats (Anzeige)
    std::int32_t premium_months = 0; // Monate mit Zinsaufschlag nach einem Staatsbankrott
    std::int32_t bankruptcies = 0;   // Zahl der Staatsbankrotte in dieser Partie
    bool customs_union = false;      // Mitglied des Zollvereins (seit M7; Inhalt, bis M9 ohne Wirkung)

    Fixed tax_rate(TaxKind kind) const { return tax_rates[static_cast<std::size_t>(kind)]; }
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

    // Wirtschaft und Staatshaushalt (seit M5, docs/technik/wirtschaft.md, Abschnitt 4.9).
    Fixed price_min; // Preisgrenzen als Faktor auf den Basispreis
    Fixed price_max;
    Fixed price_adjustment;           // Anteil der Lücke zum Zielpreis je Monat
    std::int32_t trade_neighbors = 0; // Nachbarmärkte je Markt
    Fixed trade_radius_km;            // Umkreis der Nachbarmärkte
    Fixed trade_rate;                 // Stärke des Handels je Monat
    Fixed freight_land;               // Frachtkosten je 100 km (Anteil am Preis)
    Fixed freight_sea;                // Frachtkosten zum Weltmarkt
    Fixed freight_rail;               // Frachtkosten je 100 km zwischen Märkten mit Bahnverbindung (seit M7)
    Fixed trade_margin;               // Handelsspanne (Anteil am Umsatz)
    Fixed business_adjustment;        // Anpassung der Betriebsanteile je Monat
    Fixed business_min_share;         // Mindestanteil eines erlaubten Betriebs
    Fixed harvest_variation_land;     // Erntefaktor: landesweite Schwankung
    Fixed harvest_variation_market;   // … Schwankung je Markt
    Fixed mortality_hunger;           // Zuschlag auf die Sterblichkeit ohne Grundnahrung
    std::int32_t settling_months = 0; // Einschwingen beim Start (Monate)
    Fixed administration_per_capita;  // Verwaltung: Reichstaler je Einwohner und Jahr
    Fixed soldier_pay;                // Sold: Reichstaler je Soldat und Jahr
    Fixed reserve_years;              // Rücklage in Jahresausgaben
    Fixed building_share;             // Anteil des Überschusses, der je Monat verbaut wird
    Fixed development_max;            // Landesausbau: höchster Zuschlag auf die Tragfähigkeit
    Fixed development_half;           // … Reichstaler je km² für den halben Zuschlag
    Fixed interest_base;              // Zins: Grundsatz
    Fixed interest_risk;              // … Aufschlag je Jahreseinnahme Schulden über der ersten
    Fixed interest_max;               // … Obergrenze
    Fixed bankruptcy_ratio;           // Bankrott bei Schulden über so vielen Jahreseinnahmen
    Fixed bankruptcy_haircut;         // Schuldenschnitt beim Bankrott
    Fixed bankruptcy_premium;         // Zinsaufschlag nach dem Bankrott
    std::int32_t bankruptcy_years = 0; // … für so viele Jahre
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

/// Wirtschaft eines Ortes je Monat, festgestellt an seinem Rechentag (seit M5 mit Steuern je Art,
/// docs/technik/wirtschaft.md Abschnitt 4.2).
struct LocationEconomy {
    Fixed production;                     // Wertschöpfung zu Marktpreisen, Reichstaler pro Monat
    std::array<Fixed, kTaxKinds> taxes{}; // Steuern je TaxKind an den Besitzer
    Fixed traders;                        // Köpfe in Handelshäusern (Anteil an der Handelsspanne)

    Fixed total_taxes() const { return taxes[0] + taxes[1] + taxes[2]; }
};

/// Zustand eines Marktes (seit M5, Abschnitt 4.1), veröffentlicht am Monatsersten. Vektoren je Gut.
struct MarketState {
    std::vector<Fixed> price;        // Reichstaler je Einheit
    std::vector<Fixed> buy_ratio;    // Kaufquote: Anteil der Nachfrage, der zu haben ist (0..1)
    std::vector<Fixed> sell_ratio;   // Verkaufsquote: Anteil des Angebots, der Käufer findet (0..1)
    std::vector<Fixed> import_share; // Anteil der Einfuhr am Angebot (0..1)
    std::vector<Fixed> supply;       // Angebot des Monats mit Einfuhr (Menge)
    std::vector<Fixed> demand;       // Nachfrage des Monats mit Ausfuhr (Menge)
    std::vector<Fixed> imports;      // Einfuhr des Monats (Menge, von Nachbarn und Weltmarkt)
    std::vector<Fixed> exports;      // Ausfuhr des Monats (Menge)
    Fixed margin_per_trader;         // Handelsspanne je Kopf im Handelshaus und Monat (Reichstaler)
    Fixed harvest = Fixed::one();    // Erntefaktor des laufenden Jahres (Abschnitt 4.7)
};

/// Szenario/Lesezeichen (data/szenarien/*.chron).
struct Scenario {
    std::string id;
    std::string name;
    Date start;
    std::string player_tag;
    std::vector<std::string> playable; // hervorgehobene Staaten der Staatswahl (seit M7), Tags
    std::string currency; // Name der Rechnungseinheit, z. B. „Taler“ (seit M7); leer = „Reichstaler“
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
    std::vector<Good> goods;              // seit M5
    std::vector<Need> needs;              // seit M5, Reihenfolge = Rang beim Einkauf
    std::vector<BusinessType> businesses; // seit M5
    std::vector<Market> markets;          // seit M5
    std::vector<std::string> deposits;    // Vorkommen (Kennungen aus betriebe.chron), seit M5
    std::vector<State> states;
    std::vector<District> districts;
    std::vector<AdminUnit> admin_units; // seit M7
    std::vector<Railway> railways;      // seit M7
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

    // Zustand der Wirtschaft (seit M5, wirtschaft.md Abschnitt 2.6). Wie oben füllt der Lader die Vektoren
    // beim Start (Einschwingen); die Simulation legt fehlende an.
    std::vector<Fixed> business_shares; // Betriebsanteile: [ort × betriebe + betrieb], je Beruf Summe 1
    std::vector<Fixed> location_supply; // Erzeugung je Ort und Gut im Monat: [ort × güter + gut]
    std::vector<Fixed> location_demand; // Nachfrage (Verbrauch und Vorleistungen) je Ort und Gut im Monat
    std::vector<Fixed> location_income; // Netto-Einkommen je Kopf und Monat: [ort × berufe + beruf]
    std::vector<Fixed>
        location_coverage; // Bedarfsdeckung 0..1: [(ort × berufe + beruf) × bedürfnisse + bedürfnis]
    std::vector<Fixed> location_development; // Landesausbau: verbaute Reichstaler je Ort (Abschnitt 4.6)
    std::vector<MarketState> market_states;  // parallel zu markets

    Index find_state(std::string_view tag) const;
    Index find_location(std::string_view id) const;
    Index find_district(std::string_view id) const;
    Index find_admin_unit(std::string_view id) const;
    Index find_terrain(std::string_view id) const;
    Index find_confession(std::string_view id) const;
    Index find_culture(std::string_view id) const;
    Index find_profession(std::string_view id) const;
    Index find_good(std::string_view id) const;
    Index find_need(std::string_view id) const;
    Index find_business(std::string_view id) const;
    Index find_market(std::string_view id) const;

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
