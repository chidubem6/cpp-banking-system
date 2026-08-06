#include "Cli.h"

#include <cstddef>
#include <ctime>
#include <exception>
#include <istream>
#include <ostream>
#include <string>

#include "Storage.h"
#include "Transaction.h"

namespace {
constexpr int kMenuCreate = 1;
constexpr int kMenuLogin = 2;
constexpr int kMenuExit = 3;
}  // namespace

Cli::Cli(std::istream& in, std::ostream& out) : in_{in}, out_{out} {}

int Cli::run(const std::string& dataFile) {
    dataFile_ = dataFile;

    const auto loaded = storage::load(bank_, dataFile_);
    if (!loaded.ok) {
        out_ << "Could not read " << dataFile_ << ": " << loaded.error;
        if (loaded.line > 0) out_ << " (line " << loaded.line << ")";
        out_ << "\nRefusing to start so the existing file is not overwritten.\n";
        // A file written before the ACC/TXN record prefixes existed will fail
        // here. It cannot be migrated - the old format stored PINs in plaintext
        // and there is no salt to recover - so say so rather than leave the
        // user guessing.
        out_ << "If this file predates the current save format, move it aside "
                "and start fresh; it cannot be migrated.\n";
        return 1;
    }

    mainMenu();

    const auto saved = storage::save(bank_, dataFile_);
    if (!saved.ok) {
        out_ << "Warning: could not save: " << saved.error << "\n";
        return 1;
    }
    return 0;
}

void Cli::persist() {
    const auto saved = storage::save(bank_, dataFile_);
    if (!saved.ok) {
        out_ << "Warning: change was not saved: " << saved.error << "\n";
    }
}

void Cli::mainMenu() {
    while (true) {
        out_ << "\n--- Banking System ---\n"
             << "1. Create account\n"
             << "2. Log in\n"
             << "3. Exit\n";

        const auto choice = readInt("Enter choice: ");
        if (!choice) return;   // end of input

        switch (*choice) {
            case kMenuCreate:
                createAccount();
                break;
            case kMenuLogin:
                loginFlow();
                break;
            case kMenuExit:
                return;
            default:
                out_ << "Please choose 1, 2 or 3.\n";
                break;
        }
    }
}

void Cli::createAccount() {
    const auto number = readInt("Account number: ");
    if (!number) return;
    const auto name = readLine("Full name: ");
    if (!name) return;
    const auto pin = readLine("PIN: ");
    if (!pin) return;
    const auto opening = readMoney("Opening balance (e.g. 100.00): ");
    if (!opening) return;

    switch (bank_.createAccount(*number, *name, *pin, *opening)) {
        case Bank::CreateResult::Ok:
            persist();
            out_ << "Account created.\n";
            break;
        case Bank::CreateResult::DuplicateAccountNumber:
            out_ << "That account number is already in use.\n";
            break;
        case Bank::CreateResult::InvalidName:
            out_ << "Name cannot be empty.\n";
            break;
        case Bank::CreateResult::InvalidPin:
            out_ << "PIN cannot be empty.\n";
            break;
        case Bank::CreateResult::InvalidAmount:
            out_ << "Opening balance cannot be negative.\n";
            break;
    }
}

void Cli::loginFlow() {
    const auto number = readInt("Account number: ");
    if (!number) return;
    const auto pin = readLine("PIN: ");
    if (!pin) return;

    // The clock is read here, at the edge, and passed inward. The domain layer
    // takes the time as a parameter so its lockout expiry can be tested
    // without sleeping.
    const auto result = bank_.logIn(*number, *pin, std::time(nullptr));
    // Persist before branching: a failed attempt has already incremented the
    // account's counter, and that increment is the entire lockout mechanism.
    // If it only reached disk on a clean exit, an attacker would reset it by
    // killing the process between guesses.
    persist();

    switch (result) {
        case Bank::LoginResult::Ok: {
            const Account* account = bank_.findAccount(*number);
            out_ << "Welcome, " << account->name() << ".\n";
            sessionMenu(*number);
            break;
        }
        case Bank::LoginResult::NotFound:
        case Bank::LoginResult::WrongPin:
        case Bank::LoginResult::Locked:
            // All three report identically, on purpose.
            //
            // A distinct "this account is locked" message would undo the very
            // thing the shared message exists for: probe a number with three
            // wrong PINs, and a different response on the third tells you the
            // account exists. Since the lock is permanent, that same probe is
            // also an irreversible denial of service against every account an
            // attacker can enumerate.
            out_ << "Incorrect account number or PIN.\n";
            break;
    }
}

void Cli::sessionMenu(int accountNumber) {
    while (true) {
        out_ << "\n1. Deposit\n"
             << "2. Withdraw\n"
             << "3. Transfer\n"
             << "4. Balance\n"
             << "5. Transaction history\n"
             << "6. Log out\n";

        const auto choice = readInt("Enter choice: ");
        if (!choice) return;

        switch (*choice) {
            case 1: deposit(accountNumber); break;
            case 2: withdraw(accountNumber); break;
            case 3: transfer(accountNumber); break;
            case 4: showBalance(accountNumber); break;
            case 5: showHistory(accountNumber); break;
            case 6: return;
            default: out_ << "Please choose 1 to 6.\n"; break;
        }
    }
}

