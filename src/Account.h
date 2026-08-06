#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <string>
#include <vector>

#include "Money.h"
#include "PinCredential.h"
#include "Transaction.h"

// A single bank account. Enforces its own rules and returns results; it never
// writes to the console.
class Account {
public:
    static constexpr int kMaxFailedAttempts = 3;

    enum class AuthResult { Ok, WrongPin, Locked };
    enum class DepositResult { Ok, InvalidAmount, Overflow };
    enum class WithdrawResult { Ok, InvalidAmount, InsufficientFunds };

    // New account: hashes the PIN with a fresh salt.
    Account(int accountNumber, std::string name, const std::string& pin, Money openingBalance);

    // Restore from storage: takes an already-hashed credential.
    Account(int accountNumber, std::string name, PinCredential credential, Money balance,
            int failedAttempts, std::vector<Transaction> history);

    int accountNumber() const { return accountNumber_; }
    const std::string& name() const { return name_; }
    Money balance() const { return balance_; }
    const PinCredential& credential() const { return credential_; }
    int failedAttempts() const { return failedAttempts_; }
    bool isLocked() const { return failedAttempts_ >= kMaxFailedAttempts; }

    // Increments the failure counter on a wrong PIN; resets it on success.
    AuthResult authenticate(const std::string& pin);

    DepositResult deposit(Money amount, TransactionType type, std::string details);
    WithdrawResult withdraw(Money amount, TransactionType type, std::string details);

    const std::vector<Transaction>& history() const { return history_; }

private:
    int accountNumber_;
    std::string name_;
    PinCredential credential_;
    Money balance_;
    int failedAttempts_{0};
    std::vector<Transaction> history_;
};

#endif
