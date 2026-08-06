#include "Transaction.h"

std::string toStorageString(TransactionType type) {
    switch (type) {
        case TransactionType::Deposit:     return "Deposit";
        case TransactionType::Withdrawal:  return "Withdrawal";
        case TransactionType::TransferIn:  return "TransferIn";
        case TransactionType::TransferOut: return "TransferOut";
        case TransactionType::Reversal:    return "Reversal";
    }
    // Unreachable for a valid enum. The fallback must be a token that
    // transactionTypeFromStorageString rejects: returning "Deposit" would
    // silently persist a corrupted type as a legitimate deposit.
    return "Invalid";
}

std::optional<TransactionType> transactionTypeFromStorageString(const std::string& text) {
    if (text == "Deposit")     return TransactionType::Deposit;
    if (text == "Withdrawal")  return TransactionType::Withdrawal;
    if (text == "TransferIn")  return TransactionType::TransferIn;
    if (text == "TransferOut") return TransactionType::TransferOut;
    if (text == "Reversal")    return TransactionType::Reversal;
    return std::nullopt;
}

std::string toDisplayString(TransactionType type) {
    switch (type) {
        case TransactionType::Deposit:     return "Deposit";
        case TransactionType::Withdrawal:  return "Withdrawal";
        case TransactionType::TransferIn:  return "Transfer in";
        case TransactionType::TransferOut: return "Transfer out";
        case TransactionType::Reversal:    return "Reversal";
    }
    return "Unknown";
}
