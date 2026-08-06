# CV-Worthy Banking System Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rework a working-but-rough C++ CLI banking system into a portfolio project with correct money handling, a tested domain layer, CI on two platforms, and documented design reasoning.

**Architecture:** Three responsibilities in three places. `Bank` decides what is allowed and returns result enums; `Storage` reads and writes the save file; `Cli` owns every line of console output and all input validation. Money is an `int64_t` count of pence behind a `Money` value type. No domain code calls `std::cout`.

**Tech Stack:** C++17, CMake 3.20+, GoogleTest 1.17.0 via `FetchContent`, GitHub Actions.

**Spec:** `docs/superpowers/specs/2026-08-06-cv-worthy-banking-system-design.md`

## Global Constraints

- **C++ standard:** C++17. `CMAKE_CXX_STANDARD 17`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CMAKE_CXX_EXTENSIONS OFF`.
- **CMake floor:** `cmake_minimum_required(VERSION 3.20)`. Local toolchain is CMake 4.4.2, which rejects any project declaring a floor below 3.5.
- **Local build note:** MSVC Build Tools 2022 is installed but `vcvars64.bat` is not on the default PATH, and CMake was installed this session so it is not on PATH until a shell restart. Use full paths when needed: `C:\Program Files\CMake\bin\cmake.exe`.
- **Executable path differs by generator.** MSVC is multi-config, so the binary lands at `build/Debug/bank.exe`; single-config generators (Make, Ninja) put it at `build/bank`. Commands below assume the MSVC layout because that is the local toolchain — adjust on Linux. `ctest --test-dir build` works unchanged on both.
- **Warnings:** `/W4` on MSVC, `-Wall -Wextra -Wpedantic` on GCC/Clang. Warnings are not errors locally, but CI treats the build as failed if it does not compile.
- **Currency:** GBP only. Display symbol `£`. Storage is always an integer count of pence.
- **No domain output:** nothing under `src/` except `src/cli/` may include `<iostream>` or write to `std::cout` / `std::cerr`.
- **No new third-party dependencies** beyond GoogleTest, which is test-only.
- **Encoding:** source files are UTF-8. The `£` symbol appears only in `src/Money.cpp` and `src/cli/`.
- **Commits:** one per task minimum, conventional-style messages, no `--no-verify`.

---

## File Structure

| Path | Responsibility |
|---|---|
| `CMakeLists.txt` | Top-level project, `banking_core` library, `bank` executable, warnings, `enable_testing()` |
| `tests/CMakeLists.txt` | GoogleTest `FetchContent`, one test executable, `gtest_discover_tests` |
| `src/Money.h/.cpp` | GBP as `int64_t` pence; parsing, formatting, checked arithmetic |
| `src/Sha256.h/.cpp` | `sha256Hex` digest only |
| `src/PinCredential.h/.cpp` | Salt generation, PIN hashing, verification |
| `src/Transaction.h/.cpp` | Immutable ledger entry value object |
| `src/Account.h/.cpp` | Balance, credential, lockout counter, history |
| `src/Bank.h/.cpp` | Account collection, login, transfer. Result enums, no I/O |
| `src/Storage.h/.cpp` | Save-file format, escaping, parsing |
| `src/cli/Cli.h/.cpp` | Menus, validated input, all console output |
| `src/cli/main.cpp` | Entry point only |
| `tests/test_*.cpp` | One file per unit above |
| `.github/workflows/ci.yml` | Build + test on ubuntu-latest and windows-latest |

**Deviation from spec:** the spec's file list folded PIN hashing into `Sha256`. Splitting `PinCredential` out keeps `Sha256` a pure digest with its own NIST-vector tests, and keeps salting policy in one place. This is a refinement, not a change of approach.

## Defect register

Nine defects. The spec listed eight; #9 was found while writing this plan.

| # | Defect | Fixed in |
|---|---|---|
| 1 | Uninitialised member read (UB) in `Transaction` | Task 2 |
| 2 | `double` used for currency | Tasks 3, 6 |
| 3 | Exceptions as control flow in the parser | Task 8 |
| 4 | Domain logic writes to `std::cout` | Task 7 |
| 5 | Plaintext PINs | Tasks 4, 5, 6 |
| 6 | CSV corrupts fields containing commas | Task 8 |
| 7 | Unvalidated `std::cin >>` causes infinite loop on bad input | Task 9 |
| 8 | No tests, no build system, no CI | Tasks 1, 10 |
| 9 | `std::cin >> name` truncates at whitespace — "John Smith" stores name `John` and PIN `Smith` | Task 9 |

## Task ordering rationale

Task 2 fixes the UB bug while `Transaction` still uses `double`, and Task 6 then converts it to `Money`. That is deliberate: a critical correctness bug gets its own isolated commit rather than being buried inside a refactor, and the regression test written in Task 2 survives the conversion. Expect a reviewer to ask; that is the answer.

---

## Task 1: Build system and test harness

**Files:**
- Create: `CMakeLists.txt`
- Create: `tests/CMakeLists.txt`
- Create: `tests/test_smoke.cpp`
- Create: `.gitignore` entry for `build/`
- Move: `Account.{h,cpp}`, `Bank.{h,cpp}`, `Transaction.{h,cpp}` → `src/`
- Move: `main.cpp` → `src/cli/main.cpp`
- Delete: `.vscode/tasks.json` (superseded by CMake)

**Interfaces:**
- Consumes: nothing
- Produces: CMake targets `banking_core` (static library of everything under `src/` except `src/cli/main.cpp`) and `bank` (executable); test target `banking_tests`. Every later task adds sources to `banking_core` and test files to `banking_tests`.

- [ ] **Step 1: Move sources with git mv so history is preserved**

```bash
mkdir -p src/cli tests
git mv Account.h Account.cpp Bank.h Bank.cpp Transaction.h Transaction.cpp src/
git mv main.cpp src/cli/main.cpp
git rm -r --cached .vscode 2>/dev/null; rm -rf .vscode
```

- [ ] **Step 2: Write the top-level CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.20)
project(banking_system LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(banking_core
    src/Account.cpp
    src/Bank.cpp
    src/Transaction.cpp
)
target_include_directories(banking_core PUBLIC src)

if(MSVC)
    target_compile_options(banking_core PRIVATE /W4 /utf-8)
else()
    target_compile_options(banking_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(bank src/cli/main.cpp)
target_link_libraries(bank PRIVATE banking_core)
if(MSVC)
    target_compile_options(bank PRIVATE /W4 /utf-8)
else()
    target_compile_options(bank PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
```

`/utf-8` is required on MSVC so the `£` literal added in Task 3 compiles correctly.

- [ ] **Step 3: Write tests/CMakeLists.txt**

```cmake
include(FetchContent)

FetchContent_Declare(googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(banking_tests
    test_smoke.cpp
)
target_link_libraries(banking_tests PRIVATE banking_core GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(banking_tests)
```

`gtest_force_shared_crt` prevents the MSVC runtime-library mismatch that otherwise fails the link on Windows.

- [ ] **Step 4: Write the smoke test**

```cpp
#include <gtest/gtest.h>

TEST(Smoke, TestHarnessRuns) {
    EXPECT_EQ(2 + 2, 4);
}
```

- [ ] **Step 5: Add build/ to .gitignore**

Append to `.gitignore`:

```
# CMake
build/
```

- [ ] **Step 6: Configure, build, and run the tests**

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: configure downloads GoogleTest, build succeeds, `1 test from 1 test suite ran. [ PASSED ] 1 test.`

If MSVC cannot find headers, the environment needs `vcvars64.bat`; see Global Constraints.

- [ ] **Step 7: Verify the app still runs**

```bash
./build/Debug/bank.exe < /dev/null
```

Expected: the main menu prints. Behaviour is unchanged from before the move.

- [ ] **Step 8: Commit**

```bash
git add -A
git commit -m "build: add CMake build and GoogleTest harness

Move sources into src/ and add a CMake build with warnings enabled on
both toolchains. Replaces the VS Code cl.exe task, which only worked on
one machine."
```

---

## Task 2: Fix the uninitialised read in Transaction

**Files:**
- Modify: `src/Transaction.h:13-14`
- Create: `tests/test_transaction.cpp`
- Modify: `tests/CMakeLists.txt` (add the new test file)

**Interfaces:**
- Consumes: `banking_tests` target from Task 1
- Produces: no interface change. `Transaction`'s constructor keeps its current signature (`std::string type, double amount, std::string details, double resultingBalance`) until Task 6.

- [ ] **Step 1: Add the test file to the build**

In `tests/CMakeLists.txt`, change the `add_executable` source list to:

```cmake
add_executable(banking_tests
    test_smoke.cpp
    test_transaction.cpp
)
```

- [ ] **Step 2: Write the failing regression test**

`tests/test_transaction.cpp`:

```cpp
#include <gtest/gtest.h>
#include "Transaction.h"

TEST(Transaction, StoresTheResultingBalanceItWasGiven) {
    Transaction t("Deposit", 50.0, "Money deposited", 150.0);
    EXPECT_DOUBLE_EQ(150.0, t.getResultingBalance());
}

TEST(Transaction, StoresAllConstructorArguments) {
    Transaction t("Withdrawal", 25.5, "Cash machine", 74.5);
    EXPECT_EQ("Withdrawal", t.getType());
    EXPECT_DOUBLE_EQ(25.5, t.getAmount());
    EXPECT_EQ("Cash machine", t.getDetails());
    EXPECT_DOUBLE_EQ(74.5, t.getResultingBalance());
}
```

- [ ] **Step 3: Run the test and confirm it fails**

```bash
cmake --build build && ctest --test-dir build -R Transaction --output-on-failure
```

Expected: FAIL. `getResultingBalance()` returns an indeterminate value such as `2.87198e-312`, not `150`.

This failure is the proof the bug is real. Do not skip it.

- [ ] **Step 4: Fix the constructor**

In `src/Transaction.h`, replace the constructor:

```cpp
        Transaction(std::string type, double amount, std::string details, double resultingBalance)
            : type{ type }, amount{ amount }, details { details }, resultingBalance { resultingBalance } {}
```

The only change is spelling the fourth parameter `resultingBalance` instead of `reusltingBalance`. The member initialiser now resolves to the parameter rather than to the member itself.

- [ ] **Step 5: Run the test and confirm it passes**

```bash
cmake --build build && ctest --test-dir build -R Transaction --output-on-failure
```

Expected: PASS, 2 tests.

- [ ] **Step 6: Confirm the compiler warning is gone**

```bash
cmake --build build --clean-first 2>&1 | grep -i "C4100\|unreferenced"
```

Expected: no output. Warning C4100 on `Transaction.h` is resolved.

- [ ] **Step 7: Commit**

```bash
git add src/Transaction.h tests/test_transaction.cpp tests/CMakeLists.txt
git commit -m "fix: correct uninitialised read of resultingBalance

The constructor parameter was misspelled 'reusltingBalance', so the
member initialiser resolved to the member being initialised rather than
to the parameter. Every Transaction read its own uninitialised memory
and discarded the value passed in; the corrupt figure was then written
to the save file.

Caught by /W4 as warning C4100. Adds a regression test."
```

---

## Task 3: Money value type

**Files:**
- Create: `src/Money.h`, `src/Money.cpp`
- Create: `tests/test_money.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing
- Produces:

```cpp
class Money {
public:
    Money() = default;                                          // £0.00
    static Money fromPence(std::int64_t pence);
    static std::optional<Money> fromString(const std::string& text);
    std::int64_t pence() const;
    std::string toString() const;                               // "£12.34", "-£12.34"
    std::optional<Money> tryAdd(Money other) const;
    std::optional<Money> trySubtract(Money other) const;
    bool isPositive() const;                                    // pence > 0
};
bool operator==(Money, Money);  bool operator!=(Money, Money);
bool operator< (Money, Money);  bool operator<=(Money, Money);
bool operator> (Money, Money);  bool operator>=(Money, Money);
```

- [ ] **Step 1: Add Money to both CMake source lists**

`CMakeLists.txt` — add `src/Money.cpp` to `banking_core`.
`tests/CMakeLists.txt` — add `test_money.cpp` to `banking_tests`.

- [ ] **Step 2: Write the failing tests**

`tests/test_money.cpp`:

```cpp
#include <gtest/gtest.h>
#include "Money.h"