void Cli::deposit(int accountNumber) {
    const auto amount = readMoney("Amount to deposit: ");
    if (!amount) return;

    Account* account = bank_.findAccount(accountNumber);
    switch (account->deposit(*amount, TransactionType::Deposit, "Money deposited")) {
        case Account::DepositResult::Ok:
            persist();
            out_ << "Deposited " << amount->toString() << ". Balance: "
                 << account->balance().toString() << "\n";
            break;
        case Account::DepositResult::InvalidAmount:
            out_ << "Amount must be greater than zero.\n";
            break;
        case Account::DepositResult::Overflow:
            out_ << "That would exceed the maximum balance.\n";
            break;
    }
}

void Cli::withdraw(int accountNumber) {
    const auto amount = readMoney("Amount to withdraw: ");
    if (!amount) return;

    Account* account = bank_.findAccount(accountNumber);
    switch (account->withdraw(*amount, TransactionType::Withdrawal, "Money withdrawn")) {
        case Account::WithdrawResult::Ok:
            persist();
            out_ << "Withdrew " << amount->toString() << ". Balance: "
                 << account->balance().toString() << "\n";
            break;
        case Account::WithdrawResult::InvalidAmount:
            out_ << "Amount must be greater than zero.\n";
            break;
        case Account::WithdrawResult::InsufficientFunds:
            out_ << "Insufficient funds. Balance: " << account->balance().toString() << "\n";
            break;
    }
}

void Cli::transfer(int accountNumber) {
    const auto target = readInt("Recipient account number: ");
    if (!target) return;
    const auto amount = readMoney("Amount to transfer: ");
    if (!amount) return;

    switch (bank_.transfer(accountNumber, *target, *amount)) {
        case Bank::TransferResult::Ok:
            persist();
            out_ << "Sent " << amount->toString() << " to " << bank_.findAccount(*target)->name()
                 << ". Balance: " << bank_.findAccount(accountNumber)->balance().toString() << "\n";
            break;
        case Bank::TransferResult::ReceiverNotFound:
            out_ << "No account with that number.\n";
            break;
        case Bank::TransferResult::SenderNotFound:
            out_ << "Your account could not be found.\n";
            break;
        case Bank::TransferResult::SameAccount:
            out_ << "You cannot transfer to your own account.\n";
            break;
        case Bank::TransferResult::InvalidAmount:
            out_ << "Amount must be greater than zero.\n";
            break;
        case Bank::TransferResult::InsufficientFunds:
            out_ << "Insufficient funds.\n";
            break;
        case Bank::TransferResult::Overflow:
            out_ << "That transfer would exceed the recipient's maximum balance.\n";
            break;
    }
}

void Cli::showBalance(int accountNumber) {
    out_ << "Balance: " << bank_.findAccount(accountNumber)->balance().toString() << "\n";
}

void Cli::showHistory(int accountNumber) {
    const auto& history = bank_.findAccount(accountNumber)->history();
    if (history.empty()) {
        out_ << "No transactions yet.\n";
        return;
    }
    for (const auto& transaction : history) {
        out_ << toDisplayString(transaction.type()) << "  " << transaction.amount().toString()
             << "  " << transaction.details() << "  (balance "
             << transaction.resultingBalance().toString() << ")\n";
    }
}

std::optional<int> Cli::readInt(const std::string& prompt) {
    while (true) {
        out_ << prompt;
        std::string line;
        if (!std::getline(in_, line)) return std::nullopt;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        try {
            std::size_t consumed = 0;
            const int value = std::stoi(line, &consumed);
            if (consumed == line.size() && !line.empty()) return value;
        } catch (const std::exception&) {
            // Fall through to the retry message. This conversion sits at the
            // program's input boundary, where a throw is caught immediately
            // and turned into a prompt - not threaded through control flow.
        }
        out_ << "Please enter a whole number.\n";
    }
}

std::optional<std::string> Cli::readLine(const std::string& prompt) {
    out_ << prompt;
    std::string line;
    if (!std::getline(in_, line)) return std::nullopt;

    // Strip a trailing CR so a CRLF-authored script piped in on Linux behaves.
    // Without this every menu choice fails to parse and the program burns
    // through the whole script doing nothing - and a name would silently
    // acquire a trailing '\r'. Storage::load already handles this on its own
    // input; the two paths should agree.
    if (!line.empty() && line.back() == '\r') line.pop_back();
    return line;
}

std::optional<Money> Cli::readMoney(const std::string& prompt) {
    while (true) {
        out_ << prompt;
        std::string line;
        if (!std::getline(in_, line)) return std::nullopt;
        if (!line.empty() && line.back() == '\r') line.pop_back();

        if (auto amount = Money::fromString(line)) return amount;
        out_ << "Please enter an amount such as 100 or 12.34.\n";
    }
}
