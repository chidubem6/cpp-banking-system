#include "Account.h"

#include <utility>

Account::Account(int accountNumber, std::string name, const std::string& pin, Money openingBalance)
    : accountNumber_{accountNumber},
      name_{std::move(name)},
      credential_{makePinCredential(pin)},
      balance_{openingBalance} {}

Account::Account(int accountNumber, std::string name, PinCredential credential, Money balance,
                 int failedAttempts, std::vector<Transaction> history)
    : accountNumber_{accountNumber},
      name_{std::move(name)},
      credential_{std::move(credential)},
      balance_{balance},
      failedAttempts_{failedAttempts},
      history_{std::move(history)} {}

Account::AuthResult Account::authenticate(const std::string& pin) {
    if (isLocked()) return AuthResult::Locked;

    if (!verifyPin(credential_, pin)) {
        ++failedAttempts_;
        return AuthResult::WrongPin;
    }
    failedAttempts_ = 0;
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
