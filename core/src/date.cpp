#include "uhrwerk/date.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace uhrwerk {

namespace {

constexpr std::int32_t kMinYear = 1;
constexpr std::int32_t kMaxYear = 9999;

constexpr std::int64_t kDaysPer400Years = 146097;
constexpr std::int64_t kDaysPer100Years = 36524; // ohne den Schalttag des 400. Jahres
constexpr std::int64_t kDaysPer4Years = 1461;

constexpr std::array<int, 12> kDaysInMonth = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

constexpr std::array<std::string_view, 12> kMonthNames = {
    "Januar", "Februar", "März",      "April",   "Mai",      "Juni",
    "Juli",   "August",  "September", "Oktober", "November", "Dezember",
};

constexpr std::int64_t floor_div(std::int64_t a, std::int64_t b) {
    std::int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) {
        --q;
    }
    return q;
}

std::int64_t days_before_month(std::int32_t year, std::int32_t month) {
    std::int64_t days = 0;
    for (std::int32_t m = 1; m < month && m <= 12; ++m) {
        days += days_in_month(year, m);
    }
    return days;
}

std::string pad(std::int64_t value, std::size_t width) {
    std::string text = std::to_string(value < 0 ? -value : value);
    if (text.size() < width) {
        text.insert(0, width - text.size(), '0');
    }
    if (value < 0) {
        text.insert(0, 1, '-');
    }
    return text;
}

// Liest eine Ziffernfolge (1–9 Ziffern) ab pos.
std::optional<std::int32_t> read_number(std::string_view text, std::size_t& pos) {
    const std::size_t begin = pos;
    std::int32_t value = 0;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
        if (pos - begin >= 9) {
            return std::nullopt;
        }
        value = value * 10 + (text[pos] - '0');
        ++pos;
    }
    if (pos == begin) {
        return std::nullopt;
    }
    return value;
}

} // namespace

bool is_leap_year(std::int32_t year) {
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int days_in_month(std::int32_t year, std::int32_t month) {
    if (month < 1 || month > 12) {
        return 0;
    }
    if (month == 2 && is_leap_year(year)) {
        return 29;
    }
    return kDaysInMonth[static_cast<std::size_t>(month - 1)];
}

bool is_valid(const Date& date) {
    return date.year >= kMinYear && date.year <= kMaxYear && date.month >= 1 && date.month <= 12 &&
           date.day >= 1 && date.day <= days_in_month(date.year, date.month);
}

Date next_day(const Date& date) {
    Date next = date;
    if (next.day < days_in_month(next.year, next.month)) {
        ++next.day;
        return next;
    }
    next.day = 1;
    if (next.month < 12) {
        ++next.month;
        return next;
    }
    next.month = 1;
    ++next.year;
    return next;
}

std::int64_t to_day_number(const Date& date) {
    const std::int64_t y = static_cast<std::int64_t>(date.year) - 1;
    return y * 365 + floor_div(y, 4) - floor_div(y, 100) + floor_div(y, 400) +
           days_before_month(date.year, date.month) + (static_cast<std::int64_t>(date.day) - 1);
}

Date from_day_number(std::int64_t day_number) {
    // Zerlegung in 400-, 100-, 4- und 1-Jahres-Zyklen ab dem 1. Januar des Jahres 1.
    const std::int64_t cycles = floor_div(day_number, kDaysPer400Years);
    std::int64_t rest = day_number - cycles * kDaysPer400Years; // 0 … 146096

    std::int64_t centuries = rest / kDaysPer100Years;
    if (centuries > 3) {
        centuries = 3; // letzter Tag des 400. Jahres (Schalttag)
    }
    rest -= centuries * kDaysPer100Years;

    const std::int64_t quads = rest / kDaysPer4Years;
    rest -= quads * kDaysPer4Years;

    std::int64_t years = rest / 365;
    if (years > 3) {
        years = 3; // 31. Dezember eines Schaltjahres
    }
    rest -= years * 365;

    Date date;
    date.year = static_cast<std::int32_t>(cycles * 400 + centuries * 100 + quads * 4 + years + 1);
    date.month = 1;
    while (date.month < 12 && rest >= days_in_month(date.year, date.month)) {
        rest -= days_in_month(date.year, date.month);
        ++date.month;
    }
    date.day = static_cast<std::int32_t>(rest) + 1;
    return date;
}

std::optional<Date> parse_date(std::string_view text) {
    std::size_t pos = 0;
    const auto year = read_number(text, pos);
    if (!year || pos >= text.size() || text[pos] != '.') {
        return std::nullopt;
    }
    ++pos;
    const auto month = read_number(text, pos);
    if (!month || pos >= text.size() || text[pos] != '.') {
        return std::nullopt;
    }
    ++pos;
    const auto day = read_number(text, pos);
    if (!day || pos != text.size()) {
        return std::nullopt;
    }
    const Date date{*year, *month, *day};
    if (!is_valid(date)) {
        return std::nullopt;
    }
    return date;
}

std::string format_script_date(const Date& date) {
    return pad(date.year, 4) + '.' + pad(date.month, 2) + '.' + pad(date.day, 2);
}

std::string format_date(const Date& date) {
    return pad(date.day, 2) + '.' + pad(date.month, 2) + '.' + std::to_string(date.year);
}

std::string format_date_long(const Date& date) {
    return std::to_string(date.day) + ". " + std::string(month_name(date.month)) + ' ' +
           std::to_string(date.year);
}

std::string_view month_name(std::int32_t month) {
    if (month < 1 || month > 12) {
        return {};
    }
    return kMonthNames[static_cast<std::size_t>(month - 1)];
}

} // namespace uhrwerk
