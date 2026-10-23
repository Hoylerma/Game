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
    bool taxable = true;                // privilegierte Stände (Adel, Klerus) zahlen keine Kontribution
    Fixed share_city;                   // Anteil in Städten (0..1)
    Fixed share_rural;                  // Anteil auf dem Land (0..1)
    bool agrarian = false;              // Ertrag hängt am Boden (Tragfähigkeit), seit M4
    Fixed literacy;                     // Alphabetisierung, Grundwert 0..1 (seit M4)
    Index city_profession = kNone;      // Beruf nach dem Zuzug in eine Stadt, kNone = derselbe (seit M4)
    Fixed agrarian_levy;                // Anteil am Agrarertrag des Ortes (Grundrente, Zehnt), seit M5
    Fixed self_employed = Fixed::one(); // „selbstständig“ im Sinn des Wahlrechts 1848 (0..1), seit M8
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

// ─── Politik (seit M8, docs/technik/politik.md) ─────────────────────────────

/// Politische Strömung (politik.md Abschnitt 3.1), z. B. Liberale. Inhalt, kein Zustand.
struct PoliticalCurrent {
    std::string id;
    std::string name;
    std::string short_name; // für knappe Anzeigen (Telegramme, Balken); ohne Angabe der Name
    Color color;
    Fixed position; // Links-rechts-Achse: −100 Volkssouveränität … +100 monarchisches Prinzip
};

/// Interessengruppe (politik.md Abschnitt 3.3): Mitglieder nach Beruf, Konfession und Bildung,
/// Neigung zu den Strömungen. In M8 statisch; Anteil und Einfluss je Staat sind abgeleitet.
struct InterestGroup {
    std::string id;
    std::string name;
    Color color;
    std::vector<Fixed> professions; // Anteil der Mitglieder je Beruf (0..1), Länge = Berufe
    std::vector<Fixed> confessions; // Faktor je Konfession (0..1); leer = alle Konfessionen
    bool by_literacy = false;       // Mitgliedschaft zusätzlich × Alphabetisierung des Blocks
    std::vector<Fixed> leaning;     // Neigung je Strömung, Summe 1
    std::string source;
};

/// Verfassung eines Staates zum Startdatum (politik.md Abschnitt 4).
enum class Constitution : std::uint8_t { None, Estates, Constitutional };
/// Wahlrecht: wer wählt und mit welchem Gewicht.
enum class Franchise : std::uint8_t { None, Estates, Census, ThreeClass, Manhood };
/// Wahlsystem im Wahlkreis: absolute Mehrheit mit Stichwahl oder relative Mehrheit.
enum class ElectoralSystem : std::uint8_t { Runoff, Plurality };

/// Wahlordnung eines Staates (Landtag) oder einer Wahl.
struct ElectoralLaw {
    Franchise franchise = Franchise::None;
    ElectoralSystem system = ElectoralSystem::Runoff;
    bool indirect = false;  // Wahl über Wahlmänner
    Fixed census_threshold; // Zensus: Steuern je Kopf und Jahr, ab denen ein Mann wählt

    friend bool operator==(const ElectoralLaw&, const ElectoralLaw&) = default;
};

/// Fraktion eines Parlaments: Bereich auf der Links-rechts-Achse (politik.md Abschnitt 3.2).
struct Faction {
    std::string id; // z. B. "casino"
    std::string name;
    Color color;
    Fixed from; // Untergrenze (eingeschlossen)
    Fixed to;   // Obergrenze (eingeschlossen bei der obersten Fraktion)
};

