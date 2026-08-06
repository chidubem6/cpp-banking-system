#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <optional>
#include <string>
#include <utility>

#include "Money.h"

enum class TransactionType { Deposit, Withdrawal, TransferIn, TransferOut };

// Stable token written to the save file. Changing these breaks existing files.
std::string toStorageString(TransactionType type);
std::optional<TransactionType> transactionTypeFromStorageString(const std::string& text);

// Human-facing label. Safe to reword.
std::string toDisplayString(TransactionType type);

// One immutable ledger entry.
//
// Members are trailing-underscore named so that the defect this class once
// had - a mem-initialiser resolving to the member instead of the parameter -
// cannot recur: resultingBalance_{resultingBalance} names two distinct things.
class Transaction {
public:
    Transaction(TransactionType type, Money amount, std::string details, Money resultingBalance)
        : type_{type},
          amount_{amount},
          details_{std::move(details)},
          resultingBalance_{resultingBalance} {}

    TransactionType type() const { return type_; }
    Money amount() const { return amount_; }
    const std::string& details() const { return details_; }
    Money resultingBalance() const { return resultingBalance_; }

private:
    TransactionType type_;
    Money amount_;
    std::string details_;
    Money resultingBalance_;
};

#endif