TEST(Money, DefaultsToZero) {
    EXPECT_EQ(0, Money{}.pence());
}

TEST(Money, ParsesWholePounds) {
    auto m = Money::fromString("12");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(1200, m->pence());
}

TEST(Money, ParsesTwoDecimalPlaces) {
    auto m = Money::fromString("12.34");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(1234, m->pence());
}

TEST(Money, ParsesOneDecimalPlaceAsTens) {
    auto m = Money::fromString("12.3");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(1230, m->pence());
}

TEST(Money, ParsesSubPoundAmounts) {
    auto m = Money::fromString("0.05");
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(5, m->pence());
}

TEST(Money, RejectsMalformedInput) {
    EXPECT_FALSE(Money::fromString("").has_value());
    EXPECT_FALSE(Money::fromString("abc").has_value());
    EXPECT_FALSE(Money::fromString("12.345").has_value());   // too many decimals
    EXPECT_FALSE(Money::fromString("12.").has_value());      // trailing point
    EXPECT_FALSE(Money::fromString(".5").has_value());       // no leading digit
    EXPECT_FALSE(Money::fromString("-5").has_value());       // negative input
    EXPECT_FALSE(Money::fromString("1 2").has_value());
    EXPECT_FALSE(Money::fromString("12.3.4").has_value());
    EXPECT_FALSE(Money::fromString("99999999999999999999").has_value());  // overflow
}

// This is the test that would fail if money were stored as double.
TEST(Money, ArithmeticIsExact) {
    auto tenPence = Money::fromString("0.10");
    auto twentyPence = Money::fromString("0.20");
    auto thirtyPence = Money::fromString("0.30");
    ASSERT_TRUE(tenPence && twentyPence && thirtyPence);

    auto sum = tenPence->tryAdd(*twentyPence);
    ASSERT_TRUE(sum.has_value());
    EXPECT_EQ(*thirtyPence, *sum);
    EXPECT_EQ(30, sum->pence());
}

TEST(Money, AccumulationDoesNotDrift) {
    Money total;
    auto tenPence = Money::fromPence(10);
    for (int i = 0; i < 10000; ++i) {
        auto next = total.tryAdd(tenPence);
        ASSERT_TRUE(next.has_value());
        total = *next;
    }
    EXPECT_EQ(100000, total.pence());   // exactly £1000.00
}

TEST(Money, FormatsWithTwoDecimalPlaces) {
    EXPECT_EQ("£12.34", Money::fromPence(1234).toString());
    EXPECT_EQ("£0.05",  Money::fromPence(5).toString());
    EXPECT_EQ("£0.00",  Money::fromPence(0).toString());
    EXPECT_EQ("£1500.00", Money::fromPence(150000).toString());
    EXPECT_EQ("-£12.34", Money::fromPence(-1234).toString());
}

TEST(Money, DetectsAdditionOverflow) {
    auto big = Money::fromPence(std::numeric_limits<std::int64_t>::max());
    EXPECT_FALSE(big.tryAdd(Money::fromPence(1)).has_value());
}

TEST(Money, DetectsSubtractionOverflow) {
    auto small = Money::fromPence(std::numeric_limits<std::int64_t>::min());
    EXPECT_FALSE(small.trySubtract(Money::fromPence(1)).has_value());
}

TEST(Money, SubtractionCanGoNegative) {
    auto result = Money::fromPence(100).trySubtract(Money::fromPence(250));
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(-150, result->pence());
}

TEST(Money, Compares) {
    EXPECT_TRUE(Money::fromPence(100) < Money::fromPence(200));
    EXPECT_TRUE(Money::fromPence(200) > Money::fromPence(100));
    EXPECT_TRUE(Money::fromPence(100) == Money::fromPence(100));
    EXPECT_TRUE(Money::fromPence(100) <= Money::fromPence(100));
    EXPECT_TRUE(Money::fromPence(100) != Money::fromPence(101));
}

TEST(Money, IsPositiveOnlyForAmountsAboveZero) {
    EXPECT_TRUE(Money::fromPence(1).isPositive());
    EXPECT_FALSE(Money::fromPence(0).isPositive());
    EXPECT_FALSE(Money::fromPence(-1).isPositive());
}
```

Add `#include <limits>` at the top of the test file.

- [ ] **Step 3: Run and confirm compilation fails**

```bash
cmake --build build
```

Expected: FAIL — `Money.h` does not exist.

- [ ] **Step 4: Write Money.h**

```cpp
#ifndef MONEY_H
#define MONEY_H

#include <cstdint>
#include <optional>
#include <string>

// A GBP amount held as an exact integer count of pence.
//
// Currency is never stored in floating point: binary floating point cannot
// represent 0.1 exactly, so repeated addition drifts. A ledger that does not
// balance defeats the purpose of the program.
class Money {
public:
    Money() = default;

    static Money fromPence(std::int64_t pence);

    // Parses "12", "12.3" or "12.34". Returns nullopt for anything else,
    // including negative input, more than two decimal places, and values
    // that would overflow int64.
    static std::optional<Money> fromString(const std::string& text);

    std::int64_t pence() const { return pence_; }

    // "£12.34", "£0.05", "-£12.34"
    std::string toString() const;

    std::optional<Money> tryAdd(Money other) const;
    std::optional<Money> trySubtract(Money other) const;

    bool isPositive() const { return pence_ > 0; }

private:
    explicit Money(std::int64_t pence) : pence_{pence} {}
    std::int64_t pence_{0};
};

bool operator==(Money lhs, Money rhs);
bool operator!=(Money lhs, Money rhs);
bool operator<(Money lhs, Money rhs);
bool operator<=(Money lhs, Money rhs);
bool operator>(Money lhs, Money rhs);
bool operator>=(Money lhs, Money rhs);

#endif
```

- [ ] **Step 5: Write Money.cpp**

```cpp
#include "Money.h"

#include <cstdlib>
#include <limits>

namespace {

bool isDigits(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

// Accumulate decimal digits into pence, rejecting overflow.
std::optional<std::int64_t> digitsToInt(const std::string& digits) {
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    std::int64_t value = 0;
    for (char c : digits) {
        const std::int64_t digit = c - '0';
        if (value > (kMax - digit) / 10) return std::nullopt;
        value = value * 10 + digit;
    }
    return value;
}

}  // namespace

Money Money::fromPence(std::int64_t pence) {
    return Money{pence};
}

std::optional<Money> Money::fromString(const std::string& text) {
    if (text.empty()) return std::nullopt;

    const std::size_t dot = text.find('.');
    std::string whole = (dot == std::string::npos) ? text : text.substr(0, dot);
    std::string frac  = (dot == std::string::npos) ? ""   : text.substr(dot + 1);

    if (!isDigits(whole)) return std::nullopt;
    if (dot != std::string::npos) {
        if (frac.empty() || frac.size() > 2) return std::nullopt;
        if (!isDigits(frac)) return std::nullopt;
    }
    if (frac.size() == 1) frac += '0';   // "12.3" means 30 pence, not 3

    const auto pounds = digitsToInt(whole);
    if (!pounds) return std::nullopt;

    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    if (*pounds > kMax / 100) return std::nullopt;
    std::int64_t total = *pounds * 100;

    if (!frac.empty()) {
        const auto pence = digitsToInt(frac);
        if (!pence) return std::nullopt;
        if (total > kMax - *pence) return std::nullopt;
        total += *pence;
    }
    return Money{total};
}

std::string Money::toString() const {
    const bool negative = pence_ < 0;
    // Negate via unsigned to avoid UB on int64 min.
    const std::uint64_t magnitude =
        negative ? (~static_cast<std::uint64_t>(pence_) + 1u)
                 : static_cast<std::uint64_t>(pence_);

    const std::uint64_t pounds = magnitude / 100;
    const std::uint64_t remainder = magnitude % 100;

    std::string out;
    if (negative) out += '-';
    out += "\u00A3";                       // £
    out += std::to_string(pounds);
    out += '.';
    if (remainder < 10) out += '0';
    out += std::to_string(remainder);
    return out;
}

std::optional<Money> Money::tryAdd(Money other) const {
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
    if (other.pence_ > 0 && pence_ > kMax - other.pence_) return std::nullopt;
    if (other.pence_ < 0 && pence_ < kMin - other.pence_) return std::nullopt;
    return Money{pence_ + other.pence_};
}

std::optional<Money> Money::trySubtract(Money other) const {
    constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
    constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();
    if (other.pence_ < 0 && pence_ > kMax + other.pence_) return std::nullopt;
    if (other.pence_ > 0 && pence_ < kMin + other.pence_) return std::nullopt;
    return Money{pence_ - other.pence_};
}

bool operator==(Money lhs, Money rhs) { return lhs.pence() == rhs.pence(); }
bool operator!=(Money lhs, Money rhs) { return !(lhs == rhs); }
bool operator<(Money lhs, Money rhs)  { return lhs.pence() < rhs.pence(); }
bool operator<=(Money lhs, Money rhs) { return !(rhs < lhs); }
bool operator>(Money lhs, Money rhs)  { return rhs < lhs; }
bool operator>=(Money lhs, Money rhs) { return !(lhs < rhs); }
```

`\u00A3` is used rather than a literal `£` so the file is not sensitive to source encoding.

- [ ] **Step 6: Run the tests**

```bash
cmake --build build && ctest --test-dir build -R Money --output-on-failure
```

Expected: PASS, 14 tests.

- [ ] **Step 7: Commit**

```bash
git add src/Money.h src/Money.cpp tests/test_money.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add Money value type backed by integer pence

Currency is an exact int64 count of pence with checked arithmetic.
Includes a test that accumulates 10,000 additions and asserts no drift,
which would fail if the balance were a double."
```

---

## Task 4: SHA-256 digest

**Files:**
- Create: `src/Sha256.h`, `src/Sha256.cpp`
- Create: `tests/test_sha256.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing
- Produces: `std::string crypto::sha256Hex(const std::string& input);` returning 64 lowercase hex characters.

- [ ] **Step 1: Add to both CMake source lists**

Add `src/Sha256.cpp` to `banking_core` and `test_sha256.cpp` to `banking_tests`.

- [ ] **Step 2: Write the failing tests using published FIPS 180-2 vectors**

`tests/test_sha256.cpp`:

```cpp
#include <gtest/gtest.h>
#include <string>
#include "Sha256.h"

// Vectors published in FIPS 180-2 Appendix B. Testing against the official
// specification is what makes a hand-written digest trustworthy.
TEST(Sha256, EmptyString) {
    EXPECT_EQ("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
              crypto::sha256Hex(""));
}

TEST(Sha256, Abc) {
    EXPECT_EQ("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
              crypto::sha256Hex("abc"));
}

TEST(Sha256, TwoBlockMessage) {
    EXPECT_EQ("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
              crypto::sha256Hex(
                  "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"));
}

TEST(Sha256, OneMillionAs) {
    EXPECT_EQ("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
              crypto::sha256Hex(std::string(1000000, 'a')));
}

