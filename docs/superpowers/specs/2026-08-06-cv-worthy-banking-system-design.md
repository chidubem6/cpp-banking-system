# Design: Making the C++ Banking System CV-Worthy

**Date:** 2026-08-06
**Status:** Approved, ready for implementation planning
**Author:** Chidubem (with Claude)

---

## Goal

Turn a working-but-rough C++ CLI banking system into a portfolio project that stands
up to review for **graduate / junior software engineer** roles.

The deliverable is not more features. It is evidence of professional practice —
correctness, tests, CI, and documented reasoning — plus the ability to defend every
decision in an interview.

**Constraint:** one working day. Claude writes the code; Chidubem reviews and must
understand every decision. Review bandwidth, not typing speed, is the binding limit.

---

## Starting state

A C++17 CLI application, 25 commits, ~500 lines across four translation units:

- `Account` — account entity, holds a `std::vector<Transaction>`
- `Bank` — owns `std::vector<Account>`, plus transfer logic *and* file persistence
- `Transaction` — value object
- `main.cpp` — the entire CLI

It compiles and runs. It has no tests, no build system, and no CI.

### Defects found during exploration

| # | Defect | Location | Severity |
|---|--------|----------|----------|
| 1 | Uninitialised member read (undefined behaviour) | `Transaction.h:13` | **Critical** |
| 2 | `double` used for currency | throughout | High |
| 3 | Exceptions used for control flow in the parser | `Bank.cpp:98-131` | High |
| 4 | Domain logic writes to `std::cout` | `Bank.cpp:42,44,49` | High (blocks testing) |
| 5 | PINs stored in plaintext | `Bank.cpp:65` | High |
| 6 | CSV corrupts any field containing a comma | `Bank.cpp:62-76` | Medium |
| 7 | Unvalidated `std::cin >>` — bad input causes an infinite loop | `main.cpp` | Medium |
| 8 | No tests, no build system, no CI | — | High (for CV purposes) |

#### Defect 1 in detail

```cpp
Transaction(std::string type, double amount, std::string details, double reusltingBalance)
    : type{type}, amount{amount}, details{details}, resultingBalance{resultingBalance} {}
```

The parameter is misspelled `reusltingBalance`. Therefore `resultingBalance` inside the
braces does not resolve to the parameter — it resolves to the member currently being
initialised. Every `Transaction` initialises its balance from its own uninitialised
memory, and the argument is silently discarded.

Confirmed two ways.

Compiler, at `/W4`:
```
Transaction.h(13): warning C4100: 'reusltingBalance': unreferenced parameter
```

Runtime, after depositing £50 into a £100 account:
```
Type: Deposit, Amount: 50, Details: Money deposited, Resulting Balance: 2.87198e-312
```

The corrupt value is then persisted to disk:
```
12345,Chidubem,1111,150
Deposit,50,Money deposited,2.87198e-312
```

The account balance is correct; the per-transaction ledger is garbage.

---

## Approach

Three options were considered.

**A. Correctness and craft** — *selected.* Fix the defects, add tests, CMake, and CI,
and rewrite the README around design reasoning. Adds one feature (account lockout).

**B. Feature-complete bank** — *rejected.* Build out the TODO list (change PIN, delete
account, account types, interest, admin panel). Each item repeats CRUD already
demonstrated. Adds volume, not signal, and squeezes out testing.

**C. Layered rewrite** — *rejected.* Repository interfaces, dependency injection,
abstract base classes. Not achievable *and* tested in one day; a half-finished refactor
is worse than none. Also over-engineered for a program this size, and hard to justify
under questioning.

---

## Target architecture

```
banking-system/
├── CMakeLists.txt
├── .github/workflows/ci.yml
├── src/
│   ├── Money.h / .cpp          Currency as int64 pence; parsing and formatting
│   ├── Transaction.h / .cpp    Value object (defect 1 fixed)
│   ├── Account.h / .cpp        Balance, PIN hash, lockout state, history
│   ├── Bank.h / .cpp           Domain logic only — no printing, no file access
│   ├── Storage.h / .cpp        Serialisation and parsing
│   ├── Sha256.h / .cpp         Digest used for PIN hashing
│   └── cli/
│       ├── Cli.h / .cpp        Menus, input validation, all console output
│       └── main.cpp            Entry point
├── tests/
│   ├── test_money.cpp
│   ├── test_account.cpp
│   ├── test_bank.cpp
│   ├── test_storage.cpp
│   └── test_sha256.cpp
└── README.md
```