/// Parlament mit seiner Wahl (common/parlamente.chron, politik.md Abschnitt 5.1). Inhalt.
struct Parliament {
    std::string id; // z. B. "nationalversammlung"
    std::string name;
    Index state = kNone;                 // kNone: Deutscher Bund (ebene = bund), sonst der Staat
    std::int32_t seats = 0;              // Sollzahl der Sitze
    Date election_day;                   // Wahltag
    ElectoralLaw law;                    // Wahlrecht dieser Wahl
    Date faction_day;                    // Stichtag der Fraktionen (vorher gelten die Strömungen)
    std::vector<Index> boycott_cultures; // Wahlkreise mit Mehrheit dieser Kulturen wählen nicht …
    std::vector<Index> boycott_markets; // … nur in diesen Märkten (Markt des ersten Ortes); leer = überall
    std::vector<Faction> factions;      // nach Bereich aufsteigend, lückenlos von −100 bis +100
    Index seat = kNone;                 // Tagungsort (Ort); bestimmt die Meldereihenfolge am Wahlabend
    std::string source;
};

/// Arten des Wahlkampfs (seit M8, Schritt b2, politik.md Abschnitt 9).
enum class CampaignAction : std::uint8_t { Rally, Leaflets, Club };
inline constexpr std::size_t kCampaignActions = 3;

/// Kosten und Wirkung einer Art des Wahlkampfs: Jede Aktion erhöht den Stand im Wahlkreis um
/// wirkung × (1 − stand ÷ hoechstens), also mit abnehmendem Ertrag bis zur Obergrenze.
struct CampaignRule {
    Fixed cost;   // Taler aus den Mitteln der Strömung
    Fixed effect; // Zuwachs des Standes bei Stand 0
    Fixed cap;    // Obergrenze des Standes
};

/// Regeln der Wahlsimulation (common/wahlen.chron, politik.md Abschnitt 7). Spielwerte; Vektoren je
/// Strömung, leer ohne Politik.
struct ElectionRules {
    Fixed male_share;       // Männer unter den Erwachsenen
    Fixed young_of_age;     // Anteil der 15–29-Jährigen ab 25 (Wahlalter)
    Fixed young_of_age_24;  // Anteil der 15–29-Jährigen ab 24 (Dreiklassenwahlrecht)
    Fixed turnout_base;     // Beteiligung: Grundwert …
    Fixed turnout_literacy; // … + je Alphabetisierung
    Fixed turnout_city;     // … + in Städten
    Fixed turnout_poverty;  // … − je Not (1 − Lebensstandard/100)
    Fixed turnout_min;
    Fixed turnout_max;
    Fixed poverty_below;         // Lebensstandard, unter dem die Not die Neigung verschiebt
    std::vector<Fixed> base;     // Neigung der Blöcke, soweit sie keiner Interessengruppe angehören
    std::vector<Fixed> literacy; // Verschiebung je Alphabetisierung (−1..1)
    std::vector<Fixed> poverty;  // Verschiebung bei voller Not (Lebensstandard 0)
    std::vector<Fixed> city;     // Verschiebung in Städten
    std::vector<Fixed> electors; // Faktor bei indirekter Wahl über Wahlmänner (Standard 1)
    Fixed noise;                 // Rauschen je Wahlkreis und Strömung (±, relativ)
    Fixed position_pull;         // Zug der Abgeordneten zur Mitte ihres Wahlkreises (0..1)
    Fixed position_noise;        // Rauschen der Position (± Punkte)
    Fixed independent_share;     // Anteil der fraktionslosen Abgeordneten
    // Wahlkampf (seit Schritt b2, Abschnitt 9)
    Fixed donations;                                       // Spende: Anteil am Monatseinkommen der Anhänger
    std::array<CampaignRule, kCampaignActions> campaign{}; // je CampaignAction
    Fixed poll_error;                                      // Stimmungsbericht: ± Prozentpunkte (Anzeige)
};

/// Wahlkampf einer Strömung in einem Wahlkreis (Zustand seit M8, Schritt b2).
struct CampaignState {
    Fixed mobilization; // Versammlungen: Stimmen der Strömung × (1 + Mobilisierung), bis zum Wahltag
    Fixed persuasion;   // Flugblätter: Neigung + je Alphabetisierung, bis zum Wahltag
    Fixed club;         // Vereine: bleibende Mobilisierung
};