TEST(Sha256, OutputIsAlwaysSixtyFourLowercaseHexChars) {
    const std::string digest = crypto::sha256Hex("anything");
    EXPECT_EQ(64u, digest.size());
    for (char c : digest) {
        EXPECT_TRUE((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) << "char: " << c;
    }
}

TEST(Sha256, DifferentInputsGiveDifferentDigests) {
    EXPECT_NE(crypto::sha256Hex("1234"), crypto::sha256Hex("1235"));
}
```

- [ ] **Step 3: Run and confirm compilation fails**

```bash
cmake --build build
```

Expected: FAIL — `Sha256.h` does not exist.

- [ ] **Step 4: Write Sha256.h**

```cpp
#ifndef SHA256_H
#define SHA256_H

#include <string>

namespace crypto {

// SHA-256 as specified in FIPS 180-4. Returns 64 lowercase hex characters.
//
// This is a general-purpose digest, NOT a password-hashing function. See
// PinCredential.h for why that distinction matters here.
std::string sha256Hex(const std::string& input);

}  // namespace crypto

#endif
```

- [ ] **Step 5: Write Sha256.cpp**

```cpp
#include "Sha256.h"

#include <array>
#include <cstdint>
#include <vector>

namespace crypto {
namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

std::uint32_t rotr(std::uint32_t x, unsigned n) {
    return (x >> n) | (x << (32 - n));
}

void compress(std::array<std::uint32_t, 8>& state, const unsigned char* block) {
    std::array<std::uint32_t, 64> w{};
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
               (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
               (static_cast<std::uint32_t>(block[i * 4 + 3]));
    }
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    std::uint32_t e = state[4], f = state[5], g = state[6], h = state[7];

    for (int i = 0; i < 64; ++i) {
        const std::uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t temp1 = h + S1 + ch + kRoundConstants[i] + w[i];
        const std::uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = S0 + maj;

        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

}  // namespace

std::string sha256Hex(const std::string& input) {
    std::array<std::uint32_t, 8> state = {0x6a09e667, 0xbb67ae85, 0x3c6ef372,
                                          0xa54ff53a, 0x510e527f, 0x9b05688c,
                                          0x1f83d9ab, 0x5be0cd19};

    std::vector<unsigned char> message(input.begin(), input.end());
    const std::uint64_t bitLength = static_cast<std::uint64_t>(input.size()) * 8;

    message.push_back(0x80);
    while (message.size() % 64 != 56) message.push_back(0x00);
    for (int i = 7; i >= 0; --i) {
        message.push_back(static_cast<unsigned char>((bitLength >> (i * 8)) & 0xFF));
    }

    for (std::size_t i = 0; i < message.size(); i += 64) {
        compress(state, message.data() + i);
    }

    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (std::uint32_t word : state) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            out += kHex[(word >> shift) & 0xF];
        }
    }
    return out;
}

}  // namespace crypto
```

- [ ] **Step 6: Run the tests**

```bash
cmake --build build && ctest --test-dir build -R Sha256 --output-on-failure
```

Expected: PASS, 6 tests. If any NIST vector mismatches, the implementation is wrong — do not adjust the expected values.

- [ ] **Step 7: Commit**

```bash
git add src/Sha256.h src/Sha256.cpp tests/test_sha256.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: add SHA-256 verified against FIPS 180-2 vectors

Implements the digest from the published specification and tests it
against the four official test vectors, including the one-million-byte
case that exercises multi-block processing."
```

---

## Task 5: PIN credentials

**Files:**
- Create: `src/PinCredential.h`, `src/PinCredential.cpp`
- Create: `tests/test_pin_credential.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `crypto::sha256Hex` from Task 4
- Produces:

```cpp
struct PinCredential {
    std::string salt;   // 32 lowercase hex chars
    std::string hash;   // 64 lowercase hex chars
};
PinCredential makePinCredential(const std::string& pin);
PinCredential restorePinCredential(std::string salt, std::string hash);
bool verifyPin(const PinCredential& credential, const std::string& pin);
```

- [ ] **Step 1: Add to both CMake source lists**

Add `src/PinCredential.cpp` to `banking_core` and `test_pin_credential.cpp` to `banking_tests`.

- [ ] **Step 2: Write the failing tests**

`tests/test_pin_credential.cpp`:

```cpp
#include <gtest/gtest.h>
#include "PinCredential.h"
#include "Sha256.h"

TEST(PinCredential, VerifiesTheCorrectPin) {
    const auto credential = makePinCredential("1234");
    EXPECT_TRUE(verifyPin(credential, "1234"));
}

TEST(PinCredential, RejectsTheWrongPin) {
    const auto credential = makePinCredential("1234");
    EXPECT_FALSE(verifyPin(credential, "1235"));
    EXPECT_FALSE(verifyPin(credential, ""));
    EXPECT_FALSE(verifyPin(credential, "12345"));
}

TEST(PinCredential, NeverStoresThePin) {
    const auto credential = makePinCredential("1234");
    EXPECT_EQ(std::string::npos, credential.hash.find("1234"));
    EXPECT_EQ(std::string::npos, credential.salt.find("1234"));
}

// The point of a salt: two accounts with the same PIN must not end up with
// the same hash, so one precomputed table cannot break both.
TEST(PinCredential, IdenticalPinsGetDifferentSaltsAndDifferentHashes) {
    const auto a = makePinCredential("1234");
    const auto b = makePinCredential("1234");
    EXPECT_NE(a.salt, b.salt);
    EXPECT_NE(a.hash, b.hash);
}

// Holding the salt fixed isolates the PIN as the only variable.
TEST(PinCredential, SameSaltWithDifferentPinsGivesDifferentHashes) {
    const auto reference = makePinCredential("1234");
    const auto sameSaltOtherPin =
        restorePinCredential(reference.salt, crypto::sha256Hex(reference.salt + "9999"));
    EXPECT_NE(reference.hash, sameSaltOtherPin.hash);
    EXPECT_TRUE(verifyPin(sameSaltOtherPin, "9999"));
    EXPECT_FALSE(verifyPin(sameSaltOtherPin, "1234"));
}

TEST(PinCredential, SaltAndHashHaveExpectedShape) {
    const auto credential = makePinCredential("1234");
    EXPECT_EQ(32u, credential.salt.size());
    EXPECT_EQ(64u, credential.hash.size());
}

TEST(PinCredential, RestoredCredentialVerifiesTheSamePin) {
    const auto original = makePinCredential("4321");
    const auto restored = restorePinCredential(original.salt, original.hash);
    EXPECT_TRUE(verifyPin(restored, "4321"));
    EXPECT_FALSE(verifyPin(restored, "1234"));
}
```

- [ ] **Step 3: Run and confirm compilation fails**

Expected: FAIL — `PinCredential.h` does not exist.

- [ ] **Step 4: Write PinCredential.h**

```cpp
#ifndef PIN_CREDENTIAL_H
#define PIN_CREDENTIAL_H

#include <string>

// A stored PIN: a random salt plus SHA-256(salt + pin). The PIN itself is
// never retained.
//
// LIMITATION, stated deliberately: SHA-256 is a fast digest, and speed is
// exactly what an attacker wants when guessing a four-digit PIN. Production
// systems must use a deliberately slow key-derivation function such as
// Argon2, scrypt or bcrypt. This implementation demonstrates salting and
// one-way storage; it is not fit for real deployment.
struct PinCredential {
    std::string salt;   // 32 lowercase hex chars (16 random bytes)
    std::string hash;   // 64 lowercase hex chars
};

// Generates a fresh random salt and hashes the PIN with it.
PinCredential makePinCredential(const std::string& pin);

// Rebuilds a credential from values already persisted. Performs no hashing.
PinCredential restorePinCredential(std::string salt, std::string hash);

bool verifyPin(const PinCredential& credential, const std::string& pin);

#endif
```

- [ ] **Step 5: Write PinCredential.cpp**

```cpp
#include "PinCredential.h"

#include <random>

#include "Sha256.h"

namespace {

std::string randomSaltHex() {
    static const char* kHex = "0123456789abcdef";

    std::random_device device;
    std::mt19937_64 engine{device()};
    std::uniform_int_distribution<int> nibble{0, 15};

    std::string salt;
    salt.reserve(32);
    for (int i = 0; i < 32; ++i) {
        salt += kHex[nibble(engine)];
    }
    return salt;
}

}  // namespace

PinCredential makePinCredential(const std::string& pin) {
    PinCredential credential;
    credential.salt = randomSaltHex();
    credential.hash = crypto::sha256Hex(credential.salt + pin);
    return credential;
}

PinCredential restorePinCredential(std::string salt, std::string hash) {
    PinCredential credential;
    credential.salt = std::move(salt);
    credential.hash = std::move(hash);
    return credential;
}

bool verifyPin(const PinCredential& credential, const std::string& pin) {
    return crypto::sha256Hex(credential.salt + pin) == credential.hash;
}
```

- [ ] **Step 6: Run the tests**

```bash
cmake --build build && ctest --test-dir build -R PinCredential --output-on-failure
```

Expected: PASS, 7 tests.

- [ ] **Step 7: Commit**

```bash
git add src/PinCredential.h src/PinCredential.cpp tests/test_pin_credential.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: store PINs as salted SHA-256 rather than plaintext

Each account gets a random 16-byte salt so identical PINs do not produce
identical hashes. The header documents why SHA-256 is the wrong primitive
for production password storage and what should replace it."
```

---

## Task 6: Rebuild Transaction and Account on Money

**Files:**
- Rewrite: `src/Transaction.h`, `src/Transaction.cpp`
- Rewrite: `src/Account.h`, `src/Account.cpp`
- Rewrite: `tests/test_transaction.cpp`
- Create: `tests/test_account.cpp`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Money` (Task 3), `PinCredential` (Task 5)
- Produces:

```cpp
enum class TransactionType { Deposit, Withdrawal, TransferIn, TransferOut };
std::string toStorageString(TransactionType);
std::optional<TransactionType> transactionTypeFromStorageString(const std::string&);
std::string toDisplayString(TransactionType);

class Transaction {
public:
    Transaction(TransactionType type, Money amount, std::string details, Money resultingBalance);
    TransactionType type() const;
    Money amount() const;
    const std::string& details() const;
    Money resultingBalance() const;
};

class Account {
public:
    static constexpr int kMaxFailedAttempts = 3;
    enum class AuthResult { Ok, WrongPin, Locked };
    enum class DepositResult { Ok, InvalidAmount, Overflow };
    enum class WithdrawResult { Ok, InvalidAmount, InsufficientFunds };

    Account(int accountNumber, std::string name, const std::string& pin, Money openingBalance);
    Account(int accountNumber, std::string name, PinCredential credential,
            Money balance, int failedAttempts, std::vector<Transaction> history);

    int accountNumber() const;
    const std::string& name() const;
    Money balance() const;
    const PinCredential& credential() const;
    int failedAttempts() const;
    bool isLocked() const;

    AuthResult authenticate(const std::string& pin);
    DepositResult deposit(Money amount, TransactionType type, std::string details);
    WithdrawResult withdraw(Money amount, TransactionType type, std::string details);
    const std::vector<Transaction>& history() const;
};
```

- [ ] **Step 1: Add test_account.cpp to tests/CMakeLists.txt**

- [ ] **Step 2: Rewrite tests/test_transaction.cpp**

```cpp
#include <gtest/gtest.h>
#include "Transaction.h"

TEST(Transaction, StoresTheResultingBalanceItWasGiven) {
    Transaction t(TransactionType::Deposit, Money::fromPence(5000),
                  "Money deposited", Money::fromPence(15000));
    EXPECT_EQ(Money::fromPence(15000), t.resultingBalance());
}

TEST(Transaction, StoresAllConstructorArguments) {
    Transaction t(TransactionType::Withdrawal, Money::fromPence(2550),
                  "Cash machine", Money::fromPence(7450));
    EXPECT_EQ(TransactionType::Withdrawal, t.type());
    EXPECT_EQ(Money::fromPence(2550), t.amount());
    EXPECT_EQ("Cash machine", t.details());
    EXPECT_EQ(Money::fromPence(7450), t.resultingBalance());
}

TEST(Transaction, TypeRoundTripsThroughStorageStrings) {
    const TransactionType all[] = {TransactionType::Deposit, TransactionType::Withdrawal,
                                   TransactionType::TransferIn, TransactionType::TransferOut};
    for (TransactionType type : all) {
        const auto parsed = transactionTypeFromStorageString(toStorageString(type));
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(type, *parsed);
    }
}

TEST(Transaction, RejectsUnknownStorageStrings) {
    EXPECT_FALSE(transactionTypeFromStorageString("Nonsense").has_value());
    EXPECT_FALSE(transactionTypeFromStorageString("").has_value());
}
```

- [ ] **Step 3: Write tests/test_account.cpp**

```cpp
#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "Account.h"

namespace {
Account makeAccount(std::int64_t openingPence = 10000) {
    return Account(12345, "Chidubem", "1234", Money::fromPence(openingPence));
}
}  // namespace

