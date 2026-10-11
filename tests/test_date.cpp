// Tests für den Kalender.

#include "test_hilfen.hpp"
#include "uhrwerk/date.hpp"

#include <cstdint>
#include <doctest/doctest.h>
#include <string>

using uhrwerk::Date;

TEST_CASE("Datum: Schaltjahre im gregorianischen Kalender") {
    CHECK(uhrwerk::is_leap_year(1600));
    CHECK_FALSE(uhrwerk::is_leap_year(1700));
    CHECK_FALSE(uhrwerk::is_leap_year(1800));
    CHECK_FALSE(uhrwerk::is_leap_year(1900));
    CHECK(uhrwerk::is_leap_year(2000));
    CHECK(uhrwerk::is_leap_year(1648));
    CHECK_FALSE(uhrwerk::is_leap_year(1649));
    CHECK(uhrwerk::is_leap_year(2024));
    CHECK_FALSE(uhrwerk::is_leap_year(2100));

    CHECK(uhrwerk::days_in_month(1700, 2) == 28);
    CHECK(uhrwerk::days_in_month(1600, 2) == 29);
    CHECK(uhrwerk::days_in_month(2000, 2) == 29);
    CHECK(uhrwerk::days_in_month(1649, 2) == 28);
    CHECK(uhrwerk::days_in_month(1648, 1) == 31);
    CHECK(uhrwerk::days_in_month(1648, 4) == 30);
    CHECK(uhrwerk::days_in_month(1648, 12) == 31);
    CHECK(uhrwerk::days_in_month(1648, 0) == 0);
    CHECK(uhrwerk::days_in_month(1648, 13) == 0);
}

TEST_CASE("Datum: Gültigkeit") {
    CHECK(uhrwerk::is_valid(Date{1648, 10, 24}));
    CHECK(uhrwerk::is_valid(Date{1600, 2, 29}));
    CHECK(uhrwerk::is_valid(Date{2000, 2, 29}));
    CHECK_FALSE(uhrwerk::is_valid(Date{1700, 2, 29}));
    CHECK_FALSE(uhrwerk::is_valid(Date{1648, 4, 31}));
    CHECK_FALSE(uhrwerk::is_valid(Date{1648, 13, 1}));
    CHECK_FALSE(uhrwerk::is_valid(Date{1648, 0, 10}));
    CHECK_FALSE(uhrwerk::is_valid(Date{1648, 5, 0}));
    CHECK_FALSE(uhrwerk::is_valid(Date{1648, 5, -1}));
    CHECK_FALSE(uhrwerk::is_valid(Date{0, 1, 1}));
    CHECK_FALSE(uhrwerk::is_valid(Date{10000, 1, 1}));
    CHECK(uhrwerk::is_valid(Date{1, 1, 1}));
    CHECK(uhrwerk::is_valid(Date{9999, 12, 31}));
}

TEST_CASE("Datum: Vergleich") {
    CHECK(Date{1648, 10, 24} < Date{1648, 10, 25});
    CHECK(Date{1648, 10, 24} < Date{1648, 11, 1});
    CHECK(Date{1648, 12, 31} < Date{1649, 1, 1});
    CHECK(Date{1648, 10, 24} == Date{1648, 10, 24});
}

TEST_CASE("Datum: Tages- und Monatswechsel") {
    CHECK(uhrwerk::next_day(Date{1648, 10, 24}) == Date{1648, 10, 25});
    CHECK(uhrwerk::next_day(Date{1648, 10, 31}) == Date{1648, 11, 1});
    CHECK(uhrwerk::next_day(Date{1648, 11, 30}) == Date{1648, 12, 1});
    CHECK(uhrwerk::next_day(Date{1648, 12, 31}) == Date{1649, 1, 1});
    CHECK(uhrwerk::next_day(Date{1700, 2, 28}) == Date{1700, 3, 1});
    CHECK(uhrwerk::next_day(Date{1600, 2, 28}) == Date{1600, 2, 29});
    CHECK(uhrwerk::next_day(Date{1600, 2, 29}) == Date{1600, 3, 1});
    CHECK(uhrwerk::next_day(Date{2000, 2, 29}) == Date{2000, 3, 1});
    CHECK(uhrwerk::next_day(Date{1649, 2, 28}) == Date{1649, 3, 1});
    CHECK(uhrwerk::next_day(Date{1648, 2, 28}) == Date{1648, 2, 29});
}

TEST_CASE("Datum: Tagesnummern") {
    CHECK(uhrwerk::to_day_number(Date{1, 1, 1}) == 0);
    CHECK(uhrwerk::to_day_number(Date{1, 1, 2}) == 1);
    CHECK(uhrwerk::to_day_number(Date{2, 1, 1}) == 365);
    CHECK(uhrwerk::to_day_number(Date{5, 1, 1}) == 4 * 365 + 1); // Jahr 4 ist Schaltjahr
    CHECK(uhrwerk::to_day_number(Date{401, 1, 1}) == 146097);    // genau ein 400-Jahres-Zyklus
    CHECK(uhrwerk::from_day_number(0) == Date{1, 1, 1});
    CHECK(uhrwerk::from_day_number(146096) == Date{400, 12, 31});
    CHECK(uhrwerk::from_day_number(146097) == Date{401, 1, 1});
    CHECK(uhrwerk::from_day_number(36523) == Date{100, 12, 31}); // Jahr 100 ist kein Schaltjahr
    CHECK(uhrwerk::from_day_number(36524) == Date{101, 1, 1});

    // Abstände: 50 Jahre ab dem Westfälischen Frieden enthalten 12 Schalttage.
    CHECK(uhrwerk::to_day_number(Date{1698, 10, 24}) - uhrwerk::to_day_number(Date{1648, 10, 24}) ==
          50 * 365 + 12);
    CHECK(uhrwerk::to_day_number(Date{1701, 1, 1}) - uhrwerk::to_day_number(Date{1700, 1, 1}) == 365);
    CHECK(uhrwerk::to_day_number(Date{2001, 1, 1}) - uhrwerk::to_day_number(Date{2000, 1, 1}) == 366);
    // 1. Januar 1970 (Unix-Epoche) liegt 719162 Tage nach dem 1. Januar 1.
    CHECK(uhrwerk::to_day_number(Date{1970, 1, 1}) == 719162);
}

