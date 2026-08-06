#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

#include "Bank.h"

namespace {
Bank makeBankWithTwoAccounts() {
    Bank bank;
    bank.createAccount(1001, "Alice", "1111", Money::fromPence(10000));
    bank.createAccount(1002, "Bob", "2222", Money::fromPence(5000));
    return bank;
}
}  // namespace

TEST(Bank, CreatesAnAccount) {
    Bank bank;
    EXPECT_EQ(Bank::CreateResult::Ok,
              bank.createAccount(1001, "Alice", "1111", Money::fromPence(10000)));
    ASSERT_NE(nullptr, bank.findAccount(1001));
    EXPECT_EQ("Alice", bank.findAccount(1001)->name());
}

TEST(Bank, RejectsDuplicateAccountNumbers) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::CreateResult::DuplicateAccountNumber,
              bank.createAccount(1001, "Mallory", "3333", Money::fromPence(100)));
    EXPECT_EQ(2u, bank.accounts().size());
}

TEST(Bank, RejectsEmptyNameAndPin) {
    Bank bank;
    EXPECT_EQ(Bank::CreateResult::InvalidName,
              bank.createAccount(1001, "", "1111", Money::fromPence(100)));
    EXPECT_EQ(Bank::CreateResult::InvalidPin,
              bank.createAccount(1001, "Alice", "", Money::fromPence(100)));
    EXPECT_TRUE(bank.accounts().empty());
}

TEST(Bank, RejectsNegativeOpeningBalance) {
    Bank bank;
    EXPECT_EQ(Bank::CreateResult::InvalidAmount,
              bank.createAccount(1001, "Alice", "1111", Money::fromPence(-1)));
}

TEST(Bank, AllowsZeroOpeningBalance) {
    Bank bank;
    EXPECT_EQ(Bank::CreateResult::Ok,
              bank.createAccount(1001, "Alice", "1111", Money::fromPence(0)));
}

TEST(Bank, FindsNothingForAnUnknownAccount) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(nullptr, bank.findAccount(9999));
}

TEST(Bank, LogsInWithCorrectCredentials) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::LoginResult::Ok, bank.logIn(1001, "1111"));
}

TEST(Bank, ReportsUnknownAccountAndWrongPinSeparately) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::LoginResult::NotFound, bank.logIn(9999, "1111"));
    EXPECT_EQ(Bank::LoginResult::WrongPin, bank.logIn(1001, "0000"));
}

TEST(Bank, ReportsLockedAccounts) {
    auto bank = makeBankWithTwoAccounts();
    bank.logIn(1001, "0000");
    bank.logIn(1001, "0000");
    bank.logIn(1001, "0000");
    EXPECT_EQ(Bank::LoginResult::Locked, bank.logIn(1001, "1111"));
}

TEST(Bank, TransferMovesMoneyBetweenAccounts) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::TransferResult::Ok, bank.transfer(1001, 1002, Money::fromPence(2500)));
    EXPECT_EQ(Money::fromPence(7500), bank.findAccount(1001)->balance());
    EXPECT_EQ(Money::fromPence(7500), bank.findAccount(1002)->balance());
}

TEST(Bank, TransferRecordsBothSidesInHistory) {
    auto bank = makeBankWithTwoAccounts();
    bank.transfer(1001, 1002, Money::fromPence(2500));

    ASSERT_EQ(1u, bank.findAccount(1001)->history().size());
    ASSERT_EQ(1u, bank.findAccount(1002)->history().size());
    EXPECT_EQ(TransactionType::TransferOut, bank.findAccount(1001)->history().front().type());
    EXPECT_EQ(TransactionType::TransferIn, bank.findAccount(1002)->history().front().type());
}

// The most important test in this file: a rejected transfer must not move
// money out of one account without putting it into the other.
TEST(Bank, FailedTransferLeavesBothBalancesUnchanged) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::TransferResult::InsufficientFunds,
              bank.transfer(1001, 1002, Money::fromPence(999999)));
    EXPECT_EQ(Money::fromPence(10000), bank.findAccount(1001)->balance());
    EXPECT_EQ(Money::fromPence(5000), bank.findAccount(1002)->balance());
    EXPECT_TRUE(bank.findAccount(1001)->history().empty());
    EXPECT_TRUE(bank.findAccount(1002)->history().empty());
}

TEST(Bank, RejectsTransferToAnUnknownAccount) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::TransferResult::ReceiverNotFound,
              bank.transfer(1001, 9999, Money::fromPence(100)));
    EXPECT_EQ(Money::fromPence(10000), bank.findAccount(1001)->balance());
}

TEST(Bank, RejectsTransferFromAnUnknownAccount) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::TransferResult::SenderNotFound,
              bank.transfer(9999, 1001, Money::fromPence(100)));
}

TEST(Bank, RejectsTransferToSelf) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::TransferResult::SameAccount, bank.transfer(1001, 1001, Money::fromPence(100)));
    EXPECT_EQ(Money::fromPence(10000), bank.findAccount(1001)->balance());
}

TEST(Bank, RejectsNonPositiveTransferAmounts) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::TransferResult::InvalidAmount, bank.transfer(1001, 1002, Money::fromPence(0)));
    EXPECT_EQ(Bank::TransferResult::InvalidAmount,
              bank.transfer(1001, 1002, Money::fromPence(-100)));
}

TEST(Bank, RejectsTransferThatWouldOverflowTheReceiver) {
    Bank bank;
    bank.createAccount(1001, "Alice", "1111", Money::fromPence(10000));
    bank.addAccount(Account(1002, "Rich", makePinCredential("2222"),
                            Money::fromPence(std::numeric_limits<std::int64_t>::max()), 0,
                            std::vector<Transaction>{}));
    EXPECT_EQ(Bank::TransferResult::Overflow, bank.transfer(1001, 1002, Money::fromPence(100)));
    EXPECT_EQ(Money::fromPence(10000), bank.findAccount(1001)->balance());
}