/// Region mit eigener Wahlneigung (common/wahlen.chron): Staaten und Märkte (Landschaften).
struct PoliticalRegion {
    std::string id;
    std::vector<Index> states;
    std::vector<Index> markets;
    std::vector<Fixed> leaning; // Verschiebung je Strömung (−1..1)
};

/// Ergebnis eines Wahlkreises (Zustand seit M8, politik.md Abschnitt 7.7).
struct DistrictResult {
    Index district = kNone;       // in World::electoral_districts
    Fixed electorate;             // Wahlberechtigte
    Fixed voters;                 // Wähler
    std::vector<Fixed> votes;     // Stimmen je Strömung nach Gewichten (Dreiklassen, Wahlmänner, Rauschen)
    Index winner = kNone;         // Strömung; kNone bei Boykott oder ohne Wähler
    bool runoff = false;          // durch Stichwahl entschieden
    bool boycott = false;         // Wahlkreis hat nicht gewählt
    std::int32_t report_rank = 0; // Meldereihenfolge am Wahlabend (0 = zuerst)
};

/// Gewählter Abgeordneter (Zustand seit M8).
struct Deputy {
    Index district = kNone;
    Index current = kNone;
    Fixed position;        // Links-rechts-Achse
    Index faction = kNone; // kNone = fraktionslos
};

/// Ergebnis einer Wahl (Zustand seit M8).
struct ElectionResult {
    Index parliament = kNone;
    Date day;
    std::vector<DistrictResult> districts; // in der Reihenfolge der Wahlkreise des Parlaments
    std::vector<Deputy> deputies;          // je Wahlkreis und Sitz
};

/// Wahlkreis eines Parlaments (history/wahlkreise/*.chron, politik.md Abschnitt 5.3). Inhalt; die Orte
/// gehören zum Startdatum dem Staat des Wahlkreises.
struct ElectoralDistrict {
    std::string id; // z. B. "nv_bad_10"
    std::string name;
    Index parliament = kNone;
    Index state = kNone;
    std::int32_t seats = 1;       // mehrere Sitze für große Städte (Berlin, Wien)
    std::vector<Index> locations; // in der Reihenfolge der Datei
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
    Fixed diplomacy;                      // Gesandte (seit M9, diplomatie.md Abschnitt 4.2)

    Fixed income() const { return taxes[0] + taxes[1] + taxes[2]; }
    Fixed expenses() const { return court + administration + army + interest + building + diplomacy; }
};

/// Heerwesen eines Staates (seit M10, Abschnitt 3.2): aus common/militaer.chron, zum Startdatum. Inhalt.
struct ArmyProfile {
    Fixed firepower = Fixed::one();    // Feuerkraft der Infanterie (Vorderlader = 1)
    Fixed guns_per_mille;              // Geschütze je 1.000 Soldaten
    Fixed mobilization = Fixed::one(); // Kriegsstärke ÷ Friedensstärke
    std::int32_t mobilization_days = 30;

    friend bool operator==(const ArmyProfile&, const ArmyProfile&) = default;
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
    Index customs_union = kNone;     // Zollunion (seit M9, diplomatie.md 3.1); kNone: eigenes Zollgebiet
    Fixed customs_income;            // Anteil an der Vereinskasse im laufenden Monat (seit M9, Abschnitt 3.4)
    Constitution constitution = Constitution::None; // zum Startdatum (seit M8, Inhalt)
    ElectoralLaw law;                               // Wahlordnung des Landtags (seit M8, Inhalt)
    ArmyProfile army;                               // Heerwesen zum Startdatum (seit M10, Inhalt)

    Fixed tax_rate(TaxKind kind) const { return tax_rates[static_cast<std::size_t>(kind)]; }
};