TEST(Account, StartsWithTheOpeningBalanceAndNoHistory) {
    const auto account = makeAccount();
    EXPECT_EQ(12345, account.accountNumber());
    EXPECT_EQ("Chidubem", account.name());
    EXPECT_EQ(Money::fromPence(10000), account.balance());
    EXPECT_TRUE(account.history().empty());
    EXPECT_FALSE(account.isLocked());
}

TEST(Account, DepositIncreasesTheBalance) {
    auto account = makeAccount();
    EXPECT_EQ(Account::DepositResult::Ok,
              account.deposit(Money::fromPence(5000), TransactionType::Deposit, "Pay"));
    EXPECT_EQ(Money::fromPence(15000), account.balance());
}

// Regression test for the original uninitialised-read defect.
TEST(Account, TransactionRecordsTheCorrectResultingBalance) {
    auto account = makeAccount();
    account.deposit(Money::fromPence(5000), TransactionType::Deposit, "Pay");
    ASSERT_EQ(1u, account.history().size());
    EXPECT_EQ(Money::fromPence(15000), account.history().front().resultingBalance());
}

TEST(Account, RejectsNonPositiveDeposits) {
    auto account = makeAccount();
    EXPECT_EQ(Account::DepositResult::InvalidAmount,
              account.deposit(Money::fromPence(0), TransactionType::Deposit, "Nothing"));
    EXPECT_EQ(Account::DepositResult::InvalidAmount,
              account.deposit(Money::fromPence(-100), TransactionType::Deposit, "Negative"));
    EXPECT_EQ(Money::fromPence(10000), account.balance());
    EXPECT_TRUE(account.history().empty());
}

TEST(Account, WithdrawDecreasesTheBalance) {
    auto account = makeAccount();
    EXPECT_EQ(Account::WithdrawResult::Ok,
              account.withdraw(Money::fromPence(2500), TransactionType::Withdrawal, "Cash"));
    EXPECT_EQ(Money::fromPence(7500), account.balance());
}

TEST(Account, RejectsOverdraftAndLeavesStateUntouched) {
    auto account = makeAccount();
    EXPECT_EQ(Account::WithdrawResult::InsufficientFunds,
              account.withdraw(Money::fromPence(10001), TransactionType::Withdrawal, "Too much"));
    EXPECT_EQ(Money::fromPence(10000), account.balance());
    EXPECT_TRUE(account.history().empty());
}

TEST(Account, AllowsWithdrawingTheEntireBalance) {
    auto account = makeAccount();
    EXPECT_EQ(Account::WithdrawResult::Ok,
              account.withdraw(Money::fromPence(10000), TransactionType::Withdrawal, "All"));
    EXPECT_EQ(Money::fromPence(0), account.balance());
}

TEST(Account, RejectsNonPositiveWithdrawals) {
    auto account = makeAccount();
    EXPECT_EQ(Account::WithdrawResult::InvalidAmount,
              account.withdraw(Money::fromPence(0), TransactionType::Withdrawal, "Nothing"));
    EXPECT_EQ(Account::WithdrawResult::InvalidAmount,
              account.withdraw(Money::fromPence(-1), TransactionType::Withdrawal, "Negative"));
}

TEST(Account, AuthenticatesTheCorrectPin) {
    auto account = makeAccount();
    EXPECT_EQ(Account::AuthResult::Ok, account.authenticate("1234"));
    EXPECT_EQ(0, account.failedAttempts());
}

TEST(Account, DoesNotStoreThePinInPlaintext) {
    const auto account = makeAccount();
    EXPECT_EQ(std::string::npos, account.credential().hash.find("1234"));
}

TEST(Account, LocksAfterThreeConsecutiveFailures) {
    auto account = makeAccount();
    EXPECT_EQ(Account::AuthResult::WrongPin, account.authenticate("0000"));
    EXPECT_EQ(Account::AuthResult::WrongPin, account.authenticate("0000"));
    EXPECT_FALSE(account.isLocked());
    EXPECT_EQ(Account::AuthResult::WrongPin, account.authenticate("0000"));
    EXPECT_TRUE(account.isLocked());
}

TEST(Account, CorrectPinIsRefusedOnceLocked) {
    auto account = makeAccount();
    account.authenticate("0000");
    account.authenticate("0000");
    account.authenticate("0000");
    ASSERT_TRUE(account.isLocked());
    EXPECT_EQ(Account::AuthResult::Locked, account.authenticate("1234"));
}

TEST(Account, SuccessfulLoginResetsTheFailureCounter) {
    auto account = makeAccount();
    account.authenticate("0000");
    account.authenticate("0000");
    EXPECT_EQ(2, account.failedAttempts());
    EXPECT_EQ(Account::AuthResult::Ok, account.authenticate("1234"));
    EXPECT_EQ(0, account.failedAttempts());
    EXPECT_EQ(Account::AuthResult::WrongPin, account.authenticate("0000"));
    EXPECT_FALSE(account.isLocked());
}

TEST(Account, RestoringConstructorPreservesState) {
    auto original = makeAccount();
    original.deposit(Money::fromPence(5000), TransactionType::Deposit, "Pay");

    Account restored(original.accountNumber(), original.name(), original.credential(),
                     original.balance(), original.failedAttempts(),
                     std::vector<Transaction>(original.history()));

    EXPECT_EQ(original.balance(), restored.balance());
    EXPECT_EQ(1u, restored.history().size());
    EXPECT_EQ(Account::AuthResult::Ok, restored.authenticate("1234"));
}
```

- [ ] **Step 4: Run and confirm failure**

Expected: FAIL to compile — the new interfaces do not exist yet.

- [ ] **Step 5: Rewrite src/Transaction.h**

```cpp
#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <optional>
#include <string>

#include "Money.h"

enum class TransactionType { Deposit, Withdrawal, TransferIn, TransferOut };

// Stable token written to the save file. Changing these breaks existing files.
std::string toStorageString(TransactionType type);
std::optional<TransactionType> transactionTypeFromStorageString(const std::string& text);

// Human-facing label. Safe to reword.
std::string toDisplayString(TransactionType type);

// One immutable ledger entry.
class Transaction {
public:
    Transaction(TransactionType type, Money amount, std::string details, Money resultingBalance)
        : type_{type},
          amount_{amount},
          details_{std::move(details)},
          resultingBalance_{resultingBalance} {}

    TransactionType type() const { return type_; }
    Money amount() const { return amount_; }
    const std::string& details() const { return details_; }
    Money resultingBalance() const { return resultingBalance_; }

private:
    TransactionType type_;
    Money amount_;
    std::string details_;
    Money resultingBalance_;
};

#endif
```

Members are now trailing-underscore named, which makes the original shadowing defect impossible to reintroduce: `resultingBalance_{resultingBalance}` cannot accidentally resolve to itself.

- [ ] **Step 6: Rewrite src/Transaction.cpp**

```cpp
#include "Transaction.h"

std::string toStorageString(TransactionType type) {
    switch (type) {
        case TransactionType::Deposit:     return "Deposit";
        case TransactionType::Withdrawal:  return "Withdrawal";
        case TransactionType::TransferIn:  return "TransferIn";
        case TransactionType::TransferOut: return "TransferOut";
    }
    return "Deposit";
}

std::optional<TransactionType> transactionTypeFromStorageString(const std::string& text) {
    if (text == "Deposit")     return TransactionType::Deposit;
    if (text == "Withdrawal")  return TransactionType::Withdrawal;
    if (text == "TransferIn")  return TransactionType::TransferIn;
    if (text == "TransferOut") return TransactionType::TransferOut;
    return std::nullopt;
}

std::string toDisplayString(TransactionType type) {
    switch (type) {
        case TransactionType::Deposit:     return "Deposit";
        case TransactionType::Withdrawal:  return "Withdrawal";
        case TransactionType::TransferIn:  return "Transfer in";
        case TransactionType::TransferOut: return "Transfer out";
    }
    return "Unknown";
}
```

- [ ] **Step 7: Rewrite src/Account.h**

```cpp
#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <string>
#include <vector>

#include "Money.h"
#include "PinCredential.h"
#include "Transaction.h"

// A single bank account. Enforces its own rules and returns results;
// it never writes to the console.
class Account {
public:
    static constexpr int kMaxFailedAttempts = 3;

    enum class AuthResult { Ok, WrongPin, Locked };
    enum class DepositResult { Ok, InvalidAmount, Overflow };
    enum class WithdrawResult { Ok, InvalidAmount, InsufficientFunds };

    // New account: hashes the PIN with a fresh salt.
    Account(int accountNumber, std::string name, const std::string& pin, Money openingBalance);

    // Restore from storage: takes an already-hashed credential.
    Account(int accountNumber, std::string name, PinCredential credential,
            Money balance, int failedAttempts, std::vector<Transaction> history);

    int accountNumber() const { return accountNumber_; }
    const std::string& name() const { return name_; }
    Money balance() const { return balance_; }
    const PinCredential& credential() const { return credential_; }
    int failedAttempts() const { return failedAttempts_; }
    bool isLocked() const { return failedAttempts_ >= kMaxFailedAttempts; }

    // Increments the failure counter on a wrong PIN; resets it on success.
    AuthResult authenticate(const std::string& pin);

    DepositResult deposit(Money amount, TransactionType type, std::string details);
    WithdrawResult withdraw(Money amount, TransactionType type, std::string details);

    const std::vector<Transaction>& history() const { return history_; }

private:
    int accountNumber_;
    std::string name_;
    PinCredential credential_;
    Money balance_;
    int failedAttempts_{0};
    std::vector<Transaction> history_;
};

#endif
```

- [ ] **Step 8: Rewrite src/Account.cpp**

```cpp
#include "Account.h"

#include <utility>

Account::Account(int accountNumber, std::string name, const std::string& pin, Money openingBalance)
    : accountNumber_{accountNumber},
      name_{std::move(name)},
      credential_{makePinCredential(pin)},
      balance_{openingBalance} {}

Account::Account(int accountNumber, std::string name, PinCredential credential,
                 Money balance, int failedAttempts, std::vector<Transaction> history)
    : accountNumber_{accountNumber},
      name_{std::move(name)},
      credential_{std::move(credential)},
      balance_{balance},
      failedAttempts_{failedAttempts},
      history_{std::move(history)} {}

Account::AuthResult Account::authenticate(const std::string& pin) {
    if (isLocked()) return AuthResult::Locked;

    if (!verifyPin(credential_, pin)) {
        ++failedAttempts_;
        return AuthResult::WrongPin;
    }
    failedAttempts_ = 0;
    return AuthResult::Ok;
}

Account::DepositResult Account::deposit(Money amount, TransactionType type, std::string details) {
    if (!amount.isPositive()) return DepositResult::InvalidAmount;

    const auto updated = balance_.tryAdd(amount);
    if (!updated) return DepositResult::Overflow;

    balance_ = *updated;
    history_.emplace_back(type, amount, std::move(details), balance_);
    return DepositResult::Ok;
}

Account::WithdrawResult Account::withdraw(Money amount, TransactionType type, std::string details) {
    if (!amount.isPositive()) return WithdrawResult::InvalidAmount;
    if (amount > balance_) return WithdrawResult::InsufficientFunds;

    const auto updated = balance_.trySubtract(amount);
    if (!updated) return WithdrawResult::InsufficientFunds;

    balance_ = *updated;
    history_.emplace_back(type, amount, std::move(details), balance_);
    return WithdrawResult::Ok;
}
```

- [ ] **Step 9: Temporarily exclude Bank from the build**

`Bank.cpp` still uses the old `double` interfaces and will not compile. Comment `src/Bank.cpp` out of `banking_core` in `CMakeLists.txt`, and comment out the body of `src/cli/main.cpp`'s `main` to `return 0;`. Task 7 restores both.

- [ ] **Step 10: Run the tests**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
```

Expected: PASS. Transaction 4 tests, Account 14 tests, plus Money, Sha256, PinCredential and smoke.

- [ ] **Step 11: Commit**

```bash
git add -A
git commit -m "refactor: rebuild Transaction and Account on Money and PinCredential

Transaction types become an enum rather than free-form strings, and
members are trailing-underscore named so the original self-initialisation
defect cannot recur. Account gains PIN hashing and lockout, and returns
result enums instead of bare bools.

Bank is temporarily excluded from the build; restored in the next commit."
```

---

