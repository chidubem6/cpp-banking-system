#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>
#include <utility>

#include "Storage.h"

namespace {

// Writes to a uniquely named file in the working directory and removes it.
class TempFile {
public:
    explicit TempFile(std::string name) : path_{std::move(name)} {}
    ~TempFile() { std::remove(path_.c_str()); }

    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    const std::string& path() const { return path_; }

    void write(const std::string& contents) const {
        std::ofstream out(path_);
        out << contents;
    }

private:
    std::string path_;
};

Bank makeBank() {
    Bank bank;
    bank.createAccount(1001, "Alice", "1111", Money::fromPence(10000));
    bank.createAccount(1002, "Bob", "2222", Money::fromPence(5000));
    return bank;
}

}  // namespace

TEST(StorageEscaping, LeavesOrdinaryTextAlone) {
    EXPECT_EQ("Alice", storage::escapeField("Alice"));
}

TEST(StorageEscaping, EscapesCommasAndBackslashes) {
    EXPECT_EQ("Smith\\, John", storage::escapeField("Smith, John"));
    EXPECT_EQ("a\\\\b", storage::escapeField("a\\b"));
}

TEST(StorageEscaping, RoundTrips) {
    const std::string awkward = "Smith, John \\ \"Jr\", ,,";
    EXPECT_EQ(awkward, storage::unescapeField(storage::escapeField(awkward)));
}

TEST(StorageEscaping, SplitsOnUnescapedCommasOnly) {
    const auto fields = storage::splitEscaped("ACC,Smith\\, John,1234");
    ASSERT_EQ(3u, fields.size());
    EXPECT_EQ("ACC", fields[0]);
    EXPECT_EQ("Smith, John", fields[1]);
    EXPECT_EQ("1234", fields[2]);
}

TEST(Storage, RoundTripsAnEmptyBank) {
    TempFile file("test_empty.txt");
    Bank original;
    ASSERT_TRUE(storage::save(original, file.path()).ok);

    Bank loaded;
    const auto outcome = storage::load(loaded, file.path());
    EXPECT_TRUE(outcome.ok) << outcome.error;
    EXPECT_TRUE(loaded.accounts().empty());
}

TEST(Storage, RoundTripsAccountsAndBalances) {
    TempFile file("test_round_trip.txt");
    auto original = makeBank();
    ASSERT_TRUE(storage::save(original, file.path()).ok);

    Bank loaded;
    ASSERT_TRUE(storage::load(loaded, file.path()).ok);

    ASSERT_EQ(2u, loaded.accounts().size());
    ASSERT_NE(nullptr, loaded.findAccount(1001));
    EXPECT_EQ("Alice", loaded.findAccount(1001)->name());
    EXPECT_EQ(Money::fromPence(10000), loaded.findAccount(1001)->balance());
    EXPECT_EQ(Money::fromPence(5000), loaded.findAccount(1002)->balance());
}

TEST(Storage, RoundTripsTransactionHistory) {
    TempFile file("test_history.txt");
    auto original = makeBank();
    original.transfer(1001, 1002, Money::fromPence(2500));
    ASSERT_TRUE(storage::save(original, file.path()).ok);

    Bank loaded;
    ASSERT_TRUE(storage::load(loaded, file.path()).ok);

    const auto& history = loaded.findAccount(1001)->history();
    ASSERT_EQ(1u, history.size());
    EXPECT_EQ(TransactionType::TransferOut, history.front().type());
    EXPECT_EQ(Money::fromPence(2500), history.front().amount());
    EXPECT_EQ(Money::fromPence(7500), history.front().resultingBalance());
    EXPECT_EQ("Sent to Bob (1002)", history.front().details());
}

// The old plain-CSV format silently corrupted this.
TEST(Storage, SurvivesANameContainingACommaAndBackslash) {
    TempFile file("test_awkward_name.txt");
    Bank original;
    original.createAccount(1001, "Smith, John \\ Jr", "1111", Money::fromPence(100));
    ASSERT_TRUE(storage::save(original, file.path()).ok);

    Bank loaded;
    ASSERT_TRUE(storage::load(loaded, file.path()).ok);
    ASSERT_NE(nullptr, loaded.findAccount(1001));
    EXPECT_EQ("Smith, John \\ Jr", loaded.findAccount(1001)->name());
}

