#ifndef ACCOUNT_H
#define ACCOUNT_H
#include <string>
#include <vector>
#include "Transaction.h"

class Account {
    private:
        int accountNumber{};
        std::string name{};
        std::string pin{};
        double balance{};
        std::vector<Transaction> transactionHistory;
    
    public:
        // Constructor
        Account(int accNum, const std::string& accName, const std::string& accPin, double accBalance)
            : accountNumber{ accNum }, name{ accName }, pin{ accPin }, balance{ accBalance } {}

        // Methods
        int getAccountNumber() const { return accountNumber; }
        const std::string& getName() const { return name; }
        double getBalance() const { return balance; }
        void setBalance(double newBalance) { balance = newBalance; }
        const std::string& getPin() const { return pin; }

        bool verifyPin(const std::string& inputPin) const { return inputPin == pin; }

        bool deposit(double amount, const std::string& details = "Money deposited");
        bool withdraw(double amount, const std::string& details = "Money withdrawn");

        void addTransaction(const Transaction& transaction);
        void showTransactionHistory() const;
        const std::vector<Transaction>& getTransactionHistory() const { return transactionHistory; }
    
};
#endif