Three responsibilities in three places: `Bank` decides what is allowed, `Storage`
decides how state is written to disk, `Cli` decides what the user sees. Each is
testable without the other two.

---

## Design decisions

### 1. Money as `int64_t` pence

A `Money` value type wrapping a count of pence.

**Why:** binary floating point cannot represent 0.1 exactly, so `double` balances
drift as transactions accumulate. A ledger that does not balance defeats the purpose
of the program. Integers are exact.

**Why a class rather than a bare `int64_t`:** it prevents accidentally mixing a pence
count with an unrelated integer, and centralises parsing and formatting.

**Terminology:** the general term is the currency's *minor unit* (ISO 4217). For GBP
that is pence; for USD, cents; JPY has no minor unit; KWD uses 1/1000. The
implementation is GBP-specific and named accordingly.

**Range:** `int64_t` pence covers roughly ±£92 quadrillion. Addition is nevertheless
overflow-checked: `Money::tryAdd` returns `std::optional<Money>`, empty on overflow,
and callers treat that as a failed operation rather than trapping. Overflow is
unreachable with realistic balances, but an unchecked signed overflow is undefined
behaviour, and the project has already been bitten once by UB.

**Rejected:** a currency-agnostic `Money` with configurable symbol and scale. More
flexible, but speculative — there is one currency.

### 2. Business outcomes as return values, not exceptions

```cpp
enum class TransferResult { Ok, SenderNotFound, ReceiverNotFound,
                            SameAccount, InvalidAmount, InsufficientFunds };
```

**Why:** exceptions are for the unexpected. Insufficient funds is an ordinary,
anticipated outcome. Reserving exceptions for genuinely broken invariants keeps
control flow visible and makes failure cases trivially testable.

The existing `try/catch` parser (defect 3) is the counter-example being removed.

**Boundaries:** malformed external input is reported with `std::optional` or an
explicit parse-error type, not exceptions.

### 3. Domain logic must not print

`Bank::transfer` currently writes success and failure messages to `std::cout`. This is
the root cause of the project having no tests: verifying a transfer would require
capturing stdout and matching English prose.

Once `transfer` returns a `TransferResult`, a test is a single assertion and the CLI
owns all presentation. **This change is a prerequisite for the entire test suite.**

### 4. Explicit record types in the save format

Each line is prefixed with its record type:

```
ACC,12345,Chidubem,<pin-hash>,<salt>,15000,<failed-attempts>
TXN,Deposit,5000,Money deposited,15000
```

**Why:** the parser knows what it is reading instead of inferring it from whether
`std::stoi` threw.

