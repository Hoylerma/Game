#pragma once

// Deterministische Festkommazahl für den Simulationskern.
//
// Format Q47.16 in einem int64: 16 Nachkommabits (Auflösung 1/65536),
// Wertebereich etwa ±1,4 · 10^14. Alle Rechenoperationen liefern auf jeder
// Plattform und mit jedem Compiler bitgenau dasselbe Ergebnis – Voraussetzung
// für Lockstep-Multiplayer, Replays und reproduzierbare Tests.
//
// Rundungsregeln (verbindlich, von Tests abgesichert):
//   *  Multiplikation: Abrunden (Richtung −∞) des exakten Produkts.
//   /  Division:       Abschneiden (Richtung 0) des exakten Quotienten.
//   from_ratio, parse: kaufmännisch (Hälfte weg von null).
//
// Gleitkommazahlen dürfen im Kern nur zur Anzeige verwendet werden (to_double).

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace uhrwerk {

namespace detail {

struct U128 {
    std::uint64_t hi = 0;
    std::uint64_t lo = 0;
};

// Vollständiges 64×64→128-Bit-Produkt ohne Compiler-Erweiterungen.
constexpr U128 mul_u64(std::uint64_t a, std::uint64_t b) {
    const std::uint64_t a_lo = a & 0xFFFFFFFFu, a_hi = a >> 32;
    const std::uint64_t b_lo = b & 0xFFFFFFFFu, b_hi = b >> 32;
    const std::uint64_t p0 = a_lo * b_lo;
    const std::uint64_t p1 = a_lo * b_hi;
    const std::uint64_t p2 = a_hi * b_lo;
    const std::uint64_t p3 = a_hi * b_hi;
    const std::uint64_t mid = (p0 >> 32) + (p1 & 0xFFFFFFFFu) + (p2 & 0xFFFFFFFFu);
    U128 r;
    r.lo = (mid << 32) | (p0 & 0xFFFFFFFFu);
    r.hi = p3 + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
    return r;
}

// Ganzzahlige Division 128/64 → 64 (Quotient muss in 64 Bit passen),
// bitweise Schulmethode; identisch auf allen Plattformen.
constexpr std::uint64_t div_u128_by_u64(U128 n, std::uint64_t d, std::uint64_t* remainder = nullptr) {
    std::uint64_t q = 0;
    std::uint64_t rem = 0;
    for (int i = 127; i >= 0; --i) {
        const std::uint64_t bit = i >= 64 ? (n.hi >> (i - 64)) & 1u : (n.lo >> i) & 1u;
        const bool carry = (rem >> 63) != 0;
        rem = (rem << 1) | bit;
        if (carry || rem >= d) {
            rem -= d;
            if (i < 64) {
                q |= std::uint64_t{1} << i;
            }
        }
    }
    if (remainder != nullptr) {
        *remainder = rem;
    }
    return q;
}

constexpr std::uint64_t abs_u64(std::int64_t v) {
    return v < 0 ? std::uint64_t{0} - static_cast<std::uint64_t>(v) : static_cast<std::uint64_t>(v);
}

} // namespace detail

class Fixed {
public:
    static constexpr int kFracBits = 16;
    static constexpr std::int64_t kOne = std::int64_t{1} << kFracBits;

    constexpr Fixed() = default;

    static constexpr Fixed from_raw(std::int64_t raw) {
        Fixed f;
        f.raw_ = raw;
        return f;
    }

    static constexpr Fixed from_int(std::int64_t value) { return from_raw(value * kOne); }

    /// num / den, kaufmännisch gerundet. Voraussetzung: den != 0, |num| < 2^47.
    static constexpr Fixed from_ratio(std::int64_t num, std::int64_t den) {
        const bool negative = (num < 0) != (den < 0);
        const std::uint64_t n = detail::abs_u64(num) << kFracBits;
        const std::uint64_t d = detail::abs_u64(den);
        std::uint64_t q = n / d;
        const std::uint64_t r = n % d;
        if (r * 2 >= d) {
            ++q;
        }
        const auto value = static_cast<std::int64_t>(q);
        return from_raw(negative ? -value : value);
    }

    /// Liest eine Dezimalzahl ("12", "-0.25", "3.14159"). Deterministisch,
    /// kaufmännisch gerundet auf 1/65536. Gibt nullopt bei ungültigem Text.
    static std::optional<Fixed> parse(std::string_view text);

    constexpr std::int64_t raw() const { return raw_; }

    /// Ganzzahliger Anteil, abgeschnitten Richtung 0.
    constexpr std::int64_t trunc() const { return raw_ / kOne; }

    /// Kaufmännisch gerundet auf eine ganze Zahl.
    constexpr std::int64_t round() const {
        const std::uint64_t magnitude = detail::abs_u64(raw_);
        const auto rounded = static_cast<std::int64_t>((magnitude + (kOne / 2)) >> kFracBits);
        return raw_ < 0 ? -rounded : rounded;
    }

    /// Nur für Anzeige und Werkzeuge – niemals in der Simulation verwenden.
    double to_double() const { return static_cast<double>(raw_) / static_cast<double>(kOne); }

    /// Deterministische Dezimaldarstellung mit Punkt, z. B. "12.50" (decimals 0..6).
    std::string to_string(int decimals = 2) const;

    friend constexpr Fixed operator+(Fixed a, Fixed b) { return from_raw(a.raw_ + b.raw_); }
    friend constexpr Fixed operator-(Fixed a, Fixed b) { return from_raw(a.raw_ - b.raw_); }
    friend constexpr Fixed operator-(Fixed a) { return from_raw(-a.raw_); }

    friend constexpr Fixed operator*(Fixed a, Fixed b) {
        const bool negative = (a.raw_ < 0) != (b.raw_ < 0);
        const detail::U128 p = detail::mul_u64(detail::abs_u64(a.raw_), detail::abs_u64(b.raw_));
        const std::uint64_t magnitude = (p.hi << (64 - kFracBits)) | (p.lo >> kFracBits);
        const bool has_remainder = (p.lo & static_cast<std::uint64_t>(kOne - 1)) != 0;
        auto value = static_cast<std::int64_t>(magnitude);
        if (negative) {
            value = -value;
            if (has_remainder) {
                --value; // Abrunden Richtung −∞
            }
        }
        return from_raw(value);
    }

    /// Division; Divisor 0 ergibt 0 (die Simulation prüft vorher).
    friend constexpr Fixed operator/(Fixed a, Fixed b) {
        if (b.raw_ == 0) {
            return Fixed{};
        }
        const bool negative = (a.raw_ < 0) != (b.raw_ < 0);
        const std::uint64_t n = detail::abs_u64(a.raw_);
        const detail::U128 shifted{n >> (64 - kFracBits), n << kFracBits};
        const std::uint64_t q = detail::div_u128_by_u64(shifted, detail::abs_u64(b.raw_));
        const auto value = static_cast<std::int64_t>(q);
        return from_raw(negative ? -value : value);
    }

    constexpr Fixed& operator+=(Fixed o) { return *this = *this + o; }
    constexpr Fixed& operator-=(Fixed o) { return *this = *this - o; }
    constexpr Fixed& operator*=(Fixed o) { return *this = *this * o; }
    constexpr Fixed& operator/=(Fixed o) { return *this = *this / o; }

    friend constexpr auto operator<=>(Fixed, Fixed) = default;

    static constexpr Fixed min(Fixed a, Fixed b) { return a < b ? a : b; }
    static constexpr Fixed max(Fixed a, Fixed b) { return a < b ? b : a; }
    constexpr Fixed clamp(Fixed lo, Fixed hi) const { return max(lo, min(hi, *this)); }

    static constexpr Fixed zero() { return Fixed{}; }
    static constexpr Fixed one() { return from_raw(kOne); }

private:
    std::int64_t raw_ = 0;
};

} // namespace uhrwerk