## Task 7: Rewrite Bank as pure domain logic

**Files:**
- Rewrite: `src/Bank.h`, `src/Bank.cpp`
- Create: `tests/test_bank.cpp`
- Modify: `CMakeLists.txt` (re-enable `src/Bank.cpp`), `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `Account`, `Money`, `Transaction`
- Produces:

```cpp
class Bank {
public:
    enum class CreateResult { Ok, DuplicateAccountNumber, InvalidName, InvalidPin, InvalidAmount };
    enum class LoginResult { Ok, NotFound, WrongPin, Locked };
    enum class TransferResult { Ok, SenderNotFound, ReceiverNotFound, SameAccount,
                                InvalidAmount, InsufficientFunds, Overflow };

    CreateResult createAccount(int accountNumber, const std::string& name,
                               const std::string& pin, Money openingBalance);
    LoginResult logIn(int accountNumber, const std::string& pin);
    TransferResult transfer(int fromAccount, int toAccount, Money amount);

    Account* findAccount(int accountNumber);
    const Account* findAccount(int accountNumber) const;

    const std::vector<Account>& accounts() const;
    void addAccount(Account account);        // used by Storage on load
};
```

**Pointer-lifetime note:** `findAccount` returns a raw pointer into a `std::vector<Account>`, which `addAccount` can invalidate by reallocating. `logIn` therefore returns only a `LoginResult` — never a pointer — and the CLI holds the account *number* for the session, re-resolving it per operation. That keeps every pointer's lifetime inside a single operation.

- [ ] **Step 1: Re-enable Bank in CMakeLists.txt and add test_bank.cpp**

- [ ] **Step 2: Write tests/test_bank.cpp**

```cpp
#include <gtest/gtest.h>
#include "Bank.h"

namespace {
Bank makeBankWithTwoAccounts() {
    Bank bank;
    bank.createAccount(1001, "Alice", "1111", Money::fromPence(10000));
    bank.createAccount(1002, "Bob", "2222", Money::fromPence(5000));
    return bank;
}
}  // namespace

TEST(Bank, CreatesAnAccount) {
    Bank bank;
    EXPECT_EQ(Bank::CreateResult::Ok,
              bank.createAccount(1001, "Alice", "1111", Money::fromPence(10000)));
    ASSERT_NE(nullptr, bank.findAccount(1001));
    EXPECT_EQ("Alice", bank.findAccount(1001)->name());
}

TEST(Bank, RejectsDuplicateAccountNumbers) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::CreateResult::DuplicateAccountNumber,
              bank.createAccount(1001, "Mallory", "3333", Money::fromPence(100)));
    EXPECT_EQ(2u, bank.accounts().size());
}

TEST(Bank, RejectsEmptyNameAndPin) {
    Bank bank;
    EXPECT_EQ(Bank::CreateResult::InvalidName,
              bank.createAccount(1001, "", "1111", Money::fromPence(100)));
    EXPECT_EQ(Bank::CreateResult::InvalidPin,
              bank.createAccount(1001, "Alice", "", Money::fromPence(100)));
    EXPECT_TRUE(bank.accounts().empty());
}

TEST(Bank, RejectsNegativeOpeningBalance) {
    Bank bank;
    EXPECT_EQ(Bank::CreateResult::InvalidAmount,
              bank.createAccount(1001, "Alice", "1111", Money::fromPence(-1)));
}

TEST(Bank, AllowsZeroOpeningBalance) {
    Bank bank;
    EXPECT_EQ(Bank::CreateResult::Ok,
              bank.createAccount(1001, "Alice", "1111", Money::fromPence(0)));
}

TEST(Bank, FindsNothingForAnUnknownAccount) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(nullptr, bank.findAccount(9999));
}

TEST(Bank, LogsInWithCorrectCredentials) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::LoginResult::Ok, bank.logIn(1001, "1111"));
}

TEST(Bank, ReportsUnknownAccountAndWrongPinSeparately) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::LoginResult::NotFound, bank.logIn(9999, "1111"));
    EXPECT_EQ(Bank::LoginResult::WrongPin, bank.logIn(1001, "0000"));
}

TEST(Bank, ReportsLockedAccounts) {
    auto bank = makeBankWithTwoAccounts();
    bank.logIn(1001, "0000");
    bank.logIn(1001, "0000");
    bank.logIn(1001, "0000");
    EXPECT_EQ(Bank::LoginResult::Locked, bank.logIn(1001, "1111"));
}

TEST(Bank, TransferMovesMoneyBetweenAccounts) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::TransferResult::Ok, bank.transfer(1001, 1002, Money::fromPence(2500)));
    EXPECT_EQ(Money::fromPence(7500), bank.findAccount(1001)->balance());
    EXPECT_EQ(Money::fromPence(7500), bank.findAccount(1002)->balance());
}

TEST(Bank, TransferRecordsBothSidesInHistory) {
    auto bank = makeBankWithTwoAccounts();
    bank.transfer(1001, 1002, Money::fromPence(2500));

    ASSERT_EQ(1u, bank.findAccount(1001)->history().size());
    ASSERT_EQ(1u, bank.findAccount(1002)->history().size());
    EXPECT_EQ(TransactionType::TransferOut, bank.findAccount(1001)->history().front().type());
    EXPECT_EQ(TransactionType::TransferIn, bank.findAccount(1002)->history().front().type());
}

// The most important test in this file: a rejected transfer must not move
// money out of one account without putting it into the other.
TEST(Bank, FailedTransferLeavesBothBalancesUnchanged) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::TransferResult::InsufficientFunds,
              bank.transfer(1001, 1002, Money::fromPence(999999)));
    EXPECT_EQ(Money::fromPence(10000), bank.findAccount(1001)->balance());
    EXPECT_EQ(Money::fromPence(5000), bank.findAccount(1002)->balance());
    EXPECT_TRUE(bank.findAccount(1001)->history().empty());
    EXPECT_TRUE(bank.findAccount(1002)->history().empty());
}

TEST(Bank, RejectsTransferToAnUnknownAccount) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::TransferResult::ReceiverNotFound,
              bank.transfer(1001, 9999, Money::fromPence(100)));
    EXPECT_EQ(Money::fromPence(10000), bank.findAccount(1001)->balance());
}

TEST(Bank, RejectsTransferFromAnUnknownAccount) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::TransferResult::SenderNotFound,
              bank.transfer(9999, 1001, Money::fromPence(100)));
}

TEST(Bank, RejectsTransferToSelf) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::TransferResult::SameAccount,
              bank.transfer(1001, 1001, Money::fromPence(100)));
    EXPECT_EQ(Money::fromPence(10000), bank.findAccount(1001)->balance());
}

TEST(Bank, RejectsNonPositiveTransferAmounts) {
    auto bank = makeBankWithTwoAccounts();
    EXPECT_EQ(Bank::TransferResult::InvalidAmount,
              bank.transfer(1001, 1002, Money::fromPence(0)));
    EXPECT_EQ(Bank::TransferResult::InvalidAmount,
              bank.transfer(1001, 1002, Money::fromPence(-100)));
}

TEST(Bank, RejectsTransferThatWouldOverflowTheReceiver) {
    Bank bank;
    bank.createAccount(1001, "Alice", "1111", Money::fromPence(10000));
    bank.addAccount(Account(1002, "Rich", makePinCredential("2222"),
                            Money::fromPence(std::numeric_limits<std::int64_t>::max()),
                            0, std::vector<Transaction>{}));
    EXPECT_EQ(Bank::TransferResult::Overflow,
              bank.transfer(1001, 1002, Money::fromPence(100)));
    EXPECT_EQ(Money::fromPence(10000), bank.findAccount(1001)->balance());
}
```

Add `#include <limits>` and `#include <cstdint>` at the top.

- [ ] **Step 3: Run and confirm failure**

Expected: FAIL to compile — the new `Bank` interface does not exist.

- [ ] **Step 4: Write src/Bank.h**

```cpp
#ifndef BANK_H
#define BANK_H

#include <string>
#include <vector>

#include "Account.h"
#include "Money.h"

// Owns every account and enforces the rules that span more than one of them.
// Returns results; never writes to the console and never touches the disk.
class Bank {
public:
    enum class CreateResult { Ok, DuplicateAccountNumber, InvalidName, InvalidPin, InvalidAmount };
    enum class LoginResult { Ok, NotFound, WrongPin, Locked };
    enum class TransferResult { Ok, SenderNotFound, ReceiverNotFound, SameAccount,
                                InvalidAmount, InsufficientFunds, Overflow };

    CreateResult createAccount(int accountNumber, const std::string& name,
                               const std::string& pin, Money openingBalance);

    // Returns a result only. Callers hold the account number for the session
    // and re-resolve it per operation, because addAccount can reallocate the
    // underlying vector and invalidate any pointer handed out earlier.
    LoginResult logIn(int accountNumber, const std::string& pin);

    TransferResult transfer(int fromAccount, int toAccount, Money amount);

    Account* findAccount(int accountNumber);
    const Account* findAccount(int accountNumber) const;

    const std::vector<Account>& accounts() const { return accounts_; }
    void addAccount(Account account) { accounts_.push_back(std::move(account)); }

private:
    std::vector<Account> accounts_;
};

#endif
```

- [ ] **Step 5: Write src/Bank.cpp**

```cpp
#include "Bank.h"

#include <utility>

Bank::CreateResult Bank::createAccount(int accountNumber, const std::string& name,
                                       const std::string& pin, Money openingBalance) {
    if (findAccount(accountNumber) != nullptr) return CreateResult::DuplicateAccountNumber;
    if (name.empty()) return CreateResult::InvalidName;
    if (pin.empty()) return CreateResult::InvalidPin;
    if (openingBalance.pence() < 0) return CreateResult::InvalidAmount;

    accounts_.emplace_back(accountNumber, name, pin, openingBalance);
    return CreateResult::Ok;
}

Bank::LoginResult Bank::logIn(int accountNumber, const std::string& pin) {
    Account* account = findAccount(accountNumber);
    if (account == nullptr) return LoginResult::NotFound;

    switch (account->authenticate(pin)) {
        case Account::AuthResult::Ok:       return LoginResult::Ok;
        case Account::AuthResult::Locked:   return LoginResult::Locked;
        case Account::AuthResult::WrongPin: break;
    }
    // A wrong PIN may have been the one that tripped the lock.
    return account->isLocked() ? LoginResult::Locked : LoginResult::WrongPin;
}

Bank::TransferResult Bank::transfer(int fromAccount, int toAccount, Money amount) {
    if (fromAccount == toAccount) return TransferResult::SameAccount;
    if (!amount.isPositive()) return TransferResult::InvalidAmount;

    Account* sender = findAccount(fromAccount);
    if (sender == nullptr) return TransferResult::SenderNotFound;
    Account* receiver = findAccount(toAccount);
    if (receiver == nullptr) return TransferResult::ReceiverNotFound;

    // Check both legs before mutating either, so a rejected transfer cannot
    // leave money withdrawn from one account and not credited to the other.
    if (amount > sender->balance()) return TransferResult::InsufficientFunds;
    if (!receiver->balance().tryAdd(amount).has_value()) return TransferResult::Overflow;

    const auto withdrawn =
        sender->withdraw(amount, TransactionType::TransferOut, "Sent to " + receiver->name());
    if (withdrawn != Account::WithdrawResult::Ok) return TransferResult::InsufficientFunds;

    const auto deposited =
        receiver->deposit(amount, TransactionType::TransferIn, "Received from " + sender->name());
    if (deposited != Account::DepositResult::Ok) {
        // Unreachable given the checks above, but if it ever happens, put the
        // money back rather than losing it.
        sender->deposit(amount, TransactionType::TransferIn, "Reversed failed transfer");
        return TransferResult::Overflow;
    }
    return TransferResult::Ok;
}

Account* Bank::findAccount(int accountNumber) {
    for (auto& account : accounts_) {
        if (account.accountNumber() == accountNumber) return &account;
    }
    return nullptr;
}

const Account* Bank::findAccount(int accountNumber) const {
    for (const auto& account : accounts_) {
        if (account.accountNumber() == accountNumber) return &account;
    }
    return nullptr;
}
```

- [ ] **Step 6: Run the tests**

