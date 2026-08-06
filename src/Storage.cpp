#include "Storage.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

namespace storage {
namespace {

// Parses a whole decimal integer. Returns false on empty input, stray
// characters, or overflow. Deliberately avoids std::stoll, which reports
// failure by throwing - the habit this rewrite is removing.
bool parseInt64(const std::string& text, std::int64_t& out) {
    if (text.empty()) return false;

    std::size_t index = 0;
    bool negative = false;
    if (text[0] == '-') {
        negative = true;
        index = 1;
        if (text.size() == 1) return false;
    }

    std::int64_t value = 0;
    for (; index < text.size(); ++index) {
        const char c = text[index];
        if (c < '0' || c > '9') return false;
        const int digit = c - '0';
        if (value > (std::numeric_limits<std::int64_t>::max() - digit) / 10) return false;
        value = value * 10 + digit;
    }
    out = negative ? -value : value;
    return true;
}

bool parseInt(const std::string& text, int& out) {
    std::int64_t wide = 0;
    if (!parseInt64(text, wide)) return false;
    if (wide < std::numeric_limits<int>::min() || wide > std::numeric_limits<int>::max()) {
        return false;
    }
    out = static_cast<int>(wide);
    return true;
}

LoadOutcome failure(std::string message, int line) {
    LoadOutcome outcome;
    outcome.ok = false;
    outcome.error = std::move(message);
    outcome.line = line;
    return outcome;
}

}  // namespace

// Newlines must be escaped too, not just the field separator. The format is
// line-based, so an unescaped newline in a name splits one record into two and
// every subsequent load fails with "Account record needs 7 fields" - locking
// the user out of their own data with no way back. The CLI cannot produce one
// today (readLine stops at a newline), but Bank::createAccount imposes no
// character restriction, so nothing enforces that.
std::string escapeField(const std::string& field) {
    std::string out;
    out.reserve(field.size());
    for (char c : field) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case ',':  out += "\\,";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

std::string unescapeField(const std::string& field) {
    std::string out;
    out.reserve(field.size());
    for (std::size_t i = 0; i < field.size(); ++i) {
        if (field[i] != '\\' || i + 1 >= field.size()) {
            out += field[i];
            continue;
        }
        // Mapped escapes must match escapeField exactly. Anything else keeps
        // the following character verbatim, which covers '\\' and ','.
        const char next = field[++i];
        switch (next) {
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            default:  out += next; break;
        }
    }
    return out;
}

std::vector<std::string> splitEscaped(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    for (std::size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\\' && i + 1 < line.size()) {
            current += line[i];
            current += line[++i];
        } else if (line[i] == ',') {
            fields.push_back(unescapeField(current));
            current.clear();
        } else {
            current += line[i];
        }
    }
    fields.push_back(unescapeField(current));
    return fields;
}

SaveOutcome save(const Bank& bank, const std::string& path) {
    // Write to a temporary file and rename over the target, so an interrupted
    // write cannot destroy the existing data. Opening `path` directly would
    // truncate the only copy before a single byte was written.
    const std::string temporaryPath = path + ".tmp";

    std::ofstream file(temporaryPath);
    if (!file) return {false, "Could not open " + temporaryPath + " for writing"};

    for (const auto& account : bank.accounts()) {
        file << "ACC," << account.accountNumber() << ',' << escapeField(account.name()) << ','
             << escapeField(account.credential().salt) << ','
             << escapeField(account.credential().hash) << ',' << account.balance().pence() << ','
             << account.failedAttempts() << '\n';

        for (const auto& transaction : account.history()) {
            file << "TXN," << toStorageString(transaction.type()) << ','
                 << transaction.amount().pence() << ',' << escapeField(transaction.details()) << ','
                 << transaction.resultingBalance().pence() << '\n';
        }
    }

    // Close explicitly before checking. Testing the stream while it still
    // holds buffered data reports success for a write that has not happened
    // yet - the flush occurs in the destructor, after the check.
    file.close();
    if (!file) {
        std::error_code ignored;
        std::filesystem::remove(temporaryPath, ignored);
        return {false, "Failed while writing " + temporaryPath};
    }

    std::error_code renameError;
    std::filesystem::rename(temporaryPath, path, renameError);
    if (renameError) {
        std::error_code ignored;
        std::filesystem::remove(temporaryPath, ignored);
        return {false, "Could not replace " + path + ": " + renameError.message()};
    }
    return {true, ""};
}

LoadOutcome load(Bank& bank, const std::string& path) {
    // A missing file means a first run. A file that exists but will not open
    // means something is wrong - locked by another process, or a permissions
    // problem - and must NOT be reported as an empty bank. Conflating the two
    // loses data: the caller starts empty, then saves over the real file on
    // exit. `!file` alone cannot tell them apart.
    std::error_code existsError;
    if (!std::filesystem::exists(path, existsError)) return {true, "", 0};

    std::ifstream file(path);
    if (!file) {
        return failure("File exists but could not be opened for reading", 0);
    }

    // Build into a scratch bank so a malformed file cannot leave the caller
    // holding half a dataset.
    Bank scratch;

    struct PendingAccount {
        int number{};
        std::string name;
        PinCredential credential;
        Money balance;
        int failedAttempts{};
        std::vector<Transaction> history;
        bool valid{false};
    } pending;

    // Bank::addAccount enforces none of the invariants Bank::createAccount
    // does, and the loader is its only caller, so the checks have to live
    // here. Without them a file containing two ACC records with the same
    // number loads both; findAccount returns the first every time, so the
    // second account's balance and history are unreachable while save()
    // faithfully rewrites them - money stranded invisibly and permanently.
    auto flush = [&scratch, &pending]() -> bool {
        if (!pending.valid) return true;
        if (scratch.findAccount(pending.number) != nullptr) return false;

        scratch.addAccount(Account(pending.number, std::move(pending.name),
                                   std::move(pending.credential), pending.balance,
                                   pending.failedAttempts, std::move(pending.history)));
        pending = PendingAccount{};
        return true;
    };

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();   // CRLF files
        if (line.empty()) continue;

        const auto fields = splitEscaped(line);

        if (fields[0] == "ACC") {
            if (fields.size() != 7) return failure("Account record needs 7 fields", lineNumber);

            if (!flush()) return failure("Duplicate account number", lineNumber);

            if (fields[2].empty()) return failure("Account name cannot be empty", lineNumber);

            int number = 0;
            std::int64_t balancePence = 0;
            int attempts = 0;
            if (!parseInt(fields[1], number)) return failure("Bad account number", lineNumber);
            if (!parseInt64(fields[5], balancePence)) return failure("Bad balance", lineNumber);
            if (!parseInt(fields[6], attempts)) return failure("Bad attempt count", lineNumber);
            if (attempts < 0) return failure("Negative attempt count", lineNumber);
            if (balancePence < 0) return failure("Negative balance", lineNumber);

            pending.number = number;
            pending.name = fields[2];
            pending.credential = restorePinCredential(fields[3], fields[4]);
            pending.balance = Money::fromPence(balancePence);
            pending.failedAttempts = attempts;
            pending.valid = true;

        } else if (fields[0] == "TXN") {
            if (fields.size() != 5) return failure("Transaction record needs 5 fields", lineNumber);
            if (!pending.valid) return failure("Transaction before any account", lineNumber);

            const auto type = transactionTypeFromStorageString(fields[1]);
            if (!type) return failure("Unknown transaction type", lineNumber);

            std::int64_t amountPence = 0;
            std::int64_t resultingPence = 0;
            if (!parseInt64(fields[2], amountPence)) return failure("Bad amount", lineNumber);
            if (!parseInt64(fields[4], resultingPence)) {
                return failure("Bad resulting balance", lineNumber);
            }

            pending.history.emplace_back(*type, Money::fromPence(amountPence), fields[3],
                                         Money::fromPence(resultingPence));

        } else {
            return failure("Unknown record type '" + fields[0] + "'", lineNumber);
        }
    }

    if (!flush()) return failure("Duplicate account number", lineNumber);

    bank = std::move(scratch);
    return {true, "", 0};
}

}  // namespace storage
