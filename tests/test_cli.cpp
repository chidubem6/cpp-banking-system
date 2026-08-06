#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

#include "Cli.h"
#include "Storage.h"

// The CLI was previously untested on the grounds that driving std::cin costs
// more than it returns. That was wrong: Cli already takes its streams by
// reference, so a test needs nothing but two string streams - and every input
// defect found in review lived in this file.

namespace {

class TempDataFile {
public:
    explicit TempDataFile(std::string name) : path_{std::move(name)} { remove(); }
    ~TempDataFile() { remove(); }

    TempDataFile(const TempDataFile&) = delete;
    TempDataFile& operator=(const TempDataFile&) = delete;

    const std::string& path() const { return path_; }

    void write(const std::string& contents) const {
        std::ofstream out(path_);
        out << contents;
    }

    std::string read() const {
        std::ifstream in(path_);
        std::stringstream buffer;
        buffer << in.rdbuf();
        return buffer.str();
    }

private:
    void remove() const {
        std::remove(path_.c_str());
        std::remove((path_ + ".tmp").c_str());
    }
    std::string path_;
};

// Runs the CLI over a scripted session and returns everything it printed.
struct Session {
    std::string output;
    int exitCode{};
};

Session run(const std::string& input, const std::string& dataFile) {
    std::istringstream in(input);
    std::ostringstream out;
    Cli cli(in, out);
    Session session;
    session.exitCode = cli.run(dataFile);
    session.output = out.str();
    return session;
}

// A full account-creation script, reused by most tests below.
std::string createAlice() {
    return "1\n1001\nAlice Walker\n1234\n250.00\n";
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

}  // namespace

TEST(Cli, CreatesAnAccountAndPersistsIt) {
    TempDataFile file("cli_create.txt");
    const auto session = run(createAlice() + "3\n", file.path());

    EXPECT_EQ(0, session.exitCode);
    EXPECT_TRUE(contains(session.output, "Account created."));
    EXPECT_TRUE(contains(file.read(), "ACC,1001,Alice Walker"));
}

// Regression for the defect where `std::cin >> name` stopped at whitespace:
// "John Smith" became the name "John" and the PIN "Smith".
TEST(Cli, ReadsAFullNameContainingSpaces) {
    TempDataFile file("cli_full_name.txt");
    const auto session = run("1\n1001\nJohn Smith\n1234\n100.00\n2\n1001\n1234\n6\n3\n",
                             file.path());

    EXPECT_TRUE(contains(session.output, "Welcome, John Smith."));
    EXPECT_TRUE(contains(file.read(), "ACC,1001,John Smith,"));
}

// Regression for the infinite loop: a non-numeric entry used to leave cin in a
// failed state, so the menu spun forever without consuming anything.
TEST(Cli, RepromptsOnNonNumericMenuInputAndStillExits) {
    TempDataFile file("cli_bad_menu.txt");
    const auto session = run("abc\n!!\n3\n", file.path());

    EXPECT_EQ(0, session.exitCode);
    EXPECT_TRUE(contains(session.output, "Please enter a whole number."));
}

TEST(Cli, RepromptsOnMalformedAmount) {
    TempDataFile file("cli_bad_amount.txt");
    const auto session =
        run(createAlice() + "2\n1001\n1234\n1\nnot-money\n12.345\n10.00\n6\n3\n", file.path());

    EXPECT_TRUE(contains(session.output, "Please enter an amount such as 100 or 12.34."));
    EXPECT_TRUE(contains(session.output, "Deposited \xC2\xA3\x31\x30.00"));
}

// A CRLF-authored script piped in on Linux used to fail every prompt.
TEST(Cli, AcceptsCarriageReturnLineEndings) {
    TempDataFile file("cli_crlf.txt");
    const auto session =
        run("1\r\n1001\r\nAlice Walker\r\n1234\r\n250.00\r\n3\r\n", file.path());

    EXPECT_EQ(0, session.exitCode);
    EXPECT_TRUE(contains(session.output, "Account created."));
    // The trailing CR must not end up inside the stored name.
    EXPECT_TRUE(contains(file.read(), "ACC,1001,Alice Walker,"));
}

TEST(Cli, EndOfInputExitsCleanly) {
    TempDataFile file("cli_eof.txt");
    const auto session = run("", file.path());
    EXPECT_EQ(0, session.exitCode);
}

TEST(Cli, DepositAndWithdrawUpdateTheBalance) {
    TempDataFile file("cli_deposit.txt");
    const auto session =
        run(createAlice() + "2\n1001\n1234\n1\n75.50\n2\n25.00\n4\n6\n3\n", file.path());

    EXPECT_TRUE(contains(session.output, "Balance: \xC2\xA3\x33\x30\x30.50"));
}

TEST(Cli, RefusesToOverdraw) {
    TempDataFile file("cli_overdraw.txt");
    const auto session = run(createAlice() + "2\n1001\n1234\n2\n999.00\n6\n3\n", file.path());

    EXPECT_TRUE(contains(session.output, "Insufficient funds."));
}

TEST(Cli, HistoryShowsTheCorrectResultingBalance) {
    TempDataFile file("cli_history.txt");
    const auto session = run(createAlice() + "2\n1001\n1234\n1\n75.50\n5\n6\n3\n", file.path());

    EXPECT_TRUE(contains(session.output, "Deposit"));
    EXPECT_TRUE(contains(session.output, "(balance \xC2\xA3\x33\x32\x35.50)"));
}

TEST(Cli, TransfersBetweenAccounts) {
    TempDataFile file("cli_transfer.txt");
    const auto session = run(createAlice() + "1\n1002\nBob Jones\n2222\n50.00\n" +
                                 "2\n1001\n1234\n3\n1002\n100.00\n4\n6\n3\n",
                             file.path());

    EXPECT_TRUE(contains(session.output, "Sent \xC2\xA3\x31\x30\x30.00 to Bob Jones"));
    EXPECT_TRUE(contains(session.output, "Balance: \xC2\xA3\x31\x35\x30.00"));
}

TEST(Cli, RefusesTransferToSelf) {
    TempDataFile file("cli_self_transfer.txt");
    const auto session =
        run(createAlice() + "2\n1001\n1234\n3\n1001\n10.00\n6\n3\n", file.path());

    EXPECT_TRUE(contains(session.output, "You cannot transfer to your own account."));
}

TEST(Cli, RejectsDuplicateAccountNumbers) {
    TempDataFile file("cli_duplicate.txt");
    const auto session =
        run(createAlice() + "1\n1001\nEve\n9999\n10.00\n3\n", file.path());

    EXPECT_TRUE(contains(session.output, "That account number is already in use."));
}

// The three failure modes must be indistinguishable, or the prompt becomes an
// account-number oracle.
TEST(Cli, UnknownAccountAndWrongPinAreIndistinguishable) {
    TempDataFile file("cli_oracle.txt");
    const auto wrongPin = run(createAlice() + "2\n1001\nwrong\n3\n", file.path());
    const auto unknown = run("2\n5555\nwrong\n3\n", file.path());

    EXPECT_TRUE(contains(wrongPin.output, "Incorrect account number or PIN."));
    EXPECT_TRUE(contains(unknown.output, "Incorrect account number or PIN."));
    EXPECT_FALSE(contains(wrongPin.output, "locked"));
    EXPECT_FALSE(contains(unknown.output, "locked"));
}

TEST(Cli, LockedAccountIsAlsoIndistinguishable) {
    TempDataFile file("cli_locked.txt");
    run(createAlice() + "3\n", file.path());

    // Three wrong PINs trip the lock.
    run("2\n1001\nbad\n2\n1001\nbad\n2\n1001\nbad\n3\n", file.path());

    // The correct PIN must now be refused with the same generic message.
    const auto afterLock = run("2\n1001\n1234\n3\n", file.path());
    EXPECT_TRUE(contains(afterLock.output, "Incorrect account number or PIN."));
    EXPECT_FALSE(contains(afterLock.output, "Welcome"));
    EXPECT_FALSE(contains(afterLock.output, "locked"));
}

// The counter must reach disk immediately, or killing the process between
// guesses resets it and the lockout is bypassable.
TEST(Cli, FailedAttemptsArePersistedImmediately) {
    TempDataFile file("cli_persist_attempts.txt");
    run(createAlice() + "3\n", file.path());

    run("2\n1001\nbad\n2\n1001\nbad\n3\n", file.path());

    // Read the count back through Storage rather than matching raw text, so
    // the test does not break every time a field is added to the record.
    Bank reloaded;
    ASSERT_TRUE(storage::load(reloaded, file.path()).ok);
    ASSERT_NE(nullptr, reloaded.findAccount(1001));
    EXPECT_EQ(2, reloaded.findAccount(1001)->failedAttempts());
}

TEST(Cli, StartsCleanlyWhenTheDataFileIsMissing) {
    TempDataFile file("cli_missing.txt");   // constructor removes it
    const auto session = run("3\n", file.path());
    EXPECT_EQ(0, session.exitCode);
}

// Refusing to start is what stops a malformed file being overwritten with an
// empty bank on exit.
TEST(Cli, RefusesToStartOnAMalformedDataFile) {
    TempDataFile file("cli_malformed.txt");
    file.write("ACC,1001,Alice,abc,def,10000,0\nGARBAGE\n");

    const auto session = run("3\n", file.path());
    EXPECT_EQ(1, session.exitCode);
    EXPECT_TRUE(contains(session.output, "Refusing to start"));
    // The original file must still be intact.
    EXPECT_TRUE(contains(file.read(), "GARBAGE"));
}

TEST(Cli, RejectsAnEmptyNameAndAnEmptyPin) {
    TempDataFile file("cli_empty_fields.txt");
    const auto emptyName = run("1\n1001\n\n1234\n10.00\n3\n", file.path());
    EXPECT_TRUE(contains(emptyName.output, "Name cannot be empty."));

    const auto emptyPin = run("1\n1002\nBob\n\n10.00\n3\n", file.path());
    EXPECT_TRUE(contains(emptyPin.output, "PIN cannot be empty."));
}

TEST(Cli, RejectsOutOfRangeMenuChoices) {
    TempDataFile file("cli_menu_range.txt");
    const auto session = run("9\n3\n", file.path());
    EXPECT_TRUE(contains(session.output, "Please choose 1, 2 or 3."));
}

// A lock must not be permanent: three mistyped PINs should not brick an
// account. The CLI reads the real clock, so this test verifies the lock is
// active now and leaves the expiry-window arithmetic to test_account.cpp,
// which injects the time.
TEST(Cli, LockIsRecordedWithATimestampSoItCanExpire) {
    TempDataFile file("cli_lock_expiry.txt");
    run(createAlice() + "3\n", file.path());
    run("2\n1001\nbad\n2\n1001\nbad\n2\n1001\nbad\n3\n", file.path());

    Bank reloaded;
    ASSERT_TRUE(storage::load(reloaded, file.path()).ok);
    const Account* account = reloaded.findAccount(1001);
    ASSERT_NE(nullptr, account);

    EXPECT_EQ(Account::kMaxFailedAttempts, account->failedAttempts());
    EXPECT_NE(0, account->lockedAt());   // a real instant, not a sentinel
    EXPECT_TRUE(account->isLocked(account->lockedAt()));
    EXPECT_FALSE(account->isLocked(account->lockedAt() + Account::kLockoutSeconds));
}
