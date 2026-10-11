// Tests für die Festkommazahl Fixed (Q47.16).

#include "test_hilfen.hpp"
#include "uhrwerk/fixed.hpp"
#include "uhrwerk/rng.hpp"

#include <cstdint>
#include <doctest/doctest.h>
#include <limits>
#include <optional>
#include <string>

using uhrwerk::Fixed;
using uhrwerk::Pcg32;

namespace {

constexpr Fixed raw(std::int64_t r) {
    return Fixed::from_raw(r);
}
constexpr Fixed half() {
    return Fixed::from_ratio(1, 2);
}

constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();

/// Zufälliger Rohwert mit Betrag < 2^bits und zufälligem Vorzeichen.
std::int64_t random_raw(Pcg32& rng) {
    const auto bits = static_cast<int>(rng.below(62)) + 1; // 1..62
    std::uint64_t v = (std::uint64_t{rng.next()} << 32) | rng.next();
    v &= (std::uint64_t{1} << bits) - 1;
    const auto value = static_cast<std::int64_t>(v);
    return (rng.next() & 1u) != 0 ? -value : value;
}

} // namespace

// ─── constexpr-Nutzung (wird beim Kompilieren geprüft) ──────────────────────

static_assert(Fixed::kOne == 65536);
static_assert(Fixed::from_int(3).raw() == 3 * 65536);
static_assert(Fixed::from_ratio(1, 2).raw() == 32768);
static_assert(Fixed::from_int(3) * half() == Fixed::from_ratio(3, 2));
static_assert(Fixed::from_int(7) / Fixed::from_int(2) == Fixed::from_ratio(7, 2));
static_assert((Fixed::from_int(-7) / Fixed::from_int(2)).raw() == -(3 * 65536 + 32768));
static_assert(Fixed::from_ratio(5, 2).round() == 3);
static_assert(Fixed::from_ratio(-5, 2).round() == -3);
static_assert(Fixed::from_ratio(-5, 2).trunc() == -2);
static_assert(Fixed::one().clamp(Fixed::zero(), half()) == half());
static_assert(Fixed::min(Fixed::one(), half()) == half());
static_assert(Fixed::max(Fixed::one(), half()) == Fixed::one());
static_assert(-Fixed::one() < Fixed::zero());
static_assert([] {
    Fixed f = Fixed::from_int(10);
    f += Fixed::one();
    f -= half();
    f *= Fixed::from_int(2);
    f /= Fixed::from_int(3);
    return f == Fixed::from_int(7);
}());

// ─── Grundrechenarten ───────────────────────────────────────────────────────

TEST_CASE("Fixed: Grundrechenarten und Vergleiche") {
    const Fixed a = Fixed::from_int(5);
    const Fixed b = Fixed::from_ratio(3, 4);
    CHECK((a + b).raw() == 5 * 65536 + 49152);
    CHECK((a - b).raw() == 5 * 65536 - 49152);
    CHECK((b - a).raw() == -(5 * 65536 - 49152));
    CHECK(-a == Fixed::from_int(-5));
    CHECK(a * b == Fixed::from_ratio(15, 4));
    CHECK(a / b == Fixed::from_raw((5 * 65536 * 4) / 3)); // 6,666… abgeschnitten
    CHECK(b < a);
    CHECK(a > b);
    CHECK(a == Fixed::from_int(5));
    CHECK(a != b);
    CHECK(Fixed{} == Fixed::zero());
    CHECK(Fixed::one().raw() == 65536);
    CHECK(Fixed::from_int(-2).clamp(Fixed::zero(), Fixed::one()) == Fixed::zero());
    CHECK(Fixed::from_int(2).clamp(Fixed::zero(), Fixed::one()) == Fixed::one());
}

