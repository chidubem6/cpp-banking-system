#ifndef MONEY_H
#define MONEY_H

#include <cstdint>
#include <optional>
#include <string>

// A GBP amount held as an exact integer count of pence.
//
// Currency is never stored in floating point: binary floating point cannot
// represent 0.1 exactly, so repeated addition drifts. A ledger that does not
// balance defeats the purpose of the program.
//
// "Pence" is GBP's minor unit (ISO 4217). Other currencies differ - JPY has
// no minor unit at all, KWD uses thousandths - so this type is deliberately
// GBP-specific rather than pretending to be general.
class Money {
public:
    Money() = default;

    static Money fromPence(std::int64_t pence);

    // Parses "12", "12.3" or "12.34". Returns nullopt for anything else,
    // including negative input, more than two decimal places, and values
    // that would overflow int64.
    static std::optional<Money> fromString(const std::string& text);

    std::int64_t pence() const { return pence_; }

    // "£12.34", "£0.05", "-£12.34"
    std::string toString() const;

    std::optional<Money> tryAdd(Money other) const;
    std::optional<Money> trySubtract(Money other) const;

    bool isPositive() const { return pence_ > 0; }

private:
    explicit Money(std::int64_t pence) : pence_{pence} {}
    std::int64_t pence_{0};
};

bool operator==(Money lhs, Money rhs);
bool operator!=(Money lhs, Money rhs);
bool operator<(Money lhs, Money rhs);
bool operator<=(Money lhs, Money rhs);
bool operator>(Money lhs, Money rhs);
bool operator>=(Money lhs, Money rhs);

#endif