/// Zollunion (seit M9, docs/technik/diplomatie.md Abschnitt 3.1; data/common/zollunionen.chron): ein
/// gemeinsames Zollgebiet mehrerer Staaten mit einem Tarif. Die Mitglieder stehen bei den Staaten.
struct CustomsUnion {
    std::string id; // z. B. "zollverein"
    std::string name;
    Index leader = kNone;        // Leitmacht (Staat): entscheidet über Beitritte (Abschnitt 3.5)
    Fixed tariff;                // Vereinstarif: Zollsatz aller Mitglieder
    std::vector<Index> excluded; // Staaten, deren Beitritt die Leitmacht ablehnt (Spielwert bis M12)
    Color color{90, 90, 90};     // Kartenmodus „Zollgebiete“ (seit Schritt a4, nur Anzeige)
    std::string source;
};

/// Beantragter Wechsel der Zollunion (seit M9, Abschnitt 3.5), wirksam zum nächsten 1. Januar.
struct CustomsRequest {
    Index state = kNone;
    Index customs_union = kNone; // kNone: Austritt

    friend bool operator==(const CustomsRequest&, const CustomsRequest&) = default;
};

/// Regeln der Diplomatie (seit M9, docs/technik/diplomatie.md Abschnitte 4.1–4.3;
/// data/common/diplomatie.chron). Beziehungen reichen von −200 bis +200.
struct DiplomacyRules {
    Fixed drift;         // Anteil der Lücke zum Zielwert, den eine Beziehung je Monat schließt
    Fixed customs_union; // Zielwert: gemeinsame Zollunion
    Fixed federation;    // … beide im Deutschen Bund
    Fixed confession;    // … gleiche Konfession der Herrscherhäuser (nach confession_group)
    Fixed alliance;      // … Bündnis
    Fixed guarantee;     // … Garantie (je Richtung)
    Fixed envoy;         // Gesandter: Verbesserung je Monat …
    Fixed envoy_limit;   // … bis so weit über den Zielwert
    Fixed envoy_cost;    // Kosten je Gesandtem: Anteil der Monatseinnahmen
    Fixed envoy_people;  // ein Gesandter mehr je so viele Einwohner (Kapazität 1 + Einwohner / x)
    Fixed alliance_min;  // ein Bündnis wird ab dieser Beziehung angenommen
    Fixed guarantee_min; // eine Garantie ab dieser
    Fixed rival_below;   // Gegner: Beziehung unter diesem Wert
    std::vector<Index> confession_group; // je Konfession: Gruppe für „gleiche Konfession“ (Index)
    // Bundesversammlung und diplomatische Spiele (seit Schritt b2, Abschnitte 4.4 und 4.5)
    Fixed intervention;                // Bundesintervention: einmalig auf die Beziehung des Ziels zu jedem Ja
    std::int32_t play_stage_days = 30; // Dauer einer Stufe (Protest, Ultimatum, Mobilmachung)
    Fixed play_leaning;                // Parteinahme nach Neigung ab diesem Unterschied der Beziehungen
    Fixed play_break;                  // einmalig zwischen Staaten auf verschiedenen Seiten
    Fixed play_stage;                  // je neuer Stufe zwischen den beiden Hauptparteien
    Fixed play_yield;   // Stärkeverhältnis, unter dem eine Seite nachgibt bzw. Vermittlung angenommen wird
    Fixed mobilization; // Faktor auf die Heereskosten der Hauptparteien in der Mobilmachung
};

/// Stimme im Engeren Rat der Bundesversammlung (seit M9, Schritt b2): Virilstimme eines Staates oder
/// Kuriatstimme mehrerer Staaten, deren Mehrheit nach Plenarstimmen entscheidet.
struct CouncilVote {
    std::string id;
    std::string name;
    std::vector<Index> states;
};

/// Bundesversammlung (seit M9, Schritt b2; data/common/bundesversammlung.chron, Abschnitt 4.4).
struct FederalDiet {
    std::string id; // leer: keine Bundesversammlung im Szenario
    std::string name;
    Index presidency = kNone;         // Präsidialmacht zum Start (im Spiel: World::diet_presidency)
    Index seat = kNone;               // Tagungsort (Ort), Anzeige
    std::vector<std::int32_t> plenum; // Stimmen im Plenum je Staat, 0 = keine
    std::vector<CouncilVote> council; // Engerer Rat
    std::string source;
};

