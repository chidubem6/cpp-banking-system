#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "Account.h"

namespace {
Account makeAccount(std::int64_t openingPence = 10000) {
    return Account(12345, "Chidubem", "1234", Money::fromPence(openingPence));
}
}  // namespace

TEST(Account, StartsWithTheOpeningBalanceAndNoHistory) {
    const auto account = makeAccount();
    EXPECT_EQ(12345, account.accountNumber());
    EXPECT_EQ("Chidubem", account.name());
    EXPECT_EQ(Money::fromPence(10000), account.balance());
    EXPECT_TRUE(account.history().empty());
    EXPECT_FALSE(account.isLocked());
}

TEST(Account, DepositIncreasesTheBalance) {
    auto account = makeAccount();
    EXPECT_EQ(Account::DepositResult::Ok,
              account.deposit(Money::fromPence(5000), TransactionType::Deposit, "Pay"));
    EXPECT_EQ(Money::fromPence(15000), account.balance());
}

// Regression test for the original uninitialised-read defect.
TEST(Account, TransactionRecordsTheCorrectResultingBalance) {
    auto account = makeAccount();
    account.deposit(Money::fromPence(5000), TransactionType::Deposit, "Pay");
    ASSERT_EQ(1u, account.history().size());
    EXPECT_EQ(Money::fromPence(15000), account.history().front().resultingBalance());
}

TEST(Account, RejectsNonPositiveDeposits) {
    auto account = makeAccount();
    EXPECT_EQ(Account::DepositResult::InvalidAmount,
              account.deposit(Money::fromPence(0), TransactionType::Deposit, "Nothing"));
    EXPECT_EQ(Account::DepositResult::InvalidAmount,
              account.deposit(Money::fromPence(-100), TransactionType::Deposit, "Negative"));
    EXPECT_EQ(Money::fromPence(10000), account.balance());
    EXPECT_TRUE(account.history().empty());
}

TEST(Account, WithdrawDecreasesTheBalance) {
    auto account = makeAccount();
    EXPECT_EQ(Account::WithdrawResult::Ok,
              account.withdraw(Money::fromPence(2500), TransactionType::Withdrawal, "Cash"));
    EXPECT_EQ(Money::fromPence(7500), account.balance());
}

TEST(Account, RejectsOverdraftAndLeavesStateUntouched) {
    auto account = makeAccount();
    EXPECT_EQ(Account::WithdrawResult::InsufficientFunds,
              account.withdraw(Money::fromPence(10001), TransactionType::Withdrawal, "Too much"));
    EXPECT_EQ(Money::fromPence(10000), account.balance());
    EXPECT_TRUE(account.history().empty());
}

TEST(Account, AllowsWithdrawingTheEntireBalance) {
    auto account = makeAccount();
    EXPECT_EQ(Account::WithdrawResult::Ok,
              account.withdraw(Money::fromPence(10000), TransactionType::Withdrawal, "All"));
    EXPECT_EQ(Money::fromPence(0), account.balance());
}

TEST(Account, RejectsNonPositiveWithdrawals) {
    auto account = makeAccount();
    EXPECT_EQ(Account::WithdrawResult::InvalidAmount,
              account.withdraw(Money::fromPence(0), TransactionType::Withdrawal, "Nothing"));
    EXPECT_EQ(Account::WithdrawResult::InvalidAmount,
              account.withdraw(Money::fromPence(-1), TransactionType::Withdrawal, "Negative"));
}

TEST(Account, AuthenticatesTheCorrectPin) {
    auto account = makeAccount();
    EXPECT_EQ(Account::AuthResult::Ok, account.authenticate("1234"));
    EXPECT_EQ(0, account.failedAttempts());
}

TEST(Account, DoesNotStoreThePinInPlaintext) {
    const auto account = makeAccount();
    EXPECT_EQ(std::string::npos, account.credential().hash.find("1234"));
}

TEST(Account, LocksAfterThreeConsecutiveFailures) {
    auto account = makeAccount();
    EXPECT_EQ(Account::AuthResult::WrongPin, account.authenticate("0000"));
    EXPECT_EQ(Account::AuthResult::WrongPin, account.authenticate("0000"));
    EXPECT_FALSE(account.isLocked());
    EXPECT_EQ(Account::AuthResult::WrongPin, account.authenticate("0000"));
    EXPECT_TRUE(account.isLocked());
}

TEST(Account, CorrectPinIsRefusedOnceLocked) {
    auto account = makeAccount();
    account.authenticate("0000");
    account.authenticate("0000");
    account.authenticate("0000");
    ASSERT_TRUE(account.isLocked());
    EXPECT_EQ(Account::AuthResult::Locked, account.authenticate("1234"));
}

TEST(Account, SuccessfulLoginResetsTheFailureCounter) {
    auto account = makeAccount();
    account.authenticate("0000");
    account.authenticate("0000");
    EXPECT_EQ(2, account.failedAttempts());
    EXPECT_EQ(Account::AuthResult::Ok, account.authenticate("1234"));
    EXPECT_EQ(0, account.failedAttempts());
    EXPECT_EQ(Account::AuthResult::WrongPin, account.authenticate("0000"));
    EXPECT_FALSE(account.isLocked());
}

TEST(Account, RestoringConstructorPreservesState) {
    auto original = makeAccount();
    original.deposit(Money::fromPence(5000), TransactionType::Deposit, "Pay");

    Account restored(original.accountNumber(), original.name(), original.credential(),
                     original.balance(), original.failedAttempts(),
                     std::vector<Transaction>(original.history()));

    EXPECT_EQ(original.balance(), restored.balance());
    EXPECT_EQ(1u, restored.history().size());
    EXPECT_EQ(Account::AuthResult::Ok, restored.authenticate("1234"));
}
