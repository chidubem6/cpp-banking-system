# Banking System (C++)

A command-line banking system in C++17: accounts, PIN authentication with lockout,
deposits, withdrawals, transfers, transaction history, and file persistence.

[![CI](https://github.com/chidubem6/cpp-banking-system/actions/workflows/ci.yml/badge.svg)](https://github.com/chidubem6/cpp-banking-system/actions/workflows/ci.yml)

Built to be correct rather than large: money is exact, every business rule is
tested, and the reasoning behind each decision is written down below.

---

## Example session

```
--- Banking System ---
1. Create account
2. Log in
3. Exit
Enter choice: 1
Account number: 1001
Full name: Alice Walker
PIN: 1234
Opening balance (e.g. 100.00): 250.00
Account created.

--- Banking System ---
1. Create account
2. Log in
3. Exit
Enter choice: 2
Account number: 1001
PIN: 1234
Welcome, Alice Walker.

1. Deposit
2. Withdraw
3. Transfer
4. Balance
5. Transaction history
6. Log out
Enter choice: 1
Amount to deposit: 75.50
Deposited £75.50. Balance: £325.50

Enter choice: 5
Deposit  £75.50  Money deposited  (balance £325.50)

Enter choice: 4
Balance: £325.50
```

---

## Build and run

Requires CMake 3.20 or later and a C++17 compiler. GoogleTest is fetched
automatically at configure time, so the first build needs network access.

```bash
cmake -B build
cmake --build build
ctest --test-dir build -C Debug --output-on-failure
```

Then run the binary — `build/bank` with Make or Ninja, `build/Debug/bank.exe`
with Visual Studio.

The `-C Debug` on the test command is required by multi-config generators such
as Visual Studio. Without it CTest reports "No tests were found".

---

## Architecture

| Module | Responsibility |
|---|---|
| `Money` | GBP as an exact `int64_t` count of pence; parsing, formatting, checked arithmetic |
| `Sha256` | The digest, and nothing else |
| `PinCredential` | Salt generation, PIN hashing, verification |
| `Transaction` | One immutable ledger entry |
| `Account` | Balance, credential, lockout counter, history |
| `Bank` | Account collection, login, transfers. Returns results |
| `Storage` | The save-file format: escaping, parsing, round-tripping |
| `Cli` | Menus, input validation, every line of console output |

**Domain code never writes to the console; the CLI owns all output.** That
separation is what makes the domain testable — and it is the single change that
took this project from zero tests to 82.

---

## Design decisions

### Money is an integer count of pence

Binary floating point cannot represent 0.1 exactly, so `double` balances drift as
transactions accumulate. A ledger that does not balance defeats the purpose of
the program. `tests/test_money.cpp` performs 10,000 additions of 10p and asserts
the total is exactly £1000.00 — a test that fails under `double`.

The general term is the currency's *minor unit* (ISO 4217). "Cents" is
USD-specific and the ×100 relationship is not universal: JPY has no minor unit,
KWD uses thousandths. This type is deliberately GBP-specific rather than
pretending to be general.

### Business outcomes are return values, not exceptions

Insufficient funds is an ordinary, anticipated outcome — not an exceptional one.
Operations that can fail in more than one way return an `enum class`; exceptions
are reserved for genuinely broken invariants.

The previous version used `try`/`catch` to decide whether a line of the save file
was an account or a transaction. The format now says so explicitly.

### A transfer validates both legs before mutating either

Otherwise a rejected transfer could debit the sender without crediting the
recipient. `Bank.FailedTransferLeavesBothBalancesUnchanged` covers exactly this.

### The save format escapes its fields

Records carry an explicit `ACC` or `TXN` prefix, and `\` and `,` are escaped —
backslash first, so the transformation is reversible. The previous plain-CSV
format silently corrupted any name containing a comma.

A malformed file is rejected wholesale rather than partially loaded, so a corrupt
save cannot leave the program holding half a dataset.

### `logIn` returns a result, not a pointer

`findAccount` returns a pointer into a `std::vector<Account>`, which reallocation
invalidates. The CLI holds the account *number* for the session and re-resolves
it per operation, keeping every pointer's lifetime inside a single operation.

### No repository interface

There is one storage backend and no second one planned. An interface for a single
implementation is speculative generality — it can be introduced when a second
backend actually exists.

### Login does not distinguish unknown accounts from wrong PINs

Both report "Incorrect account number or PIN." Telling an attacker which account
numbers exist is free reconnaissance.

---

## Testing

82 tests, run on Linux (GCC) and Windows (MSVC) on every push and pull request.

| Area | What is covered |
|---|---|
| `Money` | Parsing, rejection of malformed input, exact arithmetic, formatting, overflow |
| `Sha256` | The four published FIPS 180-2 test vectors |
| `PinCredential` | Salting, verification, that the PIN is never stored |
| `Account` | Deposit and withdrawal rules, lockout, counter reset |
| `Bank` | Every result branch, including that a failed transfer moves no money |
| `Storage` | Round-trips, names containing commas, truncated and malformed files |

The interactive menu loop is deliberately untested: exercising `std::cin` prompts
needs I/O plumbing worth more than it returns here. The CLI is kept as a thin
shell over tested logic, and `Cli` takes its streams by reference so it can be
driven by a test later without redesign.

Warnings are enabled on both toolchains (`/W4`, `-Wall -Wextra -Wpedantic`). That
is not decoration: `/W4` is what surfaced the uninitialised-read defect described
below.

---

## Known limitations

Stated deliberately. Each is a boundary, not an oversight.

- **PIN hashing uses SHA-256.** SHA-256 is fast, and speed is what an attacker
  wants when brute-forcing a four-digit PIN. Production needs a deliberately slow
  key-derivation function — Argon2, scrypt or bcrypt. Note also that a four-digit
  PIN has only 10,000 possibilities, so the real defence is lockout, not the hash.
- **A locked account cannot be unlocked.** There is no administrator role, so
  three failed attempts locks the account permanently. A real system would offer
  time-based expiry or an administrative reset.
- **Salt generation uses `std::mt19937_64`**, seeded from `std::random_device`.
  Adequate for demonstration, not a cryptographically secure generator.
- **Storage is a plain text file rewritten in full on exit.** There is no
  journaling, so a crash mid-write loses the session. A real system would write to
  a temporary file and rename it atomically.
- **Single user, no concurrency.** Nothing guards against two processes opening
  the same file.
- **GBP only.** `Money` hard-codes a two-decimal minor unit and the £ symbol.

---

## A note on the history

This project began as a working but rough command-line program. The rework is
recorded as a series of focused pull requests, each of which builds and passes
its tests on its own.

The most instructive was a one-word bug. `Transaction`'s constructor parameter
was misspelled `reusltingBalance`, so this member initialiser:

```cpp
: resultingBalance{resultingBalance}
```

did not resolve to the parameter — it resolved to the member being initialised.
Every transaction read its own uninitialised memory and discarded the value it
was given, then wrote the result to disk:

```
Deposit,50,Money deposited,2.87198e-312
```

It compiled cleanly at default warning levels and the account balance looked
correct, so nothing appeared wrong until the ledger was inspected. `/W4` reported
it as an unreferenced parameter. There is now a regression test.

Design notes and the implementation plan are in [`docs/superpowers/`](docs/superpowers/).

---

## Author

Chidubem
