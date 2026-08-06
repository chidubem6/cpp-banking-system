#ifndef TRANSACTION_H
#define TRANSACTION_H
#include <string>

class Transaction {
    private:
        std::string type;
        double amount;
        std::string details;
        double resultingBalance;

    public:
        Transaction(std::string type, double amount, std::string details, double reusltingBalance)
            : type{ type }, amount { amount }, details { details }, resultingBalance { resultingBalance } {}

        std::string getType() const { return type; };
        double getAmount() const { return amount; };
        std::string getDetails() const { return details; };
        double getResultingBalance() const { return resultingBalance; };

        std::string toString () const;
};

#endif