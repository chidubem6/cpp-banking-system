#ifndef BANK_H
#define BANK_H

#include <string>
#include <utility>
#include <vector>

#include "Account.h"
#include "Money.h"

// Owns every account and enforces the rules that span more than one of them.
// Returns results; never writes to the console and never touches the disk.
class Bank {
public:
    enum class CreateResult { Ok, DuplicateAccountNumber, InvalidName, InvalidPin, InvalidAmount };
    enum class LoginResult { Ok, NotFound, WrongPin, Locked };
    enum class TransferResult {
        Ok,
        SenderNotFound,
        ReceiverNotFound,
        SameAccount,
        InvalidAmount,
        InsufficientFunds,
        Overflow
    };

    CreateResult createAccount(int accountNumber, const std::string& name, const std::string& pin,
                               Money openingBalance);

    // Returns a result only, never a pointer. Callers hold the account number
    // for the session and re-resolve it per operation, because addAccount can
    // reallocate the underlying vector and invalidate any pointer handed out
    // earlier.
    LoginResult logIn(int accountNumber, const std::string& pin);

    TransferResult transfer(int fromAccount, int toAccount, Money amount);

    Account* findAccount(int accountNumber);
    const Account* findAccount(int accountNumber) const;

    const std::vector<Account>& accounts() const { return accounts_; }
    void addAccount(Account account) { accounts_.push_back(std::move(account)); }

private:
    std::vector<Account> accounts_;
};

#endif
