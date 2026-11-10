#pragma once

// Zeitung (seit M13b1, docs/technik/vertical_slice.md 5.2; chronik-skript.md 3.42).
//
// Eine Titelseite entsteht aus einem Chronikeintrag: Der erste Anlass aus common/zeitung.chron, dessen
// Muster auf den Text passt, liefert Schlagzeile, Unterzeile und Text mit den Platzhaltern des Musters.
// Das Blatt folgt dem Leser (Spielerstaat) und wechselt seine Richtung je Ausgabe. Alles ist aus Chronik
// und Inhalt abgeleitet, ohne Zufallszahlen und ohne Zustand: Die Simulation bleibt unberührt.

#include "uhrwerk/world.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace uhrwerk {

/// Werte der Platzhalter eines passenden Musters (Name, Text) in der Reihenfolge des Musters.
using NewspaperCaptures = std::vector<std::pair<std::string, std::string>>;

/// Passt das Muster des Anlasses auf den Eintrag? Platzhalter {staat}, {gegner}, {partner} und {sieger}
/// müssen Kurznamen oder Namen von Staaten sein, {zahl}, {sitze} und {besetzt} Zahlen, {titel} der Titel
/// eines Ereignisses (mit 'ereignis' dieses, mit 'wendepunkt' eines Wendepunkts). '*' steht für beliebigen
/// Text, auch leer.
std::optional<NewspaperCaptures> match_occasion(const World& world, const NewspaperOccasion& occasion,
                                                const ChronicleEntry& entry);

/// Erster passender Anlass (Index in world.newspaper.occasions), sonst nullopt.
std::optional<std::size_t> newspaper_occasion(const World& world, const ChronicleEntry& entry);

/// Gewicht des Eintrags: das des passenden Anlasses, ohne Anlass −1.
std::int32_t newspaper_weight(const World& world, const ChronicleEntry& entry);

struct FrontPage {
    std::size_t entry = 0; // Index in world.chronicle
    std::string occasion;  // Kennung des Anlasses
    std::int32_t weight = 0;
    bool involved = false;  // Leser beteiligt: Staat des Eintrags, ein Staat im Muster, Eintrag ohne Staat
    std::string paper;      // Name des Blattes
    std::string leaning;    // Richtung des Blattes
    std::int32_t issue = 0; // Nummer der Ausgabe (laufend im Jahr)
    Date date;
    std::string headline;
    std::string subline;
    std::string text;                    // Leitartikel; Absätze durch "\n\n" getrennt
    std::string report;                  // Wortlaut des Chronikeintrags (Drahtbericht)
    std::vector<std::string> miscellany; // Vermischtes: andere Einträge der Tage davor (neueste zuerst)
};

/// Titelseite zum Chronikeintrag entry für den Leser reader (Spielerstaat, kNone: überregional);
/// nullopt ohne Zeitung, ohne passenden Anlass oder mit Gewicht 0.
std::optional<FrontPage> front_page(const World& world, std::size_t entry, Index reader);

/// Platzhalter {name} in text ersetzen; unbekannte bleiben stehen.
std::string fill_placeholders(const std::string& text, const NewspaperCaptures& values);

} // namespace uhrwerk
