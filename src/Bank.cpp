#include <iostream>
#include <fstream>
#include <sstream>
#include "Bank.h"
#include "Transaction.h"

void Bank::createAccount(int accNum, const std::string& accName, const std::string& accPin, double accBalance) {
    if (findAccount(accNum)) {
        std::cout << "Create account unsuccessful. Account already exists." << "\n";

    } else {
        accounts.push_back(Account(accNum, accName, accPin, accBalance));
        std::cout << "Account creation successful." << "\n";
    }
};

Account* Bank::findAccount(int accNum) {
    for (auto& account : accounts) {
        if (account.getAccountNumber() == accNum) {
            return &account;
        }
    } return nullptr;
};

Account* Bank::logIn(int accNum, const std::string& inputPin) {
    Account* account = findAccount(accNum);

    if (account != nullptr && account->verifyPin(inputPin)) {
        return account;
    }
    return nullptr;
    
}

void Bank::transfer(int senderAccNum, int receiverAccNum, double amount) {
    Account* sender = findAccount(senderAccNum);
    Account* receiver = findAccount(receiverAccNum);

    if (sender != nullptr && receiver != nullptr) {
        if (sender->withdraw(amount, "Sent to " + receiver->getName())) {
            receiver->deposit(amount, "Received from " + sender->getName());
            std::cout << "Transfer successful. $" << amount << " was sent to " << receiver->getName() << "\n";
        } else {
            std::cout << "Insufficient Balance\n";
        }
        

    } else {
        std::cout << "Transfer unsuccessful.\n" ;
    }
}

void Bank::saveToFile(const std::string& filename) {
    // Implementation for saving accounts and transactions to a file
    std::ofstream file(filename);

    if (!file) {
        std::cerr << "Error opening file for writing: " << filename << std::endl;
        return;
    }

    for (const auto& account : accounts) {
        file << account.getAccountNumber() << "," 
        << account.getName() << "," 
        << account.getPin() << "," 
        << account.getBalance() 
        << "\n";

        for (const auto& transaction : account.getTransactionHistory()) {
            file << transaction.getType() << "," 
            << transaction.getAmount() << "," 
            << transaction.getDetails() << "," 
            << transaction.getResultingBalance() 
            << "\n";
        }
    }

    file.close();
}

void Bank::loadFromFile(const std::string& filename) {
    // Implementation for loading accounts and transactions from a file
    std::ifstream file(filename);

    if (!file) {
        std::cerr << "Error opening file for reading: " << filename << std::endl;
        return;
    }

    Account* currentAccount = nullptr;
    std::string line;

    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string firstField;
        std::getline(ss, firstField, ',');

        try {
            // try treating it as an account line
            int accNum = std::stoi(firstField);
            // parse remaining fields: name, pin, balance
            std::string name, pin, balanceStr;
            std::getline(ss, name, ',');
            std::getline(ss, pin, ',');
            std::getline(ss, balanceStr, ',');
            double balance = std::stod(balanceStr);
            // call createAccount
            createAccount(accNum, name, pin, balance);
            // set currentAccount = findAccount(accNum)
            currentAccount = findAccount(accNum);

        } catch (std::exception& e) {
            // it's a transaction line
            // firstField is already the type
            std::string type = firstField;
            std::string amountStr, details, resultingBalanceStr;

            // parse: amount, details, resultingBalance
            std::getline(ss, amountStr, ',');
            std::getline(ss, details, ',');
            std::getline(ss, resultingBalanceStr, ',');
            double amount = std::stod(amountStr);
            double resultingBalance = std::stod(resultingBalanceStr);

            // if currentAccount != nullptr, addTransaction
            if (currentAccount) {
                currentAccount->addTransaction(Transaction(type, amount, details, resultingBalance));
            } else {
                std::cerr << "Error: Transaction found without an associated account." << std::endl;
            }
        }
    }
    file.close();
}
