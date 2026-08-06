#include <iostream>
#include "Bank.h"

int main() {
    Bank bank;
    bank.loadFromFile("accounts.txt");
    int choice;

    do {
        std::cout << "\n--- Banking System ---\n";
        std::cout << "1. Create Account\n";
        std::cout << "2. Login\n";
        std::cout << "3. Exit\n";
        std::cout << "Enter choice: ";
        std::cin >> choice;

        if (choice == 1) {
            int accNum;
            std::string name, pin;
            double balance;

            std::cout << "Enter account number: ";
            std::cin >> accNum;
            std::cout << "Enter name: ";
            std::cin >> name;
            std::cout << "Enter PIN: ";
            std::cin >> pin;
            std::cout << "Enter initial balance: ";
            std::cin >> balance;

            bank.createAccount(accNum, name, pin, balance);
        }
        else if (choice == 2) {
            int accNum;
            std::string pin;

            std::cout << "Enter account number: ";
            std::cin >> accNum;
            std::cout << "Enter PIN: ";
            std::cin >> pin;

            Account* user = bank.logIn(accNum, pin);
            
            if (user != nullptr) {
                std::cout << "Login successful. Welcome, " << user->getName() << "\n";

                int loginChoice;
                do {
                    std::cout << "1. Deposit\n";
                    std::cout << "2. Withdraw\n";
                    std::cout << "3. Transfer\n";
                    std::cout << "4. Check Balance\n";
                    std::cout << "5. Transaction History\n";
                    std::cout << "6. Logout\n";
                    std::cout << "Enter choice: ";
                    std::cin >> loginChoice;

                    if (loginChoice == 1) {
                        double amount;
                        std::cout << "Input deposit amount: ";
                        std::cin >> amount;
                        if (user->deposit(amount))
                            std::cout << "Deposit successful.\n";
                        else
                            std::cout << "Deposit failed. Amount must be greater than 0.\n";
                    }
                    else if (loginChoice == 2) {
                        double amount;
                        std::cout << "Input withdraw amount: ";
                        std::cin >> amount;
                        if (user->withdraw(amount))
                            std::cout << "Withdrawal successful.\n";
                        else
                            std::cout << "Withdrawal failed. Insufficient balance or invalid amount.\n";
                    }
                    else if (loginChoice == 3) {
                        int receiverAccNum;
                        double amount;

                        std::cout << "Enter account number: ";
                        std::cin >> receiverAccNum;

                        std::cout << "Enter the amount: ";
                        std::cin >> amount;
                        bank.transfer(user->getAccountNumber(), receiverAccNum, amount);
                    }
                    else if (loginChoice == 4) {
                        std::cout << "Balance: $" << user->getBalance() << "\n";
                    }
                    else if (loginChoice == 5) {
                        user->showTransactionHistory();
                    }

                } while (loginChoice != 6);

            } else {
                std::cout << "Invalid login.\n";
            } 
        }
    } while (choice != 3);

    bank.saveToFile("accounts.txt");
    return 0;
}