// Tests für den Zufallsgenerator Pcg32.

#include "test_hilfen.hpp"
#include "uhrwerk/rng.hpp"

#include <array>
#include <cstdint>
#include <doctest/doctest.h>
#include <set>

using uhrwerk::Fixed;
using uhrwerk::Pcg32;
using uhrwerk::RngStream;

namespace {

constexpr std::uint32_t first_output(std::uint64_t seed, std::uint64_t stream) {
    Pcg32 rng(seed, stream);
    return rng.next();
}

bool same_state(const Pcg32& a, const Pcg32& b) {
    return a.state() == b.state() && a.increment() == b.increment();
}

} // namespace

// Der Generator ist vollständig constexpr nutzbar.
static_assert(first_output(42u, 54u) == 0xa15c02b7u);

TEST_CASE("Pcg32: Referenzwerte aus pcg32-demo (pcg-c-basic)") {
    // pcg32-demo.c: pcg32_srandom_r(&rng, 42u, 54u), erste Runde "32bit:".
    Pcg32 rng(42u, 54u);
    constexpr std::array<std::uint32_t, 6> expected = {0xa15c02b7u, 0x7b47f409u, 0xba1d3330u,
                                                       0x83d2f293u, 0xbfa4784bu, 0xcbed606eu};
    for (const std::uint32_t value : expected) {
        CHECK(rng.next() == value);
    }
}

TEST_CASE("Pcg32: Seed und Strom bestimmen die Folge") {
    Pcg32 a(1648u, RngStream::Demografie);
    Pcg32 b(1648u, RngStream::Demografie);
    Pcg32 other_stream(1648u, RngStream::Ereignisse);
    Pcg32 other_seed(1649u, RngStream::Demografie);
    CHECK(same_state(a, b));
    CHECK(a.increment() == ((static_cast<std::uint64_t>(RngStream::Demografie) << 1u) | 1u));
    bool differs_stream = false;
    bool differs_seed = false;
    for (int i = 0; i < 16; ++i) {
        const std::uint32_t va = a.next();
        CHECK(va == b.next());
        differs_stream = differs_stream || va != other_stream.next();
        differs_seed = differs_seed || va != other_seed.next();
    }
    CHECK(differs_stream);
    CHECK(differs_seed);

    // RngStream-Kennungen sind Teil des Speicherformats und dürfen sich nie ändern.
    CHECK(static_cast<std::uint64_t>(RngStream::Demografie) == 1);
    CHECK(static_cast<std::uint64_t>(RngStream::Wirtschaft) == 2);
    CHECK(static_cast<std::uint64_t>(RngStream::Ereignisse) == 3);
    CHECK(static_cast<std::uint64_t>(RngStream::Ki) == 4);

    // Standardkonstruktor ist deterministisch.
    Pcg32 d1;
    Pcg32 d2;
    CHECK(d1.next() == d2.next());
}

TEST_CASE("Pcg32::below: Grenzen") {
    Pcg32 rng(7u, 3u);

    SUBCASE("bound 0 liefert 0 und verbraucht keine Zufallszahl") {
        const Pcg32 before = rng;
        CHECK(rng.below(0) == 0);
        CHECK(same_state(rng, before));
    }
    SUBCASE("bound 1 liefert immer 0") {
        for (int i = 0; i < 100; ++i) {
            CHECK(rng.below(1) == 0);
        }
    }
    SUBCASE("Ergebnisse liegen in [0, bound)") {
        for (const std::uint32_t bound :
             {2u, 3u, 6u, 7u, 100u, 1000u, 0x80000000u, 0x80000001u, 0xFFFFFFFFu}) {
            for (int i = 0; i < 2000; ++i) {
                CHECK(rng.below(bound) < bound);
            }
        }
    }
    SUBCASE("kleine Grenzen treffen jeden Wert") {
        std::set<std::uint32_t> seen;
        for (int i = 0; i < 1000; ++i) {
            seen.insert(rng.below(6));
        }
        CHECK(seen.size() == 6);
        CHECK(*seen.begin() == 0);
        CHECK(*seen.rbegin() == 5);
    }
    SUBCASE("verwirft Werte unter der Schwelle wie pcg32_boundedrand_r") {
        // Schwelle für 2^31 + 1 ist 2^31 − 1: fast die Hälfte wird verworfen.
        constexpr std::uint32_t bound = 0x80000001u;
        constexpr std::uint32_t threshold = (0u - bound) % bound;
        CHECK(threshold == 0x7FFFFFFFu);
        for (int i = 0; i < 500; ++i) {
            Pcg32 copy = rng;
            std::uint32_t expected = 0;
            for (;;) {
                const std::uint32_t r = copy.next();
                if (r >= threshold) {
                    expected = r % bound;
                    break;
                }
            }
            CHECK(rng.below(bound) == expected);
            CHECK(same_state(rng, copy));
        }
    }
}