TEST_CASE("Fixed: Multiplikation rundet Richtung minus unendlich") {
    CHECK((raw(1) * half()).raw() == 0);   //  0,5 → 0
    CHECK((raw(-1) * half()).raw() == -1); // −0,5 → −1
    CHECK((raw(3) * half()).raw() == 1);   //  1,5 → 1
    CHECK((raw(-3) * half()).raw() == -2); // −1,5 → −2
    CHECK((raw(-4) * half()).raw() == -2); // exakt, keine Rundung
    CHECK((raw(-3) * -half()).raw() == 1); // positiv: abrunden
    CHECK((raw(3) * -half()).raw() == -2);
    CHECK((raw(-1) * raw(1)).raw() == -1); // −1/65536 → −1 Rohwert
    CHECK((raw(1) * raw(1)).raw() == 0);
    CHECK((Fixed::zero() * Fixed::from_int(-5)).raw() == 0);
    // 0,1 · 3 mit 0,1 = 6554/65536
    CHECK((Fixed::from_raw(6554) * Fixed::from_int(3)).raw() == 19662);
    CHECK((Fixed::from_raw(-6554) * Fixed::from_ratio(1, 3)).raw() == -2185); // −2184,6… → −2185
    CHECK((Fixed::from_raw(6554) * Fixed::from_ratio(1, 3)).raw() == 2184);   //  2184,6… →  2184
}

TEST_CASE("Fixed: Division schneidet Richtung null ab") {
    CHECK((Fixed::from_int(1) / Fixed::from_int(3)).raw() == 21845);   // 21845,33…
    CHECK((Fixed::from_int(-1) / Fixed::from_int(3)).raw() == -21845); // nicht −21846
    CHECK((Fixed::from_int(2) / Fixed::from_int(3)).raw() == 43690);   // 43690,67… → 43690
    CHECK((Fixed::from_int(-2) / Fixed::from_int(3)).raw() == -43690);
    CHECK((Fixed::from_int(2) / Fixed::from_int(-3)).raw() == -43690);
    CHECK((Fixed::from_int(-2) / Fixed::from_int(-3)).raw() == 43690);
    CHECK((raw(1) / Fixed::from_int(2)).raw() == 0);
    CHECK((raw(-1) / Fixed::from_int(2)).raw() == 0);
    CHECK(Fixed::from_int(5) / Fixed::zero() == Fixed::zero());
    CHECK(Fixed::from_int(-5) / Fixed::zero() == Fixed::zero());
}

TEST_CASE("Fixed: from_ratio und round runden kaufmännisch") {
    CHECK(Fixed::from_ratio(1, 3).raw() == 21845);
    CHECK(Fixed::from_ratio(2, 3).raw() == 43691); // 43690,67 → 43691
    CHECK(Fixed::from_ratio(-2, 3).raw() == -43691);
    CHECK(Fixed::from_ratio(2, -3).raw() == -43691);
    CHECK(Fixed::from_ratio(-2, -3).raw() == 43691);
    CHECK(Fixed::from_ratio(1, 131072).raw() == 1);   // genau ½ Rohwert → weg von null
    CHECK(Fixed::from_ratio(-1, 131072).raw() == -1); // genau −½ Rohwert → weg von null
    CHECK(Fixed::from_ratio(1, 131073).raw() == 0);
    CHECK(Fixed::from_ratio(3, -4) == -Fixed::from_ratio(3, 4));
    CHECK(Fixed::from_ratio(0, 7) == Fixed::zero());

    CHECK(Fixed::from_ratio(1, 2).round() == 1);
    CHECK(Fixed::from_ratio(-1, 2).round() == -1);
    CHECK(raw(32767).round() == 0);
    CHECK(raw(-32767).round() == 0);
    CHECK(Fixed::from_ratio(7, 4).round() == 2);
    CHECK(Fixed::from_ratio(-7, 4).round() == -2);
    CHECK(Fixed::from_ratio(7, 4).trunc() == 1);
    CHECK(Fixed::from_ratio(-7, 4).trunc() == -1);
}

