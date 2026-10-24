#pragma once

// Deterministischer Zufallsgenerator PCG32 (O'Neill, pcg-random.org, XSH-RR).
//
// Jedes System der Simulation bekommt einen eigenen Strom (RngStream), der aus
// dem Partie-Seed abgeleitet wird. Dadurch verändert neuer Inhalt in einem
// System nicht die Zufallsfolge anderer Systeme.

#include "uhrwerk/fixed.hpp"

#include <cstdint>

namespace uhrwerk {

/// Feste Strom-Kennungen. Werte niemals ändern – das würde alte Spielstände
/// und Determinismus-Referenzen ungültig machen. Neue Ströme hinten anfügen.
enum class RngStream : std::uint64_t {
    Demografie = 1,
    Wirtschaft = 2,
    Ereignisse = 3,
    Ki = 4,
    Lasttest = 5,    // seit M4: Aufbau der Lasttest-Welt (uhrwerk/lasttest.hpp), nicht in der Simulation
    Testbefehle = 6, // seit M6: Testbefehle der Kommandozeile (--testbefehle), nicht in der Simulation
    Wahlen = 7,      // seit M8: je Wahl neu aus Seed und Parlament (politics.hpp), kein Zustand
    Krieg = 8,       // seit M10: Schlachten (military.hpp), Zustand nur in Welten mit Militär
};

class Pcg32 {
public:
    constexpr Pcg32() { seed(0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL); }
    constexpr Pcg32(std::uint64_t seed_value, std::uint64_t stream) { seed(seed_value, stream); }
    constexpr Pcg32(std::uint64_t seed_value, RngStream stream) {
        seed(seed_value, static_cast<std::uint64_t>(stream));
    }

    constexpr void seed(std::uint64_t seed_value, std::uint64_t stream) {
        state_ = 0;
        inc_ = (stream << 1u) | 1u;
        next();
        state_ += seed_value;
        next();
    }

    constexpr std::uint32_t next() {
        const std::uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + inc_;
        const auto xorshifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
        const auto rot = static_cast<std::uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((0u - rot) & 31u));
    }

    /// Gleichverteilte Zahl in [0, bound) ohne Modulo-Verzerrung. bound 0 → 0.
    constexpr std::uint32_t below(std::uint32_t bound) {
        if (bound == 0) {
            return 0;
        }
        const std::uint32_t threshold = (0u - bound) % bound;
        for (;;) {
            const std::uint32_t r = next();
            if (r >= threshold) {
                return r % bound;
            }
        }
    }

    /// Gleichverteilter Festkommawert in [0, 1).
    constexpr Fixed unit() { return Fixed::from_raw(static_cast<std::int64_t>(next() >> 16u)); }

    /// Gleichverteilter Festkommawert in [lo, hi).
    constexpr Fixed uniform(Fixed lo, Fixed hi) { return lo + (hi - lo) * unit(); }

    /// true mit Wahrscheinlichkeit p (p ≤ 0 → nie, p ≥ 1 → immer).
    constexpr bool chance(Fixed p) {
        if (p <= Fixed::zero()) {
            return false;
        }
        if (p >= Fixed::one()) {
            return true;
        }
        return unit() < p;
    }

    constexpr std::uint64_t state() const { return state_; }
    constexpr std::uint64_t increment() const { return inc_; }

    /// Zustand wiederherstellen, wie ihn state() und increment() geliefert haben (Spielstände,
    /// seit M6). Ein Strom mit geradem increment ist ungültig; der Aufrufer prüft das.
    constexpr void restore(std::uint64_t state, std::uint64_t increment) {
        state_ = state;
        inc_ = increment;
    }

private:
    std::uint64_t state_ = 0;
    std::uint64_t inc_ = 0;
};

} // namespace uhrwerk