**Escaping (defect 6):** on write, `\` becomes `\\` and `,` becomes `\,`; on read, a
backslash escapes the next character. Backslash is escaped first so the transformation
is reversible. This is why the round-trip test uses a name containing a comma.

**Rejected:** JSON. It would mean a dependency or a hand-written parser, and buys
nothing at this scale.

### 5. PINs: salted SHA-256, with documented limitations

SHA-256 implemented from the published specification (~80 lines), verified in tests
against NIST test vectors. Each account stores a random salt and the digest of
`salt + pin`.

**Why not a library:** no dependency is available without adding weight to the build,
and a published digest with official test vectors is a reasonable self-contained
exercise.

**The limitation, stated plainly in the README:** SHA-256 is the wrong primitive for
password storage because it is *fast*, and speed assists brute-force attacks.
Production systems need a deliberately slow key-derivation function — Argon2, scrypt,
or bcrypt. Documenting this demonstrates the distinction between a hash and a password
hash.

**Rejected:** `std::hash` — not cryptographic, and not stable across runs, so hashes
would not survive a restart.

### 6. No repository interface

`Bank` holds a plain `std::vector<Account>`; `Storage` is plain functions.

**Why:** there is one storage backend and no second one planned. An interface for a
single implementation is speculative generality. It can be introduced when a second
backend actually exists.

### 7. Account lockout

Three consecutive failed PIN attempts lock an account. A successful login before the
third attempt resets the counter. The failure count is persisted.

**A locked account stays locked permanently** — there is no unlock path, because there
is no admin role and adding one is out of scope. This is a deliberate, documented
limitation rather than an oversight: a real system would offer time-based expiry or
administrative reset. The README says so explicitly, and the CLI tells a locked-out
user the account is locked rather than merely rejecting the PIN.

**Why this one feature:** it is the only item on the original TODO list that
demonstrates something the rest of the code does not — security thinking — and it is
naturally testable.

---

## Testing strategy

GoogleTest, obtained via CMake `FetchContent`.

**Why GoogleTest:** the most widely recognised C++ test framework, and `FetchContent`
demonstrates dependency management. **Trade-off:** requires network on first configure
and adds to cold-build time. A vendored single-header framework would build faster
offline but is less recognisable and means committing third-party code.

Coverage is chosen by where bugs live, not by percentage target.

| Unit | Focus |
|------|-------|
| `Money` | Parsing valid and invalid input; exact arithmetic (`0.10 + 0.20 == 0.30`); formatting edge cases (`£0.05`, `£0.00`); overflow |
| `Sha256` | Published NIST test vectors |
| `Account` | Deposit/withdraw rules; **regression test that a transaction records the correct resulting balance** (defect 1); lockout behaviour including counter reset |
| `Bank` | Every `TransferResult` branch; **a failed transfer leaves both balances unchanged** |
| `Storage` | Save/load round-trip; a name containing a comma survives; truncated, malformed, and empty files fail cleanly without crashing |

Roughly 40–50 cases — credible, and small enough to read in full.

**Deliberately not tested:** the interactive CLI loop. Testing `std::cin` prompts
requires I/O plumbing that costs more than it returns here. The CLI is kept as a thin
shell over tested logic, and the README says so.

---

## CI

GitHub Actions: build and test on `ubuntu-latest` (GCC) and `windows-latest` (MSVC),
on every push and pull request. Warnings enabled (`-Wall -Wextra` / `/W4`).

**Why two platforms:** the code has only ever been compiled on one machine with one
compiler. A second compiler catches what MSVC permits, and turns portability from a
claim into a fact.

`/W4` is what surfaced defect 1, which is itself the argument for raising warning
levels.

A CI badge goes at the top of the README.

---

## Documentation

The README is rewritten. The current version reads as a *plan* — "Phase 1, Phase 2,
Stretch Features" — which signals an unfinished project.

New structure:

1. One-line description, CI badge, terminal screenshot of a real session
2. Build and run — commands that work on any platform
3. Architecture — the modules and why each exists
4. **Design decisions and trade-offs** — the section that carries the weight
5. **Known limitations** — what is wrong and what would fix it
6. Testing — what is covered, what is not, and why

`TODO.md` is deleted. An open TODO list advertises unfinished work; anything still
worth noting moves into "Known limitations" as a deliberate boundary.

---

## Commit strategy

Small, focused commits with descriptive messages, one per logical change. The history
is public and reviewers read it; a clean sequence reads as someone who works in a team,
and gives Chidubem a narrative to walk an interviewer through.

---

## Scope boundaries

**In scope:** defects 1–8; account lockout; tests; CMake; CI; README rewrite.

**Explicitly out of scope:** remaining TODO features (change PIN, delete account,
account types, interest, admin panel); currency-agnostic money; JSON or database
persistence; any storage abstraction; CLI tests; concurrency.

**Cut order if time runs short:** PIN hashing → account lockout → `Storage` hostile-input
tests. The non-negotiable core is defect 1, integer pence, the domain/CLI split, tests,
CI, and the README.

---

## Success criteria

1. `Transaction` records correct balances, with a regression test proving it
2. Money arithmetic is exact, with a test that would fail under `double`
3. `Bank` and `Storage` contain no console output
4. All tests pass locally and in CI on both platforms
5. A stranger can clone and build the project from the README alone
6. Every decision above is written down in terms Chidubem can defend unprompted