TEST(Storage, PreservesPinsAcrossSaveAndLoad) {
    TempFile file("test_pins.txt");
    auto original = makeBank();
    ASSERT_TRUE(storage::save(original, file.path()).ok);

    Bank loaded;
    ASSERT_TRUE(storage::load(loaded, file.path()).ok);
    EXPECT_EQ(Bank::LoginResult::Ok, loaded.logIn(1001, "1111"));
    EXPECT_EQ(Bank::LoginResult::WrongPin, loaded.logIn(1002, "9999"));
}

TEST(Storage, PreservesTheFailedAttemptCounter) {
    TempFile file("test_attempts.txt");
    auto original = makeBank();
    original.logIn(1001, "0000");
    original.logIn(1001, "0000");
    ASSERT_TRUE(storage::save(original, file.path()).ok);

    Bank loaded;
    ASSERT_TRUE(storage::load(loaded, file.path()).ok);
    EXPECT_EQ(2, loaded.findAccount(1001)->failedAttempts());
    // One more failure locks it, proving the counter carried across.
    EXPECT_EQ(Bank::LoginResult::Locked, loaded.logIn(1001, "0000"));
}

TEST(Storage, TreatsAMissingFileAsAnEmptyBank) {
    Bank bank;
    const auto outcome = storage::load(bank, "definitely_does_not_exist_12345.txt");
    EXPECT_TRUE(outcome.ok);
    EXPECT_TRUE(bank.accounts().empty());
}

TEST(Storage, IgnoresBlankLines) {
    TempFile file("test_blank_lines.txt");
    file.write("\nACC,1001,Alice,abc,def,10000,0\n\n\n");
    Bank bank;
    EXPECT_TRUE(storage::load(bank, file.path()).ok);
    EXPECT_EQ(1u, bank.accounts().size());
}

TEST(Storage, RejectsAnUnknownRecordType) {
    TempFile file("test_unknown_record.txt");
    file.write("WAT,1001,Alice\n");
    Bank bank;
    const auto outcome = storage::load(bank, file.path());
    EXPECT_FALSE(outcome.ok);
    EXPECT_EQ(1, outcome.line);
}

TEST(Storage, RejectsAnAccountLineWithTooFewFields) {
    TempFile file("test_short_account.txt");
    file.write("ACC,1001,Alice\n");
    Bank bank;
    const auto outcome = storage::load(bank, file.path());
    EXPECT_FALSE(outcome.ok);
    EXPECT_EQ(1, outcome.line);
}

TEST(Storage, RejectsNonNumericFields) {
    TempFile file("test_non_numeric.txt");
    file.write("ACC,notanumber,Alice,abc,def,10000,0\n");
    Bank bank;
    EXPECT_FALSE(storage::load(bank, file.path()).ok);
}

TEST(Storage, RejectsATransactionBeforeAnyAccount) {
    TempFile file("test_orphan_txn.txt");
    file.write("TXN,Deposit,5000,Pay,15000\n");
    Bank bank;
    const auto outcome = storage::load(bank, file.path());
    EXPECT_FALSE(outcome.ok);
    EXPECT_EQ(1, outcome.line);
}

TEST(Storage, RejectsAnUnknownTransactionType) {
    TempFile file("test_bad_txn_type.txt");
    file.write("ACC,1001,Alice,abc,def,10000,0\nTXN,Nonsense,5000,Pay,15000\n");
    Bank bank;
    const auto outcome = storage::load(bank, file.path());
    EXPECT_FALSE(outcome.ok);
    EXPECT_EQ(2, outcome.line);
}

TEST(Storage, RejectsATruncatedFinalLine) {
    TempFile file("test_truncated.txt");
    file.write("ACC,1001,Alice,abc,def,10000,0\nTXN,Deposit,5000\n");
    Bank bank;
    EXPECT_FALSE(storage::load(bank, file.path()).ok);
}

// A failed load must not partially populate the caller's bank. Starting from
// a non-empty bank is what makes this test meaningful: if load() wrote
// directly into `bank`, the good first line would have landed before the bad
// second line aborted it.
TEST(Storage, LeavesTheBankUntouchedWhenLoadingFails) {
    TempFile file("test_atomic_load.txt");
    file.write("ACC,1001,Alice,abc,def,10000,0\nGARBAGE\n");

    Bank bank;
    bank.createAccount(7777, "Pre-existing", "9999", Money::fromPence(300));

    EXPECT_FALSE(storage::load(bank, file.path()).ok);
    ASSERT_EQ(1u, bank.accounts().size());
    EXPECT_EQ("Pre-existing", bank.accounts().front().name());
    EXPECT_EQ(nullptr, bank.findAccount(1001));   // the good line did not land
}

