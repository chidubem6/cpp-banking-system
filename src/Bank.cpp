#include "Bank.h"

#include <stdexcept>
#include <string>

Bank::CreateResult Bank::createAccount(int accountNumber, const std::string& name,
                                       const std::string& pin, Money openingBalance) {
    if (findAccount(accountNumber) != nullptr) return CreateResult::DuplicateAccountNumber;
    if (name.empty()) return CreateResult::InvalidName;
    if (pin.empty()) return CreateResult::InvalidPin;
    if (openingBalance.pence() < 0) return CreateResult::InvalidAmount;

    accounts_.emplace_back(accountNumber, name, pin, openingBalance);
    return CreateResult::Ok;
}

Bank::LoginResult Bank::logIn(int accountNumber, const std::string& pin, std::time_t now) {
    Account* account = findAccount(accountNumber);
    if (account == nullptr) return LoginResult::NotFound;

    switch (account->authenticate(pin, now)) {
        case Account::AuthResult::Ok:
            return LoginResult::Ok;
        case Account::AuthResult::Locked:
            return LoginResult::Locked;
        case Account::AuthResult::WrongPin:
            break;
    }
    // A wrong PIN may have been the attempt that tripped the lock.
    return account->isLocked(now) ? LoginResult::Locked : LoginResult::WrongPin;
}

Bank::TransferResult Bank::transfer(int fromAccount, int toAccount, Money amount) {
    if (fromAccount == toAccount) return TransferResult::SameAccount;
    if (!amount.isPositive()) return TransferResult::InvalidAmount;

    Account* sender = findAccount(fromAccount);
    if (sender == nullptr) return TransferResult::SenderNotFound;
    Account* receiver = findAccount(toAccount);
    if (receiver == nullptr) return TransferResult::ReceiverNotFound;

    // Check both legs before mutating either, so a rejected transfer cannot
    // leave money withdrawn from one account and not credited to the other.
    if (amount > sender->balance()) return TransferResult::InsufficientFunds;
    if (!receiver->balance().tryAdd(amount).has_value()) return TransferResult::Overflow;

    // The account number is recorded alongside the name because names are not
    // unique - createAccount rejects only duplicate numbers - so the name
    // alone cannot answer the one question a transfer record exists to answer.
    const auto withdrawn = sender->withdraw(
        amount, TransactionType::TransferOut,
        "Sent to " + receiver->name() + " (" + std::to_string(toAccount) + ")");
    if (withdrawn != Account::WithdrawResult::Ok) return TransferResult::InsufficientFunds;

    const auto deposited =
        receiver->deposit(amount, TransactionType::TransferIn,
                          "Received from " + sender->name() + " (" +
                              std::to_string(fromAccount) + ")");
    if (deposited != Account::DepositResult::Ok) {
        // Unreachable given the checks above, but if it ever happens, put the
        // money back rather than losing it. Labelled Reversal, not TransferIn:
        // to a user, "Transfer in" reads as money received from someone else,
        // not as their own failed payment being undone.
        const auto reversed =
            sender->deposit(amount, TransactionType::Reversal, "Reversed failed transfer");
        if (reversed != Account::DepositResult::Ok) {
            // Money would be destroyed. Nothing sensible remains to do here,
            // so fail loudly rather than return a result that implies the
            // funds are safe.
            throw std::logic_error("Transfer rollback failed; account state is inconsistent");
        }
        return TransferResult::Overflow;
    }
    return TransferResult::Ok;
}

Account* Bank::findAccount(int accountNumber) {
    for (auto& account : accounts_) {
        if (account.accountNumber() == accountNumber) return &account;
    }
    return nullptr;
}

const Account* Bank::findAccount(int accountNumber) const {
    for (const auto& account : accounts_) {
        if (account.accountNumber() == accountNumber) return &account;
    }
    return nullptr;
}
