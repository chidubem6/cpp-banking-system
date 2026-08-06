#include "Transaction.h"

std::string toStorageString(TransactionType type) {
    switch (type) {
        case TransactionType::Deposit:     return "Deposit";
        case TransactionType::Withdrawal:  return "Withdrawal";
        case TransactionType::TransferIn:  return "TransferIn";
        case TransactionType::TransferOut: return "TransferOut";
    }
    return "Deposit";
}

std::optional<TransactionType> transactionTypeFromStorageString(const std::string& text) {
    if (text == "Deposit")     return TransactionType::Deposit;
    if (text == "Withdrawal")  return TransactionType::Withdrawal;
    if (text == "TransferIn")  return TransactionType::TransferIn;
    if (text == "TransferOut") return TransactionType::TransferOut;
    return std::nullopt;
}

std::string toDisplayString(TransactionType type) {
    switch (type) {
        case TransactionType::Deposit:     return "Deposit";
        case TransactionType::Withdrawal:  return "Withdrawal";
        case TransactionType::TransferIn:  return "Transfer in";
        case TransactionType::TransferOut: return "Transfer out";
    }
    return "Unknown";
}