// --- Regression tests for review findings ---------------------------------

// The line-based format is destroyed by an unescaped newline: one record
// becomes two, and every later load fails with "Account record needs 7
// fields", locking the user out of their own data permanently.
TEST(StorageEscaping, EscapesNewlinesAndCarriageReturns) {
    // A real newline becomes the two characters backslash and 'n'.
    EXPECT_EQ("a\\nb", storage::escapeField("a\nb"));
    EXPECT_EQ("a\\rb", storage::escapeField("a\rb"));
    EXPECT_EQ("a\nb", storage::unescapeField("a\\nb"));
    EXPECT_EQ("a\rb", storage::unescapeField("a\\rb"));

    // Round trip through both directions, which is what the format relies on.
    const std::string awkward = "line one\nline two\r\n, and \\ too";
    EXPECT_EQ(awkward, storage::unescapeField(storage::escapeField(awkward)));
}

TEST(Storage, SurvivesANameContainingANewline) {
    TempFile file("test_newline_name.txt");
    Bank original;
    // Crafted to look like a second ACC record if the newline escaped the
    // format - the classic injection shape.
    original.createAccount(1001, "Eve\nACC,999,Mallory,aa,bb,999999,0", "1111",
                           Money::fromPence(100));
    ASSERT_TRUE(storage::save(original, file.path()).ok);

    Bank loaded;
    const auto outcome = storage::load(loaded, file.path());
    ASSERT_TRUE(outcome.ok) << outcome.error << " line " << outcome.line;
    ASSERT_EQ(1u, loaded.accounts().size());   // no injected second account
    EXPECT_EQ("Eve\nACC,999,Mallory,aa,bb,999999,0", loaded.findAccount(1001)->name());
    EXPECT_EQ(nullptr, loaded.findAccount(999));
}

// Bank::addAccount enforces none of createAccount's invariants and the loader
// is its only caller. Two records with the same number would both load, and
// findAccount would return only the first - stranding the second's money while
// save() faithfully rewrote it.
TEST(Storage, RejectsDuplicateAccountNumbers) {
    TempFile file("test_duplicate_accounts.txt");
    file.write("ACC,1001,Alice,abc,def,10000,0\nACC,1001,Eve,abc,def,500,0\n");
    Bank bank;
    const auto outcome = storage::load(bank, file.path());
    EXPECT_FALSE(outcome.ok);
    EXPECT_TRUE(bank.accounts().empty());
}

TEST(Storage, RejectsDuplicateAccountNumberOnTheFinalRecord) {
    TempFile file("test_duplicate_last.txt");
    file.write("ACC,1001,Alice,abc,def,10000,0\nTXN,Deposit,100,x,10100\n"
               "ACC,1001,Eve,abc,def,500,0\n");
    Bank bank;
    EXPECT_FALSE(storage::load(bank, file.path()).ok);
}

TEST(Storage, RejectsEmptyNameAndNegativeBalance) {
    TempFile emptyName("test_empty_name.txt");
    emptyName.write("ACC,1001,,abc,def,10000,0\n");
    Bank a;
    EXPECT_FALSE(storage::load(a, emptyName.path()).ok);

    TempFile negative("test_negative_balance.txt");
    negative.write("ACC,1001,Alice,abc,def,-500,0\n");
    Bank b;
    EXPECT_FALSE(storage::load(b, negative.path()).ok);
}

// save() must not leave a .tmp file behind on success.
TEST(Storage, DoesNotLeaveATemporaryFileBehind) {
    TempFile file("test_no_tmp.txt");
    TempFile temporary("test_no_tmp.txt.tmp");   // cleaned up even if left
    auto bank = makeBank();
    ASSERT_TRUE(storage::save(bank, file.path()).ok);

    std::ifstream leftover(temporary.path());
    EXPECT_FALSE(leftover.good());
}

// Saving over an existing file must replace it wholesale, not merge into it.
TEST(Storage, OverwritesAnExistingFileCompletely) {
    TempFile file("test_overwrite.txt");
    file.write("ACC,9999,Stale,aa,bb,777,0\n");

    Bank replacement;
    replacement.createAccount(1001, "Alice", "1111", Money::fromPence(100));
    ASSERT_TRUE(storage::save(replacement, file.path()).ok);

    Bank loaded;
    ASSERT_TRUE(storage::load(loaded, file.path()).ok);
    ASSERT_EQ(1u, loaded.accounts().size());
    EXPECT_EQ(nullptr, loaded.findAccount(9999));
}
