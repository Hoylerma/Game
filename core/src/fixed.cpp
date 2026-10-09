#include "uhrwerk/fixed.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace uhrwerk {

namespace {

constexpr bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

constexpr std::uint64_t kFracMask = static_cast<std::uint64_t>(Fixed::kOne - 1);
constexpr auto kMaxMagnitude = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
// Größter ganzzahliger Anteil, bei dem das Produkt mit 10 noch sicher in uint64 passt
// und der Wert noch darstellbar sein kann (2^47).
constexpr std::uint64_t kMaxIntegerPart = std::uint64_t{1} << (63 - Fixed::kFracBits);

} // namespace

std::optional<Fixed> Fixed::parse(std::string_view text) {
    std::size_t pos = 0;
    bool negative = false;
    if (pos < text.size() && text[pos] == '-') {
        negative = true;
        ++pos;
    }

    // Ganzzahliger Anteil (mindestens eine Ziffer).
    const std::size_t int_begin = pos;
    std::uint64_t int_part = 0;
    while (pos < text.size() && is_digit(text[pos])) {
        int_part = int_part * 10 + static_cast<std::uint64_t>(text[pos] - '0');
        if (int_part > kMaxIntegerPart) {
            return std::nullopt; // außerhalb des Wertebereichs
        }
        ++pos;
    }
    if (pos == int_begin) {
        return std::nullopt;
    }

    // Nachkommaanteil: exakt mit 65536 multiplizieren (schriftliche Multiplikation
    // auf den Dezimalziffern), damit beliebig lange Eingaben korrekt runden.
    std::uint64_t frac_raw = 0;
    bool round_up = false;
    if (pos < text.size() && text[pos] == '.') {
        ++pos;
        const std::size_t frac_begin = pos;
        while (pos < text.size() && is_digit(text[pos])) {
            ++pos;
        }
        if (pos == frac_begin) {
            return std::nullopt; // "5." ist ungültig
        }
        std::vector<std::uint8_t> digits;
        digits.reserve(pos - frac_begin);
        for (std::size_t i = frac_begin; i < pos; ++i) {
            digits.push_back(static_cast<std::uint8_t>(text[i] - '0'));
        }
        std::uint64_t carry = 0;
        for (std::size_t i = digits.size(); i-- > 0;) {
            const std::uint64_t v = std::uint64_t{digits[i]} * static_cast<std::uint64_t>(kOne) + carry;
            digits[i] = static_cast<std::uint8_t>(v % 10);
            carry = v / 10;
        }
        frac_raw = carry;          // floor(Nachkomma · 65536), < 65536
        round_up = digits[0] >= 5; // Rest ≥ 0,5 → kaufmännisch aufrunden
    }

    if (pos != text.size()) {
        return std::nullopt;
    }

    std::uint64_t magnitude = (int_part << kFracBits) + frac_raw + (round_up ? 1u : 0u);
    if (magnitude > kMaxMagnitude) {
        return std::nullopt;
    }
    const auto value = static_cast<std::int64_t>(magnitude);
    return from_raw(negative ? -value : value);
}

std::string Fixed::to_string(int decimals) const {
    if (decimals < 0) {
        decimals = 0;
    }
    if (decimals > 6) {
        decimals = 6;
    }
    std::uint64_t scale = 1;
    for (int i = 0; i < decimals; ++i) {
        scale *= 10;
    }

    const std::uint64_t magnitude = detail::abs_u64(raw_);
    std::uint64_t int_part = magnitude >> kFracBits;
    const std::uint64_t scaled = (magnitude & kFracMask) * scale;
    std::uint64_t frac_digits = scaled >> kFracBits;
    if ((scaled & kFracMask) * 2 >= static_cast<std::uint64_t>(kOne)) {
        ++frac_digits; // kaufmännisch
    }
    if (frac_digits >= scale) {
        frac_digits -= scale;
        ++int_part;
    }

    std::string result;
    if (raw_ < 0 && (int_part != 0 || frac_digits != 0)) {
        result += '-'; // keine "-0.00"
    }
    result += std::to_string(int_part);
    if (decimals > 0) {
        const std::string digits = std::to_string(frac_digits);
        result += '.';
        result.append(static_cast<std::size_t>(decimals) - digits.size(), '0');
        result += digits;
    }
    return result;
}

} // namespace uhrwerk