```bash
cmake --build build && ctest --test-dir build -R Bank --output-on-failure
```

Expected: PASS, 17 tests.

- [ ] **Step 7: Commit**

```bash
git add src/Bank.h src/Bank.cpp tests/test_bank.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "refactor: make Bank pure domain logic returning result enums

Bank no longer prints to stdout, which is what made the previous code
untestable. Transfer now validates both legs before mutating either, so a
rejected transfer cannot debit one account without crediting the other."
```

---

## Task 8: Storage module

**Files:**
- Create: `src/Storage.h`, `src/Storage.cpp`
- Create: `tests/test_storage.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`
- Delete: the `saveToFile` / `loadFromFile` declarations already removed from `Bank` in Task 7

**Interfaces:**
- Consumes: `Bank`, `Account`, `Money`, `Transaction`, `PinCredential`
- Produces:

```cpp
namespace storage {
struct SaveOutcome { bool ok; std::string error; };
struct LoadOutcome { bool ok; std::string error; int line; };

SaveOutcome save(const Bank& bank, const std::string& path);
LoadOutcome load(Bank& bank, const std::string& path);   // missing file => ok

std::string escapeField(const std::string& field);
std::string unescapeField(const std::string& field);
std::vector<std::string> splitEscaped(const std::string& line);
}
```

**File format.** One record per line, prefixed with its type. Every field is escaped.

```
ACC,<accountNumber>,<name>,<saltHex>,<hashHex>,<balancePence>,<failedAttempts>
TXN,<type>,<amountPence>,<details>,<resultingBalancePence>
```

`TXN` records attach to the most recent `ACC`. A `TXN` before any `ACC` is an error.

**Escaping.** On write, `\` becomes `\\`, then `,` becomes `\,`. Backslash is escaped first so the transformation is reversible. On read, a backslash escapes whatever follows it.

- [ ] **Step 1: Add to both CMake source lists**

- [ ] **Step 2: Write tests/test_storage.cpp**

```cpp
#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

#include "Storage.h"

namespace {

// Writes to a uniquely named file in the working directory and removes it.
class TempFile {
public:
    explicit TempFile(std::string name) : path_{std::move(name)} {}
    ~TempFile() { std::remove(path_.c_str()); }
    const std::string& path() const { return path_; }

    void write(const std::string& contents) const {
        std::ofstream out(path_);
        out << contents;
    }

private:
    std::string path_;
};

Bank makeBank() {
    Bank bank;
    bank.createAccount(1001, "Alice", "1111", Money::fromPence(10000));
    bank.createAccount(1002, "Bob", "2222", Money::fromPence(5000));
    return bank;
}

}  // namespace

TEST(StorageEscaping, LeavesOrdinaryTextAlone) {
    EXPECT_EQ("Alice", storage::escapeField("Alice"));
}

TEST(StorageEscaping, EscapesCommasAndBackslashes) {
    EXPECT_EQ("Smith\\, John", storage::escapeField("Smith, John"));
    EXPECT_EQ("a\\\\b", storage::escapeField("a\\b"));
}

TEST(StorageEscaping, RoundTrips) {
    const std::string awkward = "Smith, John \\ \"Jr\", ,,";
    EXPECT_EQ(awkward, storage::unescapeField(storage::escapeField(awkward)));
}

TEST(StorageEscaping, SplitsOnUnescapedCommasOnly) {
    const auto fields = storage::splitEscaped("ACC,Smith\\, John,1234");
    ASSERT_EQ(3u, fields.size());
    EXPECT_EQ("ACC", fields[0]);
    EXPECT_EQ("Smith, John", fields[1]);
    EXPECT_EQ("1234", fields[2]);
}

TEST(Storage, RoundTripsAnEmptyBank) {
    TempFile file("test_empty.txt");
    Bank original;
    ASSERT_TRUE(storage::save(original, file.path()).ok);

    Bank loaded;
    const auto outcome = storage::load(loaded, file.path());
    EXPECT_TRUE(outcome.ok) << outcome.error;
    EXPECT_TRUE(loaded.accounts().empty());
}

TEST(Storage, RoundTripsAccountsAndBalances) {
    TempFile file("test_round_trip.txt");
    auto original = makeBank();
    ASSERT_TRUE(storage::save(original, file.path()).ok);

    Bank loaded;
    ASSERT_TRUE(storage::load(loaded, file.path()).ok);

    ASSERT_EQ(2u, loaded.accounts().size());
    ASSERT_NE(nullptr, loaded.findAccount(1001));
    EXPECT_EQ("Alice", loaded.findAccount(1001)->name());
    EXPECT_EQ(Money::fromPence(10000), loaded.findAccount(1001)->balance());
    EXPECT_EQ(Money::fromPence(5000), loaded.findAccount(1002)->balance());
}

TEST(Storage, RoundTripsTransactionHistory) {
    TempFile file("test_history.txt");
    auto original = makeBank();
    original.transfer(1001, 1002, Money::fromPence(2500));
    ASSERT_TRUE(storage::save(original, file.path()).ok);

    Bank loaded;
    ASSERT_TRUE(storage::load(loaded, file.path()).ok);

    const auto& history = loaded.findAccount(1001)->history();
    ASSERT_EQ(1u, history.size());
    EXPECT_EQ(TransactionType::TransferOut, history.front().type());
    EXPECT_EQ(Money::fromPence(2500), history.front().amount());
    EXPECT_EQ(Money::fromPence(7500), history.front().resultingBalance());
    EXPECT_EQ("Sent to Bob", history.front().details());
}

// The current CSV format silently corrupts this.
TEST(Storage, SurvivesANameContainingACommaAndBackslash) {
    TempFile file("test_awkward_name.txt");
    Bank original;
    original.createAccount(1001, "Smith, John \\ Jr", "1111", Money::fromPence(100));
    ASSERT_TRUE(storage::save(original, file.path()).ok);

    Bank loaded;
    ASSERT_TRUE(storage::load(loaded, file.path()).ok);
    ASSERT_NE(nullptr, loaded.findAccount(1001));
    EXPECT_EQ("Smith, John \\ Jr", loaded.findAccount(1001)->name());
}

TEST(Storage, PreservesPinsAcrossSaveAndLoad) {
    TempFile file("test_pins.txt");
    auto original = makeBank();
    ASSERT_TRUE(storage::save(original, file.path()).ok);

    Bank loaded;
    ASSERT_TRUE(storage::load(loaded, file.path()).ok);
    EXPECT_EQ(Bank::LoginResult::Ok, loaded.logIn(1001, "1111"));
    EXPECT_EQ(Bank::LoginResult::WrongPin, loaded.logIn(1002, "9999"));
}

TEST(Storage, PreservesTheFailedAttemptCounter) {
    TempFile file("test_attempts.txt");
    auto original = makeBank();
    original.logIn(1001, "0000");
    original.logIn(1001, "0000");
    ASSERT_TRUE(storage::save(original, file.path()).ok);

    Bank loaded;
    ASSERT_TRUE(storage::load(loaded, file.path()).ok);
    EXPECT_EQ(2, loaded.findAccount(1001)->failedAttempts());
    // One more failure locks it, proving the counter carried across.
    EXPECT_EQ(Bank::LoginResult::Locked, loaded.logIn(1001, "0000"));
}

TEST(Storage, TreatsAMissingFileAsAnEmptyBank) {
    Bank bank;
    const auto outcome = storage::load(bank, "definitely_does_not_exist_12345.txt");
    EXPECT_TRUE(outcome.ok);
    EXPECT_TRUE(bank.accounts().empty());
}

TEST(Storage, IgnoresBlankLines) {
    TempFile file("test_blank_lines.txt");
    file.write("\nACC,1001,Alice,abc,def,10000,0\n\n\n");
    Bank bank;
    EXPECT_TRUE(storage::load(bank, file.path()).ok);
    EXPECT_EQ(1u, bank.accounts().size());
}

TEST(Storage, RejectsAnUnknownRecordType) {
    TempFile file("test_unknown_record.txt");
    file.write("WAT,1001,Alice\n");
    Bank bank;
    const auto outcome = storage::load(bank, file.path());
    EXPECT_FALSE(outcome.ok);
    EXPECT_EQ(1, outcome.line);
}

TEST(Storage, RejectsAnAccountLineWithTooFewFields) {
    TempFile file("test_short_account.txt");
    file.write("ACC,1001,Alice\n");
    Bank bank;
    const auto outcome = storage::load(bank, file.path());
    EXPECT_FALSE(outcome.ok);
    EXPECT_EQ(1, outcome.line);
}

TEST(Storage, RejectsNonNumericFields) {
    TempFile file("test_non_numeric.txt");
    file.write("ACC,notanumber,Alice,abc,def,10000,0\n");
    Bank bank;
    EXPECT_FALSE(storage::load(bank, file.path()).ok);
}

TEST(Storage, RejectsATransactionBeforeAnyAccount) {
    TempFile file("test_orphan_txn.txt");
    file.write("TXN,Deposit,5000,Pay,15000\n");
    Bank bank;
    const auto outcome = storage::load(bank, file.path());
    EXPECT_FALSE(outcome.ok);
    EXPECT_EQ(1, outcome.line);
}

TEST(Storage, RejectsAnUnknownTransactionType) {
    TempFile file("test_bad_txn_type.txt");
    file.write("ACC,1001,Alice,abc,def,10000,0\nTXN,Nonsense,5000,Pay,15000\n");
    Bank bank;
    const auto outcome = storage::load(bank, file.path());
    EXPECT_FALSE(outcome.ok);
    EXPECT_EQ(2, outcome.line);
}

TEST(Storage, RejectsATruncatedFinalLine) {
    TempFile file("test_truncated.txt");
    file.write("ACC,1001,Alice,abc,def,10000,0\nTXN,Deposit,5000\n");
    Bank bank;
    EXPECT_FALSE(storage::load(bank, file.path()).ok);
}

// A failed load must not partially populate the caller's bank. Starting from
// a non-empty bank is what makes this test meaningful: if load() wrote
// directly into `bank`, the good first line would have landed before the bad
// second line aborted it.
TEST(Storage, LeavesTheBankUntouchedWhenLoadingFails) {
    TempFile file("test_atomic_load.txt");
    file.write("ACC,1001,Alice,abc,def,10000,0\nGARBAGE\n");

    Bank bank;
    bank.createAccount(7777, "Pre-existing", "9999", Money::fromPence(300));

    EXPECT_FALSE(storage::load(bank, file.path()).ok);
    ASSERT_EQ(1u, bank.accounts().size());
    EXPECT_EQ("Pre-existing", bank.accounts().front().name());
    EXPECT_EQ(nullptr, bank.findAccount(1001));   // the good line did not land
}
```

- [ ] **Step 3: Run and confirm failure**

Expected: FAIL to compile — `Storage.h` does not exist.

- [ ] **Step 4: Write src/Storage.h**

```cpp
#ifndef STORAGE_H
#define STORAGE_H

#include <string>
#include <vector>

#include "Bank.h"

// Reads and writes the save file. The only module that touches the disk.
//
// Format, one record per line, every field escaped:
//   ACC,<number>,<name>,<saltHex>,<hashHex>,<balancePence>,<failedAttempts>
//   TXN,<type>,<amountPence>,<details>,<resultingBalancePence>
//
// TXN records attach to the most recent ACC. The record prefix means the
// parser knows what it is reading rather than inferring it from whether a
// conversion threw.
namespace storage {

struct SaveOutcome { bool ok{false}; std::string error; };
struct LoadOutcome { bool ok{false}; std::string error; int line{0}; };

SaveOutcome save(const Bank& bank, const std::string& path);

// A missing file is not an error: it means a first run. Anything malformed
// is, and the bank is left untouched rather than partially populated.
LoadOutcome load(Bank& bank, const std::string& path);

// Escapes backslash first, then comma, so the transformation is reversible.
std::string escapeField(const std::string& field);
std::string unescapeField(const std::string& field);
std::vector<std::string> splitEscaped(const std::string& line);

}  // namespace storage

#endif
```

- [ ] **Step 5: Write src/Storage.cpp**

```cpp
#include "Storage.h"

#include <fstream>
#include <string>
#include <vector>