enum class MotionKind : std::uint8_t {
    Reform,             // Bundesreform: Plenum, einstimmig
    CustomsUnification, // Zolleinigung des Bundes in einer Union: Plenum, zwei Drittel
    Intervention,       // Bundesintervention gegen einen Staat: Engerer Rat, Mehrheit
    Presidency,         // Präsidium für den Antragsteller: Plenum, einstimmig
};

/// Antrag in der Bundesversammlung (Zustand seit Schritt b2); Abstimmung am ersten Monatsersten nach
/// dem Tag des Antrags.
struct Motion {
    MotionKind kind = MotionKind::Reform;
    Index applicant = kNone;
    Index target = kNone; // Zolleinigung: Zollunion; Intervention: Staat
    Date filed;

    friend bool operator==(const Motion&, const Motion&) = default;
};

/// Abgestimmter Antrag (Zustand seit Schritt b2).
struct MotionResult {
    MotionKind kind = MotionKind::Reform;
    Index applicant = kNone;
    Index target = kNone;
    Index opponent = kNone; // stärkster Gegner des Antrags (Abschnitt 4.4)
    Date date;
    std::int32_t plenum_yes = 0;
    std::int32_t plenum_total = 0;
    std::int32_t council_yes = 0;
    std::int32_t council_total = 0;
    bool accepted = false;

    friend bool operator==(const MotionResult&, const MotionResult&) = default;
};

enum class DemandKind : std::uint8_t {
    Leadership,    // Führung im Bund
    CustomsUnion,  // Beitritt des Gegners zur Zollunion des Fordernden
    BreakAlliance, // der Gegner kündigt seine Bündnisse mit Gegnern des Fordernden
};

enum class PlayOutcome : std::uint8_t { DemanderWon, TargetWon, Compromise };

/// Laufendes diplomatisches Spiel (Zustand seit Schritt b2, Abschnitt 4.5).
struct DiplomaticPlay {
    DemandKind kind = DemandKind::Leadership;
    Index demander = kNone;
    Index target = kNone;
    Index customs_union = kNone; // CustomsUnion: Union des Fordernden beim Start
    std::int32_t stage = 1;      // 1 Protest, 2 Ultimatum, 3 Mobilmachung
    Date started;
    Date stage_started;
    std::vector<Index> demander_side; // Unterstützer (ohne die Hauptpartei), aufsteigend
    std::vector<Index> target_side;
    bool demander_escalates = false; // Manöver „eskalieren“ in der laufenden Stufe
    bool target_escalates = false;

    friend bool operator==(const DiplomaticPlay&, const DiplomaticPlay&) = default;
};

/// Beendetes diplomatisches Spiel (Zustand seit Schritt b2).
struct PlayResult {
    DemandKind kind = DemandKind::Leadership;
    Index demander = kNone;
    Index target = kNone;
    Index customs_union = kNone;
    Date started;
    Date ended;
    std::int32_t stage = 1;
    PlayOutcome outcome = PlayOutcome::Compromise;

    friend bool operator==(const PlayResult&, const PlayResult&) = default;
};

/// Nachbarschaft eines Ortes (seit M10, docs/technik/militaer.md Abschnitt 3.1): Landgrenze aus
/// karte/nachbarn.csv (Kartengenerator) oder Überfahrt aus karte/ueberfahrten.csv. Inhalt, nur in
/// Welten mit Militär (MilitaryRules::active).
struct Neighbor {
    Index location = kNone;      // Nachbarort
    Fixed distance_km;           // Abstand der Lagen (wie bei der Wanderung)
    std::int32_t border = 0;     // gemeinsame Grenze in Pixelkanten, 0 bei einer Überfahrt
    std::int32_t river = 0;      // Rang des größten gekreuzten Flusses (1 = Rhein, Elbe …), 0: keiner
    std::int32_t ferry_days = 0; // Überfahrt: Tage; 0: Landweg

