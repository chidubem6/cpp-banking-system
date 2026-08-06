#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <ctime>
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

    // How long a lock lasts. A permanent lock turns three mistyped PINs into
    // an unrecoverable account, and - because the lock is observable by anyone
    // who can guess three times - into a denial of service against any account
    // number an attacker can enumerate. An expiry keeps the brute-force
    // defence (10,000 PINs at 3 tries per 15 minutes is ~52 days) without the
    // permanent damage.
    static constexpr std::time_t kLockoutSeconds = 15 * 60;

    enum class AuthResult { Ok, WrongPin, Locked };
    enum class DepositResult { Ok, InvalidAmount, Overflow };
    enum class WithdrawResult { Ok, InvalidAmount, InsufficientFunds };

    // New account: hashes the PIN with a fresh salt.
    Account(int accountNumber, std::string name, const std::string& pin, Money openingBalance);

    // Restore from storage: takes an already-hashed credential.
    Account(int accountNumber, std::string name, PinCredential credential, Money balance,
            int failedAttempts, std::time_t lockedAt, std::vector<Transaction> history);

    int accountNumber() const { return accountNumber_; }
    const std::string& name() const { return name_; }
    Money balance() const { return balance_; }
    const PinCredential& credential() const { return credential_; }
    int failedAttempts() const { return failedAttempts_; }

    // Epoch seconds at which the current lock started, or 0 if never locked.
    std::time_t lockedAt() const { return lockedAt_; }

    // The caller supplies the current time rather than this class calling
    // std::time itself. That keeps the expiry testable without sleeping, and
    // keeps the domain layer free of ambient dependencies.
    bool isLocked(std::time_t now) const;

    // Increments the failure counter on a wrong PIN and starts a lock when it
    // reaches the limit; resets both on success. An expired lock is cleared
    // before the PIN is checked, so the attempt is judged on its merits.
    AuthResult authenticate(const std::string& pin, std::time_t now);

    DepositResult deposit(Money amount, TransactionType type, std::string details);
    WithdrawResult withdraw(Money amount, TransactionType type, std::string details);

    const std::vector<Transaction>& history() const { return history_; }

private:
    int accountNumber_;
    std::string name_;
    PinCredential credential_;
    Money balance_;
    int failedAttempts_{0};
    std::time_t lockedAt_{0};
    std::vector<Transaction> history_;
};

#endif