TEST_CASE("Datum: Rundreise der Tagesnummer 1600–2100") {
    const std::int64_t first = uhrwerk::to_day_number(Date{1600, 1, 1});
    const std::int64_t last = uhrwerk::to_day_number(Date{2100, 12, 31});
    Date expected{1600, 1, 1};
    std::int64_t count = 0;
    for (std::int64_t n = first; n <= last; ++n) {
        const Date d = uhrwerk::from_day_number(n);
        if (d != expected || uhrwerk::to_day_number(d) != n || !uhrwerk::is_valid(d)) {
            FAIL("Abweichung bei Tagesnummer " << n << ": " << uhrwerk::format_script_date(d) << " statt "
                                               << uhrwerk::format_script_date(expected));
        }
        expected = uhrwerk::next_day(expected);
        ++count;
    }
    CHECK(expected == Date{2101, 1, 1});
    CHECK(count == 501 * 365 + 122); // 1600–2100: 122 Schaltjahre
}

TEST_CASE("Datum: Rundreise über den ganzen gültigen Bereich (grob)") {
    for (std::int64_t n = 0; n <= uhrwerk::to_day_number(Date{9999, 12, 31}); n += 997) {
        const Date d = uhrwerk::from_day_number(n);
        REQUIRE(uhrwerk::is_valid(d));
        CHECK(uhrwerk::to_day_number(d) == n);
    }
}

TEST_CASE("Datum: parse_date") {
    CHECK(uhrwerk::parse_date("1648.10.24") == Date{1648, 10, 24});
    CHECK(uhrwerk::parse_date("1648.01.05") == Date{1648, 1, 5});
    CHECK(uhrwerk::parse_date("0800.12.25") == Date{800, 12, 25});
    CHECK(uhrwerk::parse_date("1600.02.29") == Date{1600, 2, 29});
    CHECK(uhrwerk::parse_date("0001.01.01") == Date{1, 1, 1});
    CHECK(uhrwerk::parse_date("9999.12.31") == Date{9999, 12, 31});
    // Streng JJJJ.MM.TT wie in der Spezifikation 1.5 und im Archivar: genau 4, 2 und 2 Ziffern, Jahr ab 1.
    for (const char* text : {"",           "1648",        "1648.10",           "1648.10.",    "1648.10.24.",
                             "1648-10-24", "24.10.1648x", "1700.02.29",        "1648.13.01",  "1648.00.10",
                             "1648.04.31", "abcd.10.24",  " 1648.10.24",       "1648.10.24 ", "-1648.10.24",
                             "1648.-1.10", "+1648.10.24", "99999999999.01.01", "10000.01.01", "0.01.01",
                             "1648..24",   "1648.1.5",    "1701.1.18",         "1648.01.5",   "1648.1.05",
                             "800.12.25",  "01648.10.24", "1648.010.24",       "1648.10.024", "0000.01.01"}) {
        INFO("Eingabe: '" << text << "'");
        CHECK_FALSE(uhrwerk::parse_date(text).has_value());
    }
}

TEST_CASE("Datum: Formatierung") {
    CHECK(uhrwerk::format_script_date(Date{1648, 10, 24}) == "1648.10.24");
    CHECK(uhrwerk::format_script_date(Date{1648, 1, 5}) == "1648.01.05");
    CHECK(uhrwerk::format_script_date(Date{800, 1, 5}) == "0800.01.05");
    CHECK(uhrwerk::format_date(Date{1648, 10, 24}) == "24.10.1648");
    CHECK(uhrwerk::format_date(Date{1648, 1, 5}) == "05.01.1648");
    CHECK(uhrwerk::format_date_long(Date{1648, 10, 24}) == "24. Oktober 1648");
    CHECK(uhrwerk::format_date_long(Date{1871, 1, 18}) == "18. Januar 1871");
    CHECK(uhrwerk::format_date_long(Date{1990, 3, 1}) == "1. März 1990");

    const char* const months[] = {"Januar", "Februar", "März",      "April",   "Mai",      "Juni",
                                  "Juli",   "August",  "September", "Oktober", "November", "Dezember"};
    for (int m = 1; m <= 12; ++m) {
        CHECK(uhrwerk::month_name(m) == months[m - 1]);
    }
    CHECK(uhrwerk::month_name(0).empty());
    CHECK(uhrwerk::month_name(13).empty());

    // Rundreise Skriptformat
    for (const Date d : {Date{1648, 10, 24}, Date{1600, 2, 29}, Date{1, 1, 1}, Date{9999, 12, 31}}) {
        CHECK(uhrwerk::parse_date(uhrwerk::format_script_date(d)) == d);
    }
}
