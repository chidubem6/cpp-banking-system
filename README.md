# 💳 Banking System (C++)

## 📌 Overview

A command-line banking system built in C++ that simulates core banking operations such as account creation, authentication, deposits, withdrawals, transfers, and transaction tracking.

This project is designed to demonstrate **object-oriented programming**, **data structures**, and **file persistence** in a practical, real-world system.

---

## 🎯 Goals

- Practice C++ fundamentals (OOP, STL, file handling)
- Build a stateful system with business logic
- Simulate real backend system behaviour
- Produce a project suitable for technical interviews

---

## 🚀 Features

### 👤 Account Management
- Create new accounts
- Unique account numbers
- Secure login using PIN

### 🔐 Authentication
- Login with account number + PIN
- Validation of credentials

### 💰 Transactions
- Deposit money
- Withdraw money (with balance checks)
- Transfer money between accounts

### 📊 Account Info
- View balance
- View transaction history

### 💾 Persistence
- Save account data to file
- Load data on program startup

---

## 🧱 System Design

### Classes

#### `Transaction`
Represents a single transaction.

Fields:
- `type` (Deposit, Withdrawal, Transfer)
- `amount`
- `details`
- `resultingBalance`

---

#### `Account`
Represents a bank account.

Fields:
- `accountNumber`
- `name`
- `pin`
- `balance`
- `transactionHistory`

Methods:
- `deposit()`
- `withdraw()`
- `transfer()`
- `displayBalance()`
- `displayHistory()`
- `checkPin()`

---

#### `Bank`
Manages all accounts.

Fields:
- `accounts` (vector)

Methods:
- `createAccount()`
- `findAccount()`
- `login()`
- `saveToFile()`
- `loadFromFile()`

---

## 🔄 Program Flow

### Main Menu
```

1. Create Account
2. Login
3. Exit

```

### After Login
```

1. Deposit
2. Withdraw
3. Transfer
4. Check Balance
5. View Transaction History
6. Logout

```

---

## 📂 Project Structure

```

cpp-banking-system/
│
├── main.cpp
├── Account.h
├── Account.cpp
├── Bank.h
├── Bank.cpp
├── Transaction.h
├── Transaction.cpp
├── accounts.txt
└── README.md

````

---

## ⚙️ Requirements

- C++17 or later
- g++ or any C++ compiler

---

## 🛠️ How to Run

### Compile
```bash
g++ main.cpp Account.cpp Bank.cpp Transaction.cpp -o bank
````

### Run

```bash
./bank
```

---

## ✅ Validation Rules

* No negative deposits or withdrawals
* Cannot withdraw more than balance
* Cannot transfer to non-existent account
* Account numbers must be unique
* Invalid inputs are handled gracefully

---

## 📈 Development Phases

### Phase 1 (Core)

* Account creation
* Login
* Deposit
* Withdraw
* Check balance

### Phase 2 (Intermediate)

* Transfer system
* Transaction history

### Phase 3 (Advanced)

* File persistence
* Input validation improvements
* Code modularisation

---

## 🔥 Stretch Features

* Account types (Savings / Current)
* Interest calculation
* Admin panel
* Change PIN
* Account lock after failed attempts
* Transaction timestamps
* Export transactions (CSV)

---

## 🧠 Skills Demonstrated

* Object-Oriented Programming (OOP)
* Data structures (vectors, searching)
* File I/O
* Input validation
* System design thinking
* Modular architecture

---

## 📌 Example Resume Description

**Banking System | C++**

* Built a command-line banking application using object-oriented design to manage accounts, transactions, and inter-account transfers
* Implemented persistent file storage and transaction history tracking with robust input validation and modular architecture

## 👤 Author

Chidubem