TEST_CASE("Fixed: große Werte") {
    const Fixed big = Fixed::from_int(100'000'000'000'000); // 10^14, nahe am Maximum ≈ 1,4·10^14
    CHECK(big.trunc() == 100'000'000'000'000);
    CHECK(big.round() == 100'000'000'000'000);
    CHECK(big * Fixed::one() == big);
    CHECK(big / Fixed::one() == big);
    CHECK(big / Fixed::from_int(1000) == Fixed::from_int(100'000'000'000));
    CHECK(Fixed::from_int(10'000'000) * Fixed::from_int(10'000'000) == big);
    CHECK(-big * Fixed::from_int(1) == -big);
    const Fixed half_big = Fixed::from_int(50'000'000'000'000);
    CHECK((half_big + half_big) / Fixed::from_int(2) == half_big);
    CHECK(big * half() == Fixed::from_int(50'000'000'000'000));
    CHECK(Fixed::from_int(std::int64_t{1} << 45) * Fixed::from_int(2) ==
          Fixed::from_int(std::int64_t{1} << 46));

    // Kleinste Auflösung bei großem Faktor.
    CHECK(raw(1) * big == Fixed::from_raw(100'000'000'000'000));
    CHECK(raw(-1) * big == Fixed::from_raw(-100'000'000'000'000));

    // Extremwerte des Rohformats.
    CHECK(raw(kMax).trunc() == (kMax >> 16));
    CHECK(raw(kMax) * Fixed::one() == raw(kMax));
    CHECK(raw(kMax) / Fixed::one() == raw(kMax));
    CHECK(raw(kMin + 1) * Fixed::one() == raw(kMin + 1));
    CHECK(raw(kMax) / raw(kMax) == Fixed::one());
    CHECK(raw(kMin + 1) / raw(kMax) == -Fixed::one());
}

// ─── parse ──────────────────────────────────────────────────────────────────

TEST_CASE("Fixed::parse liest Dezimalzahlen") {
    CHECK(Fixed::parse("12") == Fixed::from_int(12));
    CHECK(Fixed::parse("-0.25")->raw() == -16384);
    CHECK(Fixed::parse("3.14159")->raw() == 205887);
    CHECK(Fixed::parse("0.5")->raw() == 32768);
    CHECK(Fixed::parse("0.1")->raw() == 6554);
    CHECK(Fixed::parse("0.10")->raw() == 6554);
    CHECK(Fixed::parse("0.004")->raw() == 262);
    CHECK(Fixed::parse("1.15")->raw() == 75366);
    CHECK(Fixed::parse("-2.5")->raw() == -163840);
    CHECK(Fixed::parse("007") == Fixed::from_int(7));
    CHECK(Fixed::parse("0") == Fixed::zero());
    CHECK(Fixed::parse("-0") == Fixed::zero());
    CHECK(Fixed::parse("-0.0000001") == Fixed::zero()); // rundet auf 0, kein "−0"
    CHECK(Fixed::parse("12000") == Fixed::from_int(12000));
}

TEST_CASE("Fixed::parse rundet kaufmännisch, auch bei langen Nachkommastellen") {
    // 0.00000762939453125 = genau ½ / 65536
    CHECK(Fixed::parse("0.00000762939453125")->raw() == 1);
    CHECK(Fixed::parse("-0.00000762939453125")->raw() == -1);
    CHECK(Fixed::parse("0.0000076293945312")->raw() == 0); // knapp unter ½
    CHECK(Fixed::parse("0.00000762939453125000000000001")->raw() == 1);
    CHECK(Fixed::parse("0.99999999999999999999999")->raw() == 65536); // Übertrag in den ganzen Teil
    CHECK(Fixed::parse("1.99999999999999999999999") == Fixed::from_int(2));
    CHECK(Fixed::parse("0.000000000000000000000000000000000000000001") == Fixed::zero());
}

TEST_CASE("Fixed::parse an den Grenzen des Wertebereichs") {
    CHECK(Fixed::parse("140737488355327.9999847412109375")->raw() == kMax);
    CHECK(Fixed::parse("-140737488355327.9999847412109375")->raw() == -kMax);
    CHECK(Fixed::parse("140737488355327")->raw() == 140737488355327LL * 65536);
    CHECK_FALSE(Fixed::parse("140737488355328").has_value());          // 2^47 passt nicht
    CHECK(Fixed::parse("140737488355327.99999")->raw() == kMax);       // 65535,34 → 65535
    CHECK_FALSE(Fixed::parse("140737488355327.99999999").has_value()); // rundet über das Maximum
    CHECK_FALSE(Fixed::parse("99999999999999999999999").has_value());
    CHECK_FALSE(Fixed::parse("-99999999999999999999999").has_value());
}

TEST_CASE("Fixed::parse lehnt ungültige Eingaben ab") {
    for (const char* text :
         {"",    "-",     "+1",  "1.",   ".5", "-.5",  "1,5",      "1e5",   " 1",  "1 ",
          "--1", "1.2.3", "abc", "0x10", "1-", "1.-5", "\xD9\xA1", "1_000", "NaN", "inf"}) {
        INFO("Eingabe: '" << text << "'");
        CHECK_FALSE(Fixed::parse(text).has_value());
    }
}

// ─── to_string ──────────────────────────────────────────────────────────────

TEST_CASE("Fixed::to_string") {
    CHECK(Fixed::from_ratio(25, 2).to_string() == "12.50");
    CHECK(Fixed::from_int(-3).to_string(0) == "-3");
    CHECK(Fixed::from_int(0).to_string(3) == "0.000");
    CHECK(Fixed::from_ratio(5, 2).to_string(0) == "3");   // kaufmännisch
    CHECK(Fixed::from_ratio(-5, 2).to_string(0) == "-3"); // weg von null
    CHECK(Fixed::parse("0.125")->to_string(2) == "0.13");
    CHECK(Fixed::parse("-0.125")->to_string(2) == "-0.13");
    CHECK(Fixed::parse("0.999")->to_string(2) == "1.00");
    CHECK(Fixed::parse("-0.999")->to_string(2) == "-1.00");
    CHECK(raw(-1).to_string(2) == "0.00"); // keine negative Null
    CHECK(raw(1).to_string(6) == "0.000015");
    CHECK(Fixed::from_ratio(1, 3).to_string(6) == "0.333328");
    CHECK(Fixed::parse("1234.5")->to_string(1) == "1234.5");
    CHECK(raw(kMax).to_string(6) == "140737488355327.999985");
    CHECK(raw(kMin).to_string(0) == "-140737488355328");
    // decimals wird auf 0..6 begrenzt
    CHECK(Fixed::from_ratio(1, 3).to_string(10) == Fixed::from_ratio(1, 3).to_string(6));
    CHECK(Fixed::from_ratio(7, 2).to_string(-1) == "4");
}

TEST_CASE("Fixed: parse und to_string sind zueinander verlustfrei") {
    Pcg32 rng(20240611u, 7u);
    for (int i = 0; i < 20000; ++i) {
        const Fixed value = Fixed::from_raw(random_raw(rng));
        const std::string text = value.to_string(6);
        const auto back = Fixed::parse(text);
        INFO("Rohwert " << value.raw() << " → '" << text << "'");
        REQUIRE(back.has_value());
        CHECK(*back == value);
    }
    for (std::int64_t r = -70000; r <= 70000; r += 7) {
        CHECK(Fixed::parse(raw(r).to_string(6)) == raw(r));
    }
    for (const char* text : {"0.25", "-0.5", "12.75", "1648", "-3.125"}) {
        CHECK(Fixed::parse(Fixed::parse(text)->to_string(3))->to_string(3) ==
              Fixed::parse(text)->to_string(3));
    }
}

// ─── Vergleich mit 128-Bit-Arithmetik ───────────────────────────────────────

#ifdef __SIZEOF_INT128__
__extension__ typedef __int128 Int128;
__extension__ typedef unsigned __int128 UInt128;

TEST_CASE("Fixed: Multiplikation und Division stimmen mit __int128 überein") {
    constexpr Int128 kLimit = Int128{1} << 63;
    Pcg32 rng(0x5eed, 42);
    int mul_checked = 0;
    int div_checked = 0;
    for (int i = 0; i < 200000; ++i) {
        const std::int64_t a = random_raw(rng);
        const std::int64_t b = random_raw(rng);

        // Exaktes Produkt / 65536, abgerundet (arithmetischer Rechtsshift = floor).
        const Int128 product = (Int128{a} * Int128{b}) >> 16;
        if (product >= -kLimit + 1 && product < kLimit) {
            INFO("a=" << a << " b=" << b);
            CHECK((raw(a) * raw(b)).raw() == static_cast<std::int64_t>(product));
            ++mul_checked;
        }

        // Exakter Quotient a·65536 / b, abgeschnitten (C++-Division).
        if (b != 0) {
            const Int128 quotient = (Int128{a} * 65536) / Int128{b};
            if (quotient > -kLimit && quotient < kLimit) {
                INFO("a=" << a << " b=" << b);
                CHECK((raw(a) / raw(b)).raw() == static_cast<std::int64_t>(quotient));
                ++div_checked;
            }
        }
    }
    // Genügend Stichproben liegen im darstellbaren Bereich.
    CHECK(mul_checked > 50000);
    CHECK(div_checked > 50000);
}

TEST_CASE("Fixed: 128-Bit-Hilfsfunktionen stimmen mit unsigned __int128 überein") {
    Pcg32 rng(99, 1);
    for (int i = 0; i < 50000; ++i) {
        const std::uint64_t a = (std::uint64_t{rng.next()} << 32) | rng.next();
        const std::uint64_t b = (std::uint64_t{rng.next()} << 32) | rng.next();
        const UInt128 expected = UInt128{a} * UInt128{b};
        const uhrwerk::detail::U128 p = uhrwerk::detail::mul_u64(a, b);
        CHECK(p.hi == static_cast<std::uint64_t>(expected >> 64));
        CHECK(p.lo == static_cast<std::uint64_t>(expected));

        // Division nur, wenn der Quotient in 64 Bit passt (hi < d).
        const std::uint64_t d = b | 1u;
        const uhrwerk::detail::U128 n{p.hi % d, p.lo};
        const UInt128 numerator = (UInt128{n.hi} << 64) | n.lo;
        std::uint64_t remainder = 0;
        const std::uint64_t q = uhrwerk::detail::div_u128_by_u64(n, d, &remainder);
        CHECK(q == static_cast<std::uint64_t>(numerator / d));
        CHECK(remainder == static_cast<std::uint64_t>(numerator % d));
    }
}
#endif

// ─── Schnellwege (seit M4) gegen die bitweise Referenz ──────────────────────
// Ohne __int128, damit die Prüfung auch unter MSVC läuft.

TEST_CASE("Fixed: Division 128/64 – alle Wege stimmen mit der bitweisen Methode überein") {
    using uhrwerk::detail::div_u128_by_u64;
    using uhrwerk::detail::div_u128_by_u64_bitwise;
    using uhrwerk::detail::U128;

    const auto check = [](U128 n, std::uint64_t d) {
        std::uint64_t r_fast = 0;
        std::uint64_t r_ref = 0;
        const std::uint64_t q_fast = div_u128_by_u64(n, d, &r_fast);
        const std::uint64_t q_ref = div_u128_by_u64_bitwise(n, d, &r_ref);
        INFO("n=" << n.hi << ":" << n.lo << " d=" << d);
        CHECK(q_fast == q_ref);
        CHECK(r_fast == r_ref);
    };

    // Grenzfälle: Divisor mit und ohne gesetztes oberstes Bit, Zähler knapp unter d·2^64.
    constexpr std::uint64_t kAll = ~std::uint64_t{0};
    for (const std::uint64_t d : {std::uint64_t{1}, std::uint64_t{2}, std::uint64_t{3}, std::uint64_t{65536},
                                  std::uint64_t{0xFFFFFFFFu}, std::uint64_t{1} << 32,
                                  (std::uint64_t{1} << 32) + 1, std::uint64_t{1} << 63, kAll - 1, kAll}) {
        for (const std::uint64_t lo : {std::uint64_t{0}, std::uint64_t{1}, d - 1, d, kAll}) {
            check(U128{0, lo}, d);
            check(U128{d - 1, lo}, d);
            check(U128{(d - 1) / 2, lo}, d);
            check(U128{d, lo}, d); // Quotient passt nicht in 64 Bit: bitweiser Weg
            check(U128{kAll, lo}, d);
        }
    }

    // Zufällige Stichproben über alle Größenordnungen von Zähler und Divisor.
    Pcg32 rng(2024, 7);
    const auto random_u64 = [&rng] {
        const auto bits = static_cast<int>(rng.below(64)) + 1; // 1..64
        const std::uint64_t v = (std::uint64_t{rng.next()} << 32) | rng.next();
        return bits == 64 ? v : v & ((std::uint64_t{1} << bits) - 1);
    };
    for (int i = 0; i < 100000; ++i) {
        const std::uint64_t d = random_u64() | 1u;
        const std::uint64_t hi = random_u64();
        check(U128{hi % d, random_u64()}, d); // Quotient passt in 64 Bit
        check(U128{0, random_u64()}, d);
    }
}

TEST_CASE("Fixed: Schnellweg der Multiplikation an der Grenze 2^32") {
    // Beide Beträge < 2^32 → 64-Bit-Produkt; sonst 128 Bit. Ergebnis muss gleich sein wie
    // die Rechnung über mul_u64.
    const auto reference = [](std::int64_t a, std::int64_t b) {
        const bool negative = (a < 0) != (b < 0);
        const uhrwerk::detail::U128 p =
            uhrwerk::detail::mul_u64(uhrwerk::detail::abs_u64(a), uhrwerk::detail::abs_u64(b));
        const std::uint64_t magnitude = (p.hi << 48) | (p.lo >> 16);
        auto value = static_cast<std::int64_t>(magnitude);
        if (negative) {
            value = -value;
            if ((p.lo & 0xFFFFu) != 0) {
                --value;
            }
        }
        return value;
    };
    constexpr std::int64_t kEdge = std::int64_t{1} << 32;
    for (const std::int64_t a : {kEdge - 1, kEdge, kEdge + 1, std::int64_t{65535}, std::int64_t{65537},
                                 std::int64_t{1}, std::int64_t{0}}) {
        for (const std::int64_t b : {kEdge - 1, kEdge, std::int64_t{3}, std::int64_t{123456789}}) {
            for (const int sign : {1, -1}) {
                INFO("a=" << a * sign << " b=" << b);
                CHECK((raw(a * sign) * raw(b)).raw() == reference(a * sign, b));
                CHECK((raw(b) * raw(a * sign)).raw() == reference(b, a * sign));
            }
        }
    }
    Pcg32 rng(77, 3);
    for (int i = 0; i < 100000; ++i) {
        const std::int64_t a = random_raw(rng) >> 28; // viele Werte unter 2^32
        const std::int64_t b = random_raw(rng) >> 28;
        INFO("a=" << a << " b=" << b);
        CHECK((raw(a) * raw(b)).raw() == reference(a, b));
    }
}
