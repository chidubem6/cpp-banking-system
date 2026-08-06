#include <gtest/gtest.h>

#include "Transaction.h"

TEST(Transaction, StoresTheResultingBalanceItWasGiven) {
    Transaction t(TransactionType::Deposit, Money::fromPence(5000), "Money deposited",
                  Money::fromPence(15000));
    EXPECT_EQ(Money::fromPence(15000), t.resultingBalance());
}

TEST(Transaction, StoresAllConstructorArguments) {
    Transaction t(TransactionType::Withdrawal, Money::fromPence(2550), "Cash machine",
                  Money::fromPence(7450));
    EXPECT_EQ(TransactionType::Withdrawal, t.type());
    EXPECT_EQ(Money::fromPence(2550), t.amount());
    EXPECT_EQ("Cash machine", t.details());
    EXPECT_EQ(Money::fromPence(7450), t.resultingBalance());
}

TEST(Transaction, TypeRoundTripsThroughStorageStrings) {
    const TransactionType all[] = {TransactionType::Deposit, TransactionType::Withdrawal,
                                   TransactionType::TransferIn, TransactionType::TransferOut};
    for (TransactionType type : all) {
        const auto parsed = transactionTypeFromStorageString(toStorageString(type));
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(type, *parsed);
    }
}

TEST(Transaction, RejectsUnknownStorageStrings) {
    EXPECT_FALSE(transactionTypeFromStorageString("Nonsense").has_value());
    EXPECT_FALSE(transactionTypeFromStorageString("").has_value());
}