TEST_CASE("Pcg32::chance: Randfälle") {
    Pcg32 rng(11u, 5u);
    const Pcg32 before = rng;
    CHECK_FALSE(rng.chance(Fixed::zero()));
    CHECK_FALSE(rng.chance(-Fixed::one()));
    CHECK_FALSE(rng.chance(Fixed::from_raw(-1)));
    CHECK(rng.chance(Fixed::one()));
    CHECK(rng.chance(Fixed::from_int(2)));
    CHECK(same_state(rng, before)); // Randfälle verbrauchen keine Zufallszahl

    // Kleinste positive Wahrscheinlichkeit: nur unit() == 0 trifft.
    for (int i = 0; i < 1000; ++i) {
        Pcg32 copy = rng;
        const bool expected = copy.unit() == Fixed::zero();
        CHECK(rng.chance(Fixed::from_raw(1)) == expected);
    }
    // Größte Wahrscheinlichkeit unter 1: nur unit() == 65535/65536 verfehlt.
    for (int i = 0; i < 1000; ++i) {
        Pcg32 copy = rng;
        const bool expected = copy.unit() < Fixed::from_raw(Fixed::kOne - 1);
        CHECK(rng.chance(Fixed::from_raw(Fixed::kOne - 1)) == expected);
    }
    // Häufigkeit bei p = 0,25 (deterministisch, daher feste Grenzen).
    int hits = 0;
    for (int i = 0; i < 20000; ++i) {
        hits += rng.chance(Fixed::from_ratio(1, 4)) ? 1 : 0;
    }
    CHECK(hits > 4600);
    CHECK(hits < 5400);
}

TEST_CASE("Pcg32::unit und uniform") {
    Pcg32 rng(2024u, 1u);
    for (int i = 0; i < 100000; ++i) {
        Pcg32 copy = rng;
        const Fixed u = rng.unit();
        CHECK(u >= Fixed::zero());
        CHECK(u < Fixed::one());
        CHECK(u.raw() == static_cast<std::int64_t>(copy.next() >> 16u));
    }

    const Fixed lo = Fixed::from_ratio(4, 100);
    const Fixed hi = Fixed::from_ratio(12, 100);
    Fixed seen_min = hi;
    Fixed seen_max = lo;
    for (int i = 0; i < 20000; ++i) {
        const Fixed v = rng.uniform(lo, hi);
        CHECK(v >= lo);
        CHECK(v < hi);
        seen_min = Fixed::min(seen_min, v);
        seen_max = Fixed::max(seen_max, v);
    }
    // Der Bereich wird annähernd ausgeschöpft.
    CHECK(seen_min < lo + Fixed::from_ratio(1, 1000));
    CHECK(seen_max > hi - Fixed::from_ratio(1, 1000));

    // Leerer Bereich liefert die Untergrenze.
    CHECK(rng.uniform(lo, lo) == lo);
}
