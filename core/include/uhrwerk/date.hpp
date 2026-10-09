#pragma once

// Kalender der Simulation. 1 Tick = 1 Tag.
//
// Das Spiel rechnet durchgehend im (proleptischen) gregorianischen Kalender.
// Hinweis für Inhalte: Die evangelischen Reichsstände nutzten bis 1700 den
// julianischen Kalender; historische Daten in data/ werden gregorianisch
// angegeben (z. B. Westfälischer Frieden: 24.10.1648).

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace uhrwerk {

struct Date {
    std::int32_t year = 1;
    std::int32_t month = 1; // 1..12
    std::int32_t day = 1;   // 1..31

    friend constexpr auto operator<=>(const Date&, const Date&) = default;
};

bool is_leap_year(std::int32_t year);
int days_in_month(std::int32_t year, std::int32_t month);
bool is_valid(const Date& date);

/// Folgetag.
Date next_day(const Date& date);

/// Fortlaufende Tagesnummer (Tag 0 = 1. Januar des Jahres 1), für Abstände.
std::int64_t to_day_number(const Date& date);
Date from_day_number(std::int64_t day_number);

/// Liest das Skriptformat "JJJJ.MM.TT" (z. B. "1648.10.24"); führende Nullen erlaubt.
std::optional<Date> parse_date(std::string_view text);

/// "1648.10.24" – Format des Chronik-Skripts.
std::string format_script_date(const Date& date);
/// "24.10.1648"
std::string format_date(const Date& date);
/// "24. Oktober 1648"
std::string format_date_long(const Date& date);
/// "Oktober"
std::string_view month_name(std::int32_t month);

} // namespace uhrwerk