namespace storage {
namespace {

// Parses a whole decimal integer. Returns false on empty input, stray
// characters, or overflow. Deliberately avoids std::stoll, which reports
// failure by throwing.
bool parseInt64(const std::string& text, std::int64_t& out) {
    if (text.empty()) return false;

    std::size_t index = 0;
    bool negative = false;
    if (text[0] == '-') {
        negative = true;
        index = 1;
        if (text.size() == 1) return false;
    }

    std::int64_t value = 0;
    for (; index < text.size(); ++index) {
        const char c = text[index];
        if (c < '0' || c > '9') return false;
        const int digit = c - '0';
        if (value > (std::numeric_limits<std::int64_t>::max() - digit) / 10) return false;
        value = value * 10 + digit;
    }
    out = negative ? -value : value;
    return true;
}

bool parseInt(const std::string& text, int& out) {
    std::int64_t wide = 0;
    if (!parseInt64(text, wide)) return false;
    if (wide < std::numeric_limits<int>::min() || wide > std::numeric_limits<int>::max()) {
        return false;
    }
    out = static_cast<int>(wide);
    return true;
}

LoadOutcome failure(std::string message, int line) {
    LoadOutcome outcome;
    outcome.ok = false;
    outcome.error = std::move(message);
    outcome.line = line;
    return outcome;
}

}  // namespace

std::string escapeField(const std::string& field) {
    std::string out;
    out.reserve(field.size());
    for (char c : field) {
        if (c == '\\' || c == ',') out += '\\';
        out += c;
    }
    return out;
}

std::string unescapeField(const std::string& field) {
    std::string out;
    out.reserve(field.size());
    for (std::size_t i = 0; i < field.size(); ++i) {
        if (field[i] == '\\' && i + 1 < field.size()) {
            out += field[++i];
        } else {
            out += field[i];
        }
    }
    return out;
}

std::vector<std::string> splitEscaped(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;
    for (std::size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '\\' && i + 1 < line.size()) {
            current += line[i];
            current += line[++i];
        } else if (line[i] == ',') {
            fields.push_back(unescapeField(current));
            current.clear();
        } else {
            current += line[i];
        }
    }
    fields.push_back(unescapeField(current));
    return fields;
}

SaveOutcome save(const Bank& bank, const std::string& path) {
    std::ofstream file(path);
    if (!file) return {false, "Could not open " + path + " for writing"};

    for (const auto& account : bank.accounts()) {
        file << "ACC,"
             << account.accountNumber() << ','
             << escapeField(account.name()) << ','
             << escapeField(account.credential().salt) << ','
             << escapeField(account.credential().hash) << ','
             << account.balance().pence() << ','
             << account.failedAttempts() << '\n';

        for (const auto& transaction : account.history()) {
            file << "TXN,"
                 << toStorageString(transaction.type()) << ','
                 << transaction.amount().pence() << ','
                 << escapeField(transaction.details()) << ','
                 << transaction.resultingBalance().pence() << '\n';
        }
    }

    if (!file) return {false, "Failed while writing " + path};
    return {true, ""};
}

LoadOutcome load(Bank& bank, const std::string& path) {
    std::ifstream file(path);
    if (!file) return {true, "", 0};   // first run

    // Build into a scratch bank so a malformed file cannot leave the caller
    // holding half a dataset.
    Bank scratch;
    struct PendingAccount {
        int number{};
        std::string name;
        PinCredential credential;
        Money balance;
        int failedAttempts{};
        std::vector<Transaction> history;
        bool valid{false};
    } pending;

    auto flush = [&scratch, &pending]() {
        if (!pending.valid) return;
        scratch.addAccount(Account(pending.number, pending.name, pending.credential,
                                   pending.balance, pending.failedAttempts,
                                   std::move(pending.history)));
        pending = PendingAccount{};
    };

    std::string line;
    int lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        if (!line.empty() && line.back() == '\r') line.pop_back();   // CRLF files
        if (line.empty()) continue;

        const auto fields = splitEscaped(line);

        if (fields[0] == "ACC") {
            if (fields.size() != 7) return failure("Account record needs 7 fields", lineNumber);

            flush();

            int number = 0;
            std::int64_t balancePence = 0;
            int attempts = 0;
            if (!parseInt(fields[1], number))            return failure("Bad account number", lineNumber);
            if (!parseInt64(fields[5], balancePence))    return failure("Bad balance", lineNumber);
            if (!parseInt(fields[6], attempts))          return failure("Bad attempt count", lineNumber);
            if (attempts < 0)                            return failure("Negative attempt count", lineNumber);

            pending.number = number;
            pending.name = fields[2];
            pending.credential = restorePinCredential(fields[3], fields[4]);
            pending.balance = Money::fromPence(balancePence);
            pending.failedAttempts = attempts;
            pending.valid = true;

        } else if (fields[0] == "TXN") {
            if (fields.size() != 5) return failure("Transaction record needs 5 fields", lineNumber);
            if (!pending.valid) return failure("Transaction before any account", lineNumber);

            const auto type = transactionTypeFromStorageString(fields[1]);
            if (!type) return failure("Unknown transaction type", lineNumber);

            std::int64_t amountPence = 0;
            std::int64_t resultingPence = 0;
            if (!parseInt64(fields[2], amountPence))     return failure("Bad amount", lineNumber);
            if (!parseInt64(fields[4], resultingPence))  return failure("Bad resulting balance", lineNumber);

            pending.history.emplace_back(*type, Money::fromPence(amountPence), fields[3],
                                         Money::fromPence(resultingPence));

        } else {
            return failure("Unknown record type '" + fields[0] + "'", lineNumber);
        }
    }

    flush();
    bank = std::move(scratch);
    return {true, "", 0};
}

}  // namespace storage
```

Add `#include <cstdint>` and `#include <limits>` at the top of the file.

- [ ] **Step 6: Run the tests**

```bash
cmake --build build && ctest --test-dir build -R Storage --output-on-failure
```

Expected: PASS, 19 tests.

- [ ] **Step 7: Commit**

```bash
git add src/Storage.h src/Storage.cpp tests/test_storage.cpp CMakeLists.txt tests/CMakeLists.txt
git commit -m "feat: extract Storage with an escaped, self-describing format

Records carry an explicit ACC/TXN prefix so the parser no longer infers
record type from whether a conversion threw. Fields are escaped, so names
containing commas survive a round trip. A malformed file leaves the bank
untouched rather than partially populated."
```

---

## Task 9: CLI with validated input

**Files:**
- Create: `src/cli/Cli.h`, `src/cli/Cli.cpp`
- Rewrite: `src/cli/main.cpp`
- Modify: `CMakeLists.txt`

**Interfaces:**
- Consumes: `Bank`, `Storage`, `Money`, `Account`
- Produces: `class Cli { public: Cli(std::istream&, std::ostream&); int run(const std::string& dataFile); };`

Taking streams by reference rather than using `std::cin` / `std::cout` directly keeps the door open to testing later, at no cost today.

- [ ] **Step 1: Add src/cli/Cli.cpp to the bank executable in CMakeLists.txt**

```cmake
add_executable(bank
    src/cli/main.cpp
    src/cli/Cli.cpp
)
```

- [ ] **Step 2: Write src/cli/Cli.h**

```cpp
#ifndef CLI_H
#define CLI_H

#include <iosfwd>
#include <optional>
#include <string>

#include "Bank.h"
#include "Money.h"

// Owns every line of console output and all input validation. The only part
// of the program that talks to a human.
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

    // Input helpers. Each returns nullopt on end-of-input so the program can
    // exit cleanly instead of looping forever.
    std::optional<int> readInt(const std::string& prompt);
    std::optional<std::string> readLine(const std::string& prompt);
    std::optional<Money> readMoney(const std::string& prompt);

    std::istream& in_;
    std::ostream& out_;
    Bank bank_;
};

#endif
```

- [ ] **Step 3: Write src/cli/Cli.cpp**

```cpp
#include "Cli.h"

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
    const auto loaded = storage::load(bank_, dataFile);
    if (!loaded.ok) {
        out_ << "Could not read " << dataFile << ": " << loaded.error
             << " (line " << loaded.line << ")\n"
             << "Refusing to start so the existing file is not overwritten.\n";
        return 1;
    }

    mainMenu();

    const auto saved = storage::save(bank_, dataFile);
    if (!saved.ok) {
        out_ << "Warning: could not save: " << saved.error << "\n";
        return 1;
    }
    return 0;
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
            case kMenuCreate: createAccount(); break;
            case kMenuLogin:  loginFlow(); break;
            case kMenuExit:   return;
            default: out_ << "Please choose 1, 2 or 3.\n"; break;
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

    switch (bank_.logIn(*number, *pin)) {
        case Bank::LoginResult::Ok: {
            const Account* account = bank_.findAccount(*number);
            out_ << "Welcome, " << account->name() << ".\n";
            sessionMenu(*number);
            break;
        }
        case Bank::LoginResult::NotFound:
        case Bank::LoginResult::WrongPin:
            // Deliberately identical: telling an attacker which account
            // numbers exist is free reconnaissance.
            out_ << "Incorrect account number or PIN.\n";
            break;
        case Bank::LoginResult::Locked:
            out_ << "This account is locked after " << Account::kMaxFailedAttempts
                 << " failed attempts and cannot be unlocked.\n";
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
            out_ << "Deposited " << amount->toString()
                 << ". Balance: " << account->balance().toString() << "\n";
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
            out_ << "Withdrew " << amount->toString()
                 << ". Balance: " << account->balance().toString() << "\n";
            break;
        case Account::WithdrawResult::InvalidAmount:
            out_ << "Amount must be greater than zero.\n";
            break;
        case Account::WithdrawResult::InsufficientFunds:
            out_ << "Insufficient funds. Balance: "
                 << account->balance().toString() << "\n";
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
            out_ << "Sent " << amount->toString() << " to "
                 << bank_.findAccount(*target)->name() << ". Balance: "
                 << bank_.findAccount(accountNumber)->balance().toString() << "\n";
            break;
        case Bank::TransferResult::ReceiverNotFound:
            out_ << "No account with that number.\n"; break;
        case Bank::TransferResult::SenderNotFound:
            out_ << "Your account could not be found.\n"; break;
        case Bank::TransferResult::SameAccount:
            out_ << "You cannot transfer to your own account.\n"; break;
        case Bank::TransferResult::InvalidAmount:
            out_ << "Amount must be greater than zero.\n"; break;
        case Bank::TransferResult::InsufficientFunds:
            out_ << "Insufficient funds.\n"; break;
        case Bank::TransferResult::Overflow:
            out_ << "That transfer would exceed the recipient's maximum balance.\n"; break;
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
        out_ << toDisplayString(transaction.type()) << "  "
             << transaction.amount().toString() << "  "
             << transaction.details() << "  (balance "
             << transaction.resultingBalance().toString() << ")\n";
    }
}

std::optional<int> Cli::readInt(const std::string& prompt) {
    while (true) {
        out_ << prompt;
        std::string line;
        if (!std::getline(in_, line)) return std::nullopt;

        try {
            std::size_t consumed = 0;
            const int value = std::stoi(line, &consumed);
            if (consumed == line.size() && !line.empty()) return value;
        } catch (const std::exception&) {
            // fall through to the retry message
        }
        out_ << "Please enter a whole number.\n";
    }
}

std::optional<std::string> Cli::readLine(const std::string& prompt) {
    out_ << prompt;
    std::string line;
    if (!std::getline(in_, line)) return std::nullopt;
    return line;
}

std::optional<Money> Cli::readMoney(const std::string& prompt) {
    while (true) {
        out_ << prompt;
        std::string line;
        if (!std::getline(in_, line)) return std::nullopt;

        if (auto amount = Money::fromString(line)) return amount;
        out_ << "Please enter an amount such as 100 or 12.34.\n";
    }
}
```

Every read uses `std::getline`, never `operator>>`. That fixes both defect 7 (a non-numeric entry no longer leaves the stream in a failed state, looping forever) and defect 9 (a name with a space is read whole rather than spilling into the PIN field).

`std::stoi` throwing here is acceptable and contained: it is a boundary conversion whose failure is caught immediately and turned into a retry prompt, not control flow threaded through the program.

