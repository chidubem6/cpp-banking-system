#ifndef CLI_H
#define CLI_H

#include <iosfwd>
#include <optional>
#include <string>

#include "Bank.h"
#include "Money.h"

// Owns every line of console output and all input validation. The only part
// of the program that talks to a human.
//
// Streams are taken by reference rather than reaching for std::cin and
// std::cout directly, so the menu loop can be driven by a test later without
// redesigning anything.
class Cli {
public:
    Cli(std::istream& in, std::ostream& out);

    // Loads dataFile, runs the menu loop, saves on exit. Returns a process
    // exit code.
    int run(const std::string& dataFile);

private:
    void mainMenu();
    void createAccount();
    void loginFlow();
    void sessionMenu(int accountNumber);

    void deposit(int accountNumber);
    void withdraw(int accountNumber);
    void transfer(int accountNumber);
    void showBalance(int accountNumber);
    void showHistory(int accountNumber);

    // Input helpers. Each returns nullopt at end-of-input so the program can
    // exit cleanly instead of looping forever.
    std::optional<int> readInt(const std::string& prompt);
    std::optional<std::string> readLine(const std::string& prompt);
    std::optional<Money> readMoney(const std::string& prompt);

    // Writes the bank to disk after every state change. Saving only on a clean
    // exit made the lockout counter resettable by killing the process, and lost
    // every committed transaction on a crash despite having reported success.
    void persist();

    std::istream& in_;
    std::ostream& out_;
    Bank bank_;
    std::string dataFile_;
};

#endif
