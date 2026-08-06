#include "Money.h"

#include <limits>

namespace {

bool isDigits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

// Accumulate decimal digits, rejecting overflow rather than wrapping.
std::optional<std::int64_t> digitsToInt(const std::string& digits) {
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    std::int64_t value = 0;
    for (char c : digits) {
        const std::int64_t digit = c - '0';
        if (value > (kMax - digit) / 10) return std::nullopt;
        value = value * 10 + digit;
    }
    return value;
}

}  // namespace

Money Money::fromPence(std::int64_t pence) {
    return Money{pence};
}

std::optional<Money> Money::fromString(const std::string& text) {
    if (text.empty()) return std::nullopt;

    const std::size_t dot = text.find('.');
    const std::string whole = (dot == std::string::npos) ? text : text.substr(0, dot);
    std::string frac = (dot == std::string::npos) ? "" : text.substr(dot + 1);

    if (!isDigits(whole)) return std::nullopt;
    if (dot != std::string::npos) {
        if (frac.empty() || frac.size() > 2) return std::nullopt;
        if (!isDigits(frac)) return std::nullopt;
    }
    if (frac.size() == 1) frac += '0';   // "12.3" means 30 pence, not 3

    const auto pounds = digitsToInt(whole);
    if (!pounds) return std::nullopt;

    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    if (*pounds > kMax / 100) return std::nullopt;
    std::int64_t total = *pounds * 100;

    if (!frac.empty()) {
        const auto pence = digitsToInt(frac);
        if (!pence) return std::nullopt;
        if (total > kMax - *pence) return std::nullopt;
        total += *pence;
    }
    return Money{total};
}

std::string Money::toString() const {
    const bool negative = pence_ < 0;
    // Negate through unsigned so int64 min does not overflow.
    const std::uint64_t magnitude =
        negative ? (~static_cast<std::uint64_t>(pence_) + 1u)
                 : static_cast<std::uint64_t>(pence_);

    const std::uint64_t pounds = magnitude / 100;
    const std::uint64_t remainder = magnitude % 100;

    std::string out;
    if (negative) out += '-';
    out += "£";                       // £
    out += std::to_string(pounds);
    out += '.';
    if (remainder < 10) out += '0';
    out += std::to_string(remainder);
    return out;
}

std::optional<Money> Money::tryAdd(Money other) const {
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
    if (other.pence_ > 0 && pence_ > kMax - other.pence_) return std::nullopt;
    if (other.pence_ < 0 && pence_ < kMin - other.pence_) return std::nullopt;
    return Money{pence_ + other.pence_};
}

std::optional<Money> Money::trySubtract(Money other) const {
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
    if (other.pence_ < 0 && pence_ > kMax + other.pence_) return std::nullopt;
    if (other.pence_ > 0 && pence_ < kMin + other.pence_) return std::nullopt;
    return Money{pence_ - other.pence_};
}

bool operator==(Money lhs, Money rhs) { return lhs.pence() == rhs.pence(); }
bool operator!=(Money lhs, Money rhs) { return !(lhs == rhs); }
bool operator<(Money lhs, Money rhs) { return lhs.pence() < rhs.pence(); }
bool operator<=(Money lhs, Money rhs) { return !(rhs < lhs); }
bool operator>(Money lhs, Money rhs) { return rhs < lhs; }
bool operator>=(Money lhs, Money rhs) { return !(lhs < rhs); }
