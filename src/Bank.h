#ifndef BANK_H
#define BANK_H

#include <vector>
#include "Account.h"

class Bank {
    private:
        std::vector<Account> accounts;

    public:
        void createAccount(int accNum, const std::string& accName, const std::string& accPin, double accBalance);
        
        Account* findAccount(int accNum);

        Account* logIn(int accNum, const std::string& inputPin);

        void transfer(int senderAccNum, int receiverAccNum, double amount);

        void saveToFile(const std::string& filename);

        void loadFromFile(const std::string& filename);
};

#endif