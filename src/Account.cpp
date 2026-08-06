#include "Account.h"

#include <utility>

Account::Account(int accountNumber, std::string name, const std::string& pin, Money openingBalance)
    : accountNumber_{accountNumber},
      name_{std::move(name)},
      credential_{makePinCredential(pin)},
      balance_{openingBalance} {}

Account::Account(int accountNumber, std::string name, PinCredential credential, Money balance,
                 int failedAttempts, std::time_t lockedAt, std::vector<Transaction> history)
    : accountNumber_{accountNumber},
      name_{std::move(name)},
      credential_{std::move(credential)},
      balance_{balance},
      failedAttempts_{failedAttempts},
      lockedAt_{lockedAt},
      history_{std::move(history)} {}

bool Account::isLocked(std::time_t now) const {
    if (failedAttempts_ < kMaxFailedAttempts) return false;
    // A clock that has moved backwards (timezone change, NTP correction, an
    // edited save file) must not extend a lock indefinitely. Treat now < lockedAt_
    // as still locked, but the expiry check below bounds it either way.
    return now < lockedAt_ + kLockoutSeconds;
}

Account::AuthResult Account::authenticate(const std::string& pin, std::time_t now) {
    if (isLocked(now)) return AuthResult::Locked;

    // Past the expiry the slate is wiped before the PIN is checked, so the
    // next wrong guess starts a fresh count rather than re-tripping the lock
    // immediately.
    if (failedAttempts_ >= kMaxFailedAttempts) {
        failedAttempts_ = 0;
        lockedAt_ = 0;
    }

    if (!verifyPin(credential_, pin)) {
        ++failedAttempts_;
        if (failedAttempts_ >= kMaxFailedAttempts) lockedAt_ = now;
        return AuthResult::WrongPin;
    }

    failedAttempts_ = 0;
    lockedAt_ = 0;
    return AuthResult::Ok;
}

Account::DepositResult Account::deposit(Money amount, TransactionType type, std::string details) {
    if (!amount.isPositive()) return DepositResult::InvalidAmount;

    const auto updated = balance_.tryAdd(amount);
    if (!updated) return DepositResult::Overflow;

    balance_ = *updated;
    history_.emplace_back(type, amount, std::move(details), balance_);
    return DepositResult::Ok;
}

Account::WithdrawResult Account::withdraw(Money amount, TransactionType type, std::string details) {
    if (!amount.isPositive()) return WithdrawResult::InvalidAmount;
    if (amount > balance_) return WithdrawResult::InsufficientFunds;

    const auto updated = balance_.trySubtract(amount);
    if (!updated) return WithdrawResult::InsufficientFunds;

    balance_ = *updated;
    history_.emplace_back(type, amount, std::move(details), balance_);
    return WithdrawResult::Ok;
}