    friend bool operator==(const Neighbor&, const Neighbor&) = default;
};

/// Regeln des Militärs (seit M10, common/militaer.chron, Abschnitte 3–5). Inhalt.
struct MilitaryRules {
    bool active = false;         // Militärdaten zum Startdatum ('seit'): Heere und Krieg (Entscheidung 14)
    std::int32_t river_rank = 3; // Flussübergang: gekreuzter Fluss bis zu diesem Rang
};

/// Garnison des Friedensheers (seit M10, Abschnitt 3.2): Anteil der Friedensstärke eines Staates an einem
/// Ort. Ohne Eintrag hat ein Staat eine Garnison in seiner Hauptstadt. Inhalt.
struct Garrison {
    std::string id;
    std::string name; // z. B. "Gardekorps"
    Index state = kNone;
    Index location = kNone;
    Fixed share; // Anteil am Friedensheer (je Staat zusammen 1)
};

/// Lage eines Heeres (seit M10). Weitere Lagen (Marsch, Bahnfahrt, Schlacht, Belagerung) kommen mit
/// den Schritten a2 und a3.
enum class ArmyStatus : std::uint8_t {
    Garrison = 0, // Friedensheer in seiner Garnison
};

/// Heer (seit M10, Abschnitt 3.2). Zustand; die Nummer bleibt über die ganze Partie gleich (Befehle).
struct Army {
    std::int64_t id = 0;
    Index state = kNone;
    Index location = kNone;
    Index garrison = kNone; // Garnison, aus der es stammt; kNone für Feldarmeen ohne Garnison
    std::string name;
    Fixed soldiers;
    Fixed guns;
    Fixed morale = Fixed::one(); // 0..1
    ArmyStatus status = ArmyStatus::Garrison;

    friend bool operator==(const Army&, const Army&) = default;
};

/// Grundbeziehung zweier Staaten aus den Daten (seit M9, history/beziehungen, Abschnitt 4.1).
struct BaseRelation {
    Index a = kNone;
    Index b = kNone;
    Fixed value;
    std::string source;
};

/// Gesandter eines Staates bei einem anderen (Zustand seit M9, Abschnitt 4.2).
struct Envoy {
    Index from = kNone;
    Index to = kNone;
    Date since;

    friend bool operator==(const Envoy&, const Envoy&) = default;
};

enum class TreatyKind : std::uint8_t {
    Alliance,  // Bündnis (gegenseitig)
    Guarantee, // Garantie: from schützt to
};

/// Vertrag zwischen zwei Staaten (Zustand seit M9, Abschnitt 4.3). Beim Bündnis ist from der Staat,
/// der es vorgeschlagen hat.
struct Treaty {
    TreatyKind kind = TreatyKind::Alliance;
    Index from = kNone;
    Index to = kNone;
    Date since;

    friend bool operator==(const Treaty&, const Treaty&) = default;
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

    // Zollgebiete (seit M9, diplomatie.md Abschnitte 3.2 und 3.4), nur mit Zollunionen.
    std::vector<Index> zones;         // Zollgebiete im Markt (customs_zone), aufsteigend
    std::vector<Fixed> zone_shares;   // Anteil an der Bevölkerung je Zollgebiet, parallel zu zones
    std::vector<Fixed> foreign_share; // fremder Einfuhranteil am Angebot: [gebiet × güter + gut]
};