- [ ] **Step 4: Rewrite src/cli/main.cpp**

```cpp
#include <iostream>

#include "Cli.h"

int main() {
    Cli cli(std::cin, std::cout);
    return cli.run("accounts.txt");
}
```

- [ ] **Step 5: Build and confirm the whole project compiles**

```bash
cmake --build build
```

Expected: builds clean, no warnings.

- [ ] **Step 6: Run the app end to end manually**

```bash
cd build/Debug && printf '1\n12345\nJohn Smith\n1234\n100.00\n2\n12345\n1234\n1\n50.50\n5\n4\n6\n3\n' | ./bank.exe
```

Expected: account created for `John Smith` (full name, not `John`), deposit of £50.50 accepted, history shows `Deposit  £50.50  Money deposited  (balance £150.50)`, balance reads `£150.50`.

- [ ] **Step 7: Verify bad input no longer loops forever**

```bash
cd build/Debug && printf 'abc\nxyz\n3\n' | timeout 10 ./bank.exe
```

Expected: two "Please enter a whole number." messages, then a clean exit. Exit code 0, not a timeout.

- [ ] **Step 8: Verify the saved file and clean up**

```bash
cd build/Debug && cat accounts.txt && rm accounts.txt
```

Expected: an `ACC` line for `John Smith` with a 32-char salt and 64-char hash and balance `15050`, plus one `TXN` line whose resulting balance is `15050` — not a garbage figure.

- [ ] **Step 9: Run the full test suite**

```bash
ctest --test-dir build --output-on-failure
```

Expected: all tests pass.

- [ ] **Step 10: Commit**

```bash
git add -A
git commit -m "feat: rewrite the CLI with validated line-based input

All input now goes through std::getline, which fixes two defects: a
non-numeric entry no longer leaves cin in a failed state and spins
forever, and a name containing a space is read whole instead of
spilling into the PIN field.

Login reports an unknown account and a wrong PIN identically so the
prompt cannot be used to enumerate valid account numbers."
```

---

## Task 10: Continuous integration

**Files:**
- Create: `.github/workflows/ci.yml`

**Interfaces:**
- Consumes: the CMake targets from Task 1
- Produces: a CI badge URL for the README in Task 11

- [ ] **Step 1: Write .github/workflows/ci.yml**

```yaml
name: CI

on:
  push:
    branches: [main]
  pull_request:

jobs:
  build-and-test:
    name: ${{ matrix.os }}
    runs-on: ${{ matrix.os }}
    strategy:
      fail-fast: false
      matrix:
        os: [ubuntu-latest, windows-latest]

    steps:
      - uses: actions/checkout@v4

      - name: Configure
        run: cmake -B build -DCMAKE_BUILD_TYPE=Release

      - name: Build
        run: cmake --build build --config Release

      - name: Test
        run: ctest --test-dir build --build-config Release --output-on-failure
```

`fail-fast: false` means a Windows failure still lets the Linux job finish, so both results are visible in one run.

- [ ] **Step 2: Verify the same commands pass locally**

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --config Release
ctest --test-dir build-release --build-config Release --output-on-failure
```

Expected: all tests pass in Release as well as Debug. Do not push a workflow whose commands have not been run locally.

- [ ] **Step 3: Commit and push**

```bash
git add .github/workflows/ci.yml
git commit -m "ci: build and test on Linux and Windows

Runs the same CMake and CTest commands on GCC and MSVC. The project had
only ever been compiled with one compiler on one machine."
git push
```

- [ ] **Step 4: Confirm the run goes green**

```bash
gh run watch
```

Expected: both jobs succeed. If Windows fails on the GoogleTest link, confirm `gtest_force_shared_crt` is set in `tests/CMakeLists.txt`.

Do not proceed to Task 11 until CI is green — the README is about to claim it is.

---

## Task 11: README rewrite

**Files:**
- Rewrite: `README.md`
- Delete: `TODO.md`
- Create: `docs/screenshot.txt` (captured terminal session)

**Interfaces:**
- Consumes: the green CI run from Task 10

- [ ] **Step 1: Capture a real session transcript**

```bash
cd build/Debug && printf '1\n1001\nAlice Walker\n1234\n250.00\n2\n1001\n1234\n1\n75.50\n5\n4\n6\n3\n' | ./bank.exe > ../../docs/screenshot.txt && rm -f accounts.txt
```

Use the real output in the README. Do not hand-write a transcript — a fabricated one will not match the program and a reviewer who runs it will notice.

- [ ] **Step 2: Delete TODO.md**

```bash
git rm TODO.md
```

An open TODO list on a portfolio project advertises unfinished work. The remaining items move into "Known limitations" as deliberate boundaries.

- [ ] **Step 3: Write README.md**

Replace the file entirely. Required sections, in order:

1. **Title, one-line description, CI badge**

```markdown
# Banking System (C++)

A command-line banking system in C++17: accounts, PIN authentication with
lockout, deposits, withdrawals, transfers, transaction history, and file
persistence.

[![CI](https://github.com/chidubem6/cpp-banking-system/actions/workflows/ci.yml/badge.svg)](https://github.com/chidubem6/cpp-banking-system/actions/workflows/ci.yml)
```

2. **Example session** — paste the contents of `docs/screenshot.txt` in a fenced block.

3. **Build and run**

````markdown
## Build and run

Requires CMake 3.20+ and a C++17 compiler.

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure   # run the tests
./build/bank                                  # run the app
```
````

4. **Architecture** — the table from the File Structure section of this plan, with the one-line responsibility for each module, plus the sentence: *"Domain code never writes to the console; the CLI owns all output. That separation is what makes the domain testable."*

5. **Design decisions** — one subsection each, written as claim then reason:
   - *Money is an integer count of pence* — binary floating point cannot represent 0.1 exactly, so `double` balances drift as transactions accumulate. `tests/test_money.cpp` accumulates 10,000 additions and asserts the total is exact.
   - *Business outcomes are return values, not exceptions* — insufficient funds is an ordinary outcome, not an exceptional one. Exceptions are reserved for broken invariants. The previous version used `try`/`catch` to decide whether a line of the save file was an account or a transaction; the format now says so explicitly.
   - *The save format escapes its fields* — the previous format was plain CSV, so a name containing a comma silently corrupted the file.
   - *A transfer validates both legs before mutating either* — otherwise a rejected transfer could debit the sender without crediting the recipient.
   - *No repository interface* — there is one storage backend and no second planned. An interface for a single implementation is speculative generality; it can be added when a second backend exists.
   - *`logIn` returns a result, not a pointer* — `findAccount` returns a pointer into a `std::vector<Account>`, which reallocation invalidates. The CLI holds the account number and re-resolves per operation, keeping every pointer's lifetime inside one operation.

6. **Testing**

```markdown
## Testing

Over 80 tests under `tests/`, run on Linux (GCC) and Windows (MSVC) on
every push. Confirm the exact count with `ctest --test-dir build -N` before
publishing this number rather than copying it.

| Area | What is covered |
|---|---|
| `Money` | Parsing, rejection of malformed input, exact arithmetic, formatting, overflow |
| `Sha256` | The four published FIPS 180-2 test vectors |
| `PinCredential` | Salting, verification, that the PIN is never stored |
| `Account` | Deposit and withdrawal rules, lockout, counter reset |
| `Bank` | Every result branch, including that a failed transfer moves no money |
| `Storage` | Round-trips, awkward names, truncated and malformed files |

The interactive menu loop is deliberately untested: exercising `std::cin`
prompts needs I/O plumbing worth more than it returns here. The CLI is kept
as a thin shell over tested logic, and `Cli` takes its streams by reference
so it can be tested later without redesign.
```

7. **Known limitations** — verbatim honesty, each with the fix:

```markdown
## Known limitations

- **PIN hashing uses SHA-256.** SHA-256 is fast, and speed is what an
  attacker wants when brute-forcing a four-digit PIN. Production needs a
  deliberately slow key-derivation function — Argon2, scrypt or bcrypt.
- **A locked account cannot be unlocked.** There is no administrator role,
  so three failed attempts locks the account permanently. A real system
  would offer time-based expiry or an administrative reset.
- **Salt generation uses `std::mt19937_64`,** seeded from `std::random_device`.
  Adequate for demonstration, not a cryptographically secure generator.
- **Storage is a plain text file rewritten in full on exit.** There is no
  journaling, so a crash mid-write loses the session. A real system would
  write to a temporary file and rename it atomically.
- **Single user, no concurrency.** Nothing guards against two processes
  opening the same file.
- **GBP only.** `Money` hard-codes a two-decimal minor unit and the £ symbol.
```

8. **Author** — keep the existing line.

Remove entirely: the "Development Phases", "Stretch Features", "Goals", and "Example Resume Description" sections. They describe a plan rather than a finished project.

- [ ] **Step 4: Verify every command in the README actually works**

```bash
rm -rf build-verify
cmake -B build-verify
cmake --build build-verify
ctest --test-dir build-verify --output-on-failure
rm -rf build-verify
```

Expected: a clean clone-equivalent build succeeds. If any README command fails, fix the README, not the expectation.

- [ ] **Step 5: Confirm the badge URL resolves**

```bash
gh browse --no-browser
```

Check the badge path matches `chidubem6/cpp-banking-system` and the workflow filename is `ci.yml`.

- [ ] **Step 6: Commit and push**

```bash
git add -A
git commit -m "docs: rewrite README around design decisions and limitations

Replaces the phased plan with a description of a finished project: build
instructions that have been run from scratch, the reasoning behind each
design decision, and an explicit list of what is wrong and what would fix
it. Removes TODO.md."
git push
```

---

## Self-review

**Spec coverage.** Every numbered decision in the spec maps to a task: money as pence (3), result enums (7), no domain printing (7), explicit record types (8), salted SHA-256 (4, 5), no repository interface (7, documented in 11), account lockout (6). Testing strategy is Tasks 3–8, CI is Task 10, documentation is Task 11. All nine defects are assigned in the register above.

**Two deviations from the spec, both deliberate:**
1. `PinCredential` is its own module rather than folded into `Sha256`, so the digest keeps clean NIST-vector tests and salting policy lives in one place.
2. `Account::deposit`/`withdraw` take a `TransactionType`, which the spec did not specify. `Bank::transfer` needs to record `TransferIn`/`TransferOut` rather than plain deposits and withdrawals.

**Type consistency.** `Money::fromPence`/`fromString`/`pence`/`toString`/`tryAdd`/`trySubtract`/`isPositive` are used identically in Tasks 3, 6, 7, 8 and 9. `Account::kMaxFailedAttempts` is defined in Task 6 and used in Task 9. `toStorageString`/`transactionTypeFromStorageString` are defined in Task 6 and used in Task 8; `toDisplayString` is defined in Task 6 and used in Task 9. `restorePinCredential` is defined in Task 5 and used in Tasks 7 and 8. `storage::save`/`load` signatures match between Tasks 8 and 9.

**Ordering hazard, handled.** Task 6 breaks `Bank` and `main`; Task 6 Step 9 excludes them from the build and Task 7 Step 1 restores them. This is the only point where the tree does not build a working executable.

**Fixed during self-review:**
- `PinCredential` salt tests were incoherent — one asserted an empty string was empty and proved nothing about salting. Replaced with two tests that vary the salt and the PIN independently.
- `Storage.LeavesTheBankUntouchedWhenLoadingFails` started from an empty bank, so it would have passed even if `load` wrote directly into the caller. Now seeds an account first, which is what makes the assertion meaningful.
- Test count in the README was guessed at "around 70"; the actual total is 82. Changed to "over 80" with an instruction to verify via `ctest -N` rather than trust the figure.
- `Account(..., {})` in the `Bank` overflow test was an ambiguous braced initialiser; now spelled `std::vector<Transaction>{}`.
- Missing includes added: `<cstdint>`/`<vector>` in `test_account.cpp`, `Sha256.h` in `test_pin_credential.cpp`, `<exception>` in `Cli.cpp` (and unused `<limits>` removed).
- Added a global constraint documenting that MSVC is a multi-config generator, so `build/Debug/bank.exe` and `build/bank` differ by platform.
