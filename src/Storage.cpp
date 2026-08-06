#include "Storage.h"

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <limits>
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

std::string escapeField(const std::string& field) {
    std::string out;
    out.reserve(field.size());
    for (char c : field) {
        if (c == '\\' || c == ',') out += '\\';
        out += c;
    }
    return out;
}

std::string unescapeField(const std::string& field) {
    std::string out;
    out.reserve(field.size());
    for (std::size_t i = 0; i < field.size(); ++i) {
        if (field[i] == '\\' && i + 1 < field.size()) {
            out += field[++i];
        } else {
            out += field[i];
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
    std::ofstream file(path);
    if (!file) return {false, "Could not open " + path + " for writing"};

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

    if (!file) return {false, "Failed while writing " + path};
    return {true, ""};
}

LoadOutcome load(Bank& bank, const std::string& path) {
    std::ifstream file(path);
    if (!file) return {true, "", 0};   // first run

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

    auto flush = [&scratch, &pending]() {
        if (!pending.valid) return;
        scratch.addAccount(Account(pending.number, pending.name, pending.credential,
                                   pending.balance, pending.failedAttempts,
                                   std::move(pending.history)));
        pending = PendingAccount{};
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

            flush();

            int number = 0;
            std::int64_t balancePence = 0;
            int attempts = 0;
            if (!parseInt(fields[1], number)) return failure("Bad account number", lineNumber);
            if (!parseInt64(fields[5], balancePence)) return failure("Bad balance", lineNumber);
            if (!parseInt(fields[6], attempts)) return failure("Bad attempt count", lineNumber);
            if (attempts < 0) return failure("Negative attempt count", lineNumber);

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

    flush();
    bank = std::move(scratch);
    return {true, "", 0};
}

}  // namespace storage
