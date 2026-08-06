#include <gtest/gtest.h>

#include "Transaction.h"

TEST(Transaction, StoresTheResultingBalanceItWasGiven) {
    Transaction t("Deposit", 50.0, "Money deposited", 150.0);
    EXPECT_DOUBLE_EQ(150.0, t.getResultingBalance());
}

TEST(Transaction, StoresAllConstructorArguments) {
    Transaction t("Withdrawal", 25.5, "Cash machine", 74.5);
    EXPECT_EQ("Withdrawal", t.getType());
    EXPECT_DOUBLE_EQ(25.5, t.getAmount());
    EXPECT_EQ("Cash machine", t.getDetails());
    EXPECT_DOUBLE_EQ(74.5, t.getResultingBalance());
}