/// Szenario/Lesezeichen (data/szenarien/*.chron).
struct Scenario {
    std::string id;
    std::string name;
    Date start;
    std::string player_tag;
    std::vector<std::string> playable; // hervorgehobene Staaten der Staatswahl (seit M7), Tags
    std::string currency; // Name der Rechnungseinheit, z. B. „Taler“ (seit M7); leer = „Reichstaler“
    Fixed capacity_factor =
        Fixed::one(); // Faktor auf die Tragfähigkeit aller Gelände (Agrartechnik, seit M7)
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
    std::vector<AdminUnit> admin_units;                 // seit M7
    std::vector<Railway> railways;                      // seit M7
    std::vector<PoliticalCurrent> currents;             // seit M8
    std::vector<InterestGroup> interest_groups;         // seit M8
    std::vector<Parliament> parliaments;                // seit M8, nur die des Szenarios ('seit')
    std::vector<ElectoralDistrict> electoral_districts; // seit M8, die Wahlkreise dieser Parlamente
    ElectionRules election_rules;                       // seit M8
    std::vector<PoliticalRegion> political_regions;     // seit M8
    std::vector<CustomsUnion> customs_unions;           // seit M9, nur die des Szenarios ('seit')
    DiplomacyRules diplomacy_rules;                     // seit M9 (b1)
    std::vector<BaseRelation> base_relations;           // seit M9 (b1): Paare dieses Szenarios
    FederalDiet diet;                                   // seit M9 (b2), id leer ohne Bundesversammlung
    MilitaryRules military_rules;                       // seit M10 (a1)
    std::vector<std::vector<Neighbor>> neighbors; // seit M10 (a1): je Ort, nach Nachbarort; nur mit Militär
    std::vector<Garrison> garrisons;              // seit M10 (a1): nach Staat, dann in Dateireihenfolge
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
    std::vector<Fixed> location_development;      // Landesausbau: verbaute Reichstaler je Ort (Abschnitt 4.6)
    std::vector<MarketState> market_states;       // parallel zu markets
    std::vector<ElectionResult> elections;        // seit M8: abgehaltene Wahlen in zeitlicher Folge
    std::vector<CampaignState> campaign;          // seit M8 (b2): [wahlkreis × strömungen + strömung]
    std::vector<Fixed> party_funds;               // seit M8 (b2): Mittel je Strömung (Taler)
    std::vector<Fixed> party_donations;           // seit M8 (b2): Spenden des letzten Monats je Strömung
    std::vector<Fixed> customs_revenue;           // seit M9: Vereinskasse des Monats je Zollunion
    std::vector<CustomsRequest> customs_requests; // seit M9: beantragte Wechsel, wirksam zum 1. Januar
    // Diplomatie (seit M9, Schritt b1), nur in Welten mit Deutschem Bund (diplomacy_active); sonst leer.
    std::vector<Fixed> relations; // Beziehung je Staatenpaar a < b (relation_slot), −200..200
    std::vector<Envoy> envoys;    // Gesandte in der Reihenfolge ihrer Entsendung
    std::vector<Treaty> treaties; // Bündnisse und Garantien in der Reihenfolge des Abschlusses
    // Bundesversammlung und diplomatische Spiele (seit Schritt b2), ebenso nur mit Deutschem Bund.
    Index diet_presidency = kNone;            // Präsidialmacht (Antrag Präsidium)
    std::vector<Motion> motions;              // offener Antrag (höchstens einer)
    std::vector<MotionResult> motion_results; // abgestimmte Anträge in zeitlicher Folge
    std::vector<DiplomaticPlay> plays;        // laufende Spiele in der Reihenfolge ihres Beginns
    std::vector<PlayResult> play_results;     // beendete Spiele in zeitlicher Folge
    // Militär (seit M10, Schritt a1), nur in Welten mit Militärdaten (military_rules.active); sonst leer.
    std::vector<Army> armies;      // Heere in der Reihenfolge ihrer Aufstellung
    std::int64_t next_army_id = 1; // Nummer des nächsten Heeres

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
    Index find_current(std::string_view id) const;        // seit M8
    Index find_interest_group(std::string_view id) const; // seit M8
    Index find_parliament(std::string_view id) const;     // seit M8
    Index find_customs_union(std::string_view id) const;  // seit M9

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
